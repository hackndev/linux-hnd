/* some definition of intercomunication structures for micro and it's sub devices */

#ifndef _MICRO_H_
#define _MICRO_H_

#include <linux/spinlock.h>


#define TX_BUF_SIZE	32
#define RX_BUF_SIZE	16
#define CHAR_SOF	0x02

extern struct platform_device h3600micro_device;

typedef struct {
	spinlock_t lock;
	void (*h_key)( int len, unsigned char *data );
	void (*h_batt)( int len, unsigned char *data );
	void (*h_temp)( int len, unsigned char *data );
	void (*h_ts)( int len, unsigned char *data );
} micro_private_t;

int h3600_micro_tx_msg(unsigned char id, unsigned char len, unsigned char *data);

#endif /* _MICRO_H_ */
