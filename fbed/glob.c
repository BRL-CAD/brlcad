/*
	SCCS id:	@(#) glob.c	2.1
	Modified: 	12/9/86 at 15:55:29
	Retrieved: 	12/26/86 at 21:54:25
	SCCS archive:	/vld/moss/src/fbed/s.glob.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) glob.c 2.1, modified 12/9/86 at 15:55:29, archive /vld/moss/src/fbed/s.glob.c";
#endif

#include <stdio.h>
#include "./extern.h"

ColorMap	cmap;
RGBpixel	*menu_addr;
RGBpixel	paint;
Menu		pick_one;
Menu		pallet;
Point		cursor_pos;	/* Current location in image space.	*/
Try		*try_rootp = (Try *) NULL;
char		cread_buf[MACROBUFSZ] = { 0 }, *cptr = cread_buf;
char		macro_buf[MACROBUFSZ] = { 0 }, *macro_ptr = macro_buf;
int		brush_sz = 0;
int		gain = 1;
int		fudge_flag = true;
int		menu_flag = true;
int		remembering = false;
int		report_status = true;
int		tty = true;
int		tty_fd = 0;	/* Keyboard file descriptor.		*/
