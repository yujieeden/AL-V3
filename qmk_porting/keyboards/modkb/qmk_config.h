/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

/* ============================= USB 描述符 ============================= */
// TODO(产测前): 分配正式的 VID/PID，当前为占位值
#define VENDOR_ID    0xCAFE
#define PRODUCT_ID   0x0B92
#define DEVICE_VER   0x0001
#define MANUFACTURER ModKB
#define PRODUCT      ModKB

/* ============================= 矩阵 ============================= */
// TODO(引脚冻结后): 按实际矩阵填。当前 8x14 为占位：
//   行 0~3 / 列 0~10 大致对应主体 64 键；行 5/6 预留给左右模块延伸段；
//   行 7 预留给滑阻模块（如有按压开关则占一格）。
// 模块矩阵是主矩阵的延伸段（行列线经磁吸口直连），拔掉模块 = 该段开路
// = 扫不到按下，无需在线检测通信。
#define MATRIX_ROWS 8
#define MATRIX_COLS 14

// TODO(引脚冻结后): 填实际行列 GPIO
#define MATRIX_ROW_PINS \
    {                   \
        B0, B1, B2, B3, B4, B5, B6, B7 \
    }
#define MATRIX_COL_PINS \
    {                   \
        A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13 \
    }
#define DIODE_DIRECTION COL2ROW

/* ============================= 去抖 ============================= */
// 说明书 5.1：按下立即生效(5ms) / 抬起延迟判定(10ms) 吸收磁吸触点瞬断。
// QMK 内置去抖为对称去抖，DEBOUNCE 取抬起值（保守值）；
// 若实测磁吸触点抖动明显，改用 CUSTOM_MATRIX 自研非对称去抖（见 rules.cmake）。
#define DEBOUNCE 10
#define MODKB_DEBOUNCE_DOWN_MS 5  // 自定义去抖启用时生效
#define MODKB_DEBOUNCE_UP_MS   10

/* ============================= 层与改键 ============================= */
#define DYNAMIC_KEYMAP_LAYER_COUNT 4

/* ============================= 蓝牙 ============================= */
// 说明书 5.4：双 slot（A 方案，单连接 + 双 slot 切换）
// BLE_SLOT_NUM 在 mcuconf.h 中定义为 2，复用移植层 BLE_SLOT0/BLE_SLOT1 键码

/* ============================= 休眠 ============================= */
// 说明书 5.5：5 分钟无操作休眠，按键唤醒
#define MODKB_IDLE_SLEEP_TIMEOUT_MS (5UL * 60UL * 1000UL)
// 产测时可大幅缩短验证（例如 10s），走 qmk_config 覆盖，不改代码

/* ============================= 模块 ADC 阈值 ============================= */
// 说明书 5.2：分压判定表。阻值由硬件冻结时填入换算后的 ADC 计数。
// 3.3V 参考、10bit ADC 下 1V ≈ 1000*1/3.3 ≈ 303 计数，按实际分压比换算。
// TODO(硬件冻结): 全部替换为实测换算值
#define MODKB_ADC_TH_F_LOW        0    // F 区分压下限（ADC 计数）
#define MODKB_ADC_TH_F_HIGH       0    // F 区分压上限
#define MODKB_ADC_TH_PAD_LOW      0    // PAD 分压下限
#define MODKB_ADC_TH_PAD_HIGH     0    // PAD 分压上限
#define MODKB_ADC_TH_SLIDER_LOW   303  // 滑阻有效下限 ≈ 1V
#define MODKB_ADC_TH_SLIDER_HIGH  1212 // 滑阻有效上限 ≈ 4V
#define MODKB_SLIDER_SAMPLE_HZ    100  // 说明书 5.7：≥100Hz 采样

/* ============================= 状态灯 ============================= */
// 说明书 3.4：2 个单色灯（充电灯由电源 IC 直驱，固件只管状态灯）
// TODO(引脚冻结后): 填实际 GPIO；低电平点亮时把 _ACTIVE_LOW 置 1
#define MODKB_STATUS_LED_PIN     B8
#define MODKB_STATUS_LED_ACTIVE_LOW 0
