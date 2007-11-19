/*
* Hardware abstraction layer for HP iPAQ H3xxx Pocket Computers
*
* Copyright 2000,2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Andrew Christian
* October, 2001
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/uaccess.h>        /* get_user,copy_to_user */
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/console.h>
#include <linux/devfs_fs_kernel.h>

#include <linux/tqueue.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/apm_bios.h>
#include <linux/kmod.h>

#include <asm/hardware.h>
#include <asm/arch-sa1100/h3600_hal.h>

#define H3600_HAL_PROC_DIR     "hal"

/* Interface supplied by the low-level microcontroller driver */
struct h3600_hal_ops    *h3600_hal_ops = NULL;
EXPORT_SYMBOL(h3600_hal_ops);

static struct h3600_driver_ops g_driver_ops;

/* Global values */
static unsigned char  flite_brightness          = 25; 
static int            max_flite_brightness      = 255;
static enum flite_pwr flite_power               = FLITE_PWR_ON;
static int            lcd_active                = 1;
static unsigned char  screen_contrast           = 100;

/* Parameters */

MODULE_PARM(flite_brightness,"b");
MODULE_PARM_DESC(flite_brightness,"Initial brightness setting of the frontlight");
MODULE_PARM(max_flite_brightness,"i");
MODULE_PARM_DESC(max_flite_brightness,"Maximum allowable brightness setting of the frontlight");
MODULE_PARM(flite_power,"i");
MODULE_PARM_DESC(flite_power,"Initial power setting of the frontlight (on/off)");
MODULE_PARM(screen_contrast,"i");
MODULE_PARM_DESC(screen_contrast,"Initial screen contrast (for H3100 only)");

MODULE_AUTHOR("Andrew Christian");
MODULE_DESCRIPTION("Hardware abstraction layer for the iPAQ H3600");
MODULE_LICENSE("Dual BSD/GPL");

/***********************************************************************************/
/*   General callbacks                                                             */
/***********************************************************************************/

int h3600_set_flite(enum flite_pwr pwr, unsigned char brightness)
{
	if (0) printk("### %s: pwr=%d brightness=%d lcdactive=%d\n",
		      __FUNCTION__, pwr, brightness, lcd_active );

	if ( brightness > max_flite_brightness )
		brightness = max_flite_brightness;

	/* Save the current settings */
	flite_power        = pwr;
	flite_brightness   = brightness;

	return h3600_backlight_control( (lcd_active ? flite_power : FLITE_PWR_OFF), 
					(pwr == FLITE_PWR_ON && lcd_active ? flite_brightness : 0) );
}

void h3600_get_flite( struct h3600_ts_backlight *bl )
{
	bl->power      = flite_power;
	bl->brightness = flite_brightness;
}

int h3600_set_contrast( unsigned char contrast )
{
	if (0) printk("%s: contrast=%d\n", __FUNCTION__, contrast );

	screen_contrast = contrast;
	return h3600_contrast_control( screen_contrast );
}

void h3600_get_contrast( u_char *contrast )
{
	*contrast = screen_contrast;
}

int h3600_toggle_frontlight( void )
{
	return h3600_set_flite( 1 - flite_power, flite_brightness );
}

EXPORT_SYMBOL(h3600_get_flite);
EXPORT_SYMBOL(h3600_set_flite);
EXPORT_SYMBOL(h3600_get_contrast);
EXPORT_SYMBOL(h3600_set_contrast);
EXPORT_SYMBOL(h3600_toggle_frontlight);

/* TODO
   
   Check on blanking modes.  Right now we seem to have problems with the console
   blanking.  Its recovery appears to be in interrupt context.  Question - is this 
   function always called in interrupt context?  If it is, move the h3600_flite_control
   to a task.  If not, should we be using the power manager?

   This function is called by sa1100fb to turn on and off the backlight.  There
   are several reasons it can be called:

      1. The framebuffer is being shut down from a suspend/resume.
      2. Someone like an X server has requested blanking

   On a blank request, we shut off the backlight.  On an "unblank" request,
   we restore the backlight to its prior setting.
*/

static void h3600_hal_backlight_helper(int blank)
{
	if (0) printk("  %s: backlight=%d interrupt=%d\n", __FUNCTION__, blank, in_interrupt());

	lcd_active = !blank;
	h3600_set_flite( flite_power, flite_brightness );
}

void h3600_hal_keypress( unsigned char key )
{
	if ( g_driver_ops.keypress )
		g_driver_ops.keypress(key);
}

void h3600_hal_touchpanel( unsigned short x, unsigned short y, int down )
{
	if ( g_driver_ops.touchpanel )
		g_driver_ops.touchpanel(x,y,down);
}

void h3600_hal_option_detect( int present )
{
	if ( g_driver_ops.option_detect )
		g_driver_ops.option_detect(present);
}

EXPORT_SYMBOL(h3600_hal_keypress);
EXPORT_SYMBOL(h3600_hal_touchpanel);
EXPORT_SYMBOL(h3600_hal_option_detect);

/***********************************************************************************/
/*      Functions exported for use by the kernel and kernel modules                */
/***********************************************************************************/

int h3600_apm_get_power_status(u_char *ac_line_status,
			       u_char *battery_status, 
			       u_char *battery_flag, 
			       u_char *battery_percentage, 
			       u_short *battery_life)
{
	struct h3600_battery bstat;
	unsigned char ac    = APM_AC_UNKNOWN;
	unsigned char level = APM_BATTERY_STATUS_UNKNOWN;
	int status, result;

	result = h3600_get_battery(&bstat);
	if (result) {
		printk("%s: unable to access battery information: result=%d\n", __FUNCTION__, result);
		return 0;
	}

	switch (bstat.ac_status) {
	case H3600_AC_STATUS_AC_OFFLINE:
		ac = APM_AC_OFFLINE;
		break;
	case H3600_AC_STATUS_AC_ONLINE:
		ac = APM_AC_ONLINE;
		break;
	case H3600_AC_STATUS_AC_BACKUP:
		ac = APM_AC_BACKUP;
		break;
	}

	if (ac_line_status != NULL)
		*ac_line_status = ac;

	status = bstat.battery[0].status;
	if (status & (H3600_BATT_STATUS_CHARGING | H3600_BATT_STATUS_CHARGE_MAIN))
		level = APM_BATTERY_STATUS_CHARGING;
	else if (status & (H3600_BATT_STATUS_HIGH | H3600_BATT_STATUS_FULL))
		level = APM_BATTERY_STATUS_HIGH;
	else if (status & H3600_BATT_STATUS_LOW)
		level = APM_BATTERY_STATUS_LOW;
	else if (status & H3600_BATT_STATUS_CRITICAL)
		level = APM_BATTERY_STATUS_CRITICAL;

	if (battery_status != NULL)
		*battery_status = level;

	if (battery_percentage != NULL)
		*battery_percentage = bstat.battery[0].percentage;

	/* assuming C/5 discharge rate */
	if (battery_life != NULL) {
		*battery_life = bstat.battery[0].life;
		*battery_life |= 0x8000;   /* Flag for minutes */
	}
                        
	return 1;
}

EXPORT_SYMBOL(h3600_apm_get_power_status);

/***********************************************************************************/
/*   Proc filesystem interface                                                     */
/***********************************************************************************/

static struct ctl_table h3600_hal_table[] = 
{
	{3, "max_flite_brightness", &max_flite_brightness, sizeof(max_flite_brightness), 
	 0666, NULL, &proc_dointvec},
	{0}
};

static struct ctl_table h3600_hal_dir_table[] =
{
	{11, "hal", NULL, 0, 0555, h3600_hal_table},
	{0}
};

static struct ctl_table_header *h3600_hal_sysctl_header = NULL;
static struct proc_dir_entry   *hal_proc_dir = NULL;

enum hal_proc_index {
	HAL_DONE,
	HAL_VERSION,
	HAL_THERMAL_SENSOR,
	HAL_BATTERY,
	HAL_LIGHT_SENSOR,
	HAL_ASSETS,
	HAL_MODEL,
	HAL_SCREEN_ROTATION
};

struct h3600_proc_item {
	const char *name;
	enum hal_proc_index id;
};

static struct h3600_proc_item g_procitems[] = {
	{"version",      HAL_VERSION        },
	{"thermal",      HAL_THERMAL_SENSOR },
	{"battery",      HAL_BATTERY        },
	{"light_sensor", HAL_LIGHT_SENSOR   },
	{"assets",       HAL_ASSETS         },
	{"model",        HAL_MODEL          },
	{"screen_rotation",     HAL_SCREEN_ROTATION    },
	{"done",         HAL_DONE           },
};

struct battery_flag_name {
	int    bits;
	char  *name;
};

static struct battery_flag_name battery_chemistry[] = {
	{ H3600_BATT_CHEM_ALKALINE, "alkaline" },
	{ H3600_BATT_CHEM_NICD,     "NiCd" },
	{ H3600_BATT_CHEM_NIMH,     "NiMH" },
	{ H3600_BATT_CHEM_LION,     "Li-ion" },
	{ H3600_BATT_CHEM_LIPOLY,   "Li-Polymer" },
	{ H3600_BATT_CHEM_NOT_INSTALLED, "Not installed" },
	{ H3600_BATT_CHEM_UNKNOWN,  "unknown" },
	{ -1, "?" }
};

static struct battery_flag_name ac_status[] = {
	{ H3600_AC_STATUS_AC_OFFLINE, "offline" },
	{ H3600_AC_STATUS_AC_ONLINE,  "online" },
	{ H3600_AC_STATUS_AC_BACKUP,  "backup" },
	{ H3600_AC_STATUS_AC_UNKNOWN, "unknown" },
	{ -1, "?" }
};

static struct battery_flag_name battery_status[] = {
	{ H3600_BATT_STATUS_HIGH,     "high" },
	{ H3600_BATT_STATUS_LOW,      "low" },
	{ H3600_BATT_STATUS_CRITICAL, "critical" },
	{ H3600_BATT_STATUS_CHARGING, "charging" },
	{ H3600_BATT_STATUS_CHARGE_MAIN, "charge main" },
	{ H3600_BATT_STATUS_DEAD,     "dead" },
	{ H3600_BATT_STATUS_FULL,     "fully charged" },
	{ H3600_BATT_STATUS_NOBATT,   "no battery" },
	{ -1, "?" }
};

static char * extract_flag_name( struct battery_flag_name *list, unsigned char value )
{
	while ( list->bits != -1 && list->bits != value )
		list++;
	return list->name;
}

static void battery_status_to_string (unsigned char value, char *buf)
{
	char *p = buf;
	struct battery_flag_name *f = &battery_status[0];

	if (value == H3600_BATT_STATUS_UNKNOWN) {
		strcpy (buf, "unknown");
		return;
	}

	while (f->bits != -1) {
		if (value & (f->bits)) {
			if (p != buf) {
				*p++ = ';';
				*p++ = ' ';
			}
			strcpy (p, f->name);
			p += strlen (f->name);
		}
		f++;
	}
	
	*p = 0;
}

struct params_list {
	int   value;
	char *name;
};

static struct params_list product_id[] = {
	{ 2, "Palm" },
	{ -1, "" },
};

static struct params_list page_mode[] = {
	{ 0, "Flash" },
	{ 1, "ROM" },
	{ -1, "" },
};

/* These are guesses */
static struct params_list country_id[] = {
	{ 0, "USA" },
	{ 1, "English" },
	{ 2, "German" },
	{ 3, "French" },
	{ 4, "Italian" },
	{ 5, "Spanish" },
	{ 0x8001, "Traditional Chinese" },
	{ 0x8002, "Simplified Chinese" },
	{ 0x8003, "Japanese" },
	{ -1, "" },
};

#define BASIC_FORMAT "%20s : "

static char * lookup_params( struct params_list *list, int value )
{
	if ( !list )
		return NULL;

	while ( list->value != -1 && list->value != value )
		list++;
	return list->name;
}

static char * asset_print_tchar( char *p, struct h3600_asset *asset,
				 unsigned char *name, struct params_list *list )
{
	int len = ASSET_TCHAR_LEN(asset->type);
	unsigned char *data = asset->a.tchar;

//	printk(" * Printing %s at %p, length = %d (%p)\n", name, data, len, p );

	p += sprintf(p, BASIC_FORMAT, name );
	for (  ; len > 0 ; len-- ) {
		if ( *data )
			*p++ = *data;
		data++;
	}
        *p++ = '\n';
	return p;
}

static char * asset_print_word( char *p, struct h3600_asset *asset, 
				unsigned char *name, struct params_list *list )
{
	unsigned short value = asset->a.vshort;
	char *param = lookup_params(list,value);

//	printk(" * Printing %s value %d (%p)\n", name, value, p );

	if ( param )
		p += sprintf(p, BASIC_FORMAT "%d (%s)\n", name, value, param );
	else
		p += sprintf(p, BASIC_FORMAT "%d\n", name, value);
	return p;
}

/*
static struct params_table params_table[] = {
	{ 0, PARAMS_TCHAR, 5,  "HM Version" },
	{ 10, PARAMS_TCHAR, 20, "Serial #" },
	{ 50, PARAMS_TCHAR, 10, "Module ID" },
	{ 70, PARAMS_TCHAR, 5,  "Product Revision" },
	{ 80, PARAMS_WORD,  0,  "Product ID", product_id },
	{ 82, PARAMS_WORD,  0,  "Frame Rate" },
	{ 84, PARAMS_WORD,  0,  "Page Mode", page_mode },
	{ 86, PARAMS_WORD,  0,  "Country ID", country_id },
	{ 88, PARAMS_WORD,  0,  "Is Color Display" },
	{ 90, PARAMS_WORD,  0,  "ROM Size" },
	{ 92, PARAMS_WORD,  0,  "RAM Size" },
	{ 94, PARAMS_WORD,  0,  "Horizontal pixels" },
	{ 96, PARAMS_WORD,  0,  "Vertical pixels" },
	{ 0,  PARAMS_DONE, 0, NULL  }
};
*/

//#define PRINT_TCHAR(_p,_name,_x)  _p = print_param_tchar(_p, _name, _x, sizeof(_x))

typedef char * (*asset_printer)( char *p, struct h3600_asset *asset, 
				 unsigned char * name, struct params_list *list );

static asset_printer asset_printers[] = {
	asset_print_tchar,
	asset_print_word,
};

struct params_table {
	unsigned long       asset;
	char               *name;
	struct params_list *list;
};

static struct params_table params_table[] = {
	{ ASSET_HM_VERSION,       "HM Version" },
	{ ASSET_SERIAL_NUMBER,    "Serial #" },
	{ ASSET_MODULE_ID,        "Module ID" },
	{ ASSET_PRODUCT_REVISION, "Product Revision" },
	{ ASSET_PRODUCT_ID,       "Product ID", product_id },
	{ ASSET_FRAME_RATE,       "Frame Rate" },
	{ ASSET_PAGE_MODE,        "Page Mode",  page_mode },
	{ ASSET_COUNTRY_ID,       "Country ID", country_id },
	{ ASSET_IS_COLOR_DISPLAY, "Is Color Display" },
	{ ASSET_ROM_SIZE,         "ROM Size" },
	{ ASSET_RAM_SIZE,         "RAM Size" },
	{ ASSET_HORIZONTAL_PIXELS,"Horizontal pixels" },
	{ ASSET_VERTICAL_PIXELS,  "Vertical pixels" }
};

static char * h3600_hal_parse_eeprom( char *p )
{
	struct h3600_asset asset;
	int i;
	int retval;

//	printk( __FUNCTION__ " : called with %p\n", p );

	/* Suck in the data */
//	retval = h3600_asset_read( &asset );
//	if ( retval ) {
//		p += sprintf(p,"Error value %d\n", retval);
//		return p;
//	}

//	retval = sizeof(asset.hm_version);
//	p = print_param_tchar( p, "HM Version", asset.hm_version, retval );
//	printk(" *** assets at %p\n", &asset );

	for ( i = 0 ; i < (sizeof(params_table) / sizeof(struct params_table)) ; i++ ) {
//		printk( " ** parsing %d   0x%08lX %ld\n", i, params_table[i].asset,
//			ASSET_TYPE(params_table[i].asset));
		asset.type = params_table[i].asset;
		retval = h3600_asset_read( &asset );
		if ( retval ) {
			p += sprintf(p,"Error value %d\n", retval);
			return p;
		}
		p = asset_printers[ASSET_TYPE(params_table[i].asset)]( p, &asset, 
								       params_table[i].name,
								       params_table[i].list );
	}
/*	PRINT_TCHAR( p, "HM Version",       asset.hm_version );
	PRINT_TCHAR( p, "Serial #",         asset.serial_number );
	PRINT_TCHAR( p, "Module ID",        asset.module_id );
	PRINT_TCHAR( p, "Product Revision", asset.product_revision );

	p = print_param_word( p, "Product ID",        asset.product_id,        product_id );
	p = print_param_word( p, "Frame Rate",        asset.frame_rate,        NULL );
	p = print_param_word( p, "Page Mode",         asset.page_mode,         page_mode );
	p = print_param_word( p, "Country ID",        asset.country_id,        country_id );
	p = print_param_word( p, "Is Color Display",  asset.is_color_display,  NULL );
	p = print_param_word( p, "ROM Size",          asset.rom_size,          NULL );
	p = print_param_word( p, "RAM Size",          asset.ram_size,          NULL );
	p = print_param_word( p, "Horizontal pixels", asset.horizontal_pixels, NULL );
	p = print_param_word( p, "Vertical pixels",   asset.vertical_pixels,   NULL );*/

//	printk( __FUNCTION__ " : returning %p\n", p );
	return p;
}

/*
#include <linux/ctype.h>
#define DUMP_BUF_SIZE   16
#define MYPRINTABLE(x) \
    ( ((x)<='z' && (x)>='a') || ((x)<='Z' && (x)>='A') || isdigit(x) || (x)==':' )

static void dump_mem( unsigned char *p, int count )
{
	u_char pbuf[DUMP_BUF_SIZE + 1];
	int    len;
	int    i;

	printk(__FUNCTION__ " %p %d\n", p, count);

	while ( count ) {
		len = count;
		if ( len > DUMP_BUF_SIZE )
			len = DUMP_BUF_SIZE;

		for ( i = 0 ; i < len ; i++ ) {
			printk(" %02x", *p );
			pbuf[i] = ( MYPRINTABLE(*p) ? *p : '.' );
			count--;
			p++;
		}

		pbuf[i] = '\0';
		printk(" %s\n", pbuf);
	}
}
*/

static int h3600_hal_proc_item_read(char *page, char **start, off_t off,
			       int count, int *eof, void *data)
{			      
	char *p = page;
	int len;
	int retval = 0;

	if (0) printk("%s: %p %p %ld %d %p %p\n", __FUNCTION__, page, start, off, count, eof, data );

	switch ((int)data) {
	case HAL_VERSION:
	{
		struct h3600_ts_version v;
		retval = h3600_get_version( &v );
		if ( !retval ) {
			p += sprintf(p, "Host      : %s\n", v.host_version );
		        p += sprintf(p, "Pack      : %s\n", v.pack_version );
			p += sprintf(p, "Boot type : 0x%02x\n", v.boot_type );
		}
		break;
	}
	case HAL_THERMAL_SENSOR:
	{
		unsigned short v;
		retval = h3600_get_thermal_sensor( &v );
		if ( !retval )
			p += sprintf(p, "0x%04x\n", v );
		break;
	}
	case HAL_BATTERY:
	{
		struct h3600_battery v;
		retval = h3600_get_battery( &v );
		if ( !retval ) {
			char buf[256];
			int i;
			p += sprintf(p, "AC status   : %x (%s)\n", v.ac_status,
				     extract_flag_name(ac_status,v.ac_status));

			for ( i = 0 ; i < v.battery_count ; i++ ) {
				p += sprintf(p, "Battery #%d\n", i);
				p += sprintf(p, " Chemistry  : 0x%02x   (%s)\n", 
					     v.battery[i].chemistry,
					     extract_flag_name(battery_chemistry, 
							       v.battery[i].chemistry));
				battery_status_to_string (v.battery[i].status, buf);
				p += sprintf(p, " Status     : 0x%02x   (%s)\n", 
					     v.battery[i].status,
					     buf);
				if ( i == 0 ) 
					p += sprintf(p, " Voltage    : 0x%04x (%4d mV)\n", 
						     v.battery[i].voltage,
						     v.battery[i].voltage * 5000 / 1024);
				p += sprintf(p, " Percentage : 0x%02x", v.battery[i].percentage);
				if ( v.battery[i].percentage == 0xff )
					p += sprintf(p, "   (unknown)\n");
				else 
					p += sprintf(p, "   (%d%%)\n", v.battery[i].percentage);
				if ( i == 0 )
					p += sprintf(p, " Life (min) : %d\n", v.battery[i].life);
			}
		}
		break;
	}
	case HAL_LIGHT_SENSOR:
	{
		unsigned char level;
		retval = h3600_get_light_sensor(&level);
		if (!retval)
			p += sprintf(p,"0x%02x\n", level );
		break;
	}
	case HAL_ASSETS:
		p = h3600_hal_parse_eeprom( p );
		break;
	case HAL_MODEL:
		p += sprintf(p,"%s\n", h3600_generic_name() );
		break;
	case HAL_SCREEN_ROTATION:
		p += sprintf(p,"%s\n", h3600_screen_rotation() );
		break;
	default:
		p += sprintf(p,"Unsupported item %d\n", (int)data);
		break;
	}

	if ( retval ) {
		p += sprintf(p,"Error value %d\n", retval);
	}

//	dump_mem( page, p - page );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}


/***********************************************************************************/
/*       Registration functions from low level and from device drivers             */
/***********************************************************************************/

int h3600_hal_register_interface( struct h3600_hal_ops *ops )
{
	if ( !ops || !ops->owner )
		return -EFAULT;

	h3600_hal_ops     = ops;
	/* Initialize our screen and frontlight */
	h3600_set_flite( flite_power, flite_brightness );
	h3600_set_contrast( screen_contrast );

        return 0;
}

void h3600_hal_unregister_interface( struct h3600_hal_ops *ops )
{
	h3600_hal_ops     = NULL;
}

EXPORT_SYMBOL(h3600_hal_register_interface);
EXPORT_SYMBOL(h3600_hal_unregister_interface);

#define HAL_REG(x) \
	if ( ops->x ) g_driver_ops.x = ops->x
        
#define HAL_UNREG(x) \
	if ( ops->x ) g_driver_ops.x = NULL
        
int h3600_hal_register_driver( struct h3600_driver_ops *ops )
{
	HAL_REG(keypress);
	HAL_REG(touchpanel);
	HAL_REG(option_detect);
	return 0;
}

void h3600_hal_unregister_driver( struct h3600_driver_ops *ops )
{
	HAL_UNREG(keypress);
	HAL_UNREG(touchpanel);
	HAL_UNREG(option_detect);
}

EXPORT_SYMBOL(h3600_hal_register_driver);
EXPORT_SYMBOL(h3600_hal_unregister_driver);


/***********************************************************************************/
/*       Initialization                                                            */
/***********************************************************************************/

#ifdef CONFIG_FB_SA1100
extern void (*sa1100fb_blank_helper)(int blank);
#endif
#ifdef CONFIG_FB_PXA
extern void (*pxafb_blank_helper)(int blank);
#endif

int __init h3600_hal_init_module(void)
{
	int i;
        printk(KERN_INFO "H3600 Registering HAL abstraction layer\n");

	/* Request the appropriate underlying module to provide services */
	/* TODO: We've put this here to avoid having to add another /etc/module
	   line for people upgrading their kernel.  In the future, we'd like to 
	   automatically request and load the appropriate subsystem based on the
	   type of iPAQ owned */
	if ( machine_is_h3100() || machine_is_h3600() ) {
		request_module("h3600_micro");
	} 
	else if ( machine_is_h3800() ) {
		request_module("h3600_asic");
	}
	else if ( machine_is_h3900() ) {
		request_module("h3900_asic");
	} else if ( machine_is_h5400() ) {
		request_module("h5400_asic");
	} else if ( machine_is_h1900() ) {
		request_module("h1900_asic");
	}

	h3600_hal_sysctl_header = register_sysctl_table(h3600_hal_dir_table, 0);
#ifdef CONFIG_FB_SA1100
	sa1100fb_blank_helper = h3600_hal_backlight_helper;
#endif
#ifdef CONFIG_FB_PXA
	pxafb_blank_helper = h3600_hal_backlight_helper;
#endif

	/* Register in /proc filesystem */
	hal_proc_dir = proc_mkdir(H3600_HAL_PROC_DIR, NULL);
	if ( hal_proc_dir )
		for ( i = 0 ; g_procitems[i].id != HAL_DONE ; i++ )
			create_proc_read_entry(g_procitems[i].name, 0, hal_proc_dir, 
					       h3600_hal_proc_item_read, (void *) g_procitems[i].id );
	else
		printk(KERN_ALERT "%s: unable to create proc entry %s\n", __FUNCTION__, H3600_HAL_PROC_DIR);
	return 0;
}

void h3600_hal_cleanup_module(void)
{
	int i;
	printk(KERN_INFO "H3600 shutting down HAL abstraction layer\n");

#ifdef CONFIG_FB_SA1100
	sa1100fb_blank_helper = NULL;
#endif
#ifdef CONFIG_FB_PXA
	pxafb_blank_helper = NULL;
#endif
        unregister_sysctl_table(h3600_hal_sysctl_header);

	if ( hal_proc_dir ) {
		for ( i = 0 ; g_procitems[i].id != HAL_DONE ; i++ )
			remove_proc_entry(g_procitems[i].name, hal_proc_dir);
		remove_proc_entry(H3600_HAL_PROC_DIR, NULL );
		hal_proc_dir = NULL;
	}
}

module_init(h3600_hal_init_module);
module_exit(h3600_hal_cleanup_module);
