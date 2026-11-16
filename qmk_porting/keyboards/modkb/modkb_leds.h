/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <stdint.h>

void modkb_leds_init(void);

/* 闪烁图案：闪 n 次（间隔固定），用于 mac/win、BT1/BT2 通道提示。
 * 说明书 3.4：单色灯表达力有限，用闪烁次数区分。 */
void modkb_leds_pattern(uint8_t times);

/* 电量状态灯：电量 ADC -> LiPo 3.3-4.2V 曲线 -> 百分比 -> 四档点亮。
 * level 取 0~4（0 = 全灭）。BLE 侧电量上报由移植层 Battery Service 承载。 */
void modkb_leds_battery(uint8_t level);

/* housekeeping 周期调用：驱动闪烁图案与电量档位刷新 */
void modkb_leds_task(void);
