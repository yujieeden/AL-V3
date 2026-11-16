/*******************************************************************************
 * File Name          : I2cDev.c
 * Description        : 软件模拟 I2C 主机实现，详见 I2cDev.h。
 *
 *                      引脚：PB0 = SCL，PB1 = SDA（硬件 FFC 固定接法）。
 *                      电平：拉低 = 推挽输出 0；释放 = 浮空输入，由外部 4.7k
 *                      上拉抬高（CENTER 板已确认有上拉）。
 *
 *                      注意：GPIO 模式切换比直接写数据寄存器慢，但一次矩阵扫描
 *                      只有十几次事务，可以接受。
 *******************************************************************************/

#include "I2cDev.h"

/* ---------------------------------------------------------------- *
 *  引脚原语
 * ---------------------------------------------------------------- */
#define SCL_H()     do { GPIOB_ModeCfg(I2CDEV_PIN_SCL, GPIO_ModeIN_Floating); } while(0)
#define SCL_L()     do { GPIOB_ModeCfg(I2CDEV_PIN_SCL, GPIO_ModeOut_PP_5mA); \
                         GPIOB_ResetBits(I2CDEV_PIN_SCL); } while(0)
#define SDA_H()     do { GPIOB_ModeCfg(I2CDEV_PIN_SDA, GPIO_ModeIN_Floating); } while(0)
#define SDA_L()     do { GPIOB_ModeCfg(I2CDEV_PIN_SDA, GPIO_ModeOut_PP_5mA); \
                         GPIOB_ResetBits(I2CDEV_PIN_SDA); } while(0)
#define SDA_READ()  (GPIOB_ReadPortPin(I2CDEV_PIN_SDA) ? 1u : 0u)

/* 时钟拉伸保护：等 SCL 真的变高（从机拉低时钟时），有超时上限 */
#define SCL_WAIT_HIGH_TMO     2000u

static uint8_t scl_wait_high(void)
{
    uint16_t tmo = SCL_WAIT_HIGH_TMO;

    SCL_H();
    while(tmo--)
    {
        if(GPIOB_ReadPortPin(I2CDEV_PIN_SCL))
        {
            return I2CDEV_OK;
        }
    }
    return I2CDEV_ERR_NACK;      /* 总线被卡住，当超时处理 */
}

/*********************************************************************
 * @fn      i2c_delay
 *
 * @brief   半个位时间的电平保持。
 */
static void i2c_delay(void)
{
    mDelayuS(I2CDEV_HALF_BIT_US);
}

/*********************************************************************
 * @fn      i2c_start
 */
static uint8_t i2c_start(void)
{
    uint8_t r;

    SDA_H();
    r = scl_wait_high();
    if(r != I2CDEV_OK)
    {
        return r;
    }
    i2c_delay();
    SDA_L();                     /* SDA 先落 */
    i2c_delay();
    SCL_L();                     /* 再拉低 SCL，准备送地址 */
    i2c_delay();
    return I2CDEV_OK;
}

/*********************************************************************
 * @fn      i2c_stop
 */
static void i2c_stop(void)
{
    SDA_L();
    i2c_delay();
    (void)scl_wait_high();
    i2c_delay();
    SDA_H();                     /* SDA 后升 */
    i2c_delay();
}

/*********************************************************************
 * @fn      i2c_write_byte
 *
 * @brief   高位在前送一个字节，返回从机是否 ACK。
 */
static uint8_t i2c_write_byte(uint8_t dat)
{
    uint8_t i;
    uint8_t ack;

    for(i = 0; i < 8; i++)
    {
        if(dat & 0x80)
        {
            SDA_H();
        }
        else
        {
            SDA_L();
        }
        dat <<= 1;
        i2c_delay();
        if(scl_wait_high() != I2CDEV_OK)
        {
            return I2CDEV_ERR_NACK;
        }
        i2c_delay();
        SCL_L();
        i2c_delay();
    }

    /* 第 9 个时钟读 ACK */
    SDA_H();                                 /* 释放 SDA 交给从机 */
    i2c_delay();
    if(scl_wait_high() != I2CDEV_OK)
    {
        return I2CDEV_ERR_NACK;
    }
    i2c_delay();
    ack = SDA_READ();                        /* 0 = ACK, 1 = NACK */
    SCL_L();
    i2c_delay();

    return (ack == 0) ? I2CDEV_OK : I2CDEV_ERR_NACK;
}

/*********************************************************************
 * @fn      i2c_read_byte
 *
 * @brief   高位在前读一个字节；last=1 时回 NACK，否则回 ACK。
 */
static uint8_t i2c_read_byte(uint8_t *out, uint8_t last)
{
    uint8_t i;
    uint8_t dat = 0;

    SDA_H();                                 /* 释放 SDA 交给从机 */
    for(i = 0; i < 8; i++)
    {
        dat <<= 1;
        i2c_delay();
        if(scl_wait_high() != I2CDEV_OK)
        {
            return I2CDEV_ERR_NACK;
        }
        i2c_delay();
        if(SDA_READ())
        {
            dat |= 1;
        }
        SCL_L();
        i2c_delay();
    }

    /* 第 9 个时钟回 ACK/NACK */
    if(last)
    {
        SDA_H();                             /* NACK：释放，由上拉抬高 */
    }
    else
    {
        SDA_L();                             /* ACK：拉低 */
    }
    i2c_delay();
    if(scl_wait_high() != I2CDEV_OK)
    {
        return I2CDEV_ERR_NACK;
    }
    i2c_delay();
    SCL_L();
    i2c_delay();

    *out = dat;
    return I2CDEV_OK;
}

/*********************************************************************
 * @fn      I2cDev_Init
 */
void I2cDev_Init(void)
{
    uint8_t i;

    /* 两根线都先释放，靠外部上拉置高 */
    GPIOB_ModeCfg(I2CDEV_PIN_SCL | I2CDEV_PIN_SDA, GPIO_ModeIN_Floating);
    mDelayuS(50);

    /* 发 9 个时钟把可能卡在半字节上的从机冲出来 */
    for(i = 0; i < 9; i++)
    {
        SCL_H();
        i2c_delay();
        SCL_L();
        i2c_delay();
    }

    /* 补一个 STOP 收尾 */
    SDA_L();
    i2c_delay();
    SCL_H();
    i2c_delay();
    SDA_H();
    i2c_delay();
}

/*********************************************************************
 * @fn      I2cDev_Write
 */
uint8_t I2cDev_Write(uint8_t addr7, const uint8_t *buf, uint8_t len)
{
    uint8_t r;
    uint8_t i;

    if((buf == NULL) || (len == 0))
    {
        return I2CDEV_ERR_ARG;
    }

    r = i2c_start();
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }

    r = i2c_write_byte((uint8_t)(addr7 << 1));        /* 写方向 */
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }

    for(i = 0; i < len; i++)
    {
        r = i2c_write_byte(buf[i]);
        if(r != I2CDEV_OK)
        {
            i2c_stop();
            return r;
        }
    }

    i2c_stop();
    return I2CDEV_OK;
}

/*********************************************************************
 * @fn      I2cDev_Read
 */
uint8_t I2cDev_Read(uint8_t addr7, uint8_t *buf, uint8_t len)
{
    uint8_t r;
    uint8_t i;

    if((buf == NULL) || (len == 0))
    {
        return I2CDEV_ERR_ARG;
    }

    r = i2c_start();
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }

    r = i2c_write_byte((uint8_t)((addr7 << 1) | 0x01));   /* 读方向 */
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }

    for(i = 0; i < len; i++)
    {
        r = i2c_read_byte(&buf[i], (i == (len - 1)) ? 1u : 0u);
        if(r != I2CDEV_OK)
        {
            i2c_stop();
            return r;
        }
    }

    i2c_stop();
    return I2CDEV_OK;
}

/*********************************************************************
 * @fn      I2cDev_ReadReg
 *
 * @brief   写 reg -> 重复 START -> 读 len 字节。
 *          注意：i2c_start 之前 SDA 必须是高的，上一步写完后已经满足。
 */
uint8_t I2cDev_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t r;
    uint8_t i;

    if((buf == NULL) || (len == 0))
    {
        return I2CDEV_ERR_ARG;
    }

    /* 第一段：写寄存器地址 */
    r = i2c_start();
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }
    r = i2c_write_byte((uint8_t)(addr7 << 1));
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }
    r = i2c_write_byte(reg);
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }

    /* 第二段：重复 START 切读方向 */
    r = i2c_start();
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }
    r = i2c_write_byte((uint8_t)((addr7 << 1) | 0x01));
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }

    for(i = 0; i < len; i++)
    {
        r = i2c_read_byte(&buf[i], (i == (len - 1)) ? 1u : 0u);
        if(r != I2CDEV_OK)
        {
            i2c_stop();
            return r;
        }
    }

    i2c_stop();
    return I2CDEV_OK;
}

/*********************************************************************
 * @fn      I2cDev_WriteReg
 */
uint8_t I2cDev_WriteReg(uint8_t addr7, uint8_t reg, const uint8_t *buf, uint8_t len)
{
    uint8_t r;
    uint8_t i;

    if((buf == NULL) || (len == 0))
    {
        return I2CDEV_ERR_ARG;
    }

    r = i2c_start();
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }
    r = i2c_write_byte((uint8_t)(addr7 << 1));
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }
    r = i2c_write_byte(reg);
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return r;
    }

    for(i = 0; i < len; i++)
    {
        r = i2c_write_byte(buf[i]);
        if(r != I2CDEV_OK)
        {
            i2c_stop();
            return r;
        }
    }

    i2c_stop();
    return I2CDEV_OK;
}

/*********************************************************************
 * @fn      I2cDev_Ping
 */
uint8_t I2cDev_Ping(uint8_t addr7)
{
    uint8_t r;

    r = i2c_start();
    if(r != I2CDEV_OK)
    {
        i2c_stop();
        return 0;
    }
    r = i2c_write_byte((uint8_t)(addr7 << 1));
    i2c_stop();

    return (r == I2CDEV_OK) ? 1u : 0u;
}
