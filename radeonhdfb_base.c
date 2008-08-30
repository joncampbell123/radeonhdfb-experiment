/*
 *      Linux framebuffer driver for Radeon HD R500/R600 based cards
 *
 *	Copyright (C) 2008 Jonathan Campbell
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include <linux/fb.h>
#include <linux/init.h>

#include "radeonhdfb.h"

static struct pci_device_id radeonhdfb_pci_table[] = {
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, radeonhdfb_pci_table);

static struct fb_var_screeninfo radeonhdfb_default __initdata = {
	.xres =		640,
	.yres =		480,
	.xres_virtual =	640,
	.yres_virtual =	480,
	.bits_per_pixel = 8,
	.red =		{ 0, 8, 0 },
      	.green =	{ 0, 8, 0 },
      	.blue =		{ 0, 8, 0 },
      	.activate =	FB_ACTIVATE_TEST,
      	.height =	-1,
      	.width =	-1,
      	.pixclock =	20000,
      	.left_margin =	64,
      	.right_margin =	64,
      	.upper_margin =	32,
      	.lower_margin =	32,
      	.hsync_len =	64,
      	.vsync_len =	2,
      	.vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo radeonhdfb_fix __initdata = {
	.id =		"Radeon HD",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_PSEUDOCOLOR,
	.xpanstep =	1,
	.ypanstep =	1,
	.ywrapstep =	1,
	.accel =	FB_ACCEL_NONE,
};

static int radeonhdfb_check_var(struct fb_var_screeninfo *var,
		struct fb_info *info);
static int radeonhdfb_set_par(struct fb_info *info);
static int radeonhdfb_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *info);

static struct fb_ops radeonhdfb_ops = {
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
	.fb_check_var	= radeonhdfb_check_var,
	.fb_set_par	= radeonhdfb_set_par,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
        .fb_pan_display = radeonhdfb_pan_display,
};

    /*
     *  Internal routines
     */

static u_long get_line_length(int xres_virtual, int bpp)
{
	u_long length;

	length = xres_virtual * bpp;
	length = (length + 31) & ~31;
	length >>= 3;
	return (length);
}

    /*
     *  Setting the video mode has been split into two parts.
     *  First part, xxxfb_check_var, must not write anything
     *  to hardware, it should only verify and adjust var.
     *  This means it doesn't alter par but it does use hardware
     *  data from it to check this var. 
     */

static int radeonhdfb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info)
{
	u_long line_length;
	BLAH_("check_var\n");

	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	/*
	 *  Some very basic checks
	 */
	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24)
		var->bits_per_pixel = 24;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/*
	 *  Memory limit
	 */
	line_length =
	    get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (1)
		return -ENOMEM;

	/*
	 * Now that we checked it we alter var. The reason being is that the video
	 * mode passed in might not work but slight changes to it might make it 
	 * work. This way we let the user know what is acceptable.
	 */
	switch (var->bits_per_pixel) {
	case 1:
	case 8:
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 16:		/* RGBA 5551 */
		if (var->transp.length) {
			var->red.offset = 0;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 5;
			var->blue.offset = 10;
			var->blue.length = 5;
			var->transp.offset = 15;
			var->transp.length = 1;
		} else {	/* RGB 565 */
			var->red.offset = 0;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 6;
			var->blue.offset = 11;
			var->blue.length = 5;
			var->transp.offset = 0;
			var->transp.length = 0;
		}
		break;
	case 24:		/* RGB 888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:		/* RGBA 8888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

static int radeonhdfb_pan_display(struct fb_var_screeninfo *var,struct fb_info *info)
{
	BLAH_("pan_display\n");
	return 0;
}

/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the 
 * change in par. For this driver it doesn't do much. 
 */
static int radeonhdfb_set_par(struct fb_info *info)
{
	BLAH_("set_par\n");
	info->fix.line_length = get_line_length(info->var.xres_virtual,
						info->var.bits_per_pixel);
	return 0;
}

#ifndef MODULE
static int __init radeonhdfb_setup(char *options)
{
	char *this_opt;

	BLAH_("setup\n");
	if (!options || !*options)
		return 1;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
	}
	return 1;
}
#endif  /*  MODULE  */

    /*
     *  Initialisation
     */

static int __init radeonhdfb_probe(struct pci_dev *pdev,const struct pci_device_id *ent)
{
	BLAH_("probe\n");
	return -ENODEV;
}

static void __devexit radeonhdfb_remove(struct pci_dev *dev)
{
	BLAH_("remove()\n");
}

static struct pci_driver radeonhdfb_driver = {
	.name	= "radeonhdfb",
	.id_table = radeonhdfb_pci_table,
	.probe  = radeonhdfb_probe,
	.remove = __devexit_p(radeonhdfb_remove),
};

static int __init radeonhdfb_init(void)
{
#ifndef MODULE
	char *option = NULL;

	BLAH_("init()\n");
	if (fb_get_options("radeonhdfb", &option)) {
		BLAH("fb_get_options() failed\n",0);
		return -ENODEV;
	}

	BLAH_("now calling setup()\n");
	radeonhdfb_setup(option);
#endif
	BLAH_("now calling pci_register_driver\n");
	return pci_register_driver (&radeonhdfb_driver);
}

module_init(radeonhdfb_init);

#ifdef MODULE
static void __exit radeonhdfb_exit(void)
{
	BLAH_("exit()\n");
	pci_unregister_driver (&radeonhdfb_driver);
}

module_exit(radeonhdfb_exit);

MODULE_LICENSE("GPL");
#endif				/* MODULE */
