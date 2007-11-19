/*
*
* Driver for the HP iPAQ Mercury Backpaq gasgauge
*
* Copyright 2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*                   
* Author: Jamey Hicks
*         <jamey@handhelds.org>
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <asm/uaccess.h>         /* get_user,copy_to_user*/

#include <linux/proc_fs.h> 
#include <asm/arch-sa1100/backpaq.h>
#include <linux/delay.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kmod.h>

struct h3600_backpaq_gasgauge_dev_struct {
	unsigned int  usage_count;     /* Number of people currently using this device */
};

#define MODULE_NAME      "h3600_backpaq_gasgauge"

#define GASGAUGE_MINOR        0
#define GASGAUGE_DEVICE_NAME "backpaq/gasgauge"

#define BACKPAQ_PROC_DIR  "backpaq"
#define GASGAUGE_PROC_DIR  "backpaq/gasgauge"

/* Local variables */

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry   *proc_backpaq_gasgauge_dir;
#endif
static devfs_handle_t           devfs_gasgauge;

static struct h3600_backpaq_gasgauge_dev_struct  h3600_backpaq_gasgauge_data;
static int    h3600_backpaq_gasgauge_major_num = 0;

/**************************************************************/

static int h3600_backpaq_gasgauge_ioctl(struct inode *inode, struct file * filp,
                    unsigned int cmd, unsigned long arg)
{
	return -ENOIOCTLCMD;
}

ssize_t h3600_backpaq_gasgauge_read( struct file *filp, char *buf, 
				  size_t count, loff_t *f_pos )
{
        return 0;
}

/*
 * Write function.
 *
 * In the future this may be used to set the "null" position of the gasgaugeerometer
 */

ssize_t h3600_backpaq_gasgauge_write( struct file *filp, const char *buf, 
				   size_t count, loff_t *f_pos )
{
	return 0;
}
  
int h3600_backpaq_gasgauge_open( struct inode *inode, struct file *filp )
{
	h3600_backpaq_gasgauge_data.usage_count++;

	MOD_INC_USE_COUNT;
	return 0;    /* Success */
}

int h3600_backpaq_gasgauge_release( struct inode *inode, struct file *filp )
{
	int result = 0;

	h3600_backpaq_gasgauge_data.usage_count--;

	MOD_DEC_USE_COUNT;
	return result;
}


struct file_operations h3600_backpaq_gasgauge_fops = {
	read:    h3600_backpaq_gasgauge_read,
	write:   h3600_backpaq_gasgauge_write,
	open:    h3600_backpaq_gasgauge_open,
	release: h3600_backpaq_gasgauge_release,
	ioctl:   h3600_backpaq_gasgauge_ioctl,
};



/***********************************************************************************/

struct gasgauge_proc_entry {
   const char *name;
   char offsetl;
   char offseth;
   short flags;
   struct proc_dir_entry *entry;
};
enum {
   GPE_READ=1,
   GPE_WRITE=2,
   GPE_REGPAIR=4,
   GPE_RSTATE=8,
   GPE_WSTATE=16,
   GPE_CMD=32,
   GPE_RESULT=64,
}; 
static struct gasgauge_proc_entry gasgauge_proc_entry[] = {
   { "FLGS1", 0x01, 0, GPE_READ },
   { "TMP",   0x02, 0, GPE_READ },
   { "NAC",   0x17, 0x03, GPE_READ|GPE_WRITE|GPE_REGPAIR },
   { "BATID", 0x04, 0, GPE_READ|GPE_WRITE },
   { "LMD",   0x05, 0x05, GPE_READ|GPE_WRITE },
   { "FLGS2", 0x06, 0, GPE_READ },
   { "PPD",   0x07, 0, GPE_READ },
   { "PPU",   0x08, 0, GPE_READ },
   { "CPI",   0x09, 0, GPE_READ|GPE_WRITE },
   { "VSB",   0x0b, 0, GPE_READ },
   { "VTS",   0x0c, 0, GPE_READ|GPE_WRITE },
   { "CACT",  0x0d, 0, GPE_READ|GPE_WRITE },
   { "CACD",  0x0e, 0, GPE_READ|GPE_WRITE },
   { "SAE",   0x10, 0x0f, GPE_READ|GPE_REGPAIR },
   { "RCAC",  0x11, 0, GPE_READ },
   { "VSR",   0x13, 0x12, GPE_READ|GPE_REGPAIR },
   { "NMCV",  0x15, 0, GPE_READ|GPE_WRITE },
   { "DCR",   0x18, 0, GPE_READ|GPE_WRITE },
   { "PPFC",  0x1e, 0, GPE_READ|GPE_WRITE },
   { "INTSS", 0x38, 0, GPE_READ },
   { "RST",   0x39, 0, GPE_READ|GPE_WRITE },
   { "HEXFF", 0x3f, 0, GPE_READ|GPE_WRITE },
   { "rstate", 0, 0, GPE_READ|GPE_RSTATE },
   { "wstate", 0, 0, GPE_READ|GPE_WSTATE },
   { "result",  0, 0, GPE_READ|GPE_RESULT },
   { "cmd",  0, 0, GPE_WRITE|GPE_CMD },
   { NULL, 0, 0 }
}; 

/***************************************************************
 *  /proc/backpaq/gasgauge/RRRR
 ***************************************************************/

#define PRINT_GENERAL_REG(x,s) \
	p += sprintf (p, "%lx %-17s : 0x%04x\n", (unsigned long) &x, s, x)
#define PRINT_FPGA_REG(x,s)   PRINT_GENERAL_REG(Backpaq->x,s)

static short read_gasgauge_register(int regoffset)
{
        int timeout = 10000;
        int i;
        BackpaqFPGAGasGaugeCommand = BACKPAQ_GASGAUGE_READ | regoffset;
        if (0) printk(__FUNCTION__ ": sending command %#x\n", BACKPAQ_GASGAUGE_READ | regoffset);
        i = 0;
        while ((BackpaqFPGAGasGaugeWState == BACKPAQ_GASGAUGE_WSTATE_IDLE) && (i++ < timeout)) {
                /* wait for transition out of write_idle state */
        }
        i = 0;
        while ((BackpaqFPGAGasGaugeRState == BACKPAQ_GASGAUGE_RSTATE_IDLE) && (i++ < timeout)) {
                /* wait for transition out of read_idle state */
        }
        i = 0;
        while ((BackpaqFPGAGasGaugeRState != BACKPAQ_GASGAUGE_RSTATE_IDLE) && (i++ < timeout)) {
                /* wait for transition back to read_idle state */
        }
        return BackpaqFPGAGasGaugeResult;
}

static void write_gasgauge_register(int regoffset, unsigned short value)
{
        int i;
        int timeout = 10000;
        BackpaqFPGAGasGaugeCommand = BACKPAQ_GASGAUGE_WRITE | regoffset;
        if (1) printk(__FUNCTION__ ": sending command %#x\n", BACKPAQ_GASGAUGE_WRITE | regoffset);

        i = 0;
        while ((BackpaqFPGAGasGaugeWState == BACKPAQ_GASGAUGE_WSTATE_IDLE) && (i++ < timeout)) {
                /* wait for transition out of write_idle state */
        }
        i = 0;
        while ((BackpaqFPGAGasGaugeWState != BACKPAQ_GASGAUGE_WSTATE_IDLE) && (i++ < timeout)) {
                /* wait for transition out of read_idle state */
        }

        BackpaqFPGAGasGaugeCommand = value & 0xff;
        if (1) printk(__FUNCTION__ ": sending command %#x\n", value & 0xff);

        i = 0;
        while ((BackpaqFPGAGasGaugeWState == BACKPAQ_GASGAUGE_WSTATE_IDLE) && (i++ < timeout)) {
                /* wait for transition out of write_idle state */
        }
        i = 0;
        while ((BackpaqFPGAGasGaugeWState != BACKPAQ_GASGAUGE_WSTATE_IDLE) && (i++ < timeout)) {
                /* wait for transition out of read_idle state */
        }

}

static int proc_backpaq_gasgauge_read(char *page, char **start, off_t off,
                                      int count, int *eof, void *data)
{
        struct gasgauge_proc_entry *gpe = (struct gasgauge_proc_entry *)data;
	char *p = page;
	int len;

        /* not readable */
        if (!(gpe->flags&GPE_READ))
                return -EINVAL;

        switch (gpe->flags&(GPE_RSTATE|GPE_WSTATE|GPE_CMD|GPE_RESULT)) {
        case GPE_RSTATE:
                p += sprintf(p, "rstate=%#x\n", BackpaqFPGAGasGaugeRState);
                break;
        case GPE_WSTATE:
                p += sprintf(p, "wstate=%#x\n", BackpaqFPGAGasGaugeWState);
                break;
        case GPE_RESULT:
                p += sprintf(p, "%#x\n", BackpaqFPGAGasGaugeResult);
                break;
        case GPE_CMD:
                p += sprintf(p, "%#x\n", BackpaqFPGAGasGaugeCommand);
                break;
        default: { /* regular registers */
                unsigned short x = read_gasgauge_register(gpe->offsetl);
                if (gpe->flags & GPE_REGPAIR) {
                        x |= (read_gasgauge_register(gpe->offseth) << 8); 
                } 
                p += sprintf(p, "%#x\n", x);
	}
        }

	len = (p - page) - off;
	if (len < 0)
		len = 0;

#if 0
	*eof = (len <= count) ? 1 : 0;
#endif
	*start = page + off;

	return len;
}

static int proc_backpaq_gasgauge_write(struct file *file, const char *buffer,
                                       unsigned long count, void *data)
{
        struct gasgauge_proc_entry *gpe = (struct gasgauge_proc_entry *)data;
        int x = 0;
        char *endp = NULL;
        /* not writable */
        if (!(gpe->flags&GPE_WRITE))
                return -EINVAL;

        x = simple_strtoul(buffer, &endp, 0);
        printk(__FUNCTION__ ": x=%#x\n", x);
        switch (gpe->flags&(GPE_RSTATE|GPE_WSTATE|GPE_CMD|GPE_RESULT)) {
        case GPE_RSTATE:
                return -EINVAL;
        case GPE_WSTATE:
                return -EINVAL;
        case GPE_RESULT:
                return -EINVAL;
        case GPE_CMD:
                BackpaqFPGAGasGaugeCommand = x;
                break;
        default: /* regular registers */
                write_gasgauge_register(gpe->offsetl, x&0xFF);
                if (gpe->flags & GPE_REGPAIR) {
                        write_gasgauge_register(gpe->offseth, (x>>8)&0xFF);
                } 
	}
        return count;
}


/***********************************************************************************/

#ifdef MODULE
#define FPGA_MODULE "h3600_backpaq_fpga"

int __init h3600_backpaq_gasgauge_init_module(void)    
{    
	int result;    

	/* This module only makes sense if h3600_backpaq_fpga is loaded */
	result = request_module(FPGA_MODULE);
	if ( result ) {
		printk(KERN_ALERT __FILE__ ": unable to load " FPGA_MODULE "\n");
		return result;
	}

	/* Register my device driver */
	printk(KERN_ALERT __FILE__ ": registering char device");    
	result = devfs_register_chrdev(0,MODULE_NAME, &h3600_backpaq_gasgauge_fops);    
	if ( result <= 0 ) {
		printk(" can't get major number\n");
		return result;    
	}    

	if ( h3600_backpaq_gasgauge_major_num == 0 )
		h3600_backpaq_gasgauge_major_num = result;
	printk(" %d\n", h3600_backpaq_gasgauge_major_num);

#ifdef CONFIG_DEVFS_FS
	devfs_gasgauge = devfs_register( NULL, GASGAUGE_DEVICE_NAME, DEVFS_FL_DEFAULT,
                                         h3600_backpaq_gasgauge_major_num, GASGAUGE_MINOR,
                                         S_IFCHR | S_IRUSR | S_IWUSR, 
                                         &h3600_backpaq_gasgauge_fops, NULL );
	if ( !devfs_gasgauge ) {
		printk(KERN_ALERT __FILE__ ": unable to register %s\n", GASGAUGE_DEVICE_NAME );
		devfs_unregister_chrdev( h3600_backpaq_gasgauge_major_num, MODULE_NAME );
		return -ENODEV;
	}
#endif
	/* Clear the default structure */
 	memset(&h3600_backpaq_gasgauge_data, 0, sizeof(struct h3600_backpaq_gasgauge_dev_struct));

#ifdef CONFIG_PROC_FS
        {
                struct gasgauge_proc_entry *gpe = &gasgauge_proc_entry[0];
                proc_backpaq_gasgauge_dir = proc_mkdir(GASGAUGE_PROC_DIR, 0);
                while (gpe->flags != 0) {
                        gpe->entry = create_proc_entry(gpe->name, 0, proc_backpaq_gasgauge_dir);
                        if ( gpe->entry ) {
                                gpe->entry->data = (void*)gpe;
                                if (gpe->flags & GPE_READ)
                                        gpe->entry->read_proc = proc_backpaq_gasgauge_read;
                                if (gpe->flags & GPE_WRITE)
                                        gpe->entry->write_proc = proc_backpaq_gasgauge_write;
                        }
                        gpe++;
                }
        }
#endif

	return 0;    
} 

void __exit h3600_backpaq_gasgauge_exit_module(void)
{
	printk(KERN_ALERT __FILE__ ": exit\n");

#ifdef CONFIG_PROC_FS
        {
           struct gasgauge_proc_entry *gpe = &gasgauge_proc_entry[0];
           while (gpe->flags != 0) {
              remove_proc_entry(gpe->name, gpe->entry->parent); 
              gpe++;
           }
           remove_proc_entry(proc_backpaq_gasgauge_dir->name, proc_backpaq_gasgauge_dir->parent);
        }
#endif
	devfs_unregister( devfs_gasgauge );
	devfs_unregister_chrdev( h3600_backpaq_gasgauge_major_num, MODULE_NAME );
}

MODULE_AUTHOR("Jamey Hicks <jamey.hicks@hp.com>");
MODULE_DESCRIPTION("Driver for Compaq Mercury BackPAQ gasgauge");
MODULE_LICENSE("Dual BSD/GPL");

module_init(h3600_backpaq_gasgauge_init_module);
module_exit(h3600_backpaq_gasgauge_exit_module);

#endif /* MODULE */
