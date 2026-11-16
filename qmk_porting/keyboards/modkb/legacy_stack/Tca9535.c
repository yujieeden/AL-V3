/*******************************************************************************
 * File Name          : Tca9535.c
 * Description        : TCA9535PWR 驱动实现，详见 Tca9535.h。
 *
 *                      关键点：所有写输出口的动作都先更新影子，再只写真正变化的位，
 *                      这样行扫描（U1 P00-P04）不会覆盖同口上的电量灯/停充，
 *                      电源操作也不会把 FAULT#/PGOOD# 这些输入位改成输出。
 *******************************************************************************/

#include "Tca9535.h"

/*********************************************************************
 * @fn      reg_write2
 *
 * @brief   从 reg 起自动递增写 2 字节（TCA9535 支持自动递增）。
 */
static uint8_t reg_write2(tca9535_t *dev, uint8_t reg, uint8_t v0, uint8_t v1)
{
    uint8_t buf[2];

    buf[0] = v0;
    buf[1] = v1;
    return I2cDev_WriteReg(dev->addr, reg, buf, 2);
}

/*********************************************************************
 * @fn      reg_read2
 */
static uint8_t reg_read2(tca9535_t *dev, uint8_t reg, uint8_t *v0, uint8_t *v1)
{
    uint8_t buf[2];
    uint8_t r;

    r = I2cDev_ReadReg(dev->addr, reg, buf, 2);
    if(r != I2CDEV_OK)
    {
        return r;
    }
    *v0 = buf[0];
    *v1 = buf[1];
    return I2CDEV_OK;
}

/*********************************************************************
 * @fn      Tca9535_Init
 */
void Tca9535_Init(tca9535_t *dev, uint8_t addr7)
{
    uint8_t v0;
    uint8_t v1;

    dev->addr    = addr7;
    dev->present = 0;
    dev->cfg[0]  = 0xFF;          /* 没读到之前先全当输入，最安全 */
    dev->cfg[1]  = 0xFF;
    dev->out[0]  = 0x00;
    dev->out[1]  = 0x00;

    /* 探测从机在不在 */
    dev->present = I2cDev_Ping(addr7);
    if(!dev->present)
    {
        return;
    }

    /* 极性反转寄存器保持 0（硬件要求） */
    (void)reg_write2(dev, TCA9535_REG_POL0, 0x00, 0x00);

    /* 读回当前方向/输出作为影子，避免我们按错误的前提去改 */
    if(reg_read2(dev, TCA9535_REG_CFG0, &v0, &v1) == I2CDEV_OK)
    {
        dev->cfg[0] = v0;
        dev->cfg[1] = v1;
    }
    if(reg_read2(dev, TCA9535_REG_OUT0, &v0, &v1) == I2CDEV_OK)
    {
        dev->out[0] = v0;
        dev->out[1] = v1;
    }
}

/*********************************************************************
 * @fn      Tca9535_IsPresent
 */
uint8_t Tca9535_IsPresent(const tca9535_t *dev)
{
    return dev->present;
}

/*********************************************************************
 * @fn      Tca9535_SetDirection
 */
uint8_t Tca9535_SetDirection(tca9535_t *dev, uint8_t port, uint8_t dirMask)
{
    uint8_t r;

    if(port > TCA9535_PORT1)
    {
        return I2CDEV_ERR_ARG;
    }

    dev->cfg[port] = dirMask;
    r = I2cDev_WriteReg(dev->addr,
                        (uint8_t)(TCA9535_REG_CFG0 + port),
                        &dirMask, 1);
    return r;
}

/*********************************************************************
 * @fn      Tca9535_SetPinDirection
 */
uint8_t Tca9535_SetPinDirection(tca9535_t *dev, uint8_t port,
                                uint8_t pinMask, uint8_t isInput)
{
    uint8_t newcfg;

    if(port > TCA9535_PORT1)
    {
        return I2CDEV_ERR_ARG;
    }

    newcfg = (isInput != 0) ? (uint8_t)(dev->cfg[port] | pinMask)
                            : (uint8_t)(dev->cfg[port] & (uint8_t)~pinMask);
    return Tca9535_SetDirection(dev, port, newcfg);
}

/*********************************************************************
 * @fn      Tca9535_WritePort
 */
uint8_t Tca9535_WritePort(tca9535_t *dev, uint8_t port, uint8_t val)
{
    uint8_t r;

    if(port > TCA9535_PORT1)
    {
        return I2CDEV_ERR_ARG;
    }

    dev->out[port] = val;
    r = I2cDev_WriteReg(dev->addr,
                        (uint8_t)(TCA9535_REG_OUT0 + port),
                        &val, 1);
    return r;
}

/*********************************************************************
 * @fn      out_apply_bits
 *
 * @brief   影子改位，只把实际变化的位写出去。
 */
static uint8_t out_apply_bits(tca9535_t *dev, uint8_t port,
                              uint8_t mask, uint8_t level)
{
    uint8_t newval;
    uint8_t diff;

    if(port > TCA9535_PORT1)
    {
        return I2CDEV_ERR_ARG;
    }

    newval = (level != 0) ? (uint8_t)(dev->out[port] | mask)
                          : (uint8_t)(dev->out[port] & (uint8_t)~mask);

    diff = (uint8_t)(newval ^ dev->out[port]);
    if(diff == 0)
    {
        return I2CDEV_OK;              /* 没变化，省一次 I2C 事务 */
    }

    dev->out[port] = newval;
    return I2cDev_WriteReg(dev->addr,
                           (uint8_t)(TCA9535_REG_OUT0 + port),
                           &newval, 1);
}

/*********************************************************************
 * @fn      Tca9535_SetBits / Tca9535_ClrBits
 */
uint8_t Tca9535_SetBits(tca9535_t *dev, uint8_t port, uint8_t mask)
{
    return out_apply_bits(dev, port, mask, 1);
}

uint8_t Tca9535_ClrBits(tca9535_t *dev, uint8_t port, uint8_t mask)
{
    return out_apply_bits(dev, port, mask, 0);
}

uint8_t Tca9535_SetPin(tca9535_t *dev, uint8_t port, uint8_t pinMask, uint8_t level)
{
    return out_apply_bits(dev, port, pinMask, level);
}

/*********************************************************************
 * @fn      Tca9535_ReadPort
 */
uint8_t Tca9535_ReadPort(tca9535_t *dev, uint8_t port)
{
    uint8_t v;

    if(port > TCA9535_PORT1)
    {
        return 0xFF;
    }
    if(I2cDev_ReadReg(dev->addr,
                      (uint8_t)(TCA9535_REG_IN0 + port),
                      &v, 1) != I2CDEV_OK)
    {
        return 0xFF;
    }
    return v;
}

/*********************************************************************
 * @fn      Tca9535_ReadPorts
 */
uint8_t Tca9535_ReadPorts(tca9535_t *dev, uint8_t *in0, uint8_t *in1)
{
    return reg_read2(dev, TCA9535_REG_IN0, in0, in1);
}
