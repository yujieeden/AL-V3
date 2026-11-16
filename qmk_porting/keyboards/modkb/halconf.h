/*
Copyright 2026 ModKB contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

// 说明书 3.5：蓝牙天线 PCB 板载，置于空格键旁。发射功率保持默认（0dBm 档位
// 由硬件净空区实测后决定），需要调整时取消对应行注释。

// #define BLE_TX_POWER LL_TX_POWEER_MINUS_3_DBM
// #define BLE_TX_POWER LL_TX_POWEER_0_DBM
// #define BLE_TX_POWER LL_TX_POWEER_3_DBM

// TODO(引脚冻结后): 若调试串口占用 UART0 并做了 IO 重映射，在此声明
// #define UART0_IO_REMAPPING
