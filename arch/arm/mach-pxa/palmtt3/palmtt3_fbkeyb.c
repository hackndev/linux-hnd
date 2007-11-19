/*
 * On-screen keyboard in kernel space
 *
 * Author: Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *
 */
#include <linux/module.h>
#include <linux/config.h>

#include <linux/input.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include <linux/fb.h>
#include <linux/vt_kern.h>

static int resized;

void drawkeyb(void)
{
	/*
	struct fb_info *p;
	struct vc_data *vc;
	struct fb_fillrect rect;
	int i;
	unsigned long fg;
	
	if(resized<2) {
		resized++;
		vc = vc_cons[0].d;
		vc_resize(vc, 40, 41);
	}
	p = registered_fb[0];
	
	rect.dx = 0;
	rect.width = 320;
	rect.height = 2;
	rect.color = 0x00FF00FF;
	rect.rop = ROP_COPY;
	for(i = 150; i < 320; i+=10) {
		rect.dy = i;
		cfb_fillrect(p, &rect);
	}
	if (p->fix.visual == FB_VISUAL_TRUECOLOR || p->fix.visual == FB_VISUAL_DIRECTCOLOR ) {
		fg = ((u32 *) (p->pseudo_palette))[rect.color];
		printk(KERN_INFO "Direct col %lu\n", fg);
	} else {
		fg = rect.color;
		printk(KERN_INFO "Indir col %lu\n", fg);
	}
	*/
}

static int __init palmtt3_fbk_init(void)
{
	printk(KERN_INFO "fbkeyb: init\n");
	resized = 0;
	
	return 0;
}

module_init(palmtt3_fbk_init);
