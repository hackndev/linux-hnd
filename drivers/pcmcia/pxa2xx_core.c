/*======================================================================

  Device driver for the PCMCIA control functionality of PXA2xx
  microprocessors.

    The contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL")
                                                                                
    (c) Ian Molton (spyro@f2s.com) 2003
    (c) Stefan Eletzhofer (stefan.eletzhofer@inquant.de) 2003,4
                                                                                
    derived from sa11xx_core.c

     Portions created by John G. Dorsey are
     Copyright (C) 1999 John G. Dorsey.

  ======================================================================*/
/*
 * Please see linux/Documentation/arm/SA1100/PCMCIA for more information
 * on the low-level kernel interface.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include "pxa2xx_core.h"
#include "pxa2xx.h"

#define to_pxa2xx_socket(x)	container_of(x, struct pxa2xx_pcmcia_socket, socket)

#ifdef DEBUG
static int pc_debug;

module_param(pc_debug, int, 0644);

#define debug(skt, lvl, fmt, arg...) do {			\
	if (pc_debug > (lvl))					\
		printk(KERN_DEBUG "skt%u: %s: " fmt,		\
		       (skt)->nr, __func__ , ## arg);		\
} while (0)

#else
#define debug(skt, lvl, fmt, arg...) do { } while (0)
#endif

/* FIXME */
#define cpufreq_get(dummy) (get_clk_frequency_khz())

#if 0
/*
 * pxa2xx_core_pcmcia_default_mecr_timing
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *
 * Calculate MECR clock wait states for given CPU clock
 * speed and command wait state. This function can be over-
 * written by a board specific version.
 *
 * The default is to simply calculate the BS values as specified in
 * the INTEL SA1100 development manual
 * "Expansion Memory (PCMCIA) Configuration Register (MECR)"
 * that's section 10.2.5 in _my_ version of the manual ;)
 */
static unsigned int
pxa2xx_core_pcmcia_default_mecr_timing(struct pxa2xx_pcmcia_socket *skt,
		unsigned int cpu_speed,
		unsigned int cmd_time)
{
	debug(1, "skt->nr=%d, speed=%d, time=%d", skt->nr, cpu_speed, cmd_time);
	return pxa2xx_core_pcmcia_mecr_bs(cmd_time, cpu_speed);
}
#endif

static unsigned short calc_speed(unsigned short *spds, int num, unsigned short dflt)
{
	unsigned short speed = 0;
	int i;

	for (i = 0; i < num; i++)
		if (speed < spds[i])
			speed = spds[i];
	if (speed == 0)
		speed = dflt;

	return speed;
}

static int pxa2xx_core_pcmcia_set_mcmem( int sock, int speed, int clock )
{
	MCMEM(sock) = ((pxa2xx_mcxx_setup(speed, clock)
		& MCXX_SETUP_MASK) << MCXX_SETUP_SHIFT)
		| ((pxa2xx_mcxx_asst(speed, clock)
		& MCXX_ASST_MASK) << MCXX_ASST_SHIFT)
		| ((pxa2xx_mcxx_hold(speed, clock)
		& MCXX_HOLD_MASK) << MCXX_HOLD_SHIFT);

	return 0;
}

static int pxa2xx_core_pcmcia_set_mcio( int sock, int speed, int clock )
{
	MCIO(sock) = ((pxa2xx_mcxx_setup(speed, clock)
		& MCXX_SETUP_MASK) << MCXX_SETUP_SHIFT)
		| ((pxa2xx_mcxx_asst(speed, clock)
		& MCXX_ASST_MASK) << MCXX_ASST_SHIFT)
		| ((pxa2xx_mcxx_hold(speed, clock)
		& MCXX_HOLD_MASK) << MCXX_HOLD_SHIFT);

	return 0;
}

static int pxa2xx_core_pcmcia_set_mcatt( int sock, int speed, int clock )
{
	MCATT(sock) = ((pxa2xx_mcxx_setup(speed, clock)
		& MCXX_SETUP_MASK) << MCXX_SETUP_SHIFT)
		| ((pxa2xx_mcxx_asst(speed, clock)
		& MCXX_ASST_MASK) << MCXX_ASST_SHIFT)
		| ((pxa2xx_mcxx_hold(speed, clock)
		& MCXX_HOLD_MASK) << MCXX_HOLD_SHIFT);

	return 0;
}

static int pxa2xx_core_pcmcia_set_mcxx(struct pxa2xx_pcmcia_socket *skt, unsigned int cpu_clock)
{
	int sock = skt->nr;

	pxa2xx_core_pcmcia_set_mcmem( sock, PXA_PCMCIA_5V_MEM_ACCESS, cpu_clock );
	pxa2xx_core_pcmcia_set_mcatt( sock, PXA_PCMCIA_ATTR_MEM_ACCESS, cpu_clock );
	pxa2xx_core_pcmcia_set_mcio( sock, PXA_PCMCIA_IO_ACCESS, cpu_clock );

	return 0;
}

static unsigned int pxa2xx_core_pcmcia_skt_state(struct pxa2xx_pcmcia_socket *skt)
{
	struct pcmcia_state state;
	unsigned int stat;

	memset(&state, 0, sizeof(struct pcmcia_state));

	skt->ops->socket_state(skt, &state);

	stat = state.detect  ? SS_DETECT : 0;
	stat |= state.ready  ? SS_READY  : 0;
	stat |= state.wrprot ? SS_WRPROT : 0;
	stat |= state.vs_3v  ? SS_3VCARD : 0;
	stat |= state.vs_Xv  ? SS_XVCARD : 0;

	/* The power status of individual sockets is not available
	 * explicitly from the hardware, so we just remember the state
	 * and regurgitate it upon request:
	 */
	stat |= skt->cs_state.Vcc ? SS_POWERON : 0;

	if (skt->cs_state.flags & SS_IOCARD)
		stat |= state.bvd1 ? SS_STSCHG : 0;
	else {
		if (state.bvd1 == 0)
			stat |= SS_BATDEAD;
		else if (state.bvd2 == 0)
			stat |= SS_BATWARN;
	}
	return stat;
}

/*
 * pxa2xx_core_pcmcia_config_skt
 * ^^^^^^^^^^^^^^^^^^^^^^^^
 *
 * Convert PCMCIA socket state to our socket configure structure.
 */
static int
pxa2xx_core_pcmcia_config_skt(struct pxa2xx_pcmcia_socket *skt, socket_state_t *state)
{
	int ret;

	ret = skt->ops->configure_socket(skt, state);
	if (ret == 0) {
		/*
		 * This really needs a better solution.  The IRQ
		 * may or may not be claimed by the driver.
		 */
		if (skt->irq_state != 1 && state->io_irq) {
			skt->irq_state = 1;
			set_irq_type(skt->irq, IRQT_FALLING);
		} else if (skt->irq_state == 1 && state->io_irq == 0) {
			skt->irq_state = 0;
			set_irq_type(skt->irq, IRQT_NOEDGE);
		}

		skt->cs_state = *state;
	}

	if (ret < 0)
		printk(KERN_ERR "pxa2xx_core_pcmcia: unable to configure "
				"socket %d\n", skt->nr);

	return ret;
}

/* pxa2xx_core_pcmcia_sock_init()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^
 *
 * (Re-)Initialise the socket, turning on status interrupts
 * and PCMCIA bus.  This must wait for power to stabilise
 * so that the card status signals report correctly.
 *
 * Returns: 0
 */
static int pxa2xx_core_pcmcia_sock_init(struct pcmcia_socket *sock)
{
	struct pxa2xx_pcmcia_socket *skt = to_pxa2xx_socket(sock);

	debug(2, "initializing socket %u\n", skt->nr);

	skt->ops->socket_init(skt);
	if (skt->ops->cd_irq)
		pxa2xx_core_enable_irqs (skt, skt->ops->cd_irq, skt->ops->nr);

	return 0;
}


/*
 * pxa2xx_core_pcmcia_suspend()
 * ^^^^^^^^^^^^^^^^^^^^^^^
 *
 * Remove power on the socket, disable IRQs from the card.
 * Turn off status interrupts, and disable the PCMCIA bus.
 *
 * Returns: 0
 */
static int pxa2xx_core_pcmcia_suspend(struct pcmcia_socket *sock)
{
	struct pxa2xx_pcmcia_socket *skt = to_pxa2xx_socket(sock);
	int ret;

	debug(2, "suspending socket %u\n", skt->nr);

	ret = pxa2xx_core_pcmcia_config_skt(skt, &dead_socket);
	if (ret == 0) {
		if (skt->ops->cd_irq)
			pxa2xx_core_disable_irqs (skt, skt->ops->cd_irq, skt->ops->nr);
		skt->ops->socket_suspend(skt);
	}

	return ret;
}

static spinlock_t status_lock = SPIN_LOCK_UNLOCKED;

/* pxa2xx_core_check_status()
 * ^^^^^^^^^^^^^^^^^^^^^
 */
static void pxa2xx_core_check_status(struct pxa2xx_pcmcia_socket *skt)
{
	unsigned int events;

	//debug(4, "%s(): entering PCMCIA monitoring thread\n", __FUNCTION__);

	//debug( "skt->nr=%d", skt->nr );

	do {
		unsigned int status;
		unsigned long flags;

		status = pxa2xx_core_pcmcia_skt_state(skt);

		spin_lock_irqsave(&status_lock, flags);
		events = (status ^ skt->status) & skt->cs_state.csc_mask;
		skt->status = status;
		spin_unlock_irqrestore(&status_lock, flags);

#if 0
		debug(1, "events: %s%s%s%s%s%s\n",
				events == 0         ? "<NONE>"   : "",
				events & SS_DETECT  ? "DETECT "  : "",
				events & SS_READY   ? "READY "   : "",
				events & SS_BATDEAD ? "BATDEAD " : "",
				events & SS_BATWARN ? "BATWARN " : "",
				events & SS_STSCHG  ? "STSCHG "  : "");
#endif

		if (events)
			pcmcia_parse_events(&skt->socket, events);
	} while (events);
}

/* pxa2xx_core_pcmcia_poll_event()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Let's poll for events in addition to IRQs since IRQ only is unreliable...
 */
static void pxa2xx_core_pcmcia_poll_event(unsigned long dummy)
{
	struct pxa2xx_pcmcia_socket *skt = (struct pxa2xx_pcmcia_socket *)dummy;
	debug(4, "%s(): polling for events\n", __FUNCTION__);

	mod_timer(&skt->poll_timer, jiffies + PXA_PCMCIA_POLL_PERIOD);

	pxa2xx_core_check_status(skt);
}


/* pxa2xx_core_pcmcia_interrupt()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^
 * Service routine for socket driver interrupts (requested by the
 * low-level PCMCIA init() operation via pxa2xx_core_pcmcia_thread()).
 * The actual interrupt-servicing work is performed by
 * pxa2xx_core_pcmcia_thread(), largely because the Card Services event-
 * handling code performs scheduling operations which cannot be
 * executed from within an interrupt context.
 */
static irqreturn_t pxa2xx_core_pcmcia_interrupt(int irq, void *dev)
{
	struct pxa2xx_pcmcia_socket *skt = dev;

	debug(3, "%s(): servicing IRQ %d\n", __FUNCTION__, irq);

	pxa2xx_core_check_status(skt);

	return IRQ_HANDLED;
}


/* pxa2xx_core_pcmcia_get_status()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the get_status() operation for the in-kernel PCMCIA
 * service (formerly SS_GetStatus in Card Services). Essentially just
 * fills in bits in `status' according to internal driver state or
 * the value of the voltage detect chipselect register.
 *
 * As a debugging note, during card startup, the PCMCIA core issues
 * three set_socket() commands in a row the first with RESET deasserted,
 * the second with RESET asserted, and the last with RESET deasserted
 * again. Following the third set_socket(), a get_status() command will
 * be issued. The kernel is looking for the SS_READY flag (see
 * setup_socket(), reset_socket(), and unreset_socket() in cs.c).
 *
 * Returns: 0
 */
static int
pxa2xx_core_pcmcia_get_status(struct pcmcia_socket *sock, unsigned int *status)
{
	struct pxa2xx_pcmcia_socket *skt = to_pxa2xx_socket(sock);

	skt->status = pxa2xx_core_pcmcia_skt_state(skt);
	*status = skt->status;

	return 0;
}


/* pxa2xx_core_pcmcia_get_socket()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the get_socket() operation for the in-kernel PCMCIA
 * service (formerly SS_GetSocket in Card Services). Not a very 
 * exciting routine.
 *
 * Returns: 0
 */
static int
pxa2xx_core_pcmcia_get_socket(struct pcmcia_socket *sock, socket_state_t *state)
{
	struct pxa2xx_pcmcia_socket *skt = to_pxa2xx_socket(sock);

	debug(2, "%s() for sock %u\n", __FUNCTION__, skt->nr);

	*state = skt->cs_state;

	return 0;
}

/* pxa2xx_core_pcmcia_set_socket()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the set_socket() operation for the in-kernel PCMCIA
 * service (formerly SS_SetSocket in Card Services). We more or
 * less punt all of this work and let the kernel handle the details
 * of power configuration, reset, &c. We also record the value of
 * `state' in order to regurgitate it to the PCMCIA core later.
 *
 * Returns: 0
 */
static int
pxa2xx_core_pcmcia_set_socket(struct pcmcia_socket *sock, socket_state_t *state)
{
	struct pxa2xx_pcmcia_socket *skt = to_pxa2xx_socket(sock);

	debug(2, "%s() for sock %u\n", __FUNCTION__, skt->nr);

	debug(3, "\tmask:  %s%s%s%s%s%s\n\tflags: %s%s%s%s%s%s\n",
			(state->csc_mask==0)?"<NONE>":"",
			(state->csc_mask&SS_DETECT)?"DETECT ":"",
			(state->csc_mask&SS_READY)?"READY ":"",
			(state->csc_mask&SS_BATDEAD)?"BATDEAD ":"",
			(state->csc_mask&SS_BATWARN)?"BATWARN ":"",
			(state->csc_mask&SS_STSCHG)?"STSCHG ":"",
			(state->flags==0)?"<NONE>":"",
			(state->flags&SS_PWR_AUTO)?"PWR_AUTO ":"",
			(state->flags&SS_IOCARD)?"IOCARD ":"",
			(state->flags&SS_RESET)?"RESET ":"",
			(state->flags&SS_SPKR_ENA)?"SPKR_ENA ":"",
			(state->flags&SS_OUTPUT_ENA)?"OUTPUT_ENA ":"");
	debug(3, "\tVcc %d  Vpp %d  irq %d\n",
			state->Vcc, state->Vpp, state->io_irq);

	return pxa2xx_core_pcmcia_config_skt(skt, state);
}  /* pxa2xx_core_pcmcia_set_socket() */


/* pxa2xx_core_pcmcia_set_io_map()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the set_io_map() operation for the in-kernel PCMCIA
 * service (formerly SS_SetIOMap in Card Services). We configure
 * the map speed as requested, but override the address ranges
 * supplied by Card Services.
 *
 * Returns: 0 on success, -1 on error
 */
static int
pxa2xx_core_pcmcia_set_io_map(struct pcmcia_socket *sock, struct pccard_io_map *map)
{
	struct pxa2xx_pcmcia_socket *skt = to_pxa2xx_socket(sock);
	unsigned short speed = map->speed;

	debug(2, "%s() for sock %u\n", __FUNCTION__, skt->nr);

	debug(3, "\tmap %u  speed %u\n\tstart 0x%08x  stop 0x%08x\n",
			map->map, map->speed, map->start, map->stop);
	debug(3, "\tflags: %s%s%s%s%s%s%s%s\n",
			(map->flags==0)?"<NONE>":"",
			(map->flags&MAP_ACTIVE)?"ACTIVE ":"",
			(map->flags&MAP_16BIT)?"16BIT ":"",
			(map->flags&MAP_AUTOSZ)?"AUTOSZ ":"",
			(map->flags&MAP_0WS)?"0WS ":"",
			(map->flags&MAP_WRPROT)?"WRPROT ":"",
			(map->flags&MAP_USE_WAIT)?"USE_WAIT ":"",
			(map->flags&MAP_PREFETCH)?"PREFETCH ":"");

	if (map->map >= MAX_IO_WIN) {
		printk(KERN_ERR "%s(): map (%d) out of range\n", __FUNCTION__,
				map->map);
		return -1;
	}

	if (map->flags & MAP_ACTIVE) {
		if (speed == 0)
			speed = PXA_PCMCIA_IO_ACCESS;
	} else {
		speed = 0;
	}

	skt->spd_io[map->map] = speed;
	pxa2xx_core_pcmcia_set_mcio( skt->nr, speed, get_lclk_frequency_10khz() );

	if (map->stop == 1)
		map->stop = PAGE_SIZE-1;

	map->stop -= map->start;
	map->stop += (unsigned long)skt->virt_io;
	map->start = (unsigned long)skt->virt_io;

	return 0;
}  /* pxa2xx_core_pcmcia_set_io_map() */


/* pxa2xx_core_pcmcia_set_mem_map()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the set_mem_map() operation for the in-kernel PCMCIA
 * service (formerly SS_SetMemMap in Card Services). We configure
 * the map speed as requested, but override the address ranges
 * supplied by Card Services.
 *
 * Returns: 0 on success, -1 on error
 */
static int
pxa2xx_core_pcmcia_set_mem_map(struct pcmcia_socket *sock, struct pccard_mem_map *map)
{
	struct pxa2xx_pcmcia_socket *skt = to_pxa2xx_socket(sock);
	struct resource *res;
	unsigned short speed = map->speed;

	debug(2, "%s() for sock %u\n", __FUNCTION__, skt->nr);

	debug(3, "\tmap %u speed %u card_start %08x\n",
			map->map, map->speed, map->card_start);
	debug(3, "\tflags: %s%s%s%s%s%s%s%s\n",
			(map->flags==0)?"<NONE>":"",
			(map->flags&MAP_ACTIVE)?"ACTIVE ":"",
			(map->flags&MAP_16BIT)?"16BIT ":"",
			(map->flags&MAP_AUTOSZ)?"AUTOSZ ":"",
			(map->flags&MAP_0WS)?"0WS ":"",
			(map->flags&MAP_WRPROT)?"WRPROT ":"",
			(map->flags&MAP_ATTRIB)?"ATTRIB ":"",
			(map->flags&MAP_USE_WAIT)?"USE_WAIT ":"");

	if (map->map >= MAX_WIN)
		return -EINVAL;

	if (map->flags & MAP_ACTIVE) {
		if (speed == 0)
			speed = 300;
	} else {
		speed = 0;
	}

	if (map->flags & MAP_ATTRIB) {
		res = &skt->res_attr;
		skt->spd_attr[map->map] = speed;
		skt->spd_mem[map->map] = 0;
		pxa2xx_core_pcmcia_set_mcatt( skt->nr, speed, get_lclk_frequency_10khz() );
	} else {
		res = &skt->res_mem;
		skt->spd_attr[map->map] = 0;
		skt->spd_mem[map->map] = speed;
		pxa2xx_core_pcmcia_set_mcmem( skt->nr, speed , get_lclk_frequency_10khz());
	}

	map->sys_stop -= map->sys_start;
	map->sys_stop += res->start + map->card_start;
	map->sys_start = res->start + map->card_start;

	return 0;
}

struct bittbl {
	unsigned int mask;
	const char *name;
};

static struct bittbl status_bits[] = {
	{ SS_WRPROT,		"SS_WRPROT"	},
	{ SS_BATDEAD,		"SS_BATDEAD"	},
	{ SS_BATWARN,		"SS_BATWARN"	},
	{ SS_READY,		"SS_READY"	},
	{ SS_DETECT,		"SS_DETECT"	},
	{ SS_POWERON,		"SS_POWERON"	},
	{ SS_STSCHG,		"SS_STSCHG"	},
	{ SS_3VCARD,		"SS_3VCARD"	},
	{ SS_XVCARD,		"SS_XVCARD"	},
};

static struct bittbl conf_bits[] = {
	{ SS_PWR_AUTO,		"SS_PWR_AUTO"	},
	{ SS_IOCARD,		"SS_IOCARD"	},
	{ SS_RESET,		"SS_RESET"	},
	{ SS_DMA_MODE,		"SS_DMA_MODE"	},
	{ SS_SPKR_ENA,		"SS_SPKR_ENA"	},
	{ SS_OUTPUT_ENA,	"SS_OUTPUT_ENA"	},
};

static void
dump_bits(char **p, const char *prefix, unsigned int val, struct bittbl *bits, int sz)
{
	char *b = *p;
	int i;

	b += sprintf(b, "%-9s:", prefix);
	for (i = 0; i < sz; i++)
		if (val & bits[i].mask)
			b += sprintf(b, " %s", bits[i].name);
	*b++ = '\n';
	*p = b;
}

/* show_status()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Implements the /sys/class/pcmcia_socket/??/status file.
 *
 * Returns: the number of characters added to the buffer
 */
static ssize_t show_status(struct class_device *class_dev, char *buf)
{
	struct pxa2xx_pcmcia_socket *skt = container_of(class_dev, 
			struct pxa2xx_pcmcia_socket, socket.dev);
	unsigned int clock = get_lclk_frequency_10khz();
	char *p = buf;

	p+=sprintf(p, "slot     : %d\n", skt->nr);

	dump_bits(&p, "status", skt->status,
			status_bits, ARRAY_SIZE(status_bits));
	dump_bits(&p, "csc_mask", skt->cs_state.csc_mask,
			status_bits, ARRAY_SIZE(status_bits));
	dump_bits(&p, "cs_flags", skt->cs_state.flags,
			conf_bits, ARRAY_SIZE(conf_bits));

	p+=sprintf(p, "Vcc      : %d\n", skt->cs_state.Vcc);
	p+=sprintf(p, "Vpp      : %d\n", skt->cs_state.Vpp);
	p+=sprintf(p, "IRQ      : %d (%d)\n", skt->cs_state.io_irq, skt->irq);

	p+=sprintf(p, "I/O      : %u (%u)\n",
			calc_speed(skt->spd_io, MAX_IO_WIN, PXA_PCMCIA_IO_ACCESS),
			pxa2xx_pcmcia_cmd_time(clock, (MCIO(skt->nr)>>MCXX_ASST_SHIFT)&MCXX_ASST_MASK));

	p+=sprintf(p, "attribute: %u (%u)\n",
			calc_speed(skt->spd_attr, MAX_WIN, PXA_PCMCIA_3V_MEM_ACCESS),
			pxa2xx_pcmcia_cmd_time(clock, (MCATT(skt->nr)>>MCXX_ASST_SHIFT)&MCXX_ASST_MASK));

	p+=sprintf(p, "common   : %u (%u)\n",
			calc_speed(skt->spd_mem, MAX_WIN, PXA_PCMCIA_3V_MEM_ACCESS),
			pxa2xx_pcmcia_cmd_time(clock, (MCMEM(skt->nr)>>MCXX_ASST_SHIFT)&MCXX_ASST_MASK));

	return p-buf;
}
static CLASS_DEVICE_ATTR(status, S_IRUGO, show_status, NULL);


static struct pccard_operations pxa2xx_core_pcmcia_operations = {
	.init			= pxa2xx_core_pcmcia_sock_init,
	.suspend		= pxa2xx_core_pcmcia_suspend,
	.get_status		= pxa2xx_core_pcmcia_get_status,
	.get_socket		= pxa2xx_core_pcmcia_get_socket,
	.set_socket		= pxa2xx_core_pcmcia_set_socket,
	.set_io_map		= pxa2xx_core_pcmcia_set_io_map,
	.set_mem_map		= pxa2xx_core_pcmcia_set_mem_map,
};

int pxa2xx_core_request_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr)
{
	int i, res = 0;

	for (i = 0; i < nr; i++) {
		if (irqs[i].sock != skt->nr)
			continue;
		res = request_irq(irqs[i].irq, pxa2xx_core_pcmcia_interrupt,
				SA_INTERRUPT, irqs[i].str, skt);
		if (res)
			break;
		set_irq_type(irqs[i].irq, IRQT_NOEDGE);
	}

	if (res) {
		printk(KERN_ERR "PCMCIA: request for IRQ%d failed (%d)\n",
				irqs[i].irq, res);

		while (i--)
			if (irqs[i].sock == skt->nr)
				free_irq(irqs[i].irq, skt);
	}
	return res;
}
EXPORT_SYMBOL(pxa2xx_core_request_irqs);

void pxa2xx_core_free_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
		if (irqs[i].sock == skt->nr)
			free_irq(irqs[i].irq, skt);
}
EXPORT_SYMBOL(pxa2xx_core_free_irqs);

void pxa2xx_core_disable_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
		if (irqs[i].sock == skt->nr)
			set_irq_type(irqs[i].irq, IRQT_NOEDGE);
}
EXPORT_SYMBOL(pxa2xx_core_disable_irqs);

void pxa2xx_core_enable_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
		if (irqs[i].sock == skt->nr) {
			set_irq_type(irqs[i].irq, IRQT_RISING);
			set_irq_type(irqs[i].irq, IRQT_BOTHEDGE);
		}
}
EXPORT_SYMBOL(pxa2xx_core_enable_irqs);

static LIST_HEAD(pxa2xx_sockets);
static DECLARE_MUTEX(pxa2xx_sockets_lock);

static const char *skt_names[] = {
	"PCMCIA socket 0",
	"PCMCIA socket 1",
};

struct skt_dev_info {
	int nskt;
	struct pxa2xx_pcmcia_socket skt[0];
};

#define SKT_DEV_INFO_SIZE(n) \
(sizeof(struct skt_dev_info) + (n)*sizeof(struct pxa2xx_pcmcia_socket))

int pxa2xx_core_drv_pcmcia_probe(struct device *dev)
{
	struct skt_dev_info *sinfo;
	unsigned int cpu_clock;
	int ret = -ENODEV, i;
	struct pcmcia_low_level *ops;
	int first, nr;

#if 0
	/* FIXME: fix this for pxa or kill it
	 * set default MECR calculation if the board specific
	 * code did not specify one...
	 */
	if (!ops->socket_get_timing)
		ops->socket_get_timing = pxa2xx_pcmcia_default_mecr_timing;
#endif
	
	if (!dev || !dev->platform_data)
		goto out;

	ops = (struct pcmcia_low_level *)dev->platform_data;

	first = ops->first;
	nr = ops->nr;
	
	/* Setup GPIOs for PCMCIA/CF alternate function mode.
	 *
	 * It would be nice if set_GPIO_mode included support
	 * for driving GPIO outputs to default high/low state
	 * before programming GPIOs as outputs. Setting GPIO
	 * outputs to default high/low state via GPSR/GPCR
	 * before defining them as outputs should reduce
	 * the possibility of glitching outputs during GPIO
	 * setup. This of course assumes external terminators
	 * are present to hold GPIOs in a defined state.
	 *
	 * In the meantime, setup default state of GPIO
	 * outputs before we enable them as outputs.
	 */

	GPSR(GPIO48_nPOE) = GPIO_bit(GPIO48_nPOE) |
		GPIO_bit(GPIO49_nPWE) |
		GPIO_bit(GPIO50_nPIOR) |
		GPIO_bit(GPIO51_nPIOW) |
		GPIO_bit(GPIO52_nPCE_1) |
		GPIO_bit(GPIO53_nPCE_2);

	pxa_gpio_mode(GPIO48_nPOE_MD);
	pxa_gpio_mode(GPIO49_nPWE_MD);
	pxa_gpio_mode(GPIO50_nPIOR_MD);
	pxa_gpio_mode(GPIO51_nPIOW_MD);
	pxa_gpio_mode(GPIO52_nPCE_1_MD);
	pxa_gpio_mode(GPIO53_nPCE_2_MD);
	pxa_gpio_mode(GPIO54_pSKTSEL_MD); /* REVISIT: s/b dependent on num sockets */
	pxa_gpio_mode(GPIO55_nPREG_MD);
	pxa_gpio_mode(GPIO56_nPWAIT_MD);
	pxa_gpio_mode(GPIO57_nIOIS16_MD);


	down(&pxa2xx_sockets_lock);

	sinfo = kmalloc(SKT_DEV_INFO_SIZE(nr), GFP_KERNEL);
	if (!sinfo) {
		ret = -ENOMEM;
		goto out;
	}

	memset(sinfo, 0, SKT_DEV_INFO_SIZE(nr));
	sinfo->nskt = nr;

	cpu_clock = get_lclk_frequency_10khz();

	/*
	 * Initialise the per-socket structure.
	 */
	for (i = 0; i < nr; i++) {
		struct pxa2xx_pcmcia_socket *skt = &sinfo->skt[i];

		skt->socket.ops = &pxa2xx_core_pcmcia_operations;
		skt->socket.owner = ops->owner;
		skt->socket.dev.dev = dev;

		init_timer(&skt->poll_timer);
		skt->poll_timer.function = pxa2xx_core_pcmcia_poll_event;
		skt->poll_timer.data = (unsigned long)skt;
		skt->poll_timer.expires = jiffies + PXA_PCMCIA_POLL_PERIOD;

		skt->nr		= first + i;
		skt->irq	= NO_IRQ;
		skt->dev	= dev;
		skt->ops	= ops;

		skt->res_skt.start	= _PCMCIA(skt->nr);
		skt->res_skt.end	= _PCMCIA(skt->nr) + PCMCIASp - 1;
		skt->res_skt.name	= skt_names[skt->nr];
		skt->res_skt.flags	= IORESOURCE_MEM;

		ret = request_resource(&iomem_resource, &skt->res_skt);
		if (ret)
			goto out_err_1;

		skt->res_io.start	= _PCMCIAIO(skt->nr);
		skt->res_io.end		= _PCMCIAIO(skt->nr) + PCMCIAIOSp - 1;
		skt->res_io.name	= "io";
		skt->res_io.flags	= IORESOURCE_MEM | IORESOURCE_BUSY;

		ret = request_resource(&skt->res_skt, &skt->res_io);
		if (ret)
			goto out_err_2;

		skt->res_mem.start	= _PCMCIAMem(skt->nr);
		skt->res_mem.end	= _PCMCIAMem(skt->nr) + PCMCIAMemSp - 1;
		skt->res_mem.name	= "memory";
		skt->res_mem.flags	= IORESOURCE_MEM;

		ret = request_resource(&skt->res_skt, &skt->res_mem);
		if (ret)
			goto out_err_3;

		skt->res_attr.start	= _PCMCIAAttr(skt->nr);
		skt->res_attr.end	= _PCMCIAAttr(skt->nr) + PCMCIAAttrSp - 1;
		skt->res_attr.name	= "attribute";
		skt->res_attr.flags	= IORESOURCE_MEM;

		ret = request_resource(&skt->res_skt, &skt->res_attr);
		if (ret)
			goto out_err_4;

		skt->virt_io = ioremap(skt->res_io.start, 0x10000);
		if (skt->virt_io == NULL) {
			ret = -ENOMEM;
			goto out_err_5;
		}

		list_add(&skt->node, &pxa2xx_sockets);

		/*
		 * We initialize the MECR to default values here, because
		 * we are not guaranteed to see a SetIOMap operation at
		 * runtime.
		 */
		pxa2xx_core_pcmcia_set_mcxx(skt, cpu_clock);

		ret = ops->hw_init(skt);
		if (ret)
			goto out_err_6;

		if (ops->cd_irq &&
		    pxa2xx_core_request_irqs (skt, ops->cd_irq, ops->nr))
			goto out_err_6;

		skt->socket.features = SS_CAP_STATIC_MAP|SS_CAP_PCCARD;
		skt->socket.irq_mask = 0;
		skt->socket.map_size = PAGE_SIZE;
		skt->socket.pci_irq = skt->irq;
		skt->socket.io_offset = (unsigned long)skt->virt_io;

		skt->status = pxa2xx_core_pcmcia_skt_state(skt);

		ret = pcmcia_register_socket(&skt->socket);
		if (ret)
			goto out_err_7;

		WARN_ON(skt->socket.sock != i);

		add_timer(&skt->poll_timer);

		//printk (KERN_DEBUG "dev=%p, attr=%p\n", &skt->socket.dev, &class_device_attr_status);
		class_device_create_file(&skt->socket.dev, &class_device_attr_status);
	}

	/* We have at last one socket, so set MECR:CIT (Card Is There) */
	MECR |= MECR_CIT;

	/* Set MECR:NOS (Number Of Sockets) */
	if ( nr>1 )
		MECR |= MECR_NOS;
	else
		MECR &= ~MECR_NOS;

	dev_set_drvdata(dev, sinfo);
	ret = 0;
	goto out;

	do {
		struct pxa2xx_pcmcia_socket *skt = &sinfo->skt[i];

		del_timer_sync(&skt->poll_timer);
		pcmcia_unregister_socket(&skt->socket);

out_err_7:
		flush_scheduled_work();

		if (ops->cd_irq)
			pxa2xx_core_free_irqs (skt, ops->cd_irq, ops->nr);
		ops->hw_shutdown(skt);
out_err_6:
		list_del(&skt->node);
		iounmap(skt->virt_io);
out_err_5:
		release_resource(&skt->res_attr);
out_err_4:
		release_resource(&skt->res_mem);
out_err_3:
		release_resource(&skt->res_io);
out_err_2:
		release_resource(&skt->res_skt);
out_err_1:
		i--;
	} while (i > 0);

	kfree(sinfo);

out:
	up(&pxa2xx_sockets_lock);
	return ret;
}
EXPORT_SYMBOL(pxa2xx_core_drv_pcmcia_probe);

int pxa2xx_core_drv_pcmcia_remove(struct device *dev)
{
	struct skt_dev_info *sinfo = dev_get_drvdata(dev);
	int i;

	dev_set_drvdata(dev, NULL);

	down(&pxa2xx_sockets_lock);
	for (i = 0; i < sinfo->nskt; i++) {
		struct pxa2xx_pcmcia_socket *skt = &sinfo->skt[i];

		class_device_remove_file(&skt->socket.dev, &class_device_attr_status);

		del_timer_sync(&skt->poll_timer);

		pcmcia_unregister_socket(&skt->socket);

		flush_scheduled_work();

		if (skt->ops->cd_irq)
			pxa2xx_core_free_irqs (skt, skt->ops->cd_irq, skt->ops->nr);

		skt->ops->hw_shutdown(skt);

		pxa2xx_core_pcmcia_config_skt(skt, &dead_socket);

		list_del(&skt->node);
		iounmap(skt->virt_io);
		skt->virt_io = NULL;
		release_resource(&skt->res_attr);
		release_resource(&skt->res_mem);
		release_resource(&skt->res_io);
		release_resource(&skt->res_skt);
	}
	up(&pxa2xx_sockets_lock);

	kfree(sinfo);

	return 0;
}
EXPORT_SYMBOL(pxa2xx_core_drv_pcmcia_remove);

#ifdef CONFIG_CPU_FREQ

/* pxa2xx_core_pcmcia_update_mcxx()
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * When pxa2xx_core_pcmcia_notifier() decides that a MC{IO,MEM,ATT} adjustment (due
 * to a core clock frequency change) is needed, this routine establishes
 * new values consistent with the clock speed `clock'.
 */
static void pxa2xx_core_pcmcia_update_mcxx(unsigned int clock)
{
	struct pxa2xx_pcmcia_socket *skt;

	down(&pxa2xx_sockets_lock);
	list_for_each_entry(skt, &pxa2xx_sockets, node) {
		pxa2xx_core_pcmcia_set_mcio(skt->nr, calc_speed(skt->spd_io,
					MAX_IO_WIN, PXA_PCMCIA_IO_ACCESS), clock);
		pxa2xx_core_pcmcia_set_mcmem(skt->nr, calc_speed(skt->spd_io,
					MAX_IO_WIN, PXA_PCMCIA_3V_MEM_ACCESS), clock );
		pxa2xx_core_pcmcia_set_mcatt(skt->nr, calc_speed(skt->spd_io,
					MAX_IO_WIN, PXA_PCMCIA_3V_MEM_ACCESS), clock );
	}
	up(&pxa2xx_sockets_lock);
}

/* pxa2xx_core_pcmcia_notifier()
 * ^^^^^^^^^^^^^^^^^^^^^^^^
 * When changing the processor core clock frequency, it is necessary
 * to adjust the MECR timings accordingly. We've recorded the timings
 * requested by Card Services, so this is just a matter of finding
 * out what our current speed is, and then recomputing the new MCXX
 * values.
 *
 * Returns: 0 on success, -1 on error
 */
static int
pxa2xx_core_pcmcia_notifier(struct notifier_block *nb, unsigned long val,
		void *data)
{
	struct cpufreq_freqs *freqs = data;

	switch (val) {
		case CPUFREQ_PRECHANGE:
			if (freqs->new > freqs->old) {
				debug(2, "%s(): new frequency %u.%uMHz > %u.%uMHz, "
						"pre-updating\n", __FUNCTION__,
						freqs->new / 1000, (freqs->new / 100) % 10,
						freqs->old / 1000, (freqs->old / 100) % 10);
				pxa2xx_core_pcmcia_update_mcxx(freqs->new);
			}
			break;

		case CPUFREQ_POSTCHANGE:
			if (freqs->new < freqs->old) {
				debug(2, "%s(): new frequency %u.%uMHz < %u.%uMHz, "
						"post-updating\n", __FUNCTION__,
						freqs->new / 1000, (freqs->new / 100) % 10,
						freqs->old / 1000, (freqs->old / 100) % 10);
				pxa2xx_core_pcmcia_update_mcxx(freqs->new);
			}
			break;
	}

	return 0;
}

static struct notifier_block pxa2xx_core_pcmcia_notifier_block = {
	.notifier_call	= pxa2xx_core_pcmcia_notifier
};

static int __init pxa2xx_core_pcmcia_init(void)
{
	int ret;

	printk(KERN_INFO "PXA2xx PCMCIA\n");

	ret = cpufreq_register_notifier(&pxa2xx_core_pcmcia_notifier_block,
			CPUFREQ_TRANSITION_NOTIFIER);
	if (ret < 0)
		printk(KERN_ERR "Unable to register CPU frequency change "
				"notifier (%d)\n", ret);

	return ret;
}
module_init(pxa2xx_core_pcmcia_init);

static void __exit pxa2xx_core_pcmcia_exit(void)
{
	cpufreq_unregister_notifier(&pxa2xx_core_pcmcia_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);
}

module_exit(pxa2xx_core_pcmcia_exit);
#endif

MODULE_AUTHOR("Stefan Eletzhofer <stefan.eletzhofer@inquant.de> and Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("Linux PCMCIA Card Services: PXA2xx core socket driver");
MODULE_LICENSE("GPL");
