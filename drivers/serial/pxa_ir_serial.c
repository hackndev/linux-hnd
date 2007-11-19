/*
 * pxa_ir_serial.c - Speak serial protocol over Infra-red.
 *
 * This is driver to use PXA IR port for raw serial communication. This
 * makes it different from pxaficp_ir.c, which is intended to speak high-level
 * IrDA protocol. It is still possible to use SIR IrDA with this driver by
 * attaching corresponding line discipline (irattach does this from userspace).
 * This driver reuses struct pxaficp_platform_data. Both drivers can be easily
 * switched on a port.
 *
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/hardware.h>
#include <asm/arch/irda.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa-regs.h>

// FICP device on which we piggyback
static struct device *pxaficp_dev;

static void
pxa_ir_configure(int state)
{
	printk("pxa2xx-ir-serial: %s\n", __FUNCTION__);
	/* Switch STUART RX/TX pins to SIR */
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);
	/* make sure FIR ICP is off */
	ICCR0 = 0;

	switch (state) {

	case PXA_UART_CFG_POST_STARTUP: /* post UART enable */
		/* configure STUART to for SIR */
		STISR = STISR_XMODE | STISR_RCVEIR | STISR_RXPL;
		((struct pxaficp_platform_data*)pxaficp_dev->platform_data)->transceiver_mode(pxaficp_dev, IR_SIRMODE);
		break;

	case PXA_UART_CFG_POST_SHUTDOWN: /* UART disabled */
		STISR = 0;
		((struct pxaficp_platform_data*)pxaficp_dev->platform_data)->transceiver_mode(pxaficp_dev, IR_OFF);
		break;

	default:
		break;
	}
}

static void
pxa_ir_set_txrx(int txrx)
{
	unsigned old_stisr = STISR;
	unsigned new_stisr = old_stisr;

	printk("pxa2xx-ir-serial: %s\n", __FUNCTION__);
	if (txrx & PXA_SERIAL_TX) {
		/* Ignore RX if TX is set */
		txrx &= PXA_SERIAL_TX;
		new_stisr |= STISR_XMITIR;
	} else
		new_stisr &= ~STISR_XMITIR;

	if (txrx & PXA_SERIAL_RX)
		new_stisr |= STISR_RCVEIR;
	else
		new_stisr &= ~STISR_RCVEIR;

	if (new_stisr != old_stisr) {
		while (!(STLSR & LSR_TEMT)) ;
		((struct pxaficp_platform_data*)pxaficp_dev->platform_data)->transceiver_mode(pxaficp_dev, IR_OFF);
		STISR = new_stisr;
		((struct pxaficp_platform_data*)pxaficp_dev->platform_data)->transceiver_mode(pxaficp_dev, IR_SIRMODE);
	}
}

static int
pxa_ir_get_txrx(void)
{
	printk("pxa2xx-ir-serial: %s\n", __FUNCTION__);
	return ((STISR & STISR_XMITIR) ? PXA_SERIAL_TX : 0) |
	       ((STISR & STISR_RCVEIR) ? PXA_SERIAL_RX : 0);
}

static struct platform_pxa_serial_funcs pxa_ir_funcs = {
	.configure = pxa_ir_configure,
	.set_txrx  = pxa_ir_set_txrx,
	.get_txrx  = pxa_ir_get_txrx,
};



static int match_by_name(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	return strcmp(pdev->name, data) == 0;
}

static int __init pxa_ir_serial_init(void)
{
	pxaficp_dev = bus_find_device(&platform_bus_type, NULL, "pxa2xx-ir", match_by_name);
	if (!pxaficp_dev) {
		printk(KERN_ERR "pxa2xx-ir-serial: Parent pxa2xx-ir device not found\n");
		return -ENODEV;
	}

	pxa_set_stuart_info(&pxa_ir_funcs);
	printk(KERN_INFO "pxa2xx-ir-serial: Initialized\n");
	return 0;
}

module_init(pxa_ir_serial_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrew Zabolotny, Matt Reimer, Paul Sokolovsky");
