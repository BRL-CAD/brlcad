/*                        F B T H R E A D T E S T. C
 * BRL-CAD
 *
 * Copyright (c) 1986-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fbthreadtest.c
 *
 * Throwaway program to test ideas about Tcl/Tk threading and framebuffer
 * display.
 *
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif
#include "bio.h"

#include "bu.h"
#include "fb.h"

#include <tcl.h>
#include <tk.h>

#include "pkg.h"


int skipbytes(int fd, off_t num);

static unsigned char *scanline;		/* 1 scanline pixel buffer */
static int	scanbytes;		/* # of bytes of scanline */
static int	scanpix;		/* # of pixels of scanline */

static int	multiple_lines = 0;	/* Streamlined operation */

static char	*framebuffer = NULL;
static char	*file_name;
static int	infd;

static int	fileinput = 0;		/* file of pipe on input? */
static int	autosize = 0;		/* !0 to autosize input */

static unsigned long int	file_width = 512;	/* default input width */
static unsigned long int	file_height = 512;	/* default input height */
static int	scr_width = 0;		/* screen tracks file if not given */
static int	scr_height = 0;
static int	file_xoff, file_yoff;
static int	scr_xoff, scr_yoff;
static int	clear = 0;
static int	zoom = 0;
static int	inverse = 0;		/* Draw upside-down */
static int	one_line_only = 0;	/* insist on 1-line writes */
static int	pause_sec = 10; /* Pause that many seconds before closing the FB
				   and exiting */

static char usage[] = "\
Usage: pix-fb [-a -h -i -c -z -1] [-m #lines] [-F framebuffer]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [-p seconds]\n\
	[file.pix]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ( (c = bu_getopt( argc, argv, "1m:ahiczF:p:s:w:n:x:y:X:Y:S:W:N:" )) != EOF )  {
	switch ( c )  {
	    case '1':
		one_line_only = 1;
		break;
	    case 'm':
		multiple_lines = atoi(bu_optarg);
		break;
	    case 'a':
		autosize = 1;
		break;
	    case 'h':
		/* high-res */
		file_height = file_width = 1024;
		scr_height = scr_width = 1024;
		autosize = 0;
		break;
	    case 'i':
		inverse = 1;
		break;
	    case 'c':
		clear = 1;
		break;
	    case 'z':
		zoom = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'x':
		file_xoff = atoi(bu_optarg);
		break;
	    case 'y':
		file_yoff = atoi(bu_optarg);
		break;
	    case 'X':
		scr_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		scr_yoff = atoi(bu_optarg);
		break;
	    case 'S':
		scr_height = scr_width = atoi(bu_optarg);
		break;
	    case 'W':
		scr_width = atoi(bu_optarg);
		break;
	    case 'N':
		scr_height = atoi(bu_optarg);
		break;
	    case 'p':
		pause_sec=atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return(0);
	}
    }

    if ( bu_optind >= argc )  {
	if ( isatty(fileno(stdin)) )
	    return(0);
	file_name = "-";
	infd = 0;
    } else {
	file_name = argv[bu_optind];
	if ( (infd = open(file_name, 0)) < 0 )  {
	    perror(file_name);
	    (void)fprintf( stderr,
			   "pix-fb: cannot open \"%s\" for reading\n",
			   file_name );
	    bu_exit(1, NULL);
	}
#ifdef _WIN32
	setmode(infd, O_BINARY);
#endif
	fileinput++;
    }

    if ( argc > ++bu_optind )
	(void)fprintf( stderr, "pix-fb: excess argument(s) ignored\n" );

    return(1);		/* OK */
}

bb_tk_open(FBIO *ifp, char *file, int width, int height)
{
    FB_CK_FBIO(ifp);
    return	0;
}

int
fb_tk_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count)
{
    int	i;

    FB_CK_FBIO(ifp);

    /* Set local values of Tk_PhotoImageBlock */
    return count;
}

int
main(int argc, char **argv)
{
    Tcl_Interp *binterp;
    Tk_Window bwin;
    Tk_PhotoHandle bphoto;

    /* Note that Tk_PhotoPutBlock claims to have a faster
     * copy method when pixelSize is 4 and alphaOffset is
     * 3 - perhaps output could be massaged to generate this
     * type of information and speed up the process?
     *
     * Might as well use one block and set the three things
     * that actually change in tk_write
     */   
    Tk_PhotoImageBlock bblock = {
	NULL, /*Pointer to first pixel*/
	0,    /*Width of block in pixels*/
	1,    /*Height of block in pixels - always one for a scanline*/
	0,    /*Address difference between successive lines*/
	3,    /*Address difference between successive pixels on one scanline*/
	{
	    RED,
	    GRN,
	    BLU,
	    0   /* alpha */
	}
    };



    int y;
    int	xout, yout, n, m, xstart, xskip;

    if ( !get_args( argc, argv ) )  {
	(void)fputs(usage, stderr);
	bu_exit( 1, NULL );
    }

    /* autosize input? */
    if ( fileinput && autosize ) {
	unsigned long int	w, h;
	if ( fb_common_file_size(&w, &h, file_name, 3) ) {
	    file_width = w;
	    file_height = h;
	} else {
	    fprintf(stderr, "pix-fb: unable to autosize\n");
	}
    }

    /* If screen size was not set, track the file size */
    if ( scr_width == 0 )
	scr_width = file_width;
    if ( scr_height == 0 )
	scr_height = file_height;

    FBIO *ifp;
    ifp = (FBIO *) calloc(sizeof(FBIO), 1);
    ifp->if_name = "/dev/tk";
    ifp->if_magic = FB_MAGIC;
    char *fbname = "/dev/tk";


    /* set the screen size */
    scr_width = 512; 
    scr_height = 512;

    binterp = Tcl_CreateInterp();
    const char *cmd = "package require Tk";

    if (Tcl_Init(binterp) == TCL_ERROR) {
	fb_log( "Tcl_Init returned error in fb_open." );
    }

    if (Tcl_Eval(binterp, cmd) != TCL_OK) {
	fb_log( "Error returned attempting to start tk in fb_open." );
    }


    bwin = Tk_MainWindow(binterp);

    Tk_GeometryRequest(bwin, scr_width, scr_height);

    Tk_MakeWindowExist(bwin);

    char image_create_cmd[255];
    sprintf(image_create_cmd,
	    "image create photo b_tk_photo -height %d -width %d",
	    scr_width, scr_height);

    if (Tcl_Eval(binterp, image_create_cmd) != TCL_OK) {
	fb_log( "Error returned attempting to create image in fb_open." );
    }

    if ((bphoto = Tk_FindPhoto(binterp, "b_tk_photo")) == NULL ) {
	fb_log( "Image creation unsuccessful in fb_open." );
    }

    char canvas_create_cmd[255];
    sprintf(canvas_create_cmd,
	    "canvas .b_tk_canvas -height %d -width %d", scr_width, scr_height);

    if (Tcl_Eval(binterp, canvas_create_cmd) != TCL_OK) {
	fb_log( "Error returned attempting to create canvas in fb_open." );
    }

    const char canvas_pack_cmd[255] =
	"pack .b_tk_canvas -fill both -expand true";

    if (Tcl_Eval(binterp, canvas_pack_cmd) != TCL_OK) {
	fb_log( "Error returned attempting to pack canvas in fb_open. %s",
		Tcl_GetStringResult(binterp));
    }

    const char place_image_cmd[255] =
	".b_tk_canvas create image 0 0 -image b_tk_photo -anchor nw";
    if (Tcl_Eval(binterp, place_image_cmd) != TCL_OK) {
	fb_log( "Error returned attempting to place image in fb_open. %s",
		Tcl_GetStringResult(binterp));
    }

    /* Set our Tcl variable pertaining to whether a
     * window closing event has been seen from the
     * Window manager.  WM_DELETE_WINDOW will be
     * bound to a command setting this variable to
     * the string "close", in order to let tk_fb_close
     * detect the Tk event and handle the closure
     * itself.
     */
    Tcl_SetVar(binterp, "CloseWindow", "open", 0);
    const char *wmcmd = "wm protocol . WM_DELETE_WINDOW {set CloseWindow \"close\"}";
    if (Tcl_Eval(binterp, wmcmd) != TCL_OK) {
	fb_log( "Error binding WM_DELETE_WINDOW." );
    }
    
	 
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));


    /* compute number of pixels to be output to screen */
    if ( scr_xoff < 0 )
    {
	xout = scr_width + scr_xoff;
	xskip = (-scr_xoff);
	xstart = 0;
    }
    else
    {
	xout = scr_width - scr_xoff;
	xskip = 0;
	xstart = scr_xoff;
    }

    if ( xout < 0 )
	bu_exit(0, NULL);			/* off screen */
    if ( xout > (file_width-file_xoff) )
	xout = (file_width-file_xoff);
    scanpix = xout;				/* # pixels on scanline */

    if ( inverse )
	scr_yoff = (-scr_yoff);

    yout = scr_height - scr_yoff;
    if ( yout < 0 )
	bu_exit(0, NULL);			/* off screen */
    if ( yout > (file_height-file_yoff) )
	yout = (file_height-file_yoff);

    /* Only in the simplest case use multi-line writes */
    if ( !one_line_only && multiple_lines > 0 && !inverse && !zoom &&
	 xout == file_width &&
	 file_width <= scr_width )  {
	scanpix *= multiple_lines;
    }

    scanbytes = scanpix * sizeof(RGBpixel);
    if ( (scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL )  {
	fprintf(stderr,
		"pix-fb:  malloc(%d) failure for scanline buffer\n",
		scanbytes);
	bu_exit(2, NULL);
    }

    if ( file_yoff != 0 ) skipbytes( infd, (off_t)file_yoff*(off_t)file_width*sizeof(RGBpixel) );

    /* Normal way -- bottom to top */
    for ( y = scr_yoff; y < scr_yoff + yout; y++ )  {
        if ( y < 0 || y > scr_height )
        {
	    skipbytes( infd, (off_t)file_width*sizeof(RGBpixel) );
	    continue;
        }
        if ( file_xoff+xskip != 0 )
	    skipbytes( infd, (off_t)(file_xoff+xskip)*sizeof(RGBpixel) );
        n = bu_mread( infd, (char *)scanline, scanbytes );
        if ( n <= 0 ) break;
        bblock.pixelPtr = (unsigned char *)scanline;
        bblock.width = 512;
        bblock.pitch = 3 * scr_width;
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 5)
	Tk_PhotoPutBlock(binterp, bphoto, &bblock, 0, 512-y, 512, 1, TK_PHOTO_COMPOSITE_SET);
#else
	Tk_PhotoPutBlock(binterp, &bblock, 0, 512-y, 512, 1, TK_PHOTO_COMPOSITE_SET);
#endif
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
        /* slop at the end of the line? */
        if ( file_xoff+xskip+scanpix < file_width )
	    skipbytes( infd, (off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel) );
    }
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));

    Tcl_Eval(binterp, "vwait CloseWindow");
    if (!strcmp(Tcl_GetVar(binterp, "CloseWindow", 0), "close")) {
	Tcl_Eval(binterp, "destroy .");
    }

    bu_exit(0, NULL);
}

/*
 * Throw bytes away.  Use reads into scanline buffer if a pipe, else seek.
 */
int
skipbytes(int fd, off_t num)
{
    int	n, try;

    if ( fileinput ) {
	(void)lseek( fd, (off_t)num, 1 );
	return 0;
    }

    while ( num > 0 ) {
	try = num > scanbytes ? scanbytes : num;
	n = read( fd, scanline, try );
	if ( n <= 0 ) {
	    return -1;
	}
	num -= n;
    }
    return	0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
