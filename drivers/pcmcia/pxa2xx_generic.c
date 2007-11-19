/*======================================================================

    Device driver for the PCMCIA control functionality of Intel PXA2xx
    microprocessors.

    The contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL")

    (c) Ian Molton (spyro@f2s.com) 2003
    (c) Stefan Eletzhofer (stefan.eletzhofer@inquant.de) 2003,4

    derived from sa1100_generic.c

     Portions created by John G. Dorsey are
     Copyright (C) 1999 John G. Dorsey.
    
======================================================================*/

#include <linux/module.h>
#include <linux/init.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>

#include <asm/arch/pcmcia.h>

#include "pxa2xx.h"
#include "pxa2xx_generic.h"

static int pxa2xx_generic_drv_pcmcia_suspend(struct device *dev, u32 state, u32 level)
{
	int ret = 0;
	if (level == SUSPEND_SAVE_STATE)
		ret = pcmcia_socket_dev_suspend(dev, state);
	return ret;
}

static int pxa2xx_generic_drv_pcmcia_resume(struct device *dev, u32 level)
{
	int ret = 0;
	if (level == RESUME_RESTORE_STATE)
		ret = pcmcia_socket_dev_resume(dev);
	return ret;
}

static struct device_driver pxa2xx_generic_pcmcia_driver = {
	.probe		= pxa2xx_core_drv_pcmcia_probe,
	.remove		= pxa2xx_core_drv_pcmcia_remove,
	.name		= "pxa2xx-pcmcia",
	.bus		= &platform_bus_type,
	.suspend 	= pxa2xx_generic_drv_pcmcia_suspend,
	.resume 	= pxa2xx_generic_drv_pcmcia_resume,
};

/* pxa2xx_generic_pcmcia_init()
 * ^^^^^^^^^^^^^^^^^^^^
 *
 * This routine performs low-level PCMCIA initialization and then
 * registers this socket driver with Card Services.
 *
 * Returns: 0 on success, -ve error code on failure
 */
static int __init pxa2xx_generic_pcmcia_init(void)
{
	return driver_register(&pxa2xx_generic_pcmcia_driver);
}

/* pxa2xx_generic_pcmcia_exit()
 * ^^^^^^^^^^^^^^^^^^^^
 * Invokes the low-level kernel service to free IRQs associated with this
 * socket controller and reset GPIO edge detection.
 */
static void __exit pxa2xx_generic_pcmcia_exit(void)
{
	driver_unregister(&pxa2xx_generic_pcmcia_driver);
}

MODULE_AUTHOR("Stefan Eletzhofer <stefan.eletzhofer@inquant.de> and Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("Linux PCMCIA Card Services: PXA2xx generic routines");
MODULE_LICENSE("GPL");

module_init(pxa2xx_generic_pcmcia_init);
module_exit(pxa2xx_generic_pcmcia_exit);
