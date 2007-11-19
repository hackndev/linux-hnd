/*
 * File created for Jornada 820...
 * Id: sa1101.h,v 1.10 2005/07/29 09:56:34 fare
 */
#ifndef _ASM_ARCH_SA1101
#define _ASM_ARCH_SA1101

#include <asm/arch/bitfield.h>

#define SA1101_IRQMASK_LO(x)	(1 << (x - IRQ_SA1101_START))
#define SA1101_IRQMASK_HI(x)	(1 << (x - IRQ_SA1101_START - 32))

#ifndef __ASSEMBLY__

/* TODO: driver interface */
/*------------------------*/
extern int sa1101_probe(struct device *dev);

extern void sa1101_wake(void);
extern void sa1101_doze(void);

extern void sa1101_init_irq(int irq_nr);

extern int sa1101_pcmcia_init(void *handler);
extern int sa1101_pcmcia_shutdown(void);

extern int sa1101_usb_init(void);
extern int sa1101_usb_shutdown(void);

extern int sa1101_vga_init(void);
extern int sa1101_vga_shutdown(void);

/*------------------------*/

#define sa1101_writel(val,addr)	({ *(volatile unsigned int *)(addr) = (val); })
#define sa1101_readl(addr)	(*(volatile unsigned int *)(addr))

#define SA1101_DEVID_SBI	0
#define SA1101_DEVID_SK		1
#define SA1101_DEVID_USB	2
#define SA1101_DEVID_VGA	3
#define SA1101_DEVID_IEEE	4
#define SA1101_DEVID_PS2	5
#define SA1101_DEVID_GPIO	6
#define SA1101_DEVID_INT	7
#define SA1101_DEVID_PCMCIA	8
#define SA1101_DEVID_KEYPAD	9

struct sa1101_dev {
	struct device	dev;
	unsigned int	devid;
	struct resource	res;
	void		*mapbase;
	unsigned int	skpcr_mask;
	unsigned int	irq[6];
};

#define SA1101_DEV(_d)	container_of((_d), struct sa1101_dev, dev)

#define sa1101_get_drvdata(d)	dev_get_drvdata(&(d)->dev)
#define sa1101_set_drvdata(d,p)	dev_set_drvdata(&(d)->dev, p)

struct sa1101_driver {
	struct device_driver	drv;
	unsigned int		devid;
	int (*probe)(struct sa1101_dev *);
	int (*remove)(struct sa1101_dev *);
	int (*suspend)(struct sa1101_dev *, u32);
	int (*resume)(struct sa1101_dev *);
};

#define SA1101_DRV(_d)	container_of((_d), struct sa1101_driver, drv)

#define SA1101_DRIVER_NAME(_sadev) ((_sadev)->dev.driver->name)

void sa1101_enable_device(struct sa1101_dev *);
void sa1101_disable_device(struct sa1101_dev *);

int sa1101_driver_register(struct sa1101_driver *);
void sa1101_driver_unregister(struct sa1101_driver *);


#endif

#endif  /* _ASM_ARCH_SA1101 */
