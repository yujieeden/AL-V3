/*
Copyright 2026 ModKB contributors

键盘主文件：把各功能模块挂到 QMK 的 kb 级钩子上。
模块划分见同目录 README.md。
*/

#include QMK_KEYBOARD_H
#include "modkb.h"
#include "modkb_devmgr.h"
#include "modkb_module.h"
#include "modkb_osmode.h"
#include "modkb_leds.h"
#include "modkb_sleep.h"

void keyboard_pre_init_kb(void) {
    keyboard_pre_init_user();
    modkb_devmgr_init(); // OFF -> 充电待机，后面都不初始化
    modkb_module_init();
    modkb_osmode_init();
    modkb_leds_init();
    modkb_sleep_init();
}

void keyboard_post_init_kb(void) {
    keyboard_post_init_user();
    modkb_module_detect(); // 上电读 ADC，加载左右模块键图布局
}

/* 键盘级自定义键码与修饰键重映射放在 keymap.c 的 process_record_user：
   本移植框架在 extra_keycode.c 强定义 process_record_kb
   （内部调 process_record_user 并处理 USB_MODE/BLE slot 键码），
   键盘层不可再定义 process_record_kb，否则链接期重复定义。 */

void housekeeping_task_kb(void) {
    housekeeping_task_user();
    modkb_devmgr_task();
    modkb_slider_task(); // 滑阻 100Hz 采样 -> 音量步进（内部再喂 sleep）
    modkb_leds_task();
    modkb_sleep_task();
}

/* 分级睡眠与 USB/BLE 切换挂到移植层 suspend 钩子（platforms/ch58x/suspend.c） */
void suspend_power_down_kb(void) {
    // TODO: 关状态灯、停扫描、降轮询
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    // TODO: 外设重初始化 -> 键图校验 -> 恢复 BLE 广播/回连（完整重置协议栈状态）
    suspend_wakeup_init_user();
}
