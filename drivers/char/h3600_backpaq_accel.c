/*
*
* Driver for the HP iPAQ Mercury Backpaq accelerometer
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
* Author: Andrew Christian
*         <andyc@handhelds.org>
*
* TODO:
*       1. Add a calibration ioctl
*       2. Add calibration constants ( x' = x * a1 + a2, etc...)
*       3. Store calibration data in EEPROM
*       4. Add thermal corrections based on reading gas gauge 
*
* Interface:
* 
*       The accelerometer is controlled by the BackPAQ FPGA.  The BackPAQ
*       maintains four counters (at 32 MHz each) that count the widths of
*       high and low values of the 1 kHZ square waves coming out of the X and
*       Y axes of the accelerometer.
* 
*       Acceleration in gravities is given by the formula:
*
*          Fx = [ Xt1 / ( Xt1 + Xt2) - 0.5 ] / 0.125
*
*       where Xt1, Xt2 are the widths (number of counts recorded) for the high and low
*       cycles of the 1 kHz square wave.
*
*       To avoid floating point math, we return the calculated force as a signed
*       16-bit fractional value, with 8 bits of fraction.  The measured accelerations
*       in the X- and Y-directions are combined into a single 4 byte structure:
*         
*              struct {
*                 short x_acceleration,
*                 short y_acceleration
*              };
*
*       For example, a 1.5 g acceleration in the x direction (and 0 in the Y) would
*       be returned as "0x01800000"
*
*       Using calibration constants of "scale" and "offset" (16 bit values centered around 0x8000),
*       the formula becomes:
* 
*           Fx = (scale * ((Xt1 * ( 2^16 - offset ) - offset * Xt2) / ( Xt1 + Xt2 ) )) >> 20
* 
*       This gives 12 bits of accuracy.
*
*       Acceleration directions are as follows:
*
*             < 0 >          <=== Camera
*     +Y <-  /-----\
*            |     |         <=== iPAQ
*            |     |
*            |     |
*            |     |
*            \-----/
* 
*             | +X
*             V
*       On the backpaq revision 3 the Acceleration directions are as follows. this is because the accelerometer chip is in a
*	    different orientation in this build:
*
*             < 0 >          <=== Camera
*            /-----\ -> +Y 
*            |     |         <=== iPAQ
*            |     |
*            |     |
*            |     |
*            \-----/
* 
*             | +X
*             V
*
*
* 
*
*
* *
*       Note that the acclerometer is physically located in the upper-left corner of the
*       main board of the BackPAQ2.
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <asm/uaccess.h>         /* get_user,copy_to_user*/

#include <linux/proc_fs.h> 
#include <asm/arch-sa1100/h3600_backpaq_accel.h>
#include <asm/arch-sa1100/backpaq.h>
#include <linux/delay.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kmod.h>

struct h3600_backpaq_accel_dev_struct {
	int minor;
	int xyz;
	char *name;
	volatile short *x1;
	volatile short *x2;
	volatile short *y1;
	volatile short *y2;
	volatile short *z1;
	volatile short *z2;
	u16 scale_x;
	u16 offset_x;
	u16 scale_y;
	u16 offset_y;
	u16 scale_z;
	u16 offset_z;
	u32   usage_count;     /* Number of people currently using this device */
	devfs_handle_t devfs;
};

#define MODULE_NAME      "h3600_backpaq_accel"

#define ACCEL_MINOR        0
#define ACCEL_DEVICE_NAME "backpaq/accel"

#define ACCEL_PROC_DIR  "backpaq"
#define ACCEL_PROC_NAME "backpaq/accel"

/* Useful externals */

int h3600_backpaq_fpga_status(void);

/* Local variables */

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry   *proc_backpaq_accel;
#endif

static struct h3600_backpaq_accel_dev_struct accel_data[4] = {
	{ 0, 0, "backpaq/accel", 
	  &BackpaqFPGAAccelX1, &BackpaqFPGAAccelX2, &BackpaqFPGAAccelY1, &BackpaqFPGAAccelY2, NULL, NULL },
	{ 1, 0, "backpaq/cam_accel", 
	  &BackpaqFPGACamAccelX1, &BackpaqFPGACamAccelX2, &BackpaqFPGACamAccelY1, &BackpaqFPGACamAccelY2, NULL, NULL },
	{ 2, 1, "backpaq/cam_accel_xyz", 
	  &BackpaqFPGACamAccelX1, &BackpaqFPGACamAccelX2, &BackpaqFPGACamAccelY1, &BackpaqFPGACamAccelY2, 
	  &BackpaqFPGACamAccelZ1, &BackpaqFPGACamAccelZ2 },
};

static int    h3600_backpaq_accel_major_num = 0;

/**************************************************************/

static s16_scaled calculate_force( int t1, int t2, u16_fraction scale, u16_fraction offset )
{
	if ((t1 + t2) != 0)
		return (scale * ((t1 * ((1<<16) - offset) - offset * t2) / (t1 + t2))) >> 20;
	else
		return -22;
}

ssize_t h3600_backpaq_accel_read( struct file *filp, char *buf, 
				  size_t count, loff_t *f_pos )
{
	struct h3600_backpaq_accel_dev_struct *dev = (struct h3600_backpaq_accel_dev_struct *)filp->private_data;
	int minor = dev->minor;
	int xyz = dev->xyz; /* asked for xyz data */
	struct h3600_backpaq_accel_data_xyz result;
	int resultlen = 6;
	int xt1, xt2, yt1, yt2, zt1, zt2, rc;
	int status = h3600_backpaq_fpga_status();
	if ( status )
		return status;
	if ( minor > 2 )
		return -EINVAL;

	xt1 = *dev->x1;
	xt2 = *dev->x2;
	yt1 = *dev->y1;
	yt2 = *dev->y2;
	/* if asked for xyz data */ 
	if (xyz && dev->z1 && dev->z2) {
		zt1 = *dev->z1;
		zt2 = *dev->z2;
	} else {
		zt1 = 0;
		zt2 = 0;
	}

	if ( xt1 <= 0 || xt2 <= 0 || yt1 <= 0 || yt2 <= 0 ) {
		static int warned = 0;
		if (!warned++)
			printk(KERN_ALERT __FILE__ ": Illegal accelerometer values %d %d %d %d\n",
			       xt1,xt2,yt1,yt2);
		return -EIO;
	}

	if (xyz)
		resultlen = 6;
	else
		resultlen = 4;

	if (count < resultlen)
		return -EINVAL;

	result.x_acceleration = calculate_force( xt1, xt2, dev->scale_x, dev->offset_x );
	// this handles the different chip orientation.  
	if (h3600_backpaq_eeprom_shadow.major_revision == 3)
		result.y_acceleration = -1 * calculate_force( yt1, yt2, dev->scale_y, dev->offset_y );
	else
		result.y_acceleration = calculate_force( yt1, yt2, dev->scale_y, dev->offset_y );

	if (xyz) /* asked for xyz */
		result.z_acceleration = calculate_force( zt1, zt2, dev->scale_z, dev->offset_z );

	rc = copy_to_user(buf, &result, resultlen);

	if (rc)
		return -EFAULT;

	return resultlen;
}

/*
 * Write function.
 */

ssize_t h3600_backpaq_accel_write( struct file *filp, const char *buf, 
				   size_t count, loff_t *f_pos )
{
	return h3600_backpaq_fpga_status();
}
  
static int h3600_backpaq_accel_ioctl( struct inode *inode, struct file * filp,
				      unsigned int cmd, unsigned long arg)
{
	struct h3600_backpaq_accel_dev_struct *dev = (struct h3600_backpaq_accel_dev_struct *)filp->private_data;
	int retval = h3600_backpaq_fpga_status();
	if ( retval )
		return retval;

	switch(cmd) {
	case H3600_ACCEL_G_PARAMS:
	{
		struct h3600_backpaq_accel_params p;
		p.x_offset = dev->offset_x;
		p.y_offset = dev->offset_y;
		p.x_scale  = dev->scale_x;
		p.y_scale = dev->scale_y;
		if (copy_to_user((void *)arg,&p,sizeof(p)))
			retval = -EFAULT;
		break;
	}
	case H3600_ACCEL_G_RAW:
	{		
		struct h3600_backpaq_accel_raw_data d;
		d.xt1 = *dev->x1;
		d.xt2 = *dev->x2;
		d.yt1 = *dev->y1;
		d.yt2 = *dev->y2;
		if (copy_to_user((void *)arg,&d, sizeof(d)))
			retval = -EFAULT;
		break;
	}
	case H3600_ACCEL_G_PARAMS_XYZ:
	{
		struct h3600_backpaq_accel_params_xyz p;
		p.x_offset = dev->offset_x;
		p.y_offset = dev->offset_y;
		p.z_offset = dev->offset_z;
		p.x_scale  = dev->scale_x;
		p.y_scale  = dev->scale_y;
		p.z_scale  = dev->scale_z;
		if (copy_to_user((void *)arg,&p,sizeof(p)))
			retval = -EFAULT;
		break;
	}
	case H3600_ACCEL_G_RAW_XYZ:
	{		
		struct h3600_backpaq_accel_raw_data_xyz d;
		d.xt1 = *dev->x1;
		d.xt2 = *dev->x2;
		d.yt1 = *dev->y1;
		d.yt2 = *dev->y2;
		d.zt1 = *dev->z1;
		d.zt2 = *dev->z2;
		if (copy_to_user((void *)arg,&d, sizeof(d)))
			retval = -EFAULT;
		break;
	}
	default:
		retval = -ENOIOCTLCMD;
		break;
	}

	return retval;
}

int h3600_backpaq_accel_open( struct inode *inode, struct file *filp )
{
	int status = h3600_backpaq_fpga_status();
	int minor = MINOR(inode->i_rdev);
	struct h3600_backpaq_accel_dev_struct *dev = &accel_data[minor];
	if ( status )
		return status;

	filp->private_data = dev;
	dev->usage_count++;

	if (minor == 0) {
		dev->scale_x = h3600_backpaq_eeprom_shadow.accel_scale_x;
		dev->offset_x = h3600_backpaq_eeprom_shadow.accel_offset_x;
		dev->scale_y = h3600_backpaq_eeprom_shadow.accel_scale_y;
		dev->offset_y = h3600_backpaq_eeprom_shadow.accel_offset_y;
	} else {
		dev->scale_x = h3600_backpaq_eeprom_shadow.cam_accel_scale_x;
		dev->offset_x = h3600_backpaq_eeprom_shadow.cam_accel_offset_x;
		dev->scale_y = h3600_backpaq_eeprom_shadow.cam_accel_scale_y;
		dev->offset_y = h3600_backpaq_eeprom_shadow.cam_accel_offset_y;
		dev->scale_z = h3600_backpaq_eeprom_shadow.cam_accel_scale_z;
		dev->offset_z = h3600_backpaq_eeprom_shadow.cam_accel_offset_z;
	}
	MOD_INC_USE_COUNT;
	return 0;    /* Success */
}

int h3600_backpaq_accel_release( struct inode *inode, struct file *filp )
{
	struct h3600_backpaq_accel_dev_struct *dev = (struct h3600_backpaq_accel_dev_struct *)filp->private_data;
	int result = 0;

	dev->usage_count--;

	MOD_DEC_USE_COUNT;
	return result;
}

/***************************************************************
 *  /proc/backpaq/accel
 ***************************************************************/

#define PRINT_GENERAL_REG(x,s) \
	p += sprintf(p, "%lx %16s : 0x%04x\n", (unsigned long) &x, s, x)
#define PRINT_FPGA_REG(x,s)   PRINT_GENERAL_REG(BackpaqFPGA ## x,s)
#define PRINT_CONSTANT(x,s) \
        p += sprintf(p, "%25s : 0x%04x\n", s, h3600_backpaq_eeprom_shadow.x );

static char * s16_scaled_to_string( char *p, s16_scaled force )
{
	if ( force < 0 ) {
		p += sprintf(p,"-");
		force = -force;
	}
	else {
		p += sprintf(p,"+");
	}

	p += sprintf(p,"%d.%03d", force >> 8, (force & 0xff) * 1000 / 256 );
	return p;
}

int proc_h3600_backpaq_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	char *p = page;
	int len;
	PRINT_FPGA_REG(AccelX1,"Duration Xt1");
	PRINT_FPGA_REG(AccelX2,"Duration Xt2");
	PRINT_FPGA_REG(AccelY1,"Duration Yt1");
	PRINT_FPGA_REG(AccelY2,"Duration Yt2");

	PRINT_FPGA_REG(CamAccelX1,"Cam Duration Xt1");
	PRINT_FPGA_REG(CamAccelX2,"Cam Duration Xt2");
	PRINT_FPGA_REG(CamAccelY1,"Cam Duration Yt1");
	PRINT_FPGA_REG(CamAccelY2,"Cam Duration Yt2");
	PRINT_FPGA_REG(CamAccelZ1,"Cam Duration Zt1");
	PRINT_FPGA_REG(CamAccelZ2,"Cam Duration Zt2");

	PRINT_CONSTANT(accel_offset_x, "Calibration xoffset");
	PRINT_CONSTANT(accel_scale_x,  "Calibration  xscale");
	PRINT_CONSTANT(accel_offset_y, "Calibration yoffset");
	PRINT_CONSTANT(accel_scale_y,  "Calibration  yscale");

	PRINT_CONSTANT(cam_accel_offset_x, "Cam Calibration xoffset");
	PRINT_CONSTANT(cam_accel_scale_x,  "Cam Calibration  xscale");
	PRINT_CONSTANT(cam_accel_offset_y, "Cam Calibration yoffset");
	PRINT_CONSTANT(cam_accel_scale_y,  "Cam Calibration  yscale");
	PRINT_CONSTANT(cam_accel_offset_z, "Cam Calibration zoffset");
	PRINT_CONSTANT(cam_accel_scale_z,  "Cam Calibration  zscale");
	{
		s16_scaled x, y;

		x = calculate_force( BackpaqFPGAAccelX1, BackpaqFPGAAccelX2,
				     h3600_backpaq_eeprom_shadow.accel_scale_x,
				     h3600_backpaq_eeprom_shadow.accel_offset_x );
		y = calculate_force( BackpaqFPGAAccelY1, BackpaqFPGAAccelY2,
				     h3600_backpaq_eeprom_shadow.accel_scale_y,
				     h3600_backpaq_eeprom_shadow.accel_offset_y );

		p += sprintf(p, "Calculated acceleration X : 0x%04hx (", x );
		p = s16_scaled_to_string( p, x );
		p += sprintf(p, " g)\n");

		p += sprintf(p, "Calculated acceleration Y : 0x%04hx (", y );
		p = s16_scaled_to_string( p, y );
		p += sprintf(p, " g)\n");
	}

	{
		s16_scaled x, y, z;

		x = calculate_force( BackpaqFPGACamAccelX1, BackpaqFPGACamAccelX2,
				     h3600_backpaq_eeprom_shadow.cam_accel_scale_x,
				     h3600_backpaq_eeprom_shadow.cam_accel_offset_x );
		y = calculate_force( BackpaqFPGACamAccelY1, BackpaqFPGACamAccelY2,
				     h3600_backpaq_eeprom_shadow.cam_accel_scale_y,
				     h3600_backpaq_eeprom_shadow.cam_accel_offset_y );
		z = calculate_force( BackpaqFPGACamAccelZ1, BackpaqFPGACamAccelZ2,
				     h3600_backpaq_eeprom_shadow.cam_accel_scale_y,
				     h3600_backpaq_eeprom_shadow.cam_accel_offset_y );

		p += sprintf(p, "Calculated acceleration X : 0x%04hx (", x );
		p = s16_scaled_to_string( p, x );
		p += sprintf(p, " g)\n");

		p += sprintf(p, "Calculated acceleration Y : 0x%04hx (", y );
		p = s16_scaled_to_string( p, y );
		p += sprintf(p, " g)\n");

		p += sprintf(p, "Calculated acceleration Z : 0x%04hx (", z );
		p = s16_scaled_to_string( p, z );
		p += sprintf(p, " g)\n");
	}

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

struct file_operations h3600_backpaq_accel_fops = {
	read:    h3600_backpaq_accel_read,
	write:   h3600_backpaq_accel_write,
	open:    h3600_backpaq_accel_open,
	release: h3600_backpaq_accel_release,
	ioctl:   h3600_backpaq_accel_ioctl,
};

/***********************************************************************************/

#ifdef MODULE
#define FPGA_MODULE "h3600_backpaq_fpga"

int __init h3600_backpaq_accel_init_module(void)    
{    
	int result, i;

	/* This module only makes sense if h3600_backpaq_fpga is loaded */
	result = request_module(FPGA_MODULE);
	if ( result ) {
		printk(KERN_ALERT __FILE__ ": unable to load " FPGA_MODULE "\n");
		return result;
	}

	/* Register my device driver */
	printk(KERN_ALERT __FILE__ ": registering char device");    
	result = devfs_register_chrdev(0,MODULE_NAME, &h3600_backpaq_accel_fops);    
	if ( result <= 0 ) {
		printk(" can't get major number\n");
		return result;    
	}    

	if ( h3600_backpaq_accel_major_num == 0 )
		h3600_backpaq_accel_major_num = result;
	printk(" %d\n", h3600_backpaq_accel_major_num);

#ifdef CONFIG_DEVFS_FS
	for (i = 0; i < 3; i++ ) {
		int accel_minor = accel_data[i].minor;
		/* Clear the usage count */
		accel_data[i].usage_count = 0;
		accel_data[i].devfs =
			devfs_register( NULL, accel_data[i].name, DEVFS_FL_DEFAULT,
					h3600_backpaq_accel_major_num, accel_minor,
					S_IFCHR | S_IRUSR | S_IWUSR, 
					&h3600_backpaq_accel_fops, NULL );
		if ( !accel_data[i].devfs ) {
			printk(KERN_ALERT __FILE__ ": unable to register %s\n", ACCEL_DEVICE_NAME );
			devfs_unregister_chrdev( h3600_backpaq_accel_major_num, MODULE_NAME );
			return -ENODEV;
		}
	}
#endif

#ifdef CONFIG_PROC_FS
	if ( !(proc_backpaq_accel = create_proc_entry(ACCEL_PROC_NAME,0,NULL))) {
		// Couldn't create - we need to create the "backpaq" directory
		proc_mkdir(ACCEL_PROC_DIR,0);
		proc_backpaq_accel = create_proc_entry(ACCEL_PROC_NAME,0,NULL);
	}
	
	if ( proc_backpaq_accel )
		proc_backpaq_accel->read_proc = proc_h3600_backpaq_read;
	else {
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", ACCEL_PROC_NAME);
		for (i = 0; i < 3; i++) {
			if (accel_data[i].devfs)
				devfs_unregister( accel_data[i].devfs );
		}
		devfs_unregister_chrdev( h3600_backpaq_accel_major_num, MODULE_NAME );
		return -ENODEV;
	}
#endif

	return 0;    
} 

void __exit h3600_backpaq_accel_exit_module(void)
{
	int i;
	printk(KERN_ALERT __FILE__ ": exit\n");

#ifdef CONFIG_PROC_FS
	if (proc_backpaq_accel) 
		remove_proc_entry(ACCEL_PROC_NAME, 0);
#endif
	for (i = 0; i < 3; i++) {
		if (accel_data[i].devfs)
			devfs_unregister( accel_data[i].devfs );
	}
	devfs_unregister_chrdev( h3600_backpaq_accel_major_num, MODULE_NAME );
}

MODULE_AUTHOR("Andrew Christian <andyc@handhelds.org>");
MODULE_DESCRIPTION("Accelerometer driver for Compaq Mercury BackPAQ");
MODULE_LICENSE("Dual BSD/GPL");

module_init(h3600_backpaq_accel_init_module);
module_exit(h3600_backpaq_accel_exit_module);

#endif /* MODULE */
