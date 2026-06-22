#ifndef __I2C_QUP_H__
#define __I2C_QUP_H__

// ADV7533 is on BLSP1_QUP4 (HS-I2C2) -> Base: 0x078B8000
#define QUP_BASE                0x078B8000

// QUP Core Registers
#define QUP_CONFIG              0x000
#define QUP_STATE               0x004
#define QUP_IO_MODE             0x008
#define QUP_SW_RESET            0x00c
#define QUP_OPERATIONAL         0x018
#define QUP_ERROR_FLAGS         0x01c
#define QUP_ERROR_FLAGS_EN      0x020
#define QUP_HW_VERSION          0x030
#define QUP_MX_OUTPUT_CNT       0x100
#define QUP_OUT_FIFO_BASE       0x110
#define QUP_MX_INPUT_CNT        0x200
#define QUP_MX_READ_CNT         0x208
#define QUP_IN_FIFO_BASE        0x218
#define QUP_I2C_CLK_CTL         0x400
#define QUP_I2C_STATUS          0x404
#define QUP_I2C_MASTER_GEN      0x408

// QUP_STATE values
#define QUP_STATE_RESET         0
#define QUP_STATE_RUN           1
#define QUP_STATE_PAUSE         3
#define QUP_STATE_VALID         (1 << 19)

#endif // __I2C_QUP_H__
