/*
 * drivers/pcmcia/sa1100_backpaq.c
 *
 * PCMCIA implementation routines for Compaq CRL Mercury BackPAQ
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/pm.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/proc_fs.h> 
#include <linux/ctype.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch-sa1100/h3600-sleeve.h>
#include <asm/arch-sa1100/backpaq.h>
#include <asm/arch-sa1100/h3600_hal.h>
#include <asm/io.h>
#include "sa1100_generic.h"
#include "h3600_pcmcia.h"

static int timing_increment_ns = 0;
static unsigned int verbose = 0;
static unsigned int active_pcmcia = 1;
static unsigned int activated_pcmcia = 0;
static unsigned int active_flash = 0;
static unsigned int activated_flash = 0;

#define BACKPAQ_EEPROM_PROC_NAME "backpaq/eeprom"
#define BACKPAQ_ACTIVATE_PCMCIA_PROC_NAME "backpaq/activate_pcmcia"
#define BACKPAQ_ACTIVATE_FLASH_PROC_NAME "backpaq/activate_flash"
#define BACKPAQ_PROC_DIR	 "backpaq"

static struct h3600_backpaq_eeprom dummy_eeprom_values = {
	major_revision : 0,
	minor_revision : 0,
	fpga_version   : BACKPAQ_EEPROM_FPGA_VERTEX_100,
	camera	       : 0,
	accel_offset_x : 0x8000,
	accel_scale_x  : 0x8000,
	accel_offset_y : 0x8000,
	accel_scale_y  : 0x8000,
	serial_number  : 0,
	flash_start    : 0x00000000,
	flash_length   : 0x02000000,
	sysctl_start   : 0x02000000,
	cam_accel_offset_x : 0x8000,
	cam_accel_scale_x  : 0x8000,
	cam_accel_offset_y : 0x8000,
	cam_accel_scale_y  : 0x8000,
	cam_accel_offset_z : 0x8000,
	cam_accel_scale_z  : 0x8000,
};

static struct proc_dir_entry *proc_backpaq_eeprom = NULL;
static struct proc_dir_entry *proc_backpaq_activate_pcmcia = NULL;
static struct proc_dir_entry *proc_backpaq_activate_flash = NULL;
struct h3600_backpaq_eeprom   h3600_backpaq_eeprom_shadow;
static int		      h3600_backpaq_eeprom_offset = 0;	     // 0 indicates invalid offset

EXPORT_SYMBOL(h3600_backpaq_eeprom_shadow);

/************************************************************************************/

static u32 versionid[2];
static u32 sktstatus[2];

static struct irq_info {
	int irq;
	const char *name;
	int gpio;
	int edges;
	int request_irq;
} irq_info[] = {
#ifdef CONFIG_MACH_H3900 
	{ IRQ_GPIO_H3900_PCMCIA_CD0, "PCMCIA_CD0", GPIO_NR_H3900_PCMCIA_CD0_N, IRQT_BOTHEDGE, 1 },
	{ IRQ_GPIO_H3900_PCMCIA_CD1, "PCMCIA_CD1", GPIO_NR_H3900_PCMCIA_CD1_N, IRQT_BOTHEDGE, 1 },
	{ IRQ_GPIO_H3900_PCMCIA_IRQ0, "PCMCIA_IRQ0", GPIO_NR_H3900_PCMCIA_IRQ0_N, IRQT_FALLING, 0 },
	{ IRQ_GPIO_H3900_PCMCIA_IRQ1, "PCMCIA_IRQ1", GPIO_NR_H3900_PCMCIA_IRQ1_N, IRQT_FALLING, 0 }
#elif defined(CONFIG_SA1100_H3100) || defined(CONFIG_SA1100_H3600) || defined(CONFIG_SA1100_H3800)
	{ IRQ_GPIO_H3600_PCMCIA_CD0, "PCMCIA_CD0", GPIO_H3600_PCMCIA_CD0, IRQT_BOTHEDGE, 1 },
	{ IRQ_GPIO_H3600_PCMCIA_CD1, "PCMCIA_CD1", GPIO_H3600_PCMCIA_CD1, IRQT_BOTHEDGE, 1 },
	{ IRQ_GPIO_H3600_PCMCIA_IRQ0, "PCMCIA_IRQ0", GPIO_H3600_PCMCIA_IRQ0, IRQT_FALLING, 0 },
	{ IRQ_GPIO_H3600_PCMCIA_IRQ1, "PCMCIA_IRQ1", GPIO_H3600_PCMCIA_IRQ1, IRQT_FALLING, 0 }
#else
#error no architectural support for h3600_backpaq irq_info
#endif
};

/************************************************************************************/
/* Register devices on the backpaq which need to know about power/insertion events  */

static LIST_HEAD(h3600_backpaq_devices);
static DECLARE_MUTEX(h3600_backpaq_devs_lock);

struct h3600_backpaq_device* h3600_backpaq_register_device( h3600_backpaq_dev_t type,
							    unsigned long id,
							    h3600_backpaq_callback callback )
							  
{
	struct h3600_backpaq_device *dev = kmalloc(sizeof(struct h3600_backpaq_device), GFP_KERNEL);

//	printk(__FUNCTION__ ": backpaq register device %d %ld %p\n", type, id, callback);

	if (dev) {
		memset(dev, 0, sizeof(*dev));
		dev->type = type;
		dev->id = id;
		dev->callback = callback;

		down(&h3600_backpaq_devs_lock);
		if ( type == H3600_BACKPAQ_FPGA_DEV ) {
			list_add(&dev->entry, &h3600_backpaq_devices);
		} else {
			list_add_tail(&dev->entry, &h3600_backpaq_devices);
		}
		up(&h3600_backpaq_devs_lock);
	}
	return dev;
}

void h3600_backpaq_unregister_device(struct h3600_backpaq_device *device)
{
//	printk(__FUNCTION__ ": backpaq unregister device %d %ld %p\n", 
//	       device->type, device->id, device->callback);

	if (device) {
		down(&h3600_backpaq_devs_lock);
		list_del(&device->entry);
		up(&h3600_backpaq_devs_lock);

		kfree(device);
	}
}

int h3600_backpaq_present( void )
{
	return h3600_current_sleeve() == H3600_SLEEVE_ID( COMPAQ_VENDOR_ID, MERCURY_BACKPAQ );
}


EXPORT_SYMBOL(h3600_backpaq_present);
EXPORT_SYMBOL(h3600_backpaq_register_device);
EXPORT_SYMBOL(h3600_backpaq_unregister_device);

int h3600_backpaq_send(h3600_backpaq_request_t request )
{
	struct list_head *entry;
	struct h3600_backpaq_device *device;
	int status; /* We're not doing anything with this */

//	printk(__FUNCTION__ ": sending to everyone\n");
	down(&h3600_backpaq_devs_lock);
	entry = h3600_backpaq_devices.next;
	while ( entry != &h3600_backpaq_devices ) {
		device = list_entry(entry, struct h3600_backpaq_device, entry );
		if ( device->callback ) {
//			printk(__FUNCTION__ ": sending to %p\n", device->callback);
			status = (*device->callback)(device, request);
		}
		entry = entry->next;
	}
	up(&h3600_backpaq_devs_lock);
	return 0;
}

/***********************************************************************************/
/*    Power management callbacks  */

static int suspended = 0;

static int backpaq_suspend_sleeve( struct sleeve_dev *sleeve_dev )
{
	h3600_backpaq_send( H3600_BACKPAQ_SUSPEND );
	suspended = 1;
	clr_h3600_egpio(IPAQ_EGPIO_OPT_ON);
	return 0;
}

static int backpaq_resume_sleeve( struct sleeve_dev *sleeve_dev )
{
	if ( suspended ) {
		set_h3600_egpio(IPAQ_EGPIO_OPT_ON);
		h3600_backpaq_send( H3600_BACKPAQ_RESUME );
		suspended = 0;
	}
	return 0;
}


/***********************************************************************************/
/*			   Proc file system					   */
/***********************************************************************************/

static char * parse_eeprom( char *p )
{
	struct h3600_backpaq_eeprom *t = &h3600_backpaq_eeprom_shadow;

	p += sprintf(p, "EEPROM status	 : %s\n", 
		     (h3600_backpaq_eeprom_offset ? "Okay" : "Not programmed" ));
	p += sprintf(p, "Major revision	 : 0x%02x\n", t->major_revision );
	p += sprintf(p, "Minor revision	 : 0x%02x\n", t->minor_revision );
	p += sprintf(p, "FPGA version	 : 0x%02x\n", t->fpga_version );
	p += sprintf(p, "Camera		 : 0x%02x\n", t->camera );
	p += sprintf(p, "Accel offset x	 : 0x%04x\n", t->accel_offset_x );
	p += sprintf(p, "Accel scale x	 : 0x%04x\n", t->accel_scale_x );
	p += sprintf(p, "Accel offset y	 : 0x%04x\n", t->accel_offset_y );
	p += sprintf(p, "Accel scale y	 : 0x%04x\n", t->accel_scale_y );

	p += sprintf(p, "Cam Accel offset x      : 0x%04x\n", t->cam_accel_offset_x );
	p += sprintf(p, "Cam Accel scale x	 : 0x%04x\n", t->cam_accel_scale_x );
	p += sprintf(p, "Cam Accel offset y	 : 0x%04x\n", t->cam_accel_offset_y );
	p += sprintf(p, "Cam Accel scale y	 : 0x%04x\n", t->cam_accel_scale_y );
	p += sprintf(p, "Cam Accel offset z	 : 0x%04x\n", t->cam_accel_offset_z );
	p += sprintf(p, "Cam Accel scale z	 : 0x%04x\n", t->cam_accel_scale_z );

	p += sprintf(p, "Serial number	 : %d\n",     t->serial_number );
	p += sprintf(p, "Flash start	 : 0x%08x\n", t->flash_start );
	p += sprintf(p, "Flash length	 : 0x%08x\n", t->flash_length );
	p += sprintf(p, "Sysctl start	 : 0x%08x\n", t->sysctl_start );

	return p;
}

static int proc_backpaq_eeprom_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	char *p = page;
	int len;

	if (!h3600_backpaq_present()) {
		p += sprintf(p,"No backpaq present\n");
	}
	else {
		p = parse_eeprom( p );
	}

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int backpaq_find_oem_offset( void )
{
	short vendor, device;
	char d;
	short offset;

	if (!h3600_backpaq_present())
		return 0;

	h3600_spi_read( 6, (char *)&vendor, 2 );
	h3600_spi_read( 8, (char *)&device, 2 );

	if ( vendor != 0x1125 && device != 0x100 )
		return 0;

	offset = 10;
	do {
		h3600_spi_read( offset++, &d, 1 );
	} while ( d != 0 );
	offset += 11;
	h3600_spi_read( offset, (char *)&offset, 2 );
	offset += 4;
	// Now offset points to the oem table
	return offset;
}


static void load_eeprom( void )
{
	int offset, len;
	unsigned char *p;

	h3600_backpaq_eeprom_offset = backpaq_find_oem_offset();
	if ( h3600_backpaq_eeprom_offset ) {
		offset = h3600_backpaq_eeprom_offset;
		len = sizeof(struct h3600_backpaq_eeprom);
		p = (unsigned char *) &h3600_backpaq_eeprom_shadow;

		while ( len > 0 ) {
			int chunk = ( len > 4 ? 4 : len );
			h3600_spi_read( offset, p, chunk );
			p += chunk;
			offset += chunk;
			len -= chunk;
		}
	} else {   /* No EEPROM present : load dummy values */
		memcpy(&h3600_backpaq_eeprom_shadow, 
		       &dummy_eeprom_values,
		       sizeof(h3600_backpaq_eeprom_shadow));
	}
}


int proc_eeprom_update(ctl_table *table, int write, struct file *filp,
		       void *buffer, size_t *lenp)
{
	unsigned long val;
	size_t left, len;
#define TMPBUFLEN 20
	char buf[TMPBUFLEN];
	
	if (!table->data || !table->maxlen || !*lenp ||
	    (filp->f_pos && !write)) {
		*lenp = 0;
		return 0;
	}
	
	left = *lenp;	// How many character we have left
	
	if (write) {
		/* Skip over leading whitespace */
		while (left) {
			char c;
			if(get_user(c,(char *) buffer))
				return -EFAULT;
			if (!isspace(c))
				break;
			left--;
			((char *) buffer)++;
		}
		if ( left ) {
			len = left;
			if (len > TMPBUFLEN-1)
				len = TMPBUFLEN-1;
			if(copy_from_user(buf, buffer, len))
				return -EFAULT;
			buf[len] = 0;
			val = simple_strtoul(buf, NULL, 0);
			if (0) printk("%s: writing value 0x%lx\n", __FUNCTION__, val );

			switch (table->maxlen) {
			case 1:	*(( u8 *) (table->data)) = val;	 break;
			case 2: *((u16 *) (table->data)) = val; break;
			case 4: *((u32 *) (table->data)) = val; break;
			default:
				printk("%s: illegal table entry size\n", __FUNCTION__);
				break;
			}
			
			if ( h3600_backpaq_eeprom_offset ) {
				int offset = (int) (table->data - (void *) &h3600_backpaq_eeprom_shadow);
				if (0) printk("%s: writing offset %d\n", __FUNCTION__, offset );
				h3600_spi_write( h3600_backpaq_eeprom_offset + offset, 
						 table->data, table->maxlen);
			}
			else {
				printk("%s: unable to write to Backpaq - EEPROM not initialized\n", __FUNCTION__);
			}
		}
		filp->f_pos += *lenp;
	} else { /* Reading */
		if ( !h3600_backpaq_eeprom_offset ) {
			printk("%s: unable to read Backpaq - EEPROM not initialized\n", __FUNCTION__);
			return 0;
		}
		switch (table->maxlen) {
		case 1: val = *((u8 *)(table->data)); break;
		case 2: val = *((u16 *)(table->data)); break;
		case 4: val = *((u32 *)(table->data)); break;
		default:
			val = 0;
			printk("%s: illegal table entry size\n", __FUNCTION__);
			break;
		}

		sprintf(buf, "0x%lx\n", val);
		len = strlen(buf);
		if (len > left)
			len = left;
		if(copy_to_user(buffer, buf, len))
			return -EFAULT;

		*lenp = len;
		filp->f_pos += len;
	}

	return 0;
}


/***********************************************************************************/


#define EESHADOW(x) &(h3600_backpaq_eeprom_shadow.x), sizeof(h3600_backpaq_eeprom_shadow.x)

static struct ctl_table backpaq_accel_table[] = 
{
	{10, "xoffset", EESHADOW(accel_offset_x), 0644, NULL, &proc_eeprom_update },
	{11, "xscale",	EESHADOW(accel_scale_x),  0644, NULL, &proc_eeprom_update },
	{12, "yoffset", EESHADOW(accel_offset_y), 0644, NULL, &proc_eeprom_update },
	{13, "yscale",	EESHADOW(accel_scale_y),  0644, NULL, &proc_eeprom_update },
	{0}
};

static struct ctl_table backpaq_cam_accel_table[] = 
{
	{10, "xoffset", EESHADOW(cam_accel_offset_x), 0644, NULL, &proc_eeprom_update },
	{11, "xscale",	EESHADOW(cam_accel_scale_x),  0644, NULL, &proc_eeprom_update },
	{12, "yoffset", EESHADOW(cam_accel_offset_y), 0644, NULL, &proc_eeprom_update },
	{13, "yscale",	EESHADOW(cam_accel_scale_y),  0644, NULL, &proc_eeprom_update },
	{12, "zoffset", EESHADOW(cam_accel_offset_z), 0644, NULL, &proc_eeprom_update },
	{13, "zscale",	EESHADOW(cam_accel_scale_z),  0644, NULL, &proc_eeprom_update },
	{0}
};

static struct ctl_table backpaq_eeprom_table[] = 
{
	{1, "major",  EESHADOW(major_revision), 0644, NULL, &proc_eeprom_update },
	{2, "minor",  EESHADOW(minor_revision), 0644, NULL, &proc_eeprom_update },
	{3, "serial", EESHADOW(serial_number),	0644, NULL, &proc_eeprom_update },
	{4, "fpga",   EESHADOW(fpga_version),	0644, NULL, &proc_eeprom_update },
	{5, "camera", EESHADOW(camera),		0644, NULL, &proc_eeprom_update },
	{6, "accel",  NULL, 0, 0555, backpaq_accel_table},
	{7, "cam_accel",  NULL, 0, 0555, backpaq_cam_accel_table},
	{8, "flashstart",  EESHADOW(flash_start), 0644, NULL, &proc_eeprom_update },
	{9, "flashlen",	   EESHADOW(flash_length), 0644, NULL, &proc_eeprom_update },
	{10, "sysctlstart", EESHADOW(sysctl_start), 0644, NULL, &proc_eeprom_update },
	{0}
};

static struct ctl_table backpaq_socket0_table[] = 
{
	{1, "versionid",  &versionid[0], sizeof(int), 0644, NULL, &proc_dointvec },
	{2, "status",	  &sktstatus[0], sizeof(int), 0644, NULL, &proc_dointvec },
	{0}
};

static struct ctl_table backpaq_socket1_table[] = 
{
	{1, "versionid",  &versionid[1], sizeof(int), 0644, NULL, &proc_dointvec },
	{2, "status",	  &sktstatus[1], sizeof(int), 0644, NULL, &proc_dointvec },
	{0}
};

static struct ctl_table backpaq_pcmcia_table[] = 
{
	{1, "socket1", NULL, 0, 0555, backpaq_socket0_table },
	{2, "socket2", NULL, 0, 0555, backpaq_socket1_table },
	{3, "verbose",		   &verbose,		 sizeof(int), 0644, NULL, &proc_dointvec },
	{4, "timing_increment_ns", &timing_increment_ns, sizeof(int), 0644, NULL, &proc_dointvec },
	{5, "active_pcmcia",	   &active_pcmcia,	 sizeof(int), 0644, NULL, &proc_dointvec },
	{6, "active_flash",	   &active_flash,	 sizeof(int), 0644, NULL, &proc_dointvec },
	{0}
};

static struct ctl_table backpaq_table[] = 
{
	{5, "eeprom", NULL, 0, 0555, backpaq_eeprom_table},
	{6, "pcmcia", NULL, 0, 0555, backpaq_pcmcia_table},
	{0}
};

static struct ctl_table backpaq_dir_table[] = 
{
	{22, "backpaq", NULL, 0, 0555, backpaq_table},
	{0}
};
static struct ctl_table_header *backpaq_ctl_table_header = NULL;


/***********************************************************************************/

extern struct pcmcia_low_level h3600_backpaq_pcmcia_ops;

static void h3600_backpaq_activate_pcmcia(void)
{
	h3600_pcmcia_change_sleeves(&h3600_backpaq_pcmcia_ops);
	activated_pcmcia = 0;
}

static void h3600_backpaq_deactivate_pcmcia(void)
{
	h3600_pcmcia_remove_sleeve();
	activated_pcmcia = 0;
}

static int proc_backpaq_activate_pcmcia_read(char *page, char **start, off_t off,
					     int count, int *eof, void *data)
{
	char *p = page;
	int len;

	if (!h3600_backpaq_present()) {
		p += sprintf(p,"No backpaq present\n");
	}
	else {
		p += sprintf(p, "activated=%d", activated_pcmcia);
	}

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int proc_backpaq_activate_pcmcia_write(struct file *file, const char *buffer,
					      unsigned long count, void *data)
{
	h3600_backpaq_activate_pcmcia();
	return count;
}



#define VERSIONID_SOCKETNUM 1
#define VERSIONID_RESET_CAP 2 /* this bit set if supports per-socket reset */ 

static short *backpaq_pcmcia_status[2];
static short *backpaq_pcmcia_versionid[2];
static short *backpaq2_pcmcia_reset_assert[2];
static short *backpaq2_pcmcia_reset_deassert[2];
#define USES5V_0 ((BackpaqPCMCIAStatus_0 & BACKPAQ_PCMCIA_VS1) && \
		  (BackpaqPCMCIAStatus_0 & BACKPAQ_PCMCIA_VS2) && \
		  (BackpaqPCMCIAStatus_0 & BACKPAQ_PCMCIA_CD0) == 0)

#define USES5V_1 ((BackpaqPCMCIAStatus_1 & BACKPAQ_PCMCIA_VS1) && \
		  (BackpaqPCMCIAStatus_1 & BACKPAQ_PCMCIA_VS2) && \
		  (BackpaqPCMCIAStatus_1 & BACKPAQ_PCMCIA_CD1) == 0)


static void msleep(unsigned int msec)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout( (msec * HZ + 999) / 1000);
}

static int h3600_backpaq_pcmcia_init( struct pcmcia_init *init )
{
	int sock, i;
	int irq, res;

	printk("%s:%d\n", __FUNCTION__, __LINE__);

	backpaq_pcmcia_status[0] = (void *) &BackpaqPCMCIAStatus_0;
	backpaq_pcmcia_status[1] = (void *) &BackpaqPCMCIAStatus_1;
	backpaq_pcmcia_versionid[0] = (void *) &BackpaqPCMCIAVersionid_0;
	backpaq_pcmcia_versionid[1] = (void *) &BackpaqPCMCIAVersionid_1;
	backpaq2_pcmcia_reset_assert[0] = (void *) &BackpaqPCMCIAResetAssert_0;
	backpaq2_pcmcia_reset_assert[1] = (void *) &BackpaqPCMCIAResetAssert_1;
	backpaq2_pcmcia_reset_deassert[0] = (void *) &BackpaqPCMCIAResetDeassert_0;
	backpaq2_pcmcia_reset_deassert[1] = (void *) &BackpaqPCMCIAResetDeassert_1;
	
	for (sock = 0; sock < 2; sock++) {
		versionid[sock] = *backpaq_pcmcia_versionid[sock];
		sktstatus[sock] = *backpaq_pcmcia_status[sock];
	}

	clr_h3600_egpio(IPAQ_EGPIO_OPT_RESET);
	clr_h3600_egpio(IPAQ_EGPIO_CARD_RESET);

	for (i = 0; i < ARRAY_SIZE(irq_info); i++) {
		struct irq_info *info = &irq_info[i];

		/* Set transition detect */
		set_irq_type( info->gpio, info->edges);
		
		if (info->request_irq) {
			/* Register interrupt */
			res = request_irq( info->irq, init->handler, SA_INTERRUPT, info->name, NULL );
			if( res < 0 ) { 
				printk( KERN_ERR "%s: Request for IRQ %u failed\n", __FUNCTION__, info->irq );
				return -1;
			}
		}
	}

	return 2;  /* Two PCMCIA devices */
}

static __inline__ int backpaq_get_5ven(void) 
{
	if (h3600_backpaq_eeprom_shadow.major_revision == 3)
		return BackpaqSysctlGenControl & BACKPAQ3_GENCTL_50VEN;
	else
		return BackpaqSysctlPCMCIAPower & BACKPAQ2_PWRCTL_50VEN;
}

static __inline__ void backpaq_assign_5ven(u8 value)
{
	int is_backpaq3 = (h3600_backpaq_eeprom_shadow.major_revision == 3);
	u16 curval = (is_backpaq3 ? BackpaqSysctlGenControl : BackpaqSysctlPCMCIAPower);
	u16 newbit = (is_backpaq3 ? BACKPAQ3_GENCTL_50VEN : BACKPAQ2_PWRCTL_50VEN);
	u16 newval;
	if (value)
		newval = curval | newbit;
	else
		newval = curval & ~newbit;
	if (is_backpaq3)
		BackpaqSysctlGenControl = newval;
	else
		BackpaqSysctlPCMCIAPower = newval;
}

static int h3600_backpaq_pcmcia_shutdown( void )
{
	int i;
	for (i = 0; i < ARRAY_SIZE(irq_info); i++) {
		struct irq_info *info = &irq_info[i];
		/* disable IRQs */
		if (info->request_irq)
			free_irq( info->irq, NULL );
	}
  
	return 0;
}

static int h3600_backpaq_pcmcia_socket_state(struct pcmcia_state_array *state_array)
{
	int sock, result;
	// int curval = BackpaqSysctlPCMCIAPower;

	result = h3600_common_pcmcia_socket_state( state_array );
	if ( result < 0 )
		return result;

	/*  ok, so here's the deal, we need to clean up
	 *  after a 5 volt card has been removed
	 *  unfortunately there isnt a callback for
	 *  eject. So, we configure the power on insertion
	 *  and we use this polling routine to clean up 
	 *  after an eject
	 * */

	// have 5v on and noone using it
	if (backpaq_get_5ven() && !(USES5V_0 || USES5V_1)){
	  if (verbose)
	    printk("%s: Turning off 5V power\n", __FUNCTION__);
	  backpaq_assign_5ven(0);
	}
		
	for (sock = 0; sock < 2; sock++) { 
		int status = *backpaq_pcmcia_status[sock];
		int bvd1 = ((status & BACKPAQ_PCMCIA_BVD1) ? 1 : 0);
		int bvd2 = ((status & BACKPAQ_PCMCIA_BVD2) ? 1 : 0);
		
		if (verbose) printk("%s:%d socket[%d]->status=%x\n", __FUNCTION__,
				    __LINE__, sock,
				    *backpaq_pcmcia_status[sock]);
		state_array->state[sock].bvd1 = bvd1;
		state_array->state[sock].bvd2 = bvd2;
		state_array->state[sock].wrprot = 0;
		if ((status & BACKPAQ_PCMCIA_VS1) == 0)
			state_array->state[sock].vs_3v = 1;
		if ((status & BACKPAQ_PCMCIA_VS2) == 0)
			state_array->state[sock].vs_Xv = 1;
	}	    

	return 1;
}

static int h3600_backpaq_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	unsigned long flags;
	int sock = configure->sock;
	int shift = (sock ? 4 : 0);
	int mask = 0xf << shift;
	int curval = BackpaqSysctlPCMCIAPower;
	int val = 0;

	printk(KERN_ALERT "%s\n", __FUNCTION__);

	if(sock>1) 
		return -1;

	printk("%s: socket=%d vcc=%d vpp=%d reset=%d\n", __FUNCTION__, 
	       sock, configure->vcc, configure->vpp, configure->reset);

	local_irq_save(flags);

	switch (configure->vcc) {
	case 0:
		break;
	case 50:
		val |= (BACKPAQ_PCMCIA_A5VEN << shift);
		break;
	case 33:
		val |= (BACKPAQ_PCMCIA_A3VEN << shift);
		break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
		       configure->vcc);
		local_irq_restore(flags);
		return -1;
	}

	switch (configure->vpp) {
	case 0:
		break;
	case 50:
		break;
	case 33:
		break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vpp %u\n", __FUNCTION__,
		       configure->vpp);
		restore_flags(flags);
		return -1;
	}

	if (verbose) printk("	  BackpaqSysctlPCMCIAPower=%x mask=%x val=%x vcc=%d\n", 
			    BackpaqSysctlPCMCIAPower, mask, val, configure->vcc);
	//curval = curval & ~(short) mask;
	if (val & ((BACKPAQ_PCMCIA_A5VEN << 4) | BACKPAQ_PCMCIA_A5VEN)) {
		if (!backpaq_get_5ven()) {
				/* if 5V was not enables, turn it on and wait 100ms */
			printk("%s: enabling 5V and sleeping 100ms\n", __FUNCTION__);
			BackpaqSysctlPCMCIAPower = curval;
			backpaq_assign_5ven(1);
			msleep(100);				
		}
	}

	BackpaqSysctlPCMCIAPower = BackpaqSysctlPCMCIAPower | val;

	versionid[sock] = *backpaq_pcmcia_versionid[sock];
	sktstatus[sock] = *backpaq_pcmcia_status[sock];
	if (!(*backpaq_pcmcia_versionid[sock] & VERSIONID_RESET_CAP)) {
		printk("%s: PCMCIA_SLICE needs to be updated to support per-socket reset, using old method: versionid=%#x\n", __FUNCTION__, 
		       *backpaq_pcmcia_versionid[sock]);
		assign_h3600_egpio( IPAQ_EGPIO_CARD_RESET, configure->reset );

	} else {
		if (configure->reset)
			*backpaq2_pcmcia_reset_assert[sock] = 1;
		else
			*backpaq2_pcmcia_reset_deassert[sock] = 1;
		*(short*)backpaq_pcmcia_status[sock] = (configure->reset * BACKPAQ_PCMCIA_RESET);
	}
	restore_flags(flags);
	return 0;
}

static int nsockets = 0;

int h3600_backpaq_pcmcia_socket_init(int sock)
{
	/* Enable CF bus: */
	clr_h3600_egpio(IPAQ_EGPIO_OPT_RESET);

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(10*HZ / 1000);

	switch (sock) {
	case 0:
	case 1:
		set_irq_type(irq_info[2+sock].irq, irq_info[0+sock].edges);
		set_irq_type(irq_info[2+sock].irq, irq_info[2+sock].edges);
		nsockets++;
		break;
	}

	return 0;
}

int h3600_backpaq_pcmcia_socket_suspend(int sock)
{
	switch (sock) {
	case 0:
	case 1:
		set_irq_type(irq_info[2+sock].irq, GPIO_NO_EDGES);
		set_irq_type(irq_info[2+sock].irq, GPIO_NO_EDGES);
		nsockets--;
		break;
	}

	return 0;
}

struct pcmcia_low_level h3600_backpaq_pcmcia_ops = { 
	h3600_backpaq_pcmcia_init,
	h3600_backpaq_pcmcia_shutdown,
	h3600_backpaq_pcmcia_socket_state,
	h3600_common_pcmcia_get_irq_info,
	h3600_backpaq_pcmcia_configure_socket,
	h3600_backpaq_pcmcia_socket_init,
	h3600_backpaq_pcmcia_socket_suspend,
	h3600_common_pcmcia_mecr_timing
};



/* Needs the newer MTD code, not available quite yet...*/

/*
static u_char *h3600_backpaq_point(struct map_info *map, loff_t ofs, size_t len)
{
	return map->map_priv_1 + ofs;
}
static void h3600_backpaq_unpoint(struct map_info *map, u_char *ptr)
{
}
static __u8 h3600_backpaq_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 h3600_backpaq_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 h3600_backpaq_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void h3600_backpaq_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void h3600_backpaq_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void h3600_backpaq_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void h3600_backpaq_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void h3600_backpaq_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static void h3600_backpaq_set_vpp(struct map_info *map, int vpp)
{
	u16 mask = (BACKPAQ2_FLASH_VPPEN | BACKPAQ3_FLASH_VPPEN);
	if (vpp)
		BackpaqSysctlFlashControl |= mask;
	else
		BackpaqSysctlFlashControl &= ~mask;
}

struct mtd_partition h3600_backpaq_flash_partitions[] = {
	{
		name: "BackPAQ Flash",
		offset: 0x00000000,
		size: 0	 // will expand to the end of the flash 
	}
};

static struct map_info h3600_backpaq_map = {
  name: "backpaq_flash",
  //  point:	h3600_backpaq_point,
  //  unpoint:	h3600_backpaq_unpoint,
  read8:	h3600_backpaq_read8,
  read16:	h3600_backpaq_read16,
  read32:	h3600_backpaq_read32,
  copy_from:	h3600_backpaq_copy_from,
  write8:	h3600_backpaq_write8,
  write16:	h3600_backpaq_write16,
  write32:	h3600_backpaq_write32,
  copy_to:	h3600_backpaq_copy_to,
  set_vpp:	h3600_backpaq_set_vpp,

  buswidth:	2,
  size:		0x02000000,

  map_priv_1:	0xf1000000,

};

static struct mtd_info *h3600_backpaq_mtd;
*/

static void h3600_backpaq_activate_flash(void)
{
	/*
	int i;
	BackpaqSysctlFlashControl = (BACKPAQ_FLASH_ENABLE0 | BACKPAQ_FLASH_ENABLE1);
	for (i = 0; i < 32; i += 2)
		printk("h3600_backpaq_init: flash[%x]=%x\n", i, *(u16*)(0xf1000000 + i));
	h3600_backpaq_mtd = do_map_probe("cfi_probe", &h3600_backpaq_map);
	printk("%s:%d: h3600_backpaq_mtd=%p\n", __FUNCTION__, __LINE__, h3600_backpaq_mtd);
	if (h3600_backpaq_mtd) {
		h3600_backpaq_mtd->module = THIS_MODULE;
		if ( add_mtd_partitions(h3600_backpaq_mtd, h3600_backpaq_flash_partitions, 1))
			printk(" *** " "%s: unable to add flash partitions\n", __FUNCTION__);
		printk(KERN_NOTICE "backpaq flash access initialized\n");
	}
	activated_flash = 1;
	*/
}

static void h3600_backpaq_deactivate_flash(void)
{
	/*
	if (h3600_backpaq_mtd) {
		del_mtd_partitions(h3600_backpaq_mtd);
		map_destroy(h3600_backpaq_mtd);
	}
	activated_flash = 0;
	*/
}


static int proc_backpaq_activate_flash_read(char *page, char **start, off_t off,
					     int count, int *eof, void *data)
{
	char *p = page;
	int len;

	if (!h3600_backpaq_present()) {
		p += sprintf(p,"No backpaq present\n");
	}
	else {
		p += sprintf(p, "active=%d", active_flash);
	}

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int proc_backpaq_activate_flash_write(struct file *file, const char *buffer,
					      unsigned long count, void *data)
{
	h3600_backpaq_activate_flash();
	return count;
}




static int __devinit backpaq_probe_sleeve(struct sleeve_dev *sleeve_dev, const struct sleeve_device_id *ent)
{
	u8 major_version;
	u8 minor_version;

	printk("%s: %s\n", __FUNCTION__, sleeve_dev->driver->name);

	/* Update MSC2 values for just nCS4 */
#ifdef CONFIG_ARCH_SA1100	
	/* MSC1: 0xFFFCFFFC */
	/* 
	 * 000 10010 10010 0 00
	 */
	MSC2 = (MSC2 & 0xffff0000) | 0x00001290;
#else
	/*
	 * 
	 */
	MSC1 = (MSC1 & 0xffff0000) | 0x00007ff8;
	/*
	 * very conservative for now 
	 * 0 111 1111 1111 0 000
	 */
	MSC2 = (MSC2 & 0xffff0000) | 0x00007ff0;
#endif

	set_h3600_egpio(IPAQ_EGPIO_OPT_ON); // turn on the power to the device
	/* Don't change the MSC2 values without checking if the camera works correctly */
 
	printk("%s: setting MSC1=0x%x (&MSC1=%p)\n", __FUNCTION__,MSC1, &MSC1);
	printk("%s: setting MSC2=0x%x (&MSC2=%p)\n", __FUNCTION__,MSC2, &MSC2);

	// Set up the EEPROM program area
	load_eeprom();

	if ((h3600_backpaq_eeprom_shadow.major_revision == 2) || active_pcmcia)
		h3600_backpaq_activate_pcmcia();

	h3600_backpaq_send( H3600_BACKPAQ_INSERT );

	major_version = BackpaqSysctlMajorVersion;
	minor_version = BackpaqSysctlFirmwareVersion;
	printk("%s: firmware_major=%x firmware_minor=%x \n\t\tbackpaq_major=%x  backpaq_minor=%x fpga_version=%x\n", __FUNCTION__, 
	       major_version, minor_version,
	       h3600_backpaq_eeprom_shadow.major_revision,
	       h3600_backpaq_eeprom_shadow.minor_revision,
	       h3600_backpaq_eeprom_shadow.fpga_version);

	clr_h3600_egpio(IPAQ_EGPIO_OPT_RESET);
	if (active_flash)
		h3600_backpaq_activate_flash();

#ifdef CONFIG_PROC_FS
	backpaq_ctl_table_header = register_sysctl_table(backpaq_dir_table, 0);
	proc_backpaq_eeprom = create_proc_entry(BACKPAQ_EEPROM_PROC_NAME, 0, NULL);
	if ( !proc_backpaq_eeprom ) {
		/* We probably need to create the "backpaq" directory first */
		proc_mkdir(BACKPAQ_PROC_DIR,0);
		proc_backpaq_eeprom = create_proc_entry(BACKPAQ_EEPROM_PROC_NAME, 0, NULL);
	}
	
	if ( proc_backpaq_eeprom )
		proc_backpaq_eeprom->read_proc = proc_backpaq_eeprom_read;    
	else {
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", 
		       BACKPAQ_EEPROM_PROC_NAME);
	}
	proc_backpaq_activate_pcmcia = create_proc_entry(BACKPAQ_ACTIVATE_PCMCIA_PROC_NAME, 0, NULL);
	if (proc_backpaq_activate_pcmcia) {
		proc_backpaq_activate_pcmcia->read_proc = proc_backpaq_activate_pcmcia_read;
		proc_backpaq_activate_pcmcia->write_proc = proc_backpaq_activate_pcmcia_write;
	} else {
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", 
		       BACKPAQ_ACTIVATE_PCMCIA_PROC_NAME);
	}
	proc_backpaq_activate_flash = create_proc_entry(BACKPAQ_ACTIVATE_FLASH_PROC_NAME, 0, NULL);
	if (proc_backpaq_activate_flash) {
		proc_backpaq_activate_flash->read_proc = proc_backpaq_activate_flash_read;
		proc_backpaq_activate_flash->write_proc = proc_backpaq_activate_flash_write;
	} else {
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", 
		       BACKPAQ_ACTIVATE_FLASH_PROC_NAME);
	}

#endif	
	return 0;
}

static void __devexit backpaq_remove_sleeve(struct sleeve_dev *sleeve_dev)
{
	printk("%s: %s\n", __FUNCTION__, sleeve_dev->driver->name);
#ifdef CONFIG_PROC_FS
	unregister_sysctl_table(backpaq_ctl_table_header);
	if ( proc_backpaq_eeprom ) {
		remove_proc_entry(BACKPAQ_EEPROM_PROC_NAME, 0);
		proc_backpaq_eeprom = NULL;
	}
	if ( proc_backpaq_activate_pcmcia ) {
		remove_proc_entry(BACKPAQ_ACTIVATE_PCMCIA_PROC_NAME, 0);
		proc_backpaq_activate_pcmcia = NULL;
	}
	if ( proc_backpaq_activate_flash ) {
		remove_proc_entry(BACKPAQ_ACTIVATE_FLASH_PROC_NAME, 0);
		proc_backpaq_activate_flash = NULL;
	}
#endif

	h3600_backpaq_deactivate_flash();

	h3600_backpaq_eeprom_offset = 0;       /* No backpaq */
	h3600_backpaq_send( H3600_BACKPAQ_EJECT );
	if (active_pcmcia)
		h3600_backpaq_deactivate_pcmcia();
	printk("%s: changed to no sleeve\n", __FUNCTION__);

	/* turn off the power to the sleeve */
	clr_h3600_egpio(IPAQ_EGPIO_OPT_ON);
}

static struct sleeve_device_id backpaq_tbl[] __devinitdata = {
	{ COMPAQ_VENDOR_ID, MERCURY_BACKPAQ },
	{ 0, }
};

static struct sleeve_driver backpaq_driver = {
	name:	  "Compaq Mercury Backpaq",
	id_table: backpaq_tbl,
	features: (SLEEVE_HAS_PCMCIA(2) | SLEEVE_HAS_FIXED_BATTERY | SLEEVE_HAS_ACCELEROMETER
		   | SLEEVE_HAS_FLASH | SLEEVE_HAS_CAMERA | SLEEVE_HAS_AUDIO),
	probe:	  backpaq_probe_sleeve,
	remove:	  backpaq_remove_sleeve,
	suspend:  backpaq_suspend_sleeve,
	resume:	  backpaq_resume_sleeve
};


int __init backpaq_init_module(void)
{
	printk("%s:\n", __FUNCTION__);
	h3600_sleeve_register_driver(&backpaq_driver);
	return 0;
}

void __exit backpaq_cleanup_module(void)
{
	printk("%s:\n", __FUNCTION__);
	h3600_sleeve_unregister_driver(&backpaq_driver);
}

MODULE_AUTHOR("Jamey Hicks <jamey.hicks@hp.com>");
MODULE_DESCRIPTION("Linux PCMCIA Card Services: Compaq Mercury BackPAQ Socket Controller ");
MODULE_LICENSE("Dual BSD/GPL");

module_init(backpaq_init_module);
module_exit(backpaq_cleanup_module);

