// sj
// joshua's send xmodem
// an extremely small implementation

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/lab/lab.h>
#include "crc.h"

#define SOH (0x01)
#define STX (0x02)
#define EOT (0x04)
#define ACK (0x06)
#define NAK (0x15)
#define CAN (0x18)
#define CRC (0x43)
#define GRC (0x47)

int lab_xmodem_init(void)
{
	return 0;
}
module_init(lab_xmodem_init);

static inline unsigned char getchar(void)
{
	return lab_getc_seconds(NULL, 2);
}

static inline void eatbytes(void)
{
	while (getchar());
}

void lab_xmodem_send(unsigned char* buf, int size)
{
	int stop;
	int block;
	unsigned char gotch;
	int pktsize;
	int length;
	int crc;
	int i;
	int usecrc;
	unsigned char tbuf[1024];
	
	stop = 0;
	block = 1;
	usecrc = 0;
	length = size;
	while (!stop)
	{
		gotch = getchar();
		switch (gotch)
		{
		case CRC:
			if (block != 1) {
				lab_putc(NAK);
				continue;
			}
			length = size;
			usecrc = 1;
			goto skipcheck;
		case ACK:
			if (block == 1) {
				lab_putc(NAK);
				continue;
			}
skipcheck:
			if (length == 0) {
eotagain:			lab_putc(EOT);
				if (getchar() != ACK)
					goto eotagain;
				stop = 1;
				continue;
			}
		
			pktsize = (length > 128) ? 128 : length;
			length -= pktsize;
			
			tbuf[0] = SOH;
			tbuf[1] = (block & 0xff);
			tbuf[2] = ~(block & 0xff);
			memcpy(tbuf+3, buf, pktsize);
			buf += pktsize;
			crc = 0;
			for (i=0; i<pktsize; i++)
				if (usecrc)
					crc = crc16_table[((crc >> 8) ^ tbuf[i+3]) & 0xFF] ^ (crc << 8);
				else
					crc = (crc + tbuf[i+3]) & 0xFF;
			for (; i<128; i++) {
				tbuf[i+3] = 0x1A; /* CTRL-Z */
				if (usecrc)
					crc = crc16_table[((crc >> 8) ^ 0x1A) & 0xFF] ^ (crc << 8);
				else
					crc = (crc + 0x1A) & 0xFF;
			}
			if (usecrc) {
				tbuf[131] = (crc >> 8) & 0xFF;
				tbuf[132] = crc & 0xFF;
				lab_putsn(tbuf, 133);
			} else {
				tbuf[131] = crc & 0xFF;
				lab_putsn(tbuf, 132);
			}
			block++;
			
			break;
		default:
			if (block == 1)	// we need an explicit NAK
				break;	// in order to start a send;
		case NAK:
			if (block == 1) {
				length = size;
				usecrc = 0;
				goto skipcheck;
			}
			/* A neat trick here is that we can use the old contents of tbuf. */
			lab_putsn(tbuf, 132+usecrc);
			break;
		case CAN:
			lab_puts("Cancelled\r\n");
			stop = 1;
			break;
		};
	};
};
EXPORT_SYMBOL(lab_xmodem_send);
