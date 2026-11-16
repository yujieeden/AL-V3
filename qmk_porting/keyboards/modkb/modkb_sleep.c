/*
Copyright 2026 ModKB contributors

说明书 5.5 分级睡眠 + 已知坑 checklist：
- BLE 已连接：不睡眠（连接事件间隙自动低功耗），关轴灯/降扫描
- BLE 广播中：广播看门狗超时先停广播 -> 深度待机，按键 -> 重新广播
- 未连接：深睡（RAM 保持），矩阵行脚配 GPIO 唤醒源，任意键唤醒
- USB 模式：只关灯不深睡（总线供电，少踩坑）
- 坑 1：RTC 以外方式唤醒后 32M 晶振未稳定，需等 WAKE_UP_RTC_MAX_TIME
  （sdk/HAL/SLEEP.c 注释已记录）
- 坑 2：唤醒首键不丢字——唤醒延迟期间到达的按键缓存，恢复后补发
- 坑 3：深睡前关灯、停扫描（轮询电流降不下来等于没睡）
- 坑 4：唤醒后重新初始化外设 -> 键图校验 -> 恢复原状态（BLE 重开广播或回连）
- 坑 5：「深睡唤醒后 BLE 显示已连但发不出键」高频坑——唤醒路径必须
  完整重置协议栈状态
*/

#include QMK_KEYBOARD_H
#include "modkb_sleep.h"
#include "modkb_devmgr.h"

static uint32_t idle_ticks = 0;
static uint16_t wake_cached_keycode = 0;

__attribute__((weak)) void modkb_sleep_init(void) {
    // TODO: 矩阵行脚配置 GPIO 唤醒源（WAKE_UP_GPIO）
}

__attribute__((weak)) void modkb_sleep_feed(void) {
    idle_ticks = 0;
}

__attribute__((weak)) void modkb_sleep_task(void) {
    // TODO: idle_ticks 累加到 MODKB_IDLE_SLEEP_TIMEOUT_MS 时，
    // 按 devmgr 当前状态选择分级睡眠策略（见文件头注释）。
    // 深睡前：关状态灯、停扫描；唤醒路径按坑 1~5 checklist 逐条处理。
}

__attribute__((weak)) bool modkb_sleep_wake_cached_key(uint16_t *keycode_out) {
    if (wake_cached_keycode == 0) {
        return false;
    }
    *keycode_out = wake_cached_keycode;
    wake_cached_keycode = 0;
    return true;
}
