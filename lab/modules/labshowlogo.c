/* labshowlogo.c
 * Blits a logo to the framebuffer.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>
#include <linux/fb.h>

#include "bootlogo.h"

void lab_cmd_showlogo(int argc,const char** argv);

int labshowlogo_init(void)
{
	lab_addcommand("showlogo", lab_cmd_showlogo, "Shows LAB booting screen");
	
	return 0;
}

void labshowlogo_cleanup(void)
{
	lab_delcommand("showlogo");
}

void lab_cmd_showlogo(int argc,const char** argv)
{
	int i;
	
	for (i=0; i<num_registered_fb; i++)
	{
		lab_printf("Framebuffer %d: Type %d/%d, Visual %d, BPP %d\n", i, 
			registered_fb[i]->fix.type,
			registered_fb[i]->fix.type_aux,
			registered_fb[i]->fix.visual,
			registered_fb[i]->var.bits_per_pixel);
		
		memcpy(registered_fb[i]->screen_base, lab_image_data.pixel_data, lab_image_data.bytes_per_pixel*lab_image_data.width*lab_image_data.height);
	}
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB showlogo Module");
MODULE_LICENSE("GPL");
module_init(labshowlogo_init);
module_exit(labshowlogo_cleanup);
