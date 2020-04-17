/* I2C registers */
#define R51x_I2C_W_SID		0x41
#define R51x_I2C_SADDR_3	0x42
#define R51x_I2C_SADDR_2	0x43
#define R51x_I2C_R_SID		0x44
#define R51x_I2C_DATA		0x45
#define R518_I2C_CTL		0x47	/* OV518(+) only */
#define OVFX2_I2C_ADDR		0x00

/* OV519 Camera interface register numbers */
#define OV519_R10_H_SIZE		0x10
#define OV519_R11_V_SIZE		0x11
#define OV519_R12_X_OFFSETL		0x12
#define OV519_R13_X_OFFSETH		0x13
#define OV519_R14_Y_OFFSETL		0x14
#define OV519_R15_Y_OFFSETH		0x15
#define OV519_R16_DIVIDER		0x16
#define OV519_R20_DFR			0x20
#define OV519_R25_FORMAT		0x25

/* OV519 System Controller register numbers */
#define OV519_R51_RESET1		0x51
#define OV519_R54_EN_CLK1		0x54
#define OV519_R57_SNAPSHOT		0x57

#define OV519_GPIO_DATA_OUT0		0x71
#define OV519_GPIO_IO_CTRL0		0x72

/* OV7610 registers */
#define OV7610_REG_GAIN		0x00	/* gain setting (5:0) */
#define OV7610_REG_BLUE		0x01	/* blue channel balance */
#define OV7610_REG_RED		0x02	/* red channel balance */
#define OV7610_REG_SAT		0x03	/* saturation */
#define OV8610_REG_HUE		0x04	/* 04 reserved */
#define OV7610_REG_CNT		0x05	/* Y contrast */
#define OV7610_REG_BRT		0x06	/* Y brightness */
#define OV7610_REG_COM_C	0x14	/* misc common regs */
#define OV7610_REG_ID_HIGH	0x1c	/* manufacturer ID MSB */
#define OV7610_REG_ID_LOW	0x1d	/* manufacturer ID LSB */
#define OV7610_REG_COM_I	0x29	/* misc settings */

