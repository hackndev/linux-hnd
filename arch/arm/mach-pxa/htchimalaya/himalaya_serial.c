
#if 0
static irqreturn_t himalaya_rs232_irq(int irq, void *dev_id)
{
	int connected = 0;

	if ((GPLR(GPIO_NR_HIMALAYA_RS232_IN) & GPIO_bit(GPIO_NR_HIMALAYA_RS232_IN)))
		connected = 1;

	if (connected) {
		printk("Himalaya: RS232 connected\n");
		GPDR(GPIO_NR_HIMALAYA_UART_POWER) |= GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
		GPSR(GPIO_NR_HIMALAYA_UART_POWER) = GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
		asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 1<<GPIOA_RS232_ON);
		asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 1<<GPIOA_RS232_ON);
	} else {
		printk("Himalaya: RS232 disconnected\n");
		GPDR(GPIO_NR_HIMALAYA_UART_POWER) |= GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
		GPCR(GPIO_NR_HIMALAYA_UART_POWER) = GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
		asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 1<<GPIOA_RS232_ON);
		asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 0);
	}
	return IRQ_HANDLED;
}

static struct irqaction himalaya_rs232_irqaction = {
        name:		"himalaya_rs232",
	handler:	himalaya_rs232_irq,
	flags:		SA_INTERRUPT
};
#endif

#if 0
static void ser_ffuart_gpio_config(int mode)
{
	/* 
	 * full-function uart initialisation: set this up too.
	 *
	 * even though the bootloader (xscale.S) loads them up,
	 * we blatted them all, above!
	 */

	GPDR(GPIO_NR_HIMALAYA_UART_POWER) |= GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
	GPSR(GPIO_NR_HIMALAYA_UART_POWER) = GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);

	/* for good measure, power on the rs232 uart */
	asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 1<<GPIOA_RS232_ON);
	asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 1<<GPIOA_RS232_ON);

	/* okay.  from radio_extbootldr, we have to clear the
	 * gpio36 alt-gpio, set the direction to out,
	 * and set the gpio36 line high.
	 *
	 * lkcl01jan2006: the pxa_gpio_op() function is what i used
	 * to get this sequence right.  what was replaced here - 
	 * a call to pxa_gpio_mode() and a manual GADR(...) call - 
	 * was complete rubbish.
	 *
	 * using pxa_gpio_op() is _so_ much more obvious and simple.
	 * and correct.
	 */
	printk("gsm reset\n");
	pxa_gpio_op(GPIO36_FFDCD, GPIO_MD_MASK_NR | 
			                  GPIO_MD_MASK_DIR | GPIO_MD_MASK_FN,
			                  GPIO_OUT);

	// set GPIO 36 to high.
	pxa_gpio_op(GPIO36_FFDCD, GPIO_MD_MASK_SET, GPIO_MD_HIGH);

	/* then power on the gsm (and out direction for good measure) */
	printk("gsm reset gpio off\n");
	asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<GPIOA_USB_ON_N, 0);

	asic3_set_gpio_out_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 0);
	msleep(1);

	/* then switch it off... */
	printk("gsm reset gpio on\n");
	asic3_set_gpio_out_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 1<<GPIOB_GSM_RESET);
	msleep(1);

	/* then switch it on again... */
	printk("gsm reset gpio off\n");
	asic3_set_gpio_out_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 0);
	msleep(1);

	/* and finally, enable the gpio thingy.  set all of them just for fun */
	//pxa_gpio_mode(GPIO34_FFRXD_MD);
	//pxa_gpio_mode(GPIO35_FFCTS_MD);
	pxa_gpio_mode(GPIO36_FFDCD_MD);
	//pxa_gpio_mode(GPIO37_FFDSR_MD);
	//pxa_gpio_mode(GPIO38_FFRI_MD);
	//pxa_gpio_mode(GPIO39_FFTXD_MD);
	//pxa_gpio_mode(GPIO40_FFDTR_MD);
	//pxa_gpio_mode(GPIO41_FFRTS_MD);
	//pxa_gpio_op(GPIO36_FFDCD, GPIO_MD_MASK_SET, 0);
	//
	asic3_set_gpio_out_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 1<<GPIOB_GSM_RESET);
	asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<GPIOA_USB_ON_N, 1<<GPIOA_USB_ON_N);
	asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<GPIOA_USB_ON_N, 1<<GPIOA_USB_ON_N);
	msleep(1);
}
