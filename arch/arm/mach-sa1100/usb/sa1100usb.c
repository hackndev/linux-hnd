#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/usb/ch9.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>

#include "buffer.h"
#include "usbdev.h"
#include "sa1100_usb.h"
#include "sa1100usb.h"

#ifdef DEBUG
#define DPRINTK(fmt, args...) printk( fmt , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

int udc_ep1_queue_buffer(struct sausb_dev *usb, char *buf, unsigned int len);
int udc_ep2_send(struct sausb_dev *usb, char *buf, int len);
void udc_ep2_send_reset(struct sausb_dev *usb);
int udc_ep2_idle(struct sausb_dev *usb);
int usbctl_init(struct usbctl *ctl, struct usbc_driver *drv);
void usbctl_exit(struct usbctl *ctl);
int usbctl_proc_info(struct usbctl *ctl, char *buf);
void udc_ep1_recv_reset(struct sausb_dev *usb);

static inline void pcs(const char *prefix)
{
#ifdef DEBUG
	__u32 foo = Ser0UDCCS0;

	DPRINTK("%s UDCAR: %d\n", prefix, Ser0UDCAR);

	printk("UDC: %s: %08x [ %s%s%s%s%s%s]\n", prefix,
	       foo,
	       foo & UDCCS0_SE ? "SE " : "",
	       foo & UDCCS0_DE ? "DE " : "",
	       foo & UDCCS0_FST ? "FST " : "",
	       foo & UDCCS0_SST ? "SST " : "",
	       foo & UDCCS0_IPR ? "IPR " : "",
	       foo & UDCCS0_OPR ? "OPR " : "");
#endif
}

/*
 * soft_connect_hook()
 *
 * Some devices have platform-specific circuitry to make USB
 * not seem to be plugged in, even when it is. This allows
 * software to control when a device 'appears' on the USB bus
 * (after Linux has booted and this driver has loaded, for
 * example). If you have such a circuit, control it here.
 */
static inline void soft_connect_hook(int enable)
{
#ifdef CONFIG_SA1100_EXTENEX1
	if (machine_is_extenex1()) {
		if (enable) {
			PPDR |= PPC_USB_SOFT_CON;
			PPSR |= PPC_USB_SOFT_CON;
		} else {
			PPSR &= ~PPC_USB_SOFT_CON;
			PPDR &= ~PPC_USB_SOFT_CON;
		}
	}
#endif
}

/*
 * disable the UDC at the source
 */
static inline void udc_disable(struct sausb_dev *usb)
{
	soft_connect_hook(0);

	usb->udccr = UDCCR_UDD | UDCCR_SUSIM;

	UDC_write(Ser0UDCCR, usb->udccr);
}

/*
 * Clear any pending write from the EP0 write buffer.
 */
static void ep0_clear_write(struct sausb_dev *usb)
{
	struct usb_buf *buf;

	buf = usb->wrbuf;
	usb->wrint = NULL;
	usb->wrbuf = NULL;
	usb->wrptr = NULL;
	usb->wrlen = 0;

	if (buf)
		usbb_put(buf);
}

static int udc_start(void *priv)
{
	struct sausb_dev *usb = priv;

	usb->ep[0].maxpktsize = 0;
	usb->ep[1].maxpktsize = 0;

	/*
	 * start UDC internal machinery running, but mask interrupts.
	 */
	usb->udccr = UDCCR_SUSIM | UDCCR_TIM | UDCCR_RIM | UDCCR_EIM |
		     UDCCR_RESIM;
	UDC_write(Ser0UDCCR, usb->udccr);

	udelay(100);

	/*
	 * clear all interrupt sources
	 */
	Ser0UDCSR = UDCSR_RSTIR | UDCSR_RESIR | UDCSR_EIR |
		    UDCSR_RIR | UDCSR_TIR | UDCSR_SUSIR;

	/*
	 * flush DMA and fire through some -EAGAINs
	 */
	udc_ep1_init(usb);
	udc_ep2_init(usb);

	/*
	 * enable any platform specific hardware
	 */
	soft_connect_hook(1);

	/*
	 * Enable resume, suspend and endpoint 0 interrupts.  Leave
	 * endpoint 1 and 2 interrupts masked.
	 *
	 * If you are unplugged you will immediately get a suspend
	 * interrupt.  If you are plugged and have a soft connect-circuit,
	 * you will get a reset.  If you are plugged without a soft-connect,
	 * I think you also get suspend.
	 */
	usb->udccr &= ~(UDCCR_SUSIM | UDCCR_EIM | UDCCR_RESIM);
	UDC_write(Ser0UDCCR, usb->udccr);

	return 0;
}

static int udc_stop(void *priv)
{
	struct sausb_dev *usb = priv;

	ep0_clear_write(usb);

	/* mask everything */
	Ser0UDCCR = 0xFC;

	udc_ep1_reset(usb);
	udc_ep2_reset(usb);

	udc_disable(usb);

	return 0;
}





/*
 * some voodo I am adding, since the vanilla macros just aren't doing it
 * 1Mar01ww
 */

#define ABORT_BITS	(UDCCS0_SST | UDCCS0_SE)
#define OK_TO_WRITE	(!(Ser0UDCCS0 & ABORT_BITS))
#define BOTH_BITS	(UDCCS0_IPR | UDCCS0_DE)

static void set_de(void)
{
	int i = 1;

	while (1) {
		if (OK_TO_WRITE) {
			Ser0UDCCS0 |= UDCCS0_DE;
		} else {
			DPRINTK("UDC: quitting set DE because SST or SE set\n");
			break;
		}
		if (Ser0UDCCS0 & UDCCS0_DE)
			break;
		udelay(i);
		if (++i == 50) {
			printk("UDC: Dangnabbbit! Cannot set DE! (DE=%8.8X CCS0=%8.8X)\n",
			       UDCCS0_DE, Ser0UDCCS0);
			break;
		}
	}
}

static void set_ipr(void)
{
	int i = 1;

	while (1) {
		if (OK_TO_WRITE) {
			Ser0UDCCS0 |= UDCCS0_IPR;
		} else {
			DPRINTK("UDC: Quitting set IPR because SST or SE set\n");
			break;
		}
		if (Ser0UDCCS0 & UDCCS0_IPR)
			break;
		udelay(i);
		if (++i == 50) {
			printk("UDC: Dangnabbbit! Cannot set IPR! (IPR=%8.8X CCS0=%8.8X)\n",
			       UDCCS0_IPR, Ser0UDCCS0);
			break;
		}
	}
}

static void set_ipr_and_de(void)
{
	int i = 1;

	while (1) {
		if (OK_TO_WRITE) {
			Ser0UDCCS0 |= BOTH_BITS;
		} else {
			DPRINTK("UDC: Quitting set IPR/DE because SST or SE set\n");
			break;
		}
		if ((Ser0UDCCS0 & BOTH_BITS) == BOTH_BITS)
			break;
		udelay(i);
		if (++i == 50) {
			printk("UDC: Dangnabbbit! Cannot set DE/IPR! (DE=%8.8X IPR=%8.8X CCS0=%8.8X)\n",
			       UDCCS0_DE, UDCCS0_IPR, Ser0UDCCS0);
			break;
		}
	}
}

static inline void set_cs_bits(__u32 bits)
{
	if (bits & (UDCCS0_SO | UDCCS0_SSE | UDCCS0_FST | UDCCS0_SST))
		Ser0UDCCS0 = bits;
	else if ((bits & BOTH_BITS) == BOTH_BITS)
		set_ipr_and_de();
	else if (bits & UDCCS0_IPR)
		set_ipr();
	else if (bits & UDCCS0_DE)
		set_de();
}

/*
 * udc_ep0_write_fifo()
 *
 * Stick bytes in the 8 bytes endpoint zero FIFO.  This version uses a
 * variety of tricks to make sure the bytes are written correctly:
 *  1. The count register is checked to see if the byte went in,
 *     and the write is attempted again if not.
 *  2. An overall counter is used to break out so we don't hang in
 *     those (rare) cases where the UDC reverses direction of the
 *     FIFO underneath us without notification (in response to host
 *     aborting a setup transaction early).
 */
static void udc_ep0_write_fifo(struct sausb_dev *usb)
{
	unsigned int bytes_this_time = min(usb->wrlen, 8U);
	int bytes_written = 0;

	DPRINTK("WF=%d: ", bytes_this_time);

	while (bytes_this_time--) {
		unsigned int cwc;
		int i;

		DPRINTK("%2.2X ", *usb->wrptr);

		cwc = Ser0UDCWC & 15;

		i = 10;
		do {
			Ser0UDCD0 = *usb->wrptr;
			udelay(20);	/* voodo 28Feb01ww */
		} while ((Ser0UDCWC & 15) == cwc && --i);

		if (i == 0) {
			printk("UDC: udc_ep0_write_fifo: write failure\n");
			usb->ep0_wr_fifo_errs++;
		}

		usb->wrptr++;
		bytes_written++;
	}
	usb->wrlen -= bytes_written;

	/* following propagation voodo so maybe caller writing IPR in
	   ..a moment might actually get it to stick 28Feb01ww */
	udelay(300);

	usb->ep0_wr_bytes += bytes_written;
	DPRINTK("L=%d WCR=%8.8X\n", usb->wrlen, Ser0UDCWC);
}

/*
 * read_fifo()
 *
 * Read 1-8 bytes out of FIFO and put in request.  Called to do the
 * initial read of setup requests from the host. Return number of
 * bytes read.
 *
 * Like write fifo above, this driver uses multiple reads checked
 * against the count register with an overall timeout.
 */
static int
udc_ep0_read_fifo(struct sausb_dev *usb, struct usb_ctrlrequest *request, int sz)
{
	unsigned char *pOut = (unsigned char *) request;
	unsigned int fifo_count, bytes_read = 0;

	fifo_count = Ser0UDCWC & 15;

	DPRINTK("RF=%d ", fifo_count);
	BUG_ON(fifo_count > sz);

	while (fifo_count--) {
		unsigned int cwc;
		int i;

		cwc = Ser0UDCWC & 15;

		i = 10;
		do {
			*pOut = (unsigned char) Ser0UDCD0;
			udelay(20);
		} while ((Ser0UDCWC & 15) == cwc && --i);

		if (i == 0) {
			printk(KERN_ERR "UDC: udc_ep0_read_fifo: read failure\n");
			usb->ep0_rd_fifo_errs++;
			break;
		}
		pOut++;
		bytes_read++;
	}

	DPRINTK("fc=%d\n", bytes_read);
	usb->ep0_rd_bytes += bytes_read;
	usb->ep0_rd_packets ++;
	return bytes_read;
}

static void ep0_sh_write_data(struct sausb_dev *usb)
{
	/*
	 * If bytes left is zero, we are coming in on the
	 * interrupt after the last packet went out. And
	 * we know we don't have to empty packet this
	 * transfer so just set DE and we are done
	 */
	set_cs_bits(UDCCS0_DE);
}

static void ep0_sh_write_with_empty_packet(struct sausb_dev *usb)
{
	/*
	 * If bytes left is zero, we are coming in on the
	 * interrupt after the last packet went out.
	 * We must do short packet suff, so set DE and IPR
	 */
	set_cs_bits(UDCCS0_IPR | UDCCS0_DE);
	DPRINTK("UDC: sh_write_empty: Sent empty packet\n");
}

static int udc_clear_opr(void)
{
	int i = 10000;
	int is_clear;

	/*FIXME*/
	do {
		Ser0UDCCS0 = UDCCS0_SO;
		is_clear = !(Ser0UDCCS0 & UDCCS0_OPR);
		if (i-- <= 0)
			break;
	} while (!is_clear);

	return is_clear;
}

static int udc_ep0_queue(void *priv, struct usb_buf *buf,
			 unsigned int req_len)
{
	struct sausb_dev *usb = priv;
	__u32 cs_reg_bits = UDCCS0_IPR;

	DPRINTK("a=%d r=%d\n", buf->len, req_len);

	/*
	 * thou shalt not enter data phase until
	 * Out Packet Ready is clear
	 */
	if (!udc_clear_opr()) {
		printk("UDC: SO did not clear OPR\n");
		set_cs_bits(UDCCS0_DE | UDCCS0_SO);
		usbb_put(buf);
		return 1;
	}

	usb->ep0_wr_packets++;

	usb->wrbuf = buf;
	usb->wrptr = buf->data;
	usb->wrlen = min(buf->len, req_len);

	udc_ep0_write_fifo(usb);

	if (usb->wrlen == 0) {
		/*
		 * out in one, so data end
		 */
		cs_reg_bits |= UDCCS0_DE;
		ep0_clear_write(usb);
	} else if (buf->len < req_len) {
		/*
		 * we are going to short-change host
		 * so need nul to not stall
		 */
		usb->wrint = ep0_sh_write_with_empty_packet;
	} else {
		/*
		 * we have as much or more than requested
		 */
		usb->wrint = ep0_sh_write_data;
	}

	/*
	 * note: IPR was set uncondtionally at start of routine
	 */
	set_cs_bits(cs_reg_bits);
	return 0;
}

/*
 * When SO and DE sent, UDC will enter status phase and ack, propagating
 * new address to udc core.  Next control transfer will be on the new
 * address.
 *
 * You can't see the change in a read back of CAR until then (about 250us
 * later, on my box).  The original Intel driver sets S0 and DE and code
 * to check that address has propagated here.  I tried this, but it would
 * only sometimes work!  The rest of the time it would never propagate and
 * we'd spin forever.  So now I just set it and pray...
 */
static void udc_set_address(void *priv, unsigned int addr)
{
	Ser0UDCAR = addr;
}

static void udc_set_config(void *priv, struct cdb *cdb)
{
	struct sausb_dev *usb = priv;

	if (cdb) {
		udc_ep1_config(usb, le16_to_cpu(cdb->ep1.wMaxPacketSize));
		udc_ep2_config(usb, le16_to_cpu(cdb->ep2.wMaxPacketSize));
	} else {
		udc_ep1_reset(usb);
		udc_ep2_reset(usb);
	}
}

static unsigned int udc_ep_get_status(void *priv, unsigned int ep)
{
	unsigned int status;

	switch (ep) {
	case 0:
		status = (Ser0UDCCS0 & UDCCS0_FST) ? 1 : 0;
		break;

	case 1:
		status = (Ser0UDCCS1 & UDCCS1_FST) ? 1 : 0;
		break;

	case 2:
		status = (Ser0UDCCS2 & UDCCS2_FST) ? 1 : 0;
		break;

	default:
		printk(KERN_ERR "UDC: get_status: bad end point %d\n", ep);
		status = 0;
		break;
	}

	return status;
}

static void udc_ep_halt(void *priv, unsigned int ep, int halt)
{
	struct sausb_dev *usb = priv;

	printk("UDC: ep%d %s halt\n", ep, halt ? "set" : "clear");

	switch (ep) {
	case 1:
		udc_ep1_halt(usb, halt);
		break;

	case 2:
		udc_ep2_halt(usb, halt);
		break;
	}
}

static int udc_ep_queue(void *priv, unsigned int ep, char *buf, unsigned int len)
{
	struct sausb_dev *usb = priv;
	int ret = -EINVAL;

	switch (ep) {
	case 1:
		ret = udc_ep1_queue_buffer(usb, buf, len);
		break;
	case 2:
		ret = udc_ep2_send(usb, buf, len);
		break;
	}

	return ret;
}

static void udc_ep_reset(void *priv, unsigned int ep)
{
	struct sausb_dev *usb = priv;

	switch (ep) {
	case 1:
		udc_ep1_recv_reset(usb);
		break;
	case 2:
		udc_ep2_send_reset(usb);
		break;
	}
}

static void udc_ep_callback(void *priv, unsigned int ep, usb_callback_t cb, void *data)
{
	struct sausb_dev *usb = priv;
	unsigned long flags;

	if (ep == 1 || ep == 2) {
		ep -= 1;

		spin_lock_irqsave(&usb->lock, flags);
		usb->ep[ep].cb_func = cb;
		usb->ep[ep].cb_data = data;
		spin_unlock_irqrestore(&usb->lock, flags);
	}
}

static int udc_ep_idle(void *priv, unsigned int ep)
{
	struct sausb_dev *usb = priv;
	int ret = -EINVAL;

	switch (ep) {
	case 1:
		break;
	case 2:
		ret = udc_ep2_idle(usb);
		break;
	}

	return ret;
}

static struct usbc_driver usb_sa1100_drv = {
	.owner		= THIS_MODULE,
	.name		= "SA1100",
	.start		= udc_start,
	.stop		= udc_stop,
	.ep0_queue	= udc_ep0_queue,
	.set_address	= udc_set_address,
	.set_config	= udc_set_config,
	.ep_get_status	= udc_ep_get_status,
	.ep_halt	= udc_ep_halt,
	.ep_queue	= udc_ep_queue,
	.ep_reset	= udc_ep_reset,
	.ep_callback	= udc_ep_callback,
	.ep_idle	= udc_ep_idle,
};


/*
 * udc_ep0_read_packet()
 *
 * This setup handler is the "idle" state of endpoint zero. It looks for
 * OPR (OUT packet ready) to see if a setup request has been been received
 * from the host. Requests without a return data phase are immediately
 * handled. Otherwise, the handler may be set to one of the sh_write_xxxx
 * data pumpers if more than 8 bytes need to get back to the host.
 */
static void udc_ep0_read_packet(struct sausb_dev *usb, u32 cs_reg_in)
{
	struct usb_ctrlrequest req;
	int n, ret = RET_NOACTION;

	/*
	 * A control request has been received by EP0.
	 * Read the request.
	 */
	n = udc_ep0_read_fifo(usb, &req, sizeof(req));

	if (n == sizeof(req)) {
		ret = usbctl_parse_request(usb->ctl, &req);
	} else {
		/*
		 * The request wasn't fully received.  Force a
		 * stall.
		 */
		set_cs_bits(UDCCS0_FST | UDCCS0_SO);
		printk("UDC: fifo read error: wanted %d bytes got %d\n",
		       sizeof(req), n);
	}

	switch (ret) {
	case RET_ERROR:
	case RET_NOACTION:
		break;

	case RET_ACK:
		set_cs_bits(UDCCS0_DE | UDCCS0_SO);
		break;

	case RET_REQERROR:
		/*
		 * Send stall PID to host.
		 */
		set_cs_bits(UDCCS0_DE | UDCCS0_SO | UDCCS0_FST);
		break;
	}
}

/*
 * HACK DEBUG  3Mar01ww
 * Well, maybe not, it really seems to help!  08Mar01ww
 */
static void core_kicker(struct sausb_dev *usb)
{
	__u32 car = Ser0UDCAR;
	__u32 imp = Ser0UDCIMP;
	__u32 omp = Ser0UDCOMP;

	UDC_set(Ser0UDCCR, UDCCR_UDD);
	udelay(300);
	UDC_clear(Ser0UDCCR, UDCCR_UDD);

	Ser0UDCAR = car;
	Ser0UDCIMP = imp;
	Ser0UDCOMP = omp;
}

static void enable_resume_mask_suspend(struct sausb_dev *usb)
{
	int i;

	usb->udccr |= UDCCR_SUSIM;

	i = 1;
	do {
		Ser0UDCCR = usb->udccr;
		udelay(i);
		if (Ser0UDCCR == usb->udccr)
			break;
		if (Ser0UDCSR & UDCSR_RSTIR)
			break;
	} while (i++ < 50);

	if (i == 50)
		printk("UDC: enable_resume: could not set SUSIM 0x%08x\n",
		       Ser0UDCCR);

	usb->udccr &= ~UDCCR_RESIM;

	i = 1;	
	do {
		Ser0UDCCR = usb->udccr;
		udelay(i);
		if (Ser0UDCCR == usb->udccr)
			break;
		if (Ser0UDCSR & UDCSR_RSTIR)
			break;
	} while (i++ < 50);

	if (i == 50)
		printk("UDC: enable_resume: could not clear RESIM 0x%08x\n",
		       Ser0UDCCR);
}

static void enable_suspend_mask_resume(struct sausb_dev *usb)
{
	int i;

	usb->udccr |= UDCCR_RESIM;

	i = 1;
	do {
		Ser0UDCCR = usb->udccr;
		udelay(i);
		if (Ser0UDCCR == usb->udccr)
			break;
		if (Ser0UDCSR & UDCSR_RSTIR)
			break;
	} while (i++ < 50);

	if (i == 50)
		printk("UDC: enable_resume: could not set RESIM 0x%08x\n",
		       Ser0UDCCR);

	usb->udccr &= ~UDCCR_SUSIM;

	i = 1;	
	do {
		Ser0UDCCR = usb->udccr;
		udelay(i);
		if (Ser0UDCCR == usb->udccr)
			break;
		if (Ser0UDCSR & UDCSR_RSTIR)
			break;
	} while (i++ < 50);

	if (i == 50)
		printk("UDC: enable_resume: could not clear SUSIM 0x%08x\n",
		       Ser0UDCCR);
}

/*
 * Reset received from HUB (or controller just went nuts and reset by
 * itself!) so UDC core has been reset, track this state here
 */
static void udc_reset(struct sausb_dev *usb)
{
	if (usbctl_reset(usb->ctl)) {
		ep0_clear_write(usb);

		/*
		 * Clean up endpoints.
		 */
		udc_ep1_reset(usb);
		udc_ep2_reset(usb);
	}

	/*
	 * mask reset ints, they flood during sequence, enable
	 * suspend and resume
	 */
	usb->udccr = (usb->udccr & ~(UDCCR_SUSIM | UDCCR_RESIM)) | UDCCR_REM;
	Ser0UDCCR = usb->udccr;
}

/*
 * handle interrupt for endpoint zero
 */
static void udc_ep0_int_hndlr(struct sausb_dev *usb)
{
	u32 cs_reg_in;

	pcs("-->");

	cs_reg_in = Ser0UDCCS0;

	/*
	 * If "setup end" has been set, the usb controller has terminated
	 * a setup transaction before we set DE. This happens during
	 * enumeration with some hosts. For example, the host will ask for
	 * our device descriptor and specify a return of 64 bytes. When we
	 * hand back the first 8, the host will know our max packet size
	 * and turn around and issue a new setup immediately. This causes
	 * the UDC to auto-ack the new setup and set SE. We must then
	 * "unload" (process) the new setup, which is what will happen
	 * after this preamble is finished executing.
	 */
	if (cs_reg_in & UDCCS0_SE) {
		DPRINTK("UDC: early termination of setup\n");

		/*
		 * Clear setup end
		 */
		set_cs_bits(UDCCS0_SSE);

		/*
		 * Clear any pending write.
		 */
		ep0_clear_write(usb);
	}

	/*
	 * UDC sent a stall due to a protocol violation.
	 */
	if (cs_reg_in & UDCCS0_SST) {
		usb->ep0_stall_sent++;

		DPRINTK("UDC: write_preamble: UDC sent stall\n");

		/*
		 * Clear sent stall
		 */
		set_cs_bits(UDCCS0_SST);

		/*
		 * Clear any pending write.
		 */
		ep0_clear_write(usb);
	}

	switch (cs_reg_in & (UDCCS0_OPR | UDCCS0_IPR)) {
	case UDCCS0_OPR | UDCCS0_IPR:
		DPRINTK("UDC: write_preamble: see OPR. Stopping write to "
			"handle new SETUP\n");

		/*
		 * very rarely, you can get OPR and
		 * leftover IPR. Try to clear
		 */
		UDC_clear(Ser0UDCCS0, UDCCS0_IPR);

		/*
		 * Clear any pending write.
		 */
		ep0_clear_write(usb);

		/*FALLTHROUGH*/
	case UDCCS0_OPR:
		/*
		 * A new setup request is pending.  Handle
		 * it. Note that we don't try to read a
		 * packet if SE was set and OPR is clear.
		 */
		udc_ep0_read_packet(usb, cs_reg_in);
		break;

	case 0:
		if (usb->wrint) {
			if (usb->wrlen != 0) {
				/*
				 * More data to go
				 */
				udc_ep0_write_fifo(usb);
				set_ipr();
			}

			if (usb->wrlen == 0) {
				/*
				 * All data sent.
				 */
				usb->wrint(usb);

				ep0_clear_write(usb);
			}
		}
		break;

	case UDCCS0_IPR:
		DPRINTK("UDC: IPR set, not writing\n");
		usb->ep0_early_irqs++;
		break;
	}

	pcs("<--");
}

static irqreturn_t udc_interrupt(int irq, void *dev_id)
{
	struct sausb_dev *usb = dev_id;
	u32 status = Ser0UDCSR;

	/*
	 * ReSeT Interrupt Request - UDC has been reset
	 */
	if (status & UDCSR_RSTIR) {
		udc_reset(usb);

		/*
		 * clear all pending sources
		 */
		UDC_flip(Ser0UDCSR, status);
		return IRQ_HANDLED;
	}

	/*
	 * else we have done something other than reset,
	 * so be sure reset enabled
	 */
	usb->udccr &= ~UDCCR_REM;
	UDC_write(Ser0UDCCR, usb->udccr);

	/*
	 * RESume Interrupt Request
	 */
	if (status & UDCSR_RESIR) {
		usbctl_resume(usb->ctl);
		core_kicker(usb);
		enable_suspend_mask_resume(usb);
	}

	/*
	 * SUSpend Interrupt Request
	 */
	if (status & UDCSR_SUSIR) {
		usbctl_suspend(usb->ctl);
		enable_resume_mask_suspend(usb);
	}

	/*
	 * clear all pending sources
	 */
	UDC_flip(Ser0UDCSR, status);

	if (status & UDCSR_EIR)
		udc_ep0_int_hndlr(usb);

	if (status & UDCSR_RIR)
		udc_ep1_int_hndlr(usb);

	if (status & UDCSR_TIR)
		udc_ep2_int_hndlr(usb);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PROC_FS

#define SAY( fmt, args... )  p += sprintf(p, fmt, ## args )
#define SAYV(  num )         p += sprintf(p, num_fmt, "Value", num )
#define SAYC( label, yn )    p += sprintf(p, yn_fmt, label, yn )
#define SAYS( label, v )     p += sprintf(p, cnt_fmt, label, v )

static int
udc_read_proc(char *page, char **start, off_t off, int cnt, int *eof,
	      void *data)
{
	struct sausb_dev *usb = data;
	char *p = page;
	u32 v;
	int len, i;

	p += usbctl_proc_info(usb->ctl, p);
	p += sprintf(p, "\nUDC:\n");
	v = Ser0UDCAR;
	p += sprintf(p, "Address\t: %d (0x%02x)\n", v, v);
	v = Ser0UDCIMP;
	p += sprintf(p, "IN max\t: %d (0x%02x)\n", v + 1, v);
	v = Ser0UDCOMP;
	p += sprintf(p, "OUT max\t: %d (0x%02x)\n", v + 1, v);
	v = Ser0UDCCR;
	p += sprintf(p, "UDCCR\t: 0x%02x "
			"[ %cSUSIM %cTIM %cRIM %cEIM %cRESIM %cUDA %cUDD ] "
			"(0x%02x)\n",
			v,
			v & UDCCR_SUSIM ? '+' : '-', v & UDCCR_TIM ? '+' : '-',
			v & UDCCR_RIM   ? '+' : '-', v & UDCCR_EIM ? '+' : '-',
			v & UDCCR_RESIM ? '+' : '-', v & UDCCR_UDA ? '+' : '-',
			v & UDCCR_UDD   ? '+' : '-', usb->udccr);
	v = Ser0UDCCS0;
	p += sprintf(p, "UDCCS0\t: 0x%02x "
			"[ %cSO    %cSE  %cDE  %cFST %cSST   %cIPR %cOPR ]\n",
			v,
			v & UDCCS0_SO  ? '+' : '-', v & UDCCS0_SE  ? '+' : '-',
			v & UDCCS0_DE  ? '+' : '-', v & UDCCS0_FST ? '+' : '-',
			v & UDCCS0_SST ? '+' : '-', v & UDCCS0_IPR ? '+' : '-',
			v & UDCCS0_OPR ? '+' : '-');
	v = Ser0UDCCS1;
	p += sprintf(p, "UDCCS1\t: 0x%02x "
			"[         %cRNE %cFST %cSST %cRPE   %cRPC %cRFS ]\n",
			v,
			v & UDCCS1_RNE ? '+' : '-', v & UDCCS1_FST ? '+' : '-',
			v & UDCCS1_SST ? '+' : '-', v & UDCCS1_RPE ? '+' : '-',
			v & UDCCS1_RPC ? '+' : '-', v & UDCCS1_RFS ? '+' : '-');
	v = Ser0UDCCS2;
	p += sprintf(p, "UDCCS2\t: 0x%02x "
			"[         %cFST %cSST %cTUR %cTPE   %cTPC %cTFS ]\n",
			v,
			v & UDCCS2_FST ? '+' : '-', v & UDCCS2_SST ? '+' : '-',
			v & UDCCS2_TUR ? '+' : '-', v & UDCCS2_TPE ? '+' : '-',
			v & UDCCS2_TPC ? '+' : '-', v & UDCCS2_TFS ? '+' : '-');

	p += sprintf(p, "\n");
	p += sprintf(p, "             Bytes    Packets  FIFO errs Max Sz\n");
	p += sprintf(p, "EP0 Rd: %10ld %10ld %10ld      -\n",
		     usb->ep0_rd_bytes,
		     usb->ep0_rd_packets,
		     usb->ep0_rd_fifo_errs);
	p += sprintf(p, "EP0 Wr: %10ld %10ld %10ld      -\n",
		     usb->ep0_wr_bytes,
		     usb->ep0_wr_packets,
		     usb->ep0_wr_fifo_errs);

	for (i = 0; i < 2; i++)
		p += sprintf(p, "EP%d   : %10ld %10ld %10ld %6d\n",
			     i + 1,
			     usb->ep[i].bytes,
			     usb->ep[i].packets,
			     usb->ep[i].fifo_errs,
			     usb->ep[i].maxpktsize);

	p += sprintf(p, "Stalls sent\t: %ld\n", usb->ep0_stall_sent);
	p += sprintf(p, "Early ints\t: %ld\n", usb->ep0_early_irqs);

#if 0
	v = Ser0UDCSR;
	SAY("\nUDC Interrupt Request Register\n");
	SAYV(v);
	SAYC("Reset pending", (v & UDCSR_RSTIR) ? yes : no);
	SAYC("Suspend pending", (v & UDCSR_SUSIR) ? yes : no);
	SAYC("Resume pending", (v & UDCSR_RESIR) ? yes : no);
	SAYC("ep0 pending", (v & UDCSR_EIR) ? yes : no);
	SAYC("receiver pending", (v & UDCSR_RIR) ? yes : no);
	SAYC("tramsitter pending", (v & UDCSR_TIR) ? yes : no);

#ifdef CONFIG_SA1100_EXTENEX1
	SAYC("\nSoft connect",
	     (PPSR & PPC_USB_SOFT_CON) ? "Visible" : "Hidden");
#endif
#endif

	len = (p - page) - off;
	if (len < 0)
		len = 0;
	*eof = (len <= cnt) ? 1 : 0;
	*start = page + off;

	return len;
}

#endif

extern struct usbctl usbctl;

static int __devinit udc_probe(struct platform_device *pdev)
{
	struct sausb_dev *usb;
	int retval;

	if (!request_mem_region(0x80000000, 0x10000, "sa11x0-udc"))
		return -EBUSY;

	usb = kmalloc(sizeof(struct sausb_dev), GFP_KERNEL);
	if (!usb)
		return -ENOMEM;

	memset(usb, 0, sizeof(struct sausb_dev));
	platform_set_drvdata(pdev, usb);

	usb_sa1100_drv.priv = usb;

	usb->dev = &(pdev->dev);
	usb->ctl = &usbctl;

	spin_lock_init(&usb->lock);

	udc_disable(usb);

	usbctl_init(usb->ctl, &usb_sa1100_drv);

#ifdef CONFIG_PROC_FS
	create_proc_read_entry("sausb", 0, NULL, udc_read_proc, usb);
#endif

	/* setup rx dma */
	retval = sa1100_request_dma(DMA_Ser0UDCRd, "USB receive",
				    NULL, NULL, &usb->ep[0].dmach);
	if (retval) {
		printk("UDC: unable to register for rx dma rc=%d\n",
		       retval);
		goto err;
	}

	/* setup tx dma */
	retval = sa1100_request_dma(DMA_Ser0UDCWr, "USB transmit",
				    NULL, NULL, &usb->ep[1].dmach);
	if (retval) {
		printk("UDC: unable to register for tx dma rc=%d\n",
		       retval);
		goto err;
	}

	/* now allocate the IRQ. */
	retval = request_irq(IRQ_Ser0UDC, udc_interrupt, SA_INTERRUPT,
			     "SA USB core", usb);
	if (retval) {
		printk("UDC: couldn't request USB irq rc=%d\n", retval);
		goto err;
	}

	return retval;

 err:
	if (usb->ep[2].dmach) {
		sa1100_free_dma(usb->ep[2].dmach);
		usb->ep[2].dmach = NULL;
	}
	if (usb->ep[1].dmach) {
		sa1100_free_dma(usb->ep[1].dmach);
		usb->ep[1].dmach = NULL;
	}
#ifdef CONFIG_PROC_FS
	remove_proc_entry("sausb", NULL);
#endif
	release_mem_region(0x80000000, 0x10000);
	return retval;
}

/*
 * Release DMA and interrupt resources
 */
static int __devexit udc_remove(struct platform_device *pdev)
{
	struct sausb_dev *usb = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

#ifdef CONFIG_PROC_FS
	remove_proc_entry("sausb", NULL);
#endif

	udc_disable(usb);

	free_irq(IRQ_Ser0UDC, usb);
	sa1100_free_dma(usb->ep[1].dmach);
	sa1100_free_dma(usb->ep[0].dmach);

	usbctl_exit(usb->ctl);

	release_mem_region(0x80000000, 0x10000);

	return 0;
}

static struct platform_driver sa11x0usb_driver = {
	.driver	= {
		.name	= "sa11x0-udc",
		.owner	= THIS_MODULE,
	},
	.probe		= udc_probe,
	.remove		= __devexit_p(udc_remove),
};

static int __init udc_init(void)
{
	return platform_driver_register(&sa11x0usb_driver);
}

static void __exit udc_exit(void)
{
	platform_driver_unregister(&sa11x0usb_driver);
}

module_init(udc_init);
module_exit(udc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SA1100 USB Gadget driver");
