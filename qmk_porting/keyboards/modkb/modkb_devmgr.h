/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <stdint.h>

/* 说明书 4.3：全局状态机。任何时刻系统处于且仅处于一个主状态。 */
typedef enum {
    MODKB_ST_BLE_CONN,   // 蓝牙已连接
    MODKB_ST_BLE_ADV,    // 蓝牙广播中
    MODKB_ST_USB_PENDING,// 检测到 USB 接入，停止 BLE，等待主机枚举
    MODKB_ST_USB,        // USB 枚举成功，有线模式（优先于蓝牙）
    MODKB_ST_SLEEP,      // 深睡（RAM 保持，矩阵 GPIO 唤醒）
    MODKB_ST_SAFE_MODE,  // I2C/外设初始化重试 3 次失败：通用广播 + 默认键图
} modkb_state_t;

void           modkb_devmgr_init(void);
/* housekeeping 周期调用：推进状态转移（上电开关判定 / USB 接入与枚举
 * 超时 / 拔线回退 / 休眠入口）。QMK 移植层的 USB/BLE 模式机是主骨架，
 * DevMgr 在其上做「异常路径收口」，不重复造轮子。 */
void           modkb_devmgr_task(void);
modkb_state_t  modkb_devmgr_get_state(void);
