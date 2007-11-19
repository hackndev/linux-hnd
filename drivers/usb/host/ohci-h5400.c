/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 * (C) Copyright 2002 Hewlett-Packard Company
 * 
 * HP iPAQ H5400 Bus Glue
 *
 * Written by Catalin Drula <catalin@cs.utoronto.ca>
 * Largely based on ohci-sa1111.c and tmio_ohci.c
 *
 * This file is licenced under the GPL.
 */

#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/hardware/ipaq-samcop.h>

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#ifndef CONFIG_ARCH_H5400
#error "This file is iPAQ H5xxx bus glue.  CONFIG_ARCH_H5400 must be defined."
#endif

static u64 ohci_dmamask = 0xffffffffUL;

extern int usb_disabled(void);

static struct clk *clk_usb = NULL;
static struct clk *clk_uclk = NULL;


/*-------------------------------------------------------------------------*/

static void h5400_start_hc(struct platform_device *dev)
{
	clk_enable(clk_usb);
	clk_enable(clk_uclk);
	
	mdelay(250);
};

static void h5400_stop_hc (struct platform_device *dev)
{
	if (!IS_ERR(clk_usb))
		clk_disable(clk_usb);

	if (!IS_ERR(clk_uclk))
		clk_disable(clk_uclk);
};

void __ohci_h5400_remove(struct usb_hcd *, struct platform_device *);


/**
 * __ohci_h5400_probe - initialize Samcop-based HCDs
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 *
 */
int __ohci_h5400_probe(const struct hc_driver *driver,
		       struct usb_hcd **hcd_out,
		       struct platform_device *sdev)
{
	int retval = 0;
	struct usb_hcd *hcd = NULL;
	struct ohci_hcd *ohci;

	dmabounce_register_dev(&sdev->dev, 512, 4096);

	/* taken from tmio_ohci, have to see if they are needed */
	sdev->dev.dma_mask = &ohci_dmamask;
	sdev->dev.coherent_dma_mask = 0xffffffffUL;

	hcd = usb_create_hcd(driver, &sdev->dev, "h5400-ohci");
	if (hcd == NULL) {
		dbg("usb_create_hcd failed");
		retval = -ENOMEM;
		goto err1;
	}

	clk_usb = clk_get (&sdev->dev, "usb");
	clk_uclk = clk_get (&sdev->dev, "uclk");
	if (IS_ERR (clk_usb) || IS_ERR (clk_uclk)) {
		printk (KERN_ERR "Could not find samcop usb clocks\n");
		goto err2;
	};

	hcd->self.controller = &sdev->dev;
	hcd->rsrc_start = sdev->resource[0].start;
	hcd->rsrc_len = sdev->resource[0].end - sdev->resource[0].start + 1;
	hcd->regs = ioremap(sdev->resource[0].start, 
			    sdev->resource[0].end - sdev->resource[0].start + 1);

	if (!hcd->regs) {
		dbg ("Could not ioremap OHCI registers\n");
		goto err2;
	};


	ohci = hcd_to_ohci(hcd);
	ohci_hcd_init(ohci);

	h5400_start_hc (sdev);

	retval = usb_add_hcd (hcd, platform_get_irq(sdev, 0), SA_INTERRUPT);

	if ((retval) < 0) 
	{
		dbg("Could not add HCD!!!\n");
		goto err2;
	}

	*hcd_out = hcd;
	return 0;

 err2:
    if (!IS_ERR (clk_usb)) clk_put (clk_usb);
	if (!IS_ERR (clk_uclk)) clk_put (clk_uclk);
	usb_remove_hcd(hcd);
	usb_put_hcd (hcd);

 err1:
	h5400_stop_hc (sdev);

	dmabounce_unregister_dev(&sdev->dev);
	return retval;
}


/**
 * __ohci_h5400_remove - shutdown processing for Samcop-based HCDs
 * @hcd: USB Host Controller being removed
 * @sdev: Samcop USB device
 * Context: !in_interrupt()
 *
 * Reverses the effect of __ohci_h5400_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
void __ohci_h5400_remove(struct usb_hcd *hcd, struct platform_device *sdev)
{
	struct ohci_hcd *ohci = hcd_to_ohci (hcd);
	if (hcd == NULL) {
		dbg("This should not happen\n");
		return;
	}

	info ("remove: %s, state %x", hcd->self.bus_name, hcd->state);

	if (in_interrupt ())
		BUG ();

	/* disable interrupts from OHCI */
	ohci_writel (ohci, OHCI_INTR_MIE, &ohci->regs->intrdisable);

	/* this calls ohci_stop second time, and with interrupts already disables,
	 * this definitely stops rh_timer handler. This has to be fixed in
	 * core/hcd.c and/or host/ohci-hcd.c */

	usb_remove_hcd(hcd);
	
	h5400_stop_hc(sdev);

	usb_put_hcd(hcd);

	iounmap (hcd->regs);
	dmabounce_unregister_dev(&sdev->dev);

	clk_put (clk_usb);
	clk_put (clk_uclk);
}


/*-------------------------------------------------------------------------*/

static int __devinit ohci_h5400_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	int ret;

	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	dbg("ohci-h5400: 0x%08x 0x%08x\n", (unsigned int)ohci->hcca, ohci->hcca_dma);

	if ((ret = ohci_run(ohci)) < 0) {
		ohci_err(ohci, "can't start %s\n", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
        }

#ifdef DEBUG
	ohci_dump(ohci, 1);
#endif

	return 0;
}

/*-------------------------------------------------------------------------*/
/* TODO: Implement this if possible, then enable it in hc_driver
static int ohci_h5400_suspend (struct usb_hcd *hcd)
{
};

static int ohci_h5400_resume (struct usb_hcd *hcd)
{
};*/
/*-------------------------------------------------------------------------*/

static const struct hc_driver ohci_h5400_hc_driver = {
	.description =		hcd_name,
        .product_desc =         "h5400 OHCI",
        .hcd_priv_size =        sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_h5400_start,
#ifdef	CONFIG_PM
/*	.suspend =		ohci_h5400_suspend,
	.resume =		ohci_h5400_resume,*/
#endif
	.stop =			ohci_stop,
	.shutdown =		ohci_shutdown,

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
	.hub_irq_enable =	ohci_rhsc_enable,
#ifdef	CONFIG_PM
	.bus_suspend =		ohci_bus_suspend,
	.bus_resume = 		ohci_bus_resume,
#endif


	.start_port_reset =     ohci_start_port_reset,
};

/*-------------------------------------------------------------------------*/

static int ohci_h5400_probe(struct platform_device *dev)
{
	struct usb_hcd *hcd = NULL;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	ret = __ohci_h5400_probe(&ohci_h5400_hc_driver, &hcd, dev);

	platform_set_drvdata(dev, hcd);

	return ret;
}

static int ohci_h5400_remove(struct platform_device *dev)
{
	struct usb_hcd *hcd = (struct usb_hcd *)platform_get_drvdata(dev);

	__ohci_h5400_remove(hcd, dev);
	platform_set_drvdata (dev, NULL);

	hcd = NULL;

	return 0;
}

//static platform_device_id h5400_usb_device_ids[] = { {IPAQ_SAMCOP_USBH_DEVICE_ID}, {0} };

static struct platform_driver ohci_h5400_driver = {
	.probe = ohci_h5400_probe,
	.remove = ohci_h5400_remove,
	.driver = {
/* needed to get 'probe' called correctly */
		.name = "samcop usb host",
	},
};
