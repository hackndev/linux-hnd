/*
 * linux/arch/arm/mach-sa1100/jornada56x.c
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <linux/apm-emulation.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <asm/mach/flash.h>
#include <asm/arch/jornada56x.h>
#include <asm/arch/bitfield.h>

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <../drivers/video/sa1100fb.h>
#include <linux/platform_device.h>

#include "generic.h"

/***********************************************
 *                                             *
 * Jornada 56x LCD and backlight functions     *
 *                                             *
 ***********************************************/
 
static struct lcd_device *sa1100_lcd_device;
static struct backlight_device *sa1100_backlight_device;

static void jornada56x_lcd_power(int on)
{
	if (on) {
		GPSR = GPIO_GPIO24;
	} else {
		GPCR = GPIO_GPIO24;
	}
}

static int jornada56x_lcd_get_power(struct lcd_device *ld)
{
	return (GPLR & GPIO_GPIO24) ? 0 : 4;
}

static int jornada56x_lcd_set_power(struct lcd_device *ld, int state)
{
	if (state==0)
		jornada56x_lcd_power(1);
	else
		jornada56x_lcd_power(0);
	
	return 0;
}

static struct lcd_ops jornada56x_lcd_ops = {
	.set_power	= jornada56x_lcd_set_power,
	.get_power	= jornada56x_lcd_get_power,
};

static void jornada56x_backlight_power (int on)
{
        if (on)
                JORNADA_GPDPCR = JORNADA_BACKLIGHT;
        else
                JORNADA_GPDPSR = JORNADA_BACKLIGHT;
}

static int jornada56x_backlight_get_brightness (struct backlight_device *bd)
{
        return (JORNADA_PWM1_DATA - 0x40000000);
}

static int jornada56x_backlight_set_brightness (struct backlight_device *bd)
{
	int value=bd->props->brightness;
        int brightness = 190 - value;

	if (bd->props->power != FB_BLANK_UNBLANK) {
		jornada56x_backlight_power(0);
		return 0;
	}
	if (bd->props->fb_blank != FB_BLANK_UNBLANK) {
		jornada56x_backlight_power(0);
		return 0;
	}

	jornada56x_backlight_power(1);

        JORNADA_PWM1_CKDR = 0;
        JORNADA_PWM1_DATA = brightness; /* range is from 0 (brightest) to 255 (darkest) although values > 190 are no good */
        JORNADA_PWM_CTRL = 1; /* enable backlight control - 1=Enable ; 0=Disable */
        return 0;
}


static struct backlight_properties jornada56x_backlight_properties = {
        .owner          = THIS_MODULE,
        .get_brightness = jornada56x_backlight_get_brightness,
        .update_status = jornada56x_backlight_set_brightness,
        .max_brightness = 190,
};


/************************************
 *                                  *
 * Jornada 56x Microwire functions  *
 *                                  *
 ************************************/


int microwire_data[8];


#define ASIC_INT_DELAY  10  /* Poll every 10 milliseconds */
enum {PCR_GET_VALUE=1, PCR_SET_VALUE, PCR_BIT_SET, PCR_BIT_CLEAR};



void SetPMUPCRRegister(long arg_action, long arg_value)
{
	long this_pcr;

	/* Read PCR and remove the unwanted bit 6 */
	this_pcr = (((JORNADA_PCR & 0xff80) >>1) | (JORNADA_PCR & 0x3f));

	this_pcr &= ~(JORNADA_MUX_CLK0 | JORNADA_MUX_CLK1);
	if (this_pcr & JORNADA_SM_CLK_EN)
		this_pcr |= JORNADA_MUX_CLK1;
	else if (this_pcr & JORNADA_MMC_CLK_EN)
		this_pcr |= JORNADA_MUX_CLK0;
	switch (arg_action) {
		case PCR_SET_VALUE:
			this_pcr = arg_value;
			break;
		case PCR_BIT_SET: /* enable controller */
			if(arg_value & JORNADA_SM_CLK_EN) {		/* smart card controller */
				this_pcr &= ~(JORNADA_MUX_CLK0 | JORNADA_MUX_CLK1);
				this_pcr |= JORNADA_MUX_CLK1;
			}
			else if(arg_value & JORNADA_MMC_CLK_EN) {	/* mmc controller */
				this_pcr &= ~(JORNADA_MUX_CLK0 | JORNADA_MUX_CLK1);
				this_pcr |= JORNADA_MUX_CLK0;
			}
			this_pcr |= arg_value;
			break;
		case PCR_BIT_CLEAR:		/* disable controller */
			/* smart card or mmc controller */
			if (arg_value & (JORNADA_SM_CLK_EN | JORNADA_MMC_CLK_EN))
				this_pcr &= ~(JORNADA_MUX_CLK0 | JORNADA_MUX_CLK1);
			this_pcr &= ~arg_value;
			break;
		case PCR_GET_VALUE:
		default:
			break;
	}
	JORNADA_PCR = this_pcr;
}

void jornada_microwire_init(void)
{
	JORNADA_GPIOAFR |= JORNADA_GP_MW;
	udelay(1);
	JORNADA_MWFTR = (8 << JORNADA_MWFTR_V_TFT)
		| (8 << JORNADA_MWFTR_V_RFT); /* 0x108 */
	udelay(1);
  	SetPMUPCRRegister(PCR_BIT_SET, JORNADA_MW_CLK_EN);
	udelay(1);
	JORNADA_MWCR = JORNADA_MW_EN | JORNADA_DSS_16_BIT
		| (8 << JORNADA_MWCR_V_SCR) /* 0xa07 */
		| JORNADA_FIFO_RST; /* reset FIFO */
	udelay(1);
	JORNADA_MWCR &= ~JORNADA_FIFO_RST;
	/* set FIFO interrupt levels to 8 for each of Rx, Tx */
	udelay(1);
	JORNADA_MWFTR = (8 << JORNADA_MWFTR_V_TFT)
		| (8 << JORNADA_MWFTR_V_RFT); /* 0x108 */
}

void jornada_microwire_start(void)
{
int retry = 90000;
	while(retry-- && (JORNADA_MWFSR & JORNADA_MW_RNE)); /* empty out existing data */
}

void jornada_microwire_read(int *value1, int *value2)
{
	int i,retry = 1000000;
	while (retry-- && !(JORNADA_MWFSR & JORNADA_MW_RFS));
	
	for (i = 0; i < 8; i++)
		microwire_data[i] = (JORNADA_MWDR & 0xffff) >> 4;
		
	*value1 = (microwire_data[0] + microwire_data[1] + microwire_data[2] + microwire_data[3]) / 4;
	*value2 = (microwire_data[4] + microwire_data[5] + microwire_data[6] + microwire_data[7]) / 4;
}

/***********************************
 *                                 *
 * Jornada 56x battery functions   *
 *                                 *
 ***********************************/

typedef void (*apm_get_power_status_t)(struct apm_power_info*);
int value;
static void j56x_get_battery_info(void)
{
	int a,b;
	
	if ( !( (GPLR & GPIO_JORNADA56X_TOUCH) == 0)) {
		jornada_microwire_start();
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		JORNADA_MWDR = JORNADA_MW_MAIN_BATTERY;
		jornada_microwire_read(&a,&b);
		value = (a+b) / 2;
	}
}

static void j56x_apm_get_power_status(struct apm_power_info* info)
{
	int battery_life;
	j56x_get_battery_info();
	battery_life = ((value - 1428) * 8) / 19; /* well, sort of accuarate */
	
	if ( value < 100) {
		info->battery_life = -1;
		info->battery_status = 0x04;
		info->ac_line_status = 0x01;
		return;
	}
	info->battery_life = battery_life;

	if (JORNADA_LEDCR & JORNADA_CHARGE_FULL) /* This should check the current battery status */
		info->battery_status = 3 - battery_life / 50;
	else
		info->battery_status = 3;

	if (JORNADA_LEDCR & JORNADA_AC_IN) /* Check if the AC adapter is plugged in or not */
		info->ac_line_status = 0x00;
	else
		info->ac_line_status = 0x01;

	info->time = battery_life * 3;
	info->units = 0;
	
	return;
}

int set_apm_get_power_status( apm_get_power_status_t t)
{
	apm_get_power_status = t;
	return 0;
}

/***********************************
 *                                 *
 * Jornada 56x power management    *
 *                                 *
 ***********************************/
 
static void jornada56x_asic_setup(void)
{
	JORNADA_SCR &= ~JORNADA_ASIC_SLEEP_EN; /* wake up the ASIC */
	udelay(3);
	JORNADA_GPIOAFR = JORNADA_GP_PWM1;
	JORNADA_SCR |= JORNADA_RCLK_EN;
	
	mdelay(1);
	
	SetPMUPCRRegister(PCR_BIT_SET,(JORNADA_GPIO_INT_CLK_EN|JORNADA_PWM1CLK_EN));
	
	JORNADA_PWM1_CKDR = 0;
	JORNADA_PWM2_CKDR = 0;
	
	mdelay(2);

	JORNADA_GPBPSR = 0x001B;
  	JORNADA_GPCPSR = 0x31C0;
  	JORNADA_GPDPSR = 0xAD99;

  	JORNADA_GPBPCR = 0x3E24;
  	JORNADA_GPCPCR = 0x0002;
  	JORNADA_GPDPCR = 0x5264;
	JORNADA_GPDPSR = JORNADA_RS232_ON;	/* enable rs232 transceiver */
	JORNADA_GPDPSR = JORNADA_CF_POWER_OFF;	/* turn power off to CF */

  	JORNADA_GPBPSDR = 0x81FF;
  	JORNADA_GPCPSDR = 0xB96F;
  	JORNADA_GPDPSDR = 0xFBFF;

  	JORNADA_GPBPSLR = 0x00D0;
  	JORNADA_GPCPSLR = 0x0000;
  	JORNADA_GPDPSLR = 0xA198;

  	JORNADA_GPBPFDR = 0x81FF;
  	JORNADA_GPCPFDR = 0xB96F;
  	JORNADA_GPDPFDR = 0xFBFF;

  	JORNADA_GPBPFLR = 0x01D0;
  	JORNADA_GPCPFLR = 0x0000;
  	JORNADA_GPDPFLR = 0xA198;

  	JORNADA_GPBPDR = 0x01FF;
  	JORNADA_GPCPDR = 0x37C2;
  	JORNADA_GPDPDR = 0xFFFF;

	JORNADA_GPBPCR = 0x1c0;		/* Buttons: row lines all output 0 */
	JORNADA_GPBPDR |= 0x1c0;	/* Buttons: turn row lines into outputs */

	mdelay(2);	/* Delay 2 milliseconds. */
	JORNADA_INT_EN |= JORNADA_GPIO_B_INT | JORNADA_GPIO_C_INT;
	
	jornada_microwire_init();
	
	/* Clear screen here */
	JORNADA_GPDPCR = GPIO_GPIO5;
	JORNADA_GPDPCR = GPIO_GPIO13;   /* Turn on power to panel */
	JORNADA_GPDPSR = GPIO_GPIO14; /* force GPIO-D14 to high */
	mdelay(100);
}

static int jornada56x_probe(struct device *dev)
{
	GPCR = 0x0f424000;
	GPSR = 0x00100000;
	GAFR = 0x080803fc;
	GPDR = 0x0f5243fc;
	GRER = 0x00448C00;
	GFER = 0x00448800;
	
	TUCR = 0xA0000000;
	
	jornada56x_asic_setup(); /* initialize default ASIC configuration */
	
	JORNADA_GPDPSR = JORNADA_BACKLIGHT; /* Turn off the front light */
	
	return 0;
}

struct pm_save_data {
	int gpiob_re_en;
	int gpiob_fe_en;
	int gpioc_re_en;
	int gpioc_fe_en;
};

static int jornada56x_suspend(struct device *dev, pm_message_t state)
{
	struct pm_save_data *save;
	
	save = kmalloc(sizeof(struct pm_save_data), GFP_KERNEL);
	if (!save)
		return -ENOMEM;
	dev->power.saved_state = save;
	
	/* save interrupt states */
	disable_irq(GPIO_JORNADA56X_POWER_SWITCH_IRQ);
	save->gpiob_re_en = JORNADA_GPIOB_RE_EN;
	save->gpiob_fe_en = JORNADA_GPIOB_FE_EN;
	save->gpioc_re_en = JORNADA_GPIOC_RE_EN;
	save->gpioc_fe_en = JORNADA_GPIOC_FE_EN;
	
	/* disable all ASIC interrupts */
	JORNADA_INT_EN = 0;
	JORNADA_INT_STAT = 0xffff;
	JORNADA_INT_EN2 = 0;
	JORNADA_INT_STAT2 = 0xffff;
	JORNADA_GPIOB_FE_EN = 0;
	JORNADA_GPIOB_RE_EN = 0;
	JORNADA_GPIOB_STAT = 0xffff;
	JORNADA_GPIOC_FE_EN = 0;
	JORNADA_GPIOC_RE_EN = 0;
	JORNADA_GPIOC_STAT = 0xffff;
	
	JORNADA_GPDPCR = GPIO_GPIO14; /* clear the screen here */
	
	JORNADA_PCR = 0; /* disable everything */
	JORNADA_SCR = 0; /* disable everything */
	JORNADA_SCR |= JORNADA_ASIC_SLEEP_EN; /* put the system ASIC into zZzZz */
	mdelay(100);
	return 0;
}

static int jornada56x_resume(struct device *dev)
{
	struct pm_save_data *save;
	
	save = (struct pm_save_data *)dev->power.saved_state;
	if (!save)
		return 0;
	mdelay(100);
	jornada56x_asic_setup(); /* restore default ASIC configuration which gets lost during zZz */
	
	/* restore saved ASIC irq configuration */
	JORNADA_GPIOB_RE_EN = save->gpiob_re_en;
	JORNADA_GPIOB_FE_EN = save->gpiob_fe_en;
	JORNADA_GPIOC_RE_EN = save->gpioc_re_en;
	JORNADA_GPIOC_FE_EN = save->gpioc_fe_en;
	
	enable_irq(GPIO_JORNADA56X_POWER_SWITCH_IRQ);
	return 0;
}

static struct device_driver jornada56x_driver = {
	.name		= "jornada56x",
	.bus		= &platform_bus_type,
	.probe		= jornada56x_probe,
	.suspend	= jornada56x_suspend,
	.resume		= jornada56x_resume,
};

/***********************************
 *                                 *
 * Jornada 56x system flash        *
 *                                 *
 ***********************************/

static struct mtd_partition jornada56x_partitions[] = {
	{
		.name		= "bootldr",
		.size		= 0x00040000,
		.offset		= 0,
		.mask_flags	= MTD_WRITEABLE,
	}, {
		.name		= "rootfs",
		.size		= MTDPART_SIZ_FULL,
		.offset		= MTDPART_OFS_APPEND,
	}
};

static void jornada56x_set_vpp(int vpp)
{
	if (vpp)
		GPSR = GPIO_GPIO26;
	else
		GPCR = GPIO_GPIO26;
	GPDR |= GPIO_GPIO26;
};

static struct flash_platform_data jornada56x_flash_data = {
	.map_name	= "cfi_probe",
	.parts		= jornada56x_partitions,
	.nr_parts	= ARRAY_SIZE(jornada56x_partitions),
	.set_vpp	= jornada56x_set_vpp,
};

static struct resource jornada56x_flash_resource = {
	.start	= SA1100_CS0_PHYS,
	.end	= SA1100_CS0_PHYS + SZ_32M - 1,
	.flags	= IORESOURCE_MEM,
};

/***********************************
 *                                 *
 * Jornada 56x system functions    *
 *                                 *
 ***********************************/
 
static struct platform_device jornada56x_device = {
	.name		= "jornada56x",
	.id		= 0,
};

static struct platform_device *devices[] __initdata = {
	&jornada56x_device,
};
 
static void irq_ack_low(unsigned int irq)
{
	JORNADA_INT_STAT = (1 << (irq - IRQ_JORNADA_MMC));
}

static void irq_mask_low(unsigned int irq)
{
	JORNADA_INT_EN &= ~(1 << (irq - IRQ_JORNADA_MMC));
}

static void irq_unmask_low(unsigned int irq)
{
	JORNADA_INT_EN |= (1 << (irq - IRQ_JORNADA_MMC));
}

static struct irq_chip irq_low = {
	.ack	= irq_ack_low,
	.mask	= irq_mask_low,
	.unmask = irq_unmask_low,
};

static void irq_ack_high(unsigned int irq)
{
	JORNADA_INT_STAT2 = (1 << (irq - IRQ_JORNADA_UART));
}

static void irq_mask_high(unsigned int irq)
{
	JORNADA_INT_EN2 &= ~(1 << (irq - IRQ_JORNADA_UART));
}

static void irq_unmask_high(unsigned int irq)
{
	JORNADA_INT_EN2 |= (1 << (irq - IRQ_JORNADA_UART));
}

static struct irq_chip irq_high = {
	.ack	= irq_ack_high,
	.mask	= irq_mask_high,
	.unmask = irq_unmask_high,
};

static void irq_ack_gpiob(unsigned int irq)
{
	JORNADA_GPIOB_STAT = (1 << (irq - IRQ_JORNADA_GPIO_B0));
}

static void irq_mask_gpiob(unsigned int irq)
{
	JORNADA_GPIOB_FE_EN &= ~(1 << (irq - IRQ_JORNADA_GPIO_B0));
}

static void irq_unmask_gpiob(unsigned int irq)
{
	JORNADA_GPIOB_FE_EN |= (1 << (irq - IRQ_JORNADA_GPIO_B0));
}

static struct irq_chip irq_gpiob = {
	.ack	= irq_ack_gpiob,
	.mask	= irq_mask_gpiob,
	.unmask = irq_unmask_gpiob,
};

static void irq_ack_gpioc(unsigned int irq)
{
	JORNADA_GPIOC_STAT = (1 << (irq - IRQ_JORNADA_GPIO_C0));
}

static void irq_mask_gpioc(unsigned int irq)
{
	JORNADA_GPIOC_FE_EN &= ~(1 << (irq - IRQ_JORNADA_GPIO_C0));
}

static void irq_unmask_gpioc(unsigned int irq)
{
	JORNADA_GPIOC_FE_EN |= (1 << (irq - IRQ_JORNADA_GPIO_C0));
}

static struct irq_chip irq_gpioc = {
	.ack	= irq_ack_gpioc,
	.mask	= irq_mask_gpioc,
	.unmask = irq_unmask_gpioc,
};

extern asmlinkage void asm_do_IRQ(int irq);

static void __init jornada56x_init_irq(void)
{
	unsigned int irq;
	
	sa1100_init_irq();
	
	alloc_irq_space(64);
	
	JORNADA_INT_EN = 0;
	JORNADA_INT_STAT = 0xffff;
	JORNADA_INT_EN2 = 0;
	JORNADA_INT_STAT2 = 0xffff;
	JORNADA_GPIOB_FE_EN = 0;
	JORNADA_GPIOB_RE_EN = 0;
	JORNADA_GPIOB_STAT = 0xffff;
	JORNADA_GPIOC_FE_EN = 0;
	JORNADA_GPIOC_RE_EN = 0;
	JORNADA_GPIOC_STAT = 0xffff;
	
	for (irq = IRQ_JORNADA_MMC; irq <= IRQ_JORNADA_MMC + 15; irq++) {
		set_irq_chip(irq, &irq_low);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	
	for (irq = IRQ_JORNADA_UART; irq <= IRQ_JORNADA_UART + 15; irq++) {
		set_irq_chip(irq, &irq_high);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	
	for (irq = IRQ_JORNADA_GPIO_B0; irq <= IRQ_JORNADA_GPIO_B0 + 15; irq++) {
		set_irq_chip(irq, &irq_gpiob);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	
	for (irq = IRQ_JORNADA_GPIO_C0; irq <= IRQ_JORNADA_GPIO_C0 + 15; irq++) {
		set_irq_chip(irq, &irq_gpioc);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
}


static irqreturn_t jornada56x_IRQ_demux(int irq, void *dev_id)
{
unsigned long stat0, stat1, statb, statc;
int i, found_one;

	do {
		found_one = 0;
		stat0 = JORNADA_INT_STAT & 0xffff & JORNADA_INT_EN
			& ~(JORNADA_GPIO_B_INT | JORNADA_GPIO_C_INT);
		stat1 = JORNADA_INT_STAT2 & 0xffff & JORNADA_INT_EN2;
		statb = JORNADA_GPIOB_STAT & 0xffff & JORNADA_GPIOB_FE_EN;
		statc = JORNADA_GPIOC_STAT & 0xffff & JORNADA_GPIOC_FE_EN;
#define CHECK_STAT(A,B) \
		for (i = A; B; i++, B >>= 1) \
			if (B & 1) { \
				found_one = 1; \
				asm_do_IRQ(i); \
			}
		CHECK_STAT(IRQ_JORNADA_MMC, stat0);
		CHECK_STAT(IRQ_JORNADA_UART, stat1);
		CHECK_STAT(IRQ_JORNADA_GPIO_B0, statb);
		CHECK_STAT(IRQ_JORNADA_GPIO_C0, statc);
	} while(found_one);
	
	return IRQ_HANDLED;
}

static void jornada56x_init(void)
{
	platform_add_devices(devices, ARRAY_SIZE(devices));
	driver_register(&jornada56x_driver); /* initialize default GPIO states */
	
	if (request_irq(GPIO_JORNADA56X_ASIC_IRQ, jornada56x_IRQ_demux,
                IRQF_DISABLED, "Jornada56x ASIC", NULL)) {
		printk("%s(): request_irq failed\n", __FUNCTION__);
                return;
        }
	set_irq_type(GPIO_JORNADA56X_ASIC_IRQ, IRQT_BOTHEDGE);
	
	/* initialise backlight and lcd */
	sa1100_backlight_device         = backlight_device_register("sa1100fb", 0, &jornada56x_backlight_properties);
	sa1100_lcd_device		= lcd_device_register("sa1100fb", 0, &jornada56x_lcd_ops);
	sa1100fb_lcd_power		= jornada56x_lcd_power;
	
	set_apm_get_power_status(j56x_apm_get_power_status); /* apm handler */
	sa11x0_set_flash_data(&jornada56x_flash_data, &jornada56x_flash_resource, 1); /* set flash data */
}

static struct map_desc jornada56x_io_desc[] __initdata = {
	{ 
		.virtual 	= 0xf0000000, /* ASIC */
		.pfn		= __phys_to_pfn(0x40000000),
		.length		= 0x00100000,
		.type		= MT_DEVICE 
	} 
};

static void __init jornada56x_map_io(void)
{
	sa1100_map_io();
	iotable_init(jornada56x_io_desc, ARRAY_SIZE(jornada56x_io_desc));

        sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
	
	/* configure suspend conditions */
	PGSR = 0;
	PWER = 0;
	PCFR = 0;
	PSDR = 0;
	
	PGSR |= 0x0f5243fc; /* preserve vital GPIO */
	PWER |= GPIO_JORNADA56X_POWER_SWITCH | PWER_RTC;
	PCFR |= PCFR_OPDE;
}

MACHINE_START(JORNADA56X, "HP Jornada 56x")
	/* Maintainer: Alex Lange - chicken@handhelds.org */
	.phys_io	= 0x80000000,
	.io_pg_offst	= ((0xf8000000) >> 18) & 0xfffc,
	.boot_params	= 0xc0000100,
	.map_io		= jornada56x_map_io,
	.init_irq	= jornada56x_init_irq,
	.timer		= &sa1100_timer,
	.init_machine	= jornada56x_init,
MACHINE_END
