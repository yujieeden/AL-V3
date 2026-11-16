/*
Copyright 2026 ModKB contributors

说明书 5.1 / 5.2 / 5.7：
- 模块矩阵是主矩阵延伸段，拔掉 = 行列线开路 = 扫不到按下，无需在线检测。
- 左右互换识别：模块内预置不同分压电阻，主控 ADC 采样区分左/右。
- 滑阻模块：1V-4V 为有效接入，电压 -> 步进事件 -> 音量 +/-。
*/

#include QMK_KEYBOARD_H
#include "modkb_module.h"

// TODO(引脚冻结后): 两路位置识别 ADC 通道 + 滑阻 ADC 通道填实际引脚
#define MODKB_ADC_PIN_POS_LEFT  A14
#define MODKB_ADC_PIN_POS_RIGHT A15
#define MODKB_ADC_PIN_SLIDER    A13

static modkb_module_layout_t layout = { MOD_NONE, MOD_NONE };

/* 滑阻软件状态 */
static int16_t slider_last_pos = 0;
static uint8_t slider_stable_cnt = 0;

__attribute__((weak)) void modkb_module_init(void) {
    // TODO: 初始化三路 ADC（CH58x ADC，参考电压 3.3V）
}

__attribute__((weak)) void modkb_module_detect(void) {
    // TODO(硬件冻结后实现):
    // 1. 分别采样左右两路识别 ADC（带超时容错，说明书 4.3：
    //    电压超时/越界 -> 按未接入处理，键图只留主体 64 键）
    // 2. 对照 qmk_config.h 里的 MODKB_ADC_TH_F/PAD_LOW/HIGH 阈值表
    // 3. 更新 layout；变化时触发键图布局切换 + 状态灯提示
}

const modkb_module_layout_t *modkb_module_get_layout(void) {
    return &layout;
}

__attribute__((weak)) void modkb_slider_task(void) {
    // TODO(硬件冻结后实现), 说明书 5.7：
    // 1. 100Hz 采样滑阻 ADC，1-4V 区间外 = 未接入，直接返回（悬空不误触）
    // 2. 电压 -> 位置 -> 与上次位置比较，判定上/下拨一格
    // 3. 发一次 tap_code16(KC_VOLU / KC_VOLD)
    // 4. 滑阻如有按压：映射静音或用户自定义（进 Vial 自定义动作）
    (void)slider_last_pos;
    (void)slider_stable_cnt;
}
