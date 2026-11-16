/********************************** (C) COPYRIGHT *******************************
 * File Name          : Vial.c
 * Description        : VIAL 改键模块的实现(见 Vial.h)。
 *
 *                      分四块：
 *                        1. 动态键位(4 层 x 5行 x 4列，QMK 键码) + data-flash 存取
 *                        2. 层状态机(MO / TO / TG / DF)
 *                        3. QMK 键码 -> HID 报表字段
 *                        4. VIA / VIAL 协议解析(32 字节 raw HID 包)
 *
 *                      支持的键码：基础键(KC_A 之类)、修饰键(Ctrl/Shift/Alt/GUI)、
 *                      "修饰键+基础键"组合(LCTL(KC_C) 之类)、层键(MO/TO/TG/DF)。
 *                      不支持(VIAL GUI 里能选，但按下去没反应)：
 *                      LT() 这类 tap-hold、OSL/OSM 一次性键、宏、鼠标键、多媒体键。
 *******************************************************************************/

#include <string.h>
#include "CH58x_common.h"
#include "Kbd.h"
#include "Vial.h"
#include "VialDef.h"        /* vial_definition[]：LZMA 压缩过的键盘定义，vial_gen_def.py 生成 */

/* ------------------------------------------------------------------ *
 *  内部常量
 * ------------------------------------------------------------------ */
/* 存储布局(一共 8 + 160 = 168 字节，小于 data-flash 一个 256 字节的擦除页)：
 *   [0..3] 'V','I','A','L'   [4] 存储版本   [5] 层数   [6..7] 保留
 *   [8..]  键码：层优先，每个键码 2 字节，大端(和 VIA 的 buffer 顺序一致) */
#define VIAL_STORE_HDR_LEN   8
#define VIAL_KEYS_PER_LAYER  (MATRIX_ROWS * MATRIX_COLS)
#define VIAL_STORE_KEY_LEN   (VIAL_LAYERS * VIAL_KEYS_PER_LAYER * 2u)

/* VIA 命令(0xFE 是 VIAL 的前缀，见下面 vial_handle_cmd) */
#define VIA_GET_PROTOCOL_VERSION     0x01
#define VIA_GET_KEYBOARD_VALUE       0x02
#define VIA_SET_KEYBOARD_VALUE       0x03
#define VIA_GET_KEYCODE              0x04
#define VIA_SET_KEYCODE              0x05
#define VIA_KEYMAP_RESET             0x06
#define VIA_EEPROM_RESET             0x0A
#define VIA_MACRO_GET_COUNT          0x0C
#define VIA_MACRO_GET_BUFFER_SIZE    0x0D
#define VIA_GET_LAYER_COUNT          0x11
#define VIA_KEYMAP_GET_BUFFER        0x12
#define VIA_KEYMAP_SET_BUFFER        0x13
#define VIA_UNHANDLED                0xFF

/* VIA 的 keyboard value */
#define VIA_VALUE_UPTIME             0x01
#define VIA_VALUE_LAYOUT_OPTIONS     0x02
#define VIA_VALUE_SWITCH_MATRIX      0x03
#define VIA_VALUE_FIRMWARE_VERSION   0x04
#define VIA_VALUE_DEVICE_INDICATION  0x05

/* VIAL 子命令 */
#define VIAL_GET_KEYBOARD_ID         0x00
#define VIAL_GET_SIZE                0x01
#define VIAL_GET_DEF                 0x02
#define VIAL_GET_UNLOCK_STATUS       0x05
#define VIAL_UNLOCK_START            0x06
#define VIAL_UNLOCK_POLL             0x07
#define VIAL_LOCK                    0x08
#define VIAL_QMK_SETTINGS_QUERY      0x09
#define VIAL_DYNAMIC_ENTRY_OP        0x0D
#define VIAL_DYNAMIC_ENTRY_COUNT     0x00

/* ------------------------------------------------------------------ *
 *  内部变量
 * ------------------------------------------------------------------ */
/* 键位的 RAM 镜像：[层][行][列]，值是 QMK 键码 */
static uint16_t s_keymap[VIAL_LAYERS][MATRIX_ROWS][MATRIX_COLS];

/* 默认键位(第 0 层 = 数字小键盘)。1~3 层默认全透明，透明会回退到第 0 层，
 * 所以不动它们的时候每一层按出来都还是小键盘。想改默认键位改这里。 */
#if KB_MATRIX_TCA9535
/* CENTER board 5x14 default layer (from hardware netlist, same as AL_FULL).
 * Physical holes: r2c13, r4c4, r4c5, r4c6, r4c8, r4c9 -> KC_NO. */
static const uint16_t s_default_layer0[MATRIX_ROWS][MATRIX_COLS] = {
    /* r0: ` 1 2 3 4 5 6 7 8 9 0 - = Bksp */
    {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC},
    /* r1: Tab Q W E R T Y U I O P [ ] Backslash */
    {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS},
    /* r2: Caps A S D F G H J K L ; ' -- Enter */
    {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_NO,   KC_ENT},
    /* r3: Shift Z X C V B N M , . / Shift Up Del */
    {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT, KC_UP,   KC_DEL},
    /* r4: Ctrl Win Alt -- -- -- Space -- -- Alt Ctrl Left Down Right */
    {KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_RALT, KC_RCTL, KC_LEFT, KC_DOWN, KC_RGHT},
};
#else  /* V1 numpad default layer, unchanged */
static const uint16_t s_default_layer0[MATRIX_ROWS][MATRIX_COLS] = {
    /* 行0: Num      /        *        -          */
    {KC_NUM,   KC_PSLS, KC_PAST, KC_PMNS},
    /* 行1: 7        8        9        +          */
    {KC_P7,    KC_P8,   KC_P9,   KC_PPLS},
    /* 行2: 4        5        6        (空)       */
    {KC_P4,    KC_P5,   KC_P6,   KC_NO},
    /* 行3: 1        2        3        (空)       */
    {KC_P1,    KC_P2,   KC_P3,   KC_NO},
    /* 行4: 0        .        Enter    (空)       */
    {KC_P0,    KC_PDOT, KC_PENT, KC_NO},
};
#endif /* KB_MATRIX_TCA9535 */

/* data-flash 读写的暂存区。用 uint32_t 声明是为了保证 4 字节对齐
 * (ROM 里的 flash 读写例程按字操作，对齐了最保险) */
static uint32_t s_store_hdr[VIAL_STORE_HDR_LEN / 4];
static uint32_t s_store_buf[VIAL_STORE_KEY_LEN / 4];

/* 层状态：base_layer 是 TO/TG/DF 改过的"当前层"，momentary 是 MO 按住期间的层 */
static uint8_t s_base_layer = 0;
static uint8_t s_active_layer = 0;
/* 上一次的按键状态，用来判断"刚按下"(TO/TG/DF 只在按下那一刻生效) */
static uint8_t s_prev_state[MATRIX_ROWS][MATRIX_COLS];

/* 1 = Vial_Init() 已经跑过，键位有效 */
static uint8_t s_ready = 0;

/* raw HID 收发 */
static uint8_t s_buf[VIAL_RAW_EPSIZE];
static uint8_t (*s_send)(const uint8_t *buf, uint8_t len) = NULL;
static uint8_t s_rx_pending = 0;
static uint8_t s_tx_pending = 0;

/* 键位改动延迟落盘 */
static uint8_t  s_dirty = 0;
static uint32_t s_dirty_tick = 0;

/* ------------------------------------------------------------------ *
 *  本文件内的函数声明
 * ------------------------------------------------------------------ */
static void     vial_load_defaults(void);
static uint8_t  vial_store_load(void);
static void     vial_store_save(void);
static uint16_t vial_linear_get(uint16_t idx);
static void     vial_linear_set(uint16_t idx, uint16_t kc);
static void     vial_keymap_mark_dirty(void);
static uint8_t  vial_layer_fix(uint8_t layer);
static void     vial_handle_cmd(uint8_t *msg);
static void     vial_handle_via(uint8_t *msg);
static void     vial_handle_vial(uint8_t *msg);

/*********************************************************************
 * @fn      vial_load_defaults
 *
 * @brief   把默认键位填进 RAM 镜像：第 0 层用小键盘，1~3 层全透明
 *
 * @return  none
 */
static void vial_load_defaults(void)
{
    uint8_t l, r, c;

    for(r = 0; r < MATRIX_ROWS; r++)
    {
        for(c = 0; c < MATRIX_COLS; c++)
        {
            s_keymap[0][r][c] = s_default_layer0[r][c];
        }
    }

    for(l = 1; l < VIAL_LAYERS; l++)
    {
        for(r = 0; r < MATRIX_ROWS; r++)
        {
            for(c = 0; c < MATRIX_COLS; c++)
            {
                s_keymap[l][r][c] = VIAL_KC_TRNS;
            }
        }
    }
}

/*********************************************************************
 * @fn      vial_linear_get / vial_linear_set
 *
 * @brief   按"线性序号"读写键码：idx = 层*(行*列) + 行*列 + 列，
 *          也就是 VIA 的 keymap buffer 顺序(每个键码 2 字节)。
 *
 * @param   idx - 键码序号(不是字节号)
 * @param   kc  - 键码
 *
 * @return  get: 键码
 */
static uint16_t vial_linear_get(uint16_t idx)
{
    uint8_t layer, rem, row, col;

    layer = (uint8_t)(idx / VIAL_KEYS_PER_LAYER);
    rem   = (uint8_t)(idx % VIAL_KEYS_PER_LAYER);
    row   = (uint8_t)(rem / MATRIX_COLS);
    col   = (uint8_t)(rem % MATRIX_COLS);

    return s_keymap[layer][row][col];
}

static void vial_linear_set(uint16_t idx, uint16_t kc)
{
    uint8_t layer, rem, row, col;

    layer = (uint8_t)(idx / VIAL_KEYS_PER_LAYER);
    rem   = (uint8_t)(idx % VIAL_KEYS_PER_LAYER);
    row   = (uint8_t)(rem / MATRIX_COLS);
    col   = (uint8_t)(rem % MATRIX_COLS);

    s_keymap[layer][row][col] = kc;
}

/*********************************************************************
 * @fn      vial_store_load
 *
 * @brief   从 data-flash 读键位。没存过 / 版本不对 / 层数不对就返回 0
 *          (调用方接着用默认键位)。
 *
 * @return  1-读到有效键位, 0-没有
 */
static uint8_t vial_store_load(void)
{
    uint8_t *hdr = (uint8_t *)s_store_hdr;
    uint8_t *buf = (uint8_t *)s_store_buf;
    uint16_t i, n;

    EEPROM_READ(VIAL_STORE_ADDR, hdr, VIAL_STORE_HDR_LEN);

    if((hdr[0] != 'V') || (hdr[1] != 'I') || (hdr[2] != 'A') || (hdr[3] != 'L'))
    {
        return 0;
    }
    if(hdr[4] != VIAL_STORE_VERSION)
    {
        return 0;
    }
    if(hdr[5] != VIAL_LAYERS)
    {
        return 0;
    }

    EEPROM_READ(VIAL_STORE_ADDR + VIAL_STORE_HDR_LEN, buf, VIAL_STORE_KEY_LEN);

    n = (uint16_t)(VIAL_STORE_KEY_LEN / 2);
    for(i = 0; i < n; i++)
    {
        vial_linear_set(i, (uint16_t)(((uint16_t)buf[i * 2] << 8) | buf[i * 2 + 1]));
    }
    return 1;
}

/*********************************************************************
 * @fn      vial_store_save
 *
 * @brief   把 RAM 里的键位写进 data-flash：先擦一页(256 字节)再写头和数据。
 *          只在主循环里调用(擦写要几毫秒，不能在 USB 中断里做)。
 *
 * @return  none
 */
static void vial_store_save(void)
{
    uint8_t *hdr = (uint8_t *)s_store_hdr;
    uint8_t *buf = (uint8_t *)s_store_buf;
    uint16_t i, n;
    uint16_t kc;

    hdr[0] = 'V';
    hdr[1] = 'I';
    hdr[2] = 'A';
    hdr[3] = 'L';
    hdr[4] = VIAL_STORE_VERSION;
    hdr[5] = VIAL_LAYERS;
    hdr[6] = 0;
    hdr[7] = 0;

    n = (uint16_t)(VIAL_STORE_KEY_LEN / 2);
    for(i = 0; i < n; i++)
    {
        kc = vial_linear_get(i);
        buf[i * 2] = (uint8_t)(kc >> 8);
        buf[i * 2 + 1] = (uint8_t)(kc & 0xFF);
    }

    EEPROM_ERASE(VIAL_STORE_ADDR, 256);
    EEPROM_WRITE(VIAL_STORE_ADDR, hdr, VIAL_STORE_HDR_LEN);
    EEPROM_WRITE(VIAL_STORE_ADDR + VIAL_STORE_HDR_LEN, buf, VIAL_STORE_KEY_LEN);
}

/*********************************************************************
 * @fn      vial_keymap_mark_dirty
 *
 * @brief   键位被改了：记个标志，等 VIAL_SAVE_DELAY_MS 之后再擦写 flash，
 *          这样连续改好几个键(VIAL 会一包一包发)只擦写一次。
 *
 * @return  none
 */
static void vial_keymap_mark_dirty(void)
{
    s_dirty = 1;
    s_dirty_tick = KB_Tick;
}

/*********************************************************************
 * @fn      vial_layer_fix
 *
 * @brief   层号越界防护：VIAL 上最多能选 31 层，本固件只有 VIAL_LAYERS 层，
 *          多出来的统一退回第 0 层。
 *
 * @param   layer - 键码里取出来的层号
 *
 * @return  合法层号
 */
static uint8_t vial_layer_fix(uint8_t layer)
{
    return (layer < VIAL_LAYERS) ? layer : 0;
}

/*********************************************************************
 * @fn      Vial_Init
 *
 * @brief   初始化(见 Vial.h)
 *
 * @return  none
 */
void Vial_Init(void)
{
    vial_load_defaults();

    if(vial_store_load())
    {
        PRINT("vial: keymap loaded from data-flash\n");
    }
    else
    {
        PRINT("vial: using default keymap\n");
    }

    s_ready = 1;
    s_base_layer = 0;
    s_active_layer = 0;
    memset(s_prev_state, 0, sizeof(s_prev_state));
}

/*********************************************************************
 * @fn      Vial_SetSendFunc
 *
 * @brief   注册回包发送函数(见 Vial.h)
 *
 * @return  none
 */
void Vial_SetSendFunc(uint8_t (*fn)(const uint8_t *buf, uint8_t len))
{
    s_send = fn;
    s_tx_pending = 0;
}

/*********************************************************************
 * @fn      Vial_RawHidReceive
 *
 * @brief   raw HID 收到一包(见 Vial.h)。在 USB 中断里调用，所以只做拷贝。
 *
 * @return  none
 */
void Vial_RawHidReceive(const uint8_t *data, uint8_t len)
{
    if(len != VIAL_RAW_EPSIZE)
    {
        return;
    }

    memcpy(s_buf, data, VIAL_RAW_EPSIZE);
    s_rx_pending = 1;
}

/*********************************************************************
 * @fn      Vial_Task
 *
 * @brief   主循环任务(见 Vial.h)
 *
 * @return  none
 */
void Vial_Task(void)
{
    /* 收到新的一包：处理完就地改成回包，然后发出去 */
    if(s_rx_pending)
    {
        s_rx_pending = 0;
        vial_handle_cmd(s_buf);
        s_tx_pending = 1;
    }

    /* 端点忙(主机还没来取)就下一轮再试 */
    if(s_tx_pending)
    {
        if((s_send != NULL) && s_send(s_buf, VIAL_RAW_EPSIZE))
        {
            s_tx_pending = 0;
        }
    }

    /* 改键之后安静一会儿再落盘，避免每改一个键都擦一次 flash */
    if(s_dirty && ((KB_Tick - s_dirty_tick) >= VIAL_SAVE_DELAY_MS))
    {
        s_dirty = 0;
        vial_store_save();
        PRINT("vial: keymap saved\n");
    }
}

/*********************************************************************
 * @fn      Vial_UpdateLayer
 *
 * @brief   更新层状态(见 Vial.h)。
 *          规则(够用就好，没照搬 QMK 的完整层解析)：
 *            MO(l) 按住期间生效层 = l
 *            TO(l)/DF(l) 按下那一刻把 l 设成当前层
 *            TG(l) 按下那一刻在 l 和 0 层之间切换
 *          层键只在"当前层"上识别，所以 TO(1) 放第 0 层、TO(0) 放第 1 层这样配对用。
 *
 * @return  none
 */
void Vial_UpdateLayer(void)
{
    uint8_t r, c, down, tgt;
    uint8_t momentary = 0xFF;
    uint16_t kc;

    if(!s_ready)
    {
        return;
    }

    /* 层号越界(比如旧固件存过更多层的数据)就退回第 0 层，免得下面按下标越界 */
    if(s_base_layer >= VIAL_LAYERS)
    {
        s_base_layer = 0;
    }

    for(r = 0; r < MATRIX_ROWS; r++)
    {
        for(c = 0; c < MATRIX_COLS; c++)
        {
            down = KB_KeyPressed(r, c) ? 1 : 0;
            /* 用"解析后"的键码(透明回退)来找层键：这样切到第 1 层以后，
               还按原来那个 TG(1) 键也能切回来(第 1 层那一格没定义时回退到第 0 层) */
            kc = Vial_ResolveKeycode(s_base_layer, r, c);

            if(down && (s_prev_state[r][c] == 0))    /* 刚按下 */
            {
                if((kc & (uint16_t)(~VIAL_QK_LAYER_MASK)) == VIAL_QK_TO)
                {
                    s_base_layer = vial_layer_fix((uint8_t)(kc & VIAL_QK_LAYER_MASK));
                }
                else if((kc & (uint16_t)(~VIAL_QK_LAYER_MASK)) == VIAL_QK_DEF_LAYER)
                {
                    s_base_layer = vial_layer_fix((uint8_t)(kc & VIAL_QK_LAYER_MASK));
                }
                else if((kc & (uint16_t)(~VIAL_QK_LAYER_MASK)) == VIAL_QK_TOGGLE_LAYER)
                {
                    tgt = vial_layer_fix((uint8_t)(kc & VIAL_QK_LAYER_MASK));
                    s_base_layer = (s_base_layer == tgt) ? 0 : tgt;
                }
            }

            if(down)                                  /* 按住期间 */
            {
                if((kc & (uint16_t)(~VIAL_QK_LAYER_MASK)) == VIAL_QK_MOMENTARY)
                {
                    momentary = vial_layer_fix((uint8_t)(kc & VIAL_QK_LAYER_MASK));
                }
            }

            s_prev_state[r][c] = down;
        }
    }

    if(momentary != 0xFF)
    {
        s_active_layer = momentary;
    }
    else
    {
        s_active_layer = s_base_layer;
    }
}

/*********************************************************************
 * @fn      Vial_GetLayer
 *
 * @brief   当前生效层(见 Vial.h)
 *
 * @return  层号
 */
uint8_t Vial_GetLayer(void)
{
    return s_active_layer;
}

/*********************************************************************
 * @fn      Vial_GetLayerCount
 *
 * @brief   层数(见 Vial.h)
 *
 * @return  层数
 */
uint8_t Vial_GetLayerCount(void)
{
    return VIAL_LAYERS;
}

/*********************************************************************
 * @fn      Vial_ResolveKeycode
 *
 * @brief   取生效键码，透明则往下层回退(见 Vial.h)
 *
 * @return  QMK 键码
 */
uint16_t Vial_ResolveKeycode(uint8_t layer, uint8_t row, uint8_t col)
{
    uint16_t kc;

    if((row >= MATRIX_ROWS) || (col >= MATRIX_COLS))
    {
        return VIAL_KC_NO;
    }
    if(layer >= VIAL_LAYERS)
    {
        layer = (uint8_t)(VIAL_LAYERS - 1);
    }

    while(layer > 0)
    {
        kc = s_keymap[layer][row][col];
        if(kc != VIAL_KC_TRNS)
        {
            return kc;
        }
        layer--;
    }

    return s_keymap[0][row][col];
}

/*********************************************************************
 * @fn      Vial_KeycodeToHid
 *
 * @brief   QMK 键码 -> HID 报表字段(见 Vial.h)
 *
 * @return  1-要上报, 0-忽略
 */
uint8_t Vial_KeycodeToHid(uint16_t kc, uint8_t *mods, uint8_t *key)
{
    uint8_t k = 0;

    if((kc >= VIAL_MOD_MIN) && (kc <= VIAL_MOD_MAX))
    {
        /* 单独的修饰键：Ctrl / Shift / Alt / GUI */
        *mods |= (uint8_t)(1 << (kc - VIAL_MOD_MIN));
        *key = 0;
        return 1;
    }

    if((kc >= VIAL_QK_MODS_MIN) && (kc <= VIAL_QK_MODS_MAX))
    {
        /* LCTL(KC_C) / RCTL(KC_C) 这类：高字节低 4 位是哪几个修饰键，
         * 第 12 位(0x1000)表示是右手那几个 —— 照 QMK 的 extract_mod_bits() 翻译：
         *   左手 -> HID 修饰键字节的 bit0~3，右手 -> bit4~7 */
        uint8_t n = (uint8_t)((kc >> 8) & 0x1F);

        if(kc & VIAL_QK_RMODS)
        {
            if(n & 0x01) { *mods |= 0x10; }     /* RCtrl  */
            if(n & 0x02) { *mods |= 0x20; }     /* RShift */
            if(n & 0x04) { *mods |= 0x40; }     /* RAlt   */
            if(n & 0x08) { *mods |= 0x80; }     /* RGUI   */
        }
        else
        {
            *mods |= (uint8_t)(n & 0x0F);       /* LCtrl/LShift/LAlt/LGUI */
        }
        k = (uint8_t)(kc & 0xFF);
    }
    else if((kc >= VIAL_BASIC_MIN) && (kc <= VIAL_BASIC_MAX))
    {
        k = (uint8_t)kc;
    }
    else
    {
        /* 空键 / 层键 / 不支持的量子键(LT、宏、鼠标、多媒体等) */
        return 0;
    }

    /* 组合键里没带基础键(比如只有修饰键)时只上报修饰键 */
    if(k < VIAL_BASIC_MIN)
    {
        k = 0;
    }
    *key = k;
    return 1;
}

/*********************************************************************
 * @fn      vial_handle_cmd
 *
 * @brief   处理一包 raw HID：0xFE 开头的是 VIAL 命令，其它是 VIA 命令。
 *          处理完 s_buf 就地变成回包，由 Vial_Task 发出去。
 *
 * @param   msg  - 32 字节
 *
 * @return  none
 */
static void vial_handle_cmd(uint8_t *msg)
{
    if(msg[0] == 0xFE)
    {
        vial_handle_vial(msg);
    }
    else
    {
        vial_handle_via(msg);
    }
}

/*********************************************************************
 * @fn      vial_put_u32be / vial_put_u32le
 *
 * @brief   把 32 位数写进回包：VIA 的数值是大端，VIAL 的两个是小端
 *          (照 vial-qmk 的写法，主机就是这么解的)
 *
 * @return  none
 */
static void vial_put_u32be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

static void vial_put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/*********************************************************************
 * @fn      vial_handle_via
 *
 * @brief   VIA 协议(改键位用的就是这几个命令)
 *
 * @param   msg  - 32 字节：msg[0] 是命令，msg[1] 开始是参数/返回值
 *
 * @return  none
 */
static void vial_handle_via(uint8_t *msg)
{
    uint8_t *d = &msg[1];
    uint8_t layer, row, col, size, i;
    uint16_t offset, idx, kc;

    switch(msg[0])
    {
        case VIA_GET_PROTOCOL_VERSION:
            d[0] = (uint8_t)((VIA_PROTOCOL_VERSION >> 8) & 0xFF);
            d[1] = (uint8_t)(VIA_PROTOCOL_VERSION & 0xFF);
            break;

        case VIA_GET_KEYBOARD_VALUE:
            switch(d[0])
            {
                case VIA_VALUE_UPTIME:              /* 开机到现在多少毫秒 */
                    vial_put_u32be(&d[1], KB_Tick);
                    break;

                case VIA_VALUE_LAYOUT_OPTIONS:      /* 没用布局选项，固定 0 */
                    vial_put_u32be(&d[1], 0);
                    break;

                case VIA_VALUE_SWITCH_MATRIX:       /* 每行一个字节，bit c = (r,c) 按下 */
                    for(row = 0; row < MATRIX_ROWS; row++)
                    {
                        uint8_t m = 0;
                        for(col = 0; col < MATRIX_COLS; col++)
                        {
                            if(KB_KeyPressed(row, col))
                            {
                                m |= (uint8_t)(1 << col);
                            }
                        }
                        d[1 + row] = m;
                    }
                    break;

                case VIA_VALUE_FIRMWARE_VERSION:
                    vial_put_u32be(&d[1], VIAL_FIRMWARE_VERSION);
                    break;

                case VIA_VALUE_DEVICE_INDICATION:   /* 让键盘闪一下：没有灯效可闪 */
                    d[1] = 0;
                    break;

                default:
                    msg[0] = VIA_UNHANDLED;
                    break;
            }
            break;

        case VIA_SET_KEYBOARD_VALUE:
            /* 布局选项 / device indication 都不用存，忽略 */
            break;

        case VIA_GET_KEYCODE:                       /* d[0]=层 d[1]=行 d[2]=列 */
            layer = d[0];
            row = d[1];
            col = d[2];
            kc = 0;
            if((layer < VIAL_LAYERS) && (row < MATRIX_ROWS) && (col < MATRIX_COLS))
            {
                kc = s_keymap[layer][row][col];     /* 原样返回，不做透明回退 */
            }
            d[3] = (uint8_t)(kc >> 8);
            d[4] = (uint8_t)(kc & 0xFF);
            break;

        case VIA_SET_KEYCODE:                       /* d[0]=层 d[1]=行 d[2]=列 d[3..4]=键码 */
            layer = d[0];
            row = d[1];
            col = d[2];
            if((layer < VIAL_LAYERS) && (row < MATRIX_ROWS) && (col < MATRIX_COLS))
            {
                kc = (uint16_t)(((uint16_t)d[3] << 8) | d[4]);
                s_keymap[layer][row][col] = kc;
                vial_keymap_mark_dirty();
            }
            break;

        case VIA_KEYMAP_GET_BUFFER:                 /* d[0..1]=字节偏移 d[2]=长度(<=28) */
            offset = (uint16_t)(((uint16_t)d[0] << 8) | d[1]);
            size = d[2];
            if(size > (VIAL_RAW_EPSIZE - 4))
            {
                size = (uint8_t)(VIAL_RAW_EPSIZE - 4);
            }
            for(i = 0; i < size; i++)
            {
                idx = (uint16_t)((offset + i) / 2);
                if(idx >= (VIAL_STORE_KEY_LEN / 2))
                {
                    d[3 + i] = 0;
                    continue;
                }
                kc = vial_linear_get(idx);
                d[3 + i] = ((offset + i) & 1) ? (uint8_t)(kc & 0xFF) : (uint8_t)(kc >> 8);
            }
            break;

        case VIA_KEYMAP_SET_BUFFER:
            offset = (uint16_t)(((uint16_t)d[0] << 8) | d[1]);
            size = d[2];
            if(size > (VIAL_RAW_EPSIZE - 4))
            {
                size = (uint8_t)(VIAL_RAW_EPSIZE - 4);
            }
            for(i = 0; i < size; i++)
            {
                uint16_t byte_idx = (uint16_t)(offset + i);

                if(byte_idx >= VIAL_STORE_KEY_LEN)
                {
                    break;
                }

                /* 单字节写：把对应的那个字节替换掉，另一个字节保持原样 */
                idx = (uint16_t)(byte_idx / 2);
                kc = vial_linear_get(idx);
                if((byte_idx & 1) == 0)
                {
                    kc = (uint16_t)((kc & 0x00FF) | ((uint16_t)d[3 + i] << 8));
                }
                else
                {
                    kc = (uint16_t)((kc & 0xFF00) | d[3 + i]);
                }
                vial_linear_set(idx, kc);
            }
            vial_keymap_mark_dirty();
            break;

        case VIA_KEYMAP_RESET:                      /* 恢复默认键位 */
        case VIA_EEPROM_RESET:
            vial_load_defaults();
            vial_keymap_mark_dirty();
            PRINT("vial: keymap reset\n");
            break;

        case VIA_MACRO_GET_COUNT:                   /* 不支持宏 */
            d[0] = 0;
            break;

        case VIA_MACRO_GET_BUFFER_SIZE:
            d[0] = 0;
            d[1] = 0;
            break;

        case VIA_GET_LAYER_COUNT:
            d[0] = (uint8_t)VIAL_LAYERS;
            break;

        default:                                    /* 不认识的命令 */
            msg[0] = VIA_UNHANDLED;
            break;
    }
}

/*********************************************************************
 * @fn      vial_handle_vial
 *
 * @brief   VIAL 扩展命令：msg[0] 是 0xFE 前缀，msg[1] 是子命令
 *
 * @param   msg  - 32 字节
 *
 * @return  none
 */
static void vial_handle_vial(uint8_t *msg)
{
    uint32_t start, end, page;

    switch(msg[1])
    {
        case VIAL_GET_KEYBOARD_ID:                  /* 协议版本 + 8 字节 UID */
        {
            uint8_t uid[8] = VIAL_KEYBOARD_UID;

            memset(msg, 0, VIAL_RAW_EPSIZE);
            vial_put_u32le(&msg[0], VIAL_PROTOCOL_VERSION);
            memcpy(&msg[4], uid, 8);
            break;
        }

        case VIAL_GET_SIZE:                         /* 内嵌定义的字节数 */
            vial_put_u32le(&msg[0], (uint32_t)sizeof(vial_definition));
            break;

        case VIAL_GET_DEF:                          /* 按 32 字节一页读定义 */
            page = (uint32_t)msg[2] + ((uint32_t)msg[3] << 8);
            start = page * VIAL_RAW_EPSIZE;
            end = start + VIAL_RAW_EPSIZE;
            memset(msg, 0, VIAL_RAW_EPSIZE);        /* 最后一页不足 32 字节时补 0 */
            if(start < (uint32_t)sizeof(vial_definition))
            {
                if(end > (uint32_t)sizeof(vial_definition))
                {
                    end = (uint32_t)sizeof(vial_definition);
                }
                memcpy(msg, &vial_definition[start], (size_t)(end - start));
            }
            break;

        case VIAL_GET_UNLOCK_STATUS:                /* 一直解锁，不用按组合键 */
            memset(msg, 0xFF, VIAL_RAW_EPSIZE);
            msg[0] = VIAL_UNLOCKED;
            msg[1] = 0;
            break;

        case VIAL_UNLOCK_START:
            break;

        case VIAL_UNLOCK_POLL:
            memset(msg, 0, VIAL_RAW_EPSIZE);
            msg[0] = VIAL_UNLOCKED;
            msg[1] = 0;
            msg[2] = 0;
            break;

        case VIAL_LOCK:                             /* 不锁 */
            break;

        case VIAL_QMK_SETTINGS_QUERY:               /* 不支持 QMK settings */
            memset(msg, 0xFF, VIAL_RAW_EPSIZE);
            break;

        case VIAL_DYNAMIC_ENTRY_OP:                 /* 不支持 tap dance / combo 等 */
            if(msg[2] == VIAL_DYNAMIC_ENTRY_COUNT)
            {
                memset(msg, 0, VIAL_RAW_EPSIZE);
            }
            break;

        default:
            break;
    }
}

/*********************************************************************
 *                    endfile @ Vial
 *********************************************************************/