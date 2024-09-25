/*                          D M - X . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2024 United States Government as represented by
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
/** @file libdm/dm-X.c
 *
 * An X Window System Display Manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdocumentation"
#endif

#define class REDEFINE_CLASS_STRING_TO_AVOID_CXX_CONFLICT
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_X11_XOSDEFS_H
#  include <X11/Xfuncproto.h>
#  include <X11/Xosdefs.h>
#endif

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
#  include <X11/extensions/XInput.h>
#endif /* HAVE_X11_XINPUT_H */

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#if defined(linux)
#  undef X_NOT_STDC_ENV
#  undef X_NOT_POSIX
#endif

#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "png.h"

#include "vmath.h"
#include "bu/endian.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "./fb_X.h"
#include "./dm-X.h"

#include "bv/defines.h"

#include "../include/private.h"

#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

#define DM_X_DEFAULT_POINT_SIZE 1.0

/* Vector count at which a Tcl event processing step is triggered */
#define vectorThreshold 100000

extern void X_allocate_color_cube(Display *, Colormap, long unsigned int *, int, int, int);
extern unsigned long X_get_pixel(unsigned char, unsigned char, unsigned char, long unsigned int *, int);


struct X_mvars {
    int zclip;
    double bound;
    int boundFlag;
};


struct allocated_colors {
    struct bu_list l;
    int r;
    int g;
    int b;
    XColor c;
};


static void
get_color(Display *dpy, Colormap cmap, XColor *color)
{
    Status st;
    static struct allocated_colors *colors = NULL;
    struct allocated_colors *c;
    int r, g, b;

    if (!colors) {
	BU_ALLOC(colors, struct allocated_colors);
	BU_LIST_INIT(&(colors->l));
	colors->r = colors->g = colors->b = -1;
    }

    /* allocated colors are stashed into a list in order to avoid
     * subsequent repeat allocations.
     */
    for (BU_LIST_FOR(c, allocated_colors, &(colors->l))) {
	if (DM_SAME_COLOR(c->r, c->g, c->b, color->red, color->green, color->blue)) {
	    *color = c->c; /* struct copy */
	    return;
	}
    }

    r = color->red;
    g = color->green;
    b = color->blue;

    st = XAllocColor(dpy, cmap, color);
    switch (st) {
	case 1:
	    break;
	case BadColor:
	    bu_log("XAllocColor failed (BadColor) for (%3d, %3d, %3d) %04x, %04x, %04x\n",
		   (r >> 8), (g >> 8), (b >> 8),
		   r, g, b);
	    break;

	default:
	    bu_log("XAllocColor error for (%3d, %3d, %3d) %04x, %04x, %04x\n",
		   (r >> 8), (g >> 8), (b >> 8),
		   r, g, b);
	    break;
    }

    /* got new valid color, add it to our list */
    BU_ALLOC(c, struct allocated_colors);
    c->r = r;
    c->g = g;
    c->b = b;
    c->c = *color; /* struct copy */
    BU_LIST_PUSH(&(colors->l), &(c->l));
}


static int
X_reshape(struct dm *dmp, int width, int height)
{
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    dmp->i->dm_height = height;
    dmp->i->dm_width = width;
    dmp->i->dm_aspect = (fastf_t)dmp->i->dm_width / (fastf_t)dmp->i->dm_height;

    privars->disp_mat[0] = 2. * privars->ppmm_x / dmp->i->dm_width;
    privars->disp_mat[5] = 2. * privars->ppmm_y / dmp->i->dm_width;

    return 0;
}

static int
X_doevent(struct dm *dmp, void *UNUSED(vclientData), void *veventPtr)
{
    XEvent *eventPtr= (XEvent *)veventPtr;
    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
        dm_set_dirty(dmp, 1);
        /* no further processing for this event */
        return TCL_RETURN;
    }
    /* allow further processing of this event */
    return TCL_OK;
}

static int
X_configureWin_guts(struct dm *dmp, int force)
{
    XWindowAttributes xwa;
    XFontStruct *newfontstruct;
    XGCValues gcv;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    XGetWindowAttributes(pubvars->dpy,
			 pubvars->win, &xwa);

    /* nothing to do */
    if (!force &&
	dmp->i->dm_height == xwa.height &&
	dmp->i->dm_width == xwa.width)
	return BRLCAD_OK;

    X_reshape(dmp, xwa.width, xwa.height);

    if (dmp->i->dm_debugLevel) {
	bu_log("X_configureWin_guts()\n");
	bu_log("width = %d, height = %d\n", dmp->i->dm_width, dmp->i->dm_height);
    }

#ifdef HAVE_TK
    Tk_FreePixmap(pubvars->dpy,
		  privars->pix);
    privars->pix =
	Tk_GetPixmap(pubvars->dpy,
		     DefaultRootWindow(pubvars->dpy),
		     dmp->i->dm_width,
		     dmp->i->dm_height,
		     Tk_Depth(pubvars->xtkwin));
#endif

    /* First time through, load a font or quit */
    if (pubvars->fontstruct == NULL) {
	if ((pubvars->fontstruct =
	     XLoadQueryFont(pubvars->dpy, FONT9)) == NULL) {
	    /* Try hardcoded backup font */
	    if ((pubvars->fontstruct =
		 XLoadQueryFont(pubvars->dpy, FONTBACK)) == NULL) {
		bu_log("dm-X: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return BRLCAD_ERROR;
	    }
	}

	gcv.font = pubvars->fontstruct->fid;
	XChangeGC(pubvars->dpy,
		  privars->gc, GCFont, &gcv);
    }

    /* Always try to choose a the font that best fits the window size.
     */

    if (dmp->i->dm_width < 582) {
	if (pubvars->fontstruct->per_char->width != 5) {
	    if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						FONT5)) != NULL) {
		XFreeFont(pubvars->dpy,
			  pubvars->fontstruct);
		pubvars->fontstruct = newfontstruct;
		gcv.font = pubvars->fontstruct->fid;
		XChangeGC(pubvars->dpy,
			  privars->gc, GCFont, &gcv);
	    }
	}
    } else if (dmp->i->dm_width < 679) {
	if (pubvars->fontstruct->per_char->width != 6) {
	    if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						FONT6)) != NULL) {
		XFreeFont(pubvars->dpy,
			  pubvars->fontstruct);
		pubvars->fontstruct = newfontstruct;
		gcv.font = pubvars->fontstruct->fid;
		XChangeGC(pubvars->dpy,
			  privars->gc, GCFont, &gcv);
	    }
	}
    } else if (dmp->i->dm_width < 776) {
	if (pubvars->fontstruct->per_char->width != 7) {
	    if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						FONT7)) != NULL) {
		XFreeFont(pubvars->dpy,
			  pubvars->fontstruct);
		pubvars->fontstruct = newfontstruct;
		gcv.font = pubvars->fontstruct->fid;
		XChangeGC(pubvars->dpy,
			  privars->gc, GCFont, &gcv);
	    }
	}
    } else if (dmp->i->dm_width < 873) {
	if (pubvars->fontstruct->per_char->width != 8) {
	    if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						FONT8)) != NULL) {
		XFreeFont(pubvars->dpy,
			  pubvars->fontstruct);
		pubvars->fontstruct = newfontstruct;
		gcv.font = pubvars->fontstruct->fid;
		XChangeGC(pubvars->dpy,
			  privars->gc, GCFont, &gcv);
	    }
	}
    } else {
	if (pubvars->fontstruct->per_char->width != 9) {
	    if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						FONT9)) != NULL) {
		XFreeFont(pubvars->dpy,
			  pubvars->fontstruct);
		pubvars->fontstruct = newfontstruct;
		gcv.font = pubvars->fontstruct->fid;
		XChangeGC(pubvars->dpy,
			  privars->gc, GCFont, &gcv);
	    }
	}
    }

    return BRLCAD_OK;
}


static XVisualInfo *
X_choose_visual(struct dm *dmp)
{
    XVisualInfo *vip, vitemp, *vibase, *maxvip;
    int num, i, j;
    int tries, baddepth;
    int desire_trueColor = 1;
    int min_depth = 8;
    int *good = NULL;
    int screen;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    vibase = XGetVisualInfo(pubvars->dpy, 0, &vitemp, &num);
    screen = DefaultScreen(((struct dm_Xvars *)dmp->i->dm_vars.pub_vars)->dpy);

    good = (int *)bu_malloc(sizeof(int)*num, "alloc good visuals");

    while (1) {
	for (i=0, j=0, vip=vibase; i<num; i++, vip++) {
	    /* requirements */
	    if (vip->screen != screen)
		continue;
	    if (vip->depth < min_depth)
		continue;
	    if (desire_trueColor) {
		if (vip->class != TrueColor)
		    continue;
	    } else if (vip->class != PseudoColor)
		continue;

	    /* this visual meets criteria */
	    good[j++] = i;
	}

	baddepth = 1000;
	for (tries = 0; tries < j; ++tries) {
	    maxvip = vibase + good[0];
	    for (i=1; i<j; i++) {
		vip = vibase + good[i];
		if ((vip->depth > maxvip->depth)&&(vip->depth < baddepth)) {
		    maxvip = vip;
		}
	    }

	    /* make sure Tk handles it */
	    if (desire_trueColor) {
		pubvars->cmap =
		    XCreateColormap(pubvars->dpy,
				    RootWindow(pubvars->dpy,
					       maxvip->screen),
				    maxvip->visual, AllocNone);
		privars->is_trueColor = 1;
	    } else {
		pubvars->cmap =
		    XCreateColormap(pubvars->dpy,
				    RootWindow(pubvars->dpy,
					       maxvip->screen),
				    maxvip->visual, AllocAll);
		privars->is_trueColor = 0;
	    }

#ifdef HAVE_TK
	    if (Tk_SetWindowVisual(pubvars->xtkwin,
				   maxvip->visual,
				   maxvip->depth,
				   pubvars->cmap)) {
		pubvars->depth = maxvip->depth;

		bu_free(good, "dealloc good visuals");
		return maxvip; /* success */
	    } else
#endif
	    {
		/* retry with lesser depth */
		baddepth = maxvip->depth;
		XFreeColormap(pubvars->dpy, pubvars->cmap);
	    }
	}

	if (desire_trueColor) {
	    desire_trueColor = 0;
	} else {
	    /* ran out of visuals, give up */
	    break;
	}
    }

    bu_free(good, "dealloc good visuals");
    return (XVisualInfo *)NULL; /* failure */
}


/*
 * Gracefully release the display.
 */
static int
X_close(struct dm *dmp)
{
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (pubvars->dpy) {
	if (privars->gc)
	    XFreeGC(pubvars->dpy,
		    privars->gc);

#ifdef HAVE_TK
	if (privars->pix)
	    Tk_FreePixmap(pubvars->dpy,
			  privars->pix);
#endif

	/*XXX Possibly need to free the colormap */
	if (pubvars->cmap)
	    XFreeColormap(pubvars->dpy,
			  pubvars->cmap);

#ifdef HAVE_TK
	if (pubvars->xtkwin)
	    Tk_DestroyWindow(pubvars->xtkwin);
#endif

    }

    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_free((void *)dmp->i->m_vars, "X_close: m_vars");
    bu_free((void *)dmp->i->dm_vars.priv_vars, "X_close: x_vars");
    bu_free((void *)dmp->i->dm_vars.pub_vars, "X_close: dm_Xvars");
    bu_free((void *)dmp->i, "X_close: dmp->i");
    bu_free((void *)dmp, "X_close: dmp");

    return BRLCAD_OK;
}

int
X_viable(const char *dpy_string)
{
    Display *dpy;
    if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
	XCloseDisplay(dpy);
	return 1;
    }
    return -1;
}

/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
X_open(void *UNUSED(ctx), void *vinterp, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;
    static int count = 0;
    int make_square = -1;
    XGCValues gcv;
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
    int j, k;
    int ndevices;
    int nclass = 0;
    int unused;
    XDeviceInfoPtr olist = NULL, list = NULL;
    XDevice *dev = NULL;
    XEventClass e_class[15];
    XInputClassInfo *cip = NULL;
#endif
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct dm *dmp = NULL;
    struct dm_impl *dmpi = NULL;
    Tk_Window tkwin = (Tk_Window)NULL;
    Screen *screen = (Screen *)NULL;

    struct dm_Xvars *pubvars = NULL;
    struct x_vars *privars = NULL;

#ifdef HAVE_TK
    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }
#endif

    BU_ALLOC(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_ALLOC(dmpi, struct dm_impl);

    *dmpi = *dm_X.i; /* struct copy */
    dmp->i = dmpi;
    dmp->i->dm_interp = interp;

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_Xvars);
    pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct x_vars);
    privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    BU_ALLOC(dmp->i->m_vars, struct X_mvars);
    struct X_mvars *m_vars = (struct X_mvars *)dmp->i->m_vars;
    m_vars->zclip = 0;
    m_vars->bound = PLOTBOUND;
    m_vars->boundFlag = 1;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0)
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_X%d", count);

    ++count;
    if (bu_vls_strlen(&dmp->i->dm_dName) == 0) {
	char *dp;

	dp = getenv("DISPLAY");
	if (dp)
	    bu_vls_strcpy(&dmp->i->dm_dName, dp);
	else
	    bu_vls_strcpy(&dmp->i->dm_dName, ":0.0");
    }

    /* initialize dm specific variables */
    pubvars->devmotionnotify = LASTEvent;
    pubvars->devbuttonpress = LASTEvent;
    pubvars->devbuttonrelease = LASTEvent;
    dmp->i->dm_aspect = 1.0;

    pubvars->fontstruct = NULL;

    if (dmp->i->dm_top) {
#ifdef HAVE_TK
	/* Make xtkwin a toplevel window */
	pubvars->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
						  bu_vls_addr(&dmp->i->dm_pathName),
						  bu_vls_addr(&dmp->i->dm_dName));
	pubvars->top = pubvars->xtkwin;
#endif
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->i->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->i->dm_pathName)) {
	    pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->i->dm_pathName), cp - bu_vls_addr(&dmp->i->dm_pathName));

#ifdef HAVE_TK
	    pubvars->top =
		Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
#endif
	    bu_vls_free(&top_vls);
	}

#ifdef HAVE_TK
	/* Make xtkwin an embedded window */
	pubvars->xtkwin =
	    Tk_CreateWindow(interp, pubvars->top,
			    cp + 1, (char *)NULL);
#endif
    }

    if (pubvars->xtkwin == NULL) {
	bu_log("X_open: Failed to open %s\n", bu_vls_addr(&dmp->i->dm_pathName));
	(void)X_close(dmp);
	return DM_NULL;
    }

#ifdef HAVE_TK
    bu_vls_printf(&dmp->i->dm_tkName, "%s",
		  (char *)Tk_Name(pubvars->xtkwin));
#endif

    if (bu_vls_strlen(&init_proc_vls) > 0) {
	bu_vls_printf(&str, "%s %s\n", bu_vls_addr(&init_proc_vls), bu_vls_addr(&dmp->i->dm_pathName));

	if (Tcl_Eval(interp, bu_vls_addr(&str)) == BRLCAD_ERROR) {
	    bu_vls_free(&str);
	    (void)X_close(dmp);
	    return DM_NULL;
	}
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy = NULL;
#ifdef HAVE_TK
    pubvars->dpy = Tk_Display(pubvars->top);
#endif

    /* make sure there really is a display before proceeding. */
    if (!pubvars->dpy) {
	bu_log("ERROR: Unable to attach to display (%s)\n", bu_vls_addr(&dmp->i->dm_pathName));
	(void)X_close(dmp);
	return DM_NULL;
    }

    screen = DefaultScreenOfDisplay(pubvars->dpy);

    if (!screen) {
#ifdef HAVE_TK
	/* failed to get a default screen, try harder */
	screen = Tk_Screen(pubvars->top);
#endif
    }

    /* make sure there really is a screen before processing. */
    if (!screen) {
	bu_log("ERROR: Unable to attach to screen (%s)\n", bu_vls_addr(&dmp->i->dm_pathName));
	(void)X_close(dmp);
	return DM_NULL;
    }

    /* FIXME: Applications on Mac OS X linking against the system
     * Tcl/Tk frameworks may crash here due to dpy being a high (7+
     * digit) value and the screens[] array it indexes into being 0x0.
     * Might have better luck calling functions instead of macros.
     */

    privars->ppmm_x = screen->width / screen->mwidth;
    privars->ppmm_y = screen->height / screen->mheight;

    if (dmp->i->dm_width == 0) {
	dmp->i->dm_width =
	    DisplayWidth(pubvars->dpy,
			 DefaultScreen(pubvars->dpy)) - 30;
	++make_square;
    }

    if (dmp->i->dm_height == 0) {
	dmp->i->dm_height =
	    DisplayHeight(pubvars->dpy,
			  DefaultScreen(pubvars->dpy)) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->i->dm_height <
	    dmp->i->dm_width)
	    dmp->i->dm_width = dmp->i->dm_height;
	else
	    dmp->i->dm_height = dmp->i->dm_width;
    }

#ifdef HAVE_TK
    Tk_GeometryRequest(pubvars->xtkwin,
		       dmp->i->dm_width,
		       dmp->i->dm_height);
#endif

    /* must do this before MakeExist */
    if ((pubvars->vip = X_choose_visual(dmp)) == NULL) {
	bu_log("X_open: Can't get an appropriate visual.\n");
	(void)X_close(dmp);
	return DM_NULL;
    }

#ifdef HAVE_TK
    Tk_MakeWindowExist(pubvars->xtkwin);
    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->i->dm_id = pubvars->win;
    privars->pix =
	Tk_GetPixmap(pubvars->dpy,
		     pubvars->win,
		     dmp->i->dm_width,
		     dmp->i->dm_height,
		     Tk_Depth(pubvars->xtkwin));
#endif

    if (privars->is_trueColor) {
	XColor fg, bg;

	INIT_XCOLOR(&fg);
	INIT_XCOLOR(&bg);

	fg.red = 65535;
	fg.green = fg.blue = 0;

	get_color(pubvars->dpy,
		  pubvars->cmap,
		  &fg);

	privars->fg = fg.pixel;

	bg.red = bg.green = bg.blue = 0;
	get_color(pubvars->dpy,
		  pubvars->cmap,
		  &bg);

	privars->bg = bg.pixel;
    } else {
	X_allocate_color_cube(pubvars->dpy,
			      pubvars->cmap,
			      privars->pixels,
			      /* cube dimension, uses XStoreColor */
			      6, CMAP_BASE, 1);

	privars->bg =
	    X_get_pixel(DM_BLACK,
			privars->pixels,
			CUBE_DIMENSION);
	privars->fg =
	    X_get_pixel(DM_RED,
			privars->pixels,
			CUBE_DIMENSION);
    }

    gcv.background = privars->bg;
    gcv.foreground = privars->fg;
    privars->gc = XCreateGC(pubvars->dpy,
			    pubvars->win,
			    (GCForeground|GCBackground), &gcv);

    /* First see if the server supports XInputExtension */
    {
	int return_val;

	if (!XQueryExtension(pubvars->dpy,
			     "XInputExtension", &return_val, &return_val, &return_val))
	    goto Skip_dials;
    }

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
    /*
     * Take a look at the available input devices. We're looking for
     * "dial+buttons".
     */
    if (XQueryExtension(pubvars->dpy, "XInputExtension", &unused, &unused, &unused)) {
	olist = list = (XDeviceInfoPtr)XListInputDevices(pubvars->dpy, &ndevices);
    }

    if (list == (XDeviceInfoPtr)NULL ||
	list == (XDeviceInfoPtr)1) goto Done;

    for (j = 0; j < ndevices; ++j, list++) {
	if (list->use == IsXExtensionDevice) {
	    if (BU_STR_EQUAL(list->name, "dial+buttons")) {
		if ((dev = XOpenDevice(pubvars->dpy,
				       list->id)) == (XDevice *)NULL) {
		    bu_log("X_open: Couldn't open the dials+buttons\n");
		    goto Done;
		}

		for (cip = dev->classes, k = 0; k < dev->num_classes;
		     ++k, ++cip) {
		    switch (cip->input_class) {
#ifdef IR_BUTTONS
			case ButtonClass:
			    DeviceButtonPress(dev, pubvars->devbuttonpress,
					      e_class[nclass]);
			    ++nclass;
			    DeviceButtonRelease(dev, pubvars->devbuttonrelease,
						e_class[nclass]);
			    ++nclass;
			    break;
#endif
#ifdef IR_KNOBS
			case ValuatorClass:
			    DeviceMotionNotify(dev, pubvars->devmotionnotify,
					       e_class[nclass]);
			    ++nclass;
			    break;
#endif
			default:
			    break;
		    }
		}

		XSelectExtensionEvent(pubvars->dpy,
				      pubvars->win, e_class, nclass);
		goto Done;
	    }
	}
    }
Done:
    XFreeDeviceList(olist);
#endif

Skip_dials:
    MAT_IDN(privars->mod_mat);
    MAT_IDN(privars->disp_mat);

    privars->xmat = &(privars->mod_mat[0]);

    (void)X_configureWin_guts(dmp, 1);

#ifdef HAVE_TK
    Tk_SetWindowBackground(pubvars->xtkwin,
			   privars->bg);
    Tk_MapWindow(pubvars->xtkwin);
#endif

    return dmp;
}


static int
X_drawBegin(struct dm *dmp)
{
    XGCValues gcv;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_drawBegin()\n");

    /* clear pixmap */
    gcv.foreground = privars->bg;
    XChangeGC(pubvars->dpy,
	      privars->gc,
	      GCForeground, &gcv);
    XFillRectangle(pubvars->dpy,
		   privars->pix,
		   privars->gc, 0,
		   0, dmp->i->dm_width + 1,
		   dmp->i->dm_height + 1);

    /* reset foreground */
    gcv.foreground = privars->fg;
    XChangeGC(pubvars->dpy,
	      privars->gc,
	      GCForeground, &gcv);

    return BRLCAD_OK;
}


static int
X_drawEnd(struct dm *dmp)
{
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_drawEnd()\n");

    XCopyArea(pubvars->dpy,
	      privars->pix,
	      pubvars->win,
	      privars->gc,
	      0, 0, dmp->i->dm_width,
	      dmp->i->dm_height, 0, 0);

    /* Prevent lag between events and updates */
    XSync(pubvars->dpy, 0);

    return BRLCAD_OK;
}


/*
 * Load a new Model matrix.  This will be followed by many
 * calls to X_draw().
 */
static int
X_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("X_load_Matrix()\n");

	bu_log("which eye = %d\t", which_eye);
	bu_log("transformation matrix = \n");

	/* note the row/column ordering */
	bu_log("%g %g %g %g\n", mat[0], mat[1], mat[2], mat[3]);
	bu_log("%g %g %g %g\n", mat[4], mat[5], mat[6], mat[7]);
	bu_log("%g %g %g %g\n", mat[8], mat[9], mat[10], mat[11]);
	bu_log("%g %g %g %g\n", mat[12], mat[13], mat[14], mat[15]);
    }

    MAT_COPY(privars->mod_mat, mat);
    return BRLCAD_OK;
}


static int
X_drawVList(struct dm *dmp, struct bv_vlist *vp)
{
    static vect_t spnt, lpnt, pnt;
    struct bv_vlist *tvp;
    XSegment segbuf[1024];	/* XDrawSegments list */
    XSegment *segp;		/* current segment */
    int nseg;		        /* number of segments */
    fastf_t delta;
    point_t *pt_prev = NULL;
    fastf_t dist_prev=1.0;
    fastf_t pointSize = DM_X_DEFAULT_POINT_SIZE;

    static int nvectors = 0;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;
    struct X_mvars *m_vars = (struct X_mvars *)dmp->i->m_vars;

    /* delta is used in clipping to insure clipped endpoint is
     * slightly in front of eye plane (perspective mode only).  This
     * value is a SWAG that seems to work OK.
     */
    delta = privars->xmat[15]*0.0001;
    if (delta < 0.0)
	delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
	delta = SQRT_SMALL_FASTF;

    nseg = 0;
    segp = segbuf;
    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	point_t tlate;
	fastf_t dist;

	/* Viewing region is from -1.0 to +1.0 */
	/* 2^31 ~= 2e9 -- dynamic range of a long int */
	/* 2^(31-11) = 2^20 ~= 1e6 */
	/* Integerize and let the X server do the clipping */
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BV_VLIST_POLY_START:
		case BV_VLIST_POLY_VERTNORM:
		case BV_VLIST_TRI_START:
		case BV_VLIST_TRI_VERTNORM:
		    continue;
		case BV_VLIST_MODEL_MAT:
		    privars->xmat = &(privars->mod_mat[0]);
		    continue;
		case BV_VLIST_DISPLAY_MAT:
		    MAT4X3PNT(tlate, privars->mod_mat, *pt);
		    privars->disp_mat[3] = tlate[0];
		    privars->disp_mat[7] = tlate[1];
		    privars->disp_mat[11] = tlate[2];
		    privars->xmat = &(privars->disp_mat[0]);
		    continue;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_LINE_MOVE:
		case BV_VLIST_TRI_MOVE:
		    /* Move, not draw */
		    if (dmp->i->dm_debugLevel > 2) {
			bu_log("before transformation:\n");
			bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
		    }

		    if (dmp->i->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &(privars->xmat)[12]) + privars->xmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			} else {
			    MAT4X3PNT(lpnt, privars->xmat, *pt);
			    dist_prev = dist;
			    pt_prev = pt;
			}
		    } else {
			MAT4X3PNT(lpnt, privars->xmat, *pt);
		    }

		    lpnt[0] *= 2047;
		    lpnt[1] *= 2047 * dmp->i->dm_aspect;
		    lpnt[2] *= 2047;
		    continue;
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_POLY_END:
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_TRI_DRAW:
		case BV_VLIST_TRI_END:
		    /* draw */
		    if (dmp->i->dm_debugLevel > 2) {
			bu_log("before transformation:\n");
			bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
		    }

		    if (dmp->i->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &(privars->xmat)[12]) + privars->xmat[15];
			if (dmp->i->dm_debugLevel > 2)
			    bu_log("dist=%g, dist_prev=%g\n", dist, dist_prev);
			if (dist <= 0.0) {
			    if (dist_prev <= 0.0) {
				/* nothing to plot */
				dist_prev = dist;
				pt_prev = pt;
				continue;
			    } else {
				if (pt_prev) {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip this end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (dist_prev - delta) / (dist_prev - dist);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(pnt, privars->xmat, tmp_pt);
				}
			    }
			} else {
			    if (dist_prev <= 0.0) {
				if (pt_prev) {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip other end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (-dist_prev + delta) / (dist - dist_prev);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(lpnt, privars->xmat, tmp_pt);
				lpnt[0] *= 2047;
				lpnt[1] *= 2047 * dmp->i->dm_aspect;
				lpnt[2] *= 2047;
				MAT4X3PNT(pnt, privars->xmat, *pt);
				}
			    } else {
				MAT4X3PNT(pnt, privars->xmat, *pt);
			    }
			}
			dist_prev = dist;
		    } else {
			MAT4X3PNT(pnt, privars->xmat, *pt);
		    }

		    pnt[0] *= 2047;
		    pnt[1] *= 2047 * dmp->i->dm_aspect;
		    pnt[2] *= 2047;

		    /* save pnt --- it might get changed by clip() */
		    VMOVE(spnt, pnt);
		    pt_prev = pt;

		    if (dmp->i->dm_debugLevel > 2) {
			bu_log("before clipping:\n");
			bu_log("clipmin - %lf %lf %lf\n",
			       dmp->i->dm_clipmin[X],
			       dmp->i->dm_clipmin[Y],
			       dmp->i->dm_clipmin[Z]);
			bu_log("clipmax - %lf %lf %lf\n",
			       dmp->i->dm_clipmax[X],
			       dmp->i->dm_clipmax[Y],
			       dmp->i->dm_clipmax[Z]);
			bu_log("pt1 - %lf %lf %lf\n", lpnt[X], lpnt[Y], lpnt[Z]);
			bu_log("pt2 - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
		    }

		    if (m_vars->zclip) {
			if (vclip(lpnt, pnt,
				  dmp->i->dm_clipmin,
				  dmp->i->dm_clipmax) == 0) {
			    VMOVE(lpnt, spnt);
			    continue;
			}
		    } else {
			fastf_t min_short = (fastf_t)SHRT_MIN;
			fastf_t max_short = (fastf_t)SHRT_MAX;

			/* Check to see if lpnt or pnt contain values
			 * that exceed the capacity of a short (segbuf
			 * is an array of XSegments which contain
			 * shorts). If so, do clipping now. Otherwise,
			 * let the X server do the clipping.
			 */
			if (lpnt[0] < min_short || max_short < lpnt[0] ||
			    lpnt[1] < min_short || max_short < lpnt[1] ||
			    pnt[0] < min_short || max_short < pnt[0] ||
			    pnt[1] < min_short || max_short < pnt[1]) {
			    /* if the entire line segment will not be
			     * visible then ignore it.
			     */
			    if (clip(&lpnt[0], &lpnt[1], &pnt[0], &pnt[1]) == -1) {
				VMOVE(lpnt, spnt);
				continue;
			    }
			}
		    }

		    if (dmp->i->dm_debugLevel > 2) {
			bu_log("after clipping:\n");
			bu_log("pt1 - %lf %lf %lf\n", lpnt[X], lpnt[Y], lpnt[Z]);
			bu_log("pt2 - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
		    }

		    /* convert to X window coordinates */
		    segp->x1 = (short)GED_TO_Xx(dmp, lpnt[0]);
		    segp->y1 = (short)GED_TO_Xy(dmp, lpnt[1]);
		    segp->x2 = (short)GED_TO_Xx(dmp, pnt[0]);
		    segp->y2 = (short)GED_TO_Xy(dmp, pnt[1]);

		    nseg++;
		    segp++;
		    VMOVE(lpnt, spnt);

		    if (nseg == 1024) {
			XDrawSegments(pubvars->dpy,
				      privars->pix,
				      privars->gc, segbuf, nseg);

			nseg = 0;
			segp = segbuf;
		    }
		    break;
		case BV_VLIST_POINT_DRAW:
		    if (dmp->i->dm_debugLevel > 2) {
			bu_log("before transformation:\n");
			bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
		    }

		    if (dmp->i->dm_perspective > 0) {
			dist = VDOT(*pt, &(privars->xmat)[12]) + privars->xmat[15];

			if (dist <= 0.0) {
			    /* nothing to plot - point is behind eye plane */
			    continue;
			}
		    }

		    MAT4X3PNT(pnt, privars->xmat, *pt);

		    pnt[0] *= 2047;
		    pnt[1] *= 2047 * dmp->i->dm_aspect;
		    pnt[2] *= 2047;
		    VMOVE(lpnt, pnt);

		    if (dmp->i->dm_debugLevel > 2) {
			bu_log("after clipping:\n");
			bu_log("pt - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
		    }

		    if (pointSize <= DM_X_DEFAULT_POINT_SIZE) {
			/* draws a single pixel */
			XDrawPoint(pubvars->dpy, privars->pix, privars->gc,
				GED_TO_Xx(dmp, pnt[0]), GED_TO_Xy(dmp, pnt[1]));
		    } else {
			int upperLeft[2];

			/* need the upper-left adjusted a half-pixel
			 * so points with odd size are centered.
			 */
			upperLeft[X] = (double)GED_TO_Xx(dmp, pnt[0]) - (pointSize / 2.0) + 0.5;
			upperLeft[Y] = (double)GED_TO_Xy(dmp, pnt[1]) - (pointSize / 2.0) + 0.5;

			/* NOTE: in order to get smooth circles,
			 * especially for small radii, X11 requires
			 * that you draw both the interior and the
			 * exterior.  only filling small radius
			 * circles will otherwise render as a square.
			 */
			XDrawArc(pubvars->dpy, privars->pix, privars->gc, upperLeft[X], upperLeft[Y], pointSize, pointSize, 0, 360*64);
			XFillArc(pubvars->dpy, privars->pix, privars->gc, upperLeft[X], upperLeft[Y], pointSize, pointSize, 0, 360*64);
		    }
		    break;
		case BV_VLIST_POINT_SIZE:
		    pointSize = (*pt)[0];
		    if (pointSize < DM_X_DEFAULT_POINT_SIZE) {
			pointSize = DM_X_DEFAULT_POINT_SIZE;
		    }
		    break;
	    }
	}

	nvectors += nused;
	if (nvectors >= vectorThreshold) {
	    if (dmp->i->dm_debugLevel)
		bu_log("X_drawVList(): handle Tcl events\n");

	    nvectors = 0;

	    /* Handle events in the queue */
	    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
	}
    }

    if (nseg) {
	XDrawSegments(pubvars->dpy,
		      privars->pix,
		      privars->gc, segbuf, nseg);
    }

    return BRLCAD_OK;
}


static int
X_draw(struct dm *dmp, struct bv_vlist *(*callback_function)(void *), void **data)
{
    struct bv_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bv_vlist *)data;
	    X_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return BRLCAD_ERROR;
	} else {
	    (void)callback_function(data);
	}
    }
    return BRLCAD_OK;
}


static int
X_hud_begin(struct dm *dmp)
{
    if (dmp->i->dm_debugLevel)
	bu_log("X_normal()\n");

    return BRLCAD_OK;
}

static int
X_hud_end(struct dm *dmp)
{
    if (dmp->i->dm_debugLevel)
	bu_log("X_normal()\n");

    return BRLCAD_OK;
}



/**
 * Output a string into the displaylist.  The starting position of the
 * beam is as specified.
 */
static int
X_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect)
{
    int sx, sy;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("X_drawString2D():\n");
	bu_log("\tstr - %s\n", str);
	bu_log("\tx - %g\n", x);
	bu_log("\ty - %g\n", y);
	bu_log("\tsize - %d\n", size);
	if (use_aspect) {
	    bu_log("\tuse_aspect - %d\t\taspect ratio - %g\n", use_aspect, dmp->i->dm_aspect);
	} else
	    bu_log("\tuse_aspect - 0");
    }

    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, use_aspect);

    XDrawString(pubvars->dpy,
		privars->pix,
		privars->gc,
		sx, sy, str, strlen(str));

    return BRLCAD_OK;
}


static int
X_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2)
{
    int sx1, sy1, sx2, sy2;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    sx1 = dm_Normal2Xx(dmp, x_1);
    sx2 = dm_Normal2Xx(dmp, x_2);
    sy1 = dm_Normal2Xy(dmp, y_1, 0);
    sy2 = dm_Normal2Xy(dmp, y_2, 0);

    if (dmp->i->dm_debugLevel) {
	bu_log("X_drawLine2D()\n");
	bu_log("x1 = %g, y1 = %g\n", x_1, y_1);
	bu_log("x2 = %g, y2 = %g\n", x_2, y_2);
	bu_log("sx1 = %d, sy1 = %d\n", sx1, sy1);
	bu_log("sx2 = %d, sy2 = %d\n", sx2, sy2);
    }

    XDrawLine(pubvars->dpy,
	      privars->pix,
	      privars->gc,
	      sx1, sy1, sx2, sy2);

    return BRLCAD_OK;
}


static int
X_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    return draw_Line3D(dmp, pt1, pt2);
}


static int
X_drawLines3D(struct dm *dmp, int npoints, point_t *points, int UNUSED(sflag))
{
    if (!dmp || npoints < 0 || !points)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


static int
X_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    int sx, sy;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, 0);

    if (dmp->i->dm_debugLevel) {
	bu_log("X_drawPoint2D()\n");
	bu_log("x = %g, y = %g\n", x, y);
	bu_log("sx = %d, sy = %d\n", sx, sy);
    }

    XDrawPoint(pubvars->dpy,
	       privars->pix,
	       privars->gc, sx, sy);

    return BRLCAD_OK;
}


static int
X_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    XGCValues gcv;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_setFGColor() rgb=[%d, %d, %d] strict=%d transparency=%f\n", r, g, b, strict, transparency);

    dmp->i->dm_fg[0] = r;
    dmp->i->dm_fg[1] = g;
    dmp->i->dm_fg[2] = b;

    if (privars->is_trueColor) {
	XColor color;

	INIT_XCOLOR(&color);

	color.red = r << 8;
	color.green = g << 8;
	color.blue = b << 8;
	get_color(pubvars->dpy,
		  pubvars->cmap,
		  &color);

	gcv.foreground = color.pixel;
    } else {
	gcv.foreground = X_get_pixel(r, g, b, privars->pixels, CUBE_DIMENSION);
    }

    /* save foreground pixel */
    privars->fg = gcv.foreground;

    XChangeGC(pubvars->dpy,
	      privars->gc,
	      GCForeground, &gcv);

    return BRLCAD_OK;
}


static int
X_setBGColor(struct dm *dmp,
	unsigned char r, unsigned char g, unsigned char b,
	unsigned char UNUSED(r2), unsigned char UNUSED(g2), unsigned char UNUSED(b2)
	)
{
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_setBGColor()\n");

    dmp->i->dm_bg1[0] = r;
    dmp->i->dm_bg1[1] = g;
    dmp->i->dm_bg1[2] = b;

    if (privars->is_trueColor) {
	XColor color;

	INIT_XCOLOR(&color);

	color.red = r << 8;
	color.green = g << 8;
	color.blue = b << 8;

	get_color(pubvars->dpy,
		  pubvars->cmap,
		  &color);

	privars->bg = color.pixel;
    } else
	privars->bg =
	    X_get_pixel(r, g, b, privars->pixels, CUBE_DIMENSION);

    return BRLCAD_OK;
}


static int
X_setLineAttr(struct dm *dmp, int width, int style)
{
    int linestyle;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_setLineAttr()\n");

    dmp->i->dm_lineWidth = width;
    dmp->i->dm_lineStyle = style;

    if (width <= 1)
	width = 0;

    if (style == DM_DASHED_LINE)
	linestyle = LineOnOffDash;
    else
	linestyle = LineSolid;

    XSetLineAttributes(pubvars->dpy,
		       privars->gc,
		       width, linestyle, CapButt, JoinMiter);

    return BRLCAD_OK;
}


static int
X_debug(struct dm *dmp, int lvl)
{
    dmp->i->dm_debugLevel = lvl;

    return BRLCAD_OK;
}


static int
X_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->i->dm_log, "%s", filename);

    return BRLCAD_OK;
}


static int
X_setWinBounds(struct dm *dmp, fastf_t *w)
{
    if (dmp->i->dm_debugLevel)
	bu_log("X_setWinBounds()\n");

    dmp->i->dm_clipmin[0] = w[0];
    dmp->i->dm_clipmin[1] = w[2];
    dmp->i->dm_clipmin[2] = w[4];
    dmp->i->dm_clipmax[0] = w[1];
    dmp->i->dm_clipmax[1] = w[3];
    dmp->i->dm_clipmax[2] = w[5];

    return BRLCAD_OK;
}

static int
X_setZClip(struct dm *dmp, int zclip)
{
    struct X_mvars *mvars = (struct X_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_setZClip");

    mvars->zclip = zclip;

    return BRLCAD_OK;
}

static int
X_getZClip(struct dm *dmp)
{
    struct X_mvars *mvars = (struct X_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_getZClip");

    return mvars->zclip;
}

static int
X_setBound(struct dm *dmp, double bound)
{
    struct X_mvars *mvars = (struct X_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_setBound");

    mvars->bound = bound;

    return BRLCAD_OK;
}

static double
X_getBound(struct dm *dmp)
{
    struct X_mvars *mvars = (struct X_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_getBound");

    return mvars->bound;
}

static int
X_setBoundFlag(struct dm *dmp, int bound)
{
    struct X_mvars *mvars = (struct X_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_setBoundFlag");

    mvars->boundFlag = bound;

    return BRLCAD_OK;
}

static int
X_getBoundFlag(struct dm *dmp)
{
    struct X_mvars *mvars = (struct X_mvars *)dmp->i->m_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("X_getBoundFlag");

    return mvars->boundFlag;
}

static int
X_configureWin(struct dm *dmp, int force)
{
    /* don't force */
    return X_configureWin_guts(dmp, force);
}


static int
X_getDisplayImage(struct dm *dmp, unsigned char **image, int flip, int alpha)
{
    XImage *ximage_p;
    unsigned char **rows;
    unsigned char *idata;
    unsigned char *irow;
    int bytes_per_pixel;
    int bytes_per_pixel_output = 3; /* limit for current bu_image raw pix usage */
    int bytes_per_line_output;
    int i, j, k;
    unsigned char *dbyte0, *dbyte1, *dbyte2;
    int red_shift;
    int green_shift;
    int blue_shift;
    int red_bits;
    int green_bits;
    int blue_bits;

    if (flip) {
	bu_log("X: flipping unimplemented for this backend\n");
	return BRLCAD_ERROR;
    }

    if (alpha) {
	bu_log("X: alpha support unimplemented for this backend\n");
	return BRLCAD_ERROR;
    }

    ximage_p = XGetImage(((struct dm_Xvars *)dmp->i->dm_vars.pub_vars)->dpy,
			 ((struct dm_Xvars *)dmp->i->dm_vars.pub_vars)->win,
			 0, 0,
			 dmp->i->dm_width,
			 dmp->i->dm_height,
			 ~0, ZPixmap);

    if (!ximage_p) {
	bu_log("png: could not get XImage\n");
	return BRLCAD_ERROR;
    }

    bytes_per_pixel = ximage_p->bytes_per_line / ximage_p->width;

    if (bytes_per_pixel == 4) {
	unsigned long mask;
	unsigned long tmask;

	/* This section assumes 8 bits per channel */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 32; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 32; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 32; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	/*
	 * We need to reverse things if the image byte order
	 * is different from the system's byte order.
	 */
	if (((bu_byteorder() == BU_BIG_ENDIAN) && (ximage_p->byte_order == LSBFirst)) ||
	    ((bu_byteorder() == BU_LITTLE_ENDIAN) && (ximage_p->byte_order == MSBFirst))) {
	    DM_REVERSE_COLOR_BYTE_ORDER(red_shift, ximage_p->red_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(green_shift, ximage_p->green_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(blue_shift, ximage_p->blue_mask);
	}

    } else if (bytes_per_pixel == 2) {
	unsigned long mask;
	unsigned long tmask;
	int bpb = 8;   /* bits per byte */

	/*XXX
	 * This section probably needs logic similar
	 * to the previous section (i.e. bytes_per_pixel == 4).
	 * That is we may need to reverse things depending on
	 * the image byte order and the system's byte order.
	 */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 16; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (red_bits = red_shift; red_bits < 16; red_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	red_bits = red_bits - red_shift;
	if (red_shift == 0)
	    red_shift = red_bits - bpb;
	else
	    red_shift = red_shift - (bpb - red_bits);

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 16; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (green_bits = green_shift; green_bits < 16; green_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	green_bits = green_bits - green_shift;
	green_shift = green_shift - (bpb - green_bits);

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 16; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (blue_bits = blue_shift; blue_bits < 16; blue_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}
	blue_bits = blue_bits - blue_shift;

	if (blue_shift == 0)
	    blue_shift = blue_bits - bpb;
	else
	    blue_shift = blue_shift - (bpb - blue_bits);
    } else {
	bu_log("png: %d bytes per pixel is not yet supported\n", bytes_per_pixel);
	return BRLCAD_ERROR;
    }

    rows = (unsigned char **)bu_calloc(ximage_p->height, sizeof(unsigned char *), "rows");
    idata = (unsigned char *)bu_calloc(ximage_p->height * ximage_p->width, bytes_per_pixel_output, "png data");
    *image = idata;

    /* for each scanline */
    bytes_per_line_output = ximage_p->width * bytes_per_pixel_output;
    for (i = ximage_p->height - 1, j = 0; 0 <= i; --i, ++j) {
	/* irow points to the current scanline in ximage_p */
	irow = (unsigned char *)(ximage_p->data + ((ximage_p->height-i-1)*ximage_p->bytes_per_line));

	if (bytes_per_pixel == 4) {
	    unsigned int pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*bytes_per_line_output));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->width; k++) {
		pixel = *((unsigned int *)(irow + k*bytes_per_pixel));

		dbyte0 = rows[j] + k*bytes_per_pixel_output;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;

		*dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;
		*dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
	    }
	} else if (bytes_per_pixel == 2) {
	    unsigned short spixel;
	    unsigned long pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*bytes_per_line_output));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->width; k++) {
		spixel = *((unsigned short *)(irow + k*bytes_per_pixel));
		pixel = spixel;

		dbyte0 = rows[j] + k*bytes_per_pixel_output;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;

		if (0 <= red_shift)
		    *dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		else
		    *dbyte0 = (pixel & ximage_p->red_mask) << -red_shift;

		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;

		if (0 <= blue_shift)
		    *dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
		else
		    *dbyte2 = (pixel & ximage_p->blue_mask) << -blue_shift;
	    }
	} else {
	    bu_free(rows, "rows");
	    bu_free(idata, "image data");

	    bu_log("png: not supported for this platform\n");
	    return BRLCAD_ERROR;
	}
    }

    bu_free(rows, "rows");
    return BRLCAD_OK;
}

int
X_openFb(struct dm *dmp)
{
    struct fb_platform_specific *fb_ps;
    struct X24_fb_info *xfb_ps;
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    struct x_vars *privars = (struct x_vars *)dmp->i->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_X24_MAGIC);
    xfb_ps = (struct X24_fb_info *)fb_ps->data;
    xfb_ps->dpy = pubvars->dpy;
    xfb_ps->win = privars->pix;
    xfb_ps->cwinp = pubvars->win;
    xfb_ps->cmap = pubvars->cmap;
    xfb_ps->vip = pubvars->vip;
    xfb_ps->gc = privars->gc;

    dmp->i->fbp = fb_open_existing("X", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
    return 0;
}

#define X_MV_O(_m) offsetof(struct X_mvars, _m)
struct bu_structparse X_vparse[] = {
    {"%g",  1, "bound",         X_MV_O(bound),          dm_generic_hook, NULL, NULL},
    {"%d",  1, "useBound",      X_MV_O(boundFlag),      dm_generic_hook, NULL, NULL},
    {"%d",  1, "zclip",         X_MV_O(zclip),          dm_generic_hook, NULL, NULL},
    {"",    0, (char *)0,       0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

int
X_geometry_request(struct dm *dmp, int width, int height)
{
    if (!dmp) return -1;
    Tk_GeometryRequest(((struct dm_Xvars *)dmp->i->dm_vars.pub_vars)->xtkwin, width, height);
    return 0;
}

#define XVARS_MV_O(_m) offsetof(struct dm_Xvars, _m)

struct bu_structparse dm_Xvars_vparse[] = {
    {"%x",      1,      "dpy",                  XVARS_MV_O(dpy),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "win",                  XVARS_MV_O(win),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "top",                  XVARS_MV_O(top),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "tkwin",                XVARS_MV_O(xtkwin),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "depth",                XVARS_MV_O(depth),      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "cmap",                 XVARS_MV_O(cmap),       BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "vip",                  XVARS_MV_O(vip),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "fontstruct",           XVARS_MV_O(fontstruct), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devmotionnotify",      XVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       XVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     XVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
X_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal X variables", dm_Xvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_Xvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');
    return 0;
}


// TODO - this and getDisplayImage need to be consolidated...
int
X_write_image(struct bu_vls *msgs, FILE *fp, struct dm *dmp)
{
    png_structp png_p;
    png_infop info_p;
    XImage *ximage_p;
    unsigned char **rows;
    unsigned char *idata;
    unsigned char *irow;
    int bytes_per_pixel;
    int bits_per_channel = 8;  /* bits per color channel */
    int i, j, k;
    unsigned char *dbyte0, *dbyte1, *dbyte2, *dbyte3;
    int red_shift;
    int green_shift;
    int blue_shift;
    int red_bits;
    int green_bits;
    int blue_bits;

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "X_write_image: could not create PNG write structure\n");
	}
	return -1;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "X_write_image: could not create PNG info structure\n");
	}
	png_destroy_write_struct(&png_p, NULL);
	return -1;
    }

    ximage_p = XGetImage(((struct dm_Xvars *)dmp->i->dm_vars.pub_vars)->dpy,
			 ((struct dm_Xvars *)dmp->i->dm_vars.pub_vars)->win,
			 0, 0,
			 dmp->i->dm_width,
			 dmp->i->dm_height,
			 ~0, ZPixmap);
    if (!ximage_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "png: could not get XImage\n");
	}
	png_destroy_write_struct(&png_p, &info_p);
	return -1;
    }

    bytes_per_pixel = ximage_p->bytes_per_line / ximage_p->width;

    if (bytes_per_pixel == 4) {
	unsigned long mask;
	unsigned long tmask;

	/* This section assumes 8 bits per channel */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 32; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 32; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 32; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	/*
	 * We need to reverse things if the image byte order
	 * is different from the system's byte order.
	 */
	if (((bu_byteorder() == BU_BIG_ENDIAN) && (ximage_p->byte_order == LSBFirst)) ||
	    ((bu_byteorder() == BU_LITTLE_ENDIAN) && (ximage_p->byte_order == MSBFirst))) {
	    DM_REVERSE_COLOR_BYTE_ORDER(red_shift, ximage_p->red_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(green_shift, ximage_p->green_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(blue_shift, ximage_p->blue_mask);
	}

    } else if (bytes_per_pixel == 2) {
	unsigned long mask;
	unsigned long tmask;
	int bpb = 8;   /* bits per byte */

	/*XXX
	 * This section probably needs logic similar
	 * to the previous section (i.e. bytes_per_pixel == 4).
	 * That is we may need to reverse things depending on
	 * the image byte order and the system's byte order.
	 */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 16; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (red_bits = red_shift; red_bits < 16; red_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	red_bits = red_bits - red_shift;
	if (red_shift == 0)
	    red_shift = red_bits - bpb;
	else
	    red_shift = red_shift - (bpb - red_bits);

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 16; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (green_bits = green_shift; green_bits < 16; green_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	green_bits = green_bits - green_shift;
	green_shift = green_shift - (bpb - green_bits);

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 16; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (blue_bits = blue_shift; blue_bits < 16; blue_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}
	blue_bits = blue_bits - blue_shift;

	if (blue_shift == 0)
	    blue_shift = blue_bits - bpb;
	else
	    blue_shift = blue_shift - (bpb - blue_bits);
    } else {
	if (msgs) {
	    bu_vls_printf(msgs, "png: %d bytes per pixel is not yet supported\n", bytes_per_pixel);
	}
	png_destroy_write_struct(&png_p, &info_p);
	return -1;
    }

    rows = (unsigned char **)bu_calloc(ximage_p->height, sizeof(unsigned char *), "rows");
    idata = (unsigned char *)bu_calloc(ximage_p->height * ximage_p->width, 4, "png data");

    /* for each scanline */
    for (i = ximage_p->height - 1, j = 0; 0 <= i; --i, ++j) {
	/* irow points to the current scanline in ximage_p */
	irow = (unsigned char *)(ximage_p->data + ((ximage_p->height-i-1)*ximage_p->bytes_per_line));

	if (bytes_per_pixel == 4) {
	    unsigned int pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*ximage_p->bytes_per_line));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->bytes_per_line; k += bytes_per_pixel) {
		pixel = *((unsigned int *)(irow + k));

		dbyte0 = rows[j] + k;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;
		dbyte3 = dbyte0 + 3;

		*dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;
		*dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
		*dbyte3 = 255;
	    }
	} else if (bytes_per_pixel == 2) {
	    unsigned short spixel;
	    unsigned long pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*ximage_p->bytes_per_line*2));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->bytes_per_line; k += bytes_per_pixel) {
		spixel = *((unsigned short *)(irow + k));
		pixel = spixel;

		dbyte0 = rows[j] + k*2;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;
		dbyte3 = dbyte0 + 3;

		if (0 <= red_shift)
		    *dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		else
		    *dbyte0 = (pixel & ximage_p->red_mask) << -red_shift;

		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;

		if (0 <= blue_shift)
		    *dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
		else
		    *dbyte2 = (pixel & ximage_p->blue_mask) << -blue_shift;

		*dbyte3 = 255;
	    }
	} else {
	    bu_free(rows, "rows");
	    bu_free(idata, "image data");
	    png_destroy_write_struct(&png_p, &info_p);
	    if (msgs) {
		bu_vls_printf(msgs, "png: not supported for this platform\n");
	    }
	    return -1;
	}
    }

    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p, ximage_p->width, ximage_p->height, bits_per_channel,
		 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, 0.77);
    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);

    png_destroy_write_struct(&png_p, &info_p);

    bu_free(rows, "rows");
    bu_free(idata, "image data");

    return 0;
}

void
X_flush(struct dm *dmp)
{
    struct dm_Xvars *pub_vars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    XFlush(pub_vars->dpy);
}

void
X_sync(struct dm *dmp)
{
    struct dm_Xvars *pub_vars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    XSync(pub_vars->dpy, 0);
}

int
X_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_Xvars *pubvars = (struct dm_Xvars *)dmp->i->dm_vars.pub_vars;
    switch (type) {
	case DM_MOTION_NOTIFY:
	    return (event == pubvars->devmotionnotify) ? 1 : 0;
	    break;
	case DM_BUTTON_PRESS:
	    return (event == pubvars->devbuttonpress) ? 1 : 0;
	    break;
	case DM_BUTTON_RELEASE:
	    return (event == pubvars->devbuttonrelease) ? 1 : 0;
	    break;
	default:
	    return -1;
	    break;
    };
}

/* Display Manager package interface */
struct dm_impl dm_X_impl = {
    X_open,
    X_close,
    X_viable,
    X_drawBegin,
    X_drawEnd,
    X_hud_begin,
    X_hud_end,
    X_loadMatrix,
    null_loadPMatrix,
    null_popPMatrix,
    X_drawString2D,
    null_String2DBBox,
    X_drawLine2D,
    X_drawLine3D,
    X_drawLines3D,
    X_drawPoint2D,
    null_drawPoint3D,
    null_drawPoints3D,
    X_drawVList,
    X_drawVList,
    null_draw_obj,
    NULL,
    X_draw,
    X_setFGColor,
    X_setBGColor,
    X_setLineAttr,
    X_configureWin,
    X_setWinBounds,
    null_setLight,
    null_getLight,
    null_setTransparency,
    null_getTransparency,
    null_setDepthMask,
    null_setZBuffer,
    null_getZBuffer,
    X_setZClip,
    X_getZClip,
    X_setBound,
    X_getBound,
    X_setBoundFlag,
    X_getBoundFlag,
    X_debug,
    X_logfile,
    null_beginDList,
    null_endDList,
    null_drawDList,
    null_freeDLists,
    null_genDLists,
    NULL,
    X_getDisplayImage, /* display to image function */
    X_reshape,
    null_makeCurrent,
    null_SwapBuffers,
    X_doevent,
    X_openFb,
    NULL,
    NULL,
    X_geometry_request,
    X_internal_var,
    X_write_image,
    X_flush,
    X_sync,
    X_event_cmp,
    NULL,
    NULL,
    0,
    1,				/* is graphical */
    "Tk",                       /* uses Tk graphics system */
    0,				/* no displaylist */
    0,                          /* no stereo */
    "X",
    "X Window System (X11)",
    1, /* top */
    0, /* width */
    0, /* height */
    0, /* dirty */
    0, /* bytes per pixel */
    0, /* bits per channel */
    0,
    0,
    1.0, /* aspect ratio */
    0,
    {0, 0},
    NULL,
    NULL,
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
    {0, 0, 0},			/* bg1 color */
    {0, 0, 0},			/* bg2 color */
    {0, 0, 0},			/* fg color */
    {GED_MIN, GED_MIN, GED_MIN},	/* clipmin */
    {GED_MAX, GED_MAX, GED_MAX},	/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* depth buffer is not writable */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    X_vparse,
    FB_NULL,
    0,				/* Tcl interpreter */
    NULL,                       /* Drawing context */
    NULL                        /* App data */
};

struct dm dm_X = { DM_MAGIC, &dm_X_impl, 0 };

#ifdef DM_PLUGIN
static const struct dm_plugin pinfo = { DM_API, &dm_X };

COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info(void)
{
    return &pinfo;
}
#endif

/* Because class is actually used to access a struct
 * entry in this file, preserve our redefinition
 * of class for the benefit of avoiding C++ name
 * collisions until the end of this file */
#undef class

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
