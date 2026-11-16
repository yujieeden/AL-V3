/********************************** (C) COPYRIGHT *******************************
 * File Name          : Kbd.c
 * Author             : WCH / project
 * Version            : V1.2
 * Description        : ���̱����߼���5x4 ����ɨ�� + ���� + 8�ֽڱ�׼���̱��� +
 *                      ָʾ�� + 17 �� WS2812 �ƴ���
 *                      �ⲿ�ֺ�"�� USB �����������ϱ�"�޹أ����ִ���ģʽ���á�
 *********************************************************************************
 * ���ļ���ԭ���� src/Main.c ����������Ŀ������������ USB ��������ͬһ��
 * ����/�ƹ���룺˭��Ч˭�͵� KB_SetSendFunc() ע��һ�����ͺ�����Ȼ�����Լ�
 * ��ѭ���ﷴ���� KB_Task()��
 *******************************************************************************/

#include <string.h>
#include "CH58x_common.h"
#include "Kbd.h"
#include "Vial.h"
#if KB_MATRIX_TCA9535
#include "I2cDev.h"
#include "Tca9535.h"
#endif

/*********************************************************************
 *                          ���̾�������
 *********************************************************************
 *  Row0: [NumLock] [ / ]   [ * ]   [ - ]
 *  Row1: [7]       [8]     [9]     [ +  ](2U��)
 *  Row2: [4]       [5]     [6]     [��  ]
 *  Row3: [1]       [2]     [3]     [Enter](2U��)
 *  Row4: [0(2U��)]  [��]    [ . ]   [��  ]
 ********************************************************************/
/* ������Ӧ������(PB0/PB1/PB2/PB3/PB5)���������룬�����͵�ƽ��ʾ�����м����� */
#if !KB_MATRIX_TCA9535
#define MATRIX_ROW_PINS   {GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3, GPIO_Pin_5}
#endif
/* ������Ӧ������(PA4/PA5/PA6/PA7)���������������������ɨ�� */
#if !KB_MATRIX_TCA9535
#define MATRIX_COL_PINS   {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7}
#endif

/* �����ܳ�����->��(COL2ROW)����������������������� */
#define DIODE_DIRECTION_COL2ROW

#define SCAN_DELAY_US    10     /* ÿ�����ͺ�ȴ���ƽ�ȶ���ʱ��(us) */

/*********************** ���Կ��� ***********************
 * KB_BOOT_SELFTEST : 1 = �ϵ�(USBö����� / ������������)���Զ�"����"һ��"ok",
 *                        �����ж��ϱ�ͨ���Ƿ�ͨ������ok˵��ͨ��û����,
 *                        �����ھ���/����; ʲô��û�����˵���ϱ�ͨ�������⡣
 *                        ����ĳ� 0 �Ͳ�����������������ˡ�
 * MATRIX_SCAN_BOTH_WAYS : 1 = ����˫��ɨ�衣�Ȱ������Ͷ���(COL2ROW),
 *                        �ٰ������Ͷ��С��������ܰ����϶��������ĸ�����
 *                        �����öŰ��߶̽���/�����Ų���, ���ܼ�⵽������
 * KB_RAW_DUMP      : 1 = ÿ���ԭʼɨ�����Ӵ���1(PA9, 115200)��һ�г���,
 *                        ��ֱ�ӿ����ĸ�����λ�ñ���⵽�ˡ��ĸ�����һֱ�ǵ͵�ƽ��
 * KB_BOOT_CHASE    : 1 = �ϵ���ˮ��(һ��һ�����ε���)�������˶Ե���˳��
 *                        ע�⣺����ģʽ�� PC �����������ڴ�ӡ�ģ�Ҫ�Ӵ���ģ�顣
 ****************************************************************/
#define KB_BOOT_SELFTEST        1
#define MATRIX_SCAN_BOTH_WAYS   1
#define KB_RAW_DUMP             1
#define KB_EVENT_PRINT          1

// Retry a busy BLE report path at most once per 10ms.  This prevents the
// tight BLE loop from hammering GATT_Notification() when the stack reports
// blePending (0x16) because no TX packet credit is available.
#define KB_SEND_RETRY_TICKS     10

/*********************** ָʾ��(LED)���� ***********************
 * ����"�������ŵ�ɫָʾ��"�����ã�������� WS2812 �ƴ��������¡�
 * ���ڵƹ��� WS2812(ÿ��һ��)��NumLock ״ֱ̬���� NumLock ���ĵ���ʾ��
 * ����������Ĭ�϶���"û��"(����д 0���̼�����ȥ���κν�)��
 * ���������ȷʵ���� 3 �Ŷ���ָʾ�ƣ����������Ͼ����ã�
 *   KB_LED_NUM_PORT / KB_LED_NUM_PIN  ->  NumLock ��
 *   KB_LED_CAPS_PORT / KB_LED_CAPS_PIN ->  CapsLock ��
 *   KB_LED_SCROLL_PORT / KB_LED_SCROLL_PIN -> ScrollLock ��
 *   �˿�д 'A' �� 'B'������д GPIO_Pin_x��д 0 = ��ŵ�û��
 *   KB_LED_ACTIVE_LOW : 1 = �͵�ƽ����(��������3V3��������IO�����)
 *                       0 = �ߵ�ƽ����(IO -> �������� -> ������ -> GND)
 ****************************************************************/
#define KB_LED_NUM_PORT        'A'
#define KB_LED_NUM_PIN         0
#define KB_LED_CAPS_PORT       'A'
#define KB_LED_CAPS_PIN        0
#define KB_LED_SCROLL_PORT     'A'
#define KB_LED_SCROLL_PIN      0
#define KB_LED_ACTIVE_LOW      1

/*********************** WS2812 �ƴ����� ***********************
 * ÿ����������һ�� WS2812/SK6812��WS_LED_NUM �Ŵ���һ����
 *   ������ IO --DIN--> ��0�� --DOUT--> ��1�� ... --> ���һ��
 *
 * ����Ҫ�㣺
 * 1. WS_PIN �����ǿսţ����Բ��ܺ���Щ�ų�ͻ��
 *      �������� PA4~PA7 / PB0~PB3 / PB5�����Դ��� PA8/PA9��USB PB10(D-)/PB11(D+)��
 *    �ر�ע�⣺�������������ŵ����ݽš���ɨ��ʱ������������ͣ��ƻ�������
 * 2. �Ƶ� VCC/GND �������ߣ�17 ��ȫ�������ȵ����ܵ� 1A��USB ��������ˣ�
 *    ����Ĭ���� WS_BRIGHT �������ȣ����߸ɴ���ƴ��������硣
 * 3. ������̫��ʱ����һ�ŵ�ǰ�� 100~300�� ������ȡ�
 * 4. PA14 ������� 3.3V ��ƽ���� WS2812 һ��� 5V��������һ��û����(�����Ƕ���)��
 *    �����һ�ŵ���˸/���ܿأ��ѵƴ����罵�� 4.5V ����(����������)���һ����ƽת����
 ****************************************************************/
#define WS_ENABLE        1                  /* 0 = ���õƣ�WS2812 ��ش���ȫ�������� */
#define WS_PORT          'A'                /* ���ݽ����ڶ˿ڣ�'A' �� 'B' */
#define WS_PIN           GPIO_Pin_14        /* ���ݽţ�PA14������ʵ�ʽ��ߣ� */
#define WS_LED_NUM       17                 /* �������� = �������� */
#define WS_BRIGHT        48                 /* ȫ������ 0~255��ԽСԽʡ��(48 �� 19%) */

#define WS_EFFECT_KEY     0                 /* Ч����������Ӧ(��������NumLock����ʾNumLock״̬) */
#define WS_EFFECT_RAINBOW 1                 /* Ч�����ʺ����� */
#define WS_EFFECT         WS_EFFECT_KEY     /* ��ǰЧ�� */
#if KB_MATRIX_TCA9535
#define WS_BOOT_CHASE   0       /* CENTER board has no WS2812 chain */
#else
#define WS_BOOT_CHASE   1
#endif                 /* �ϵ���ˮ��(һ��һ�����ε���)�������˶Ե���˳�� */
#define WS_CHASE_ROUNDS   3                 /* ��ˮ���ظ�����(����������)���궨����������иĳ�0 */

/* WS2812 ʱ�򣬰�ϵͳʱ���Զ�����ɶ�ʱ���������������ֹ��㣺
 *   0�� = �� 350ns   1�� = �� 700ns   λ���� 1250ns   ֡�临λ�͵�ƽ > 50us */
#define WS_NS2TICK(ns)   ((uint32_t)(((uint64_t)FREQ_SYS * (uint64_t)(ns)) / 1000000000ULL))
#define WS_T0H           WS_NS2TICK(350)
#define WS_T1H           WS_NS2TICK(700)
#define WS_PERIOD        WS_NS2TICK(1250)

/* ��ɫ(0~255)�����ջ��ٳ� WS_BRIGHT��ȫ 0 ����Ϩ�� */
#define WS_COLOR_IDLE_R  0                  /* ����ʱ�����ĵ�ɫ */
#define WS_COLOR_IDLE_G  0
#define WS_COLOR_IDLE_B  6
#define WS_COLOR_KEY_R   56                 /* ����ʱ�ü��ĵ�ɫ */
#define WS_COLOR_KEY_G   56
#define WS_COLOR_KEY_B   56
#define WS_COLOR_NUM_R   0                  /* NumLock ��ʱ NumLock ���ĵ�ɫ */
#define WS_COLOR_NUM_G   40
#define WS_COLOR_NUM_B   0

/* ���ݽſ��ٲ�����ֱ��д�Ĵ������ȵ�������ö�(WS2812 �� 0 ��ߵ�ƽֻ�� 21 ��ʱ��) */
#if (WS_PORT == 'A')
#define WS_PIN_CFG()     GPIOA_ModeCfg(WS_PIN, GPIO_ModeOut_PP_5mA)
#define WS_PIN_HIGH()    (R32_PA_OUT |= WS_PIN)
#define WS_PIN_LOW()     (R32_PA_CLR = WS_PIN)
#else
#define WS_PIN_CFG()     GPIOB_ModeCfg(WS_PIN, GPIO_ModeOut_PP_5mA)
#define WS_PIN_HIGH()    (R32_PB_OUT |= WS_PIN)
#define WS_PIN_LOW()     (R32_PB_CLR = WS_PIN)
#endif

/******** ���尴�������� VIA ����Ϊ׼��Numpad 5x4 single cell�� ********
 *
 *   VIA �� layouts.keymap ��������д�ģ�17 ������ȫ�� 1 �������
 *       ["0,0", "0,1", "0,2", "0,3"]
 *       ["1,0", "1,1", "1,2", "1,3"]
 *       ["2,0", "2,1", "2,2"]          <- �� 3 ��û�� (2,3)
 *       ["3,0", "3,1", "3,2"]          <- �� 3 ��û�� (3,3)
 *       ["4,0", "4,1", "4,2"]          <- �� 3 ��û�� (4,3)
 *
 *   ʵ���Ų���
 *       �� ��        c0(PA4)  c1(PA5)  c2(PA6)  c3(PA7)
 *       �� r0(PB0)  Num      /        *        -
 *       �� r1(PB1)  7        8        9        +
 *       �� r2(PB2)  4        5        6        (��)
 *       �� r3(PB3)  1        2        3        (��)
 *       �� r4(PB5)  0        .        Enter    (��)
 *
 *   20 ������λ��װ�� 17 ��������λ�� (2,3) / (3,3) / (4,3) ���� Ҳ�������� c3 ��
 *   �� r2 ���¶�ûװ���ء����м����� 1 �����û�� 2U ����
 *   ��λ�� KC_NO�������뱨��������Ҳ�����ӡ�����¼���
 */
// ��/�����ű�
#if !KB_MATRIX_TCA9535
static const uint32_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static const uint32_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

// ��/����������(�����������Ż�һ��)���л����ŷ���ʱ����һ����
static uint32_t row_mask = 0x00;
static uint32_t col_mask = 0x00;
#endif /* !KB_MATRIX_TCA9535 */

#if KB_MATRIX_TCA9535
/* --- CENTER board expander backend (ported from AL_FULL common/Kbd.c,
 *     hardware-verified 2026-10).  Soft I2C: PB0 = SCL, PB1 = SDA. --- */
static tca9535_t s_exp_row;                 /* U1 @0x20: rows */static uint8_t   s_exp_ok = 0;              /* 0 = expanders not ready (retrying) */
static tca9535_t s_exp_col;                 /* U2 @0x21: cols */
#define MATRIX_EXP_ROW_ADDR   0x20
#define MATRIX_EXP_COL_ADDR   0x21
#define MATRIX_EXP_ROW_MASK   0x1F         /* P00-P04 */
static void Matrix_InitExpander(void);
static void Matrix_ScanExpanderRow(uint8_t r, uint16_t *out);
#endif /* KB_MATRIX_TCA9535 */

// ������İ���״̬��1-���£�0-̧��
static uint8_t key_state[MATRIX_ROWS][MATRIX_COLS] = {0};

// ���̱�����{���μ�, �����ֽ�, ������1 ~ ������6}
uint8_t KB_Report[KB_REPORT_SIZE]     = {0};
// �Ѿ��ϱ��������ı����������жϰ���״̬�Ƿ�仯
uint8_t KB_ReportSent[KB_REPORT_SIZE] = {0};
// �����·���ָʾ��״̬(bit0=NumLock, bit1=CapsLock, bit2=ScrollLock)
uint8_t KB_LED_State = 0x00;

// �����¼�(1msɨ���ж����¼����ѭ�����ӡ)�����ڵ�����֤�������
volatile uint8_t KB_EventFlag = 0;
volatile uint8_t KB_EventRow = 0;
volatile uint8_t KB_EventCol = 0;
volatile uint8_t KB_EventDown = 0;

// ԭʼɨ����(δ����)��bit c = 1 ��ʾ (��r, ��c) ��⵽����
#if KB_MATRIX_TCA9535
volatile uint16_t KB_RawMap[MATRIX_ROWS] = {0};
#else
volatile uint8_t KB_RawMap[MATRIX_ROWS] = {0};
#endif
volatile uint8_t KB_RawDumpFlag = 0;    // ɨ�����仯ʱ����ÿ1����һ�Σ���ѭ����ӡ
// ����ʱ��/�����ŵĵ�ƽ(�����ж�������û�б�����/����)
volatile uint8_t KB_RawPinB = 0;
volatile uint8_t KB_RawPinA = 0;

// 1ms ϵͳ���ģ��� TMR0 ɨ���ж��ۼӣ��ƹ�Ч��������ʱ���׼
volatile uint32_t KB_Tick = 0;

#if WS_ENABLE
// WS2812 �Դ棺ÿ�ŵ� 3 �ֽڣ�˳���� G-R-B(WS2812 Ҫ���˳��)���ѳ˹�����
static uint8_t WS_Buf[WS_LED_NUM * 3] = {0};

// ����˳������ƴ��ϵĵ� n �ŵ�װ���ĸ�����λ����(��, ��)��
// ���˳������� PCB �� DIN->DOUT ������˳��Ĭ�ϰ�"���ϵ��¡�������"�š�
// �ϵ���ˮ�ƾ��������˳��һ��һ�ŵ����ģ������ˮ�Ƶ����������ӵ����߲�һ����
// �����ű����У���Ĵ��붼���ö���
static const uint8_t WS_KeyOfLed[WS_LED_NUM][2] = {
    {0, 0}, {0, 1}, {0, 2}, {0, 3},     /* �� 0~3  : Num  /  *  -   */
    {1, 0}, {1, 1}, {1, 2}, {1, 3},     /* �� 4~7  : 7    8  9  +   */
    {2, 0}, {2, 1}, {2, 2},             /* �� 8~10 : 4    5  6      */
    {3, 0}, {3, 1}, {3, 2},             /* �� 11~13: 1    2  3      */
#if KB_ROW4_SEPARATE_KEYS
    {4, 0}, {4, 1}, {4, 3},             /* �� 14~16: 0    .  Enter  */
#else
    {4, 0}, {4, 2}, {4, 3},             /* �� 14~16: 0    .  Enter  */
#endif
};
#endif  /* WS_ENABLE */

/* ------------------------------------------------------------------ *
 *  �����ӿ�
 * ------------------------------------------------------------------ */
// "�ѱ�������ȥ"�ĺ������ɵ�ǰ��Ч�Ĵ����(USB / BLE)ע��
static uint8_t (*s_kb_send)(uint8_t *report) = NULL;

// 1 = �����Ѿ������ձ�����(USB ��ö�� / ���������Ӳ�����)
// �ϵ���ˮ�ƺ��Լ춼Ҫ�������־
static uint8_t kb_host_ready = 0;

// �����Ƿ�������(�� BleKbd.c ����)�������ڼ� WS2812 ˢ��Ҫ�� BLE ��Ƶ�ж�
// �ᵼ�µ��ߣ����� WS_Update ������ 1 ʱ�����ƴ����͡�
volatile uint8_t KB_Ble_Connected = 0;

/* ------------------------------------------------------------------ *
 *  ���ļ��ڵĺ�������
 * ------------------------------------------------------------------ */
static void Matrix_Init(void);
static void LED_Init(void);
static void LED_Update(void);
static void LED_Pin(char port, uint32_t pin, uint8_t on);
#if WS_ENABLE
static void WS_Init(void);
static void WS_Update(void);
static void WS_Show(void);
static void WS_SendBuf(const uint8_t *buf, uint16_t len);
static void WS_LedColor(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
static void WS_ClearBuf(void);
static void WS_Rainbow(uint8_t idx, uint8_t hue_base, uint8_t *r, uint8_t *g, uint8_t *b);
static uint8_t WS_KeyPressed(uint8_t r, uint8_t c);
static void WS_Render(void);
#else
#define WS_Init()
#define WS_Update()
#endif
#if KB_BOOT_SELFTEST
static void KB_TypeChar(uint8_t usage);
#endif

static void Matrix_Init(void)
{
#if KB_MATRIX_TCA9535
    Matrix_InitExpander();
#else  /* V1 GPIO matrix init, unchanged */

    uint8_t i;

    row_mask = 0x00;
    col_mask = 0x00;
    for(i = 0; i < MATRIX_ROWS; i++)
    {
        row_mask |= row_pins[i];
    }
    for(i = 0; i < MATRIX_COLS; i++)
    {
        col_mask |= col_pins[i];
    }

    // ��̬���� = �������룬�� = �������(Ĭ�������)
    GPIOB_ModeCfg(row_mask, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(col_mask, GPIO_ModeOut_PP_5mA);
    GPIOA_SetBits(col_mask);
#endif /* KB_MATRIX_TCA9535 */
}

/*********************************************************************
 * @fn      Matrix_Scan
 *
 * @brief   ����ɨ��(1ms����һ��)��
 *          ��λ1(COL2ROW)��������������������ͣ��������������ƽ��
 *                          �ж����͵�ƽ˵�������н����İ������¡�
 *          ��λ2(����)  ��������������������ͣ��������������ƽ��
 *                        �����϶����ܷ�����(�����öŰ��߶̽���/������
 *                        ����)ʱ������λ2Ҳ�ܲ⵽��������λ�Ľ����������
 *          ������ÿ��������һ���ɼӿɼ��ļ������ӵ�DEBOUNCE_TICKS��Ϊ���£�
 *                ����0��Ϊ̧���м�Ķ�������ı�״̬��
 *
 * @return  none
 */

#if KB_MATRIX_TCA9535
/*********************************************************************
 * @fn      Matrix_InitExpander
 *
 * @brief   Init soft I2C + both TCA9535s (CENTER board).
 *          U1@0x20 P00-P04 rows (active LOW outputs; other P0 pins left
 *          untouched - they carry LED/charge nets).  U2@0x21: P0 all
 *          inputs, P1 P11/P12 outputs low (LED701/702), P10/P13-P17
 *          column inputs with external pull-ups.
 *          NOTE: resetting the MCU does NOT reset the expanders, so a
 *          known state is re-established on every power-up.
 */
static void Matrix_InitExpander(void)
{
    uint8_t rowdir;

    I2cDev_Init();

    Tca9535_Init(&s_exp_row, MATRIX_EXP_ROW_ADDR);
    Tca9535_Init(&s_exp_col, MATRIX_EXP_COL_ADDR);

    if(!Tca9535_IsPresent(&s_exp_row))
    {
        if(s_exp_ok) PRINT("exp: U1(0x20) NOT FOUND -- check SCL/SDA wiring + 3V3/GND\n");
        s_exp_ok = 0;
        return;                     /* row expander missing: no keys, keep running */
    }

    /* rows: touch P00-P04 only, keep the rest of P0 as-is */
    rowdir = (uint8_t)(s_exp_row.cfg[TCA9535_PORT0] & (uint8_t)~MATRIX_EXP_ROW_MASK);
    Tca9535_SetDirection(&s_exp_row, TCA9535_PORT0, rowdir);

    /* all rows high = none selected (active LOW) */
    Tca9535_SetBits(&s_exp_row, TCA9535_PORT0, MATRIX_EXP_ROW_MASK);

    if(!Tca9535_IsPresent(&s_exp_col))
    {
        if(s_exp_ok) PRINT("exp: U1 OK, U2(0x21) NOT FOUND -- col expander wiring\n");
        s_exp_ok = 0;
        return;                     /* col expander missing: rows stay configured */
    }

    Tca9535_SetDirection(&s_exp_col, TCA9535_PORT0, 0xFF);  /* P0 all input */
    Tca9535_SetDirection(&s_exp_col, TCA9535_PORT1, 0xF9);  /* P11/P12 output */
    if(!s_exp_ok) PRINT("exp: U1(0x20)+U2(0x21) both OK, matrix ready\n");
    s_exp_ok = 1;
    Tca9535_ClrBits(&s_exp_col, TCA9535_PORT1, TCA9535_P11 | TCA9535_P12);
}

/*********************************************************************
 * @fn      Matrix_ScanExpanderRow
 *
 * @brief   Drive one row LOW, read both input ports in a single I2C
 *          transaction, decode the 14 column bits (pressed = 1).
 *
 *          Timing: one row = 2 writes + 1 read ~= 500-600us at 60MHz /
 *          ~125kHz soft I2C, so Matrix_Scan() runs ONE row per 1ms TMR0
 *          tick (full scan every 5ms) - same split as AL_FULL.  Scanning
 *          all 5 rows inside one 1ms interrupt would starve the main
 *          loop (USB/BLE stop reporting).
 */
static void Matrix_ScanExpanderRow(uint8_t r, uint16_t *out)
{
    uint8_t  p0, p1;
    uint16_t cols;

    if(r >= MATRIX_ROWS)
    {
        return;
    }

    /* select row r (drive LOW) */
    if(Tca9535_ClrBits(&s_exp_row, TCA9535_PORT0, (uint8_t)(1u << r)) != I2CDEV_OK)
    {
        return;                     /* bus error: skip this row */
    }
    mDelayuS(SCAN_DELAY_US);

    if(Tca9535_ReadPorts(&s_exp_col, &p0, &p1) == I2CDEV_OK)
    {
        /* P00-P07 = COL1-8, P10 = COL9, P13-P17 = COL10-14 */
        cols = (uint16_t)((uint16_t)p0
                          | ((uint16_t)(p1 & 0x01) << 8)
                          | ((uint16_t)(p1 & 0xF8) << 6));
        cols = (uint16_t)~cols;     /* active LOW -> pressed = 1 */
        *out = (uint16_t)(cols & (uint16_t)((1u << MATRIX_COLS) - 1u));
    }

    /* release row r */
    (void)Tca9535_SetBits(&s_exp_row, TCA9535_PORT0, (uint8_t)(1u << r));
}
#endif /* KB_MATRIX_TCA9535 */

void Matrix_Scan(void)
{
#if KB_MATRIX_TCA9535

    /* ---------------- CENTER board: TCA9535 expander scan ----------------
     * One row per 1ms TMR0 tick (full scan = 5ms).  Debounce counters use
     * the same DEBOUNCE_TICKS rule as V1, sampled at 5ms per row. */
    static uint8_t  debounce_cnt[MATRIX_ROWS][MATRIX_COLS] = {0};
    static uint8_t  scan_row = 0;
    static uint16_t retry_cnt = 0;
    static uint8_t  retry_print = 0;

    if(!s_exp_ok)
    {   /* auto-retry init: covers power-sequencing / late FFC plug-in */
        if(++retry_cnt >= 1000)             /* every 1s */
        {
            retry_cnt = 0;
            retry_print++;
            Matrix_InitExpander();
            if(!s_exp_ok && (retry_print % 5u) == 0)
            {   /* probe all 8 TCA9535 address straps: tells dead bus vs wrong addr */
                uint8_t aa;
                uint8_t found = 0;
                PRINT("exp: U1/U2 still NOT FOUND, bus scan 0x20-0x27:");
                for(aa = 0x20; aa <= 0x27; aa++)
                {
                    if(I2cDev_Ping(aa))
                    {
                        PRINT(" 0x%02x", aa);
                        found++;
                    }
                }
                if(found == 0)
                {
                    PRINT(" -> no ACK at all (SCL/SDA broken, no pullup, or expander unpowered)");
                }
                PRINT("\n");
            }
        }
        if(!s_exp_ok) return;
    }
#if KB_RAW_DUMP
    static uint16_t dump_timer = 0;
    static uint16_t last_raw[MATRIX_ROWS] = {0};
#endif
    uint8_t  r = scan_row;
    uint8_t  c;
    uint16_t rowbits = 0;

    scan_row = (uint8_t)((scan_row + 1u) % MATRIX_ROWS);

    Matrix_ScanExpanderRow(r, &rowbits);

    for(c = 0; c < MATRIX_COLS; c++)
    {
        if(rowbits & (1u << c))                       /* key detected */
        {
            if(debounce_cnt[r][c] < DEBOUNCE_TICKS)
            {
                debounce_cnt[r][c]++;
            }
        }
        else                                          /* key released */
        {
            if(debounce_cnt[r][c] > 0)
            {
                debounce_cnt[r][c]--;
            }
        }

        if((debounce_cnt[r][c] >= DEBOUNCE_TICKS) && (key_state[r][c] == 0))
        {
            key_state[r][c] = 1;
            KB_EventRow = r;
            KB_EventCol = c;
            KB_EventDown = 1;
            KB_EventFlag = 1;
        }
        else if((debounce_cnt[r][c] == 0) && (key_state[r][c] == 1))
        {
            key_state[r][c] = 0;
            KB_EventRow = r;
            KB_EventCol = c;
            KB_EventDown = 0;
            KB_EventFlag = 1;
        }
    }

#if KB_RAW_DUMP
    KB_RawMap[r] = rowbits;
    if(rowbits != last_raw[r])
    {
        last_raw[r] = rowbits;
        dump_timer = 0;
        KB_RawDumpFlag = 1;
    }
    else if(dump_timer < 1000)
    {
        dump_timer++;
        if(dump_timer >= 1000)
        {
            dump_timer = 0;
            KB_RawDumpFlag = 1;       /* periodic heartbeat dump every 1s */
        }
    }
#endif

#else  /* !KB_MATRIX_TCA9535: original V1 GPIO scan, byte-for-byte unchanged */

    static uint8_t  debounce_cnt[MATRIX_ROWS][MATRIX_COLS] = {0};
#if KB_RAW_DUMP
    static uint16_t dump_timer = 0;
#endif
    uint8_t r, c;
    uint8_t raw[MATRIX_ROWS];       // bit c = 1 ��ʾ(��r,��c)��⵽����

    for(r = 0; r < MATRIX_ROWS; r++)
    {
        raw[r] = 0;
    }

    //---------- ��λ1�������(��������)������������ ----------
#if MATRIX_SCAN_BOTH_WAYS
    GPIOB_ModeCfg(row_mask, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(col_mask, GPIO_ModeOut_PP_5mA);
    GPIOA_SetBits(col_mask);
#endif
    for(c = 0; c < MATRIX_COLS; c++)
    {
        GPIOA_ResetBits(col_pins[c]);       // ��ǰ������
        mDelayuS(SCAN_DELAY_US);            // �ȴ��е�ƽ�ȶ�

        for(r = 0; r < MATRIX_ROWS; r++)
        {
            if(GPIOB_ReadPortPin(row_pins[r]) == 0)     // �б����� => ��������
            {
                raw[r] |= (1 << c);
            }
        }

        GPIOA_SetBits(col_pins[c]);         // ��ǰ�лָ��ߵ�ƽ
    }

    //---------- ��λ2�������(��������)������������ ----------
#if MATRIX_SCAN_BOTH_WAYS
    GPIOA_ModeCfg(col_mask, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(row_mask, GPIO_ModeOut_PP_5mA);
    GPIOB_SetBits(row_mask);
    for(r = 0; r < MATRIX_ROWS; r++)
    {
        GPIOB_ResetBits(row_pins[r]);       // ��ǰ������
        mDelayuS(SCAN_DELAY_US);

        for(c = 0; c < MATRIX_COLS; c++)
        {
            if(GPIOA_ReadPortPin(col_pins[c]) == 0)     // �б����� => ��������
            {
                raw[r] |= (1 << c);
            }
        }

        GPIOB_SetBits(row_pins[r]);
    }

    // �ָ���̬�����������룬�����������
    GPIOB_ModeCfg(row_mask, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(col_mask, GPIO_ModeOut_PP_5mA);
    GPIOA_SetBits(col_mask);
#endif

    //---------- ���� + ״̬���� ----------
    for(r = 0; r < MATRIX_ROWS; r++)
    {
        for(c = 0; c < MATRIX_COLS; c++)
        {
            if(raw[r] & (1 << c))                       // ��⵽����
            {
                if(debounce_cnt[r][c] < DEBOUNCE_TICKS)
                {
                    debounce_cnt[r][c]++;
                }
            }
            else                                        // ̧��
            {
                if(debounce_cnt[r][c] > 0)
                {
                    debounce_cnt[r][c]--;
                }
            }

            if((debounce_cnt[r][c] >= DEBOUNCE_TICKS) && (key_state[r][c] == 0))
            {
                key_state[r][c] = 1;                    // ����
                KB_EventRow = r;                        // ֻ����¼����ӡ�ŵ���ѭ��������
                KB_EventCol = c;                        // �ж��ﲻҪ�����ڴ�ӡ(̫��)
                KB_EventDown = 1;
                KB_EventFlag = 1;
            }
            else if((debounce_cnt[r][c] == 0) && (key_state[r][c] == 1))
            {
                key_state[r][c] = 0;                    // ̧��
                KB_EventRow = r;
                KB_EventCol = c;
                KB_EventDown = 0;
                KB_EventFlag = 1;
            }
        }
    }

#if KB_RAW_DUMP
    //---------- ԭʼɨ�����仯ʱ������ӡ������ÿ1���ٴ�һ�� ----------
    {
        static uint8_t last_raw[MATRIX_ROWS] = {0};
        uint8_t changed = 0;

        for(r = 0; r < MATRIX_ROWS; r++)
        {
            if(raw[r] != last_raw[r])
            {
                changed = 1;
                last_raw[r] = raw[r];
                KB_RawMap[r] = raw[r];
            }
        }

        if(dump_timer < 1000)
        {
            dump_timer++;
        }

        // �仯��20ms�ھ��ܴ�������������һֱ�������Ҳ��50��/�룬����Ѵ���ˢ��
        if((changed && (dump_timer >= 20)) || (dump_timer >= 1000))
        {
            dump_timer = 0;
            // ��ʱ�����ѻָ���̬(������ߡ�����������)��˳�������ʵ�ʵ�ƽ�����
            KB_RawPinB = (uint8_t)(GPIOB_ReadPort() & row_mask);
            KB_RawPinA = (uint8_t)(GPIOA_ReadPort() & col_mask);
            KB_RawDumpFlag = 1;
        }
    }
#endif
#endif /* KB_MATRIX_TCA9535 */
}

/*********************************************************************
 * @fn      KB_ReportBuild
 *
 * @brief   ���ݵ�ǰ����״̬����8�ֽڼ��̱�����
 *          ������û�����μ������Ե�0�ֽڹ̶�Ϊ0������ϱ�6��������
 *          ͬһ������ֻ�ϱ�һ��(����)����һ�Ժ����������λӳ�䵽ͬһ�����룬
 *          Ҳֻ�����һ�Ρ����� 17 �������붼���ظ���
 *          ��λ((2,3)/(3,3)/(4,3))����� KC_NO�������뱨����
 *
 * @param   report  - �����������(8�ֽ�)
 *
 * @return  none
 */
uint8_t KB_KeyPressed(uint8_t row, uint8_t col)
{
    if((row >= MATRIX_ROWS) || (col >= MATRIX_COLS))
    {
        return 0;
    }
    return key_state[row][col];
}

void KB_ReportBuild(uint8_t *report)
{
    uint8_t r, c, n = 2, i, dup, mods = 0, key;
    uint16_t keycode;

    memset(report, 0, KB_REPORT_SIZE);
    Vial_UpdateLayer();

    for(r = 0; r < MATRIX_ROWS; r++)
    {
        for(c = 0; c < MATRIX_COLS; c++)
        {
            if(key_state[r][c])
            {
                keycode = Vial_ResolveKeycode(Vial_GetLayer(), r, c);
                if(!Vial_KeycodeToHid(keycode, &mods, &key) || (key == 0))
                {
                    continue;
                }
                if(n >= KB_REPORT_SIZE)             // ����6�������İ��²����ϱ�
                {
                    continue;
                }

                dup = 0;                            // ��һ����������ǲ����Ѿ��ڱ�������
                for(i = 2; i < n; i++)
                {
                    if(report[i] == key)
                    {
                        dup = 1;
                        break;
                    }
                }
                if(dup == 0)
                {
                    report[n++] = key;
                }
            }
        }
    }
    report[0] = mods;
}

void KB_NKRO_ReportBuild(uint8_t *report)
{
    uint8_t r, c, mods, key;
    uint16_t keycode;

    memset(report, 0, KB_BLE_NKRO_REPORT_SIZE);
    Vial_UpdateLayer();

    for(r = 0; r < MATRIX_ROWS; r++)
    {
        for(c = 0; c < MATRIX_COLS; c++)
        {
            if(!key_state[r][c])
            {
                continue;
            }

            keycode = Vial_ResolveKeycode(Vial_GetLayer(), r, c);
            mods = report[0];
            if(Vial_KeycodeToHid(keycode, &mods, &key))
            {
                report[0] = mods;
                if((key >= 0x04) && (key <= 0x65))
                {
                    uint8_t bit = (uint8_t)(key - 0x04);
                    report[1 + (bit >> 3)] |= (uint8_t)(1U << (bit & 0x07));
                }
            }
        }
    }
}

/*********************************************************************
 * @fn      KB_SetSendFunc
 *
 * @brief   ע�ᵱǰ�����ķ��ͺ���(�� Kbd.h)
 *
 * @return  none
 */
void KB_SetSendFunc(uint8_t (*fn)(uint8_t *report))
{
    s_kb_send = fn;
    kb_host_ready = 0;              // �������/����ö�ٶ�Ҫ���µ���������
}

/*********************************************************************
 * @fn      KB_Send
 *
 * @brief   ��ע��õķ��ͺ�����һ�α���(�� Kbd.h)
 *
 * @return  1-�ѷ���, 0-����ͨ����������
 */
uint8_t KB_Send(uint8_t *report)
{
    if(s_kb_send == NULL)
    {
        return 0;
    }

    if(s_kb_send(report) == 0)
    {
        return 0;
    }

    kb_host_ready = 1;              // ���ٷ���ȥ��һ�� => ����ͨ����ͨ��
    return 1;
}

/*********************************************************************
 * @fn      KB_SetLEDState
 *
 * @brief   ��¼�����·���ָʾ��״̬(�� Kbd.h)
 *
 * @return  none
 */
void KB_SetLEDState(uint8_t state)
{
    KB_LED_State = state;
}

/*********************************************************************
 * @fn      KB_ResetReport
 *
 * @brief   �������״̬�ͱ�������(�� Kbd.h)
 *
 * @return  none
 */
void KB_ResetReport(void)
{
    uint8_t r, c;

    for(r = 0; r < MATRIX_ROWS; r++)
    {
        for(c = 0; c < MATRIX_COLS; c++)
        {
            key_state[r][c] = 0;
        }
    }

    memset(KB_Report, 0, KB_REPORT_SIZE);
    memset(KB_ReportSent, 0, KB_REPORT_SIZE);
}

#if KB_BOOT_SELFTEST
/*********************************************************************
 * @fn      KB_TypeChar
 *
 * @brief   ֱ�Ӱ�һ�����ɿ�ĳ������(����������)��������֤�ϱ�ͨ��
 *
 * @param   usage   - HID����
 *
 * @return  none
 */
static void KB_TypeChar(uint8_t usage)
{
    uint8_t rep[KB_REPORT_SIZE] = {0};
    uint8_t rel[KB_REPORT_SIZE] = {0};
    uint16_t t;

    rep[2] = usage;

    // ���£�����һֱ����ȡ������ 100ms�������ѭ������
    for(t = 0; t < 100; t++)
    {
        if(KB_Send(rep))
        {
            break;
        }
        mDelaymS(1);
    }
    if(t == 100)
    {
        PRINT("selftest: report channel busy (press)\n");
        return;
    }
    mDelaymS(30);

    // �ɿ�
    for(t = 0; t < 100; t++)
    {
        if(KB_Send(rel))
        {
            break;
        }
        mDelaymS(1);
    }
    if(t == 100)
    {
        PRINT("selftest: report channel busy (release)\n");
        return;
    }
    mDelaymS(30);

    memcpy(KB_ReportSent, rel, KB_REPORT_SIZE);     // ͬ��һ�£������ѭ���������ط�
    PRINT("selftest: typed 0x%02x\n", usage);
}
#endif

/*********************************************************************
 * @fn      KB_Task
 *
 * @brief   ����������(�� Kbd.h)
 *
 * @return  none
 */
void KB_Task(void)
{
    static uint32_t last_send_tick = 0;
    static uint32_t last_attempt_tick = 0;
    static uint8_t  send_pending = 1;   /* ��һ��/�ϴ�û���ɹ������������� */
    uint8_t key_state_changed = KB_EventFlag;
#if KB_BOOT_SELFTEST
    static uint16_t selftest_wait = 0;
    static uint8_t  selftest_done = 0;
#endif

    LED_Update();                   // �����·���NumLock/CapsLock/ScrollLockָʾ��
    WS_Update();                    // WS2812�ƴ����ϵ���ˮ�� + ������Ӧ/�ʺ�Ч��

#if KB_EVENT_PRINT
    if(KB_EventFlag)                // ��ӡ�����¼������������֤�������
    {
        KB_EventFlag = 0;
        PRINT("key %s: r%d c%d -> KC 0x%04x\n", KB_EventDown ? "down" : "up  ",
              KB_EventRow, KB_EventCol,
              Vial_ResolveKeycode(Vial_GetLayer(), KB_EventRow, KB_EventCol));
    }
#else
    KB_EventFlag = 0;               // ����ӡҲҪ���־������ѻ�
#endif

#if KB_RAW_DUMP
    if(KB_RawDumpFlag)              // ����ԭʼɨ����(δ����)���仯ʱ������, ����ÿ���һ��
    {
        KB_RawDumpFlag = 0;
#if KB_MATRIX_TCA9535
        PRINT("raw r0..r4=%04x %04x %04x %04x %04x  host=%d LED=0x%02x\n",
              KB_RawMap[0], KB_RawMap[1], KB_RawMap[2], KB_RawMap[3], KB_RawMap[4],
              kb_host_ready, KB_LED_State);
#else
        PRINT("raw r0..r4=%02x %02x %02x %02x %02x  PB=%02x PA=%02x  host=%d LED=0x%02x\n",
              KB_RawMap[0], KB_RawMap[1], KB_RawMap[2], KB_RawMap[3], KB_RawMap[4],
              KB_RawPinB, KB_RawPinA, kb_host_ready, KB_LED_State);
#endif
    }
#endif

    KB_ReportBuild(KB_Report);      // ���ɵ�ǰ����״̬��Ӧ�ı���

    // ״̬�б仯�����߾��ϴγɹ��ϱ����� KB_REFRESH_TICKS ���룬���ط�һ�Ρ�
    // ע�⣺����ģʽ�� BleKbd_Process() �ǽ�ѭ����KB_Task() �ĵ��ü��ԶС�� 1ms��
    // ������������� 1ms �� KB_Tick ��ʱ�������ô�����ʱ����ÿ����ǧ���ط���
    // ��Э��ջ�� GATT ����ռ���������İ��������������ױ�������
    if(key_state_changed ||
       send_pending ||
       memcmp(KB_Report, KB_ReportSent, KB_REPORT_SIZE) ||
       ((uint32_t)(KB_Tick - last_send_tick) >= KB_REFRESH_TICKS))
    {
        if((uint32_t)(KB_Tick - last_attempt_tick) >= KB_SEND_RETRY_TICKS)
        {
            last_attempt_tick = KB_Tick;
            if(KB_Send(KB_Report))
            {
                memcpy(KB_ReportSent, KB_Report, KB_REPORT_SIZE);
                last_send_tick = KB_Tick;
                send_pending = 0;
            }
            else
            {
                send_pending = 1;   // ͨ����ʱ�����ã�10ms ������
            }
        }
    }

#if KB_BOOT_SELFTEST
    // �ϵ��Լ죺������ͨ��ͨ���ٵ�Լ1.5��(��Windows/�ֻ��Ѽ�����������)��Ȼ���"ok"
    if(!selftest_done && kb_host_ready)
    {
        if(selftest_wait < 1500)
        {
            selftest_wait++;
        }
        else
        {
            selftest_done = 1;
            PRINT("selftest start\n");
            KB_TypeChar(KC_O);
            KB_TypeChar(KC_K);
            PRINT("selftest end\n");
        }
    }
#endif
}

/*********************************************************************
 * @fn      KeyboardInit
 *
 * @brief   ���̳�ʼ��������GPIO + ָʾ��GPIO + WS2812�ƴ� + 1msɨ�趨ʱ��
 *
 * @return  none
 */
void KeyboardInit(void)
{
    Matrix_Init();
    LED_Init();
    WS_Init();

    TMR0_TimerInit(FREQ_SYS / 1000);            // ��ʱ����1ms
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    // �ؼ����� TMR0 ��Ϊ�����ռ���ȼ�(bit7=1)������ 1ms ɨ�����
    // mDelayuS ��תԼ 90us ������ BLE ��Ƶ�жϣ��������/���Ӽ�Ъ��ʧ�ܡ�
    PFIC_SetPriority(TMR0_IRQn, 0xf0);
    PFIC_EnableIRQ(TMR0_IRQn);
}

/*********************************************************************
 * @fn      LED_Pin
 *
 * @brief   ����һ��ָʾ�ƣ������Ծ�������ߵ͵�ƽ
 *
 * @param   port    - 'A' �� 'B'
 * @param   pin     - ��������(GPIO_Pin_x)��д0��ʾ��ŵ�û��
 * @param   on      - 1-����, 0-Ϩ��
 *
 * @return  none
 */
static void LED_Pin(char port, uint32_t pin, uint8_t on)
{
    if(pin == 0)
    {
        return;                             // ��ŵ�û�ӣ���ȥ���κ�����
    }

#if KB_LED_ACTIVE_LOW
    on = (on ? 0 : 1);                      // �͵�ƽ������ȡ��
#endif

    if(port == 'A')
    {
        if(on)
        {
            GPIOA_SetBits(pin);
        }
        else
        {
            GPIOA_ResetBits(pin);
        }
    }
    else
    {
        if(on)
        {
            GPIOB_SetBits(pin);
        }
        else
        {
            GPIOB_ResetBits(pin);
        }
    }
}

/*********************************************************************
 * @fn      LED_Init
 *
 * @brief   ָʾ�Ƴ�ʼ�����ѽ��˵Ƶ�������������������ȫ��Ϩ��
 *
 * @return  none
 */
static void LED_Init(void)
{
    if(KB_LED_NUM_PIN)
    {
        if(KB_LED_NUM_PORT == 'A')
        {
            GPIOA_ModeCfg(KB_LED_NUM_PIN, GPIO_ModeOut_PP_5mA);
        }
        else
        {
            GPIOB_ModeCfg(KB_LED_NUM_PIN, GPIO_ModeOut_PP_5mA);
        }
    }
    if(KB_LED_CAPS_PIN)
    {
        if(KB_LED_CAPS_PORT == 'A')
        {
            GPIOA_ModeCfg(KB_LED_CAPS_PIN, GPIO_ModeOut_PP_5mA);
        }
        else
        {
            GPIOB_ModeCfg(KB_LED_CAPS_PIN, GPIO_ModeOut_PP_5mA);
        }
    }
    if(KB_LED_SCROLL_PIN)
    {
        if(KB_LED_SCROLL_PORT == 'A')
        {
            GPIOA_ModeCfg(KB_LED_SCROLL_PIN, GPIO_ModeOut_PP_5mA);
        }
        else
        {
            GPIOB_ModeCfg(KB_LED_SCROLL_PIN, GPIO_ModeOut_PP_5mA);
        }
    }

    LED_Pin(KB_LED_NUM_PORT,    KB_LED_NUM_PIN,    0);      // �ϵ���ȫ��Ϩ��
    LED_Pin(KB_LED_CAPS_PORT,   KB_LED_CAPS_PIN,   0);
    LED_Pin(KB_LED_SCROLL_PORT, KB_LED_SCROLL_PIN, 0);
}

/*********************************************************************
 * @fn      LED_Update
 *
 * @brief   ���������·���LED����ˢ������ָʾ�ơ�
 *          �����ֽڣ�bit0=NumLock, bit1=CapsLock, bit2=ScrollLock
 *          ��ѭ���ﷴ�����ã�״̬û���ʲôҲ������
 *
 * @return  none
 */
static void LED_Update(void)
{
    static uint8_t last_state = 0xFF;       // 0xFF��֤��һ��һ����ˢ��

    if(KB_LED_State == last_state)
    {
        return;
    }
    last_state = KB_LED_State;

    LED_Pin(KB_LED_NUM_PORT,    KB_LED_NUM_PIN,    (KB_LED_State & 0x01) ? 1 : 0);
    LED_Pin(KB_LED_CAPS_PORT,   KB_LED_CAPS_PIN,   (KB_LED_State & 0x02) ? 1 : 0);
    LED_Pin(KB_LED_SCROLL_PORT, KB_LED_SCROLL_PIN, (KB_LED_State & 0x04) ? 1 : 0);
}

#if WS_ENABLE
/*********************************************************************
 * @fn      WS_SendBuf
 *
 * @brief   ���Դ水 WS2812 ʱ���Ƹ��ƴ���
 *          ʱ���� TMR3 ���ɼ���������(1 �� tick = 1 ��ϵͳʱ������)��
 *          ���Ժ���Ƶ���������Ż������޹أ�
 *            ȡ��ǰ���� t0 -> ���� -> �ȵ� t0+T1H(1��) �� t0+T0H(0��) -> ����
 *            -> �ȵ� t0+λ����(1.25us)��
 *          ��λ�ȷ�(MSB first)��8 �� bit һ���ֽڣ�G-R-B ˳��
 *
 * @param   buf  - �Դ�
 * @param   len  - �ֽ���
 *
 * @return  none
 */
static void WS_SendBuf(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t  n, b;
    uint32_t t0, th;

    for(i = 0; i < len; i++)
    {
        b = buf[i];
        for(n = 0; n < 8; n++)
        {
            t0 = R32_TMR3_COUNT;                        // ��һλ�����
            WS_PIN_HIGH();
            th = (b & 0x80) ? WS_T1H : WS_T0H;          // 1��ߵ�ƽ��, 0���
            while((uint32_t)(R32_TMR3_COUNT - t0) < th)
            {
            }
            WS_PIN_LOW();
            while((uint32_t)(R32_TMR3_COUNT - t0) < WS_PERIOD)  // ����һ��λ����
            {
            }
            b <<= 1;
        }
    }
}

/*********************************************************************
 * @fn      WS_LedColor
 *
 * @brief   ���õ� idx �ŵƵ���ɫ(�ڲ��� WS_BRIGHT ���ţ����� GRB ����Դ�)
 *
 * @param   idx  - ������� 0 ~ WS_LED_NUM-1
 * @param   r,g,b - ��ɫ 0~255
 *
 * @return  none
 */
static void WS_LedColor(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if(idx >= WS_LED_NUM)
    {
        return;
    }

    WS_Buf[idx * 3 + 0] = (uint8_t)(((uint16_t)g * WS_BRIGHT) >> 8);    // G
    WS_Buf[idx * 3 + 1] = (uint8_t)(((uint16_t)r * WS_BRIGHT) >> 8);    // R
    WS_Buf[idx * 3 + 2] = (uint8_t)(((uint16_t)b * WS_BRIGHT) >> 8);    // B
}

/*********************************************************************
 * @fn      WS_ClearBuf
 *
 * @brief   �Դ�ȫ���� 0(Ϩ��)
 *
 * @return  none
 */
static void WS_ClearBuf(void)
{
    memset(WS_Buf, 0, sizeof(WS_Buf));
}

/*********************************************************************
 * @fn      WS_Rainbow
 *
 * @brief   �ʺ�ɫ�������������� 6 �����Թ��ɵ� RGB�����ø���
 *
 * @param   idx      - �������
 * @param   hue_base - ɫ���׼����ʱ��仯������������
 * @param   r,g,b    - �����ɫ
 *
 * @return  none
 */
static void WS_Rainbow(uint8_t idx, uint8_t hue_base, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t h = (uint8_t)(hue_base + (uint16_t)idx * 256 / WS_LED_NUM);     // 0~255
    uint8_t seg = (uint8_t)(h / 43);                                       // 0~5 ����
    uint8_t frac = (uint8_t)(h - seg * 43);
    uint8_t q = (uint8_t)(((uint16_t)frac * 255) / 43);                     // ����
    uint8_t t = (uint8_t)(255 - q);                                        // �ݼ�

    switch(seg)
    {
        case 0:  *r = 255; *g = q;   *b = 0;   break;
        case 1:  *r = t;   *g = 255; *b = 0;   break;
        case 2:  *r = 0;   *g = 255; *b = q;   break;
        case 3:  *r = 0;   *g = t;   *b = 255; break;
        case 4:  *r = q;   *g = 0;   *b = 255; break;
        default: *r = 255; *g = 0;   *b = t;   break;
    }
}

/*********************************************************************
 * @fn      WS_KeyPressed
 *
 * @brief   �ж�ĳ������λ�øò�������
 *          һ������¾��ǿ�����û���£���������һ���£���� keymap ���б�ĸ�
 *          �ͱ��������ͬ(������һ���������������λӳ�䵽ͬһ������ʱ���õ�)��
 *          ֻҪ��Щ����һ�����£������Ӧ�ĵ�Ҳ�������� 17 ����һһ��Ӧ���ò�����㡣
 *
 * @param   r, c - ��������
 *
 * @return  1-����, 0-û����
 */
static uint8_t WS_KeyPressed(uint8_t r, uint8_t c)
{
    uint8_t rr, cc;
    uint16_t code;

    if(key_state[r][c])
    {
        return 1;
    }

    code = Vial_ResolveKeycode(Vial_GetLayer(), r, c);
    if(code == KC_NO)
    {
        return 0;
    }

    for(rr = 0; rr < MATRIX_ROWS; rr++)
    {
        for(cc = 0; cc < MATRIX_COLS; cc++)
        {
            if(((rr != r) || (cc != c)) &&
               (Vial_ResolveKeycode(Vial_GetLayer(), rr, cc) == code) &&
               key_state[rr][cc])
            {
                return 1;
            }
        }
    }
    return 0;
}

/*********************************************************************
 * @fn      WS_Render
 *
 * @brief   ����ǰ����״̬/Ч����Ⱦ�Դ�(ֻ���ڴ棬������)
 *
 * @return  none
 */
static void WS_Render(void)
{
    uint8_t n, r, c, rr, gg, bb;

    for(n = 0; n < WS_LED_NUM; n++)
    {
        r = WS_KeyOfLed[n][0];
        c = WS_KeyOfLed[n][1];

#if (WS_EFFECT == WS_EFFECT_RAINBOW)
        WS_Rainbow(n, (uint8_t)(KB_Tick >> 5), &rr, &gg, &bb);      // ÿ32ms����һ��
        if(WS_KeyPressed(r, c))                                     // ���µļ��ǳɰ�ɫ
        {
            rr = WS_COLOR_KEY_R;
            gg = WS_COLOR_KEY_G;
            bb = WS_COLOR_KEY_B;
        }
#else
        if(WS_KeyPressed(r, c))                                     // ��������
        {
            rr = WS_COLOR_KEY_R;
            gg = WS_COLOR_KEY_G;
            bb = WS_COLOR_KEY_B;
        }
        else if((Vial_ResolveKeycode(Vial_GetLayer(), r, c) == KC_NUM) &&
                (KB_LED_State & 0x01))                              // NumLock ָʾ��
        {
            rr = WS_COLOR_NUM_R;
            gg = WS_COLOR_NUM_G;
            bb = WS_COLOR_NUM_B;
        }
        else                                                        // ��ɫ
        {
            rr = WS_COLOR_IDLE_R;
            gg = WS_COLOR_IDLE_G;
            bb = WS_COLOR_IDLE_B;
        }
#endif

        WS_LedColor(n, rr, gg, bb);
    }
}

/*********************************************************************
 * @fn      WS_Show
 *
 * @brief   ���Դ��Ƹ��ƴ���
 *          �����ڼ�Ҫ�ص�ɨ���жϡ�USB�жϺ� RTC �жϣ���֡
 *          17*24*1.25us �� 0.5ms���м䱻��ϵĻ�ʱ�������(�ƻ�����ɫ)��
 *          ����ģʽ�� RTC �� TMOS ��ʱ����Ҳֻ���� 0.5ms��
 *          ����Э��ջ������Ϊ�ټ�ʮ�� tick �����ߡ�
 *          ֻ����ɫ�б仯ʱ�ŵ��ã�����ƽ��ռ�ú�С��
 *
 * @return  none
 */
static void WS_Show(void)
{
    static uint32_t last_tick = 0;
    uint8_t  was_tmr0, was_usb, was_rtc, was_ble;

    if((KB_Tick - last_tick) < 2)       // ����һ֡��̫���͵�һ�£���֤��λ�͵�ƽ����
    {
        mDelayuS(300);
    }

    // ��¼����ǰ���ж�ʹ��״̬�������갴ԭ���ָ�����������ģʽ�»��
    // UsbHid_Exit() �ص��� USB �ж��ִ�(Ready_WeakUp ������ô����)��
    was_tmr0 = PFIC_GetStatusIRQ(TMR0_IRQn) ? 1 : 0;
    was_usb  = PFIC_GetStatusIRQ(USB_IRQn)  ? 1 : 0;
    was_rtc  = PFIC_GetStatusIRQ(RTC_IRQn)  ? 1 : 0;
#if defined(BLE_IRQn)
    was_ble  = PFIC_GetStatusIRQ(BLE_IRQn)  ? 1 : 0;
#endif

    PFIC_DisableIRQ(TMR0_IRQn);         // ����1msɨ����ʱ��
    PFIC_DisableIRQ(USB_IRQn);
    PFIC_DisableIRQ(RTC_IRQn);
#if defined(BLE_IRQn)
    PFIC_DisableIRQ(BLE_IRQn);          // ����ģʽ�²�������ж�
#endif

    WS_SendBuf(WS_Buf, sizeof(WS_Buf));

    if(was_tmr0) PFIC_EnableIRQ(TMR0_IRQn);
    if(was_usb)  PFIC_EnableIRQ(USB_IRQn);
    if(was_rtc)  PFIC_EnableIRQ(RTC_IRQn);
#if defined(BLE_IRQn)
    if(was_ble)  PFIC_EnableIRQ(BLE_IRQn);
#endif

    last_tick = KB_Tick;
}

/*********************************************************************
 * @fn      WS_Init
 *
 * @brief   �ƴ���ʼ�������ݽ����������������ͣ�TMR3 �������ɼ�������
 *          Ȼ����һ�εƴ�(�ϵ籾��������ģ��������)
 *
 * @return  none
 */
static void WS_Init(void)
{
    WS_PIN_CFG();
    WS_PIN_LOW();

    TMR3_TimerInit(0x03FFFFFF);         // 1 tick = 1 ��ϵͳʱ�ӣ���λ26λԼ1.1�����

    WS_ClearBuf();
    WS_Show();
}

/*********************************************************************
 * @fn      WS_Update
 *
 * @brief   ��ѭ���ﷴ�����ã�
 *          1. �ϵ���ˮ��(WS_BOOT_CHASE)������ͨ��ͨ��֮��һ��һ�ŵ������ظ�
 *             WS_CHASE_ROUNDS �飬�����˶Ե���˳��
 *          2. ��Ч����Ⱦ�Դ棬��ɫ�б仯���������͡�
 *
 * @return  none
 */
static void WS_Update(void)
{
    static uint8_t  last_buf[WS_LED_NUM * 3] = {0};
#if WS_BOOT_CHASE
    static uint8_t  chase_on = 1;
    static uint8_t  chase_idx = 0;
    static uint8_t  chase_round = 0;
    static uint32_t chase_tick = 0;
#endif
#if (WS_EFFECT == WS_EFFECT_RAINBOW)
    static uint32_t anim_tick = 0;
#endif

    if(KB_Ble_Connected)   // BLE �����У�WS_Show ��� BLE ��Ƶ�жϵ��µ��ߣ���ͣ��Ч
    {
        return;
    }

#if WS_BOOT_CHASE
    if(chase_on)
    {
        if(kb_host_ready && ((KB_Tick - chase_tick) >= 90))     // ÿ90ms��һ��
        {
            chase_tick = KB_Tick;
            WS_ClearBuf();
            if(chase_idx < WS_LED_NUM)
            {
                WS_LedColor(chase_idx, WS_COLOR_KEY_R, WS_COLOR_KEY_G, WS_COLOR_KEY_B);
                PRINT("ws: round %d led %d\n", chase_round, chase_idx);
                chase_idx++;
            }
            else
            {
                chase_idx = 0;
                chase_round++;
                if(chase_round >= WS_CHASE_ROUNDS)              // �ܹ�������չ�
                {
                    chase_on = 0;
                    PRINT("ws: chase done\n");
                }
            }
            WS_Show();
            memcpy(last_buf, WS_Buf, sizeof(WS_Buf));
        }
        return;
    }
#endif

#if (WS_EFFECT == WS_EFFECT_RAINBOW)
    if((KB_Tick - anim_tick) < 30)      // �ʺ�ÿ30msˢ��һ�ξ͹���
    {
        return;
    }
    anim_tick = KB_Tick;
#endif

    WS_Render();
    if(memcmp(WS_Buf, last_buf, sizeof(WS_Buf)))    // û�仯�Ͳ�����
    {
        memcpy(last_buf, WS_Buf, sizeof(WS_Buf));
        WS_Show();
    }
}
#endif  /* WS_ENABLE */

/*********************************************************************
 * @fn      TMR0_IRQHandler
 *
 * @brief   ��ʱ��0�жϷ�������1msɨ��һ�μ��̾��󣬲��ۼ�ϵͳ���ġ�
 *          ���ִ���ģʽ��������ж���ɨ�裻������ 625us TMOS ʱ���� RTC��
 *          �� TMR0 ����ͻ��
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
    if(TMR0_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
        KB_Tick++;                  // 1ms ���ģ��ƹ�Ч��������ʱ
        Matrix_Scan();
    }
}

/******************************** endfile @ Kbd ********************************/
