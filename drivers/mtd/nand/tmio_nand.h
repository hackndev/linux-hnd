/* TMIO NAND control registers */
#define TNAND_CMD_REG     0x000
#define TNAND_MODE_REG    0x004
#define TNAND_STATUS_REG  0x005
#define TNAND_INTCTL_REG  0x006
#define TNAND_INTMASK_REG 0x007

/* These bits directly control the smartmedia control signals. */
#define TNAND_MODE_BIT_WE                    0x80
#define TNAND_MODE_BIT_CE                    0x10
#define TNAND_MODE_BIT_PCNT1                 0x08
#define TNAND_MODE_BIT_PCNT0                 0x04
#define TNAND_MODE_BIT_ALE                   0x02
#define TNAND_MODE_BIT_CLE                   0x01

/* Modes that can be programmed into the TMIO NAND mode reg */
#define TNAND_MODE_READ_COMMAND              0x15
#define TNAND_MODE_READ_ADDRESS              0x16
#define TNAND_MODE_READ_DATAREAD             0x14

#define TNAND_MODE_WRITE_COMMAND             0x95
#define TNAND_MODE_WRITE_ADDRESS             0x96
#define TNAND_MODE_WRITE_DATAWRITE           0x94

#define TNAND_MODE_POWER_ON                  0x0C    // Enable SMVC3EN
#define TNAND_MODE_POWER_OFF                 0x08    // Disable SMVC3EN
#define TNAND_MODE_LED_OFF                   0x00    // LED OFF
#define TNAND_MODE_LED_ON                    0x04    // LED ON

#define TNAND_MODE_EJECT_ON                  0x68    // Eject request
#define TNAND_MODE_EJECT_OFF                 0x08    // Eject request

#define TNAND_MODE_LOCK                      0x6C    // Disable eject switch
#define TNAND_MODE_UNLOCK                    0x0C    // Enable eject switch

#define TNAND_MODE_HWECC_READ_ECCCALC        0x34
#define TNAND_MODE_HWECC_READ_RESET_ECC      0x74
#define TNAND_MODE_HWECC_READ_CALC_RESULT    0x54

#define TNAND_MODE_HWECC_WRITE_ECCCALC       0xB4
#define TNAND_MODE_HWECC_WRITE_RESET_ECC     0xF4
#define TNAND_MODE_HWECC_WRITE_CALC_RESULT   0xD4

#define TNAND_MODE_CONTROLLER_ID_READ        0x40
#define TNAND_MODE_STANDBY                   0x00

/* Status register definitions */

#define TNAND_STATUS_BUSY 0x80

/* Some config reg values */
#define SMCREN 0x2
#define SCRUNEN 0x80 

struct tmio_mtd_data {
	struct mtd_info *mtd;
	void * cnf_base;
	void * ctl_base;
};

