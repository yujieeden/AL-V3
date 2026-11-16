/********************************** (C) COPYRIGHT *******************************
 * File Name          : Kbd.h
 * Description        : Board level keypad logic, shared by both transports:
 *                        - 5x4 matrix scan (1ms TMR0 interrupt) + debounce
 *                        - standard 8-byte HID keyboard report building
 *                        - host LED indicator state (NumLock/CapsLock/ScrollLock)
 *                        - 17 x WS2812/SK6812 LEDs (key response / rainbow)
 *
 *                      This file used to be the top part of Main.c.  It was
 *                      split out so that USB (src/UsbHid.c) and BLE
 *                      (src/BleKbd.c) can both drive the same keypad code:
 *                      whoever is active just registers a "send report"
 *                      function with KB_SetSendFunc() and then calls KB_Task()
 *                      from its own loop.
 *
 *                      All hardware configuration macros (pins, keymap, LED
 *                      colours, debug switches) live in Kbd.c / Kbd.h, see the
 *                      README for the "what do I change" table.
 *******************************************************************************/

#ifndef __KBD_H
#define __KBD_H

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 *  ���̾������� (keyboard matrix)
 * ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  Matrix driver switch (add 2026-11-06, CENTER board verification):
 *    1 = CENTER board (V2 hardware): 5x14 matrix driven by two TCA9535
 *        I2C expanders on the CENTER keybed PCB.
 *          U1 @ 0x20  P00-P04 = ROW1-ROW5 outputs (active LOW)
 *          U2 @ 0x21  P00-P07 = COL1-COL8, P10 = COL9, P13-P17 = COL10-14
 *                     (inputs, external pull-ups on the CENTER board)
 *        Soft I2C on PB0 = SCL / PB1 = SDA (FFC pin #2/#3, fixed by hw).
 *        Wiring: main board J1/H1 -> PCB#3 adapter -> 26P FFC -> J415.
 *    0 = V1 numpad hardware: direct GPIO scan, PB0-PB5 rows / PA4-PA7
 *        cols.  This is the original code path, byte-for-byte unchanged.
 *
 *  When flipping back to 0 you must also restore vial.json + VialDef.h
 *  from vial_v1.json (git checkout the pair) -- the VIAL definition
 *  binary embeds the matrix size.
 * ------------------------------------------------------------------ */
#ifndef KB_MATRIX_TCA9535
#define KB_MATRIX_TCA9535   1
#endif

#define MATRIX_ROWS   5         /* ���� */
#if KB_MATRIX_TCA9535
#define MATRIX_COLS   14        /* CENTER board: 5x14, 64 keys + 6 holes */
#else
#define MATRIX_COLS   4
#endif         /* ���� */
#define KB_ROW4_SEPARATE_KEYS 1  /* 1: 0/. are separate; 0: 0 is a 2U key */

#define KC_NO     0x0000
#define KC_NUM    0x0053
#define KC_PSLS   0x0054
#define KC_PAST   0x0055
#define KC_PMNS   0x0056
#define KC_PPLS   0x0057
#define KC_PENT   0x0058
#define KC_P1     0x0059
#define KC_P2     0x005A
#define KC_P3     0x005B
#define KC_P4     0x005C
#define KC_P5     0x005D
#define KC_P6     0x005E
#define KC_P7     0x005F
#define KC_P8     0x0060
#define KC_P9     0x0061
#define KC_P0     0x0062
#define KC_PDOT   0x0063
#define KC_O      0x0012
#define KC_K      0x000E

#if KB_MATRIX_TCA9535
/* QMK keycodes for the CENTER board default keymap (values match QMK). */
#define KC_LCTL   0x00E0
#define KC_LSFT   0x00E1
#define KC_LALT   0x00E2
#define KC_LGUI   0x00E3
#define KC_RCTL   0x00E4
#define KC_RSFT   0x00E5
#define KC_RALT   0x00E6
#define KC_A      0x0004
#define KC_B      0x0005
#define KC_C      0x0006
#define KC_D      0x0007
#define KC_E      0x0008
#define KC_F      0x0009
#define KC_G      0x000A
#define KC_H      0x000B
#define KC_I      0x000C
#define KC_J      0x000D
/* KC_K 0x000E and KC_O 0x0012 already defined above (V1) */
#define KC_L      0x000F
#define KC_M      0x0010
#define KC_N      0x0011
#define KC_P      0x0013
#define KC_Q      0x0014
#define KC_R      0x0015
#define KC_S      0x0016
#define KC_T      0x0017
#define KC_U      0x0018
#define KC_V      0x0019
#define KC_W      0x001A
#define KC_X      0x001B
#define KC_Y      0x001C
#define KC_Z      0x001D
#define KC_1      0x001E
#define KC_2      0x001F
#define KC_3      0x0020
#define KC_4      0x0021
#define KC_5      0x0022
#define KC_6      0x0023
#define KC_7      0x0024
#define KC_8      0x0025
#define KC_9      0x0026
#define KC_0      0x0027
#define KC_ENT    0x0028
#define KC_BSPC   0x002A
#define KC_TAB    0x002B
#define KC_SPC    0x002C
#define KC_MINS   0x002D
#define KC_EQL    0x002E
#define KC_LBRC   0x002F
#define KC_RBRC   0x0030
#define KC_BSLS   0x0031
#define KC_SCLN   0x0033
#define KC_QUOT   0x0034
#define KC_GRV    0x0035
#define KC_COMM   0x0036
#define KC_DOT    0x0037
#define KC_SLSH   0x0038
#define KC_CAPS   0x0039
#define KC_DEL    0x004C
#define KC_RGHT   0x004F
#define KC_LEFT   0x0050
#define KC_DOWN   0x0051
#define KC_UP     0x0052
#endif /* KB_MATRIX_TCA9535 */

#define DEBOUNCE_TICKS   5      /* ���������� 5ms ��ƽ����Ÿ�״̬ */
#define KB_REPORT_SIZE   8      /* ��׼���̱������ȣ����μ� + ���� + 6 ���� */
#define KB_BLE_NKRO_REPORT_SIZE 14 /* 8 modifier bits + 98 key bits + 6 padding bits */
#define KB_REFRESH_TICKS 100    /* ״̬�ޱ仯ʱÿ 100ms �ط�һ�α���(�ݴ�)��
                                   ע�ⵥλ�Ǻ��룬�� 1ms �� KB_Tick ��ʱ�����ܰ�
                                   KB_Task �ĵ��ô�����ʱ������ģʽ�����ǽ�ѭ�����õġ� */

/* ------------------------------------------------------------------ *
 *  ����״̬ (shared with the transport in charge)
 * ------------------------------------------------------------------ */
/* ��ǰ���뱨���Լ���һ���Ѿ�����ȥ�ı��� */
extern uint8_t KB_Report[KB_REPORT_SIZE];
extern uint8_t KB_ReportSent[KB_REPORT_SIZE];

/* �����·���ָʾ��״̬��bit0=NumLock bit1=CapsLock bit2=ScrollLock */
extern uint8_t KB_LED_State;

/* ��������״̬��1=�����ӡ�BLE �����ڼ� WS2812 ˢ�»�� BLE/RTC �жϵ���
   ���ߣ����Ե�Ч�������ڼ���ͣ(�� Kbd.c WS_Update)���� BleKbd.c ά���� */
extern volatile uint8_t KB_Ble_Connected;

/* 1ms ϵͳ���ģ��� TMR0 ɨ���ж��ۼӣ��ƹ�Ч��������ʱ���׼ */
extern volatile uint32_t KB_Tick;

/* ԭʼɨ����(δ����)�밴���¼������ڵ����� */
#if KB_MATRIX_TCA9535
extern volatile uint16_t KB_RawMap[MATRIX_ROWS];   /* 14 cols -> 16-bit row bits */
#else
extern volatile uint8_t KB_RawMap[MATRIX_ROWS];
#endif
extern volatile uint8_t KB_RawDumpFlag;
extern volatile uint8_t KB_RawPinA;
extern volatile uint8_t KB_RawPinB;
extern volatile uint8_t KB_EventFlag;
extern volatile uint8_t KB_EventRow;
extern volatile uint8_t KB_EventCol;
extern volatile uint8_t KB_EventDown;

/*********************************************************************
 * @fn      KeyboardInit
 *
 * @brief   ���̳�ʼ�������� GPIO + ָʾ�� GPIO + WS2812 �ƴ� + 1ms ɨ�趨ʱ����
 *          ���ִ���ģʽ��Ҫ�ȵ�������
 *
 * @return  none
 */
extern void KeyboardInit(void);

/*********************************************************************
 * @fn      KB_SetSendFunc
 *
 * @brief   ע��"�� 8 �ֽڱ�������ȥ"�ĺ�����USB ģʽ�� USB �ķ��ͺ�����
 *          ����ģʽ�� BLE �ķ��ͺ�����
 *          ���ͺ������� 1 ��ʾ�ѽ���Ӳ��(USB: д�� EP1��BLE: ֪ͨ�ѷ���)��
 *          ���� 0 ��ʾ��ʱ������ȥ(δö�� / δ����)��KB_Task ���Ժ����ԡ�
 *
 * @param   fn  - ���ͺ���ָ�룬NULL ��ʾû�п��õ�ͨ��
 *
 * @return  none
 */
extern void KB_SetSendFunc(uint8_t (*fn)(uint8_t *report));

/*********************************************************************
 * @fn      KB_Send
 *
 * @brief   ��ע��õķ��ͺ�����һ�α���(���Լ�ȳ���ֱ�ӵ���)��
 *
 * @param   report  - 8 �ֽڱ���
 *
 * @return  1-�ѷ���, 0-����ͨ��������
 */
extern uint8_t KB_Send(uint8_t *report);

/*********************************************************************
 * @fn      KB_Task
 *
 * @brief   �����������ɵ�ǰ��Ч�Ĵ����ѭ���������ã�
 *          ˢ��ָʾ��/�ƴ� -> ��ӡ������Ϣ -> �ϵ��Լ� -> ���ɱ�������
 *          ״̬�仯(��ʱ)ʱ�ϱ���
 *
 * @return  none
 */
extern void KB_Task(void);

/*********************************************************************
 * @fn      KB_SetLEDState
 *
 * @brief   ��¼�����·���ָʾ��״̬(USB �� EP1 OUT�������� HID �������)��
 *
 * @param   state   - bit0=NumLock bit1=CapsLock bit2=ScrollLock
 *
 * @return  none
 */
extern void KB_SetLEDState(uint8_t state);

/*********************************************************************
 * @fn      KB_ResetReport
 *
 * @brief   �������״̬�ͱ������档�����Ͽ�ʱ���ã�����������̳�һ��
 *          "ĳ����������"�ľɱ�����
 *
 * @return  none
 */
extern void KB_ResetReport(void);

/*********************************************************************
 * @fn      Matrix_Scan
 *
 * @brief   ����ɨ��(1ms ����һ�Σ��� TMR0 �ж���)��
 *
 * @return  none
 */
extern void Matrix_Scan(void);

/* Debounced matrix state, used by the VIA switch-matrix query. */
extern uint8_t KB_KeyPressed(uint8_t row, uint8_t col);

/*********************************************************************
 * @fn      KB_ReportBuild
 *
 * @brief   ����ǰ����״̬���ɱ���(ȥ�أ���� 6 ��)��
 *
 * @param   report  - �������(8 �ֽ�)
 *
 * @return  none
 */
extern void KB_ReportBuild(uint8_t *report);

/* Build the BLE-only NKRO bitmap report; the existing USB report stays 6KRO. */
extern void KB_NKRO_ReportBuild(uint8_t *report);

#ifdef __cplusplus
}
#endif

#endif /* __KBD_H */