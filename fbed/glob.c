/*
	SCCS id:	%Z% %M%	%I%
	Last edit: 	%G% at %U%
	Retrieved: 	%H% at %T%
	SCCS archive:	%P%

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#if ! defined( lint )
static
char	sccsTag[] = "%Z% %M%	%I%	last edit %G% at %U%";
#endif
#include <stdio.h>
#include <std.h>
#include <fb.h>
#include "popup.h"
#include "extern.h"
ColorMap	cmap;
Pixel		*menu_addr;
Pixel		paint;
Menu		pick_one;
Menu		pallet;
Point		cursor_pos;	/* Current location in image space.	*/
Try		*try_rootp = (Try *) NULL;
char		cread_buf[BUFSIZ], *cptr = cread_buf;
char		macro_buf[BUFSIZ], *macro_ptr = macro_buf;
int		brush_sz = 0;
int		gain = 1;
int		fudge_flag = true;
int		menu_flag = true;
int		remembering = false;
int		report_status = true;
int		tty = true;
int		tty_fd = 0;	/* Keyboard file descriptor.		*/
