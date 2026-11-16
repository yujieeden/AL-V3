/*******************************************************************************
 * File Name          : I2cDev.h
 * Description        : 软件模拟 I2C 主机（PB0 = SCL，PB1 = SDA）。
 *
 *                      为什么用软件模拟：硬件把 I2C 接在 PB0/PB1，而 CH582/583 的
 *                      硬件 I2C 外设只能映射到 PB12/PB13 或 PB20/PB21
 *                      （R16_PIN_ALTERNATE 的 RB_PIN_I2C），PB0/PB1 没有 I2C
 *                      复用功能，只能 GPIO 模拟。
 *
 *                      总线上有外部 4.7k 上拉到 3V3（CENTER 板已确认），所以
 *                      "拉低"用推挽输出 0，"释放"用浮空输入由上拉抬高。
 *
 *                      使用者：Tca9535（两颗 IO 扩展器）、以及后续所有挂在这
 *                      条总线上的从设备。
 *******************************************************************************/

#ifndef __I2CDEV_H
#define __I2CDEV_H

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 引脚：硬件 FFC 固定接法 PB0 = SCL，PB1 = SDA */
#define I2CDEV_PIN_SCL         GPIO_Pin_0
#define I2CDEV_PIN_SDA         GPIO_Pin_1

/* 返回值 */
#define I2CDEV_OK          0
#define I2CDEV_ERR_NACK    1   /* 从机无应答 */
#define I2CDEV_ERR_ARG     2   /* 参数非法 */

/* 默认总线速率：每次电平翻转之间的延时(微秒)。
 * 2us => 半周期 4us => 约 125kHz，对 TCA9535 足够且留了余量。 */
#ifndef I2CDEV_HALF_BIT_US
#define I2CDEV_HALF_BIT_US     2
#endif

/*********************************************************************
 * @fn      I2cDev_Init
 *
 * @brief   初始化 I2C 引脚（PB0/PB1 都置浮空输入，靠外部上拉抬高），
 *          并发出几个时钟把总线上的从机状态理顺。
 *
 * @return  none
 */
extern void I2cDev_Init(void);

/*********************************************************************
 * @fn      I2cDev_Write
 *
 * @brief   向从机写一段数据（START + 地址(写) + 数据 + STOP）。
 *
 * @param   addr7 - 7 位从机地址
 * @param   buf   - 待写数据
 * @param   len   - 数据长度
 *
 * @return  I2CDEV_OK / I2CDEV_ERR_NACK / I2CDEV_ERR_ARG
 */
extern uint8_t I2cDev_Write(uint8_t addr7, const uint8_t *buf, uint8_t len);

/*********************************************************************
 * @fn      I2cDev_Read
 *
 * @brief   从从机读一段数据（START + 地址(读) + 数据 + STOP）。
 *          最后一个字节回 NACK，前面的字节回 ACK。
 *
 * @param   addr7 - 7 位从机地址
 * @param   buf   - 接收缓冲
 * @param   len   - 要读的长度
 *
 * @return  I2CDEV_OK / I2CDEV_ERR_NACK / I2CDEV_ERR_ARG
 */
extern uint8_t I2cDev_Read(uint8_t addr7, uint8_t *buf, uint8_t len);

/*********************************************************************
 * @fn      I2cDev_ReadReg
 *
 * @brief   先写一个寄存器地址，再重复 START 读数据（TCA9535 的标准读法）。
 *
 * @param   addr7 - 7 位从机地址
 * @param   reg   - 寄存器/命令字节
 * @param   buf   - 接收缓冲
 * @param   len   - 要读的长度
 *
 * @return  I2CDEV_OK / I2CDEV_ERR_NACK / I2CDEV_ERR_ARG
 */
extern uint8_t I2cDev_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len);

/*********************************************************************
 * @fn      I2cDev_WriteReg
 *
 * @brief   先写寄存器地址再写数据（单次 START，自动递增写多个寄存器）。
 *
 * @param   addr7 - 7 位从机地址
 * @param   reg   - 起始寄存器/命令字节
 * @param   buf   - 待写数据
 * @param   len   - 数据长度
 *
 * @return  I2CDEV_OK / I2CDEV_ERR_NACK / I2CDEV_ERR_ARG
 */
extern uint8_t I2cDev_WriteReg(uint8_t addr7, uint8_t reg, const uint8_t *buf, uint8_t len);

/*********************************************************************
 * @fn      I2cDev_Ping
 *
 * @brief   探测总线上某地址是否有从机应答（用来判断扩展器在不在）。
 *
 * @param   addr7 - 7 位从机地址
 *
 * @return  1-有应答, 0-无应答
 */
extern uint8_t I2cDev_Ping(uint8_t addr7);

#ifdef __cplusplus
}
#endif

#endif /* __I2CDEV_H */
