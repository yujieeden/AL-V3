/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <stdint.h>

/* 模块类型：ADC 位置识别结果（说明书 5.2） */
typedef enum {
    MOD_NONE = 0, // 未接入（ADC 悬空/超时/越界）
    MOD_F,        // F 区功能模块（15 键）
    MOD_PAD,      // 数字区 PAD（17 键）
} modkb_module_type_t;

/* 左右两个槽位 */
typedef struct {
    modkb_module_type_t left;
    modkb_module_type_t right;
} modkb_module_layout_t;

void     modkb_module_init(void);
/* 上电/热插拔后调用：读两路位置识别 ADC，判定左右模块类型 */
void     modkb_module_detect(void);
const modkb_module_layout_t *modkb_module_get_layout(void);

/* 滑阻：housekeeping 里周期调用，内部自带 1-4V 在位判定与去抖，
 * 拨动一格发出 KC_VOLU / KC_VOLD（QMK consumer 通道，USB/BLE 两路通吃） */
void     modkb_slider_task(void);

/* 按下/抬起去抖（时间戳法，说明书 5.1）参数 */
#define MODKB_MODULE_SCAN_RETRY_MS 50
