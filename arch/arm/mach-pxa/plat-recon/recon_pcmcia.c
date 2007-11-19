/*
 * Recon PCMCIA support
 *
 * Copyright © 2005 SDG Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch-pxa/recon.h>
#include "../../../../drivers/pcmcia/soc_common.h"

#include "recon.h"

#ifdef CONFIG_RECON_X
    static struct soc_pcmcia_socket *sockets[3];
#   define is_recon_x 1
#else
    static struct soc_pcmcia_socket *sockets[2];
#   define is_recon_x 0
#endif

#ifdef CONFIG_RECON_X
static struct pcmcia_irqs reconx_irqs[] = {
    { 0, RECON_IRQ(RECON_GPIO_nP1_CD), "cf0_change" },
    { 1, RECON_IRQ(RECON_GPIO_nP2_CD), "cf1_change" },
};
#endif

static int
recon_pcmcia_hw_init(struct soc_pcmcia_socket *sock)
{
    // printk( KERN_INFO "recon_pcmcia_hw_init: %d\n", sock->nr );
    sockets[sock->nr] = sock;
    if(sock->nr == 0) sock->irq = RECON_IRQ(RECON_GPIO_P1_IRQ);
    if(sock->nr == 1) sock->irq = RECON_IRQ(RECON_GPIO_P2_IRQ);
    if(is_recon_x) {
        if(sock->nr == 2) {
            sock->irq = RECON_IRQ(RECON_GPIO_P3_IRQ);
            set_irq_type(RECON_IRQ(RECON_GPIO_P3_IRQ), IRQT_FALLING);
        }
    }
#   ifdef CONFIG_RECON_X
    {
        int retval;
        retval = soc_pcmcia_request_irqs(sock, reconx_irqs, ARRAY_SIZE(reconx_irqs));
        if(retval) return retval;
        /* For some dumb reason, soc_pcmcia_request_irqs() sets the irq types to
        * IRQT_NOEDGE */
        // set_irq_type(RECON_IRQ(RECON_GPIO_nP1_CD), IRQT_BOTHEDGE);
        // set_irq_type(RECON_IRQ(RECON_GPIO_nP2_CD), IRQT_BOTHEDGE);
    }
#   endif
    return 0;
}

static void
recon_pcmcia_hw_shutdown(struct soc_pcmcia_socket *sock)
{
    sockets[sock->nr] = 0;
#   ifdef CONFIG_RECON_X
        soc_pcmcia_free_irqs(sock, reconx_irqs, ARRAY_SIZE(reconx_irqs));
#   endif
}

static void
recon_pcmcia_socket_state(struct soc_pcmcia_socket *sock,
    struct pcmcia_state *state)
{
    // printk( KERN_INFO "recon socket state, slot=%d\n", sock->nr );
    if(sock->nr == 0) state->detect = recon_get_card_detect1();
    if(sock->nr == 1) state->detect = recon_get_card_detect2();
    if(sock->nr == 0) state->ready  = GET_RECON_GPIO(RECON_GPIO_P1_IRQ) ? 1 : 0;
    if(sock->nr == 1) state->ready  = GET_RECON_GPIO(RECON_GPIO_P2_IRQ) ? 1 : 0;
    if(is_recon_x) {
        if(sock->nr == 2) state->detect = recon_get_card_detect3();
        if(sock->nr == 2) state->ready  = GET_RECON_GPIO(RECON_GPIO_P3_IRQ) ? 1 : 0;
    }
    state->bvd1   = 1;
    state->bvd2   = 1;
    state->wrprot = 0;
    state->vs_3v  = 1;
    state->vs_Xv  = 0;
    // printk( KERN_INFO "recon socket state, slot=%d exit\n", sock->nr );
}

static void
reset_cf(int slot, int reset)
{
    // printk( KERN_INFO "recon reset_cf %d\n", slot );
    if(reset) {
        if(slot == 0) {
            GPSR0 = (1 << RECON_GPIO_P1_RST); /* Active high reset */
        } else if(slot == 1) {
            GPSR0 = (1 << RECON_GPIO_P2_RST); /* Active high reset */
        } else if(is_recon_x && (slot == 2)) {
            GPSR0 = (1 << RECON_GPIO_P3_RST); /* Active high reset */
        }
    } else {
        if(slot == 0) {
            GPCR0 = (1 << RECON_GPIO_P1_RST); /* Active high reset */
        } else if(slot == 1) {
            GPCR0 = (1 << RECON_GPIO_P2_RST); /* Active high reset */
        } else if(is_recon_x && (slot == 2)) {
            GPCR0 = (1 << RECON_GPIO_P3_RST); /* Active high reset */
        }
    }
}

static void
power_off(unsigned long slot)
{
    if(slot == 0) {
        GPSR0 = (1 << RECON_GPIO_nP1_ON); /* Active low power */
    } else if(slot == 1) {
        GPSR0 = (1 << RECON_GPIO_nP2_ON); /* Active low power */
    }
}

static struct timer_list power_off_0 = TIMER_INITIALIZER(power_off, 0, 0);
static struct timer_list power_off_1 = TIMER_INITIALIZER(power_off, 0, 1);

static int
recon_pcmcia_config_socket(struct soc_pcmcia_socket *sock, const socket_state_t *state)
{
    int slot = sock->nr;

    // printk( KERN_INFO "recon_pcmcia_config_socket %d\n", slot );
    if(state->Vcc == 0) {
        if(slot == 0) {
            reset_cf(slot, 1);
            /*
             * A note about these timers.  The Recon X has bus holders that
             * require power to get the bus into a reasonable state
             * (particularly nPWAIT pulled high).  So we don't turn the power
             * off until everything settles down.
             */
            mod_timer(&power_off_0, jiffies + HZ);
        } else if(slot == 1) {
            reset_cf(slot, 1);
            mod_timer(&power_off_1, jiffies + HZ);
        } else if(is_recon_x && (slot == 2)) {
            reset_cf(slot, 1);
            GPSR0 = (1 << RECON_GPIO_WLAN_OFF); /* Active low power */
        }
    } else if(state->Vcc == 33 || state->Vcc == 50) {

        if(state->flags & SS_RESET) {
            reset_cf(slot, 1);
        } else {
            reset_cf(slot, 0);
        }

        if(slot == 0) {
            del_timer_sync(&power_off_0);
            GPCR0 = (1 << RECON_GPIO_nP1_ON); /* Active low power */
        } else if(slot == 1) {
            del_timer_sync(&power_off_1);
            GPCR0 = (1 << RECON_GPIO_nP2_ON); /* Active low power */
        } else if(is_recon_x && (slot == 2)) {
            GPCR0 = (1 << RECON_GPIO_WLAN_OFF); /* Active low power */
        }
    } else {
        return -EINVAL;
    }
    return 0;
}

static void
recon_pcmcia_socket_init(struct soc_pcmcia_socket *sock)
{
    reset_cf(sock->nr, 0);
}

static void
recon_pcmcia_socket_suspend(struct soc_pcmcia_socket *sock)
{
    // printk( "pcmcia: suspending socket %d\n", sock->nr );
#   ifdef CONFIG_RECON_X
    if (sock->nr == 0) {
	int card_in = GET_RECON_GPIO(RECON_GPIO_nP1_CD) ? 0 : 1;
	del_timer_sync(&power_off_0);
	GPCR0 = (1 << RECON_GPIO_nP1_ON); /* Active low power */
	PWER |= (1 << RECON_GPIO_nP1_CD);
	if (card_in) {
	    PRER |= (1 << RECON_GPIO_nP1_CD);
	    PFER &= ~(1 << RECON_GPIO_nP1_CD);
	}
	else {
	    PFER |= (1 << RECON_GPIO_nP1_CD);
	    PRER &= ~(1 << RECON_GPIO_nP1_CD);
	}
    }
    else if (sock->nr == 1) {
	int card_in = GET_RECON_GPIO(RECON_GPIO_nP2_CD) ? 0 : 1;
	del_timer_sync(&power_off_1);
	GPCR0 = (1 << RECON_GPIO_nP2_ON); /* Active low power */
	PWER |= (1 << RECON_GPIO_nP2_CD);
	if (card_in) {
	    PRER |= (1 << RECON_GPIO_nP2_CD);
	    PFER &= ~(1 << RECON_GPIO_nP2_CD);
	}
	else {
	    PFER |= (1 << RECON_GPIO_nP2_CD);
	    PRER &= ~(1 << RECON_GPIO_nP2_CD);
	}
    }
#   endif
}

static struct pcmcia_low_level recon_pcmcia_ops = {
    .owner              = THIS_MODULE,
#ifdef CONFIG_RECON_X
    .nr                 = 2,            /* Number of slots */
#else
    .nr                 = 2,            /* Number of slots */
#endif
    .hw_init            = recon_pcmcia_hw_init,
    .hw_shutdown        = recon_pcmcia_hw_shutdown,
    .socket_state       = recon_pcmcia_socket_state,
    .configure_socket   = recon_pcmcia_config_socket,
    .socket_init        = recon_pcmcia_socket_init,
    .socket_suspend     = recon_pcmcia_socket_suspend,
};

static struct platform_device recon_pcmcia_device = {
    .name               = "pxa2xx-pcmcia",
    .dev                = {
        .platform_data      = &recon_pcmcia_ops
    }
};

static void
card_insertion(int slot, int in)
{
    pcmcia_parse_events(&(sockets[slot]->socket), SS_DETECT);
}

static int __init
pcmcia_init(void)
{
    // printk(KERN_NOTICE "TDS Recon PC Card Driver\n");
    recon_register_pcmcia_callback(card_insertion);
    return platform_device_register(&recon_pcmcia_device);
}

static void __exit
pcmcia_exit(void)
{
    del_timer_sync(&power_off_0);
    del_timer_sync(&power_off_1);
    recon_unregister_pcmcia_callback();
    reset_cf(0, 1);
    GPSR0 = (1 << RECON_GPIO_nP1_ON); /* Active low power */
    reset_cf(1, 1);
    GPSR0 = (1 << RECON_GPIO_nP2_ON); /* Active low power */
    if(is_recon_x) {
        reset_cf(2, 1);
        GPSR0 = (1 << RECON_GPIO_WLAN_OFF); /* Active low power */
    }
    return platform_device_unregister(&recon_pcmcia_device);
}

module_init(pcmcia_init);
module_exit(pcmcia_exit);

MODULE_AUTHOR("SDG Systems, LLC");
MODULE_DESCRIPTION("Recon PCMCIA Driver");
MODULE_LICENSE("GPL");
