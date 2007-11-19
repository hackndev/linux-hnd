/*
 * Synaptics NavPoint(tm) support in the hx470x.
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * 10-Feb-2005          Todd Blumer <todd@sdgsystems.com>
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/navpoint.h>
#include <linux/input.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/ssp.h>

/*
 * The iPAQ hx4700 uses several subsystems for input: PXA GPIOs for 2 keys, 
 * ASIC3 GPIOs for 3 other keys, NavPoint(tm) for input. We'll have each of
 * these tie into the kernel input system. That seems to be the most straight-
 * forward thing to do.
 *
 * Components that make up this driver: PXA SSP1 (SPI), input subsystem.
 */

#define HX4700_NAVPT_SSP_PORT		1
#define HX4700_NAVPT_SSP_TIMEOUT	0x17777		/* from WinCE */

/*
 * must be power of 2
 */
#define SSP_QSIZE		512

static struct ssp_dev ssp_dev;
static struct navpoint_dev navpt_dev;
static struct completion thread_comp;
static volatile int must_die;

static struct hx4700_navpoint {
	struct task_struct *kthread;
	wait_queue_head_t kthread_wait;
	volatile unsigned int read_ndx;
	volatile unsigned int write_ndx;
	u_int8_t dataq[SSP_QSIZE];
} hx4700_np;

#define SSP_QMASK		(SSP_QSIZE-1)

static void
rx_qinsert( u_int8_t octet )
{
	unsigned int nextw = (hx4700_np.write_ndx+1) & SSP_QMASK;
	if (nextw == hx4700_np.read_ndx) {
		printk( KERN_ERR "hx4700 navpoint queue full\n" );
	}
	else {
		hx4700_np.dataq[nextw] = octet;
		hx4700_np.write_ndx = nextw;
	}
}

static void
rx_threshold_irq( void *priv, int count )
{
	int i;
	u32 word;

	for (i=0; i<count; i++) {
		if (ssp_read_word( &ssp_dev, &word )) 
			return;
		/*
		 * ssp mode is 16-bit words, extract and send to
		 * navpoint driver
		 */
		rx_qinsert( (u_int8_t)((word >> 8) & 0xff) );
		rx_qinsert( (u_int8_t)word );
	}
	wake_up_interruptible( &hx4700_np.kthread_wait );
}

int
hx4700_navptd( void *notused )
{
	printk( "hx4700_navptd: receive thread started.\n" );
	daemonize( "hx4700_navptd" );
	while (!kthread_should_stop()) {
		if (must_die)
			break;

		interruptible_sleep_on(&hx4700_np.kthread_wait);
		/*wait_event_interruptible( hx4700_np.kthread_wait,
			kthread_should_stop()
			|| hx4700_np.read_ndx != hx4700_np.write_ndx );*/

		try_to_freeze();

		if (must_die)
			break;
		while (hx4700_np.read_ndx != hx4700_np.write_ndx) {
			u_int8_t octet = hx4700_np.dataq[hx4700_np.read_ndx++];
			hx4700_np.read_ndx &= SSP_QMASK;
			navpoint_rx( &navpt_dev, octet, hx4700_np.write_ndx - hx4700_np.read_ndx);
		}
	}
	complete_and_exit( &thread_comp, 0 );
}

static int
hx4700_navpt_write( u_int8_t *data, int count )
{
	int odd, i;
	u_int32_t word;

	/*
	 * SPI bus is in 16-bit word mode. If odd, we must still
	 * shift in a zero byte to fill the word
	 */
	odd = count & 1;
	for (i=0; i<count-odd; i+=2) {
		word = (u_int32_t)data[i] << 8 | data[i+1];
		// printk( "ssp write: 0x%x\n", word );
		(void) ssp_write_word( &ssp_dev, word );
	}
	if (odd) {
		word = (u_int32_t)data[count-1] << 8;
		// printk( "ssp write odd: 0x%x\n", word );
		(void) ssp_write_word( &ssp_dev, word );
	}
	return 0;
}

static struct ssp_ops hx4700_ssp_ops = {
	.rx_thresh = rx_threshold_irq,
	.priv = &ssp_dev,
};

// module_param(qsize, uint, 1024);

static void
hx4700_ssp_init( void )
{
	ssp_dev.ops = &hx4700_ssp_ops;
	ssp_init( &ssp_dev, HX4700_NAVPT_SSP_PORT, 0 );
	ssp_config( &ssp_dev, SSCR0_Motorola | SSCR0_DataSize(16),
		SSCR1_SCFR | SSCR1_SCLKDIR | SSCR1_SFRMDIR
			| SSCR1_RWOT | SSCR1_TINTE | SSCR1_PINTE
			| SSCR1_SPH | SSCR1_RIE
			| SSCR1_TxTresh(1) | SSCR1_RxTresh(1),
		0, SSCR0_SerClkDiv(2) );
	SSTO_P(HX4700_NAVPT_SSP_PORT) = HX4700_NAVPT_SSP_TIMEOUT;
}

static int
hx4700_navpt_probe( struct platform_device *pdev )
{
	pxa_gpio_mode( GPIO_NR_HX4700_SYNAPTICS_SPI_CLK_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_SYNAPTICS_SPI_CS_N_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_SYNAPTICS_SPI_DO_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_SYNAPTICS_SPI_DI_MD );

	init_waitqueue_head( &hx4700_np.kthread_wait );

	hx4700_ssp_init();
	SET_HX4700_GPIO( SYNAPTICS_POWER_ON, 1 );
	/*
	 * start receiver thread for navpoint data
	 */
        must_die = 0;
	init_completion( &thread_comp );
	hx4700_np.kthread = kthread_run( &hx4700_navptd, NULL, "hx4700_navpt" );
	if (IS_ERR(hx4700_np.kthread)) {
		printk( KERN_ERR "iPAQ hx470x NavPoint start failure.\n" );
		return -EIO;
	}
	navpt_dev.unit = 0;
	navpt_dev.tx_func = hx4700_navpt_write;
	navpoint_init( &navpt_dev );
	ssp_enable( &ssp_dev );
	// navpoint_reset( &navpt_dev );
	return 0;
}

#ifdef CONFIG_PM
static int
hx4700_navpt_suspend( struct platform_device *pdev, pm_message_t state )
{
	SET_HX4700_GPIO( SYNAPTICS_POWER_ON, 0 );
	ssp_exit( &ssp_dev );
	return 0;
}

static int
hx4700_navpt_resume( struct platform_device *pdev )
{
	hx4700_ssp_init();
	SET_HX4700_GPIO( SYNAPTICS_POWER_ON, 1 );
	ssp_enable( &ssp_dev );
	navpoint_reset( &navpt_dev );
	return 0;
}
#else
#define hx4700_navpt_resume	NULL
#define hx4700_navpt_suspend	NULL
#endif

static int
hx4700_navpt_remove( struct platform_device *pdev )
{
	navpoint_exit( &navpt_dev );
	ssp_disable( &ssp_dev );
	SET_HX4700_GPIO( SYNAPTICS_POWER_ON, 0 );
        must_die = 1;
	wake_up_interruptible( &hx4700_np.kthread_wait );
	wait_for_completion( &thread_comp );
	ssp_exit( &ssp_dev );
	return 0;
}

static struct platform_driver hx4700_navpt_driver = {
	.driver = {
		.name     = "hx4700-navpoint",
	},
	.probe    = hx4700_navpt_probe,
	.suspend  = hx4700_navpt_suspend,
	.resume   = hx4700_navpt_resume,
	.remove   = hx4700_navpt_remove,
};


static int __init
hx4700_navpt_init( void )
{
	printk( "hx4700 NavPoint Driver\n" );
	return platform_driver_register( &hx4700_navpt_driver );
}


static void __exit
hx4700_navpt_exit( void )
{
	platform_driver_unregister( &hx4700_navpt_driver );
}


module_init( hx4700_navpt_init );
module_exit( hx4700_navpt_exit );

MODULE_AUTHOR( "Todd Blumer <todd@sdgsystems.com>" );
MODULE_DESCRIPTION( "PXA27x NavPoint(tm) driver for iPAQ hx470x" );
MODULE_LICENSE( "GPL" );

/* vim600: set noexpandtab sw=8 ts=8 :*/

