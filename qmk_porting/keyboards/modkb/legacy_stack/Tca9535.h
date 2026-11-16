/*******************************************************************************
 * File Name          : Tca9535.h
 * Description        : TCA9535PWR（16 位 I2C IO 扩展器）驱动。
 *
 *                      硬件用法（见《当前硬件控制交接》）：
 *                        CENTER 板 U1 @0x20：P00-P04 = 5 行扫描输出，
 *                                            P05/P06 = LED703/LED704，
 *                                            P07 = 停充，P10/P11 = BQ EN1/EN2，
 *                                            P12/P13 = PGOOD#/CHG#(输入)，
 *                                            P14 = 电池 ADC 分压使能，
 *                                            P15/P16 = 左右 FAULT#(输入)，
 *                                            P17 = U306 待机旁路使能
 *                        CENTER 板 U2 @0x21：P00-P07 + P10 + P13-P17 = 14 列输入，
 *                                            P11/P12 = LED701/LED702
 *                        RIGHT 板  U1 @0x20：P00-P04 = 5 行，P10-P14 = 5 列
 *
 *                      寄存器（自动递增写，可一次写多个）：
 *                        0x00 输入口0  0x01 输入口1
 *                        0x02 输出口0  0x03 输出口1
 *                        0x04 极性反转0 0x05 极性反转1
 *                        0x06 方向0(1=输入) 0x07 方向1(1=输入)
 *
 *                      影子寄存器：本固件里多个功能共用同一个口（行扫描 / 电量灯 /
 *                      停充 / 电源），直接整口写会互相覆盖。驱动内部维护输出口影子，
 *                      所有写都走"改影子 -> 只写变化位"，保证不覆盖别人的位。
 *
 *                      注意：仅复位 MCU 不会复位持续供电的扩展器，所以每次上电都要
 *                      重新建立已知状态（重设方向 + 重装输出影子）。
 *******************************************************************************/

#ifndef __TCA9535_H
#define __TCA9535_H

#include "CH58x_common.h"
#include "I2cDev.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 寄存器地址 */
#define TCA9535_REG_IN0        0x00
#define TCA9535_REG_IN1        0x01
#define TCA9535_REG_OUT0       0x02
#define TCA9535_REG_OUT1       0x03
#define TCA9535_REG_POL0       0x04
#define TCA9535_REG_POL1       0x05
#define TCA9535_REG_CFG0       0x06
#define TCA9535_REG_CFG1       0x07

/* 口编号 */
#define TCA9535_PORT0          0
#define TCA9535_PORT1          1

/* 位号：port0 的 bit n = P0n，port1 的 bit n = P1n */
#define TCA9535_P00            (1u << 0)
#define TCA9535_P01            (1u << 1)
#define TCA9535_P02            (1u << 2)
#define TCA9535_P03            (1u << 3)
#define TCA9535_P04            (1u << 4)
#define TCA9535_P05            (1u << 5)
#define TCA9535_P06            (1u << 6)
#define TCA9535_P07            (1u << 7)
#define TCA9535_P10            (1u << 0)
#define TCA9535_P11            (1u << 1)
#define TCA9535_P12            (1u << 2)
#define TCA9535_P13            (1u << 3)
#define TCA9535_P14            (1u << 4)
#define TCA9535_P15            (1u << 5)
#define TCA9535_P16            (1u << 6)
#define TCA9535_P17            (1u << 7)

/* 器件句柄 */
typedef struct
{
    uint8_t  addr;        /* 7 位 I2C 地址 */
    uint8_t  cfg[2];      /* 影子：方向寄存器，1 = 输入 */
    uint8_t  out[2];      /* 影子：输出寄存器 */
    uint8_t  present;     /* 上电探测到从机在总线上了 */
} tca9535_t;

/*********************************************************************
 * @fn      Tca9535_Init
 *
 * @brief   探测从机、读回当前方向/输出寄存器作为影子、并强制极性反转寄存器为 0
 *          （硬件要求极性反转保持 0）。
 *
 * @param   dev   - 器件句柄（调用方提供存储）
 * @param   addr7 - 7 位 I2C 地址
 *
 * @return  none（用 Tca9535_IsPresent 判断在不在）
 */
extern void Tca9535_Init(tca9535_t *dev, uint8_t addr7);

/*********************************************************************
 * @fn      Tca9535_IsPresent
 */
extern uint8_t Tca9535_IsPresent(const tca9535_t *dev);

/*********************************************************************
 * @fn      Tca9535_SetDirection
 *
 * @brief   设置某个口的方向（1 = 输入，0 = 输出）。会更新影子。
 *
 * @return  I2CDEV_OK 或错误码
 */
extern uint8_t Tca9535_SetDirection(tca9535_t *dev, uint8_t port, uint8_t dirMask);

/*********************************************************************
 * @fn      Tca9535_SetPinDirection
 *
 * @brief   按位设置单个引脚方向，不影响该口其他位（影子 + 只写变化）。
 *
 * @param   pinMask - 位掩码
 * @param   isInput - 1 = 输入，0 = 输出
 */
extern uint8_t Tca9535_SetPinDirection(tca9535_t *dev, uint8_t port,
                                       uint8_t pinMask, uint8_t isInput);

/*********************************************************************
 * @fn      Tca9535_WritePort
 *
 * @brief   整口写输出寄存器（会更新影子）。用于初始化预装。
 */
extern uint8_t Tca9535_WritePort(tca9535_t *dev, uint8_t port, uint8_t val);

/*********************************************************************
 * @fn      Tca9535_SetBits / Tca9535_ClrBits
 *
 * @brief   按位置 1 / 清 0，走影子只写实际变化的那一位，绝不覆盖同口其他位。
 */
extern uint8_t Tca9535_SetBits(tca9535_t *dev, uint8_t port, uint8_t mask);
extern uint8_t Tca9535_ClrBits(tca9535_t *dev, uint8_t port, uint8_t mask);

/*********************************************************************
 * @fn      Tca9535_SetPin
 *
 * @brief   把单个引脚设为高/低（走影子）。
 */
extern uint8_t Tca9535_SetPin(tca9535_t *dev, uint8_t port, uint8_t pinMask,
                              uint8_t level);

/*********************************************************************
 * @fn      Tca9535_ReadPort
 *
 * @brief   读一个输入口。失败返回 0xFF（正常数据里 0xFF 也可能是真实值，
 *          调用方需要的话用 Tca9535_ReadPorts 的返回值判断）。
 */
extern uint8_t Tca9535_ReadPort(tca9535_t *dev, uint8_t port);

/*********************************************************************
 * @fn      Tca9535_ReadPorts
 *
 * @brief   一次读回两个输入口（自动递增，单次 I2C 事务，矩阵扫描用这个）。
 *
 * @param   in0 - 输出：输入口0（P00-P07）
 * @param   in1 - 输出：输入口1（P10-P17）
 *
 * @return  I2CDEV_OK 或错误码
 */
extern uint8_t Tca9535_ReadPorts(tca9535_t *dev, uint8_t *in0, uint8_t *in1);

#ifdef __cplusplus
}
#endif

#endif /* __TCA9535_H */
