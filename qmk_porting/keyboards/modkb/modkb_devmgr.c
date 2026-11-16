/*
Copyright 2026 ModKB contributors

说明书 4.3 / 5.6：
- 上电读开关：OFF -> 充电待机（主控不初始化外设，充电灯硬件直驱）；
  ON -> 蓝牙。
- USB > 蓝牙：VBUS 检测/接入事件触发 ST_USB_PENDING -> 启动 USB 栈等枚举
  -> 成功进 ST_USB；超时（实测确定，勿假定 300ms）退回 BLE。
- 异常路径必须有定义行为，不允许卡在中间态：
  * USB 连接中拔线 -> 回 BLE，重开广播（拔线事件先去抖）
  * 休眠中插 USB -> 唤醒 -> 切 ST_USB
  * 绑定数据 CRC 失败 -> 丢弃该 slot，重开通用广播重新配对
  * ADC 模块电压超时/越界 -> 按未接入处理
  * I2C/外设初始化失败 -> 重试 3 次 -> 安全模式
*/

#include QMK_KEYBOARD_H
#include "modkb_devmgr.h"
#include "modkb_module.h"

// 说明书 5.6：枚举超时必须实测标定，先用 500ms 占位
#define MODKB_USB_ENUM_TIMEOUT_MS 500

static modkb_state_t state = MODKB_ST_BLE_ADV;

__attribute__((weak)) void modkb_devmgr_init(void) {
    // TODO: 读开关三态（说明书 3.3）。OFF 时进入充电待机，不初始化外设。
    // 开关状态脚 GPIO 输入上拉，由硬件提供电平定义。
}

__attribute__((weak)) void modkb_devmgr_task(void) {
    // TODO(按 4.3 状态机实现)。要点：
    // 1. BLE 状态下检测 VBUS -> 停广播/断链 -> ST_USB_PENDING
    // 2. ST_USB_PENDING 起计时，枚举成功事件 -> ST_USB；超时 -> 退回 BLE
    // 3. ST_USB 拔出 -> 关 USB -> 恢复 BLE 广播/回连
    // 4. 空闲计时到 MODKB_IDLE_SLEEP_TIMEOUT_MS -> 请求休眠（见 modkb_sleep）
    // 5. 每个转移前释放旧链路按键状态（防止切换期间卡键/丢键）
    (void)MODKB_USB_ENUM_TIMEOUT_MS;
}

modkb_state_t modkb_devmgr_get_state(void) {
    return state;
}
