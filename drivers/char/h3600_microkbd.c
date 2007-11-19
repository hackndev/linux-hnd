/*
 * Compaq Microkeyboard serial driver for the iPAQ H3800
 * 
 * Copyright 2002 Compaq Computer Corporation.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Andrew Christian 
 *         <andyc@handhelds.org>
 *         13 February 2002
 *
 * ---------- CHANGES-----------------------
 * 2002-05-19 Marcus Wolschon <Suran@gmx.net>
 *  - added proper support for the Compaq foldable keyboard
 * 
 * 2002-Fall  Andrew Christian <andrew.christian@hp.com>
 *  - cleaned up a bunch of things, added new keyboards
 *
 * 2002-10    Andrew Christian <andrew.christian@hp.com>
 *  - added Flexis FX100, PocketVIK
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kbd_ll.h>
#include <linux/init.h>
#include <linux/kbd_kern.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/pm.h>

#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/hardware.h>

MODULE_AUTHOR("Andrew Christian <andyc@handhelds.org>");

#define MKBD_PRESS	      0x7f
#define MKBD_RELEASE	      0x80

#define MKBD_FUNCTION          128        /* Magic key = function down */
#define MKBD_IGNORE            129        /* A keyboard to be ignored */

#define MKBD_MAX_KEYCODES      128        /* 128 possible keycodes */
#define MKBD_BITMAP_SIZE       MKBD_MAX_KEYCODES / BITS_PER_LONG

struct microkbd_dev;

struct uart_reg {
	void (*init)( struct microkbd_dev *dev );
	void (*release)( struct microkbd_dev *dev );
	volatile u32 * utsr1;
	volatile u32 * utdr;
	volatile u32 * utsr0;
	int irq;
};

struct mkbd_data;

struct microkbd_dev {
	char *           full_name;
	char *           name;
	void           (*process_char)(struct mkbd_data *, unsigned char);
	u32              baud;
	u32              cflag;
	struct uart_reg *uart;
	devfs_handle_t   devfs;
};

/* Modifier flags */
#define MODIFIER_FUNCTION     (1<<0)
#define MODIFIER_NUMLOCK      (1<<1)

struct mkbd_data {
	struct microkbd_dev *dev;
	unsigned long        key_down[MKBD_BITMAP_SIZE];   /* _keycodes_ sent */
	int                  usage_count;

	int                  modifiers;        /* Can be set by NUMLOCK callback */
	/* These variables are internal to each driver
	   On release_all_keys(), we set state = modifiers = 0 */
	int                  state;
	unsigned char        last;             /* Last character received */
};

#define	MKBD_READ     0
#define	MKBD_CHKSUM   1
#define	MKBD_BITMASK  2

struct mkbd_statistics {
	u32   isr;          /* RX interrupts       */
	u32   rx;           /* Bytes received      */
	u32   frame;        /* Frame errors        */
	u32   overrun;      /* Overrun errors      */
	u32   parity;       /* Parity errors       */
	u32   bad_xor;
	u32   invalid;
	u32   already_down;
	u32   already_up;
	u32   valid;
	u32   forced_release;
};

static struct mkbd_statistics     g_statistics;

#define MKBD_DLEVEL 4
#define SDEBUG(x)                   if ( x < MKBD_DLEVEL ) printk(__FUNCTION__ "\n")
#define SFDEBUG(x, format, args...) if ( x < MKBD_DLEVEL ) printk(__FUNCTION__ ": " format, ## args )

#define SERIAL_BAUD_BASE 230400

/***********************************************************************************/
/*   Interrupt handlers                                                            */
/***********************************************************************************/

static void h3600_microkbd_rx_chars( struct mkbd_data *mkbd )
{
	unsigned int status, ch;
	struct uart_reg *uart = mkbd->dev->uart;
	int count = 0;

	while ( (status = *uart->utsr1) & UTSR1_RNE ) {
		ch = *uart->utdr;
		g_statistics.rx++;

		if ( status & UTSR1_PRE ) { /* Parity error */
			g_statistics.parity++;
		} else 	if ( status & UTSR1_FRE ) { /* Framing error */
			g_statistics.frame++;
		} else {
			if ( status & UTSR1_ROR )   /* Overrun error */
				g_statistics.overrun++;
			
			count++;
			mkbd->dev->process_char( mkbd, ch );
		}
	}
}

static void h3600_microkbd_data_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct mkbd_data *mkbd = (struct mkbd_data *) dev_id;
	struct uart_reg *uart = mkbd->dev->uart;
        unsigned int status;	/* UTSR0 */

	SFDEBUG(4,"data interrupt %p\n", mkbd);
	g_statistics.isr++;
        status = *uart->utsr0;

	if ( status & (UTSR0_RID | UTSR0_RFS) ) {
		if ( status & UTSR0_RID )
			*uart->utsr0 = UTSR0_RID; /* Clear the Receiver IDLE bit */
		h3600_microkbd_rx_chars( mkbd );
	}

	/* Clear break bits */
	if (status & (UTSR0_RBB | UTSR0_REB))
		*uart->utsr0 = status & (UTSR0_RBB | UTSR0_REB);
}


/***********************************************************************************/
/*   Initialization and shutdown code                                              */
/***********************************************************************************/

static void h3600_microkbd_init_uart2( struct microkbd_dev *dev )
{
	u32 brd = (SERIAL_BAUD_BASE / dev->baud) - 1;

	SFDEBUG(2,"initializing serial port 2\n");

	PPSR &= ~PPC_TXD2;
	PSDR &= ~PPC_TXD2;
	PPDR |= PPC_TXD2;

	Ser2UTCR3 = 0;                                 /* Clean up CR3                  */

	Ser2HSCR0 = HSCR0_UART;
	Ser2UTCR4 = UTCR4_HPSIR | UTCR4_Z1_6us;

	Ser2UTCR0 = dev->cflag;

	Ser2UTCR1 = (brd & 0xf00) >> 8;
	Ser2UTCR2 = brd & 0xff;

	Ser2UTSR0 = 0xff;                              /* Clear SR0 */
        Ser2UTCR3 = UTCR3_RXE | UTCR3_RIE;             /* Enable receive interrupt */

	set_h3600_egpio(IPAQ_EGPIO_IR_ON);
}

static void h3600_microkbd_release_uart2( struct microkbd_dev *dev )
{
	clr_h3600_egpio(IPAQ_EGPIO_IR_ON);
}


static void h3600_microkbd_init_uart3( struct microkbd_dev *dev )
{
	u32 brd = (SERIAL_BAUD_BASE / dev->baud) - 1;

	SFDEBUG(2,"initializing serial port 3\n");
	
	/* Turn on power to the RS232 chip */
	set_h3600_egpio(IPAQ_EGPIO_RS232_ON);

	/* Set up interrupts */
	Ser3UTCR3 = 0;                                 /* Clean up CR3                  */
	Ser3UTCR0 = dev->cflag;

	Ser3UTCR1 = (brd & 0xf00) >> 8;
	Ser3UTCR2 = brd & 0xff;

	Ser3UTSR0 = 0xff;                              /* Clear SR0 */
        Ser3UTCR3 = UTCR3_RXE | UTCR3_RIE;             /* Enable receive interrupt */
}

static void h3600_microkbd_release_uart3( struct microkbd_dev *dev )
{
	clr_h3600_egpio(IPAQ_EGPIO_RS232_ON);
}

static struct uart_reg g_uart_2 = { 
	init :    h3600_microkbd_init_uart2, 
	release : h3600_microkbd_release_uart2,
	utsr1 :   &Ser2UTSR1, 
	utdr :    &Ser2UTDR, 
	utsr0 :   &Ser2UTSR0, 
	irq :     IRQ_Ser2ICP 
};

static struct uart_reg g_uart_3 = { 
	init :    h3600_microkbd_init_uart3, 
	release : h3600_microkbd_release_uart3,
	utsr1 :   &Ser3UTSR1, 
	utdr :    &Ser3UTDR, 
	utsr0 :   &Ser3UTSR0, 
	irq :     IRQ_Ser3UART 
};


/***********************************************************************************/
/*   Utility routine                                                               */
/***********************************************************************************/

static int do_keycode_down( struct mkbd_data *mkbd, unsigned char keycode )
{
	SFDEBUG(3,"keycode=0x%02x\n", keycode);
	
	if (keycode > 127)
		return 0;

	if (test_bit(keycode,mkbd->key_down)) {
		g_statistics.already_down++;
		SFDEBUG(2,"already down keycode=0x%02x\n", keycode);
		return 1;
	}

	set_bit(keycode,mkbd->key_down);
	g_statistics.valid++;
	handle_scancode(keycode,1);
	tasklet_schedule(&keyboard_tasklet);
	return 0;
}

static int do_keycode_up( struct mkbd_data *mkbd, unsigned char keycode )
{
	SFDEBUG(3,"keycode=0x%02x\n", keycode);
	
	if (keycode > 127)
		return 0;

	if (!test_bit(keycode,mkbd->key_down)) {
		g_statistics.already_up++;
		SFDEBUG(2,"already up keycode=0x%02x\n", keycode);
		return 1;
	}

	clear_bit(keycode,mkbd->key_down);
	g_statistics.valid++;
	handle_scancode(keycode,0);
	tasklet_schedule(&keyboard_tasklet);
	return 0;
}

static inline int do_keycode( struct mkbd_data *mkbd, unsigned char keycode, int down )
{
	if ( down )
		return do_keycode_down(mkbd,keycode);
	else
		return do_keycode_up(mkbd,keycode);
}

static int try_keycode_up( struct mkbd_data *mkbd, unsigned char keycode )
{
	if ( keycode > 0 && keycode < 128 && test_bit(keycode,mkbd->key_down))
		return do_keycode_up( mkbd, keycode);
	return 0;
}

static void release_all_keys( struct mkbd_data *mkbd )
{
	int i,j;
	unsigned long *bitfield = mkbd->key_down;

	SFDEBUG(2,"releasing all keys.\n"); 

	mkbd->modifiers = 0;
	for ( i = 0 ; i < MKBD_MAX_KEYCODES ; i+=BITS_PER_LONG, bitfield++ ) {
		if ( *bitfield ) {  /* Should fix this to use the ffs() function */
			for (j=0 ; j<BITS_PER_LONG; j++)
				if (test_and_clear_bit(j,bitfield))
					handle_scancode(i+j, 0);
		}
	}
	tasklet_schedule(&keyboard_tasklet);
	g_statistics.forced_release++;
}

static inline int valid_keycode( int keycode )
{
	if ( !keycode ) {
		g_statistics.invalid++;
		SFDEBUG(3," invalid keycode 0x%02x\n", keycode );
		return 0;
	}
	return 1;
}


/***********************************************************************************
 *   iConcepts
 * 
 *  9600 baud, 8N1
 *
 *  Notes:
 *   The shift, control, alt, and fn keys are the only keys that can be held
 *      down while pressing another key.  
 *   All other keys generate keycode for down and "0x85 0x85" for up.
 *
 *  It seems to want an ACK of some kind?
 *
 *              Down       Up
 *   Control    1          9e 9e
 *   Function   2          9d 9d       (we use this as AltGr)
 *   Shift      3          9c 9c 
 *   Alt        4          9b 9b       (we use this as Meta or Alt)
 *   OTHERS     keycode    85 85 
 *   
 ***********************************************************************************/

static unsigned char iconcepts_normal[128] = { 
        0, 29, 100, 42, 56, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        2, 10, 34, 24, 4, 30, 23, 16, 6, 46, 37, 31, 8, 18, 50, 22, 
        0, 0, 0, 0, 0, 0, 0, 0, 45, 39, 28, 105, 44, 51, 1, 123, 
        13, 53, 111, 125, 26, 15, 108, 87, 0, 0, 0, 0, 0, 0, 0, 0, 
        47, 49, 33, 9, 20, 38, 32, 7, 19, 36, 48, 5, 25, 35, 11, 3, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        122, 110, 52, 12, 124, 57, 40, 21, 106, 58, 43, 17, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 103, 41, 27,  };

static unsigned char iconcepts_numlock[128] = { 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 73, 0, 77, 0, 0, 76, 0, 0, 0, 80, 0, 75, 0, 82, 75, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 74, 0, 0, 0, 51, 0, 0, 
        0, 78, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 72, 0, 81, 0, 0, 0, 79, 0, 0, 55, 0, 98, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 83, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static void h3600_iconcepts_process_char( struct mkbd_data *mkbd, unsigned char data )
{
	unsigned char key       = data;
	unsigned int  key_down  = !(data & 0x80);
	unsigned char keycode;
	int release_all = 1;

	SFDEBUG(2,"Read 0x%02x (%ld)\n", data, jiffies );

	if ( data >= 0x9b && data <= 0x9e ) {
		key = 0x9f - data;
		SFDEBUG(3,"set key to 0x%02x\n", key);
	}
	else if ( data == 0x85 ) {
		key = mkbd->last;
		SFDEBUG(3,"last key to 0x%02x\n", key);
		mkbd->last = 0;
	}

	keycode = iconcepts_normal[key];

	if ( valid_keycode(keycode) ) {
		if ( key_down ) {
			if ( (mkbd->modifiers & MODIFIER_NUMLOCK) && iconcepts_numlock[key])
				keycode = iconcepts_numlock[key];
			release_all = do_keycode_down( mkbd, keycode );
			if ( key > 4 )
				mkbd->last = key;
		}
		else {
			release_all = try_keycode_up( mkbd, keycode ) +
				try_keycode_up( mkbd, iconcepts_numlock[key] );
		}
	}

	if ( release_all )
		release_all_keys(mkbd);
}

/***********************************************************************************
 *   Snap N Type keyboard                                                         
 *
 * Key down sends single byte: KEY
 * Key up sends two bytes:     (KEY | 0x80), 0x7f
 *
 ***********************************************************************************/

static unsigned char snapntype_normal[128] = { 
        0, 0, 0, 0, 0, 29, 0, 0, 14, 0, 28, 104, 109, 0, 0, 1, 
        42, 0, 122, 123, 124, 125, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0, 
        57, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50, 49, 24, 
        25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44, 0, 0, 0, 0, 0,  };

static void h3600_snapntype_process_char( struct mkbd_data *mkbd, unsigned char data )
{
	unsigned char key       =   data & 0x7f;
	unsigned int  key_down  = !(data & 0x80);
	unsigned char keycode;
	int release_all = 1;

	SFDEBUG(2,"Read 0x%02x\n", data );
	
	if ( data == 0x7f )
		return;

	keycode = snapntype_normal[key];
	if ( valid_keycode(keycode) )
		release_all = do_keycode(mkbd,keycode,key_down);

	if ( release_all )
		release_all_keys(mkbd);
}

/***********************************************************************************
 *   Compaq Microkeyboard
 *
 * 4800 baud, 8N1
 *
 * Key down sends two bytes:  KEY          ~KEY (complement of KEY)
 * Key up sends two bytes:    (KEY | 0x80) ~KEY
 ***********************************************************************************/

static unsigned char compaq_normal[128] = { 
        0, 0, 100, 0, 124, 122, 123, 0, 0, 0, 0, 0, 125, 0, 0, 0, 
        0, 0, 42, 0, 0, 16, 0, 0, 0, 29, 44, 31, 30, 17, 0, 0, 
        0, 46, 45, 32, 18, 0, 0, 0, 103, 0, 47, 33, 20, 19, 0, 106, 
        0, 49, 48, 35, 34, 21, 0, 0, 0, 0, 50, 36, 22, 0, 0, 0, 
        0, 0, 37, 23, 24, 0, 0, 0, 0, 0, 0, 38, 0, 25, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 58, 0, 28, 0, 57, 0, 105, 0, 
        108, 0, 0, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static void h3600_compaq_process_char(struct mkbd_data *mkbd, unsigned char data )
{
	unsigned char key       =   data & 0x7f;
	unsigned char keycode;
	int key_down;
	int release_all = 1;

	switch (mkbd->state) {
	case MKBD_READ:
 		SFDEBUG(2,"Read 0x%02x\n", data );
		keycode = compaq_normal[key];
		if ( valid_keycode(keycode) ) {
			mkbd->state = MKBD_CHKSUM;
			mkbd->last  = data;
			release_all = 0;
		}
		break;
	case MKBD_CHKSUM:
		key      =   mkbd->last & 0x7f;
		key_down = !(mkbd->last & 0x80);

 		SFDEBUG(3,"Checking read 0x%02x with 0x%02x\n", mkbd->last, data );
		if ( (key ^ data) != 0xff ) {
			g_statistics.bad_xor++;
			SFDEBUG(3,"   XOR doesn't match last=0x%02x data=0x%02x XOR=0x%02x\n", 
			       key, data, (key ^ data));
		}
		else if ( key == 0x75 ) {
			SFDEBUG(3,"   Valid keyboard restart\n");
		}
		else {
			release_all = do_keycode(mkbd, compaq_normal[key], key_down);
			mkbd->state = MKBD_READ;
		}
		break;
	}
	
	if ( release_all ) {
		release_all_keys(mkbd);
		mkbd->state = MKBD_READ;
	}
}

/***********************************************************************************
 *   HP foldable keyboard                                                     
 *
 * 4800 baud, 8N1
 *
 * Key down sends two bytes:  KEY          ~KEY (complement of KEY)
 * Key up sends two bytes:    (KEY | 0x80) ~KEY
 ***********************************************************************************/

static unsigned char foldable_normal[128] = { 
        0, 0, 128, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 15, 40, 0, 
        0, 56, 42, 0, 29, 16, 2, 0, 0, 0, 44, 31, 30, 17, 3, 0, 
        0, 46, 45, 32, 18, 5, 4, 0, 103, 0, 47, 33, 20, 19, 6, 106, 
        0, 49, 48, 35, 34, 21, 7, 0, 0, 0, 50, 36, 22, 8, 9, 0, 
        0, 51, 37, 23, 24, 11, 10, 0, 0, 52, 53, 38, 39, 25, 12, 0, 
        0, 0, 40, 0, 26, 13, 0, 0, 58, 54, 28, 27, 57, 43, 105, 0, 
        108, 0, 0, 0, 0, 0, 111, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        92, 14, 0, 97, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static unsigned char foldable_function[128] = { 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 
        0, 0, 0, 0, 0, 124, 59, 0, 0, 0, 0, 0, 123, 85, 60, 0, 
        0, 0, 0, 0, 89, 62, 61, 0, 104, 0, 0, 0, 125, 122, 63, 107, 
        0, 0, 0, 0, 0, 90, 64, 0, 0, 0, 0, 0, 0, 65, 66, 0, 
        0, 0, 0, 0, 0, 68, 67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 69, 0, 91, 0, 0, 0, 102, 0, 
        109, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        125, 88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static unsigned char foldable_numlock[128] = { 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 82, 79, 75, 71, 72, 0, 
        0, 0, 80, 76, 77, 55, 73, 0, 0, 83, 98, 81, 78, 74, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static void h3600_foldable_process_char(struct mkbd_data *mkbd, unsigned char data )
{
	unsigned char key;
	unsigned char keycode;
	int key_down;
	int release_all = 1;

	switch (mkbd->state) {
	case MKBD_READ:
 		SFDEBUG(2,"Read 0x%02x\n", data );
		key     = data & 0x7f;
		keycode = foldable_normal[key];
		if ( valid_keycode(keycode) ) {
			mkbd->state = MKBD_CHKSUM;
			mkbd->last  = data;
			release_all = 0;
		}
		break;
	case MKBD_CHKSUM:
		key      =   mkbd->last & 0x7f;
		key_down = !(mkbd->last & 0x80);

 		SFDEBUG(3,"Checking read 0x%02x with 0x%02x\n", mkbd->last, data );
		if ( (key ^ data) != 0xff ) {
			g_statistics.bad_xor++;
			SFDEBUG(3,"   XOR doesn't match last=0x%02x data=0x%02x XOR=0x%02x\n", 
				key, data, (key ^ data));
		}
		else if ( key == 0x75 ) {
			SFDEBUG(3,"   Valid keyboard restart\n");
		}
		else {
			keycode = foldable_normal[key];
			if ( keycode == MKBD_FUNCTION ) {
				if ( key_down )
					mkbd->modifiers |= MODIFIER_FUNCTION;
				else
					mkbd->modifiers &= ~MODIFIER_FUNCTION;
				mkbd->state = MKBD_READ;
				return;
			}

			if ( key_down ) {
				if ( (mkbd->modifiers & MODIFIER_NUMLOCK) && foldable_numlock[key])
					keycode = foldable_numlock[key];
				else if ( (mkbd->modifiers & MODIFIER_FUNCTION) && foldable_function[key])
					keycode = foldable_function[key];
				release_all = do_keycode_down( mkbd, keycode );
			}
			else {
				release_all = try_keycode_up( mkbd, keycode ) + 
					try_keycode_up( mkbd, foldable_numlock[key] ) +
					try_keycode_up( mkbd, foldable_function[key] );
			}
			mkbd->state = MKBD_READ;
		}
		break;
	}
	
	if ( release_all ) {
		release_all_keys(mkbd);
		mkbd->state = MKBD_READ;
	}
}


/***********************************************************************************
 * Eagle-Touch Portable PDA keyboard
 *
 * 9600 baud, 8N1
 *
 * Key down sends:  KEY
 * Key up sends    ~KEY
 *
 * After a delay, keyboard sends 74 to indicate it's going low power
 * When you press a key after the delay, it sends:  75 6d 61 78 
 *   followed by the normal KEY, ~KEY
 ***********************************************************************************/

static unsigned char eagletouch_normal[128] = { 
        0, 0, 128, 122, 88, 87, 124, 0, 85, 0, 89, 123, 90, 15, 40, 0, 
        0, 125, 42, 0, 29, 16, 2, 0, 0, 0, 44, 31, 30, 17, 3, 111, 
        0, 46, 45, 32, 18, 5, 4, 0, 103, 57, 47, 33, 20, 19, 6, 106, 
        100, 49, 48, 35, 34, 21, 7, 0, 91, 0, 50, 36, 22, 8, 9, 0, 
        56, 51, 37, 23, 24, 11, 10, 1, 0, 52, 53, 38, 39, 25, 12, 0, 
        0, 0, 40, 0, 26, 13, 0, 0, 58, 54, 28, 27, 0, 43, 105, 0, 
        108, 129, 0, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 129, 0, 0, 
        0, 0, 0, 0, 129, 129, 0, 0, 129, 0, 0, 0, 0, 0, 0, 0,  };

static unsigned char eagletouch_function[128] = { 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 60, 0, 
        0, 0, 0, 0, 0, 62, 61, 0, 104, 0, 0, 0, 0, 0, 63, 107, 
        0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 65, 66, 0, 
        0, 0, 0, 0, 0, 68, 67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 102, 0, 
        109, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static void h3600_eagletouch_process_char( struct mkbd_data *mkbd, unsigned char data )
{
	unsigned int  key_down  = (data & 0x80) ? 1 : 0;
	unsigned char key       = (key_down ? ~data : data);
	unsigned char keycode   = eagletouch_normal[key];
	int release_all = 1;

	SFDEBUG(2,"Read 0x%02x  key=%x down=%d keycode=%d\n", data, key, key_down, keycode );

	if ( keycode == MKBD_IGNORE ) {
		release_all_keys(mkbd);
		return;
	}

	if ( keycode == MKBD_FUNCTION ) {
		if ( key_down )
			mkbd->modifiers |= MODIFIER_FUNCTION;
		else
			mkbd->modifiers &= ~MODIFIER_FUNCTION;
		return;
	}

	if ( valid_keycode(keycode) ) {
		if ( key_down ) {
			if ( (mkbd->modifiers & MODIFIER_FUNCTION) && eagletouch_function[key])
				keycode = eagletouch_function[key];
			release_all = do_keycode_down( mkbd, keycode );
		}
		else {
			release_all = try_keycode_up( mkbd, keycode ) + 
				try_keycode_up( mkbd, eagletouch_function[key] );
		}
	}

	if ( release_all )
		release_all_keys(mkbd);
}


/***********************************************************************************
 * Micro Innovations IR keyboard
 *
 * 9600 baud, 8N1
 *
 * Key down sends one byte:  KEY
 * Key up sends one byte:    KEY | 0x80
 * Last key up repeats key up
 ***********************************************************************************/

static unsigned char microinnovations_normal[128] = { 
        0, 16, 0, 44, 0, 0, 0, 30, 0, 17, 0, 45, 0, 0, 0, 31, 
        0, 18, 0, 46, 0, 0, 0, 32, 0, 19, 0, 47, 0, 0, 0, 33, 
        0, 20, 0, 48, 0, 0, 0, 34, 0, 21, 0, 57, 0, 0, 0, 35, 
        0, 22, 36, 49, 57, 0, 0, 0, 0, 23, 37, 50, 128, 0, 0, 0, 
        122, 24, 38, 51, 105, 0, 0, 0, 123, 25, 39, 52, 108, 0, 0, 0, 
        125, 12, 40, 103, 106, 0, 0, 0, 90, 14, 28, 111, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 42, 56, 0, 0, 15, 0, 0, 0, 29, 69, 0, 
        0, 0, 0, 0, 0, 87, 100, 58, 0, 0, 0, 0, 0, 54, 0, 0,  };

static unsigned char microinnovations_function[128] = { 
        0, 59, 0, 117, 0, 0, 0, 92, 0, 60, 0, 126, 0, 0, 0, 93, 
        0, 61, 0, 127, 0, 0, 0, 111, 0, 62, 0, 113, 0, 0, 0, 94, 
        0, 63, 0, 124, 0, 0, 0, 95, 0, 64, 0, 0, 0, 0, 0, 99, 
        0, 65, 0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 0, 0, 0, 0, 
        88, 67, 0, 0, 102, 0, 0, 0, 85, 68, 0, 0, 109, 0, 0, 0, 
        89, 0, 0, 104, 107, 0, 0, 0, 119, 91, 116, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static unsigned char microinnovations_numlock[128] = { 
        0, 79, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 
        0, 81, 0, 0, 0, 0, 0, 0, 0, 75, 0, 0, 0, 0, 0, 0, 
        0, 76, 0, 0, 0, 0, 0, 0, 0, 77, 0, 0, 0, 0, 0, 0, 
        0, 71, 75, 0, 0, 0, 0, 0, 0, 72, 76, 79, 82, 0, 0, 0, 
        0, 73, 77, 80, 83, 0, 0, 0, 0, 82, 55, 81, 78, 0, 0, 0, 
        0, 98, 0, 74, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static void h3600_microinnovations_process_char( struct mkbd_data *mkbd, unsigned char data )
{
	unsigned char key       =   data & 0x7f;
	unsigned int  key_down  = !(data & 0x80);
	unsigned char keycode   = microinnovations_normal[key];
	int release_all = 1;

	SFDEBUG(2,"Read 0x%02x\n", data );
	
	if ( keycode == MKBD_FUNCTION ) {
		if ( key_down )
			mkbd->modifiers |= MODIFIER_FUNCTION;
		else
			mkbd->modifiers &= ~MODIFIER_FUNCTION;
		return;
	}

	if ( valid_keycode(keycode) ) {
		if ( key_down ) {
			if ( (mkbd->modifiers & MODIFIER_NUMLOCK) && microinnovations_numlock[key])
				keycode = microinnovations_numlock[key];
			else if ( (mkbd->modifiers & MODIFIER_FUNCTION) && microinnovations_function[key])
				keycode = microinnovations_function[key];
			release_all = do_keycode_down( mkbd, keycode );
		}
		else {
			release_all = try_keycode_up( mkbd, keycode ) + 
				try_keycode_up( mkbd, microinnovations_numlock[key] ) +
				try_keycode_up( mkbd, microinnovations_function[key] );
		}
	}

	if ( release_all )
		release_all_keys(mkbd);
}

/***********************************************************************************
 * Grandtec PocketVIK     (www.grandtec.com)
 *
 * 57600 baud, 8N1
 *
 *  This keyboard autorepeats
 *  It does not generate key up
 * 
 *  It may have a low power sleep mode that needs to be cycled????
 *
 *  Set 1:   ShiftL, ShiftR, CtrlL, AltL, AltR, CtrlR, Fn
 *  Set 2:   All others
 *
 *  Set 1 keys do not generate keystrokes.  They set fields in a bit mask.
 *
 *  MASK = 0x80 | (sum of active modifiers)
 *  
 *  On keydown, you either get
 * 
 *  Case 1 (no bit mask field) 	   KEY
 *  Case 2 (bitmask field)         MASK, KEY
 ***********************************************************************************/

static unsigned char pocketvik_normal[128] = { 
        0, 108, 105, 51, 0, 0, 57, 46, 0, 50, 103, 53, 52, 0, 106, 47, 
        0, 44, 87, 28, 40, 0, 37, 35, 34, 45, 31, 0, 27, 39, 11, 36, 
        21, 33, 32, 30, 16, 26, 25, 23, 22, 20, 19, 18, 17, 15, 13, 58, 
        10, 9, 7, 6, 4, 3, 41, 14, 24, 12, 8, 5, 0, 122, 2, 38, 
        111, 1, 48, 43, 14, 125, 123, 124, 49, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

static unsigned char pocketvik_function[128] = { 
        0, 109, 102, 0, 0, 0, 0, 0, 0, 0, 104, 0, 0, 0, 107, 0, 
        0, 0, 0, 91, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 89, 0, 0, 
        88, 0, 0, 0, 0, 92, 85, 90, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  };

#define POCKETVIK_Fn		0x40

struct {
	unsigned char mask;
	unsigned char key;
} pocketvik_modifiers[] = {
	{ 0x01, KEY_LEFTSHIFT },
	{ 0x02, KEY_RIGHTSHIFT },
	{ 0x04, KEY_LEFTCTRL },
	{ 0x08, KEY_RIGHTCTRL },
	{ 0x10, KEY_LEFTALT },
	{ 0x20, KEY_RIGHTALT }
};

static void h3600_pocketvik_process_char( struct mkbd_data *mkbd, unsigned char data )
{
	unsigned char keycode;
	unsigned char modifier_keys = 0;
	int i;

	SFDEBUG(2,"Read 0x%02x\n", data );

	switch (mkbd->state) {
	case MKBD_READ:
		if (data & 0x80) {
			mkbd->state = MKBD_BITMASK;
			mkbd->last  = data;
			return;
		}
		break;
	case MKBD_BITMASK:
		modifier_keys = mkbd->last;
		mkbd->state = MKBD_READ;
		break;
	}

	keycode = (modifier_keys & POCKETVIK_Fn) ? pocketvik_function[data] : pocketvik_normal[data];

	if ( valid_keycode(keycode) ) {
		for ( i = 0 ; i < ARRAY_SIZE(pocketvik_modifiers) ; i++ ) 
			if ( pocketvik_modifiers[i].mask & modifier_keys )
				do_keycode_down( mkbd, pocketvik_modifiers[i].key );

		do_keycode_down( mkbd, keycode );
		do_keycode_up( mkbd, keycode );

		for ( i = 0 ; i < ARRAY_SIZE(pocketvik_modifiers) ; i++ ) 
			if ( pocketvik_modifiers[i].mask & modifier_keys )
				do_keycode_up( mkbd, pocketvik_modifiers[i].key );
	}
}

/***********************************************************************************
 * Flexis FX-100
 *
 * 9600 baud, 8N1
 *
 * This keyboard autorepeats
 * It does not generate key up for most keys
 *
 * Set 1:   ShiftL, ShiftR, Ctrl, Alt, Fn
 * Set 2:   All others
 *
 *             Down       	  Up
 *             ====		  ==
 *  Set 1      KEY | 0x80	  KEY
 *  Set 2      KEY | 0x80, KEY    <none>
 *  
 * If you press two keys from Set 1 you get the following:
 *
 * Press Key #1			KEY1_DOWN
 * Press Key #2			KEY1_UP
 * Release Key #1		KEY2_DOWN
 * Release Key #2		KEY2_UP
 ***********************************************************************************/

static unsigned char flexis_fx100_normal[128] = { 
        30, 31, 32, 33, 35, 34, 44, 45, 46, 47, 1, 48, 16, 17, 18, 19, 
        21, 20, 2, 3, 4, 5, 7, 6, 13, 10, 8, 12, 9, 11, 27, 24, 
        22, 26, 23, 25, 28, 38, 36, 40, 37, 39, 43, 51, 53, 49, 50, 52, 
        15, 57, 41, 14, 0, 0, 0, 56, 54, 58, 128, 29, 42, 87, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 111, 105, 106, 108, 103, 0,  };

static unsigned char flexis_fx100_function[128] = { 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 123, 88, 85, 89, 0, 0, 90, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 91, 0, 0, 0, 92, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 102, 107, 109, 104, 0,  };

static void h3600_flexis_process_char( struct mkbd_data *mkbd, unsigned char data )
{
	unsigned char key       = data & 0x7f;
	unsigned int  key_down  = data & 0x80;
	unsigned char keycode   = flexis_fx100_normal[key];
	int release_all = 1;

	SFDEBUG(2,"Read 0x%02x\n", data );
	
	if ( keycode == MKBD_FUNCTION ) {
		if ( key_down )
			mkbd->modifiers |= MODIFIER_FUNCTION;
		else
			mkbd->modifiers &= ~MODIFIER_FUNCTION;
		return;
	}

	if ( valid_keycode(keycode) ) {
		if ( key_down ) {
			if ( (mkbd->modifiers & MODIFIER_FUNCTION) && flexis_fx100_function[key])
				keycode = flexis_fx100_function[key];
			release_all = do_keycode_down( mkbd, keycode );
		}
		else {
			release_all = try_keycode_up( mkbd, keycode ) + 
				try_keycode_up( mkbd, flexis_fx100_function[key] );
		}
	}

	if ( release_all )
		release_all_keys(mkbd);
}

/***********************************************************************************/

static struct microkbd_dev keyboards[] = {
	{ "HP/Compaq Micro Keyboard", "compaq",    
	  h3600_compaq_process_char, 4800, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_3 },
	{ "RipTide SnapNType", "snapntype", 
	  h3600_snapntype_process_char, 2400, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_3 },
	{ "iConcepts Portable Keyboard", "iconcepts",
	  h3600_iconcepts_process_char, 9600, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_3 },
	{ "HP/Compaq Foldable Keyboard", "foldable",  
	  h3600_foldable_process_char,  4800, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_3 },
	{ "Micro Innovations IR Keyboard", "microinnovations",  
	  h3600_microinnovations_process_char, 9600, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_2 },
	{ "Eagle-Touch Portable PDA Keyboard", "eagletouch",  
	  h3600_eagletouch_process_char,  9600, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_3 },
	{ "GrandTec PocketVIK", "pocketvik",  
	  h3600_pocketvik_process_char, 57600, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_3 },
	{ "Flexis FX100", "flexis",  
	  h3600_flexis_process_char, 9600, UTCR0_8BitData | UTCR0_1StpBit, &g_uart_3 },
};

#define NUM_KEYBOARDS ARRAY_SIZE(keyboards)


/***********************************************************************************/
/*   Basic driver code                                                             */
/***********************************************************************************/

static ssize_t h3600_microkbd_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	SDEBUG(1);

	current->state = TASK_INTERRUPTIBLE;
	while (!signal_pending(current))
		schedule();
	current->state = TASK_RUNNING;
	return 0;
}

static struct mkbd_data g_keyboard;

static void h3600_microkbd_ledfunc(unsigned int led)
{
	SFDEBUG(2," led=0x%02x [%s %s %s]\n", led,
		(led & 0x04 ? "CAPS" : "----"),
		(led & 0x02 ? "NUML" : "----"),
		(led & 0x01 ? "SCRL" : "----"));
	if ( led & 0x02 )
		g_keyboard.modifiers |= MODIFIER_NUMLOCK;
	else
		g_keyboard.modifiers &= ~MODIFIER_NUMLOCK;
}

static int h3600_microkbd_open( struct inode * inode, struct file * filp)
{
	unsigned int minor = MINOR( inode->i_rdev );
	struct microkbd_dev *dev = keyboards + minor;
	int retval;

	SFDEBUG(1," minor=%d\n",minor);

	if ( minor >= NUM_KEYBOARDS ) {
		printk(KERN_ALERT __FUNCTION__ " bad minor=%d\n", minor);
		return -ENODEV;
	}

	if ( g_keyboard.usage_count != 0 ) {
		printk(KERN_ALERT __FUNCTION__ " keyboard device already open\n");
		return -EBUSY;
	}

	retval = request_irq( dev->uart->irq, h3600_microkbd_data_interrupt,
			      SA_INTERRUPT | SA_SAMPLE_RANDOM,
			      "h3600_microkbd", (void *) &g_keyboard);
	if ( retval ) return retval;

	memset(&g_keyboard, 0, sizeof(struct mkbd_data));
	g_keyboard.dev = dev;

	dev->uart->init( dev );

	filp->private_data = &g_keyboard;
	kbd_ledfunc = h3600_microkbd_ledfunc;
	g_keyboard.usage_count++;
	MOD_INC_USE_COUNT;
	return 0;
}

static int h3600_microkbd_release(struct inode * inode, struct file * filp)
{
	struct mkbd_data *mkbd = (struct mkbd_data *) filp->private_data;

	SDEBUG(1);

	if ( --mkbd->usage_count == 0 ) {
		SFDEBUG(1,"Closing down keyboard\n");
		release_all_keys(mkbd);
		free_irq(mkbd->dev->uart->irq, (void *)mkbd);
		mkbd->dev->uart->release( mkbd->dev );
		kbd_ledfunc = NULL;
	}

	MOD_DEC_USE_COUNT;
	return 0;
}

struct file_operations microkbd_fops = {
	read:           h3600_microkbd_read,
	open:		h3600_microkbd_open,
	release:	h3600_microkbd_release,
};

/***********************************************************************************/
/*   Proc filesystem interface                                                     */
/***********************************************************************************/

static struct proc_dir_entry   *proc_dir;

#define PRINT_DATA(x,s) \
	p += sprintf (p, "%-20s : %d\n", s, g_statistics.x)

int h3600_microkbd_proc_stats_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	char *p = page;
	int len, i;

	PRINT_DATA(isr,        "Keyboard interrupts");
	PRINT_DATA(rx,         "Bytes received");
	PRINT_DATA(frame,      "Frame errors");
	PRINT_DATA(overrun,    "Overrun errors");
	PRINT_DATA(parity,     "Parity errors");
	PRINT_DATA(bad_xor,    "Bad XOR");
	PRINT_DATA(invalid,    "Invalid keycode");
	PRINT_DATA(already_down,  "Key already down");
	PRINT_DATA(already_up,    "Key already up");
	PRINT_DATA(valid,         "Valid keys");
	PRINT_DATA(forced_release, "Forced release");
	p += sprintf(p,"%-20s : %d\n", "Usage count", g_keyboard.usage_count);
	p += sprintf(p,"\nRegistered keyboards\n");
	for ( i = 0 ; i < NUM_KEYBOARDS ; i++ ) 
		p += sprintf(p, " %20s (%d) : %s\n", keyboards[i].name, i, keyboards[i].full_name);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

/***********************************************************************************
 *
 *  TODO:  We seem to survive suspend/resume - no doubt due to settings by the serial driver
 *   We might want to not rely on them.
 *
 *   TODO:  Limit ourselves to a single reader at a time.
 *   
 *   TODO:  Have a PROC setting so you don't need a reader to keep alive....
 *          This is the "I want to be myself" mode of operation.
 ***********************************************************************************/

static struct pm_dev *microkbd_pm;

#ifdef CONFIG_PM
static int h3600_microkbd_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	struct mkbd_data *mkbd = (struct mkbd_data *) dev->data;
	SFDEBUG(1,"changing to state %d\n", rqst);
	if ( mkbd->usage_count > 0 ) {
		switch (rqst) {
		case PM_SUSPEND:
			release_all_keys(mkbd);
			break;
		case PM_RESUME:
			mkbd->dev->uart->init( mkbd->dev );
			break;
		}
	}
	return 0;
}
#endif

/***********************************************************************************/
/*   Initialization code                                                           */
/***********************************************************************************/

static int            g_microkbd_major;  /* Dynamic major number */
static devfs_handle_t devfs_dir;


#define H3600_MICROKBD_MODULE_NAME "microkbd"
#define H3600_MICROKBD_PROC_STATS  "microkbd"

int __init h3600_microkbd_init_module( void )
{
	int i;

        SDEBUG(0);
        g_microkbd_major = devfs_register_chrdev(0, H3600_MICROKBD_MODULE_NAME, &microkbd_fops);
        if (g_microkbd_major < 0) {
                printk(KERN_ALERT __FUNCTION__ ": can't get major number\n");
                return g_microkbd_major;
        }

	devfs_dir = devfs_mk_dir(NULL, "microkbd", NULL);
	if ( !devfs_dir ) return -EBUSY;

	for ( i = 0 ; i < NUM_KEYBOARDS ; i++ )
		keyboards[i].devfs = devfs_register( devfs_dir, keyboards[i].name, DEVFS_FL_DEFAULT, 
						     g_microkbd_major, i, S_IFCHR | S_IRUSR | S_IWUSR, &microkbd_fops, NULL );

	/* Register in /proc filesystem */
	create_proc_read_entry(H3600_MICROKBD_PROC_STATS, 0, proc_dir,
			       h3600_microkbd_proc_stats_read, NULL );

#ifdef CONFIG_PM
	microkbd_pm = pm_register(PM_SYS_DEV, PM_SYS_COM, h3600_microkbd_pm_callback);
	if ( microkbd_pm )
		microkbd_pm->data = &g_keyboard;
#endif
	return 0;
}

void __exit h3600_microkbd_cleanup_module( void )
{
	int i;

	SDEBUG(0);
	pm_unregister(microkbd_pm);
	remove_proc_entry(H3600_MICROKBD_PROC_STATS, proc_dir);

	for ( i = 0 ; i < NUM_KEYBOARDS ; i++ )
		devfs_unregister( keyboards[i].devfs );

	devfs_unregister( devfs_dir );
	devfs_unregister_chrdev( g_microkbd_major, H3600_MICROKBD_MODULE_NAME );
}

MODULE_DESCRIPTION("Compaq iPAQ microkeyboard driver");
MODULE_LICENSE("Dual BSD/GPL");

module_init(h3600_microkbd_init_module);
module_exit(h3600_microkbd_cleanup_module);


