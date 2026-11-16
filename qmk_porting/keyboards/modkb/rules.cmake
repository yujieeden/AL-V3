# ModKB: 64-key main body + swappable magnetic modules (F-zone 15k / PAD 17k) + slider
#
# 功能开关说明见同目录 README.md 的「rules.cmake」条目。

set(NKRO_ENABLE ON CACHE BOOL "KB" FORCE)
set(VIA_ENABLE ON CACHE BOOL "KB" FORCE)
set(BOOTMAGIC_ENABLE ON CACHE BOOL "KB" FORCE)
set(MOUSE_ENABLE OFF CACHE BOOL "KB" FORCE)
set(COMMAND_ENABLE OFF CACHE BOOL "KB" FORCE)
set(GRAVE_ESC_ENABLE OFF CACHE BOOL "KB" FORCE)
set(RGBLIGHT_ENABLE OFF CACHE BOOL "KB" FORCE)
set(RGB_MATRIX_ENABLE OFF CACHE BOOL "KB" FORCE)
set(AUXILIARY_RGB_ENABLE OFF CACHE BOOL "KB" FORCE)
set(ENCODER_ENABLE OFF CACHE BOOL "KB" FORCE)
set(ENCODER_MAP_ENABLE OFF CACHE BOOL "KB" FORCE)
set(SPLIT_KEYBOARD OFF CACHE BOOL "KB" FORCE)
set(OLED_ENABLE OFF CACHE BOOL "KB" FORCE)
set(LTO_ENABLE ON CACHE BOOL "KB" FORCE)

# CH582M 内置 eeprom + 磨损均衡（eeprom_ch58x_custom / wear_leveling）
set(EEPROM_DRIVER "custom" CACHE STRING "KB" FORCE)

# 上电默认蓝牙 + USB 有线优先，双通道 BLE slot（mcuconf.h 里 BLE_SLOT_NUM=2）
set(USB_ENABLE ON CACHE BOOL "KB" FORCE)
# TODO(M3 双 slot 蓝牙里程碑): 恢复 BLE_ENABLE ON。
# 开 BLE 需要 ${KEYBOARD_ROOT}/wireless/libmodkb.a + wireless/include
# （O-H-M2 wireless 预编译库，闭源件不入库），或按说明书 4.2
# 用 legacy_stack/BleKbd.c 移植出等效 wireless 库后再开。
set(BLE_ENABLE OFF CACHE BOOL "KB" FORCE)
set(ESB_ENABLE OFF CACHE BOOL "KB" FORCE)

# TODO(引脚冻结后): 若标准矩阵扫描不满足「模块延伸段」需求（如需要
# 非对称去抖 down=5ms/up=10ms），取消注释启用自定义矩阵：
# set(CUSTOM_MATRIX "${CMAKE_CURRENT_LIST_DIR}/matrix.c" CACHE STRING "KB" FORCE)
