/*                        F B _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup fb */
/*@{*/
/** @file fb_obj.c
 * A framebuffer object contains the attributes and
 * methods for controlling  framebuffers.
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Authors -
 *	Robert G. Parker
 *	Ronald Bowers
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#include "common.h"

#include <stdlib.h>

#include "tcl.h"
#include "machine.h"
#include "cmd.h"                  /* includes bu.h */
#include "fb.h"
#include "fbserv_obj.h"

/* defined in libfb/tcl.c */
extern int fb_refresh(FBIO *ifp, int x, int y, int w, int h);

static int fbo_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_cell_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_clear_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_close_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_cursor_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_getcursor_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_getheight_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_getsize_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_getwidth_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_pixel_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_flush_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_listen_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_refresh_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_rect_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_configure_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int fbo_coords_ok(Tcl_Interp *interp, FBIO *fbp, int x, int y);
static int fbo_tcllist2color(Tcl_Interp *interp, char *string, unsigned char *pixel);

#define FBO_CONSTRAIN(_v, _a, _b)\
	((_v > _a) ? (_v < _b ? _v : _b) : _a)

struct fb_obj {
	struct bu_list		l;
	struct bu_vls		fbo_name;	/* framebuffer object name/cmd */
	struct fbserv_obj	fbo_fbs;	/* fbserv object */
};

static struct fb_obj HeadFBObj;			/* head of display manager object list */

static struct bu_cmdtab fbo_cmds[] = {
       {"cell",		fbo_cell_tcl},
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
       {"rect",		fbo_rect_tcl},
       {"refresh",	fbo_refresh_tcl},
       {(char *)0,	(int (*)())0}
};

/*
 *			F B O _ C M D
 *
 * Generic interface for framebuffer object routines.
 * Usage:
 *        procname cmd ?args?
 *
 * Returns: result of FB command.
 */
static int
fbo_cmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	return bu_cmd(clientData, interp, argc, argv, fbo_cmds, 1);
}

int
Fbo_Init(Tcl_Interp *interp)
{
	BU_LIST_INIT(&HeadFBObj.l);
	(void)Tcl_CreateCommand(interp, "fb_open", (Tcl_CmdProc *)fbo_open_tcl,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

/*
 * Called by Tcl when the object is destroyed.
 */
static void
fbo_deleteProc(ClientData clientData)
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
 *	  procname close
 */
static int
fbo_close_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Among other things, this will call dmo_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&fbop->fbo_name));

	return TCL_OK;
}

/*
 * Open/create a framebuffer object.
 *
 * Usage:
 *	  fb_open [name device [args]]
 */
static int
fbo_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop;
	FBIO *ifp;
	int width = 512;
	int height = 512;
	register int c;
	struct bu_vls vls;

	if (argc == 1) {
		/* get list of framebuffer objects */
		for (BU_LIST_FOR(fbop, fb_obj, &HeadFBObj.l))
			Tcl_AppendResult(interp, bu_vls_addr(&fbop->fbo_name), " ", (char *)NULL);

		return TCL_OK;
	}

	if (argc < 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_open");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* process args */
	bu_optind = 3;
	bu_opterr = 0;
	while ((c = bu_getopt(argc, argv, "w:W:s:S:n:N:")) != EOF)  {
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
			Tcl_AppendResult(interp, "fb_open: bad option - ",
					 bu_optarg, (char *)NULL);
			return TCL_ERROR;
		}
	}

	if ((ifp = fb_open(argv[2], width, height)) == FBIO_NULL) {
		Tcl_AppendResult(interp, "fb_open: bad device - ",
				 argv[2], (char *)NULL);
	}

	if (fb_ioinit(ifp) != 0) {
		Tcl_AppendResult(interp, "fb_open: fb_ioinit() failed.", (char *) NULL);
		return TCL_ERROR;
	}

	BU_GETSTRUCT(fbop, fb_obj);
	bu_vls_init(&fbop->fbo_name);
	bu_vls_strcpy(&fbop->fbo_name, argv[1]);
	fbop->fbo_fbs.fbs_fbp = ifp;
	fbop->fbo_fbs.fbs_listener.fbsl_fbsp = &fbop->fbo_fbs;
	fbop->fbo_fbs.fbs_listener.fbsl_fd = -1;
	fbop->fbo_fbs.fbs_listener.fbsl_port = -1;

	/* append to list of fb_obj's */
	BU_LIST_APPEND(&HeadFBObj.l,&fbop->l);

	(void)Tcl_CreateCommand(interp,
				bu_vls_addr(&fbop->fbo_name),
				(Tcl_CmdProc *)fbo_cmd,
				(ClientData)fbop,
				fbo_deleteProc);

	/* Return new function name as result */
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, bu_vls_addr(&fbop->fbo_name), (char *)NULL);
	return TCL_OK;
}

/*
 * Clear the framebuffer with the specified color.
 * Otherwise, clear the framebuffer with black.
 *
 * Usage:
 *	  procname clear [rgb]
 */
static int
fbo_clear_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	int status;
	RGBpixel pixel;
	unsigned char *ms;


	if (argc < 2 || 3 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_clear");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argc == 3) {
		/*
		 * Decompose the color list into its constituents.
		 * For now must be in the form of rrr ggg bbb.
		 */
		if (fbo_tcllist2color(interp, argv[6], pixel) == TCL_ERROR) {
			Tcl_AppendResult(interp, "fb_cell: invalid color spec: ", argv[6], ".",
					 (char *)NULL);
			return TCL_ERROR;
		}

		ms = pixel;
	}else
		ms = RGBPIXEL_NULL;

	status = fb_clear(fbop->fbo_fbs.fbs_fbp, ms);

	if(status < 0)
		return TCL_ERROR;

	return TCL_OK;
}

/*
 *
 * Usage:
 *	  procname cursor mode x y
 */
static int
fbo_cursor_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	int mode;
	int x, y;
	int status;
	struct bu_vls vls;

	if (argc != 5) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_cursor");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
	}

	if (sscanf(argv[2], "%d", &mode) != 1) {
		Tcl_AppendResult(interp, "fb_cursor: bad mode - ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[3], "%d", &x) != 1) {
		Tcl_AppendResult(interp, "fb_cursor: bad x value - ",
				 argv[3], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[4], "%d", &y) != 1) {
		Tcl_AppendResult(interp, "fb_cursor: bad y value - ",
				 argv[4], (char *)NULL);
		return TCL_ERROR;
	}

	status = fb_cursor(fbop->fbo_fbs.fbs_fbp, mode, x, y);
	if (status == 0)
		return TCL_OK;

	return TCL_ERROR;
}

/*
 *
 * Usage:
 *	  procname getcursor
 */
static int
fbo_getcursor_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	int status;
	int mode;
	int x, y;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_getcursor");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	status = fb_getcursor(fbop->fbo_fbs.fbs_fbp, &mode, &x, &y);
	if (status == 0) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d %d %d", mode, x, y);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	return TCL_ERROR;
}

/*
 * Refresh the entire framebuffer or that part specified by
 * a rectangle (i.e. x y width height)
 * Usage:
 *	  procname refresh [rect]
 */
static int
fbo_refresh_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	int x, y, w, h;		       /* rectangle to be refreshed */

	if (argc < 2 || 3 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_refresh");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argc == 2) {  /* refresh the whole display */
		x = y = 0;
		w = fbop->fbo_fbs.fbs_fbp->if_width;
		h = fbop->fbo_fbs.fbs_fbp->if_height;
	} else if (sscanf(argv[2], "%d %d %d %d", &x, &y, &w, &h) != 4) { /* refresh rectanglar area */
		Tcl_AppendResult(interp, "fb_refresh: bad rectangle - ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

#if 1
	return fb_refresh(fbop->fbo_fbs.fbs_fbp, x, y, w, h);
#else
	return fbop->fbo_fbs.fbs_fbp->if_refresh(fbop->fbo_fbs.fbs_fbp, x, y, w, h)
#endif
}

/*
 * Listen for framebuffer clients.
 *
 * Usage:
 *	  procname listen port
 *
 * Returns the port number actually used.
 *
 */
static int
fbo_listen_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (fbop->fbo_fbs.fbs_fbp == FBIO_NULL) {
		bu_vls_printf(&vls, "%s listen: framebuffer not open!\n", argv[0]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	/* return the port number */
	if (argc == 2) {
		bu_vls_printf(&vls, "%d", fbop->fbo_fbs.fbs_listener.fbsl_port);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	if (argc == 3) {
		int port;

		if (sscanf(argv[2], "%d", &port) != 1) {
			Tcl_AppendResult(interp, "listen: bad value - ", argv[2], "\n", (char *)NULL);
			return TCL_ERROR;
		}

		if (port >= 0)
			fbs_open(interp, &fbop->fbo_fbs, port);
		else {
			fbs_close(interp, &fbop->fbo_fbs);
		}
		bu_vls_printf(&vls, "%d", fbop->fbo_fbs.fbs_listener.fbsl_port);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	bu_vls_printf(&vls, "helplib fb_listen");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Set/get the pixel value at position (x, y).
 *
 * Usage:
 *	  procname pixel x y [rgb]
 */
static int
fbo_pixel_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;
	int x, y; 	/* pixel position */
	RGBpixel pixel;


	if (argc < 4)
		goto error;

	/* get pixel position */
	if (sscanf(argv[2], "%d", &x) != 1) {
		Tcl_AppendResult(interp, "fb_pixel: bad x value - ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[3], "%d", &y) != 1) {
		Tcl_AppendResult(interp, "fb_pixel: bad y value - ",
				 argv[3], (char *)NULL);
		return TCL_ERROR;
	}

	/* check pixel position */
	if (!fbo_coords_ok(interp, fbop->fbo_fbs.fbs_fbp, x, y)) {
		Tcl_AppendResult(interp,
				 "fb_pixel: coordinates (", argv[2], ", ", argv[3],
				 ") are invalid.", (char *)NULL);
		return TCL_ERROR;
	}

	/* get pixel value */
	if (argc == 4) {
		fb_rpixel(fbop->fbo_fbs.fbs_fbp, pixel);
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d %d %d", pixel[RED], pixel[GRN], pixel[BLU]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* set pixel value */
	if (argc == 5) {
		/*
		 * Decompose the color list into its constituents.
		 * For now must be in the form of rrr ggg bbb.
		 */

		if (fbo_tcllist2color(interp, argv[4], pixel) == TCL_ERROR) {
			Tcl_AppendResult(interp, "fb_pixel: invalid color spec - ", argv[4], ".",
					 (char *)NULL);
			return TCL_ERROR;
		}

		fb_write(fbop->fbo_fbs.fbs_fbp, x, y, pixel, 1);

		return TCL_OK;
	}

error:
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib fb_pixel");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 *
 * Usage:
 *	  procname cell xmin ymin width height color
 */
static int
fbo_cell_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;
	int xmin, ymin;
	int width;
	int height;
	int i;
	RGBpixel pixel;
	unsigned char *pp;



	if (argc != 7) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_cell");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%d", &xmin) != 1) {
		Tcl_AppendResult(interp, "fb_cell: bad xmin value - ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[3], "%d", &ymin) != 1) {
		Tcl_AppendResult(interp, "fb_cell: bad ymin value - ",
				 argv[3], (char *)NULL);
		return TCL_ERROR;
	}

	/*  check coordinates */
	if (!fbo_coords_ok(interp, fbop->fbo_fbs.fbs_fbp, xmin, ymin)) {
		Tcl_AppendResult(interp,
				 "fb_cell: coordinates (", argv[2], ", ", argv[3],
				 ") are invalid.", (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[4], "%d", &width) != 1) {
		Tcl_AppendResult(interp, "fb_cell: bad width - ",
				 argv[4], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[5], "%d", &height) != 1) {
		Tcl_AppendResult(interp, "fb_cell: bad height - ",
				 argv[5], (char *)NULL);
		return TCL_ERROR;
	}


	/* check width and height */
	if (width <=0  || height <=0) {
		Tcl_AppendResult(interp, "fb_cell: width and height must be > 0", (char *)NULL);
		return TCL_ERROR;
	}

	/*
	 * Decompose the color list into its constituents.
	 * For now must be in the form of rrr ggg bbb.
	 */
	if (fbo_tcllist2color(interp, argv[6], pixel) == TCL_ERROR) {
		Tcl_AppendResult(interp, "fb_cell: invalid color spec: ", argv[6], ".",
				 (char *)NULL);
		return TCL_ERROR;
	}

	pp = (unsigned char *)calloc(width*height, sizeof(RGBpixel));
	for (i = 0; i < width*height*sizeof(RGBpixel); i+=sizeof(RGBpixel)) {
		pp[i] = pixel[0];
		pp[i+1] = pixel[1];
		pp[i+2] = pixel[2];
	}
	fb_writerect(fbop->fbo_fbs.fbs_fbp, xmin, ymin, width, height, pp);
	free((void *)pp);

	return TCL_OK;
}

/*
 *
 * Usage:
 *	  procname flush
 */
static int
fbo_flush_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;


	if (argc != 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_flush");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	fb_flush(fbop->fbo_fbs.fbs_fbp);

	return TCL_OK;
}

/*
 *
 * Usage:
 *	  procname getheight
 */
static int
fbo_getheight_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_getheight");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%d", fb_getheight(fbop->fbo_fbs.fbs_fbp));
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 *
 * Usage:
 *	  procname getwidth
 */
static int
fbo_getwidth_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_getwidth");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%d", fb_getwidth(fbop->fbo_fbs.fbs_fbp));
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 *
 * Usage:
 *	  procname getsize
 */
static int
fbo_getsize_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_getsize");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%d %d",
		      fb_getwidth(fbop->fbo_fbs.fbs_fbp),
		      fb_getheight(fbop->fbo_fbs.fbs_fbp));
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);


	return TCL_OK;
}

/*
 *
 * Usage:
 *	  procname cell xmin ymin width height color
 */
static int
fbo_rect_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;
	int xmin, ymin;
	int xmax, ymax;
	int width;
	int height;
	int i;
	RGBpixel pixel;

	if (argc != 7) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_rect");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%d", &xmin) != 1) {
		Tcl_AppendResult(interp, "fb_rect: bad xmin value - ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[3], "%d", &ymin) != 1) {
		Tcl_AppendResult(interp, "fb_rect: bad ymin value - ",
				 argv[3], (char *)NULL);
		return TCL_ERROR;
	}

	/*  check coordinates */
	if (!fbo_coords_ok(interp, fbop->fbo_fbs.fbs_fbp, xmin, ymin)) {
		Tcl_AppendResult(interp,
				 "fb_rect: coordinates (", argv[2], ", ", argv[3],
				 ") are invalid.", (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[4], "%d", &width) != 1) {
		Tcl_AppendResult(interp, "fb_rect: bad width - ",
				 argv[4], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[5], "%d", &height) != 1) {
		Tcl_AppendResult(interp, "fb_rect: bad height - ",
				 argv[5], (char *)NULL);
		return TCL_ERROR;
	}


	/* check width and height */
	if (width <=0  || height <=0) {
		Tcl_AppendResult(interp, "fb_rect: width and height must be > 0", (char *)NULL);
		return TCL_ERROR;
	}

	/*
	 * Decompose the color list into its constituents.
	 * For now must be in the form of rrr ggg bbb.
	 */
	if (fbo_tcllist2color(interp, argv[6], pixel) == TCL_ERROR) {
		Tcl_AppendResult(interp, "fb_rect: invalid color spec: ", argv[6], ".",
				 (char *)NULL);
		return TCL_ERROR;
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

	return TCL_OK;
}

/*
 * Usage:
 *	  procname configure width height
 */
int
fbo_configure_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	int width, height;

	if (argc != 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib fb_configure");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%d", &width) != 1) {
		Tcl_AppendResult(interp, "fb_configure: bad width - ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	if (sscanf(argv[3], "%d", &height) != 1) {
		Tcl_AppendResult(interp, "fb_configure: bad height - ",
				 argv[3], (char *)NULL);
		return TCL_ERROR;
	}

	/* configure the framebuffer window */
	if (fbop->fbo_fbs.fbs_fbp != FBIO_NULL)
		fb_configureWindow(fbop->fbo_fbs.fbs_fbp, width, height);

	return TCL_OK;
}
#if 0
/*
 *
 * Usage:
 *	  procname
 */
int
fbo__tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
	struct fb_obj *fbop = (struct fb_obj *)clientData;
	struct bu_vls vls;

	return TCL_OK;
}
#endif

/****************** utility routines ********************/
static int
fbo_coords_ok(Tcl_Interp *interp, FBIO *fbp, int x, int y)
{
	int	    width;
	int	    height;
	int	    errors;
	width = fb_getwidth(fbp);
	height = fb_getheight(fbp);

	errors = 0;

	if (x < 0) {
		Tcl_AppendResult(interp, "fbo_coords_ok: Error!: X value < 0\n",
				 (char *)NULL);
		++errors;
	}

	if (y < 0) {
		Tcl_AppendResult(interp, "fbo_coords_ok: Error!: Y value < 0\n",
				 (char *)NULL);
		++errors;
	}

	if (x > width - 1) {
		Tcl_AppendResult(interp, "fbo_coords_ok: Error!: X value too large\n",
				 (char *)NULL);
		++errors;
	}

	if (y > height - 1) {
		Tcl_AppendResult(interp, "fbo_coords_ok: Error!: Y value too large\n",
				 (char *)NULL);
		++errors;
	}

	if ( errors ) {
		return 0;
	} else {
		return 1;
	}
}

static int
fbo_tcllist2color(Tcl_Interp *interp, char *string, unsigned char *pixel)
{
    int r, g, b;

    if (sscanf(string, "%d %d %d", &r, &g, &b) != 3) {
	    Tcl_AppendResult(interp,
			     "fb_clear: bad color spec - ",
			     string, (char *)NULL);
	    return TCL_ERROR;
    }

    pixel[RED] = FBO_CONSTRAIN (r, 0, 255);
    pixel[GRN] = FBO_CONSTRAIN (g, 0, 255);
    pixel[BLU] = FBO_CONSTRAIN (b, 0, 255);

    return TCL_OK;
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
