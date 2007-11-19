/*
 * Driver interface to the sleeve portion of the ASIC Companion chip on
 * the iPAQ H5400
 *
 * Copyright 2005 Erik Hovland
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/ipaq-sleeve.h>
#include <asm/hardware/ipaq-ops.h>
#include <asm/hardware/ipaq-samcop.h>
#include <linux/mfd/samcop_base.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach-types.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/h5400-gpio.h>
#include <asm/arch/h5400-asic.h>


#define SPI_READ_CMD 3

#define SAMCOP_PCMCIA_OPTION_DETECTED	1

struct eps_data {
	int			irq_base;
	struct device		*parent;
	void			*map;
	struct semaphore	lock;
};

#define SSCR0_DSS_8BIT  7
#define SSCR0_DSS_16BIT 15

#define SSCR0_SCR_SHIFT 8
#define SSCR0_SCR_WIDTH 8

#define SSCR1_SPO_RISING (0 << 3)
#define SSCR1_SPO_FALLING (1 << 3)
/*
 * Helper pcmcia sleeve register read and write access
 */

static inline void
samcop_eps_write_register (struct eps_data *eps, u32 reg, u16 val)
{
	__raw_writew (val, reg + eps->map);
}

static inline u16
samcop_eps_read_register (struct eps_data *eps, u32 reg)
{
	return __raw_readw (reg + eps->map);
}

/*
 * ipaq_sleeve_ops functions
 */

static void 
spi_set_cs(int cs)
{
	int count = 0;

	while (SSSR & SSSR_BSY) {
		if (count++ > 10000) {
			printk("spi_set_cs: timeout SSSR=%x\n", SSSR);
			break;
		}
	}

	SET_H5400_GPIO (OPT_SPI_CS_N, cs);
}

static int 
spi_transfer_byte(int dataout)
{
	int datain;
	int count = 0;
	/* write to fifo */
	SSDR = dataout;
	while (!(SSSR & SSSR_RNE)) {
		/* wait for data to arrive */
		if (count++ > 10000) {
			printk("spi_transfer_byte: timeout SSSR=%x\n", SSSR);
			break;
		}
	}
	datain = SSDR;
#ifdef DEBUG_SPI
	printk("spi_transfer_byte: dataout=%x datain=%x\n", dataout, datain);
#endif
	return datain;
}

static int
samcop_eps_spi_read (void *dev, unsigned short address, unsigned char *data, unsigned short len)
{
	struct eps_data *eps = dev;
	int i = 0;
	unsigned long flags;

	if (!data)
		return -EINVAL;

	if (down_interruptible (&eps->lock))
		return -ERESTARTSYS;

	local_irq_save (flags);
	CKEN |= CKEN3_SSP;
	local_irq_restore (flags);

	spi_set_cs(0);
	spi_transfer_byte(SPI_READ_CMD);
	spi_transfer_byte((address >> 0) & 0xFF);
	for (i = 0; i < len; i++)
		data[i] = spi_transfer_byte( 0 );
	spi_set_cs(1);

	/* disable SSP clock */
	local_irq_save (flags);
	CKEN &= ~CKEN3_SSP;
	local_irq_restore (flags);

	up (&eps->lock);

	return len;
}

static int
samcop_eps_read_egpio (void *dev, int nr)
{
	struct eps_data *eps = dev;
	switch (nr) {
	case IPAQ_EGPIO_PCMCIA_CD0_N:
		return samcop_eps_read_register (eps, _SAMCOP_PCMCIA_EPS) & SAMCOP_PCMCIA_EPS_CD0_N;
	case IPAQ_EGPIO_PCMCIA_CD1_N:
		return samcop_eps_read_register (eps, _SAMCOP_PCMCIA_EPS) & SAMCOP_PCMCIA_EPS_CD1_N;
	case IPAQ_EGPIO_PCMCIA_IRQ0:
		return samcop_eps_read_register (eps, _SAMCOP_PCMCIA_EPS) & SAMCOP_PCMCIA_EPS_IRQ0_N;
	case IPAQ_EGPIO_PCMCIA_IRQ1:
		return samcop_eps_read_register (eps, _SAMCOP_PCMCIA_EPS) & SAMCOP_PCMCIA_EPS_IRQ1_N;
	default:
		printk("%s:%d: unknown ipaq_egpio_type=%d\n", __FUNCTION__, __LINE__, nr);
		return 0;
	}
}

struct egpio_irq_info {
	int egpio_nr;
	int irq;
};

static const struct egpio_irq_info h5400_egpio_irq_info[] = {
	{ IPAQ_EGPIO_PCMCIA_CD0_N, _IRQ_SAMCOP_EPS_CD0 }, 
	{ IPAQ_EGPIO_PCMCIA_CD1_N, _IRQ_SAMCOP_EPS_CD1 },
	{ IPAQ_EGPIO_PCMCIA_IRQ0,  _IRQ_SAMCOP_EPS_IRQ0 },
	{ IPAQ_EGPIO_PCMCIA_IRQ1,  _IRQ_SAMCOP_EPS_IRQ1 },
	{ 0, 0 }
}; 

static int
samcop_eps_egpio_irq (void *dev, int egpio_nr)
{
	const struct egpio_irq_info *info = NULL;
	struct eps_data *eps = dev;

	if (machine_is_h5400())
		info = h5400_egpio_irq_info;

	if (!info) {
		printk("%s: no irq_info, machine is not a h5400\n", __FUNCTION__);
		return -EINVAL;
	}

	while (info->egpio_nr != 0) {
		if (info->egpio_nr == egpio_nr) {
			return info->irq + eps->irq_base;
		}
		info++;
	}

	printk("%s: unhandled egpio_nr=%d\n", __FUNCTION__, egpio_nr);
	return -EINVAL;
}

static int
samcop_eps_control_egpio (void *dev, int egpio, int state)
{
	struct eps_data *eps = dev;

	switch (egpio) {
	case IPAQ_EGPIO_OPT_NVRAM_ON:
		SET_H5400_GPIO (OPT_NVRAM, state);
		break;
	case IPAQ_EGPIO_OPT_ON:
		samcop_set_gpio_a (eps->parent, SAMCOP_GPIO_GPA_OPT_ON_N, state ? 0 : SAMCOP_GPIO_GPA_OPT_ON_N);
		break;
	case IPAQ_EGPIO_CARD_RESET:
		if (state)
		    samcop_eps_write_register (eps, _SAMCOP_PCMCIA_CC, SAMCOP_PCMCIA_CC_RESET);
		else
		    samcop_eps_write_register (eps, _SAMCOP_PCMCIA_CC, 0);
		break;
	case IPAQ_EGPIO_OPT_RESET:
		samcop_set_gpio_a (eps->parent, SAMCOP_GPIO_GPA_OPT_RESET, state ? SAMCOP_GPIO_GPA_OPT_RESET : 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
			
static int
samcop_eps_get_option_detect (void *dev, int *result )
{
	struct eps_data *eps = dev;
	u16 val = 0; 

	val = samcop_eps_read_register (eps, _SAMCOP_PCMCIA_EPS);
	val &= (SAMCOP_PCMCIA_EPS_ODET0_N | SAMCOP_PCMCIA_EPS_ODET1_N);

	if (val == 0 && result) {
		*result = SAMCOP_PCMCIA_OPTION_DETECTED;
	} else if (result) {
		*result = !SAMCOP_PCMCIA_OPTION_DETECTED;
	}
	return 0;
}

static struct ipaq_sleeve_ops samcop_sleeve_ops = {
	.set_ebat = NULL,
	.get_option_detect = samcop_eps_get_option_detect,
	.spi_read = samcop_eps_spi_read,
	.spi_write = NULL,
	.get_version = NULL,
	.control_egpio = samcop_eps_control_egpio,
	.read_egpio = samcop_eps_read_egpio,
	.egpio_irq = samcop_eps_egpio_irq,
};

static int
samcop_eps_suspend (struct platform_device *dev, pm_message_t state)
{
	struct eps_data *eps = platform_get_drvdata(dev);

	down (&eps->lock);

	return 0;
}

static int
samcop_eps_resume (struct platform_device *dev)
{
	struct eps_data *eps = platform_get_drvdata (dev);

	up (&eps->lock);

	return 0;
}

static int
samcop_eps_probe (struct platform_device *dev)
{
	int result = 0;
	struct eps_data *eps;
	//struct platform_device *dev = dev;
	unsigned long flags;

	eps = kmalloc (sizeof (*eps), GFP_KERNEL);
	if (!eps)
		return -ENOMEM;

	init_MUTEX (&eps->lock);

	eps->parent = dev->dev.parent;

	eps->map = ioremap ((unsigned long)dev->resource[0].start, (unsigned long)dev->resource[0].end - (unsigned long)dev->resource[0].start + 1);
	eps->irq_base = dev->resource[1].start;

	platform_set_drvdata(dev, eps);

	SET_H5400_GPIO (OPT_SPI_CLK, 0);
	SET_H5400_GPIO (OPT_SPI_DOUT, 0);
	SET_H5400_GPIO (OPT_SPI_DIN, 0);
	SET_H5400_GPIO (OPT_SPI_CS_N, 1);

	/* set modes and directions */
	pxa_gpio_mode (GPIO23_SCLK_MD);
	pxa_gpio_mode (GPIO24_SFRM | GPIO_MD_MASK_DIR | GPIO_DFLT_HIGH);
	pxa_gpio_mode (GPIO25_STXD_MD);
	pxa_gpio_mode (GPIO26_SRXD_MD);
	pxa_gpio_mode (GPIO27_SEXTCLK);

	local_irq_save (flags);

	/* enable clock to the SSP */ 
	CKEN |= CKEN3_SSP;

	/* set up control regs */
	SSCR0 = 0;
	SSCR1 = 0;
	
	SSCR0 = (SSCR0_DSS_8BIT | SSCR0_Motorola | (255 << SSCR0_SCR_SHIFT));
	SSCR1 = (SSCR1_SPO_RISING | SSCR1_SPH /* early */);
	
	SSCR0 |= SSCR0_SSE;

	/* disable SSP clock until required */
	CKEN &= ~CKEN3_SSP;

	local_irq_restore (flags);

	result = ipaq_sleeve_register (&samcop_sleeve_ops, eps);
	if (result) {
		printk ("samcop_sleeve: unable to start up sleeve driver, error %d\n",
			result);
		kfree (eps);
		return result;
	}

	result = request_irq (eps->irq_base + _IRQ_SAMCOP_EPS_ODET,
			      ipaq_sleeve_presence_interrupt,
			      SA_INTERRUPT | SA_SAMPLE_RANDOM,
			      "sleeve detect", NULL);

	if (result) {
		printk ("samcop_sleeve: unable to claim sleeve presence irq, error %d\n",
			result);
		ipaq_sleeve_unregister ();
		kfree (eps);
	}

	return result;
}

static int
samcop_eps_cleanup (struct platform_device *dev)
{
	struct eps_data *eps = platform_get_drvdata (dev);
	free_irq (eps->irq_base + _IRQ_SAMCOP_EPS_ODET, NULL);
	ipaq_sleeve_unregister ();
	iounmap (eps->map);
	kfree (eps);

	return 0;
}

//static platform_device_id samcop_eps_device_ids[] = { { IPAQ_SAMCOP_EPS_DEVICE_ID }, { 0 } };

static struct platform_driver
samcop_eps_device_driver = {
	.driver = {
		.name    = "samcop sleeve",
	},
	.probe   = samcop_eps_probe,
	.remove  = samcop_eps_cleanup,
#ifdef CONFIG_PM
	.suspend = samcop_eps_suspend,
	.resume  = samcop_eps_resume
#endif
};

// go over h5400_eps_init to make sure nothing is missed
static int __init
samcop_eps_init (void)
{
	return platform_driver_register (&samcop_eps_device_driver);
}

static void __exit
samcop_eps_exit (void)
{
	platform_driver_unregister (&samcop_eps_device_driver);
}

module_init (samcop_eps_init);
module_exit (samcop_eps_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erik Hovland <erik@hovland.org>");
MODULE_DESCRIPTION("Sleeve driver for iPAQ SAMCOP");
//MODULE_DEVICE_TABLE(soc, samcop_eps_device_ids);

