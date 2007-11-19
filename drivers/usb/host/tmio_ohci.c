/*
 * Preliminary driver for TMIO ohci. non functional. borrows heavily from
 * sa1111 driver.
 * 
 * Supports the TMIO (Toshiba Mobile IO Controller) USB function.
 * supported variants:
 *   TC6393XB
 *
 * (c)2004 Ian Molton and Sebastien Carlier
 *
 */

#include <linux/device.h>
#include <linux/mfd/tmio_ohci.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch/eseries-irq.h>
#include <asm/arch/eseries-gpio.h>
#include <asm/arch/pxa-regs.h>

/*-------------------------------------------------------------------------*/

static irqreturn_t usb_hcd_tmio_hcim_irq (int irq, void *__hcd, struct pt_regs * r)
{
	struct usb_hcd *hcd = __hcd;
	printk("usb irq\n");

	return usb_hcd_irq(irq, hcd, r);
}

/*-------------------------------------------------------------------------*/

int __tmio_ohci_probe (const struct hc_driver *driver,
			  struct usb_hcd **hcd_out,
			  struct device *dev)
{
	struct tmio_ohci_hwconfig *hwconfig = (struct tmio_mmc_hwconfig *)dev->platform_data;
	struct platform_device *sdev = to_platform_device(dev);
	int retval;
	struct usb_hcd *hcd = NULL;
	struct ohci_hcd *ohci;
	void *conf_base;

	if (dma_declare_coherent_memory(dev, 0x10010000, 0x10000, 32*1024, DMA_MEMORY_MAP) != DMA_MEMORY_MAP)
		return -EBUSY;

	dmabounce_register_dev(dev, 512, 4096);

	if (!request_mem_region((unsigned long)sdev->resource[0].start, 0x100, hcd_name)) {
		dbg("request_mem_region failed");
		return -EBUSY;
	}

	hcd = usb_create_hcd (driver);
        if (!hcd){
                dbg ("hcd_alloc failed");
                retval = -ENOMEM;
                goto err1;
        }

	conf_base = ioremap((unsigned long)sdev->resource[1].start, 0x100);

	/* Set USB register base address */
	writel((unsigned long)sdev->resource[0].start & 0x1f00, conf_base + 0x10);

	/* Enable local memory */
	writeb(1, conf_base + 0x40);

	if(hwconfig && hwconfig->hwinit)
		hwconfig->hwinit(sdev);

	ohci = hcd_to_ohci(hcd);
	ohci_hcd_init(ohci);

	hcd->irq = sdev->resource[2].start;
	hcd->regs = ioremap((unsigned long)sdev->resource[0].start, 0x100); //FIXME - test for fail
	hcd->self.controller = dev;

	printk("ohci-TMIO: regs %08x %08x\n", hcd->regs, ((int*)(hcd->regs))[4]);
	printk("TMIO_OHCI: irq = %d\n", hcd->irq);

	dev->dma_mask = 0xffffffffUL;
        dev->coherent_dma_mask = 0xffffffffUL;

	retval = hcd_buffer_create (hcd);
	if (retval != 0) {
		dbg ("pool alloc fail");
		goto err1;
	}

	retval = request_irq (hcd->irq, usb_hcd_tmio_hcim_irq, SA_INTERRUPT,
			      hcd->driver->description, hcd);

	if (retval != 0) {
		dbg("request_irq failed");
		retval = -EBUSY;
		goto err2;
	}

        /* Enable interrupts... */
        writeb(0x2, conf_base + 0x50);
        /* Turn on the power */
        writew(0x3, conf_base + 0x4c);


//	set_irq_type(hcd->irq, IRQT_FALLING);

	info ("%s (tmio) at 0x%p, irq %d\n",
	      hcd->driver->description, hcd->regs, hcd->irq);

	hcd->self.bus_name = "tmio";
	usb_register_bus (&hcd->self);

	if ((retval = driver->start (hcd)) < 0)
	{
		printk("OOOPSie!!!!\n"); //FIXME - die gracefully.
		return retval;
	}

	return 0;

 err2:
	hcd_buffer_destroy (hcd);
 err1: // more cleanup needs doing
	return retval;
}


/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/*-------------------------------------------------------------------------*/

static int __devinit
ohci_tmio_start (struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci (hcd);
	int		ret;


	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	printk("OHCI-TMIO: %08x %08x \n", ohci->hcca, ohci->hcca_dma);

/*	if ((ret = ohci_mem_init (ohci)) < 0) {
		ohci_stop (hcd);
		return ret;
	}
	ohci->regs = hcd->regs;

	if (hc_reset (ohci) < 0) {
		ohci_stop (hcd);
		return -ENODEV;
	}
*/
	if ((ret = ohci_run (ohci)) < 0) {
		err ("can't start %s", hcd->self.bus_name);
		ohci_stop (hcd);
		return ret;
	}

	return 0;
}

/*-------------------------------------------------------------------------*/

static const struct hc_driver tmio_ohci_hc_driver = {
	.description =		hcd_name,
	.product_desc = "Toshiba Mobile IO Controller (TMIO) OHCI",
	.hcd_priv_size =        sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_tmio_start,
	.stop =			ohci_stop,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,

	.start_port_reset =     ohci_start_port_reset,
};

/*-------------------------------------------------------------------------*/

static int tmio_ohci_probe(struct device *dev)
{
	struct usb_hcd *hcd = NULL;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	ret = __tmio_ohci_probe(&tmio_ohci_hc_driver, &hcd, dev);

//	if (ret == 0)
//		tmio_set_drvdata(dev, hcd);

	return ret;
}

/* ----------------------- SoC driver setup ---------------------------- */

static platform_device_id tmio_platform_device_ids[] = {TMIO_USB_DEVICE_ID, 0};

struct device_driver tmio_ohci_device = {
	.name = "tmio_ohci",
	.probe = tmio_ohci_probe,
//                .remove = tmio_ohci_remove,
};

static int __init tmio_ohci_init(void)
{
        printk("tmio_ohci_init()\n");
        return driver_register (&tmio_ohci_device);
}

static void __exit tmio_ohci_exit(void)
{
        return driver_unregister (&tmio_ohci_device);
}

module_init(tmio_ohci_init);
module_exit(tmio_ohci_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ian Molton and Sebastian Carlier");
MODULE_DESCRIPTION("USB driver for Toshiba Mobile IO Controller (TMIO)");

