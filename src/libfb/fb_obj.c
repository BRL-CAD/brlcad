/*                        F B _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
/** @addtogroup fb */
/** @{ */
/** @file fb_obj.c
 *
 * A framebuffer object contains the attributes and
 * methods for controlling framebuffers.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "bio.h"
#include "tcl.h"
#include "cmd.h"
#include "fb.h"
#include "fbserv_obj.h"


/* defined in libfb/tcl.c */
extern int fb_refresh(FBIO *ifp, int x, int y, int w, int h);


#define FBO_CONSTRAIN(_v, _a, _b)		\
    ((_v > _a) ? (_v < _b ? _v : _b) : _a)

struct fb_obj {
    struct bu_list l;
    struct bu_vls fbo_name;	/* framebuffer object name/cmd */
    struct fbserv_obj fbo_fbs;	/* fbserv object */
    Tcl_Interp *fbo_interp;
};


static struct fb_obj HeadFBObj;			/* head of display manager object list */


HIDDEN int
fbo_coords_ok(FBIO *fbp, int x, int y)
{
    int width;
    int height;
    int errors;
    width = fb_getwidth(fbp);
    height = fb_getheight(fbp);

    errors = 0;

    if (x < 0) {
	bu_log("fbo_coords_ok: Error!: X value < 0\n");
	++errors;
    }

    if (y < 0) {
	bu_log("fbo_coords_ok: Error!: Y value < 0\n");
	++errors;
    }

    if (x > width - 1) {
	bu_log("fbo_coords_ok: Error!: X value too large\n");
	++errors;
    }

    if (y > height - 1) {
	bu_log("fbo_coords_ok: Error!: Y value too large\n");
	++errors;
    }

    if (errors) {
	return 0;
    } else {
	return 1;
    }
}


/*
 * Called when the object is destroyed.
 */
HIDDEN void
fbo_deleteProc(void *clientData)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;

    /* close framebuffer */
    fb_close(fbop->fbo_fbs.fbs_fbp);

    bu_vls_free(&fbop->fbo_name);
    BU_LIST_DEQUEUE(&fbop->l);
    bu_free((genptr_t)fbop, "fbo_deleteProc: fbop");
}


/*
 * Close a framebuffer object.
 *
 * Usage:
 * procname close
 */
HIDDEN int
fbo_close_tcl(void *clientData, int argc, const char **UNUSED(argv))
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;

    if (argc != 2) {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    /* Among other things, this will call dmo_deleteProc. */
    Tcl_DeleteCommand(fbop->fbo_interp, bu_vls_addr(&fbop->fbo_name));

    return BRLCAD_OK;
}


HIDDEN int
fbo_tcllist2color(const char *str, unsigned char *pixel)
{
    int r, g, b;

    if (sscanf(str, "%d %d %d", &r, &g, &b) != 3) {
	bu_log("fb_clear: bad color spec - %s", str);
	return BRLCAD_ERROR;
    }

    pixel[RED] = FBO_CONSTRAIN (r, 0, 255);
    pixel[GRN] = FBO_CONSTRAIN (g, 0, 255);
    pixel[BLU] = FBO_CONSTRAIN (b, 0, 255);

    return BRLCAD_OK;
}


/*
 * Clear the framebuffer with the specified color.
 * Otherwise, clear the framebuffer with black.
 *
 * Usage:
 * procname clear [rgb]
 */
HIDDEN int
fbo_clear_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int status;
    RGBpixel pixel;
    unsigned char *ms;


    if (argc < 2 || 3 < argc) {
	bu_log("ERROR: expecting only two or three arguments\n");
	return BRLCAD_ERROR;
    }

    if (argc == 3) {
	/*
	 * Decompose the color list into its constituents.
	 * For now must be in the form of rrr ggg bbb.
	 */
	if (fbo_tcllist2color(argv[6], pixel) == BRLCAD_ERROR) {
	    bu_log("fb_cell: invalid color spec: %s.", argv[6]);
	    return BRLCAD_ERROR;
	}

	ms = pixel;
    } else
	ms = RGBPIXEL_NULL;

    status = fb_clear(fbop->fbo_fbs.fbs_fbp, ms);

    if (status < 0)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname cursor mode x y
 */
HIDDEN int
fbo_cursor_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int mode;
    int x, y;
    int status;

    if (argc != 5) {
	bu_log("ERROR: expecting five arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &mode) != 1) {
	bu_log("fb_cursor: bad mode - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &x) != 1) {
	bu_log("fb_cursor: bad x value - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%d", &y) != 1) {
	bu_log("fb_cursor: bad y value - ", argv[4]);
	return BRLCAD_ERROR;
    }

    status = fb_cursor(fbop->fbo_fbs.fbs_fbp, mode, x, y);
    if (status == 0)
	return BRLCAD_OK;

    return BRLCAD_ERROR;
}


/*
 *
 * Usage:
 * procname getcursor
 */
HIDDEN int
fbo_getcursor_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int status;
    int mode;
    int x, y;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| strcasecmp(argv[1], "getcursor") != 0)
    {
	bu_log("ERROR: unexpected argument(s)\n");
	return BRLCAD_ERROR;
    }

    status = fb_getcursor(fbop->fbo_fbs.fbs_fbp, &mode, &x, &y);
    if (status == 0) {
	bu_vls_printf(&vls, "%d %d %d", mode, x, y);
	Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}


/*
 * Refresh the entire framebuffer or that part specified by
 * a rectangle (i.e. x y width height)
 * Usage:
 * procname refresh [rect]
 */
HIDDEN int
fbo_refresh_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int x, y, w, h;		       /* rectangle to be refreshed */

    if (argc < 2 || 3 < argc) {
	bu_log("ERROR: expecting only two or three arguments\n");
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	/* refresh the whole display */
	x = y = 0;
	w = fbop->fbo_fbs.fbs_fbp->if_width;
	h = fbop->fbo_fbs.fbs_fbp->if_height;
    } else if (sscanf(argv[2], "%d %d %d %d", &x, &y, &w, &h) != 4) {
	/* refresh rectanglar area */
	bu_log("fb_refresh: bad rectangle - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    return fb_refresh(fbop->fbo_fbs.fbs_fbp, x, y, w, h);
}


/*
 * Listen for framebuffer clients.
 *
 * Usage:
 * procname listen port
 *
 * Returns the port number actually used.
 *
 */
HIDDEN int
fbo_listen_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (fbop->fbo_fbs.fbs_fbp == FBIO_NULL) {
	bu_log("%s listen: framebuffer not open!\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc != 2 && argc != 3) {
	bu_log("ERROR: expecting only two or three arguments\n");
	return BRLCAD_ERROR;
    }

    if (argc == 3) {
	int port;

	if (sscanf(argv[2], "%d", &port) != 1) {
	    bu_log("listen: bad value - %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (port >= 0)
	    fbs_open(&fbop->fbo_fbs, port);
	else {
	    fbs_close(&fbop->fbo_fbs);
	}
	bu_vls_printf(&vls, "%d", fbop->fbo_fbs.fbs_listener.fbsl_port);
	Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_OK;
    }

    /* return the port number */
    /* argc == 2 */
    bu_vls_printf(&vls, "%d", fbop->fbo_fbs.fbs_listener.fbsl_port);
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 * Set/get the pixel value at position (x, y).
 *
 * Usage:
 * procname pixel x y [rgb]
 */
HIDDEN int
fbo_pixel_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int x, y; 	/* pixel position */
    RGBpixel pixel;

    if (argc != 4 && argc != 5) {
	bu_log("ERROR: expecting five arguments\n");
	return BRLCAD_ERROR;
    }

    /* get pixel position */
    if (sscanf(argv[2], "%d", &x) != 1) {
	bu_log("fb_pixel: bad x value - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &y) != 1) {
	bu_log("fb_pixel: bad y value - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    /* check pixel position */
    if (!fbo_coords_ok(fbop->fbo_fbs.fbs_fbp, x, y)) {
	bu_log("fb_pixel: coordinates (%s, %s) are invalid.", argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    /* get pixel value */
    if (argc == 4) {
	fb_rpixel(fbop->fbo_fbs.fbs_fbp, pixel);
	bu_vls_printf(&vls, "%d %d %d", pixel[RED], pixel[GRN], pixel[BLU]);
	Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_OK;
    }

    /*
     * Decompose the color list into its constituents.
     * For now must be in the form of rrr ggg bbb.
     */

    /* set pixel value */
    if (fbo_tcllist2color(argv[4], pixel) == BRLCAD_ERROR) {
	bu_log("fb_pixel: invalid color spec - %s", argv[4]);
	return BRLCAD_ERROR;
    }

    fb_write(fbop->fbo_fbs.fbs_fbp, x, y, pixel, 1);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname cell xmin ymin width height color
 */
HIDDEN int
fbo_cell_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int xmin, ymin;
    long width;
    long height;
    size_t i;
    RGBpixel pixel;
    unsigned char *pp;


    if (argc != 7) {
	bu_log("ERROR: expecting seven arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &xmin) != 1) {
	bu_log("fb_cell: bad xmin value - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &ymin) != 1) {
	bu_log("fb_cell: bad ymin value - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    /* check coordinates */
    if (!fbo_coords_ok(fbop->fbo_fbs.fbs_fbp, xmin, ymin)) {
	bu_log("fb_cell: coordinates (%s, %s) are invalid.", argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%ld", &width) != 1) {
	bu_log("fb_cell: bad width - %s", argv[4]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[5], "%ld", &height) != 1) {
	bu_log("fb_cell: bad height - %s", argv[5]);
	return BRLCAD_ERROR;
    }


    /* check width and height */
    if (width <=0  || height <=0) {
	bu_log("fb_cell: width and height must be > 0");
	return BRLCAD_ERROR;
    }

    /*
     * Decompose the color list into its constituents.
     * For now must be in the form of rrr ggg bbb.
     */
    if (fbo_tcllist2color(argv[6], pixel) == BRLCAD_ERROR) {
	bu_log("fb_cell: invalid color spec: %s", argv[6]);
	return BRLCAD_ERROR;
    }

    pp = (unsigned char *)bu_calloc(width*height, sizeof(RGBpixel), "allocate pixel array");
    for (i = 0; i < width*height*sizeof(RGBpixel); i+=sizeof(RGBpixel)) {
	pp[i] = pixel[0];
	pp[i+1] = pixel[1];
	pp[i+2] = pixel[2];
    }
    fb_writerect(fbop->fbo_fbs.fbs_fbp, xmin, ymin, width, height, pp);
    bu_free((void *)pp, "free pixel array");

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname flush
 */
HIDDEN int
fbo_flush_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;

    if (argc != 2
	|| strcasecmp(argv[1], "flush") != 0)
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    fb_flush(fbop->fbo_fbs.fbs_fbp);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname getheight
 */
HIDDEN int
fbo_getheight_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| strcasecmp(argv[1], "getheight") != 0)
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&vls, "%d", fb_getheight(fbop->fbo_fbs.fbs_fbp));
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname getwidth
 */
HIDDEN int
fbo_getwidth_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| strcasecmp(argv[1], "getwidth") != 0)
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&vls, "%d", fb_getwidth(fbop->fbo_fbs.fbs_fbp));
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname getsize
 */
HIDDEN int
fbo_getsize_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| strcasecmp(argv[1], "getsize") != 0)
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&vls, "%d %d",
		  fb_getwidth(fbop->fbo_fbs.fbs_fbp),
		  fb_getheight(fbop->fbo_fbs.fbs_fbp));
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname cell xmin ymin width height color
 */
HIDDEN int
fbo_rect_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int xmin, ymin;
    int xmax, ymax;
    int width;
    int height;
    int i;
    RGBpixel pixel;

    if (argc != 7) {
	bu_log("ERROR: expecting seven arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &xmin) != 1) {
	bu_log("fb_rect: bad xmin value - ",
			 argv[2], (char *)NULL);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &ymin) != 1) {
	bu_log("fb_rect: bad ymin value - ",
			 argv[3], (char *)NULL);
	return BRLCAD_ERROR;
    }

    /* check coordinates */
    if (!fbo_coords_ok(fbop->fbo_fbs.fbs_fbp, xmin, ymin)) {
	bu_log("fb_rect: coordinates (%s, %s) are invalid.", argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%d", &width) != 1) {
	bu_log("fb_rect: bad width - %s", argv[4]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[5], "%d", &height) != 1) {
	bu_log("fb_rect: bad height - %s", argv[5]);
	return BRLCAD_ERROR;
    }


    /* check width and height */
    if (width <=0  || height <=0) {
	bu_log("fb_rect: width and height must be > 0");
	return BRLCAD_ERROR;
    }

    /*
     * Decompose the color list into its constituents.
     * For now must be in the form of rrr ggg bbb.
     */
    if (fbo_tcllist2color(argv[6], pixel) == BRLCAD_ERROR) {
	bu_log("fb_rect: invalid color spec: ", argv[6]);
	return BRLCAD_ERROR;
    }

    xmax = xmin + width;
    ymax = ymin + height;

    /* draw horizontal lines */
    for (i = xmin; i <= xmax; ++i) {
	/* working on bottom line */
	fb_write(fbop->fbo_fbs.fbs_fbp, i, ymin, pixel, 1);

	/* working on top line */
	fb_write(fbop->fbo_fbs.fbs_fbp, i, ymax, pixel, 1);
    }

    /* draw vertical lines */
    for (i = ymin; i <= ymax; ++i) {
	/* working on left line */
	fb_write(fbop->fbo_fbs.fbs_fbp, xmin, i, pixel, 1);

	/* working on right line */
	fb_write(fbop->fbo_fbs.fbs_fbp, xmax, i, pixel, 1);
    }

    return BRLCAD_OK;
}


/*
 * Usage:
 * procname configure width height
 */
HIDDEN int
fbo_configure_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int width, height;

    if (argc != 4) {
	bu_log("ERROR: expecting four arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &width) != 1) {
	bu_log("fb_configure: bad width - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &height) != 1) {
	bu_log("fb_configure: bad height - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    /* configure the framebuffer window */
    if (fbop->fbo_fbs.fbs_fbp != FBIO_NULL)
	fb_configureWindow(fbop->fbo_fbs.fbs_fbp, width, height);

    return BRLCAD_OK;
}


/*
 * F B O _ C M D
 *
 * Generic interface for framebuffer object routines.
 * Usage:
 * procname cmd ?args?
 *
 * Returns: result of FB command.
 */
HIDDEN int
fbo_cmd(ClientData clientData, Tcl_Interp *UNUSED(interp), int argc, const char **argv)
{
    static struct bu_cmdtab fbo_cmds[] = {
	{"cell",	fbo_cell_tcl},
	{"clear",	fbo_clear_tcl},
	{"close",	fbo_close_tcl},
	{"configure",	fbo_configure_tcl},
	{"cursor",	fbo_cursor_tcl},
	{"pixel",	fbo_pixel_tcl},
	{"flush",	fbo_flush_tcl},
	{"getcursor",	fbo_getcursor_tcl},
	{"getheight",	fbo_getheight_tcl},
	{"getsize",	fbo_getsize_tcl},
	{"getwidth",	fbo_getwidth_tcl},
	{"listen",	fbo_listen_tcl},
	{"rect",	fbo_rect_tcl},
	{"refresh",	fbo_refresh_tcl},
	{(char *)0,	(int (*)())0}
    };

    return bu_cmd(clientData, argc, argv, fbo_cmds, 1);
}


/*
 * Open/create a framebuffer object.
 *
 * Usage:
 * fb_open [name device [args]]
 */
HIDDEN int
fbo_open_tcl(void *UNUSED(clientData), Tcl_Interp *interp, int argc, const char **argv)
{
    struct fb_obj *fbop;
    FBIO *ifp;
    int width = 512;
    int height = 512;
    register int c;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc == 1) {
	/* get list of framebuffer objects */
	for (BU_LIST_FOR(fbop, fb_obj, &HeadFBObj.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&fbop->fbo_name), " ", (char *)NULL);

	return BRLCAD_OK;
    }

    if (argc < 3) {
	bu_vls_printf(&vls, "helplib fb_open");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /* process args */
    bu_optind = 3;
    bu_opterr = 0;
    while ((c = bu_getopt(argc, (char * const *)argv, "w:W:s:S:n:N:")) != -1) {
	switch (c) {
	    case 'W':
	    case 'w':
		width = atoi(bu_optarg);
		break;
	    case 'N':
	    case 'n':
		height = atoi(bu_optarg);
		break;
	    case 'S':
	    case 's':
		width = atoi(bu_optarg);
		height = width;
		break;
	    case '?':
	    default:
		bu_log("fb_open: bad option - ",
				 bu_optarg, (char *)NULL);
		return BRLCAD_ERROR;
	}
    }

    if ((ifp = fb_open(argv[2], width, height)) == FBIO_NULL) {
	bu_log("fb_open: bad device - ",
			 argv[2], (char *)NULL);
    }

    if (fb_ioinit(ifp) != 0) {
	bu_log("fb_open: fb_ioinit() failed.", (char *) NULL);
	return BRLCAD_ERROR;
    }

    BU_GETSTRUCT(fbop, fb_obj);
    bu_vls_init(&fbop->fbo_name);
    bu_vls_strcpy(&fbop->fbo_name, argv[1]);
    fbop->fbo_fbs.fbs_fbp = ifp;
    fbop->fbo_fbs.fbs_listener.fbsl_fbsp = &fbop->fbo_fbs;
    fbop->fbo_fbs.fbs_listener.fbsl_fd = -1;
    fbop->fbo_fbs.fbs_listener.fbsl_port = -1;
    fbop->fbo_interp = interp;

    /* append to list of fb_obj's */
    BU_LIST_APPEND(&HeadFBObj.l, &fbop->l);

    (void)Tcl_CreateCommand(interp,
			    bu_vls_addr(&fbop->fbo_name),
			    (Tcl_CmdProc *)fbo_cmd,
			    (ClientData)fbop,
			    fbo_deleteProc);

    /* Return new function name as result */
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, bu_vls_addr(&fbop->fbo_name), (char *)NULL);
    return BRLCAD_OK;
}


int
Fbo_Init(Tcl_Interp *interp)
{
    BU_LIST_INIT(&HeadFBObj.l);
    (void)Tcl_CreateCommand(interp, "fb_open", (Tcl_CmdProc *)fbo_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return BRLCAD_OK;
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
