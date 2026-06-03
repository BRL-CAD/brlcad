/*                 D M - T K S W R A S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file dm-tkswrast.cpp
 *
 * Native Tk + osmesa implementation of libdm 3D display logic
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tcl.h"
#include "tk.h"

#include "OSMesa/gl.h"
#include "OSMesa/osmesa.h"

extern "C" {
#include "dm.h"
#include "../dm-gl.h"
#include "../include/private.h"
#include "../swrast/dm-swrast.h"
}

struct tkswrast_vars {
    Tk_Window top;
    Tk_Window xtkwin;
    Tk_PhotoHandle photo;
    struct bu_vls photo_name;
    struct bu_vls label_path;
    int photo_ready;
    int photo_w;
    int photo_h;
    int mapped_once;
    int (*orig_configureWin)(struct dm *, int);
    int (*orig_close)(struct dm *);
    unsigned char *img_b;
    size_t img_b_size;
};

static int tkswrast_debug(void);
static void tkswrast_log(const char *fmt, ...);

static int
tkswrast_debug(void)
{
    const char *d = getenv("TKSWRAST_DEBUG");
    return (d && BU_STR_EQUAL(d, "1"));
}

static void
tkswrast_log(const char *fmt, ...)
{
    static FILE *lfp = NULL;
    if (!tkswrast_debug())
	return;

    if (!lfp) {
	const char *lfile = getenv("TKSWRAST_LOG_FILE");
	if (!lfile || !strlen(lfile))
	    lfile = "swrast_log.txt";
	lfp = fopen(lfile, "a");
	if (!lfp)
	    return;
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(lfp, fmt, ap);
    va_end(ap);
    fflush(lfp);
}

static unsigned long
tkswrast_black_pixel(Tk_Window tkwin)
{
#ifdef TKSWRAST_X11
    return BlackPixelOfScreen(Tk_Screen(tkwin));
#else
    return 0;
#endif
}

static void
tkswrast_log_tk_hierarchy(struct dm *dmp)
{
    if (!tkswrast_debug() || !dmp || !dmp->i || !dmp->i->dm_interp)
	return;

    Tcl_Interp *ti = (Tcl_Interp *)dmp->i->dm_interp;
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    struct bu_vls out = BU_VLS_INIT_ZERO;

    bu_vls_printf(&cmd,
	    "set _w %s; set _p [winfo parent $_w]; set _mgr [winfo manager $_w]; "
	    "set _g [winfo geometry $_w]; set _pg [winfo geometry $_p]; "
	    "set _wch [winfo children $_w]; set _pch [winfo children $_p]; "
	    "list $_w $_p $_mgr $_g $_pg $_wch $_pch",
	    bu_vls_addr(&dmp->i->dm_pathName));
    if (Tcl_Eval(ti, bu_vls_addr(&cmd)) == TCL_OK) {
	bu_vls_printf(&out, "%s", Tcl_GetStringResult(ti));
	tkswrast_log("tkswrast_tk[%s]: %s\n", bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&out));
    } else {
	tkswrast_log("tkswrast_tk[%s]: hierarchy query failed: %s\n",
		bu_vls_addr(&dmp->i->dm_pathName), Tcl_GetStringResult(ti));
    }

    bu_vls_free(&out);
    bu_vls_free(&cmd);
}

static void
tkswrast_log_startup_windows(struct dm *dmp, const char *tag)
{
    if (!tkswrast_debug() || !dmp || !dmp->i || !dmp->i->dm_interp)
	return;

    Tcl_Interp *ti = (Tcl_Interp *)dmp->i->dm_interp;
    struct bu_vls cmd = BU_VLS_INIT_ZERO;

    bu_vls_printf(&cmd,
	    "set _r {}; "
	    "foreach _w {.id_0 .topid_0} {"
	    "if {[winfo exists $_w]} {"
	    "lappend _r $_w [winfo ismapped $_w] [winfo geometry $_w] [winfo manager $_w] [winfo children $_w]"
	    "} else {"
	    "lappend _r $_w missing"
	    "}"
	    "}; "
	    "set _cw %s; "
	    "if {[winfo exists $_cw]} {lappend _r $_cw [winfo ismapped $_cw] [winfo geometry $_cw] [winfo manager $_cw]} ; "
	    "set _pw [winfo parent $_cw]; "
	    "if {[winfo exists $_pw]} {lappend _r $_pw [winfo ismapped $_pw] [winfo geometry $_pw] [winfo manager $_pw]} ; "
	    "set _r",
	    bu_vls_addr(&dmp->i->dm_pathName));

    if (Tcl_Eval(ti, bu_vls_addr(&cmd)) == TCL_OK) {
	tkswrast_log("tkswrast_startup[%s][%s]: %s\n",
		bu_vls_addr(&dmp->i->dm_pathName),
		tag ? tag : "",
		Tcl_GetStringResult(ti));
    } else {
	tkswrast_log("tkswrast_startup[%s][%s]: query failed: %s\n",
		bu_vls_addr(&dmp->i->dm_pathName),
		tag ? tag : "",
		Tcl_GetStringResult(ti));
    }

    bu_vls_free(&cmd);
}

static void
tkswrast_safe_image_name(struct bu_vls *out, const char *path)
{
    bu_vls_sprintf(out, "tkswrast_img");
    if (!path)
	return;
    for (const char *c = path; *c; c++) {
	if ((*c >= 'a' && *c <= 'z') ||
		(*c >= 'A' && *c <= 'Z') ||
		(*c >= '0' && *c <= '9') ||
		*c == '_') {
	    bu_vls_putc(out, *c);
	} else {
	    bu_vls_putc(out, '_');
	}
    }
}

static int
tkswrast_configureWin(struct dm *dmp, int force)
{
    struct tkswrast_vars *tv = (struct tkswrast_vars *)dmp->i->dm_udata;
    struct swrast_vars *sv = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;
    if (!tv || !sv || !sv->v)
	return BRLCAD_ERROR;

    int width = Tk_Width(tv->xtkwin);
    int height = Tk_Height(tv->xtkwin);
    if ((width <= 1 || height <= 1) && tv->top) {
	int pw = Tk_Width(tv->top);
	int ph = Tk_Height(tv->top);
	if (pw > 1 && ph > 1) {
	    width = pw;
	    height = ph;
	}
    }
    if (tkswrast_debug()) {
	tkswrast_log("tkswrast_configureWin[%s]: force=%d tk=%dx%d dm(before)=%dx%d top=%d\n",
		bu_vls_addr(&dmp->i->dm_pathName), force, width, height,
		dmp->i->dm_width, dmp->i->dm_height, dmp->i->dm_top);
	tkswrast_log_tk_hierarchy(dmp);
	tkswrast_log_startup_windows(dmp, "configure");
    }
    if (width < 2 || height < 2)
	return BRLCAD_OK;

    if (!force && dmp->i->dm_width == width && dmp->i->dm_height == height)
	return BRLCAD_OK;

    Tk_GeometryRequest(tv->xtkwin, width, height);
    if (!Tk_IsMapped(tv->xtkwin) && width > 1 && height > 1)
	Tk_MapWindow(tv->xtkwin);

    sv->v->gv_width = width;
    sv->v->gv_height = height;

    dmp->i->dm_width = width;
    dmp->i->dm_height = height;
    dmp->i->dm_aspect = (fastf_t)width / (fastf_t)height;

    if (!tv->mapped_once && width > 1 && height > 1) {
	tv->mapped_once = 1;
	dm_set_dirty(dmp, 1);
    }

    if (tkswrast_debug()) {
	tkswrast_log("tkswrast_configureWin[%s]: dm(after)=%dx%d aspect=%g\n",
		bu_vls_addr(&dmp->i->dm_pathName), dmp->i->dm_width, dmp->i->dm_height, dmp->i->dm_aspect);
    }

    return tv->orig_configureWin(dmp, 1);
}

static int
tkswrast_SwapBuffers(struct dm *dmp)
{
    struct tkswrast_vars *tv = (struct tkswrast_vars *)dmp->i->dm_udata;
    struct swrast_vars *sv = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;
    if (!tv || !sv || !sv->ctx)
	return BRLCAD_OK;

    int ww = Tk_Width(tv->xtkwin);
    int wh = Tk_Height(tv->xtkwin);
    if (ww > 0 && wh > 0 && (ww != dmp->i->dm_width || wh != dmp->i->dm_height)) {
	(void)dmp->i->dm_configureWin(dmp, 1);
    }

    int src_w = 0, src_h = 0, format = 0;
    void *buffer = NULL;
    if (!OSMesaGetColorBuffer(sv->ctx, &src_w, &src_h, &format, &buffer) || !buffer)
	return BRLCAD_ERROR;

    int width = src_w;
    int height = src_h;
    int dw = Tk_Width(tv->xtkwin);
    int dh = Tk_Height(tv->xtkwin);
    if (dw < 2 || dh < 2) {
	if (tkswrast_debug()) {
	    tkswrast_log("tkswrast_swap[%s]: defer tiny tk=%dx%d osmesa=%dx%d\n",
		    bu_vls_addr(&dmp->i->dm_pathName), dw, dh, src_w, src_h);
	}
	return BRLCAD_OK;
    }
    if (dw > 0 && dh > 0) {
	if (dw < width)
	    width = dw;
	if (dh < height)
	    height = dh;
    }

    if (tkswrast_debug()) {
	tkswrast_log("tkswrast_swap[%s]: tk=%dx%d osmesa=%dx%d blit=%dx%d dm=%dx%d fmt=%d\n",
		bu_vls_addr(&dmp->i->dm_pathName), ww, wh, src_w, src_h, width, height,
		dmp->i->dm_width, dmp->i->dm_height, format);
    }

    {
	Tcl_Interp *ti_geom = (Tcl_Interp *)dmp->i->dm_interp;
	if (ti_geom && bu_vls_strlen(&tv->label_path) > 0) {
	    struct bu_vls pcmd = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&pcmd,
		    "if {[winfo exists %s]} {place configure %s -x 0 -y 0 -relwidth 1 -relheight 1; raise %s}",
		    bu_vls_addr(&tv->label_path), bu_vls_addr(&tv->label_path),
		    bu_vls_addr(&tv->label_path));
	    (void)Tcl_Eval(ti_geom, bu_vls_addr(&pcmd));
	    if (tkswrast_debug()) {
		bu_vls_trunc(&pcmd, 0);
		bu_vls_printf(&pcmd,
			"if {[winfo exists %s]} {list [winfo geometry %s] [winfo ismapped %s] [winfo manager %s]}",
			bu_vls_addr(&tv->label_path), bu_vls_addr(&tv->label_path), bu_vls_addr(&tv->label_path), bu_vls_addr(&tv->label_path));
		if (Tcl_Eval(ti_geom, bu_vls_addr(&pcmd)) == TCL_OK) {
		    tkswrast_log("tkswrast_label[%s]: %s\n", bu_vls_addr(&dmp->i->dm_pathName), Tcl_GetStringResult(ti_geom));
		}
	    }
	    bu_vls_free(&pcmd);
	}
    }

    size_t need = (size_t)width * (size_t)height * 4;
    if (!tv->img_b || tv->img_b_size < need) {
	tv->img_b = (unsigned char *)bu_realloc(tv->img_b, need, "tkswrast image buffer");
	tv->img_b_size = need;
    }

    unsigned char *src = (unsigned char *)buffer;
    for (int y = 0; y < height; y++) {
	int sy = src_h - 1 - y;
	for (int x = 0; x < width; x++) {
	    size_t si = ((size_t)sy * (size_t)src_w + (size_t)x) * 4;
	    size_t di = ((size_t)y * (size_t)width + (size_t)x) * 4;
	    tv->img_b[di + 0] = src[si + 0];
	    tv->img_b[di + 1] = src[si + 1];
	    tv->img_b[di + 2] = src[si + 2];
	    tv->img_b[di + 3] = 255;
	}
    }

    if (!tv->photo) {
	Tcl_Interp *ti = (Tcl_Interp *)dmp->i->dm_interp;
	if (!ti)
	    return BRLCAD_ERROR;
	tv->photo = Tk_FindPhoto(ti, bu_vls_addr(&tv->photo_name));
    }
    if (!tv->photo)
	return BRLCAD_ERROR;

    Tcl_Interp *ti = (Tcl_Interp *)dmp->i->dm_interp;
    if (!ti)
	return BRLCAD_ERROR;

    if (tv->photo_w != width || tv->photo_h != height) {
	(void)Tk_PhotoSetSize(ti, tv->photo, width, height);
	tv->photo_w = width;
	tv->photo_h = height;
    }

    Tk_PhotoImageBlock pb;
    pb.pixelPtr = tv->img_b;
    pb.width = width;
    pb.height = height;
    pb.pitch = width * 4;
    pb.pixelSize = 4;
    pb.offset[0] = 0;
    pb.offset[1] = 1;
    pb.offset[2] = 2;
    pb.offset[3] = 3;

    if (Tk_PhotoPutBlock(ti, tv->photo, &pb, 0, 0, width, height, TK_PHOTO_COMPOSITE_SET) != TCL_OK) {
	if (tkswrast_debug()) {
	    tkswrast_log("tkswrast_swap[%s]: Tk_PhotoPutBlock failed: %s\n",
		    bu_vls_addr(&dmp->i->dm_pathName), Tcl_GetStringResult(ti));
	}
	return BRLCAD_ERROR;
    }

    if (bu_vls_strlen(&tv->label_path) > 0) {
	struct bu_vls lcmd = BU_VLS_INIT_ZERO;
	bu_vls_printf(&lcmd,
		"if {[winfo exists %s]} {%s configure -image %s}",
		bu_vls_addr(&tv->label_path),
		bu_vls_addr(&tv->label_path),
		bu_vls_addr(&tv->photo_name));
	(void)Tcl_Eval(ti, bu_vls_addr(&lcmd));
	bu_vls_free(&lcmd);
    }

    (void)Tcl_Eval(ti, "update idletasks");

    if (tkswrast_debug() && width > 0 && height > 0) {
	struct bu_vls qcmd = BU_VLS_INIT_ZERO;
	bu_vls_printf(&qcmd,
		"set _iw [image width %s]; set _ih [image height %s]; set _iu [image inuse %s]; "
		"set _li \"\"; if {[winfo exists %s]} {set _li [%s cget -image]}; "
		"list $_iw $_ih $_iu $_li",
		bu_vls_addr(&tv->photo_name),
		bu_vls_addr(&tv->photo_name),
		bu_vls_addr(&tv->photo_name),
		bu_vls_addr(&tv->label_path),
		bu_vls_addr(&tv->label_path));
	if (Tcl_Eval(ti, bu_vls_addr(&qcmd)) == TCL_OK) {
	    tkswrast_log("tkswrast_photo[%s]: %s\n",
		    bu_vls_addr(&dmp->i->dm_pathName), Tcl_GetStringResult(ti));
	} else {
	    tkswrast_log("tkswrast_photo[%s]: query failed: %s\n",
		    bu_vls_addr(&dmp->i->dm_pathName), Tcl_GetStringResult(ti));
	}
	bu_vls_free(&qcmd);
	tkswrast_log("tkswrast_swap[%s]: px0=%u,%u,%u,%u\n",
		bu_vls_addr(&dmp->i->dm_pathName),
		(unsigned int)tv->img_b[0],
		(unsigned int)tv->img_b[1],
		(unsigned int)tv->img_b[2],
		(unsigned int)tv->img_b[3]);
    }
    return BRLCAD_OK;
}

static int
tkswrast_doevent(struct dm *dmp, void *UNUSED(vclientData), void *veventPtr)
{
    if (veventPtr) {
	dm_set_dirty(dmp, 1);
    }
    return TCL_OK;
}

static int
tkswrast_close(struct dm *dmp)
{
    struct tkswrast_vars *tv = (struct tkswrast_vars *)dmp->i->dm_udata;
    int (*orig_close)(struct dm *) = NULL;
    if (tv) {
	orig_close = tv->orig_close;
	if (tv->xtkwin)
	    Tk_DestroyWindow(tv->xtkwin);
	if (tv->img_b)
	    bu_free(tv->img_b, "tkswrast image buffer");
	if (BU_VLS_IS_INITIALIZED(&tv->photo_name))
	    bu_vls_free(&tv->photo_name);
	if (BU_VLS_IS_INITIALIZED(&tv->label_path))
	    bu_vls_free(&tv->label_path);
	bu_free(tv, "tkswrast vars");
	dmp->i->dm_udata = NULL;
    }
    if (orig_close)
	return orig_close(dmp);
    return BRLCAD_OK;
}

static int
tkswrast_viable(const char *UNUSED(dpy_string))
{
    return 1;
}

static struct dm *
tkswrast_open(void *ctx, void *vinterp, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;
    Tk_Window tkwin = Tk_MainWindow(interp);
    if (!tkwin)
	return DM_NULL;

    struct dm *dmp = dm_open(ctx, interp, "swrast", argc, argv);
    if (!dmp)
	return DM_NULL;

    dmp->i->dm_interp = (void *)interp;

    struct tkswrast_vars *tv = NULL;
    BU_ALLOC(tv, struct tkswrast_vars);
    bu_vls_init(&tv->photo_name);
    bu_vls_init(&tv->label_path);
    tv->orig_configureWin = dmp->i->dm_configureWin;
    tv->orig_close = dmp->i->dm_close;

    char *cp = strrchr(bu_vls_addr(&dmp->i->dm_pathName), '.');
    if (dmp->i->dm_top) {
	tv->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
		bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&dmp->i->dm_dName));
	tv->top = tv->xtkwin;
    } else {
	if (!cp || cp == bu_vls_addr(&dmp->i->dm_pathName)) {
	    tv->top = tkwin;
	    cp = (char *)bu_vls_addr(&dmp->i->dm_pathName);
	    if (*cp == '.')
		cp++;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;
	    bu_vls_strncpy(&top_vls, bu_vls_addr(&dmp->i->dm_pathName), cp - bu_vls_addr(&dmp->i->dm_pathName));
	    tv->top = Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	    cp++;
	}
	tv->xtkwin = Tk_CreateWindow(interp, tv->top, cp, (char *)NULL);
    }

    if (tkswrast_debug()) {
	tkswrast_log("tkswrast_open: path=%s top=%d\n", bu_vls_addr(&dmp->i->dm_pathName), dmp->i->dm_top);
    }

    tkswrast_log_startup_windows(dmp, "open-precreate");

    if (!tv->xtkwin) {
	bu_free(tv, "tkswrast vars");
	dm_close(dmp);
	return DM_NULL;
    }

    if (dmp->i->dm_top) {
	Tk_GeometryRequest(tv->xtkwin, dmp->i->dm_width, dmp->i->dm_height);
    } else {
	int pw = Tk_Width(tv->top);
	int ph = Tk_Height(tv->top);
	if (tkswrast_debug()) {
	    tkswrast_log("tkswrast_open: embedded parent initial size=%dx%d\n", pw, ph);
	}
	if (pw > 1 && ph > 1) {
	    struct swrast_vars *sv = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;
	    dmp->i->dm_width = pw;
	    dmp->i->dm_height = ph;
	    Tk_GeometryRequest(tv->xtkwin, pw, ph);
	    if (sv && sv->v) {
		sv->v->gv_width = pw;
		sv->v->gv_height = ph;
	    }
	}
    }
    Tk_SetWindowBackground(tv->xtkwin, tkswrast_black_pixel(tv->xtkwin));
    Tk_MakeWindowExist(tv->xtkwin);

    tkswrast_safe_image_name(&tv->photo_name, bu_vls_addr(&dmp->i->dm_pathName));
    bu_vls_printf(&tv->label_path, "%s.__img", bu_vls_addr(&dmp->i->dm_pathName));

    {
	struct bu_vls tcmd = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tcmd, "if {[catch {image create photo %s}]} {image create photo %s}", bu_vls_addr(&tv->photo_name), bu_vls_addr(&tv->photo_name));
	if (Tcl_Eval(interp, bu_vls_addr(&tcmd)) != TCL_OK && tkswrast_debug()) {
	    tkswrast_log("tkswrast_open: photo create failed: %s\n", Tcl_GetStringResult(interp));
	}
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd, "if {![winfo exists %s]} {label %s -image %s -bd 0 -highlightthickness 0 -background black}", bu_vls_addr(&tv->label_path), bu_vls_addr(&tv->label_path), bu_vls_addr(&tv->photo_name));
	if (Tcl_Eval(interp, bu_vls_addr(&tcmd)) != TCL_OK && tkswrast_debug()) {
	    tkswrast_log("tkswrast_open: label create failed: %s\n", Tcl_GetStringResult(interp));
	}
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd, "set _bt [bindtags %s]; if {[lsearch -exact $_bt %s] < 0} {bindtags %s [linsert $_bt end-1 %s]}", bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	if (Tcl_Eval(interp, bu_vls_addr(&tcmd)) != TCL_OK && tkswrast_debug()) {
	    tkswrast_log("tkswrast_open: bindtags failed: %s\n", Tcl_GetStringResult(interp));
	}
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd, "bind %s <Expose> {catch {%s dirty 1}; catch {%s configure}}", bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd, "bind %s <Configure> {catch {%s dirty 1}; catch {%s configure}}", bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd, "bind %s <Expose> {catch {%s dirty 1}; catch {%s configure}}", bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd,
		"bind %s <ButtonPress> {event generate %s <ButtonPress> -x %%x -y %%y -state %%s -button %%b; break}",
		bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd,
		"bind %s <ButtonRelease> {event generate %s <ButtonRelease> -x %%x -y %%y -state %%s -button %%b; break}",
		bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd,
		"bind %s <Motion> {event generate %s <Motion> -x %%x -y %%y -state %%s; break}",
		bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd,
		"bind %s <B1-Motion> {event generate %s <B1-Motion> -x %%x -y %%y -state %%s; break}",
		bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd,
		"bind %s <B2-Motion> {event generate %s <B2-Motion> -x %%x -y %%y -state %%s; break}",
		bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd,
		"bind %s <B3-Motion> {event generate %s <B3-Motion> -x %%x -y %%y -state %%s; break}",
		bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd, "place %s -in %s -x 0 -y 0 -relwidth 1 -relheight 1", bu_vls_addr(&tv->label_path), bu_vls_addr(&dmp->i->dm_pathName));
	if (Tcl_Eval(interp, bu_vls_addr(&tcmd)) != TCL_OK && tkswrast_debug()) {
	    tkswrast_log("tkswrast_open: label place failed: %s\n", Tcl_GetStringResult(interp));
	}
	bu_vls_trunc(&tcmd, 0);
	bu_vls_printf(&tcmd, "raise %s", bu_vls_addr(&tv->label_path));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcmd));
	bu_vls_free(&tcmd);
    }

    tv->photo = Tk_FindPhoto(interp, bu_vls_addr(&tv->photo_name));
    tv->photo_ready = (tv->photo != NULL);

    dmp->i->dm_udata = (void *)tv;
    dmp->i->dm_id = Tk_WindowId(tv->xtkwin);
    bu_vls_sprintf(&dmp->i->dm_tkName, "%s", Tk_Name(tv->xtkwin));

    dmp->i->dm_close = tkswrast_close;
    dmp->i->dm_viable = tkswrast_viable;
    dmp->i->dm_configureWin = tkswrast_configureWin;
    dmp->i->dm_SwapBuffers = tkswrast_SwapBuffers;
    dmp->i->dm_doevent = tkswrast_doevent;
    dmp->i->dm_graphical = 1;
    dmp->i->graphics_system = "Tk";
    dmp->i->dm_name = "tkswrast";
    dmp->i->dm_lname = "Tk OSMesa swrast graphics";

    dm_set_zbuffer(dmp, 1);
    {
	struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
	if (mvars) {
	    mvars->adaptive_zclip = 1;
	    mvars->adaptive_zclip_factor = 2.0;
	    mvars->adaptive_zclip_min = 100.0;
	    mvars->adaptive_zclip_max = 5000.0;
	}
    }

    if (dmp->i->dm_top)
	Tk_MapWindow(tv->xtkwin);
    if (tkswrast_debug()) {
	tkswrast_log("tkswrast_open: child mapped size=%dx%d parent size=%dx%d\n",
		Tk_Width(tv->xtkwin), Tk_Height(tv->xtkwin),
		Tk_Width(tv->top), Tk_Height(tv->top));
	tkswrast_log_tk_hierarchy(dmp);
	tkswrast_log_startup_windows(dmp, "open-postmap");
    }
    (void)dmp->i->dm_configureWin(dmp, 1);

    /* Ensure each embedded Tk DM gets configure callbacks, matching the
     * expected Tk-driven resize behavior used by other windowed DMs. */
    {
	Tcl_Interp *ti = (Tcl_Interp *)dmp->i->dm_interp;
	if (ti) {
	    struct bu_vls tcmd = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&tcmd, "bind %s <Configure> {catch {%%W configure}}", bu_vls_addr(&dmp->i->dm_pathName));
	    (void)Tcl_Eval(ti, bu_vls_addr(&tcmd));
	    bu_vls_trunc(&tcmd, 0);
	    bu_vls_printf(&tcmd, "event generate %s <Configure>", bu_vls_addr(&dmp->i->dm_pathName));
	    (void)Tcl_Eval(ti, bu_vls_addr(&tcmd));
	    bu_vls_free(&tcmd);
	}
    }

    dm_set_dirty(dmp, 1);

    return dmp;
}

static struct dm_impl dm_tkswrast_impl;

struct dm dm_tkswrast = { DM_MAGIC, &dm_tkswrast_impl, 0 };

#ifdef DM_PLUGIN
static const struct dm_plugin pinfo = { DM_API, &dm_tkswrast };
extern "C" {
    COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info(void)
    {
	dm_tkswrast_impl.dm_open = tkswrast_open;
	dm_tkswrast_impl.dm_close = tkswrast_close;
	dm_tkswrast_impl.dm_viable = tkswrast_viable;
	dm_tkswrast_impl.dm_configureWin = tkswrast_configureWin;
	dm_tkswrast_impl.dm_SwapBuffers = tkswrast_SwapBuffers;
	dm_tkswrast_impl.dm_doevent = tkswrast_doevent;
	dm_tkswrast_impl.dm_graphical = 1;
	dm_tkswrast_impl.graphics_system = "Tk";
	dm_tkswrast_impl.dm_name = "tkswrast";
	dm_tkswrast_impl.dm_lname = "Tk OSMesa swrast graphics";
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
