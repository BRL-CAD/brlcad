/*
*			I F _ A P . C
*
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
*/

#include <time.h>
#include "base.h"
#include "gpr.h"
#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	ap_dopen(),
		ap_dclose(),
		ap_dreset(),
		ap_dclear(),
		ap_bread(),
		ap_bwrite(),
		ap_cmread(),
		ap_cmwrite(),
		ap_viewport_set(),
		ap_window_set(),
		ap_zoom_set(),
		ap_curs_set(),
		ap_cmemory_addr(),
		ap_cscreen_addr(),
		ap_help();

/* This is the ONLY thing that we normally "export" */
FBIO ap_interface =  {
	ap_dopen,		/* device_open		*/
	ap_dclose,		/* device_close		*/
	ap_dreset,		/* device_reset		*/
	ap_dclear,		/* device_clear		*/
	ap_bread,		/* buffer_read		*/
	ap_bwrite,		/* buffer_write		*/
	ap_cmread,		/* colormap_read	*/
	ap_cmwrite,		/* colormap_write	*/
	ap_viewport_set,		/* viewport_set		*/
	ap_window_set,		/* window_set		*/
	ap_zoom_set,		/* zoom_set		*/
	ap_curs_set,		/* curs_set		*/
	ap_cmemory_addr,		/* cursor_move_memory_addr */
	ap_cscreen_addr,		/* cursor_move_screen_addr */
	ap_help,			/* help message		*/
	"Apollo General Primitive Resource",/* device description	*/
	1280,				/* max width		*/
	1024,				/* max height		*/
	"/dev/shortname",		/* short device name	*/
	1280,				/* default/current width  */
	1024,				/* default/current height */
	-1,				/* file descriptor	*/
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

_LOCAL_ void
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

_LOCAL_ int
ap_dopen( ifp, file, width, height )
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

_LOCAL_ int
ap_dclose( ifp )
FBIO	*ifp;
{
    status_$t status;
    gpr_$terminate(false,status);
    return(0);
}

_LOCAL_ int
ap_dreset( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
ap_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
    status_$t status;

/*  Clear to the background color  */

    gpr_$clear(-2,status);
    return(0);
}

_LOCAL_ int
ap_bread( ifp, x, y, pixelp, count )
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

_LOCAL_ int
ap_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

_LOCAL_ int
ap_cmread( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
ap_cmwrite( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
ap_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	return(0);
}

_LOCAL_ int
ap_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);
}

_LOCAL_ int
ap_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);
}

_LOCAL_ int
ap_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	return(0);
}

_LOCAL_ int
ap_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	return(0);
}

_LOCAL_ int
ap_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	return(0);
}

_LOCAL_ int
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
