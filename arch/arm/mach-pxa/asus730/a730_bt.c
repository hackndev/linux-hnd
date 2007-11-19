/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
       hciattach /dev/ttyS1 bcsp 921600
 */
#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus730-gpio.h>

static int enabled = 0;

ssize_t btpwr_show(struct device_driver* drv, char* buf)
{
       return sprintf(buf,"%d\n",enabled);
}

ssize_t btpwr_set(struct device_driver* drv, const char* buf, size_t count)
{
       int v;

       if (sscanf(buf, "%d", &v) != 1 || v == enabled)
       {
               return count;
       }
       enabled = v;
       printk("changing to %d\n",v);
       SET_A730_GPIO(BT_POWER1, enabled);
       SET_A730_GPIO(BT_POWER2, enabled);
       return count;
}

static DRIVER_ATTR(power, 0644, btpwr_show, btpwr_set);
/* /sys/devices/platform/a730-bt-power/driver/power */

static int bt_probe(struct platform_device *dev)
{
       printk("%s\n",__PRETTY_FUNCTION__);
       return 0;
}

static int bt_resume(struct platform_device *dev)
{
//       if (level != RESUME_ENABLE) return 0;
//       printk("%s lvl=%d\n",__PRETTY_FUNCTION__,level);
       return 0;
}

static int bt_remove (struct platform_device * dev)
{
       printk("%s\n",__PRETTY_FUNCTION__);
       return 0;
}

static int bt_suspend(struct platform_device *dev, pm_message_t state)
{
//       printk("%s lvl=%d\n",__PRETTY_FUNCTION__,level);
       return 0;
}

/*static void bt_shdn(struct device * dev)
{
       printk("%s\n",__PRETTY_FUNCTION__);
}*/

static void bt_rel (struct device * dev)
{
       printk("%s\n",__PRETTY_FUNCTION__);
}

static struct platform_driver a730_bt_driver = {
       .driver	= {
	       .name = "a730-bt-power",
       },
       .probe          = bt_probe,
       .remove         = bt_remove,
       .shutdown       = NULL,
#ifdef CONFIG_PM
       .suspend        = bt_suspend,//null, not used
       .resume         = bt_resume,
#endif
};

static struct platform_device platform_btpwr = {
    .name = "a730-bt-power", //driver name
    .id = -1,
    .dev = { /* struct device */
	.release = bt_rel,//null, not used
    },
    .num_resources = 0,
};

static int a730_btpwr_init (void)
{
       platform_driver_register(&a730_bt_driver);
       platform_device_register(&platform_btpwr);
       driver_create_file(&a730_bt_driver.driver, &driver_attr_power);
       return 0;
}

static void a730_btpwr_exit(void)
{
       driver_remove_file(&a730_bt_driver.driver, &driver_attr_power);
       platform_device_unregister(&platform_btpwr);//remove device
       platform_driver_unregister(&a730_bt_driver);
}

module_init(a730_btpwr_init);
module_exit(a730_btpwr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Sroczynski <msroczyn@elka.pw.edu.pl>");
MODULE_DESCRIPTION("Bluetooth power control driver for Asus MyPal A730(W)");
