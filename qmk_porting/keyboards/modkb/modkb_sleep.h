/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <stdbool.h>

/* 分级睡眠（说明书 5.5），QMK 移植层 suspend.c 已有 CH58x 低功耗骨架 */
void modkb_sleep_init(void);
/* housekeeping 周期调用：任何按键/滑阻/USB 事件清零空闲计时 */
void modkb_sleep_feed(void);
void modkb_sleep_task(void);
/* 唤醒后首键缓存：恢复后补发（量产硬指标） */
bool modkb_sleep_wake_cached_key(uint16_t *keycode_out);
