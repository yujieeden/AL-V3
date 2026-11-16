/*
Copyright 2026 ModKB contributors

说明书 5.2：Vial 改键按「布局槽位」存储，模块互换后自动切到另一套。
TODO(引脚冻结后): LAYOUT_all 按真实矩阵改为三组——主体 64 键 +
F 区 15 键 + PAD 17 键（union 布局，模块延伸段占独立行）。
*/

#include QMK_KEYBOARD_H
#include "modkb_osmode.h"
#include "modkb_sleep.h"

#define BT1 BLE_SLOT0
#define BT2 BLE_SLOT1

/* 键盘级按键拦截：框架的 process_record_kb（extra_keycode.c）会先调到这里。
   自定义键码拦截 + mac/win 修饰键重映射 + 空闲计时喂狗都在这一层做。 */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // 唤醒首键缓存补发由 modkb_sleep 负责；这里同时喂空闲计时
    modkb_sleep_feed();

    switch (keycode) {
        case KC_OS_TOGGLE:
            if (record->event.pressed) {
                modkb_osmode_toggle();
            }
            return false;
        case KC_SL_MUTE:
            if (record->event.pressed) {
                tap_code16(KC_MUTE);
            }
            return false;
        default:
            break;
    }
    return modkb_osmode_process_record(keycode, record);
}

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_all(
        KC_ESC, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_BSPC,
        KC_TAB, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_ENT,
        KC_CAPS, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH,
        KC_LSFT, KC_LCTL, KC_LGUI, KC_LALT, KC_SPC,
        // 以下为模块占位：引脚冻结后按真实矩阵拆成 15 键 / 17 键
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX
    ),
    [1] = LAYOUT_all(
        KC_GRV, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_DEL,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_OS_TOGGLE, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        BT1, BT2, USB_MODE, KC_MUTE, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),
    [2] = LAYOUT_all(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),
    [3] = LAYOUT_all(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),
};
