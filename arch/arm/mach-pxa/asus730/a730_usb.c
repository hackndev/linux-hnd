/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * History:
 *
 * 2005-03-26	Markus Wagner		file created
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>

#include <asm/arch/asus730-init.h>
#include <asm/arch/asus730-gpio.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/ohci.h>

#include "../generic.h"

/* USB host */

static int a730_ohci_init(struct device *dev)
{
	UHCHCON = (UHCHCON_HCFS_USBOPERATIONAL | /*UHCHCON_PLE |*/ UHCHCON_CLE | UHCHCON_CBSR41);//0x97 (USBOPERATIONAL | CLE(may be not set?) | PLE | CBSR=0x3)
	UHCINTE = (UHCINT_MIE | UHCINT_RHSC | UHCINT_UE | UHCINT_WDH | UHCINT_SO);//0x80000053;// (MIE | RHSC | UE | WDH | SO)
	UHCINTD = (UHCINT_MIE | UHCINT_RHSC | UHCINT_UE | UHCINT_WDH | UHCINT_SO);//0x80000053;// (MIE | RHSC | UE | WDH | SO)
	UHCFMI = 0x27782edf;
	//UHCFMR = 0x2d6b;// (no need to set ?)
	//UHCFMN = 0xd1bc;// (no need to set ?)
	//intel says typical val is 0x3e67. wince sets to 0x2a2f
	UHCPERS = 0x3e67;//0x2a2f
	UHCLS = 0x628;// (lsthreshold=0x628)
	UHCRHDA = 0x4001a02;// (numberdownstreamports=1 | psm=1 | overcurrentprotection=1 | noovercurrentprotection=1 | powerontopowergoodtime(bit26)=1)
	UHCRHDB = 0x0;
	UHCRHS = 0x0;
	UHCRHPS1 = 0x100;// (port power on)
	UHCRHPS2 = 0x100;// (port power on)
	UHCRHPS3 = 0x100;// (port power on)
	UHCSTAT = 0x0;
	UHCHR = (UHCHR_PCPL | UHCHR_CGR);//0x84 (power control polarity low | clock generation reset inactive)
	UHCHIE = 0x0;
	UHCHIT = 0x0;

	SET_A730_GPIO(USB_HOST_EN, 0);

	return 0;
}

static void a730_ohci_exit(struct device *dev)
{
	SET_A730_GPIO(USB_HOST_EN, 1);
	UHCRHPS1 = 0x0;// (port power off)
	UHCRHPS2 = 0x0;// (port power off)
	UHCRHPS3 = 0x0;// (port power off)
}

static struct pxaohci_platform_data a730_ohci_platform_data = {
	.init		= a730_ohci_init,
	.exit		= a730_ohci_exit,
	.port_mode	= /*PMM_NPS_MODE,//*/PMM_PERPORT_MODE,
};

static int __init a730_usb_init(void)
{
	if (!machine_is_a730()) return -ENODEV;

	printk("A730: Enabling USB-Host controller\n");

	pxa_set_ohci_info(&a730_ohci_platform_data);
	
	return 0;
}

static void __exit a730_usb_exit(void)
{
	printk("A730: Disabling USB-Host controller\n");
}

module_init(a730_usb_init);
module_exit(a730_usb_exit);

MODULE_AUTHOR("Markus Wagner <markus1108wagner@t-online.de");
MODULE_DESCRIPTION("Asus MyPal A730(W) USB Host status driver");
MODULE_LICENSE("GPL");
