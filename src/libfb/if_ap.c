/** \addtogroup if */
/*@{*/
/** \file if_ap.c
*  Apollo display interface using GPR.  This interface runs under the
*  display manager(direct mode).
*
*  Author-
*           Dave Barber
*
*  Source -
*           Idaho National Engineering Lab
*           P.O. Box 1625
*           Idaho Falls, ID 83415-2408
*           (208)526-9415
*
*  Copyright Notice -
*           This software is Copyright (C) 1989 by the Department of Energy.
*           All rights reserved.
*
*  BRL NOTE: This is only the scant beginnings of an Apollo interface.
*   We have no way of testing this, and given the changes in LIBFB this
*   code may not even compile any longer.  If you make improvements to
*   this please let us know.
*/
/*@}*/

#include "common.h"

#include <stdio.h>
#include <time.h>

#include "machine.h"
#include "base.h"
#include "gpr.h"
#include "fb.h"
#include "./fblocal.h"


HIDDEN int	ap_open(),
		ap_close(),
		ap_clear(),
		ap_read(),
		ap_write(),
		ap_rmap(),
		ap_wmap(),
		ap_setcursor(),
		ap_cursor(),
		ap_help();

/* This is the ONLY thing that we normally "export" */
FBIO ap_interface =  {
	0,
	ap_open,		/* device_open		*/
	ap_close,		/* device_close		*/
	ap_clear,		/* device_clear		*/
	ap_read,		/* buffer_read		*/
	ap_write,		/* buffer_write		*/
	ap_rmap,		/* colormap_read	*/
	ap_wmap,		/* colormap_write	*/
	fb_sim_view,		/* set view		*/
	fb_sim_getview,		/* get view		*/
	ap_setcursor,		/* define cursor	*/
	ap_cursor,		/* set cursor		*/
	fb_sim_getcursor,	/* get cursor		*/
	fb_sim_readrect,
	fb_sim_writerect,
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
	fb_null,			/* poll */
	fb_null,			/* flush */
	fb_null,			/* free */
	ap_help,			/* help message		*/
	"Apollo General Primitive Resource",/* device description	*/
	1280,				/* max width		*/
	1024,				/* max height		*/
	"/dev/shortname",		/* short device name	*/
	1280,				/* default/current width  */
	1024,				/* default/current height */
	-1,				/* select fd		*/
	-1,				/* file descriptor	*/
	1, 1,				/* zoom			*/
	640, 512,			/* window center	*/
	0, 0, 0,			/* cursor		*/
	PIXEL_NULL,			/* page_base		*/
	PIXEL_NULL,			/* page_curp		*/
	PIXEL_NULL,			/* page_endp		*/
	-1,				/* page_no		*/
	0,				/* page_dirty		*/
	0L,				/* page_curpos		*/
	0L,				/* page_pixels		*/
	0				/* debug		*/
};

gpr_$bitmap_desc_t	display_bm;
static char window_id	'1';
gpr_$event_t		event_type;
gpr_$keyset_t		key_set;
char			event_data;
gpr_$position_t		position;
char			unobs;
char			delete_display=false;

HIDDEN void
ap_storepixel( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	{	register RGBpixel *memp;
	for( 	memp = (RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, p++, count--)
		{
			COPYRGB(*memp, *p);
		}
	return(count);
}

HIDDEN int
ap_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
    static gpr_$display_mode_t	mode = gpr_$direct;
    ios_$id_t			graphics_strid;
    pad_$window_desc_t		window;
    gpr_$offset_t		display_bm_size;
    static gpr_$rgb_plane_t	hi_plane = 1;
    status_$t status;

    FB_CK_FBIO(ifp);

/*  Create a window  */

    window.top = 0;
    window.left = 0;
    window.width = width;
    window.height = height;
    pad_$create_window("",0,pad_$transcript,1,window,graphics_strid,status);

/*  Size and initialize the window  */

    display_bm_size.x_size = width;
    display_bm_size.y_size = height;
    gpr_$init(mode,graphics_strid,display_bm_size,hi_plane,display_bm,status);
    gpr_$set_window_id(window_id,status);
    gpr_$enable_input(gpr_$entered_window,key_set,status);
    gpr_$set_auto_close(grpahics_strid,1,true,status);
    gpr_$set_obscured_opt(gpr_$pop_if_obs,status);
    gpr_$set_auto_refresh(true,status);
    return(0);
}

HIDDEN int
ap_close( ifp )
FBIO	*ifp;
{
    status_$t status;
    gpr_$terminate(false,status);
    return(0);
}

HIDDEN int
ap_clear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
    status_$t status;

/*  Clear to the background color  */

    gpr_$clear(-2,status);
    return(0);
}

HIDDEN int
ap_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	{	register RGBpixel *memp;
	for( 	memp = (RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, p++, count--)
		{
			COPYRGB(*p, *memp);
		}
	return(count);
}

HIDDEN int
ap_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

HIDDEN int
ap_rmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

HIDDEN int
ap_wmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

HIDDEN int
ap_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	return(0);
}

HIDDEN int
ap_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fb_sim_cursor( ifp, mode, x, y );
	return(0);
}

HIDDEN int
ap_help( ifp )
FBIO	*ifp;
{
	fb_log( "Description: %s\n", ap_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		ap_interface.if_max_width,
		ap_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		ap_interface.if_width,
		ap_interface.if_height );
	return(0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
