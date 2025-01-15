/*                         E D E B M . C
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
/** @file mged/primitives/edebm.c
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

#define ECMD_EBM_FNAME		12053	/* set EBM file name */
#define ECMD_EBM_FSIZE		12054	/* set EBM file size */
#define ECMD_EBM_HEIGHT		12055	/* set EBM extrusion depth */

#define MENU_EBM_FNAME		12080
#define MENU_EBM_FSIZE		12081
#define MENU_EBM_HEIGHT		12082

extern const char * get_file_name(struct mged_state *s, char *str);

static void
ebm_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    s->s_edit.edit_menu = arg;
    mged_set_edflag(s, -1);

    switch (arg) {
	case MENU_EBM_FNAME:
	    s->s_edit.edit_flag = ECMD_EBM_FNAME;
	    break;
	case MENU_EBM_FSIZE:
	    s->s_edit.edit_flag = ECMD_EBM_FSIZE;
	    break;
	case MENU_EBM_HEIGHT:
	    s->s_edit.edit_flag = ECMD_EBM_HEIGHT;
	    s->s_edit.solid_edit_scale = 1;
	    break;
    }

    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item ebm_menu[] = {
    {"EBM MENU", NULL, 0 },
    {"File Name", ebm_ed, MENU_EBM_FNAME },
    {"File Size (W N)", ebm_ed, MENU_EBM_FSIZE },
    {"Extrude Depth", ebm_ed, MENU_EBM_HEIGHT },
    { "", NULL, 0 }
};

struct menu_item *
mged_ebm_menu_item(const struct bn_tol *UNUSED(tol))
{
    return ebm_menu;
}

int
ecmd_ebm_fsize(struct mged_state *s)
{
    if (s->s_edit.e_inpara != 2) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: two arguments needed\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit.e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: X SIZE <= 0\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    } else if (s->s_edit.e_para[1] <= 0.0) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: Y SIZE <= 0\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    struct rt_ebm_internal *ebm =
	(struct rt_ebm_internal *)s->s_edit.es_int.idb_ptr;
    struct stat stat_buf;
    b_off_t need_size;

    RT_EBM_CK_MAGIC(ebm);

    if (s->s_edit.e_inpara == 2) {
	if (stat(ebm->name, &stat_buf)) {
	    bu_vls_printf(s->s_edit.log_str, "Cannot get status of ebm data source %s", ebm->name);
	    mged_print_result(s, TCL_ERROR);
	    return BRLCAD_ERROR;
	}
	need_size = s->s_edit.e_para[0] * s->s_edit.e_para[1] * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    bu_vls_printf(s->s_edit.log_str, "File (%s) is too small, set data source name first", ebm->name);
	    mged_print_result(s, TCL_ERROR);
	    return BRLCAD_ERROR;
	}
	ebm->xdim = s->s_edit.e_para[0];
	ebm->ydim = s->s_edit.e_para[1];
    } else if (s->s_edit.e_inpara > 0) {
	bu_vls_printf(s->s_edit.log_str, "width and length of data source are required\n");
	mged_print_result(s, TCL_ERROR);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
ecmd_ebm_fname(struct mged_state *s)
{
    struct rt_ebm_internal *ebm =
	(struct rt_ebm_internal *)s->s_edit.es_int.idb_ptr;
    const char *fname;
    struct stat stat_buf;
    b_off_t need_size;

    RT_EBM_CK_MAGIC(ebm);

    fname = get_file_name(s, ebm->name);
    if (fname) {
	struct bu_vls message = BU_VLS_INIT_ZERO;

	if (stat(fname, &stat_buf)) {
	    bu_vls_printf(&message, "Cannot get status of file %s\n", fname);
	    Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
	    bu_vls_free(&message);
	    mged_print_result(s, TCL_ERROR);
	    return BRLCAD_ERROR;
	}
	need_size = ebm->xdim * ebm->ydim * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    bu_vls_printf(&message, "File (%s) is too small, adjust the file size parameters first", fname);
	    Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
	    bu_vls_free(&message);
	    mged_print_result(s, TCL_ERROR);
	    return BRLCAD_ERROR;
	}
	bu_strlcpy(ebm->name, fname, RT_EBM_NAME_LEN);
    }

    return BRLCAD_OK;
}

int
ecmd_ebm_height(struct mged_state *s)
{
    if (s->s_edit.e_inpara != 1) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: only one argument needed\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }
    if (s->s_edit.e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    struct rt_ebm_internal *ebm =
	(struct rt_ebm_internal *)s->s_edit.es_int.idb_ptr;

    RT_EBM_CK_MAGIC(ebm);

    if (s->s_edit.e_inpara == 1)
	ebm->tallness = s->s_edit.e_para[0];
    else if (s->s_edit.e_inpara > 0) {
	bu_vls_printf(s->s_edit.log_str, "extrusion depth required\n");
	mged_print_result(s, TCL_ERROR);
	return BRLCAD_ERROR;
    } else if (s->s_edit.es_scale > 0.0) {
	ebm->tallness *= s->s_edit.es_scale;
	s->s_edit.es_scale = 0.0;
    }

    return BRLCAD_OK;
}

int
mged_ebm_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    return mged_generic_sscale(s, &s->s_edit.es_int);
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->s_edit.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->s_edit.es_int);
	    break;
	case ECMD_EBM_FSIZE:    /* set file size */
	    if (ecmd_ebm_fsize(s) != BRLCAD_OK)
		return -1;
	    break;

	case ECMD_EBM_FNAME:
	    if (ecmd_ebm_fname(s) != BRLCAD_OK)
		return -1;
	    break;
	case ECMD_EBM_HEIGHT:   /* set extrusion depth */
	    if (ecmd_ebm_height(s) != BRLCAD_OK)
		return -1;
	    break;
    }

    return 0;
}

int
mged_ebm_edit_xy(
	struct mged_state *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->s_edit.es_int;

    switch (edflag) {
	case SSCALE:
	case PSCALE:
	case ECMD_EBM_HEIGHT:
	    mged_generic_sscale_xy(s, mousevec);
	    mged_ebm_edit(s, edflag);
	    return 0;
	case STRANS:
	    mged_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->s_edit.log_str, "%s: XY edit undefined in solid edit mode %d\n", MGED_OBJ[ip->idb_type].ft_label, edflag);
	    mged_print_result(s, TCL_ERROR);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);
    mged_ebm_edit(s, edflag);

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
