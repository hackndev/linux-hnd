#include <linux/platform_device.h>

#define MMC_CLOCK_DISABLED 0
#define MMC_CLOCK_ENABLED  1

#define TMIO_WP_ALWAYS_RW ((void*)-1)

struct tmio_mmc_hwconfig {
	void (*hwinit)(struct platform_device *sdev);
	void (*set_mmc_clock)(struct platform_device *sdev, int state);

	/* NULL - use ASIC3 signal, 
	   TMIO_WP_ALWAYS_RW - assume always R/W (e.g. miniSD) 
	   otherwise - machine-specific handler */
	int (*mmc_get_ro)(struct platform_device *pdev);
	short address_shift;
};
