/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * h3600 atmel micro companion support
 * based on previous kernel 2.4 version
 * Author : Alessandro Gardich <gremlin@gremlin.it>
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
#include <linux/device.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/corgi_bl.h>
#include <linux/fb.h>

#include <asm/irq.h>
#include <asm/atomic.h>
#include <asm/arch/hardware.h>

#include <asm/arch/h3600.h>
#include <asm/arch/SA-1100.h>

#include "../../arch/arm/mach-sa1100/generic.h"

#include <asm/hardware/micro.h>

#define TX_BUF_SIZE	32
#define RX_BUF_SIZE	16
#define CHAR_SOF	0x02

/* state of receiver parser */
enum rx_state {
	STATE_SOF = 0,     /* Next byte should be start of frame */
	STATE_ID,          /* Next byte is ID & message length   */
	STATE_DATA,        /* Next byte is a data byte           */
	STATE_CHKSUM       /* Next byte should be checksum       */
};

struct h3600_txdev {
	unsigned char buf[TX_BUF_SIZE];
	atomic_t head;
	atomic_t tail;
};
static struct h3600_txdev tx;	/* transmit ISR state */

struct h3600_rxdev {
	enum rx_state state;            /* context of rx state machine */
	unsigned char chksum;           /* calculated checksum */
	int           id;               /* message ID from packet */
	unsigned int  len;              /* rx buffer length */
	unsigned int  index;            /* rx buffer index */
	unsigned char buf[RX_BUF_SIZE]; /* rx buffer size  */
};

static struct h3600_rxdev rx;	/* receive ISR state */

/*--- backlight ---*/
#define MAX_BRIGHTNESS 255

static void micro_set_bl_intensity(int intensity)
{
	unsigned char data[3];

	//printk(KERN_ERR "h3600_micro : micro backlight send %d %d\n",micro_backlight_power,micro_backlight_brightness);
	data[0] = 0x01;
	data[1] = intensity > 0 ? 1 : 0;
	data[2] = intensity;
	h3600_micro_tx_msg(0x0D,3,data);
}

static struct corgibl_machinfo micro_bl_info = {
        .default_intensity = MAX_BRIGHTNESS / 4,
        .limit_mask = 0xffff,
        .max_intensity = MAX_BRIGHTNESS,
        .set_bl_intensity = micro_set_bl_intensity,
};

/*--- manage messages from Atmel Micro  ---*/

static micro_private_t micro;

static void h3600_micro_rx_msg( int id, int len, unsigned char *data )
{
	int i;

	/*printk(KERN_ERR "h3600_micro : got a message from micro\n");*/
	spin_lock(micro.lock);
	switch (id) {
	case 0x0D : /* backlight */
		/* empty ack, just ignore */
		break;
	case 0x02 : /* keyboard */
		if (micro.h_key != NULL) {
			micro.h_key(len, data);
		} else {
			printk(KERN_ERR "h3600_micro : key message ignored, no handle \n");
		}
		break;
	case 0x03 : /* touchscreen */
		if (micro.h_ts != NULL) {
			micro.h_ts(len, data);
		} else {
			printk(KERN_ERR "h3600_micro : touchscreen message ignored, no handle \n");
		}
		break;
	case 0x06 : /* temperature */
		if (micro.h_temp != NULL) {
			micro.h_temp(len, data);
		} else {
			printk(KERN_ERR "h3600_micro : temperature message ignored, no handle \n");
		}
		break;
	case 0x09 : /* battery */
		if (micro.h_batt != NULL) {
			micro.h_batt(len, data);
		} else {
			printk(KERN_ERR "h3600_micro : battery message ignored, no handle \n");
		}
		break;
	default :
		printk(KERN_ERR "h3600_micro : unknown msg %d [%d] ", id, len);
		for (i=0;i<len;++i)
			printk("0x%02x ", data[i]);
		printk("\n");
	}
	spin_unlock(micro.lock);
}

/*--- low lever serial interface ---*/

static void h3600_micro_process_char( unsigned char ch )
{
	switch ( rx.state ) {
	case STATE_SOF:	/* Looking for SOF */
		if ( ch == CHAR_SOF )
			rx.state=STATE_ID; /* Next byte is the id and len */
		//else
		//	g_statistics.missing_sof++;
		break;
	case STATE_ID: /* Looking for id and len byte */
		rx.id = ( ch & 0xf0 ) >> 4 ;
		rx.len = ( ch & 0x0f );
		rx.index = 0;
		rx.chksum = ch;
		rx.state = ( rx.len > 0 ) ? STATE_DATA : STATE_CHKSUM;
		break;
	case STATE_DATA: /* Looking for 'len' data bytes */
		rx.chksum += ch;
		rx.buf[rx.index]= ch;
		if ( ++rx.index == rx.len )
			rx.state = STATE_CHKSUM;
		break;
	case STATE_CHKSUM: /* Looking for the checksum */
		if ( ch == rx.chksum )
			h3600_micro_rx_msg( rx.id, rx.len, rx.buf );
		//else
		//	g_statistics.bad_checksum++;
		rx.state = STATE_SOF;
		break;
	}
}

static void h3600_micro_rx_chars( void )
{
	unsigned int status, ch;

	while ( (status = Ser1UTSR1) & UTSR1_RNE ) {
		ch = Ser1UTDR;
		/*statistics.rx++;*/
		if ( status & UTSR1_PRE ) { /* Parity error */
			printk(KERN_ERR "h3600_micro_rx : parity error\n");
			/*statistics.parity++;*/
		} else 	if ( status & UTSR1_FRE ) { /* Framing error */
			printk(KERN_ERR "h3600_micro_rx : framing error\n");
			/*statistics.frame++;*/
		} else {
			if ( status & UTSR1_ROR ) {  /* Overrun error */
				printk(KERN_ERR "h3600_micro_rx : overrun error\n");
				/*statistics.overrun++;*/
			}
			h3600_micro_process_char(ch);
		}
	}
}

int h3600_micro_tx_msg(unsigned char id, unsigned char len, unsigned char *data)
{
	int free_space;
	int i;
	unsigned char checksum;
	int head,tail;

	tail = atomic_read(&tx.tail);
	head = atomic_read(&tx.head);

	free_space = (head >= tail) ? (TX_BUF_SIZE - head + tail - 1) : (tail - head - 1);

	if (free_space < len + 2) {
		printk(KERN_ERR "%s : no avaiable space on tx buffer.",__FUNCTION__);
		return -EIO;
	}

	if (0) printk(KERN_ERR "%s : avaiable space %d %d %d\n",__FUNCTION__,free_space,head,tail);

	tx.buf[head]=(unsigned char) CHAR_SOF;
	head = ((head+1) % TX_BUF_SIZE);

	checksum = ((id & 0x0f) << 4) | (len & 0x0f);
	tx.buf[head]=checksum;
	head = ((head+1) % TX_BUF_SIZE);

	for(i=0; i<len; ++i) {
		tx.buf[head]=data[i];
		head = ((head+1) % TX_BUF_SIZE);
		checksum += data[i];
	}

	tx.buf[head]=checksum;
	head = ((head+1) % TX_BUF_SIZE);

	atomic_set(&tx.head,head);

	Ser1UTCR3 |= UTCR3_TIE; /* enable interrupt */

	return 0;
}
EXPORT_SYMBOL(h3600_micro_tx_msg);


static void h3600_micro_tx_chars( void )
{
	int head,tail;

	head = atomic_read(&tx.head);
	tail = atomic_read(&tx.tail);

	while ( (head != tail) && (Ser1UTSR1 & UTSR1_TNF) ) {
		Ser1UTDR = tx.buf[tail];
		tail = ((tail+1) % TX_BUF_SIZE);
	}
	atomic_set(&tx.tail,tail);

	if ( tail == head ) /* Stop interrupts */
		Ser1UTCR3 &= ~UTCR3_TIE;
}

static void h3600_micro_reset_comm( void )
{
	if (1) printk("%s: initializing serial port\n", __FUNCTION__);

	/* Initialize Serial channel protocol frame */
	rx.state = STATE_SOF;  /* Reset the state machine */

	atomic_set(&tx.head,0);
	atomic_set(&tx.tail,0);

	/* Set up interrupts */
	Ser1SDCR0 = 0x1;                               /* Select UART mode */

	Ser1UTCR3 = 0;                                 /* Clean up CR3                  */
	Ser1UTCR0 = UTCR0_8BitData | UTCR0_1StpBit;    /* 8 bits, no parity, 1 stop bit */
	Ser1UTCR1 = 0;                                 /* Baud rate to 115K bits/sec    */
	Ser1UTCR2 = 0x1;

	Ser1UTSR0 = 0xff;                              /* Clear SR0 */
	Ser1UTCR3 = UTCR3_TXE | UTCR3_RXE | UTCR3_RIE; /* Enable receive interrupt */
	Ser1UTCR3 &= ~UTCR3_TIE;                       /* Disable transmit interrupt */
}

/*--- core interrupt ---*/
enum MessageHandleType {
	HANDLE_NORMAL,
	HANDLE_ACK,
	HANDLE_ERROR
};

#define MICRO_MSG_WAITING 0
#define MICRO_MSG_SUCCESS 1
#define MICRO_MSG_ERROR  -1

static irqreturn_t h3600_micro_serial_isr(int irq, void *dev_id)
{
	unsigned int status; /* UTSR0 */
	int head,tail;
	/*unsigned int pass_counter = 0;*/

	if (0) printk("%s\n", __FUNCTION__);

	//statistics.isr++;
	status = Ser1UTSR0;
	do {
		if ( status & (UTSR0_RID | UTSR0_RFS) ) {
			if ( status & UTSR0_RID )
				Ser1UTSR0 = UTSR0_RID; /* Clear the Receiver IDLE bit */
			h3600_micro_rx_chars();
		}

		/* Clear break bits */
		if (status & (UTSR0_RBB | UTSR0_REB))
			Ser1UTSR0 = status & (UTSR0_RBB | UTSR0_REB);

		if ( status & UTSR0_TFS )
			h3600_micro_tx_chars();

		status = Ser1UTSR0;

		head = atomic_read(&tx.head);
		tail = atomic_read(&tx.tail);
	} while ( ( ( (head != tail) && (status & UTSR0_TFS) ) ||
		  status & (UTSR0_RFS | UTSR0_RID ) )
		  /*&& pass_counter++ < H3600_TS_PASS_LIMIT*/ );

	//if ( pass_counter >= H3600_TS_PASS_LIMIT )
	//	statistics.pass_limit++;

	return IRQ_HANDLED;
}

/*--- sub devices declaration ---*/

struct platform_device micro_bl = {
        .name = "corgi-bl",
	.id   = -1,
        .dev = {
    		.platform_data = &micro_bl_info,
	},
};

static struct platform_device h3600_micro_keys = {
	.name = "h3600-micro-keys",
	.id   = -1,
	.dev  = {
		.parent = &h3600micro_device.dev,
	},
};

static struct platform_device h3600_micro_batt = {
	.name = "h3600-micro-battery",
	.id   = -1,
	.dev  = {
		.parent = &h3600micro_device.dev,
	},
};

static struct platform_device h3600_micro_ts = {
	.name = "h3600-micro-ts",
	.id   = -1,
	.dev  = {
		.parent = &h3600micro_device.dev,
	},
};

enum { h3600bl=0, h3600keys, h3600batt, h3600ts, n_devices };
static struct platform_device *devices[n_devices] = {
	&micro_bl,
	&h3600_micro_keys,
	&h3600_micro_batt,
	&h3600_micro_ts,
};

/*--- micro ---*/
static int micro_suspend (struct platform_device *dev, pm_message_t state)
{
	printk(KERN_ERR "micro : suspend \n");
	/*__micro_backlight_set_power(FB_BLANK_POWERDOWN);*/
	return 0;
}

static int micro_resume (struct platform_device *dev)
{
	printk(KERN_ERR "micro : resume\n");
	h3600_micro_reset_comm();
	mdelay(10);

	return 0;
}

static int micro_probe (struct platform_device *dev)
{
	int result=0;
	//struct platform_device *plat;

	printk(KERN_ERR "micro probe : begin \n");

	h3600_micro_reset_comm();

	result = request_irq(IRQ_Ser1UART, h3600_micro_serial_isr,
			     SA_SHIRQ | SA_INTERRUPT | SA_SAMPLE_RANDOM,
			     "h3600_micro", h3600_micro_serial_isr);
	if ( result ) {
		printk(KERN_CRIT "%s: unable to grab serial port IRQ\n", __FUNCTION__);
		return result;
	} else {
		printk(KERN_ERR "h3600_micro : grab serial port IRQ\n");
	}

	/*--- add platform devices that should be avaiable with micro ---*/
	platform_set_drvdata(devices[h3600keys], &micro);
	platform_set_drvdata(devices[h3600batt], &micro);
	platform_set_drvdata(devices[h3600ts], &micro);

	result = platform_add_devices(devices, ARRAY_SIZE(devices));
	if ( result != 0 ) {
		printk(KERN_ERR "micro probe : platform_add_devices fail [%d].\n", result);
	}

	spin_lock_init(&micro.lock);

	printk(KERN_ERR "micro probe : end [%d]\n",result);

	return result;
}

static int micro_remove (struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devices); i++) {
		platform_device_unregister(devices[i]);
	}

	Ser1UTCR3 &= ~(UTCR3_RXE | UTCR3_RIE); /* disable receive interrupt */
	Ser1UTCR3 &= ~(UTCR3_TXE | UTCR3_TIE); /* disable transmit interrupt */
	free_irq(IRQ_Ser1UART, h3600_micro_serial_isr);
	return 0;
}

static struct platform_driver micro_device_driver = {
	.driver   = {
		.name     = "h3600-micro",
	},
	.probe    = micro_probe,
	.remove   = micro_remove,
	.suspend  = micro_suspend,
	.resume   = micro_resume,
	//.shutdown = micro_suspend,
};

static int micro_init (void)
{
	return platform_driver_register(&micro_device_driver);
}

static void
micro_cleanup (void)
{
	platform_driver_unregister (&micro_device_driver);
}

module_init (micro_init);
module_exit (micro_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gremlin.it");
MODULE_DESCRIPTION("driver for iPAQ Atmel micro core and backlight");

