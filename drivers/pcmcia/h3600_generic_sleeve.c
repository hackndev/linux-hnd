/*
 * drivers/pcmcia/h3600_generic
 *
 * PCMCIA implementation routines for H3600 iPAQ standards sleeves
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/i2c.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/hardware/ipaq-ops.h>
#include <asm/ipaq-sleeve.h>

#include <linux/serial.h>
#include <pcmcia/cs_types.h>
#include <linux/sysctl.h>
#include <pcmcia/ss.h>

#include "h3600_pcmcia.h"

#ifdef CONFIG_ARCH_PXA
#include <asm/arch/pxa-regs.h>
#include "pxa2xx_base.h"
#endif

#include <asm/hardware/linkup-l1110.h>

/***************** Initialization *****************/


static void buffered_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
					struct pcmcia_state *state)
{
	h3600_common_pcmcia_socket_state(skt, state);

	/* no bvd or vs bits on single pcmcia sleeve or CF sleeve */
	state->bvd1=1;
	state->bvd2=1;
	state->wrprot=0; /* Not available on H3600. */
	state->vs_3v=0;
	state->vs_Xv=0;
}

static int buffered_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
					     const socket_state_t *state)
{
	int vcc = state->Vcc;
	int reset = state->flags & SS_RESET;

	SLEEVE_DEBUG(3,"vcc=%d vpp=%d reset=%d\n", 
		      vcc, state->Vpp, reset);

	switch (vcc) {
	case 0:
		break;

	case 50:
	case 33:
		break;

	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
		       vcc);
		return -1;
	}

	ipaq_sleeve_write_egpio (IPAQ_EGPIO_CARD_RESET, reset);
	return 0;
}

/*******************************************************/

struct pcmcia_low_level buffered_pcmcia_socket_ops = {
	.hw_init =	   h3600_common_pcmcia_init,
	.hw_shutdown =	   h3600_common_pcmcia_shutdown,
	.socket_state =	   buffered_pcmcia_socket_state,
	.configure_socket =  buffered_pcmcia_configure_socket,
	.socket_init =	   h3600_common_pcmcia_socket_init,
	.socket_suspend =	   h3600_common_pcmcia_socket_suspend,
#ifdef CONFIG_ARCH_PXA
	.set_timing =	     pxa2xx_pcmcia_set_timing,
#endif
};

/*************************************************************************************/
/*     
       Common sleeve functions 
*/
/*************************************************************************************/

static int
generic_remove_sleeve(struct device *dev)
{
        SLEEVE_DEBUG(1,"%s\n", dev->driver->name);
	soc_common_drv_pcmcia_remove (dev);
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_ON);
	
	return 0;
}

static int 
generic_suspend_sleeve(struct device *dev, pm_message_t state)
{
        SLEEVE_DEBUG(1, "%s\n", dev->driver->name);
	h3600_pcmcia_suspend_sockets();	
	ipaq_sleeve_clear_egpio(IPAQ_EGPIO_OPT_ON);
	return 0;
}

static int 
generic_resume_sleeve(struct device *dev)
{
        SLEEVE_DEBUG(1,"%s\n", dev->driver->name);
	ipaq_sleeve_set_egpio(IPAQ_EGPIO_OPT_ON);
	h3600_pcmcia_resume_sockets();	
	return 0;
}


/*************************************************************************************/
/*     
       Compact Flash sleeve 
*/
/*************************************************************************************/

static int __devinit cf_probe_sleeve(struct device *dev)
{
        SLEEVE_DEBUG(1,"%s\n", dev->driver->name);
	ipaq_sleeve_set_egpio(IPAQ_EGPIO_OPT_ON);
//        soc_common_drv_pcmcia_probe (dev, &buffered_pcmcia_socket_ops, 0, 1);
	MECR |= MECR_CIT | MECR_NOS;
        return 0;
}

static struct ipaq_sleeve_device_id cf_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, SINGLE_COMPACTFLASH_SLEEVE },
	{ 0, }
};

static struct ipaq_sleeve_driver cf_driver = {
	.driver = {
		.name =	  "Compaq Compact Flash Sleeve",
		.probe =	  cf_probe_sleeve,
		.remove =   generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = cf_tbl,
	.features = SLEEVE_HAS_CF(1),
};

static struct ipaq_sleeve_device_id cf_plus_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, SINGLE_CF_PLUS_SLEEVE },
	{ 0, }
};

static struct ipaq_sleeve_driver cf_plus_driver = {
	.driver = {
		.name =	  "Compaq Compact Flash Plus Sleeve",
		.probe =	  cf_probe_sleeve,
		.remove =   generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = cf_plus_tbl,
	.features = (SLEEVE_HAS_CF(1) | SLEEVE_HAS_REMOVABLE_BATTERY | SLEEVE_HAS_EBAT),
};

static struct ipaq_sleeve_device_id gprs_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, GPRS_EXPANSION_PACK },
	{ 0, }
};

static struct ipaq_sleeve_driver gprs_driver = {
	.driver = {
		.name =	  "Compaq GSM and GPRS Sleeve",
		.probe =	  cf_probe_sleeve,
		.remove =   generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = gprs_tbl,
	.features = (SLEEVE_HAS_FIXED_BATTERY | SLEEVE_HAS_GPRS),
};


/*************************************************************************************/
/*     
       Single slot PCMCIA sleeve 
*/
/*************************************************************************************/

static int
pcmcia_probe_sleeve(struct device *dev)
{
	ipaq_sleeve_set_egpio(IPAQ_EGPIO_OPT_ON);
//        soc_common_drv_pcmcia_probe (dev, &buffered_pcmcia_socket_ops, 0, 1);
	MECR |= MECR_CIT | MECR_NOS;
//	  pcmcia_sleeve_attach_flash(pcmcia_set_vpp, 0x02000000);
	return 0;
}

static struct ipaq_sleeve_device_id pcmcia_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, SINGLE_PCMCIA_SLEEVE },
	{ 0xFFFF, 0xFFFF },
	{ 0, }
};

static struct ipaq_sleeve_driver pcmcia_driver = {
	.driver = {
		.name =	  "Compaq PC Card Sleeve",
		.probe =	  pcmcia_probe_sleeve,
		.remove =	  generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = pcmcia_tbl,
	.features = (SLEEVE_HAS_PCMCIA(1) | SLEEVE_HAS_FIXED_BATTERY),
};



/*************************************************************************************/
/*     
       Compaq Bluetooth/CF Sleeve (operationally equivalent to dual CF sleeve)
*/
/*************************************************************************************/

static struct ipaq_sleeve_device_id bluetooth_cf_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, BLUETOOTH_EXPANSION_PACK },
	{ 0, }
};

static struct ipaq_sleeve_driver bluetooth_cf_driver = {
	.driver = {
		.name =	  "Compaq Bluetooth and CF Sleeve",
		.probe =	  pcmcia_probe_sleeve,
		.remove =	  generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = bluetooth_cf_tbl,
	.features = (SLEEVE_HAS_CF(1) | SLEEVE_HAS_BLUETOOTH),
};

/*************************************************************************************/
/*     
       Nexian Camera/CF Sleeve
*/
/*************************************************************************************/

static struct ipaq_sleeve_device_id nexian_camera_cf_tbl[] __devinitdata = {
	{ NEXIAN_VENDOR, NEXIAN_CAMERA_CF_SLEEVE },
	{ 0, }
};

static struct ipaq_sleeve_driver nexian_camera_cf_driver = {
	.driver = {
		.name =	  "Nexian Nexicam Camera and CF Sleeve",
		.probe =	  pcmcia_probe_sleeve,
		.remove =   generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = nexian_camera_cf_tbl,
	.features = (SLEEVE_HAS_CF(1) | SLEEVE_HAS_CAMERA),
};

/*************************************************************************************/
/*     
       Symbol wlan barcode scanner sleeve 
*/
/*************************************************************************************/

static struct ipaq_sleeve_device_id symbol_wlan_scanner_tbl[] __devinitdata = {
	{ SYMBOL_VENDOR, SYMBOL_WLAN_SCANNER_SLEEVE },
	{ 0, }
};

static struct ipaq_sleeve_driver symbol_wlan_scanner_driver = {
	.driver = {
		.name =	  "Symbol WLAN Scanner Sleeve",
		.probe =	  pcmcia_probe_sleeve,
		.remove =	  generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = symbol_wlan_scanner_tbl,
	.features = (SLEEVE_HAS_BARCODE_READER | SLEEVE_HAS_802_11B | SLEEVE_HAS_FIXED_BATTERY),
};

/*************************************************************************************/
/*     
       Compaq Dual Sleeve uses the L1110 PCMCIA interface chip
*/
/*************************************************************************************/

static struct linkup_l1110 *dual_pcmcia_sleeve[2]; 

static void L1110_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				     struct pcmcia_state *state)
{
	short prs = readl(&dual_pcmcia_sleeve[skt->nr]->prc);

	h3600_common_pcmcia_socket_state(skt, state);

	state->bvd1 = prs & LINKUP_PRS_BVD1;
	state->bvd2 = prs & LINKUP_PRS_BVD2;
	state->wrprot = 0;
	if ((prs & LINKUP_PRS_VS1) == 0)
		state->vs_3v = 1;
	if ((prs & LINKUP_PRS_VS2) == 0)
		state->vs_Xv = 1;
}

static int L1110_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
					 const socket_state_t *state)
{
	unsigned int  prc;
	int vcc = state->Vcc;
	int vpp = state->Vpp;
	int reset = state->flags & SS_RESET;

	SLEEVE_DEBUG(3,"vcc=%d vpp=%d reset=%d\n", 
		      vcc, vpp, reset);

	prc = (LINKUP_PRC_APOE | LINKUP_PRC_SOE | LINKUP_PRC_S1 | LINKUP_PRC_S2
	       | (skt->nr * LINKUP_PRC_SSP));

	/* Linkup Systems L1110 with TI TPS2205 PCMCIA Power Switch */
	/* S1 is VCC5#, S2 is VCC3# */ 
	/* S3 is VPP_VCC, S4 is VPP_PGM */
	/* PWR_ON is wired to #SHDN */
	switch (vcc) {
	case 0:
		break;
	case 50:
		prc &= ~LINKUP_PRC_S1;
		break;
	case 33:
		prc &= ~LINKUP_PRC_S2;
		break;
	default:
		SLEEVE_DEBUG(0,"unrecognized Vcc %u\n", vcc);
		return -1;
	}
	if (vpp == 12) {
		prc |= LINKUP_PRC_S4;
	} else if (vpp == vcc) {
		prc |= LINKUP_PRC_S3;
	}

	if (reset)
		prc |= LINKUP_PRC_RESET;

	writel(prc, &dual_pcmcia_sleeve[skt->nr]->prc);

	return 0;
}

struct pcmcia_low_level L1110_pcmcia_socket_ops = {
	.hw_init =           h3600_common_pcmcia_init,
	.hw_shutdown =       h3600_common_pcmcia_shutdown,
	.socket_state =      L1110_pcmcia_socket_state,
	.configure_socket =  L1110_pcmcia_configure_socket,
        .socket_init =       h3600_common_pcmcia_socket_init,
        .socket_suspend =    h3600_common_pcmcia_socket_suspend,
#ifdef CONFIG_ARCH_PXA
	.set_timing =	     pxa2xx_pcmcia_set_timing,
#endif
};

static int
dual_pcmcia_probe_sleeve(struct device *dev)
{
#ifdef CONFIG_ARCH_SA1100
	unsigned long cs3_phys = SA1100_CS3_PHYS;
#elif defined(CONFIG_ARCH_PXA)
	unsigned long cs3_phys = PXA_CS3_PHYS;
#else
#error only supported on SA1100 or PXA 
#endif
	/* on SA1100: 0x1a000000 */
	dual_pcmcia_sleeve[0] = (struct linkup_l1110 *)__ioremap(cs3_phys + 0x02000000, PAGE_SIZE, 0);
	/* on SA1100: 0x19000000 */
	dual_pcmcia_sleeve[1] = (struct linkup_l1110 *)__ioremap(cs3_phys + 0x01000000, PAGE_SIZE, 0);

	writel(LINKUP_PRC_S2|LINKUP_PRC_S1, &dual_pcmcia_sleeve[0]->prc);
	writel(LINKUP_PRC_S2|LINKUP_PRC_S1|LINKUP_PRC_SSP, &dual_pcmcia_sleeve[1]->prc);

        SLEEVE_DEBUG(1,"%s\n", dev->driver->name);
	ipaq_sleeve_set_egpio(IPAQ_EGPIO_OPT_ON);
	soc_common_drv_pcmcia_probe (dev, &L1110_pcmcia_socket_ops, 0, 2);
	MECR |= MECR_CIT | MECR_NOS;
        return 0;
}

static int
dual_pcmcia_remove_sleeve(struct device *dev)
{
	soc_common_drv_pcmcia_remove (dev);
	__iounmap(dual_pcmcia_sleeve[0]);
	__iounmap(dual_pcmcia_sleeve[1]);
	return 0;
}

static struct ipaq_sleeve_device_id dual_pcmcia_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, DUAL_PCMCIA_SLEEVE },
	{ 0, }
};

static struct ipaq_sleeve_driver dual_pcmcia_driver = {
	.driver = {
		.name =	  "Compaq Dual PC Card Sleeve",
		.probe =	  dual_pcmcia_probe_sleeve,
		.remove =	  dual_pcmcia_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = dual_pcmcia_tbl,
	.features = (SLEEVE_HAS_PCMCIA(2) | SLEEVE_HAS_FIXED_BATTERY),
};



static struct ipaq_sleeve_device_id pcmcia_plus_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, SINGLE_PCMCIA_PLUS_SLEEVE },
	{ 0, }
};

static struct ipaq_sleeve_driver pcmcia_plus_driver = {
	.driver = {
		.name =	  "Compaq PC Card Plus Sleeve",
		.probe =	  dual_pcmcia_probe_sleeve,
		.remove =   generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = pcmcia_plus_tbl,
	.features = (SLEEVE_HAS_PCMCIA(1) | SLEEVE_HAS_REMOVABLE_BATTERY | SLEEVE_HAS_EBAT),
};



/*************************************************************************************/
/*     
       Nexian Dual CF Sleeve
 */
/*************************************************************************************/

static void h3600_dual_cf_socket_state(struct soc_pcmcia_socket *skt,
				      struct pcmcia_state *state)
{
	h3600_common_pcmcia_socket_state(skt, state);

	/* no bvd or vs bits on single pcmcia sleeve or CF sleeve */
	state->bvd1=1;
	state->bvd2=1;
	state->wrprot=0; /* Not available on H3600. */
	/* CF sleeve is 3.3V only */
	state->vs_3v=1;
	state->vs_Xv=0;
}

static int h3600_dual_cf_configure_socket(struct soc_pcmcia_socket *skt,
					  const socket_state_t *state)
{
	int vcc = state->Vcc;
	int reset = state->flags & SS_RESET;


	SLEEVE_DEBUG(3,"vcc=%d vpp=%d reset=%d\n", 
                      vcc, state->Vpp, reset);

	switch (vcc) {
	case 0:
		break;

	case 50:
	case 33:
		break;

	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
		       vcc);
		return -1;
	}

	/* CARD_RESET is attached to socket 0, no reset signal available for sock 1 */
        if (skt->nr == 0)
		ipaq_sleeve_write_egpio (IPAQ_EGPIO_CARD_RESET, reset);
	return 0;
}

/*******************************************************/

struct pcmcia_low_level h3600_dual_cf_sleeve_ops = {
	.hw_init =           h3600_common_pcmcia_init,
	.hw_shutdown =       h3600_common_pcmcia_shutdown,
	.socket_state =      h3600_dual_cf_socket_state,
	.configure_socket =  h3600_dual_cf_configure_socket,
        .socket_init =       h3600_common_pcmcia_socket_init,
        .socket_suspend =    h3600_common_pcmcia_socket_suspend,
#ifdef CONFIG_ARCH_PXA
	.set_timing =	     pxa2xx_pcmcia_set_timing,
#endif
};
 
static int
dual_cf_probe_sleeve(struct device *dev)
{
        SLEEVE_DEBUG(1,"%s\n",dev->driver->name);
	ipaq_sleeve_set_egpio(IPAQ_EGPIO_OPT_ON);
        soc_common_drv_pcmcia_probe (dev, &h3600_dual_cf_sleeve_ops, 0, 2);
	MECR |= MECR_CIT | MECR_NOS;
        return 0;
}

static struct ipaq_sleeve_device_id nexian_dual_cf_tbl[] __devinitdata = {
	{ NEXIAN_VENDOR, NEXIAN_DUAL_CF_SLEEVE },
	{ 0, }
};

static struct ipaq_sleeve_driver nexian_dual_cf_driver = {
	.driver = {
		.name =	  "Nexian Dual CF Sleeve",
		.probe =	  dual_cf_probe_sleeve,
		.remove =   generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
	.id_table = nexian_dual_cf_tbl,
	.features = (SLEEVE_HAS_CF(2) | SLEEVE_HAS_REMOVABLE_BATTERY),
};



/*************************************************************************************/
/*     
       Subject: PITech Dual CF sleeve
 */
/*************************************************************************************/

static struct ipaq_sleeve_device_id pitech_memplug_tbl[] __devinitdata = {
        { PITECH_VENDOR, MEMPLUG_SLEEVE },
        { PITECH_VENDOR, MEMPLUG_SLEEVE3 },
        { 0, }
};

static struct ipaq_sleeve_driver pitech_memplug_driver = {
	.driver = {
		.name =     "PITech Dual CF sleeve",
		.probe =    dual_cf_probe_sleeve,
		.remove =   generic_remove_sleeve,
		.suspend =  generic_suspend_sleeve,
		.resume =   generic_resume_sleeve,
	},
        .id_table = pitech_memplug_tbl,
	.features = (SLEEVE_HAS_CF(2)),
};


/*************************************************************************************/

int __init h3600_generic_sleeve_init_module(void)
{
	SLEEVE_DEBUG(1,": registering sleeve drivers\n");
	ipaq_sleeve_driver_register(&cf_driver);
	ipaq_sleeve_driver_register(&pcmcia_driver);
	ipaq_sleeve_driver_register(&bluetooth_cf_driver);
	ipaq_sleeve_driver_register(&nexian_dual_cf_driver);
	ipaq_sleeve_driver_register(&nexian_camera_cf_driver);
	ipaq_sleeve_driver_register(&symbol_wlan_scanner_driver);
	ipaq_sleeve_driver_register(&dual_pcmcia_driver);
	ipaq_sleeve_driver_register(&pcmcia_plus_driver);
	ipaq_sleeve_driver_register(&cf_plus_driver);
	ipaq_sleeve_driver_register(&gprs_driver);
	ipaq_sleeve_driver_register(&pitech_memplug_driver);
	return 0;
}

void __exit h3600_generic_sleeve_exit_module(void)
{
	SLEEVE_DEBUG(1,": unregistering sleeve drivers\n");
	ipaq_sleeve_driver_unregister(&cf_driver);
	ipaq_sleeve_driver_unregister(&pcmcia_driver);
	ipaq_sleeve_driver_unregister(&bluetooth_cf_driver);
	ipaq_sleeve_driver_unregister(&nexian_dual_cf_driver);
	ipaq_sleeve_driver_unregister(&nexian_camera_cf_driver);
	ipaq_sleeve_driver_unregister(&symbol_wlan_scanner_driver);
	ipaq_sleeve_driver_unregister(&dual_pcmcia_driver);
	ipaq_sleeve_driver_unregister(&pcmcia_plus_driver);
	ipaq_sleeve_driver_unregister(&cf_plus_driver);
	ipaq_sleeve_driver_unregister(&gprs_driver);
	ipaq_sleeve_driver_unregister(&pitech_memplug_driver);
} 

module_init(h3600_generic_sleeve_init_module);
module_exit(h3600_generic_sleeve_exit_module);

MODULE_AUTHOR("Jamey Hicks <jamey@handhelds.org> and Phil Blundell <pb@handhelds.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for PCMCIA sleeves on HP iPAQ");
