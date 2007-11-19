// jrxy
// joshua's receive xmodem/ymodem
// an extremely small portable implementation

#define SOH (0x01)
#define STX (0x02)
#define EOT (0x04)
#define ACK (0x06)
#define NAK (0x15)
#define CAN (0x18)
#define CRC (0x43)
#define GRC (0x47)

#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/lab/lab.h>
#include "crc.h"

int lab_ymodem_init(void)
{
	return 0;
}

static inline unsigned char getchar(void)
{
	return lab_getc_seconds(NULL, 2);
}

static inline void eatbytes(void)
{
	while (getchar());
}

char* lab_ymodem_receive(int* length, int ymodemg)
{
	int stop;
	int firstblk;
	unsigned char gotch;
	int pktsize;
	int size = 0;
	char* rxbuf = NULL;
	int rbytes;
	int waitpkts;
		
	stop = 0;
	firstblk = 1;
	lab_delay(6);
	rbytes = 0;
	waitpkts = 16;
	
	while (!stop)
	{
		if (firstblk)
		{
			if (ymodemg)
				lab_putc(GRC);
			else
				lab_putc(CRC);
		}
		
		gotch = getchar();
		if (gotch == 0)		// timeout
		{
			waitpkts--;
			if (!waitpkts)
			{
				printk("WARNING: YMODEM receive timed out!\n");
				if (rxbuf)
					vfree(rxbuf);
				return NULL;
			}
			
			continue;
		}
		
		switch (gotch)
		{
		case SOH:
			pktsize = 128;
			goto havesize;
		case STX:
			pktsize = 1024;
havesize:
		{
			unsigned char blk;
			unsigned char blk255;
			unsigned char dbytes[1028];
			int i;
			
			blk = getchar();
			blk255 = getchar();
			for (i=0; i<pktsize+2; i++)
				dbytes[i] = getchar();
			
			if (crc16_buf(dbytes, pktsize+2))
			{
				/* CRC failed, try again */
				lab_putc(NAK);
				break;
			}
			
			if (blk255 != (255-blk))
			{
				lab_putc(NAK);
				break;
			}
			
			if (firstblk)
			{
				int i;
				char* buf;
				buf = dbytes + strlen(dbytes) + 1;
				i = 0;
				size = 0;
				while (*buf >= '0' && *buf <= '9')
				{
					size *= 10;
					size += *buf - '0';
					buf++;
				}
				
				*length = size;
				size += 1024;
				rxbuf = vmalloc(size+1024); // a little more safety...
				// printk(">>> YMODEM: getting file of size %d (buf addr: %08X)\n", size-1024, rxbuf);
				
				lab_putc(ACK);
				if (ymodemg)
					lab_putc(GRC);
				else
					lab_putc(CRC);
				
				firstblk = 0;
				break;
			}
			
			if ((rbytes + pktsize) > size)
			{
				lab_putc(CAN);
				lab_putc(CAN);
				/* BUFFER OVERRUN!!! */
				stop = 1;
				break;
			}
			
			memcpy(rxbuf+rbytes, dbytes, pktsize);
			rbytes += pktsize;
			
			if (!ymodemg)
				lab_putc(ACK);
			
			break;
		}
		case EOT:
			lab_putc(ACK);
			lab_delay(1);
			lab_putc(CRC);
			lab_delay(1);
			lab_putc(ACK);
			lab_delay(1);
			lab_putc(CAN);
			lab_putc(CAN);
			lab_putc(CAN);
			lab_putc(CAN);
			lab_puts("\x08\x08\x08\x08    \x08\x08\x08\x08");
			
			eatbytes();
			
			stop = 1;
			
			break;
		case CAN:
			lab_putc(CAN);
			lab_putc(CAN);
			lab_putc(CAN);
			lab_putc(CAN);
			lab_puts("\x08\x08\x08\x08    \x08\x08\x08\x08");
			stop = 1;
			
			lab_delay(1);
			lab_puts("YMODEM transfer aborted\r\n");
			
			if (rxbuf)
				vfree(rxbuf);
			
			rxbuf = NULL;
			
			eatbytes();
			
			break;
		case 0x03:
		case 0xFF:
			/* Control-C. We should NAK it if it was line noise,
			 * but it's more likely to be the user banging on the
			 * keyboard trying to abort a screwup.
			 */
			lab_putc(CAN);
			lab_putc(CAN);
			lab_putc(CAN);
			lab_putc(CAN);
			lab_puts("\x08\x08\x08\x08    \x08\x08\x08\x08");
			
			eatbytes();
			
			if (rxbuf)
				vfree(rxbuf);
				
			rxbuf = NULL;
			
			stop=1;
			break;
		default:
			lab_putc(NAK);
		}
	}
	
	return rxbuf;
}
EXPORT_SYMBOL(lab_ymodem_receive);

module_init(lab_ymodem_init);
