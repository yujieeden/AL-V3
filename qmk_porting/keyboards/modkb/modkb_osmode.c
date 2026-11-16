/*
Copyright 2026 ModKB contributors

说明书 5.3：
- 全局 flag os_mode 存 data-flash（CRC 保护；与 BT slot 等同属「配置区」）。
- 触发：自定义键码 KC_OS_TOGGLE（在 process_record_kb 拦截，不进报表）。
- mac 模式下修饰键互换：GUI(0x08) <-> Alt(0x04)，Win 键码 0xE3 <-> 0xE5。
  实现走键码级重映射（KC_LGUI<->KC_LALT / KC_RGUI<->KC_RALT），
  作用点在报表构建前，USB 6KRO 与 BLE NKRO 两条路自然都生效。
- 反馈：状态灯 win=闪 1 次，mac=闪 2 次。
*/

#include QMK_KEYBOARD_H
#include "modkb_osmode.h"
#include "modkb_leds.h"

static modkb_os_mode_t os_mode = MODKB_OS_WIN;

__attribute__((weak)) void modkb_osmode_init(void) {
    // TODO: 从 eeprom 配置区读 os_mode（校验 CRC/版本号，坏值回默认 WIN）
}

__attribute__((weak)) modkb_os_mode_t modkb_osmode_get(void) {
    return os_mode;
}

__attribute__((weak)) modkb_os_mode_t modkb_osmode_toggle(void) {
    os_mode = (os_mode == MODKB_OS_WIN) ? MODKB_OS_MAC : MODKB_OS_WIN;
    // TODO: 写 eeprom 配置区（沿用移植层延迟落盘机制，连接活跃时不写）
    modkb_leds_pattern(os_mode == MODKB_OS_WIN ? 1 : 2); // win=1 次, mac=2 次
    return os_mode;
}

__attribute__((weak)) bool modkb_osmode_process_record(uint16_t keycode, keyrecord_t *record) {
    if (os_mode != MODKB_OS_MAC) {
        return true;
    }
    uint8_t remapped_mods = 0;
    switch (keycode) {
        case KC_LGUI: remapped_mods = MOD_BIT(KC_LALT); break;
        case KC_LALT: remapped_mods = MOD_BIT(KC_LGUI); break;
        case KC_RGUI: remapped_mods = MOD_BIT(KC_RALT); break;
        case KC_RALT: remapped_mods = MOD_BIT(KC_RGUI); break;
        default: return true;
    }
    /* 修饰键是按住生效的语义：直接以目标修饰键注册/注销，
       不改写 record（keyrecord_t.keycode 仅在 COMBO/REPEAT_KEY 下存在） */
    if (record->event.pressed) {
        register_mods(remapped_mods);
    } else {
        unregister_mods(remapped_mods);
    }
    return false;
}
