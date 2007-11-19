/*
 * drivers/pcmcia/sa1100_h3600.c
 *
 * PCMCIA implementation routines for H3600
 * All true functionality is shuttled off to the
 * pcmcia implementation for the current sleeve
 */

/****************************************************/
/*  Common functions used by the different sleeves */
/****************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/irq.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

//#include <asm/arch-sa1100/h3600.h>
#include <asm/hardware/ipaq-ops.h>
#include <asm/ipaq-sleeve.h>

#include "h3600_pcmcia.h"

int timing_increment_ns;

static struct pcmcia_irqs cd_irqs[] = {
        { 0, 0, "PCMCIA CD0" },
        { 1, 0, "PCMCIA CD1" },
};

int 
h3600_common_pcmcia_init(struct soc_pcmcia_socket *skt)
{
	/* Enable PCMCIA/CF bus: */
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_RESET);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_CARD_RESET);

	skt->irq = ipaq_sleeve_egpio_irq (skt->nr == 0 ? IPAQ_EGPIO_PCMCIA_IRQ0:
					  IPAQ_EGPIO_PCMCIA_IRQ1);

	cd_irqs[skt->nr].irq = ipaq_sleeve_egpio_irq (skt->nr == 0 ? IPAQ_EGPIO_PCMCIA_CD0_N :
						      IPAQ_EGPIO_PCMCIA_CD1_N);

	set_irq_type (cd_irqs[skt->nr].irq, IRQT_BOTHEDGE);

        return soc_pcmcia_request_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}
EXPORT_SYMBOL(h3600_common_pcmcia_init);

void
h3600_common_pcmcia_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}
EXPORT_SYMBOL(h3600_common_pcmcia_shutdown);

int 
h3600_common_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				 struct pcmcia_state *state)
{
	memset(state, 0, sizeof (*state));
  
	switch (skt->nr) {
	case 0:
		state->detect = (ipaq_sleeve_read_egpio(IPAQ_EGPIO_PCMCIA_CD0_N)==0)?1:0;
		state->ready = (ipaq_sleeve_read_egpio(IPAQ_EGPIO_PCMCIA_IRQ0))?1:0;
		break;
	case 1:
		state->detect = (ipaq_sleeve_read_egpio(IPAQ_EGPIO_PCMCIA_CD1_N)==0)?1:0;
		state->ready = (ipaq_sleeve_read_egpio(IPAQ_EGPIO_PCMCIA_IRQ1))?1:0;
		break;
	}

	return 0;
}
EXPORT_SYMBOL(h3600_common_pcmcia_socket_state);

void
h3600_common_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	/* Enable CF bus: */
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_RESET);

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(10*HZ / 1000);

}
EXPORT_SYMBOL(h3600_common_pcmcia_socket_init);

void
h3600_common_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
#if 0
	switch (skt->nr) {
	case 0:
		h3600_egpio_set_irq_type(IPAQ_EGPIO_PCMCIA_IRQ0, IRQT_NOEDGE);
		h3600_egpio_set_irq_type(IPAQ_EGPIO_PCMCIA_CD0_N, IRQT_NOEDGE);
		break;
	case 1:
		h3600_egpio_set_irq_type(IPAQ_EGPIO_PCMCIA_IRQ1, IRQT_NOEDGE);
		h3600_egpio_set_irq_type(IPAQ_EGPIO_PCMCIA_CD1_N, IRQT_NOEDGE);
		break;
	}
#endif
}
EXPORT_SYMBOL(h3600_common_pcmcia_socket_suspend);

/****************************************************/
/*  Swapping functions for PCMCIA operations        */
/****************************************************/

void 
h3600_pcmcia_suspend_sockets (void)
{
}

void 
h3600_pcmcia_resume_sockets (void)
{
}

EXPORT_SYMBOL(h3600_pcmcia_suspend_sockets);
EXPORT_SYMBOL(h3600_pcmcia_resume_sockets);
