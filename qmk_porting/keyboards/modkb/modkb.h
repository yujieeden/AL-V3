/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

/* 注意：QMK_KEYBOARD_H 会展开为本文件自身（modkb.h），不能在这里 include 它，
   直接拉 QMK 核心头，keyrecord_t / QK_USER_0 都从这里来 */
#include "quantum.h"
#include "extra_keycode.h" // USB_MODE / BLE_SLOT0-15（移植层自定义键码）

/* 自定义键码：safe range 起，vial.json 需同步登记 */
enum modkb_custom_keycodes {
    KC_OS_TOGGLE = QK_USER_0, // mac/win 切换
    KC_SL_MUTE,               // 滑阻按压（静音/自定义，进 Vial）
};

/* LAYOUT_all：48 个布局位（本移植不跑 qmk CLI，宏手写，与 info.json 对齐）。
   矩阵 8x14 占位：行 0-3 主体键，行 5/6 模块延伸段，引脚冻结后重排。 */
// clang-format off
#define LAYOUT_all(                                                          \
    k00, k01, k02, k03, k04, k05, k06, k07, k08, k09, k10, k11,              \
    k12, k13, k14, k15, k16, k17, k18, k19, k20, k21, k22, k23,              \
    k24, k25, k26, k27, k28, k29, k30, k31, k32, k33, k34,                   \
    k35, k36, k37, k38, k39,                                                 \
    k40, k41, k42, k43, k44, k45, k46, k47                                   \
) {                                                                          \
    { k00,  k01,  k02,  k03,  k04,  k05,  k06,  k07,  k08,  k09, k10, k11, KC_NO, KC_NO }, \
    { k12,  k13,  k14,  k15,  k16,  k17,  k18,  k19,  k20,  k21, k22, k23, KC_NO, KC_NO }, \
    { k24,  k25,  k26,  k27,  k28,  k29,  k30,  k31,  k32,  k33, k34, KC_NO, KC_NO, KC_NO }, \
    { k35,  k36,  k37,  k38,  k39,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO }, \
    { KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO }, \
    { k40,  k41,  k42,  k43,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO }, \
    { k44,  k45,  k46,  k47,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO }, \
    { KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO } \
}
// clang-format on
