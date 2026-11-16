# ModKB 固件仓库结构说明

可互换模块的无线机械键盘（64 键主体 + 磁吸 F 区 15 键 / PAD 17 键 + 滑阻音量模块），基于 qmk_port_ch582M（CH582M 上 QMK 框架移植）。

对应设计文档：《键盘固件设计说明书 v3.0》。本文档回答两个问题：**每个文件负责什么**、**出问题时查哪里**。

---

## 一、目录总览

```
qmk_porting/keyboards/modkb/
├── info.json                    键盘元数据 + 布局 + 自定义键码登记
├── rules.cmake                  编译期功能开关
├── qmk_config.h                 引脚 / 矩阵 / 去抖 / ADC 阈值 / 休眠超时等所有参数
├── mcuconf.h                    芯片级配置（主频 / BLE slot 数 / 晶振负载电容）
├── halconf.h                    HAL 级配置（串口重映射 / BLE 发射功率）
├── modkb.c / modkb.h            键盘主文件：把功能模块挂到 QMK 钩子
├── modkb_devmgr.c/.h            全局状态机（说明书 4.3）
├── modkb_module.c/.h            模块识别 + 布局切换 + 滑阻（说明书 5.1/5.2/5.7）
├── modkb_osmode.c/.h            mac/win 切换（说明书 5.3）
├── modkb_leds.c/.h              状态灯图案 + 电量分档（说明书 3.4/5.8）
├── modkb_sleep.c/.h             分级休眠 + 唤醒首键缓存（说明书 5.5）
├── README.md                    本文档
└── keymaps/bluetooth_le/
    ├── keymap.c                 默认键图（4 层）
    └── rules.cmake              keymap 级开关
```

## 二、每个文件负责什么

### info.json
- VID/PID、自定义键码（OS_TOGGLE/BT1/BT2）的 GUI 登记项。
- 布局骨架：当前是**占位**（模块 15 键 / 17 键各画成一个块），引脚冻结后按真实矩阵拆开。
- Vial GUI 与 vial.json 依赖此文件自动识别布局。

### rules.cmake
编译期开关。已开：NKRO、VIA、BOOTMAGIC、USB+BLE、自定义 eeprom。已关：RGB（说明书 3.4 明确不用 RGB 做系统指示）、ESB、鼠标、编码器。

### qmk_config.h
**所有可调参数的唯一入口**：
- 矩阵尺寸与行列引脚（占位，等引脚冻结）
- 去抖：`DEBOUNCE 10`（抬起判定值），`MODKB_DEBOUNCE_DOWN_MS 5 / UP_MS 10` 备用于自定义非对称去抖
- `MODKB_IDLE_SLEEP_TIMEOUT_MS` 休眠超时（产测可缩短）
- ADC 阈值表：F/PAD 分压上下限、滑阻 1V–4V 有效区间（3.3V/10bit 下 1V≈303 计数）
- 状态灯 GPIO

### mcuconf.h
- `BLE_SLOT_NUM 2`：说明书 5.4 双 slot，直接砍掉移植层默认多 slot。
- `FREQ_SYS / DCDC / LSE / 晶振负载电容`。

### halconf.h
串口 IO 重映射、BLE 发射功率。天线净空区实测后调 `BLE_TX_POWER`。

### modkb.c / modkb.h
唯一向 QMK 暴露的入口：
- `keyboard_pre_init_kb`：OFF 开关判定（充电待机）+ 各模块 init
- `keyboard_post_init_kb`：上电 ADC 模块识别
- `process_record_kb`：`KC_OS_TOGGLE` / `KC_SL_MUTE` 拦截 + mac/win 修饰键重映射 + 喂空闲计时
- `housekeeping_task_kb`：状态机推进 / 滑阻采样 / 状态灯 / 休眠
- `suspend_power_down_kb` / `suspend_wakeup_init_kb`：分级睡眠两端

### modkb_devmgr.c/.h
说明书 4.3 状态机：`OFF充电待机 / BLE_CONN / BLE_ADV / USB_PENDING / USB / SLEEP / SAFE_MODE`。职责是**异常路径收口**——每条转移都有定义行为，不允许卡中间态。USB>蓝牙的主骨架复用移植层现有模式机，DevMgr 只做超时判定、拔线去抖、异常归宿。

### modkb_module.c/.h
- 上电/变化时读两路位置识别 ADC → `MOD_NONE / MOD_F / MOD_PAD`，加载对应布局。
- 模块矩阵是主矩阵延伸段（拔掉=开路=扫不到），**不存在在线检测通信，也不做离线清键逻辑**。
- 滑阻 100Hz 采样，1–4V 在位判定 + 去抖，上/下拨一格发 `KC_VOLU/KC_VOLD`（QMK consumer 通道 USB/BLE 通吃，无需自建报表）。

### modkb_osmode.c/.h
`os_mode` 持久化 + 切换 + 键码级 GUI↔Alt 互换。作用点在报表构建前，USB 6KRO 与 BLE NKRO 两路自然都生效。状态灯反馈：win 闪 1 次、mac 闪 2 次。

### modkb_leds.c/.h
闪烁图案引擎（非阻塞，在 task 里推进）+ 电量四档（25/50/75/100%）。充电灯由电源 IC 直驱，固件不碰。

### modkb_sleep.c/.h
空闲计时 + 分级睡眠策略 + 唤醒首键缓存补发。QMK 移植层 `platforms/ch58x/suspend.c` 已有 CH58x 低功耗骨架。

### keymaps/bluetooth_le/keymap.c
4 层默认键图；`BT1/BT2` 用移植层现成的 `BLE_SLOT0/1`，`USB_MODE` 也是现成键码。

---

## 三、遇到问题 → 查什么

### 编译 / 构建类

| 现象 | 检查顺序 |
|---|---|
| CMake 找不到 modkb | `keyboard_target.cmake` 里 `set(keyboard "modkb")` 改了吗；`qmk_porting/keyboards/modkb/rules.cmake` 存在吗 |
| qmk_firmware 子模块为空导致缺头文件 | `git submodule update --init --recursive`（当前仓库 qmk_firmware 是空的） |
| 自定义键码未定义 | `modkb.h` 的 `QK_USER_0` 起的 enum 是否被 keymap.c include；Vial 转换表里登记了吗 |
| LAYOUT_all 参数个数和 info.json 对不上 | info.json `layouts.keymap.layout` 数组长度 = keymap.c `LAYOUT_all(...)` 参数数 = `MATRIX_ROWS×MATRIX_COLS` 中被引用的位置数，三处必须一致 |
| 改了 qmk_config.h 不生效 | CMake 缓存：删 build 目录重配（rules.cmake 里 `CACHE BOOL ... FORCE` 只管开关，不管头文件缓存） |

### 按键 / 矩阵类

| 现象 | 检查顺序 |
|---|---|
| 某键无响应 | ① info.json 该位置的 matrix 坐标 vs qmk_config.h 引脚表 ② 万用表量该键行列线通断（磁吸触点氧化）③ `rules.cmake` 里 CUSTOM_MATRIX 是否误开 |
| 模块所有键无响应 | ① 模块位置识别结果（ADC 判定是否误判成 MOD_NONE——阈值表是否还是占位 0 值）② 磁吸 14pin 中行列 9 pin 是否接插到位 ③ info.json 模块区的 matrix 坐标是否落在预留行（5/6 行） |
| 互换后键序错乱 | ① 位置 ADC 判定左右是否正确（打印两路采样值对照阈值表）② 两套布局槽位的动态键图存储是否按布局分开（Vial 存储按槽位） |
| 磁吸触点振动下断键 | 抬起去抖：`DEBOUNCE 10` 是否生效；仍抖 → 开 CUSTOM_MATRIX 用 `MODKB_DEBOUNCE_UP_MS`（8–10ms 可吸收触点瞬断，说明书 5.1） |
| 长按连发/卡键 | 按下/抬起去抖不对称配置；USB/BLE 切换路径是否释放了旧链路按键状态（devmgr 转移前必须清） |

### 蓝牙类

| 现象 | 检查顺序 |
|---|---|
| 配对后重连失败 | ① slot 数据 CRC 是否校验失败被丢弃（devmgr 异常路径：坏一弃一，重开通用广播）② `BLE_SLOT_NUM` 是否为 2 且键码用 BLE_SLOT0/1 |
| 切 BT1/BT2 回连慢 | 定向广播（directed adv）是否发出；目标 slot 是否真的有绑定数据（空 slot 会开 60s 通用广播等配对，不是回连慢） |
| 两 slot 都配过后新设备配不进 | 覆盖策略：新配对覆盖当前不活跃 slot，确认活跃 slot 判断逻辑 |
| BLE 显示已连但发不出键（尤其唤醒后） | 说明书 5.5 高频坑：唤醒路径必须完整重置协议栈状态，查 `suspend_wakeup_init_kb` 是否走了完整重初始化；另外查 32M 晶振稳定等待 `WAKE_UP_RTC_MAX_TIME`（sdk/HAL/SLEEP.c 注释） |
| 天线附近信号弱 | halconf.h `BLE_TX_POWER`；天线净空区是否被铺铜/电池/螺丝金属件侵占（说明书 3.5，硬件问题） |

### USB / 模式切换类

| 现象 | 检查顺序 |
|---|---|
| 插充电头键盘卡死在有线模式 | `MODKB_USB_ENUM_TIMEOUT_MS` 是否在 devmgr 里真正生效（超时→关 USB→回 BLE）；超时值必须实测，别假定 300ms |
| 插电脑不进有线 | VBUS 检测脚电平；枚举成功事件是否被 devmgr 收到（`ST_USB_PENDING` 卡住说明等的事件没来） |
| USB/蓝牙切换瞬间丢键 | 切换前释放旧连接按键状态 + 短暂缓存（说明书 5.6）；查 devmgr 各转移路径 |
| 拔线后蓝牙不回来 | 拔线事件去抖是否过长；关 USB 后是否重开了广播/回连 |

### mac/win 类

| 现象 | 检查顺序 |
|---|---|
| mac 下 Win 键仍弹出 Spotlight 等错位行为 | `modkb_osmode_process_record` 是否真的被 `process_record_kb` 调到；macOS 下修饰键映射后还要查系统级修饰键设置 |
| 切换后重启丢失 | os_mode 持久化写 eeprom 失败（连接活跃时不写 flash，延迟落盘机制；确认落盘发生在写请求后 100ms 间隙） |
| 切了但灯不闪 | modkb_leds_pattern 是否非阻塞实现、task 是否在 housekeeping 里推进 |

### 休眠类

| 现象 | 检查顺序 |
|---|---|
| 不休眠 | ① 空闲计时有没有被持续喂（滑阻 task 若在未接入时仍走 feed 会永不休眠——未接入必须不喂）② devmgr 当前状态是否属于「不睡」档（BLE 已连/USB 模式按设计不睡） |
| 休眠电流降不下来 | 深睡前关灯、停扫描了吗（轮询电流降不下来等于没睡）；GPIO 悬空漏电（硬件） |
| 唤醒首键丢字 | 唤醒延迟期间按键缓存（`modkb_sleep_wake_cached_key`）是否在唤醒路径里补发 |
| 唤醒后键图异常 | 唤醒重初始化顺序：外设 → 键图校验 → 恢复原状态 |

### 滑阻 / ADC 类

| 现象 | 检查顺序 |
|---|---|
| 滑阻误触发 | 1–4V 在位判定阈值（`MODKB_ADC_TH_SLIDER_LOW/HIGH` 占位值需换算成实际 ADC 计数）；未接入时 ADC 悬空读数是否落在判定区间内 |
| 音量只加不减 | 位置→步进的方向判定；上次位置缓存是否被误清 |
| 模块识别不稳 | 采样平均/去抖；说明书 4.3：ADC 超时/越界一律按未接入处理（键图只留 64 键），不要拿异常值猜模块 |
| 电量跳变 | 新分压比标定（说明书 4.2：现有 battery_measure 的标定接口按新硬件重标）；LiPo 3.3–4.2V 曲线表 |

### 存储类

| 现象 | 检查顺序 |
|---|---|
| 改键数据全丢 | **`VIAL_STORE_VERSION` 必须 bump**——矩阵尺寸/布局槽位一变旧数据必须作废（说明书 5.2，团队踩过 5×4→5×14 的坑） |
| CRC 校验失败频发 | 擦除是否按 256B 页向上取整；连接活跃时不写 flash（沿用 100ms 延迟落盘） |

---

## 四、当前 TODO 清单（等硬件冻结）

1. 矩阵行列引脚、模块延伸段行列分布（F 区 15 键 / PAD 17 键）→ `qmk_config.h` + `info.json` + keymap
2. 三路 ADC 通道（左位置 / 右位置 / 滑阻）→ `modkb_module.c`
3. ADC 阈值表全部实测换算值 → `qmk_config.h`
4. 状态灯 GPIO（四灯方案）→ `qmk_config.h` + `modkb_leds.c`
5. 开关三态脚定义 → `modkb_devmgr.c`
6. USB 枚举超时实测值 → `modkb_devmgr.c`
7. vial.json（两套布局分别画 + 自定义键码登记）
8. 产测模式（串口/raw HID 进入、全键遍历、MAC/序列号写入）——说明书第七节，M6 里程碑再搭
