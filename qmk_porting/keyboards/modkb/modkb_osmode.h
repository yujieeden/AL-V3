/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODKB_OS_WIN = 0,
    MODKB_OS_MAC,
} modkb_os_mode_t;

void            modkb_osmode_init(void);
modkb_os_mode_t modkb_osmode_get(void);
/* 切换并持久化（data-flash，CRC 保护），返回新模式 */
modkb_os_mode_t modkb_osmode_toggle(void);

/* 在 process_record_kb 里、报表构建前调用：
 * mac 模式下把 GUI <-> Alt 修饰键互换（说明书 5.3）。
 * 返回 true 表示事件继续走默认处理。 */
bool            modkb_osmode_process_record(uint16_t keycode, keyrecord_t *record);
