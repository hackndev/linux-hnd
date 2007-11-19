
static void ser_stuart_gpio_config(int mode)
{
	/* standard uart initialisation: might as well get it over with, here... */
	/* the standard uart apparently deals with GSM commands */
	//pxa_gpio_mode(GPIO46_STRXD_MD); // STD_UART receive data
	//pxa_gpio_mode(GPIO47_STTXD_MD); // STD_UART transmit data

	GPDR(GPIO_NR_HIMALAYA_UART_POWER) |= GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
	GPSR(GPIO_NR_HIMALAYA_UART_POWER) = GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);

	asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 1<<GPIOA_RS232_ON);
	asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<GPIOA_RS232_ON, 1<<GPIOA_RS232_ON);

	asic3_set_gpio_dir_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 1<<GPIOB_GSM_RESET);

	pxa_gpio_op(GPIO36_FFDCD, GPIO_MD_MASK_NR | 
			                  GPIO_MD_MASK_DIR | GPIO_MD_MASK_FN,
			                  GPIO_OUT);

	// set GPIO36 to high
	pxa_gpio_op(GPIO36_FFDCD, GPIO_MD_MASK_SET, GPIO_MD_HIGH);

	/* okay.  from radio_extbootldr, we have to clear the
	 * gpio36 alt-gpio, set the direction to out,
	 * and set the gpio36 line high.
	 *
	 */
	printk("gsm reset\n");
	//asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<2, 1<<2);
	//asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<2, 1<<2);

	asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<2, 1<<2);
	asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<2, 0);

	/* then power on the gsm */
	printk("gsm reset gpio off\n");
	asic3_set_gpio_out_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 0);
	msleep(1);

	/* then switch it off... */
	printk("gsm reset gpio on\n");
	asic3_set_gpio_out_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 1<<GPIOB_GSM_RESET);
	msleep(1);

	/* then switch it on again... */
	printk("gsm reset gpio off\n");
	asic3_set_gpio_out_b(&himalaya_asic3.dev, 1<<GPIOB_GSM_RESET, 0);
	//msleep(1);

	//asic3_set_gpio_out_b(&himalaya_asic3.dev, GPIOB_GSM_RESET, GPIOB_GSM_RESET);
	//asic3_set_gpio_dir_a(&himalaya_asic3.dev, 1<<2, 1<<2);
	//asic3_set_gpio_out_a(&himalaya_asic3.dev, 1<<2, 1<<2);

	pxa_gpio_op(GPIO36_FFDCD, GPIO_MD_MASK_NR | 
			                  GPIO_MD_MASK_DIR,
			                  GPIO_ALT_FN_1_IN);

	//asic3_set_gpio_dir_a(&himalaya_asic3.dev, GPIOA_USB_ON_N, GPIOA_USB_ON_N);
	//asic3_set_gpio_out_a(&himalaya_asic3.dev, GPIOA_RS232_ON, GPIOA_RS232_ON);

	//asic3_set_gpio_out_b(&himalaya_asic3.dev, GPIOB_GSM_RESET, GPIOB_GSM_RESET);
}
