/*
 * Palm TC hardware buttons
 * 
 * Author: Holger Bocklet, Bitz.Email@gmx.net,  
 *	   based on research by Chetan Kumar S<shivakumar.chetan@gmail.com>
 *	   with bits and pieces from 
 *		htcuniversal_kbd.c, (Milan Votava)
 *		palmtt3_buttons.c, (Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>, Martin Kupec)
 *		palmte2_keyboard, (Carlos Eduardo Medaglia Dyonisio <cadu@nerdfeliz.com>)
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>

#define USE_RELEASE_TIMER
#define USE_DOUBLECLICK

//#define PALMTC_KEYBOARD_DEBUG

#ifdef PALMTC_KEYBOARD_DEBUG
#define DBG(x...) \
	printk(KERN_INFO "Palmtc keyboard: " x)
#else
#define DBG(x...)	do { } while (0)
#endif

#define TRUE		1
#define FALSE		0
#define PERMANENT	2

#define MAX_ROW	12
#define MAX_COL	4

#define SHIFT_BIT	0x80 
#define PRESSED_BIT	1

#define PALMTC_BLUEKEY	-17
#define PALMTC_REBOOT	-19

#define RELEASE_CHECK_TIME_MS	50
#define DOUBLECLICK_TIME_MS	250

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

//#define GET_KEY_BIT(vkey, bit) ((vkey >> (bit) ) & 0x01)
//#define SET_KEY_BIT(vkey, bit) (vkey |= (1 << (bit)) )
//#define CLEAR_KEY_BIT(vkey, bit) (vkey &= ~(1 << (bit)) )

#ifdef USE_RELEASE_TIMER
static void release_timer_went_off (unsigned long);
static struct timer_list key_release_timer;
#endif

/**********************************************************
 * 		Key-matrix map german
 * GPIO	0	9	10	11	
 * 18	CALEND	CONTACT	MAIL	WEB
 * 19	x	DOWN	OPT	OK
 * 20	POWER
 * 21	TAB	d	h	.	
 * 22	a	c	z	l	
 * 23	q	f	n	o	
 * 24	SHIFT	r	j	EXIT	
 * 25	y	SPACE	u	CR	
 * 26	s	v	m	BS	
 * 27	w	g	k	p	
 * 79	e	t	i	b	
 * 80	UP	LEFT	RIGHT	BLUE	
 **********************************************************/

static u8 alternate_key=FALSE; 
static int col_gpio[MAX_COL] = { 0, 9, 10, 11 };
static u8 row_gpio[MAX_ROW] = {18,19,20,21,22,23,24,25,26,27,79,80};

static struct {  // matrix for real buttons
	int key;
	int alt_key;
	u8 flags;  // for recording last status (lowest) and shift_bit (highest)
	unsigned long tstamp;  // timestamp (jiffies)
	char *desc;
} palmtc_buttons[MAX_ROW][MAX_COL] = {
// Gpio Column+IRQ   
// german keymap
/*18*/{{KEY_F1,KEY_F5,FALSE,0,"CAL/F1"},		//0
	  {KEY_F2,KEY_F6,FALSE,0,"CONTACT/F2"},	//9
	  {KEY_F3,KEY_F7,FALSE,0,"MAIL/F3"},		//10
	  {KEY_F4,KEY_F8,FALSE,0,"WEB/F4"}		},//11  
/*19*/ {{KEY_X,KEY_9,SHIFT_BIT|FALSE,0,"X"},	
	  {KEY_DOWN,KEY_PAGEDOWN,FALSE,0,"down/pgdown"} , 	
	  {KEY_LEFTCTRL,KEY_F9,FALSE,0,"cmd/ctrl/f9"},
	  {KEY_ENTER,-1,FALSE,0,"Select/enter/;"}	},  //1 free
/*20*/ {{KEY_POWER,PALMTC_REBOOT,FALSE,0,"power"},		//0
	  {-1,-1,FALSE,0,"unused 9/20"}, 			//9
	  {-1,-1,FALSE,0,"unused 10/20"},			//10
	  {-1,-1,FALSE,0,"unused 11/20"} 	},		//11
/*21*/ {{KEY_TAB,KEY_BACKSLASH,SHIFT_BIT|FALSE,0,"tab/|"},
	  {KEY_D,KEY_KPPLUS,FALSE,0,"D"},
	  {KEY_H,KEY_MINUS,SHIFT_BIT|FALSE,0,"H/_"},
	  {KEY_DOT,KEY_SEMICOLON,SHIFT_BIT|FALSE,0,"./:"} },
/*22*/ {{KEY_A,KEY_EQUAL,FALSE,0,"A/@"},	// changed to "="
	  {KEY_C,KEY_0,SHIFT_BIT|FALSE,0,"C/)"},
	  {KEY_Z,KEY_6,FALSE,0,"de:Z us:Y"},
	  {KEY_L,KEY_COMMA,SHIFT_BIT|FALSE,0,"L/ß"}		},  // Keyoard shows "ß" but better use "<"
/*23*/ {{KEY_Q,KEY_1,FALSE,0,"Q"},			//0
	  {KEY_F,KEY_KPMINUS,FALSE,0,"F"},		//9
	  {KEY_N,KEY_COMMA,FALSE,0,"N"},		//10
	  {KEY_O,KEY_9,FALSE,0,"O"} 			},//11
/*24*/ {{KEY_LEFTSHIFT,KEY_7,SHIFT_BIT|FALSE,0,"shift/&"},
	  {KEY_R,KEY_4,FALSE,0,"R"},	
	  {KEY_J,KEY_SLASH,FALSE,0,"J"},
	  {KEY_LEFTALT,KEY_F10,FALSE,0,"exit/alt"}	},
/*25*/ {{KEY_Y,KEY_4,SHIFT_BIT|FALSE,0,"de:Y us:Z / $"}, 	// dollar is more usefull
	  {KEY_SPACE,KEY_ESC,FALSE,0,"space/esc"},	
	  {KEY_U,KEY_7,FALSE,0,"U"},
	  {KEY_ENTER,KEY_SEMICOLON,FALSE,0,"enter"}	},
/*26*/ {{KEY_S,KEY_APOSTROPHE,SHIFT_BIT|FALSE,0,"S"}, 
	  {KEY_V,KEY_3,SHIFT_BIT|FALSE,0,"V/#"},	
	  {KEY_M,KEY_SLASH,SHIFT_BIT|FALSE,0,"M/?"},
	  {KEY_BACKSPACE,KEY_DOT,SHIFT_BIT|FALSE,0,"bs/>"}	}, 
/*27*/ {{KEY_W,KEY_2,FALSE,0,"W"},
	  {KEY_G,KEY_KPASTERISK,FALSE,0,"G"},
	  {KEY_K,KEY_APOSTROPHE,FALSE,0,"K"},  
	  {KEY_P,KEY_0,FALSE,0,"P"}			},
/*79*/ {{KEY_E,KEY_3,FALSE,0,"E"},
	  {KEY_T,KEY_5,FALSE,0,"T"},
	  {KEY_I,KEY_8,FALSE,0,"I"},
	  {KEY_B,KEY_1,SHIFT_BIT|FALSE,0,"B/!"} },  
/*80*/ {{KEY_UP,KEY_PAGEUP,FALSE,0,"up"},
	  {KEY_LEFT,KEY_HOME,FALSE,0,"left"},
	  {KEY_RIGHT,KEY_END,FALSE,0,"right"},	
	  {PALMTC_BLUEKEY,-1,FALSE,0,"blue"}		}
};

struct input_dev *keyboard_dev; 

static struct workqueue_struct *palmtc_kbd_workqueue;
static struct work_struct palmtc_kbd_task;

static spinlock_t kbd_lock = SPIN_LOCK_UNLOCKED;

static irqreturn_t palmtc_kbd_irq_handler(int irq, void *dev_id)
{
	queue_work(palmtc_kbd_workqueue, &palmtc_kbd_task);
	return IRQ_HANDLED;
}

static void palmtc_kbd_queuework(struct work_struct *data)
{
	unsigned long tdiff_msec, flags;
	int gpio, row, col;

	// ignore irqs
	for(col=0;col<MAX_COL;col++) { 
	    set_irq_type (IRQ_GPIO(col_gpio[col]), IRQT_NOEDGE);
	}
	// set row-gpios high so we can pull them low later one-by-one in loop
	spin_lock_irqsave(&kbd_lock, flags);
	for (row=0; row<MAX_ROW;row++) {
		GPSR(row_gpio[row]) = GPIO_bit(row_gpio[row]);
	}
	spin_unlock_irqrestore(&kbd_lock, flags);

	// run thru rows, pull row-gpio low and see if what gpio of the colums goes low with it 
	for(row = MAX_ROW-1; row >=0; row--) { // counting backwards, so we get BLUE first
	    spin_lock_irqsave(&kbd_lock, flags);
	    GPCR(row_gpio[row]) = GPIO_bit(row_gpio[row]);
	    spin_unlock_irqrestore(&kbd_lock, flags);

	    udelay(50);

	    for(col = MAX_COL-1; col >= 0; col--) {
	    	spin_lock_irqsave(&kbd_lock, flags);
		gpio = GET_GPIO(col_gpio[col]);
		spin_unlock_irqrestore(&kbd_lock, flags);
		
		tdiff_msec = jiffies_to_msecs((unsigned long)((long)jiffies - 
		    	    (long)palmtc_buttons[row][col].tstamp));

		if(!gpio) {
		    if (! (palmtc_buttons[row][col].flags&PRESSED_BIT)) { //is not pressed at the moment
		    
			palmtc_buttons[row][col].flags|=PRESSED_BIT; 
			palmtc_buttons[row][col].tstamp=jiffies;    
#ifdef USE_DOUBLECLICK
			if ( (tdiff_msec<DOUBLECLICK_TIME_MS) ) {
				if(palmtc_buttons[row][col].alt_key >= 0) { // report "alternate" keys
				    input_report_key(keyboard_dev, KEY_BACKSPACE, 1);
				    input_report_key(keyboard_dev, KEY_BACKSPACE, 0);
				    if (palmtc_buttons[row][col].flags&SHIFT_BIT)
					input_report_key(keyboard_dev,KEY_LEFTSHIFT,1);
				    input_report_key(keyboard_dev, palmtc_buttons[row][col].alt_key, 1);
				    input_sync(keyboard_dev);
				    alternate_key=FALSE; 
				}
			} else {
#endif
			    if (palmtc_buttons[row][col].key==PALMTC_BLUEKEY) {
				switch(alternate_key) {
				    case (PERMANENT):
					alternate_key=FALSE; 
					break;
				    case (TRUE):
					alternate_key=PERMANENT; 
					break;
				    case (FALSE):
					alternate_key=TRUE; 
					break;
				} 
			    } else {
				if (alternate_key) {
				    if (palmtc_buttons[row][col].alt_key==PALMTC_REBOOT) {
					     DBG("reboot\n");
					    //kernel_restart(NULL);
					    kill_proc(1, SIGINT, 1);
					     input_sync(keyboard_dev);
				    } else {
					if(palmtc_buttons[row][col].alt_key >= 0) {
					     if (palmtc_buttons[row][col].flags&SHIFT_BIT)
						input_report_key(keyboard_dev,KEY_LEFTSHIFT,1);
					     input_report_key(keyboard_dev, palmtc_buttons[row][col].alt_key, 1);
					     //DBG("alt: ms:%0lu char: %s\n",palmtc_buttons[row][col].tstamp,palmtc_buttons[row][col].desc); 
					     input_sync(keyboard_dev);
					} 
				    }
				    if(! (alternate_key==PERMANENT)) {
					alternate_key=FALSE;
				    }
				} else { 
				    if(palmtc_buttons[row][col].key >= 0) {
					input_report_key(keyboard_dev, palmtc_buttons[row][col].key, 1);
					input_sync(keyboard_dev);
				    }
				}
			    }
#ifdef USE_DOUBLECLICK	
			}
#endif
		    }
#ifdef USE_RELEASE_TIMER
    		spin_lock_irqsave(&kbd_lock, flags);
		if (timer_pending(&key_release_timer)) {
        	    mod_timer (&key_release_timer,jiffies + msecs_to_jiffies(RELEASE_CHECK_TIME_MS));
		} else {
        	    key_release_timer.expires = (jiffies + msecs_to_jiffies(RELEASE_CHECK_TIME_MS));
        	    add_timer (&key_release_timer);
		}
		spin_unlock_irqrestore(&kbd_lock, flags);
#endif
		} else { // not pressed branch
		    if (palmtc_buttons[row][col].flags&PRESSED_BIT) {
		    
			palmtc_buttons[row][col].flags ^= PRESSED_BIT;  //xor bit to 0
			if (palmtc_buttons[row][col].key >= 0) {
			    input_report_key(keyboard_dev, palmtc_buttons[row][col].key, 0);
			    if ((palmtc_buttons[row][col].alt_key >= 0) && (palmtc_buttons[row][col].flags&SHIFT_BIT) )
				input_report_key(keyboard_dev,KEY_LEFTSHIFT,0);
			    input_report_key(keyboard_dev, palmtc_buttons[row][col].alt_key, 0);
			    //DBG("rel: %s i:%d\n",palmtc_buttons[row][col].desc,i); i=0; 
			    input_sync(keyboard_dev);
			}
		    }
		}
	    }
		// back to high for this row
	    spin_lock_irqsave(&kbd_lock, flags);
	    GPSR(row_gpio[row]) = GPIO_bit(row_gpio[row]);
	    spin_unlock_irqrestore(&kbd_lock, flags);
	}

	// set row-gpios low again
	spin_lock_irqsave(&kbd_lock, flags);
	for (row=0; row<MAX_ROW;row++) {
		GPCR(row_gpio[row]) = GPIO_bit(row_gpio[row]);
	}
	spin_unlock_irqrestore(&kbd_lock, flags);

	// restore irqs (cols) 
	for(col=0;col<MAX_COL;col++) {
	    set_irq_type (IRQ_GPIO(col_gpio[col]), IRQT_BOTHEDGE);
	}
}


#ifdef USE_RELEASE_TIMER
/*
 * This is called when the key release timer goes off.  That's the
 * only time it should be called.  Check for changes in what keys
 * are down.
 */
static void
release_timer_went_off(unsigned long unused)
{
unsigned long flags;

    spin_lock_irqsave(&kbd_lock,flags);

    queue_work(palmtc_kbd_workqueue, &palmtc_kbd_task);

    spin_unlock_irqrestore(&kbd_lock, flags);
}
#endif


static int palmtc_kbd_probe(struct device *dev)
{
	int row, col;
	unsigned long ret;
	//DBG("Probing device\n" );

	keyboard_dev = input_allocate_device();
	
	keyboard_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);

	keyboard_dev->name = "Palm Tungsten C Keyboard";
	keyboard_dev->id.bustype = BUS_HOST;

	for(row=0;row<MAX_ROW;row++) {
	    for(col=0;col<MAX_COL;col++) {
		if(palmtc_buttons[row][col].key >= 0) {
		    keyboard_dev->keybit[LONG(palmtc_buttons[row][col].key)] |=
				BIT(palmtc_buttons[row][col].key);
		}
		if (palmtc_buttons[row][col].alt_key >= 0) {
		    keyboard_dev->keybit[LONG(palmtc_buttons[row][col].alt_key)] |=
				BIT(palmtc_buttons[row][col].alt_key);
		}
	    }
	}
	input_register_device(keyboard_dev);

	palmtc_kbd_workqueue = create_workqueue("palmtckbdw");
	INIT_WORK(&palmtc_kbd_task, palmtc_kbd_queuework);

#ifdef USE_RELEASE_TIMER
	init_timer (&key_release_timer);
        key_release_timer.function = release_timer_went_off;
#endif

	for(col=0;col<MAX_COL;col++) {
	    ret = request_irq(IRQ_GPIO(col_gpio[col]), palmtc_kbd_irq_handler,
		SA_SAMPLE_RANDOM,"palmtc-kbd", (void*)col_gpio[col]);
		set_irq_type(IRQ_GPIO(col_gpio[col]), IRQT_BOTHEDGE);
	}
        DBG("device enabled\n");
	return 0;
}

static int palmtc_kbd_remove (struct device *dev)
{
    int col;
    
        DBG("removing device...\n");
	destroy_workqueue(palmtc_kbd_workqueue);
	input_unregister_device(keyboard_dev);

	/* free irqs */
	for(col=0;col<MAX_COL;col++) {
	    free_irq(IRQ_GPIO(col_gpio[col]), (void*) col_gpio[col]);
	}

#ifdef USE_RELEASE_TIMER
        del_timer (&key_release_timer);
#endif

        return 0;
}

static struct device_driver palmtc_keyboard_driver = {
	.name           = "palmtc-kbd",
	.bus            = &platform_bus_type,
	.probe          = palmtc_kbd_probe,
        .remove		= palmtc_kbd_remove,
#ifdef CONFIG_PM
	.suspend        = NULL,
	.resume         = NULL,
#endif
};

static int __init palmtc_kbd_init(void)
{
	DBG("init\n");

	return driver_register(&palmtc_keyboard_driver);
}

static void __exit palmtc_kbd_cleanup(void)
{
        DBG("unloading...\n");

	driver_unregister(&palmtc_keyboard_driver);
}

module_init(palmtc_kbd_init);
module_exit(palmtc_kbd_cleanup);

MODULE_AUTHOR("Holger Bocklet");
MODULE_DESCRIPTION("Support for Palm TC Keyboard");
MODULE_LICENSE("GPL");
