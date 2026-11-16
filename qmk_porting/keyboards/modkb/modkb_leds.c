/*
Copyright 2026 ModKB contributors

说明书 3.4 / 5.8：
- 充电指示灯：电源 IC 直驱，固件不碰。
- 状态指示灯（四个，按电池电压分档 25/50/75/100%）：固件职责是
  电量 ADC 采样 -> 分档 -> 驱动；mac/win 与 BT 通道切换用闪烁图案提示。
- TODO(引脚冻结后): 四个灯的 GPIO（当前 qmk_config.h 只给了 1 个占位脚，
  四灯方案冻结后改为 4 脚数组）。
*/

#include QMK_KEYBOARD_H
#include "modkb_leds.h"
#include "modkb_module.h"

__attribute__((weak)) void modkb_leds_init(void) {
    // TODO: 状态灯 GPIO 初始化为输出，默认灭
}

__attribute__((weak)) void modkb_leds_pattern(uint8_t times) {
    // TODO: 非阻塞实现（在 leds_task 里推进），不要在回调里死延时——
    // 会拖慢矩阵扫描与 BLE 连接事件
    (void)times;
}

__attribute__((weak)) void modkb_leds_battery(uint8_t level) {
    // TODO: level 0~4 -> 4 个灯逐档点亮
    (void)level;
}

__attribute__((weak)) void modkb_leds_task(void) {
    // TODO: 周期采样电量 ADC（移植层 battery_measure 已有分压比标定接口，
    // 说明书 4.2：按新分压比重新标定）-> 分档 -> 刷新灯
}
