/*                         E D V O L . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
 */
/** @file mged/primitives/edvol.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include <sys/stat.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./mged_functab.h"
#include "./edvol.h"

extern const char * get_file_name(struct mged_state *s, char *str);

static void
vol_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;

    switch (arg) {
	case MENU_VOL_FNAME:
	    es_edflag = ECMD_VOL_FNAME;
	    break;
	case MENU_VOL_FSIZE:
	    es_edflag = ECMD_VOL_FSIZE;
	    break;
	case MENU_VOL_CSIZE:
	    es_edflag = ECMD_VOL_CSIZE;
	    break;
	case MENU_VOL_THRESH_LO:
	    es_edflag = ECMD_VOL_THRESH_LO;
	    break;
	case MENU_VOL_THRESH_HI:
	    es_edflag = ECMD_VOL_THRESH_HI;
	    break;
    }

    sedit(s);
    set_e_axes_pos(s, 1);
}

struct menu_item vol_menu[] = {
    {"VOL MENU", NULL, 0 },
    {"File Name", vol_ed, MENU_VOL_FNAME },
    {"File Size (X Y Z)", vol_ed, MENU_VOL_FSIZE },
    {"Voxel Size (X Y Z)", vol_ed, MENU_VOL_CSIZE },
    {"Threshold (low)", vol_ed, MENU_VOL_THRESH_LO },
    {"Threshold (hi)", vol_ed, MENU_VOL_THRESH_HI },
    { "", NULL, 0 }
};

struct menu_item *
mged_vol_menu_item(const struct bn_tol *UNUSED(tol))
{
    return vol_menu;
}

/* scale voxel size */
void
menu_vol_csize(struct mged_state *s)
{
    bu_log("s->edit_state.es_scale = %g\n", s->edit_state.es_scale);
}

/* set voxel size */
void
ecmd_vol_csize(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;

    RT_VOL_CK_MAGIC(vol);

    if (inpara == 3) {
	VMOVE(vol->cellsize, es_para);
    } else if (inpara > 0 && inpara != 3) {
	Tcl_AppendResult(s->interp, "x, y, and z cell sizes are required\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (s->edit_state.es_scale > 0.0) {
	VSCALE(vol->cellsize, vol->cellsize, s->edit_state.es_scale);
	s->edit_state.es_scale = 0.0;
    }
}

/* set file size */
void
ecmd_vol_fsize(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;
    struct stat stat_buf;
    b_off_t need_size;

    RT_VOL_CK_MAGIC(vol);

    if (inpara == 3) {
	if (stat(vol->name, &stat_buf)) {
	    Tcl_AppendResult(s->interp, "Cannot get status of file ", vol->name, (char *)NULL);
	    mged_print_result(s, TCL_ERROR);
	    return;
	}
	need_size = es_para[0] * es_para[1] * es_para[2] * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    Tcl_AppendResult(s->interp, "File (", vol->name,
		    ") is too small, set file name first", (char *)NULL);
	    mged_print_result(s, TCL_ERROR);
	    return;
	}
	vol->xdim = es_para[0];
	vol->ydim = es_para[1];
	vol->zdim = es_para[2];
    } else if (inpara > 0) {
	Tcl_AppendResult(s->interp, "x, y, and z file sizes are required\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }
}

void
ecmd_vol_thresh_lo(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;

    RT_VOL_CK_MAGIC(vol);

    size_t i = vol->lo;
    if (inpara) {
	i = es_para[0];
    } else if (s->edit_state.es_scale > 0.0) {
	i = vol->lo * s->edit_state.es_scale;
	if (i == vol->lo && s->edit_state.es_scale > 1.0) {
	    i++;
	} else if (i == vol->lo && s->edit_state.es_scale < 1.0) {
	    i--;
	}
    }

    if (i > 255)
	i = 255;

    vol->lo = i;
}

void
ecmd_vol_thresh_hi(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;

    RT_VOL_CK_MAGIC(vol);

    size_t i = vol->hi;
    if (inpara) {
	i = es_para[0];
    } else if (s->edit_state.es_scale > 0.0) {
	i = vol->hi * s->edit_state.es_scale;
	if (i == vol->hi && s->edit_state.es_scale > 1.0) {
	    i++;
	} else if (i == vol->hi && s->edit_state.es_scale < 1.0) {
	    i--;
	}
    }

    if (i > 255)
	i = 255;

    vol->hi = i;
}

void
ecmd_vol_fname(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;
    const char *fname;
    struct stat stat_buf;
    b_off_t need_size;

    RT_VOL_CK_MAGIC(vol);

    fname = get_file_name(s, vol->name);
    if (fname) {
	struct bu_vls message = BU_VLS_INIT_ZERO;

	if (stat(fname, &stat_buf)) {
	    bu_vls_printf(&message, "Cannot get status of file %s\n", fname);
	    Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
	    bu_vls_free(&message);
	    mged_print_result(s, TCL_ERROR);
	    return;
	}
	need_size = vol->xdim * vol->ydim * vol->zdim * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    bu_vls_printf(&message, "File (%s) is too small, adjust the file size parameters first", fname);
	    Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
	    bu_vls_free(&message);
	    mged_print_result(s, TCL_ERROR);
	    return;
	}
	bu_strlcpy(vol->name, fname, RT_VOL_NAME_LEN);
    }
}

static int
mged_vol_pscale(struct mged_state *s, int mode)
{
    if (inpara > 1) {
	Tcl_AppendResult(s->interp, "ERROR: only one argument needed\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    if (es_para[0] <= 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    switch (mode) {
	case MENU_VOL_CSIZE:
	    menu_vol_csize(s);
	    break;
    };

    return 0;
}

int
mged_vol_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    mged_generic_sscale(s, &s->edit_state.es_int);
	    break;
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->edit_state.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->edit_state.es_int);
	    break;
	case PSCALE:
	    mged_vol_pscale(s, es_menu);
	    break;
	case ECMD_VOL_CSIZE:
	    ecmd_vol_csize(s);
	    break;
	case ECMD_VOL_FSIZE:
	    ecmd_vol_fsize(s);
	    break;
	case ECMD_VOL_THRESH_LO:
	    ecmd_vol_thresh_lo(s);
	    break;
	case ECMD_VOL_THRESH_HI:
	    ecmd_vol_thresh_hi(s);
	    break;
	case ECMD_VOL_FNAME:
	    ecmd_vol_fname(s);
	    break;
    }

    return 0;
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
