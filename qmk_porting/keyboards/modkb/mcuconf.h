/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#define DEBUG_BAUDRATE       460800
#define DCDC_ENABLE          1
#define FREQ_SYS             40000000
#define LSE_ENABLE           1

// 说明书 5.4：双 slot（slot1/slot2 各存一套 bonding，data-flash 分区 + CRC
// 由移植层 wear-leveling eeprom 承载）
#define BLE_SLOT_NUM         2

#define HSE_LOAD_CAPACITANCE 20 // in pF unit
#define LSE_LOAD_CAPACITANCE 19 // in pF unit
