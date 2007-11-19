/*************************************************************************************/
/*     
       Navman GPS/CF Sleeve
*/
/*************************************************************************************/

/* adapted from drivers/char/pcmcia/serial_cs.c (where it is a static procedure) */
static int setup_serial(ioaddr_t port, int irq)
{
	struct serial_struct serial;
	int line;
    
	memset(&serial, 0, sizeof(serial));
	serial.port = port;
	serial.irq = irq;
	serial.flags = ASYNC_SKIP_TEST | ASYNC_SHARE_IRQ;
	serial.baud_base = 230400;
	line = register_serial(&serial);
	if (line < 0) {
		printk(KERN_NOTICE " " __FILE__ ": register_serial() at 0x%04lx,"
		       " irq %d failed\n", (u_long)serial.port, serial.irq);
	}
	return line;
}

static int serial_port = 0;
#ifdef CONFIG_ARCH_SA1100
static int serial_irq = IRQ_GPIO_H3600_OPT_IRQ;
#elif defined(CONFIG_MACH_H3900)
static int serial_irq = IRQ_GPIO_H3900_OPT_INT;
#else
#warning need serial_irq for non sa1100
static int serial_irq = 0;
#endif
MODULE_PARM(serial_port, "i");
MODULE_PARM(serial_irq, "i");
MODULE_LICENSE("Dual BSD/GPL");
static int serial_line = -1;
static void *serial_ioaddr = 0;

static int __devinit navman_gps_cf_probe_sleeve(struct ipaq_sleeve_device *sleeve_dev, 
						const struct ipaq_sleeve_device_id *ent)
{
	set_h3600_egpio(IPAQ_EGPIO_OPT_ON);

	serial_ioaddr = ioremap(0x30000000 + serial_port, PAGE_SIZE);
	SLEEVE_DEBUG(0, "%s serial_port=%x serial_ioaddr=%p\n", 
		      sleeve_dev->driver->name, serial_port, serial_ioaddr);
	h3600_pcmcia_change_sleeves(&buffered_pcmcia_socket_ops);

#ifdef CONFIG_ARCH_SA1100
	GPDR &= ~GPIO_H3600_OPT_IRQ;	/* GPIO line as input */
	set_irq_type( GPIO_H3600_OPT_IRQ, IRQT_RISING );  /* Rising edge */
#elif defined(CONFIG_MACH_H3900)
	GPDR0 &= ~GPIO_H3900_OPT_INT;	/* GPIO line as input */
        set_irq_type( GPIO_NR_H3900_OPT_INT, IRQT_RISING );  /* Rising edge */
#elif defined(CONFIG_ARCH_H5400)
	#warning need to set up serial irq for navman sleeve for h5400
#else
#error unsupported architecture in navman_gps_cf_probe_sleeve 
#endif
	serial_line = setup_serial((ioaddr_t) serial_ioaddr,  serial_irq);
	return 0;
}

static void __devexit navman_gps_cf_remove_sleeve(struct ipaq_sleeve_device *sleeve_dev)
{
	SLEEVE_DEBUG(0, "%s\n", sleeve_dev->driver->name);
	if (serial_line > 0)
		unregister_serial(serial_line);
	if (serial_ioaddr) 
		iounmap(serial_ioaddr);
	h3600_pcmcia_remove_sleeve();

	clr_h3600_egpio(IPAQ_EGPIO_OPT_ON);
}

static struct ipaq_sleeve_device_id navman_gps_cf_tbl[] __devinitdata = {
	{ TALON_VENDOR, NAVMAN_GPS_SLEEVE },
	{ TALON_VENDOR, NAVMAN_GPS_SLEEVE_ALT },
	{ 0, }
};

static struct ipaq_sleeve_driver navman_gps_cf_driver = {
	.name =	  "Navman GPS/CF Sleeve",
	.id_table = navman_gps_cf_tbl,
	.features = (SLEEVE_HAS_CF(1) | SLEEVE_HAS_GPS),
	.probe =	  navman_gps_cf_probe_sleeve,
	.remove =	  navman_gps_cf_remove_sleeve,
 	.suspend =  generic_suspend_sleeve,
 	.resume =   generic_resume_sleeve,
};

