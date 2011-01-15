/*                         I F _ T K . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup if */
/** @{ */
/** @file if_tk.c
 *
 * Tk libfb interface.
 *
 */
/** @} */

#include "common.h"

#ifdef IF_TK

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#include <tcl.h>
#include <tk.h>

#include "fb.h"


Tcl_Interp *fbinterp;
Tk_Window fbwin;
Tk_PhotoHandle fbphoto;
int p[2] = {0, 0};

/* Note that Tk_PhotoPutBlock claims to have a faster
 * copy method when pixelSize is 4 and alphaOffset is
 * 3 - perhaps output could be massaged to generate this
 * type of information and speed up the process?
 *
 * Might as well use one block and set the three things
 * that actually change in tk_write
 */   
Tk_PhotoImageBlock block = {
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


char *tkwrite_buffer;


HIDDEN int fb_tk_open(FBIO *ifp, char *file, int width, int height),
    fb_tk_close(FBIO *ifp),
    tk_clear(FBIO *ifp, unsigned char *pp),
    tk_read(FBIO *ifp, int x, int y, unsigned char *pixelp, size_t count),
    tk_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, size_t count),
    tk_rmap(FBIO *ifp, ColorMap *cmp),
    tk_wmap(FBIO *ifp, const ColorMap *cmp),
    tk_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
    tk_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
    tk_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
    tk_cursor(FBIO *ifp, int mode, int x, int y),
    tk_getcursor(FBIO *ifp, int *mode, int *x, int *y),
    tk_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
    tk_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
    tk_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
    tk_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
    tk_poll(FBIO *ifp),
    tk_flush(FBIO *ifp),
    tk_free(FBIO *ifp),
    tk_help(FBIO *ifp);

/* This is the ONLY thing that we "export" */
FBIO tk_interface = {
    0,
    fb_tk_open,
    fb_tk_close,
    tk_clear,
    tk_read,
    tk_write,
    tk_rmap,
    tk_wmap,
    tk_view,
    tk_getview,
    tk_setcursor,
    tk_cursor,
    tk_getcursor,
    tk_readrect,
    tk_writerect,
    tk_bwreadrect,
    tk_bwwriterect,
    tk_poll,
    tk_flush,
    tk_free,
    tk_help,
    "Debugging Interface",
    32*1024,		/* max width */
    32*1024,		/* max height */
    "/dev/tk",
    512,			/* current/default width */
    512,			/* current/default height */
    -1,			/* select fd */
    -1,			/* file descriptor */
    1, 1,			/* zoom */
    256, 256,		/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,		/* page_base */
    PIXEL_NULL,		/* page_curp */
    PIXEL_NULL,		/* page_endp */
    -1,			/* page_no */
    0,			/* page_ref */
    0L,			/* page_curpos */
    0L,			/* page_pixels */
    0,			/* debug */
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};


HIDDEN int
fb_tk_open(FBIO *ifp, char *file, int width, int height)
{
    int pid = -1;
    const char *cmd = "package require Tk";
    char image_create_cmd[255] = {'\0'};
    char canvas_create_cmd[255] = {'\0'};
    char reportcolorcmd[255] = {'\0'};
    const char canvas_pack_cmd[255] =
	"pack .fb_tk_canvas -fill both -expand true";
    const char place_image_cmd[255] =
	".fb_tk_canvas create image 0 0 -image fb_tk_photo -anchor nw";
    const char *wmclosecmd = "wm protocol . WM_DELETE_WINDOW {set CloseWindow \"close\"}";
    const char *bindclosecmd = "bind . <Button-3> {set CloseWindow \"close\"}";

    char *buffer;
    char *linebuffer;

    FB_CK_FBIO(ifp);
    if (file == (char *)NULL)
	fb_log("fb_open(0x%lx, NULL, %d, %d)\n",
	       (unsigned long)ifp, width, height);
    else
	fb_log("fb_open(0x%lx, \"%s\", %d, %d)\n",
	       (unsigned long)ifp, file, width, height);

    /* check for default size */
    if (width <= 0)
	width = ifp->if_width;
    if (height <= 0)
	height = ifp->if_height;

    /* set debug bit vector */
    if (file != NULL) {
	char *cp;
	for (cp = file; *cp != '\0' && !isdigit(*cp); cp++)
	    ;
	sscanf(cp, "%d", &ifp->if_debug);
    } else {
	ifp->if_debug = 0;
    }

    /* Give the user whatever width was asked for */
    ifp->if_width = width;
    ifp->if_height = height;

    fbinterp = Tcl_CreateInterp();

    if (Tcl_Init(fbinterp) == TCL_ERROR) {
	fb_log("Tcl_Init returned error in fb_open.");
    }

    if (Tcl_Eval(fbinterp, cmd) != TCL_OK) {
	fb_log("Error returned attempting to start tk in fb_open.");
    }

    fbwin = Tk_MainWindow(fbinterp);

    Tk_GeometryRequest(fbwin, width, height);

    Tk_MakeWindowExist(fbwin);

    sprintf(image_create_cmd,
	    "image create photo fb_tk_photo -height %d -width %d",
	    width, height);

    if (Tcl_Eval(fbinterp, image_create_cmd) != TCL_OK) {
	fb_log("Error returned attempting to create image in fb_open.");
    }

    if ((fbphoto = Tk_FindPhoto(fbinterp, "fb_tk_photo")) == NULL) {
	fb_log("Image creation unsuccessful in fb_open.");
    }

    sprintf(canvas_create_cmd,
	    "canvas .fb_tk_canvas -highlightthickness 0 -height %d -width %d", width, height);

    sprintf (reportcolorcmd,
	     "bind . <Button-2> {puts \"At image (%%x, [expr %d - %%y]), real RGB = ([fb_tk_photo get %%x %%y])\n\"}", height);
    
    if (Tcl_Eval(fbinterp, canvas_create_cmd) != TCL_OK) {
	fb_log("Error returned attempting to create canvas in fb_open.");
    }

    if (Tcl_Eval(fbinterp, canvas_pack_cmd) != TCL_OK) {
	fb_log("Error returned attempting to pack canvas in fb_open. %s",
	       Tcl_GetStringResult(fbinterp));
    }

    if (Tcl_Eval(fbinterp, place_image_cmd) != TCL_OK) {
	fb_log("Error returned attempting to place image in fb_open. %s",
	       Tcl_GetStringResult(fbinterp));
    }

    /* Set our Tcl variable pertaining to whether a
     * window closing event has been seen from the
     * Window manager.  WM_DELETE_WINDOW will be
     * bound to a command setting this variable to
     * the string "close", and a vwait watching
     * for a change to the CloseWindow variable ensures
     * a "lingering" tk window.
     */
    Tcl_SetVar(fbinterp, "CloseWindow", "open", 0);
    if (Tcl_Eval(fbinterp, wmclosecmd) != TCL_OK) {
	fb_log("Error binding WM_DELETE_WINDOW.");
    }
    if (Tcl_Eval(fbinterp, bindclosecmd) != TCL_OK) {
	fb_log("Error binding right mouse button.");
    }
    if (Tcl_Eval(fbinterp, reportcolorcmd) != TCL_OK) {
	fb_log("Error binding middle mouse button.");
    }
 
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));

    /* FIXME: malloc() is necessary here because there are callers
     * that acquire a BU_SYM_SYSCALL semaphore for libfb calls.
     * this should be investigated more closely to see if the
     * semaphore acquires are critical or if they can be pushed
     * down into libfb proper.  in the meantime, manually call
     * malloc()/free().
     */
    buffer = (char *)malloc(sizeof(uint32_t)*3+ifp->if_width*3);
    linebuffer = (char *)malloc(ifp->if_width*3);
    tkwrite_buffer = (char *)malloc(ifp->if_width*3);

    if (pipe(p) == -1) {
	perror("pipe failed");
    }

    pid = fork();
    if (pid < 0) {
	printf("boo, something bad\n");
    } else if (pid > 0) {
	int line = 0;
	uint32_t lines[3];
	int i;
	int y[2];
	y[0] = 0;

	/* parent */
	while (y[0] >= 0) {
	    int count;
	    
	    /* If the Tk window gets a close event, bail */
	    if (BU_STR_EQUAL(Tcl_GetVar(fbinterp, "CloseWindow", 0), "close")) {
		free(buffer);
		free(linebuffer);
		free(tkwrite_buffer);
		fclose(stdin);
		printf("Close Window event\n");
    		Tcl_Eval(fbinterp, "destroy .");
    	    	bu_exit(0, NULL);
	    }

	    /* Unpack inputs from pipe */
	    count = read(p[0], buffer, sizeof(uint32_t)*3+ifp->if_width*3);
	    memcpy(lines, buffer, sizeof(uint32_t)*3);
	    memcpy(linebuffer, buffer+sizeof(uint32_t)*3, ifp->if_width*3);
	    y[0] = ntohl(lines[0]);
	    count = ntohl(lines[1]);

	    if (y[0] < 0) {
		break;
	    }

	    line++;
	    block.pixelPtr = (unsigned char *)linebuffer;
	    block.width = count;
	    block.pitch = 3 * ifp->if_width;

#if defined(TK_MAJOR_VERSION) && TK_MAJOR_VERSION >= 8 && defined(TK_MINOR_VERSION) && TK_MINOR_VERSION >= 5
	    Tk_PhotoPutBlock(fbinterp, fbphoto, &block, 0, ifp->if_height-y[0], count, 1, TK_PHOTO_COMPOSITE_SET);
#else
	    Tk_PhotoPutBlock(fbinterp, &block, 0, ifp->if_height-y[0], count, 1, TK_PHOTO_COMPOSITE_SET);
#endif
		    
	    do {
		i = Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT);
	    } while (i);
	}
	/* very bad things will happen if the parent does not terminate here */
	free(buffer);
	free(linebuffer);
	free(tkwrite_buffer);
	fclose(stdin);
	Tcl_Eval(fbinterp, "vwait CloseWindow");
	if (BU_STR_EQUAL(Tcl_GetVar(fbinterp, "CloseWindow", 0), "close")) {
	    printf("Close Window event\n");
	    Tcl_Eval(fbinterp, "destroy .");
	}
	bu_exit(0, NULL);
    } else {
	/* child */
	printf("IMA CHILD\n");
	fflush(stdout);
    }

    return 0;
}


HIDDEN int
fb_tk_close(FBIO *ifp)
{
    int y[2];
    int ret;
    y[0] = -1;
    y[1] = 0;
    printf("Entering fb_tk_close\n");
    FB_CK_FBIO(ifp);
    ret = write(p[1], y, sizeof(y));
    close(p[1]);
    printf("Sent write (ret=%d) from fb_tk_close\n", ret);
    return 0;
}


HIDDEN int
tk_clear(FBIO *ifp, unsigned char *pp)
{
    FB_CK_FBIO(ifp);
    if (pp == 0)
	fb_log("fb_clear(0x%lx, NULL)\n", (unsigned long)ifp);
    else
	fb_log("fb_clear(0x%lx, &[%d %d %d])\n",
	       (unsigned long)ifp,
	       (int)(pp[RED]), (int)(pp[GRN]),
	       (int)(pp[BLU]));
    return 0;
}


HIDDEN int
tk_read(FBIO *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_read(0x%lx, %4d, %4d, 0x%lx, %ld)\n",
	   (unsigned long)ifp, x, y,
	   (unsigned long)pixelp, (long)count);
    return (int)count;
}


HIDDEN int
tk_write(FBIO *ifp, int UNUSED(x), int y, const unsigned char *pixelp, size_t count)
{
    uint32_t line[3];

    FB_CK_FBIO(ifp);
    /* Set local values of Tk_PhotoImageBlock */
    block.pixelPtr = (unsigned char *)pixelp;
    block.width = count;
    block.pitch = 3 * ifp->if_width;

    /* Pack values to be sent to parent */
    line[0] = htonl(y);
    line[1] = htonl((long)count);
    line[2] = 0;
    
    memcpy(tkwrite_buffer, line, sizeof(uint32_t)*3);
    memcpy(tkwrite_buffer+sizeof(uint32_t)*3, block.pixelPtr, 3 * ifp->if_width);
   
    /* Send values and data to parent for display */ 
    if (write(p[1], tkwrite_buffer, 3 * ifp->if_width + 3*sizeof(uint32_t)) == -1) {
	perror("Unable to write to pipe");
	sleep(1);
    }

    return (int)count;
}


HIDDEN int
tk_rmap(FBIO *ifp, ColorMap *cmp)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_rmap(0x%lx, 0x%lx)\n",
	   (unsigned long)ifp, (unsigned long)cmp);
    return 0;
}


HIDDEN int
tk_wmap(FBIO *ifp, const ColorMap *cmp)
{
    int i;

    FB_CK_FBIO(ifp);
    if (cmp == NULL)
	fb_log("fb_wmap(0x%lx, NULL)\n",
	       (unsigned long)ifp);
    else
	fb_log("fb_wmap(0x%lx, 0x%lx)\n",
	       (unsigned long)ifp, (unsigned long)cmp);

    if (ifp->if_debug & FB_DEBUG_CMAP && cmp != NULL) {
	for (i = 0; i < 256; i++) {
	    fb_log("%3d: [ 0x%4lx, 0x%4lx, 0x%4lx ]\n",
		   i,
		   (unsigned long)cmp->cm_red[i],
		   (unsigned long)cmp->cm_green[i],
		   (unsigned long)cmp->cm_blue[i]);
	}
    }

    return 0;
}


HIDDEN int
tk_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_view(0x%lx, %4d, %4d, %4d, %4d)\n",
	   (unsigned long)ifp, xcenter, ycenter, xzoom, yzoom);
    fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);
    return 0;
}


HIDDEN int
tk_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_getview(0x%lx, 0x%x, 0x%x, 0x%x, 0x%x)\n",
	   (unsigned long)ifp, xcenter, ycenter, xzoom, yzoom);
    fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);
    fb_log(" <= %d %d %d %d\n",
	   *xcenter, *ycenter, *xzoom, *yzoom);
    return 0;
}


HIDDEN int
tk_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_setcursor(0x%lx, 0x%lx, %d, %d, %d, %d)\n",
	   (unsigned long)ifp, bits, xbits, ybits, xorig, yorig);
    return 0;
}


HIDDEN int
tk_cursor(FBIO *ifp, int mode, int x, int y)
{
    fb_log("fb_cursor(0x%lx, %d, %4d, %4d)\n",
	   (unsigned long)ifp, mode, x, y);
    fb_sim_cursor(ifp, mode, x, y);
    return 0;
}


HIDDEN int
tk_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_getcursor(0x%lx, 0x%x, 0x%x, 0x%x)\n",
	   (unsigned long)ifp, mode, x, y);
    fb_sim_getcursor(ifp, mode, x, y);
    fb_log(" <= %d %d %d\n", *mode, *x, *y);
    return 0;
}


HIDDEN int
tk_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_readrect(0x%lx, (%4d, %4d), %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_writerect(0x%lx, %4d, %4d, %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_bwreadrect(0x%lx, (%4d, %4d), %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_bwwriterect(0x%lx, %4d, %4d, %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_poll(FBIO *ifp)
{
    FB_CK_FBIO(ifp);
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
    fb_log("fb_poll(0x%lx)\n", (unsigned long)ifp);
    return 0;
}


HIDDEN int
tk_flush(FBIO *ifp)
{
    FB_CK_FBIO(ifp);
    fb_log("if_flush(0x%lx)\n", (unsigned long)ifp);
    return 0;
}


HIDDEN int
tk_free(FBIO *ifp)
{
    FB_CK_FBIO(ifp);
    fb_log("fb_free(0x%lx)\n", (unsigned long)ifp);
    return 0;
}


/*ARGSUSED*/
HIDDEN int
tk_help(FBIO *ifp)
{
    FB_CK_FBIO(ifp);
    fb_log("Description: %s\n", tk_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   tk_interface.if_max_width,
	   tk_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   tk_interface.if_width,
	   tk_interface.if_height);
    fb_log("\
Usage: /dev/tk[#]\n\
  where # is a optional bit vector from:\n\
    1 debug buffered I/O calls\n\
    2 show colormap entries in rmap/wmap calls\n\
    4 show actual pixel values in read/write calls\n");
    /*8 buffered read/write values - ifdef'd out*/

    return 0;
}


#else

/* quell empty-compilation unit warnings */
static const int unused = 0;

#endif /* IF_TK */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
