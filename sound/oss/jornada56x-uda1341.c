/* Jornada 56x glue audio driver
** (C) Alex Lange - 2005, 2006
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/l3/l3.h>
#include <linux/l3/uda1341.h>

#include <asm/semaphore.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/arch/jornada56x.h>

#include "sa1100-audio.h"

void SetPMUPCRRegister(long arg_action, long arg_value);
enum {PCR_GET_VALUE=1, PCR_SET_VALUE, PCR_BIT_SET, PCR_BIT_CLEAR};


/*
 * L3 bus driver
 */
static void l3_jornada56x_send_data(struct l3_msg *msg)
{
	int addr = msg->addr;
	int len = msg->len;
	char *data = msg->buf;
	int i = 100;

	while ((len--) > 1) {
		JORNADA_L3CAR = addr;
		mdelay(100);
		while ((!(JORNADA_L3CFR & JORNADA_L3_ADDR_DONE))&&(i--));

		JORNADA_L3CDW = *data++;
		mdelay(100);
		i=100;
		while ((!(JORNADA_L3CFR & JORNADA_L3_DATA_DONE))&&(i--));
	}
	printk("l3_jornada56x_send_data() done!");
}

static int l3_jornada56x_xfer(struct l3_adapter *adap, struct l3_msg msgs[], int num)
{
	int i;

	for (i = 0; i < num; i++) {
		struct l3_msg *pmsg = &msgs[i];

		if (pmsg->flags & L3_M_RD)
			return -1; /* data read is not supported */
		else
			l3_jornada56x_send_data(pmsg);
	}

	return num;
}

static struct l3_algorithm l3_jornada56x_algo = {
	.name	= "L3 Jornada 56x algorithm",
	.xfer	= l3_jornada56x_xfer,
};

static DECLARE_MUTEX(jornada56x_lock);

static struct l3_adapter l3_jornada56x_adapter = {
	.owner	= THIS_MODULE,
	.name	= "l3-jornada56x",
	.algo	= &l3_jornada56x_algo,
	.lock	= &jornada56x_lock,
};

/*
 * Definitions
 */
#define AUDIO_RATE_DEFAULT	44100

/*
 * Mixer interface
 */
static struct uda1341 *uda1341;

static int
mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	return uda1341_mixer_ctl(uda1341, cmd, (void *)arg);
}

static struct file_operations uda1341_mixer_fops = {
	.owner	= THIS_MODULE,
	.ioctl	= mixer_ioctl,
};


/*
 * Audio interface
 */

static void jornada56x_set_audio_clock(long val)
{
	/* the Jornada 56x uses a ICS548-05A audio clock connected to GPIO-B[0-3] so we get the following frequencies: */
	switch (val) {
		case 8000: /* 2.048 MHz */
			JORNADA_GPBPSR = GPIO_GPIO0;
			JORNADA_GPBPSR = GPIO_GPIO1;
			JORNADA_GPBPSR = GPIO_GPIO2;
			JORNADA_GPBPSR = GPIO_GPIO3;
			break;
		case 10666: case 10985: /* 2.8224 MHz */
			JORNADA_GPBPCR = GPIO_GPIO0;
			JORNADA_GPBPSR = GPIO_GPIO1;
			JORNADA_GPBPCR = GPIO_GPIO2;
			JORNADA_GPBPCR = GPIO_GPIO3;
			break;
		case 14647: case 16000: /* 4.096 MHz */
			JORNADA_GPBPCR = GPIO_GPIO0;
			JORNADA_GPBPCR = GPIO_GPIO1;
			JORNADA_GPBPCR = GPIO_GPIO2;
			JORNADA_GPBPSR = GPIO_GPIO3;
			break;
		case 21970: case 22050: /* 5.6448 MHz */
			JORNADA_GPBPSR = GPIO_GPIO0;
			JORNADA_GPBPCR = GPIO_GPIO1;
			JORNADA_GPBPCR = GPIO_GPIO2;
			JORNADA_GPBPSR = GPIO_GPIO3;
			break;
		case 24000: /* 6.144 MHz */
			JORNADA_GPBPCR = GPIO_GPIO0;
			JORNADA_GPBPSR = GPIO_GPIO1;
			JORNADA_GPBPCR = GPIO_GPIO2;
			JORNADA_GPBPSR = GPIO_GPIO3;
			break;
		case 29400: case 32000: /* 8.192 MHz */
			JORNADA_GPBPCR = GPIO_GPIO0;
			JORNADA_GPBPCR = GPIO_GPIO1;
			JORNADA_GPBPSR = GPIO_GPIO2;
			JORNADA_GPBPSR = GPIO_GPIO3;
			break;
		case 44100: /* 11.2896 MHz */
			JORNADA_GPBPSR = GPIO_GPIO0;
			JORNADA_GPBPCR = GPIO_GPIO1;
			JORNADA_GPBPSR = GPIO_GPIO2;
			JORNADA_GPBPSR = GPIO_GPIO3;
			break;
		case 48000: /* 12.288 MHz */
			JORNADA_GPBPCR = GPIO_GPIO0;
			JORNADA_GPBPSR = GPIO_GPIO1;
			JORNADA_GPBPSR = GPIO_GPIO2;
			JORNADA_GPBPSR = GPIO_GPIO3;
			break;
	}
}
 
static void jornada56x_set_audio_rate(long val)
{
	struct uda1341_cfg cfg;
	int clk_div;

	/* We don't want to mess with clocks when frames are in flight */
	Ser4SSCR0 &= ~SSCR0_SSE;
	/* wait for any frame to complete */
	udelay(125);

	if (val >= 48000)
		val = 48000;
	else if (val >= 44100)
		val = 44100;
	else if (val >= 32000)
		val = 32000;
	else if (val >= 29400)
		val = 29400;
	else if (val >= 24000)
		val = 24000;
	else if (val >= 22050)
		val = 22050;
	else if (val >= 21970)
		val = 21970;
	else if (val >= 16000)
		val = 16000;
	else if (val >= 14647)
		val = 14647;
	else if (val >= 10985)
		val = 10985;
	else if (val >= 10666)
		val = 10666;
	else
		val = 8000;

	/* Set the external clock generator */
	jornada56x_set_audio_clock(val);

	/* Select the clock divisor */

	cfg.fs = 256; /* always use 256 */
	clk_div = SSCR0_SerClkDiv(2);

	cfg.format = FMT_MSB; /* the jornada 56x appears to be using the MSB format */
	uda1341_configure(uda1341, &cfg);
	Ser4SSCR0 = (Ser4SSCR0 & ~0xff00) + clk_div + SSCR0_SSE;
}
 
static void jornada56x_audio_init(void *dummy)
{
	int i = 100;

	JORNADA_GPIOAFR |= JORNADA_GP_L3;
	udelay(100);
  	SetPMUPCRRegister(PCR_BIT_SET, JORNADA_L3CLK_EN | JORNADA_I2S_CLK_EN);
	udelay(100);
	JORNADA_L3CFR |= JORNADA_L3_EN; /* Enable L3 interface */
	
	GAFR |= (GPIO_SSP_CLK);
	GPDR &= ~(GPIO_SSP_CLK);
	Ser4SSCR0 = 0; 
	Ser4SSCR0 = SSCR0_DataSize(16) + SSCR0_TI + SSCR0_SerClkDiv(2); 
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk; 
	Ser4SSCR0 |= SSCR0_SSE; 
	
	Ser4SSDR = 0;

	JORNADA_GPDPCR = GPIO_GPIO10;
	
	uda1341_open(uda1341);
	
	jornada56x_set_audio_rate(AUDIO_RATE_DEFAULT);
	
	JORNADA_L3CAR = 0x14;
	mdelay(100);
	while ((!(JORNADA_L3CFR & JORNADA_L3_ADDR_DONE))&&(i--));
	JORNADA_L3CDW = 0xc3;
	mdelay(100);
	i=100;
	while ((!(JORNADA_L3CFR & JORNADA_L3_DATA_DONE))&&(i--));

	printk("jornada56x_audi_init() done!");
}

static void jornada56x_audio_shutdown(void *dummy)
{
	SetPMUPCRRegister(PCR_BIT_CLEAR, JORNADA_L3CLK_EN | JORNADA_I2S_CLK_EN);
	JORNADA_L3CFR &= ~JORNADA_L3_EN; /* Disable L3 interface */

	JORNADA_GPDPSR = GPIO_GPIO10;
	
	uda1341_close(uda1341);
	
	Ser4SSCR0 = 0;
}

static int
jornada56x_audio_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	long val = 0;
	int ret = 0;

	switch (cmd) {
	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (int *) arg);
		if (ret)
			break;
		/* the UDA1341 is stereo only */
		ret = (val == 0) ? -EINVAL : 1;
		ret = put_user(ret, (int *) arg);
		break;

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
		/* the UDA1341 is stereo only */
		ret = put_user(2, (long *) arg);
		break;

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (long *) arg);
		if (ret)
			break;
		jornada56x_set_audio_rate(val);
		/* fall through */

	case SOUND_PCM_READ_RATE:
		ret = put_user(val, (long *) arg); /* we assume that jornada56x_set_audio_rate() is always OK */
		break;

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS:
		/* we can do 16-bit only */
		ret = put_user(AFMT_S16_LE, (long *) arg);
		break;

	default:
		/* Maybe this is meant for the mixer (as per OSS Docs) */
		ret = mixer_ioctl(inode, file, cmd, arg);
		break;
	}

	return ret;
}

static audio_stream_t output_stream = {
	.id		= "UDA1345 out",
	.dma_dev	= DMA_Ser4SSPWr,
};

static audio_stream_t input_stream = {
	.id		= "UDA1345 in",
	.dma_dev	= DMA_Ser4SSPRd,
};

static audio_state_t audio_state = {
	.output_stream	= &output_stream,
	.input_stream	= &input_stream,
	.need_tx_for_rx	= 1,
	.hw_init	= jornada56x_audio_init,
	.hw_shutdown	= jornada56x_audio_shutdown,
	.client_ioctl	= jornada56x_audio_ioctl,
	.sem		= __SEMAPHORE_INIT(audio_state.sem,1),
};

static int jornada56x_audio_open(struct inode *inode, struct file *file)
{
        return sa1100_audio_attach(inode, file, &audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_attach().
 */
static struct file_operations jornada56x_audio_fops = {
	.owner	= THIS_MODULE,
	.open	= jornada56x_audio_open,
};

static int audio_dev_id, mixer_dev_id;

static int jornada56x_uda1345_probe(struct device *dev)
{
	input_stream.dev = dev;
	output_stream.dev = dev;
	l3_jornada56x_adapter.data = dev;

	l3_add_adapter(&l3_jornada56x_adapter);

	uda1341 = uda1341_attach("l3-jornada56x");
	
	/* register devices */
	audio_dev_id = register_sound_dsp(&jornada56x_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&uda1341_mixer_fops, -1);

	printk(KERN_INFO "Sound: SA1100 UDA1345 on Jornada 56x: dsp id %d mixer id %d\n",
		audio_dev_id, mixer_dev_id);
	return 0;

}

static int jornada56x_uda1345_remove(struct device *dev)
{
	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	uda1341_detach(uda1341);
	l3_del_adapter(&l3_jornada56x_adapter);

	return 0;
}

static int jornada56x_uda1345_suspend(struct device *dev, pm_message_t state)
{
	sa1100_audio_suspend(&audio_state, state);
	return 0;
}

static int jornada56x_uda1345_resume(struct device *dev)
{
	sa1100_audio_resume(&audio_state);
	return 0;
}

static struct device_driver jornada56x_uda1345_driver = {
	.name		= "sa11x0-ssp",
	.bus		= &platform_bus_type,
	.probe		= jornada56x_uda1345_probe,
	.remove		= jornada56x_uda1345_remove,
	.suspend	= jornada56x_uda1345_suspend,
	.resume		= jornada56x_uda1345_resume,
};

static int jornada56x_uda1345_init(void)
{
	return driver_register(&jornada56x_uda1345_driver);
}

static void jornada56x_uda1345_exit(void)
{
	driver_unregister(&jornada56x_uda1345_driver);
}

module_init(jornada56x_uda1345_init);
module_exit(jornada56x_uda1345_exit);

MODULE_AUTHOR("Alex Lange");
MODULE_DESCRIPTION("Glue audio driver for the Philips UDA1345 codec used on the Jornada 56x.");
