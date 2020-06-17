/*                         I F _ T K . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2020 United States Government as represented by
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
/** @addtogroup libfb */
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
#include <ctype.h>
#include <string.h>

#include <tcl.h>
#include <tk.h>


#include "bio.h"
#include "bnetwork.h"

#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/snooze.h"

#include "fb_private.h"
#include "fb.h"

#define TKINFO(ptr) ((struct tk_info *)((ptr)->u6.p))
#define TKINFOL(ptr) (ptr)->u6.p

struct tk_info {
    Tcl_Interp *fbinterp;
    Tk_Window fbwin;

    // The rendering memory used to actually assemble the framebuffer contents.
    long fb_buff_size;
    unsigned char *fbpixel;

    Tk_PhotoHandle fbphoto;
    int draw;

    Tcl_ThreadId parent_id;
    Tcl_ThreadId fb_id;
    int ready;
    int shutdown;
};

TCL_DECLARE_MUTEX(drawMutex)

TCL_DECLARE_MUTEX(fbthreadMutex)
// Event container passed to routines triggered by events.
struct FbEvent {
    Tcl_Event event;            /* Must be first */
    fb *ifp;
};

// Even for events where we don't intend to actually run a proc,
// we need to tell Tcl it successfully processed them.  For that
// we define a no-op callback proc.
static int
noop_proc(Tcl_Event *UNUSED(evPtr), int UNUSED(mask))
{
    // Return one to signify a successful completion of the process execution
    return 1;
}

static void
ImageUpdate(ClientData clientData)
{
    fb *ifp = (fb *)clientData;
    struct tk_info *tki = TKINFO(ifp);

    Tcl_MutexLock(&fbthreadMutex);
    tki->draw = 0;
    Tcl_MutexUnlock(&fbthreadMutex);

    Tcl_MutexLock(&drawMutex);
    Tk_PhotoImageBlock dm_data;
    Tk_PhotoGetImage(tki->fbphoto, &dm_data);
    dm_data.pixelPtr = tki->fbpixel;
    Tk_PhotoPutBlock(tki->fbinterp, tki->fbphoto, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);
    Tcl_MutexUnlock(&drawMutex);
}

static Tcl_ThreadCreateType
fb_tk_run(ClientData clientData)
{
    fb *ifp = (fb *)clientData;
    struct tk_info *tki = TKINFO(ifp);

    tki->fbinterp = Tcl_CreateInterp();

    if (Tcl_Init(tki->fbinterp) == TCL_ERROR) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    if (Tcl_Eval(tki->fbinterp, "package require Tk") != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    tki->fbwin = Tk_MainWindow(tki->fbinterp);

    Tk_GeometryRequest(tki->fbwin, ifp->if_width, ifp->if_height);

    Tk_MakeWindowExist(tki->fbwin);

    if (Tcl_Eval(tki->fbinterp, "wm resizable . 0 0") != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    if (Tcl_Eval(tki->fbinterp, "wm title . \"Frame buffer\"") != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    char frame_create_cmd[255] = {'\0'};
    sprintf(frame_create_cmd, "pack [frame .fb -borderwidth 0 -highlightthickness 0 -height %d -width %d]", ifp->if_width, ifp->if_height);
    if (Tcl_Eval(tki->fbinterp, frame_create_cmd) != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    char canvas_create_cmd[255] = {'\0'};
    sprintf(canvas_create_cmd, "pack [canvas .fb.canvas -borderwidth 0 -highlightthickness 0 -insertborderwidth 0 -selectborderwidth 0 -height %d -width %d]", ifp->if_width, ifp->if_height);
    if (Tcl_Eval(tki->fbinterp, canvas_create_cmd) != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    //const char canvas_pack_cmd[255] = "pack .fb_tk_canvas -fill both -expand true";
    char image_create_cmd[255] = {'\0'};
    sprintf(image_create_cmd, "image create photo .fb.canvas.photo -height %d -width %d", ifp->if_width, ifp->if_height);
    if (Tcl_Eval(tki->fbinterp, image_create_cmd) != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    if ((tki->fbphoto = Tk_FindPhoto(tki->fbinterp, ".fb.canvas.photo")) == NULL) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    const char place_image_cmd[255] = ".fb.canvas create image 0 0 -image .fb.canvas.photo -anchor nw";
    if (Tcl_Eval(tki->fbinterp, place_image_cmd) != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    char reportcolorcmd[255] = {'\0'};
    sprintf (reportcolorcmd, "bind . <Button-2> {puts \"At image (%%x, [expr %d - %%y]), real RGB = ([.fb.canvas.photo get %%x %%y])\n\"}", ifp->if_height);

    /* Set our Tcl variable pertaining to whether a
     * window closing event has been seen from the
     * Window manager.  WM_DELETE_WINDOW will be
     * bound to a command setting this variable to
     * the string "close", and a vwait watching
     * for a change to the CloseWindow variable ensures
     * a "lingering" tk window.
     */
    Tcl_SetVar(tki->fbinterp, "CloseWindow", "open", 0);

    const char *wmclosecmd = "wm protocol . WM_DELETE_WINDOW {set CloseWindow \"close\"}";
    if (Tcl_Eval(tki->fbinterp, wmclosecmd) != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    const char *bindclosecmd = "bind . <Button-3> {set CloseWindow \"close\"}";
    if (Tcl_Eval(tki->fbinterp, bindclosecmd) != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }
    if (Tcl_Eval(tki->fbinterp, reportcolorcmd) != TCL_OK) {
	Tcl_ExitThread(TCL_OK);
	TCL_THREAD_CREATE_RETURN;
    }

    // Clear out any events up to this point
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));

    tki->ready = 1;

    while (!tki->shutdown) {

	Tcl_DoOneEvent(0);

	/* If the Tk window gets a close event, wrap up */
	if (BU_STR_EQUAL(Tcl_GetVar(tki->fbinterp, "CloseWindow", 0), "close")) {
	    tki->shutdown = 1;
	    continue;
	}

	if (tki->draw) {
	    Tk_DoWhenIdle(ImageUpdate, (ClientData) ifp);
	}
    }

    Tcl_Eval(tki->fbinterp, "destroy .");

    // Let the parent thread know we're done
    Tcl_MutexLock(&fbthreadMutex);
    struct FbEvent *threadEventPtr = (struct FbEvent *)ckalloc(sizeof(struct FbEvent));
    threadEventPtr->ifp = ifp;
    threadEventPtr->event.proc = noop_proc;
    Tcl_ThreadQueueEvent(tki->parent_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(tki->parent_id);
    Tcl_MutexUnlock(&fbthreadMutex);

    Tcl_DeleteInterp(tki->fbinterp);

    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
}


HIDDEN int
fb_tk_open(fb *ifp, const char *file, int width, int height)
{
    FB_CK_FB(ifp);
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
	const char *cp;
	for (cp = file; *cp != '\0' && !isdigit((unsigned char)*cp); cp++)
	    ;
	sscanf(cp, "%d", &ifp->if_debug);
    } else {
	ifp->if_debug = 0;
    }

    /* Give the user whatever width was asked for */
    ifp->if_width = width;
    ifp->if_height = height;

    /* Set up Tk specific info */
    if ((TKINFOL(ifp) = (char *)calloc(1, sizeof(struct tk_info))) == NULL) {
	fb_log("fb_tk_open:  tk_info malloc failed\n");
	return -1;
    }

    struct tk_info *tki = TKINFO(ifp);
    tki->shutdown = 0;
    tki->ready = 0;
    tki->draw = 0;

    long b_size = ifp->if_width * ifp->if_height * 4;
    tki->fbpixel = (unsigned char *)calloc(b_size+1, sizeof(char));

    Tcl_MutexLock(&fbthreadMutex);
    Tcl_ThreadId fbthreadID;
    if (Tcl_CreateThread(&fbthreadID, fb_tk_run, (ClientData)ifp, TCL_THREAD_STACK_DEFAULT, TCL_THREAD_JOINABLE) != TCL_OK) {
	fb_log("can't create fb thread\n");
	return -1;
    }
    tki->fb_id = fbthreadID;
    Tcl_MutexUnlock(&fbthreadMutex);

#if 0
    // Let the window finish setting up before open is considered done
    while (!tki->ready) {
	Tcl_Sleep(1);
    }
#endif

    return 0;
}

HIDDEN struct fb_platform_specific *
tk_get_fbps(uint32_t UNUSED(magic))
{
        return NULL;
}


HIDDEN void
tk_put_fbps(struct fb_platform_specific *UNUSED(fbps))
{
        return;
}

HIDDEN int
tk_open_existing(fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height), struct fb_platform_specific *UNUSED(fb_p))
{
        return 0;
}

HIDDEN int
tk_close_existing(fb *UNUSED(ifp))
{
        return 0;
}

HIDDEN int
tk_configure_window(fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height))
{
        return 0;
}

HIDDEN int
tk_refresh(fb *UNUSED(ifp), int UNUSED(x), int UNUSED(y), int UNUSED(w), int UNUSED(h))
{
        return 0;
}

HIDDEN int
fb_tk_close(fb *ifp)
{
    FB_CK_FB(ifp);
    struct tk_info *tki = TKINFO(ifp);

    // Make a local interp so we can get events from the window
    Tcl_Interp *cinterp = Tcl_CreateInterp();

    // Let the window know who to tell when it's time to shut down
    tki->parent_id = Tcl_GetCurrentThread();

    while (!tki->shutdown) {
	Tcl_DoOneEvent(0);
	if (tki->shutdown) {
	    int tret;
	    Tcl_DeleteInterp(cinterp);
	    Tcl_JoinThread(tki->fb_id, &tret);
	    if (tret != TCL_OK) {
		printf("shutdown of thread failed\n");
		return -1;
	    }

	    bu_free(tki->fbpixel, "fbpixel");
	    bu_free(tki, "tkinfo");
	    return 0;
	}
    }

    // Just in case shutdown got set before fb_tk_close was ever
    // called, be prepared to clean up outside the loop as well.
    int tret;
    Tcl_DeleteInterp(cinterp);
    Tcl_JoinThread(tki->fb_id, &tret);
    if (tret != TCL_OK) {
	printf("shutdown of thread failed\n");
	return -1;
    }
    bu_free(tki->fbpixel, "fbpixel");
    bu_free(tki, "tkinfo");

    return 0;
}


HIDDEN int
tk_clear(fb *ifp, unsigned char *pp)
{
    FB_CK_FB(ifp);
    if (pp == 0)
	fb_log("fb_clear(0x%lx, NULL)\n", (unsigned long)ifp);
    else
	fb_log("fb_clear(0x%lx, &[%d %d %d])\n",
	       (unsigned long)ifp,
	       (int)(pp[RED]), (int)(pp[GRN]),
	       (int)(pp[BLU]));
    return 0;
}


HIDDEN ssize_t
tk_read(fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    FB_CK_FB(ifp);
    fb_log("fb_read(0x%lx, %4d, %4d, 0x%lx, %ld)\n",
	   (unsigned long)ifp, x, y,
	   (unsigned long)pixelp, (long)count);
    return (ssize_t)count;
}

// Given an X,Y coordinate in a TkPhoto image and a desired offset in
// X and Y, return the index value into the pixelPtr array N such that
// N is the integer value of the R color at that X,Y coordiante, N+1
// is the G value, N+2 is the B value and N+3 is the Alpha value.
// If either desired offset is beyond the width and height boundaries,
// cap the return at the minimum/maximum allowed value.
static int
img_xy_index(int width, int height, int x, int y, int dx, int dy)
{
    int nx = ((x + dx) > width)  ? width  : ((x + dx) < 0) ? 0 : x + dx;
    int ny = ((y + dy) > height) ? height : ((y + dy) < 0) ? 0 : y + dy;
    return (ny * width * 4) + nx * 4;
}

HIDDEN ssize_t
tk_write(fb *ifp, int x, int y, const unsigned char *pixelp, size_t count)
{
    FB_CK_FB(ifp);

    struct tk_info *tki = TKINFO(ifp);

#if 0
    // If the shutdown flag got set and we're still processing, bail
    // (TODO - is this the right thing to do?  We're rendering to a framebuffer
    // that's not there anymore, so if we don't do this the process is arguably
    // a zombie, but maybe it's desirable to keep going in some situations ...)
    if (tki->shutdown) {
	bu_exit(0, "Quitting - framebuffer shutdown flag found set during tk_write");
    }
#endif

    int pindex = img_xy_index(ifp->if_width, ifp->if_height, x, ifp->if_height-y-1, 0, 0);

    // Touching the image buffer - lock drawing mutex
    Tcl_MutexLock(&drawMutex);

    // TkPhoto uses an alpha channel, but at the moment the incoming pix data doesn't.
    // Write pixels accordingly, setting alpha to opaque (for now)
    for (size_t i = 0; i < count; i++) {
	tki->fbpixel[pindex+i*4] = pixelp[i*3];
	tki->fbpixel[pindex+(i*4+1)] = pixelp[i*3+1];
	tki->fbpixel[pindex+(i*4+2)] = pixelp[i*3+2];
	tki->fbpixel[pindex+(i*4+3)] = 255;
    }

    // Lock the drawing thread - we have updated image data, and want to make sure
    // the drawing thread doesn't clear anything in the middle of this block.
    Tcl_MutexLock(&fbthreadMutex);
    tki->draw = 1;

    struct FbEvent *threadEventPtr = (struct FbEvent *)ckalloc(sizeof(struct FbEvent));
    threadEventPtr->ifp = ifp;
    threadEventPtr->event.proc = noop_proc;
    Tcl_ThreadQueueEvent(tki->fb_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(tki->fb_id);

    // Flag set, event queued, drawing done - unlock everything
    Tcl_MutexUnlock(&fbthreadMutex);
    Tcl_MutexUnlock(&drawMutex);

    return (ssize_t)count;
}


HIDDEN int
tk_rmap(fb *ifp, ColorMap *cmp)
{
    FB_CK_FB(ifp);
    fb_log("fb_rmap(0x%lx, 0x%lx)\n",
	   (unsigned long)ifp, (unsigned long)cmp);
    return 0;
}


HIDDEN int
tk_wmap(fb *ifp, const ColorMap *cmp)
{
    int i;

    FB_CK_FB(ifp);
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
tk_view(fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    FB_CK_FB(ifp);
    fb_log("fb_view(%p, %4d, %4d, %4d, %4d)\n",
	   (void *)ifp, xcenter, ycenter, xzoom, yzoom);
    fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);
    return 0;
}


HIDDEN int
tk_getview(fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    FB_CK_FB(ifp);
    fb_log("fb_getview(%p, %p, %p, %p, %p)\n",
	   (void *)ifp, (void *)xcenter, (void *)ycenter, (void *)xzoom, (void *)yzoom);
    fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);
    fb_log(" <= %d %d %d %d\n",
	   *xcenter, *ycenter, *xzoom, *yzoom);
    return 0;
}


HIDDEN int
tk_setcursor(fb *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
    FB_CK_FB(ifp);
    fb_log("fb_setcursor(%p, %p, %d, %d, %d, %d)\n",
	   (void *)ifp, (void *)bits, xbits, ybits, xorig, yorig);
    return 0;
}


HIDDEN int
tk_cursor(fb *ifp, int mode, int x, int y)
{
    fb_log("fb_cursor(0x%lx, %d, %4d, %4d)\n",
	   (unsigned long)ifp, mode, x, y);
    fb_sim_cursor(ifp, mode, x, y);
    return 0;
}


HIDDEN int
tk_getcursor(fb *ifp, int *mode, int *x, int *y)
{
    FB_CK_FB(ifp);
    fb_log("fb_getcursor(%p, %p, %p, %p)\n",
	   (void *)ifp, (void *)mode, (void *)x, (void *)y);
    fb_sim_getcursor(ifp, mode, x, y);
    fb_log(" <= %d %d %d\n", *mode, *x, *y);
    return 0;
}


HIDDEN int
tk_readrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_readrect(0x%lx, (%4d, %4d), %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_writerect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_writerect(0x%lx, %4d, %4d, %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_bwreadrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_bwreadrect(0x%lx, (%4d, %4d), %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_bwwriterect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    FB_CK_FB(ifp);
    fb_log("fb_bwwriterect(0x%lx, %4d, %4d, %4d, %4d, 0x%lx)\n",
	   (unsigned long)ifp, xmin, ymin, width, height,
	   (unsigned long)pp);
    return width*height;
}


HIDDEN int
tk_poll(fb *ifp)
{
    FB_CK_FB(ifp);
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
    fb_log("fb_poll(0x%lx)\n", (unsigned long)ifp);
    return 0;
}


HIDDEN int
tk_flush(fb *ifp)
{
    FB_CK_FB(ifp);
    struct tk_info *tki = TKINFO(ifp);

    Tcl_MutexLock(&drawMutex);
    tki->draw = 1;
    Tcl_MutexLock(&fbthreadMutex);
    struct FbEvent *threadEventPtr = (struct FbEvent *)ckalloc(sizeof(struct FbEvent));
    threadEventPtr->ifp = ifp;
    threadEventPtr->event.proc = noop_proc;
    Tcl_ThreadQueueEvent(tki->fb_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(tki->fb_id);
    Tcl_MutexUnlock(&fbthreadMutex);
    Tcl_MutexUnlock(&drawMutex);

    fb_log("if_flush(0x%lx)\n", (unsigned long)ifp);
    return 0;
}


HIDDEN int
tk_free(fb *ifp)
{
    FB_CK_FB(ifp);
    fb_log("fb_free(0x%lx)\n", (unsigned long)ifp);
    return 0;
}


/*ARGSUSED*/
HIDDEN int
tk_help(fb *ifp)
{
    FB_CK_FB(ifp);
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

/* This is the ONLY thing that we "export" */
fb tk_interface = {
    0,
    FB_TK_MAGIC,
    fb_tk_open,
    tk_open_existing,
    tk_close_existing,
    tk_get_fbps,
    tk_put_fbps,
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
    tk_configure_window,
    tk_refresh,
    tk_poll,
    tk_flush,
    tk_free,
    tk_help,
    "Debugging Interface",
    FB_XMAXSCREEN,	/* max width */
    FB_YMAXSCREEN,	/* max height */
    "/dev/tk",
    512,		/* current/default width */
    512,		/* current/default height */
    -1,			/* select fd */
    -1,			/* file descriptor */
    1, 1,		/* zoom */
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
    0,			/* refresh rate */
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};



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
