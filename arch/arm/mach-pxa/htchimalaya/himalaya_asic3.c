/* LCD */

static struct platform_device himalaya_lcd = {
  .name = "himalaya-lcd",
  .id = -1,
  .dev = {
    .platform_data = NULL,
  },
};

/* 
 * All the asic3 dependant devices 
 */

extern struct platform_device himalaya_bl;
static struct platform_device himalaya_udc = { .name = "himalaya-udc", };
//static struct platform_device himalaya_leds = { .name = "himalaya-leds", };
#ifdef CONFIG_HIMALAYA_PCMCIA
static struct platform_device himalaya_pcmcia    = { .name =
                                            "htchimalaya_pcmcia", };
#endif

static struct platform_device *himalaya_asic3_devices[] __initdata = {
  &himalaya_lcd,
  &himalaya_udc,
//  &himalaya_leds,
};

/*
 * the ASIC3 should really only be referenced via the asic3_base
 * module.  it contains functions something like asic3_gpio_b_out()
 * which should really be used rather than macros.
 *
 */

static int himalaya_get_mmc_ro(struct platform_device *dev)
{
 return (((asic3_get_gpio_status_d( &himalaya_asic3.dev )) & (1<<GPIOD_SD_WRITE_PROTECT)) != 0);
}

static struct tmio_mmc_hwconfig himalaya_mmc_hwconfig = {
        .mmc_get_ro = himalaya_get_mmc_ro,
};

static struct asic3_platform_data asic3_platform_data = {
	.gpio_a = {
		.dir		= 0xbfff,
		.init		= 0x4061, /* or 406b */
		.sleep_out	= 0x4001,
		.batt_fault_out	= 0x4001,
		.sleep_conf	= 0x000c,
		.alt_function	= 0x9800, 
	},
	.gpio_b = {
		.dir		= 0xffff,
		.init		= 0x0f98, /* or 0fb8 */
		.sleep_out	= 0x8220,
		.batt_fault_out	= 0x0220,
		.sleep_conf	= 0x000c,
		.alt_function	= 0x0000, 
	},
	.gpio_c = {
		.dir		= 0x0187,
		.init		= 0xfe04,             
		.sleep_out	= 0xfe00,            
		.batt_fault_out	= 0xfe00,            
		.sleep_conf	= 0x0008, 
		.alt_function	= 0x0003,
	},
	.gpio_d = {
		.dir		= 0x10e0,
		.init		= 0x6907,            
		.sleep_mask	= 0x0000,
		.sleep_out	= 0x6927,            
		.batt_fault_out = 0x6927,  
		.sleep_conf	= 0x0008, 
		.alt_function	= 0x0000,
	},
	.bus_shift		 = 2,
	.irq_base = HTCHIMALAYA_ASIC3_IRQ_BASE,

	.child_devs     = himalaya_asic3_devices,
	.num_child_devs = ARRAY_SIZE(himalaya_asic3_devices),
	.tmio_mmc_hwconfig	 = &himalaya_mmc_hwconfig,
};

static struct resource asic3_resources[] = {
	[0] = {
		.start  = HIMALAYA_ASIC3_GPIO_PHYS,
		.end    = HIMALAYA_ASIC3_GPIO_PHYS + 0xfffff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_NR_HIMALAYA_ASIC3,
		.end    = IRQ_NR_HIMALAYA_ASIC3,
		.flags  = IORESOURCE_IRQ,
	},
	[2] = {
		.start  = HIMALAYA_ASIC3_MMC_PHYS,
		.end    = HIMALAYA_ASIC3_MMC_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = IRQ_GPIO(GPIO_NR_HIMALAYA_SD_IRQ_N),
		.flags  = IORESOURCE_IRQ,
	},
};


struct platform_device himalaya_asic3 = {
	.name       = "asic3",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(asic3_resources),
	.resource   = asic3_resources,
	.dev = {
		.platform_data = &asic3_platform_data
	},
};
EXPORT_SYMBOL_GPL(himalaya_asic3);

static struct platform_device *devices[] __initdata = {
	&himalaya_asic3,
};
