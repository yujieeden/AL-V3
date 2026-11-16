/********************************** (C) COPYRIGHT *******************************
 * File Name          : Vial.h
 * Description        : VIAL 改键模块：插上 USB 后用 VIAL GUI 实时改键位，改完存在
 *                      芯片的 data-flash 里，掉电保留。
 *
 *                      这个模块只做三件事：
 *                        1. 解析 VIA / VIAL 协议(32 字节 raw HID 包)
 *                        2. 维护 4 层动态键位(内存镜像 + data-flash 存取)
 *                        3. 把 QMK 键码翻译成 HID 报表的"修饰键字节 + 键码"
 *
 *                      它不认识 USB 也不认识蓝牙：谁要发包谁就用
 *                      Vial_SetSendFunc() 注册一个发送函数(和 Kbd.c 的
 *                      KB_SetSendFunc 一个套路)，收到包时调 Vial_RawHidReceive()。
 *
 *                      和别的目录的关系：BLE_HID/、StdPeriphDriver/ 一行不改；
 *                      本模块只 include Kbd.h(要矩阵行列数)，别的都不依赖。
 *                      目前只在 USB 模式下工作(蓝牙模式下 USB 已经关掉了)。
 *******************************************************************************/

#ifndef __VIAL_H
#define __VIAL_H

#include "CH58x_common.h"
#include "Kbd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 *  配置
 * ------------------------------------------------------------------ */
/* 层数。VIAL GUI 会按这个数显示层标签；改大要重新烧一次固件 */
#define VIAL_LAYERS             4

/* VIAL 规定 raw HID 包长就是 32 字节，不能改 */
#define VIAL_RAW_EPSIZE         32

/* 键位存在 data-flash 的哪个偏移：
 *   data-flash(EEPROM) 一共 32KB，擦除单位是 256 字节；
 *   蓝牙的绑定信息(SNV)占的是偏移 0x7E00(见 BLE_HID/HAL/include/config.h)，
 *   所以这里放在偏移 0x1000，两边互不干扰。 */
#define VIAL_STORE_ADDR         0x1000
#define VIAL_STORE_VERSION      2       /* v2: matrix 5x4->5x14, old stored keymap layout invalid */

/* 改键后延迟多少毫秒才真正擦写 flash：连着改好几个键只擦写一次 */
#define VIAL_SAVE_DELAY_MS      100

/* 1 = 键盘永远处于"已解锁"状态(等于 vial-qmk 的 VIAL_INSECURE)，
 *    打开 VIAL GUI 不会弹"按住某两个键解锁"的框。
 *    小键盘只有 17 个键，没有多余的键用来解锁，所以默认一直解锁。 */
#define VIAL_UNLOCKED           1

/* 键盘 UID：8 个字节，VIAL GUI 用它区分不同的键盘(随机生成一次后就别改了) */
#define VIAL_KEYBOARD_UID       {0xA6, 0xBC, 0x7C, 0xCE, 0x88, 0xCB, 0x41, 0x88}

/* 协议版本：VIA 0x0009 / VIAL 6，这两个数是协议规定的，别动 */
#define VIA_PROTOCOL_VERSION    0x0009
#define VIAL_PROTOCOL_VERSION   0x00000006u
/* 固件版本：VIAL GUI 会显示，改键位默认值时顺手加一就行 */
#define VIAL_FIRMWARE_VERSION   0x00000001u

/* ------------------------------------------------------------------ *
 *  QMK 键码(VIAL GUI 下发下来的就是 QMK 键码)
 * ------------------------------------------------------------------ */
#define VIAL_KC_NO              0x0000  /* 空键 */
#define VIAL_KC_TRNS            0x0001  /* 透明：本层没定义，回退到下面一层 */

/* 修饰键：0xE0=左Ctrl ... 0xE7=右GUI，正好是 HID 修饰键字节的位序 */
#define VIAL_MOD_MIN            0x00E0
#define VIAL_MOD_MAX            0x00E7

/* 能直接塞进 HID 报表的基础键(KC_A=0x04 ... 0xA4) */
#define VIAL_BASIC_MIN          0x0004
#define VIAL_BASIC_MAX          0x00A4

/* LCTL(KC_C) 这类"修饰键 + 基础键"(QK_MODS)：高字节低 4 位是哪几个修饰键，
 * 0x1000 这一位表示是右手那几个(RCTL/RSFT/RALT/RGUI)。
 * 注意右手键不能直接拿高字节当 HID 修饰键字节用，见 Vial.c 里的翻译。 */
#define VIAL_QK_MODS_MIN        0x0100
#define VIAL_QK_MODS_MAX        0x1FFF
#define VIAL_QK_RMODS           0x1000

/* 层操作键码(QMK 的 quantum 键码)，低 5 位是层号 */
#define VIAL_QK_TO              0x5200  /* TO(l)  ：切到 l 层           */
#define VIAL_QK_MOMENTARY       0x5220  /* MO(l)  ：按住期间在 l 层     */
#define VIAL_QK_DEF_LAYER       0x5240  /* DF(l)  ：把 l 层设为默认层   */
#define VIAL_QK_TOGGLE_LAYER    0x5260  /* TG(l)  ：在 l 层和 0 层间切  */
#define VIAL_QK_LAYER_MASK      0x001F

/* ------------------------------------------------------------------ *
 *  对外接口
 * ------------------------------------------------------------------ */

/*********************************************************************
 * @fn      Vial_Init
 *
 * @brief   初始化：从 data-flash 读键位，没存过(或者版本不对)就用默认键位。
 *          主循环开始前调一次，两种传输模式都可以调。
 *
 * @return  none
 */
extern void Vial_Init(void);

/*********************************************************************
 * @fn      Vial_Task
 *
 * @brief   主循环里反复调用：处理收到的 raw HID 包并回包，
 *          另外把"改过的键位"延迟落盘到 data-flash。
 *          只在 USB 模式的循环里调(蓝牙模式没有 raw HID 通道)。
 *
 * @return  none
 */
extern void Vial_Task(void);

/*********************************************************************
 * @fn      Vial_SetSendFunc
 *
 * @brief   注册"把 32 字节回包发给主机"的函数(USB 模式传 UsbHid_SendRawReport)。
 *          返回 1 = 已经交给硬件，0 = 暂时发不出去(端点忙)，Vial_Task 会重试。
 *
 * @param   fn  - 发送函数，NULL = 没有可用通道
 *
 * @return  none
 */
extern void Vial_SetSendFunc(uint8_t (*fn)(const uint8_t *buf, uint8_t len));

/*********************************************************************
 * @fn      Vial_RawHidReceive
 *
 * @brief   raw HID 端点收到一包数据时调用(在 USB 中断里调，所以这里只拷贝不处理)。
 *
 * @param   data  - 收到的 32 字节
 * @param   len   - 长度(必须是 VIAL_RAW_EPSIZE，否则忽略)
 *
 * @return  none
 */
extern void Vial_RawHidReceive(const uint8_t *data, uint8_t len);

/*********************************************************************
 * @fn      Vial_UpdateLayer
 *
 * @brief   按当前按下的键更新层状态(处理 MO/TO/TG/DF)。
 *          Kbd.c 在生成报表之前调一次，之后用 Vial_GetLayer() 取生效层。
 *
 * @return  none
 */
extern void Vial_UpdateLayer(void);

/*********************************************************************
 * @fn      Vial_GetLayer
 *
 * @brief   当前生效的层号(0 ~ VIAL_LAYERS-1)。
 *
 * @return  层号
 */
extern uint8_t Vial_GetLayer(void);

/*********************************************************************
 * @fn      Vial_GetLayerCount
 *
 * @brief   层数(VIAL 协议要报给主机)。
 *
 * @return  层数
 */
extern uint8_t Vial_GetLayerCount(void);

/*********************************************************************
 * @fn      Vial_ResolveKeycode
 *
 * @brief   取某个键位真正生效的键码：从 layer 这一层开始往下找，
 *          遇到 KC_TRNS(透明)就回退到下一层，第 0 层兜底。
 *
 * @param   layer - 层号
 * @param   row   - 行
 * @param   col   - 列
 *
 * @return  QMK 键码
 */
extern uint16_t Vial_ResolveKeycode(uint8_t layer, uint8_t row, uint8_t col);

/*********************************************************************
 * @fn      Vial_KeycodeToHid
 *
 * @brief   把 QMK 键码翻译成 8 字节键盘报表要的东西。
 *            0xE0~0xE7(修饰键)     -> 只置修饰键位
 *            0x0100~0x1FFF(组合键) -> 置修饰键位 + 一个基础键
 *            0x04~0xA4(基础键)     -> 一个基础键
 *            其它(空键/层键/不支持的量子键如 LT、宏) -> 不上报
 *
 * @param   kc    - QMK 键码
 * @param   mods  - 修饰键字节：本键的修饰键位会被"或"进去
 * @param   key   - 输出基础键码，0 表示这一键只有修饰键
 *
 * @return  1-这个键要上报, 0-忽略
 */
extern uint8_t Vial_KeycodeToHid(uint16_t kc, uint8_t *mods, uint8_t *key);

#ifdef __cplusplus
}
#endif

#endif /* __VIAL_H */