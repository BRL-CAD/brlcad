/*                         E D V O L . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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

#define ECMD_VOL_CSIZE		13048	/* set voxel size */
#define ECMD_VOL_FSIZE		13049	/* set VOL file dimensions */
#define ECMD_VOL_THRESH_LO	13050	/* set VOL threshold (lo) */
#define ECMD_VOL_THRESH_HI	13051	/* set VOL threshold (hi) */
#define ECMD_VOL_FNAME		13052	/* set VOL file name */

#define MENU_VOL_FNAME		13075
#define MENU_VOL_FSIZE		13076
#define MENU_VOL_CSIZE		13077
#define MENU_VOL_THRESH_LO	13078
#define MENU_VOL_THRESH_HI	13079

extern const char * get_file_name(struct mged_state *s, char *str);

static void
vol_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->s_edit->edit_menu = arg;

    switch (arg) {
	case MENU_VOL_FNAME:
	    mged_set_edflag(s, ECMD_VOL_FNAME);
	    break;
	case MENU_VOL_FSIZE:
	    mged_set_edflag(s, ECMD_VOL_FSIZE);
	    break;
	case MENU_VOL_CSIZE:
	    s->s_edit->edit_flag = ECMD_VOL_CSIZE;
	    s->s_edit->solid_edit_rotate = 0;
	    s->s_edit->solid_edit_translate = 0;
	    s->s_edit->solid_edit_scale = 1;
	    s->s_edit->solid_edit_pick = 0;
	    break;
	case MENU_VOL_THRESH_LO:
	    s->s_edit->edit_flag = ECMD_VOL_THRESH_LO;
	    s->s_edit->solid_edit_rotate = 0;
	    s->s_edit->solid_edit_translate = 0;
	    s->s_edit->solid_edit_scale = 1;
	    s->s_edit->solid_edit_pick = 0;
	    break;
	case MENU_VOL_THRESH_HI:
	    s->s_edit->edit_flag = ECMD_VOL_THRESH_HI;
	    s->s_edit->solid_edit_rotate = 0;
	    s->s_edit->solid_edit_translate = 0;
	    s->s_edit->solid_edit_scale = 1;
	    s->s_edit->solid_edit_pick = 0;
	    break;
    }

    sedit(s);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_state_clbk_get(&f, &d, s, 0, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
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
    bu_log("s->s_edit->es_scale = %g\n", s->s_edit->es_scale);
}

/* set voxel size */
void
ecmd_vol_csize(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->s_edit->es_int.idb_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_VOL_CK_MAGIC(vol);

    /* must convert to base units */
    s->s_edit->e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit->e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit->e_para[2] *= s->dbip->dbi_local2base;

    if (s->s_edit->e_inpara == 3) {
	VMOVE(vol->cellsize, s->s_edit->e_para);
    } else if (s->s_edit->e_inpara > 0 && s->s_edit->e_inpara != 3) {
	bu_vls_printf(s->s_edit->log_str, "x, y, and z cell sizes are required\n");
	mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    } else if (s->s_edit->es_scale > 0.0) {
	VSCALE(vol->cellsize, vol->cellsize, s->s_edit->es_scale);
	s->s_edit->es_scale = 0.0;
    }
}

/* set file size */
void
ecmd_vol_fsize(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->s_edit->es_int.idb_ptr;
    struct stat stat_buf;
    b_off_t need_size;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_VOL_CK_MAGIC(vol);

    if (s->s_edit->e_inpara == 3) {
	if (stat(vol->name, &stat_buf)) {
	    bu_vls_printf(s->s_edit->log_str, "Cannot get status of file %s\n", vol->name);
	    mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return;
	}
	need_size = s->s_edit->e_para[0] * s->s_edit->e_para[1] * s->s_edit->e_para[2] * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    bu_vls_printf(s->s_edit->log_str, "File (%s) is too small, set file name first", vol->name);
	    mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return;
	}
	vol->xdim = s->s_edit->e_para[0];
	vol->ydim = s->s_edit->e_para[1];
	vol->zdim = s->s_edit->e_para[2];
    } else if (s->s_edit->e_inpara > 0) {
	bu_vls_printf(s->s_edit->log_str, "x, y, and z file sizes are required\n");
	mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    }
}

int
ecmd_vol_thresh_lo(struct mged_state *s)
{
    if (s->s_edit->e_inpara != 1) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: only one argument needed\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit->e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->s_edit->es_int.idb_ptr;

    RT_VOL_CK_MAGIC(vol);

    size_t i = vol->lo;
    if (s->s_edit->e_inpara) {
	i = s->s_edit->e_para[0];
    } else if (s->s_edit->es_scale > 0.0) {
	i = vol->lo * s->s_edit->es_scale;
	if (i == vol->lo && s->s_edit->es_scale > 1.0) {
	    i++;
	} else if (i == vol->lo && s->s_edit->es_scale < 1.0) {
	    i--;
	}
    }

    if (i > 255)
	i = 255;

    vol->lo = i;

    return 0;
}

int
ecmd_vol_thresh_hi(struct mged_state *s)
{
    if (s->s_edit->e_inpara != 1) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: only one argument needed\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit->e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->s_edit->es_int.idb_ptr;

    RT_VOL_CK_MAGIC(vol);

    size_t i = vol->hi;
    if (s->s_edit->e_inpara) {
	i = s->s_edit->e_para[0];
    } else if (s->s_edit->es_scale > 0.0) {
	i = vol->hi * s->s_edit->es_scale;
	if (i == vol->hi && s->s_edit->es_scale > 1.0) {
	    i++;
	} else if (i == vol->hi && s->s_edit->es_scale < 1.0) {
	    i--;
	}
    }

    if (i > 255)
	i = 255;

    vol->hi = i;

    return 0;
}

void
ecmd_vol_fname(struct mged_state *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->s_edit->es_int.idb_ptr;
    const char *fname;
    struct stat stat_buf;
    b_off_t need_size;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_VOL_CK_MAGIC(vol);

    fname = get_file_name(s, vol->name);
    if (fname) {
	if (stat(fname, &stat_buf)) {
	    // We were calling Tcl_SetResult here, which reset the result str, so zero out log_str
	    bu_vls_trunc(s->s_edit->log_str, 0);
	    bu_vls_printf(s->s_edit->log_str, "Cannot get status of file %s\n", fname);
	    mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return;
	}
	need_size = vol->xdim * vol->ydim * vol->zdim * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    // We were calling Tcl_SetResult here, which reset the result str, so zero out log_str
	    bu_vls_trunc(s->s_edit->log_str, 0);
	    bu_vls_printf(s->s_edit->log_str, "File (%s) is too small, adjust the file size parameters first", fname);
	    mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return;
	}
	bu_strlcpy(vol->name, fname, RT_VOL_NAME_LEN);
    }
}

static int
mged_vol_pscale(struct mged_state *s, int mode)
{
    if (s->s_edit->e_inpara > 1) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: only one argument needed\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit->e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->s_edit->e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit->e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit->e_para[2] *= s->dbip->dbi_local2base;

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
	    return mged_generic_sscale(s, &s->s_edit->es_int);
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->s_edit->es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->s_edit->es_int);
	    break;
	case PSCALE:
	    return mged_vol_pscale(s, s->s_edit->edit_menu);
	case ECMD_VOL_CSIZE:
	    ecmd_vol_csize(s);
	    break;
	case ECMD_VOL_FSIZE:
	    ecmd_vol_fsize(s);
	    break;
	case ECMD_VOL_THRESH_LO:
	    return ecmd_vol_thresh_lo(s);
	case ECMD_VOL_THRESH_HI:
	    return ecmd_vol_thresh_hi(s);
	case ECMD_VOL_FNAME:
	    ecmd_vol_fname(s);
	    break;
    }

    return 0;
}

int
mged_vol_edit_xy(
	struct mged_state *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->s_edit->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (edflag) {
	case SSCALE:
	case PSCALE:
	case ECMD_VOL_CSIZE:
	case ECMD_VOL_THRESH_LO:
	case ECMD_VOL_THRESH_HI:
	    mged_generic_sscale_xy(s, mousevec);
	    mged_vol_edit(s, edflag);
	    return 0;
	case STRANS:
	    mged_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->s_edit->log_str, "%s: XY edit undefined in solid edit mode %d\n", MGED_OBJ[ip->idb_type].ft_label, edflag);
	    mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);
    mged_vol_edit(s, edflag);

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
