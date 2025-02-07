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
/** @file primitives/edebm.c
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

#include "../edit_private.h"

#define ECMD_EBM_FNAME		12053	/* set EBM file name */
#define ECMD_EBM_FSIZE		12054	/* set EBM file size */
#define ECMD_EBM_HEIGHT		12055	/* set EBM extrusion depth */

static void
ebm_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_DEFAULT);
    s->edit_flag = arg;

    if (arg == ECMD_EBM_HEIGHT)
	s->solid_edit_scale = 1;

    rt_solid_edit_process(s);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item ebm_menu[] = {
    {"EBM MENU", NULL, 0 },
    {"File Name", ebm_ed, ECMD_EBM_FNAME },
    {"File Size (W N)", ebm_ed, ECMD_EBM_FSIZE },
    {"Extrude Depth", ebm_ed, ECMD_EBM_HEIGHT },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *
rt_solid_edit_ebm_menu_item(const struct bn_tol *UNUSED(tol))
{
    return ebm_menu;
}

int
ecmd_ebm_fsize(struct rt_solid_edit *s)
{
    bu_clbk_t f = NULL;
    void *d = NULL;
    if (s->e_inpara != 2) {
	bu_vls_printf(s->log_str, "ERROR: two arguments needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: X SIZE <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    } else if (s->e_para[1] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: Y SIZE <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct rt_ebm_internal *ebm =
	(struct rt_ebm_internal *)s->es_int.idb_ptr;
    struct stat stat_buf;
    b_off_t need_size;

    RT_EBM_CK_MAGIC(ebm);

    if (s->e_inpara == 2) {
	if (stat(ebm->name, &stat_buf)) {
	    bu_vls_printf(s->log_str, "Cannot get status of ebm data source %s", ebm->name);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	need_size = s->e_para[0] * s->e_para[1] * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    bu_vls_printf(s->log_str, "File (%s) is too small, set data source name first", ebm->name);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	ebm->xdim = s->e_para[0];
	ebm->ydim = s->e_para[1];
    } else if (s->e_inpara > 0) {
	bu_vls_printf(s->log_str, "width and length of data source are required\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
ecmd_ebm_fname(struct rt_solid_edit *s)
{
    struct rt_ebm_internal *ebm = (struct rt_ebm_internal *)s->es_int.idb_ptr;
    const char *fname = NULL;
    struct stat stat_buf;
    b_off_t need_size;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_EBM_CK_MAGIC(ebm);

    const char *av[2] = {NULL};
    av[0] = ebm->name;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_GET_FILENAME, BU_CLBK_DURING);
    if (f)
	(*f)(1, (const char **)av, d, &fname);

    if (fname) {
	if (stat(fname, &stat_buf)) {
	    // We were calling Tcl_SetResult here, which reset the result str, so zero out log_str
	    bu_vls_trunc(s->log_str, 0);
	    bu_vls_printf(s->log_str, "Cannot get status of file %s\n", fname);
	    f = NULL; d = NULL;
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	need_size = ebm->xdim * ebm->ydim * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    // We were calling Tcl_SetResult here, which reset the result str, so zero out log_str
	    bu_vls_trunc(s->log_str, 0);
	    bu_vls_printf(s->log_str, "File (%s) is too small, adjust the file size parameters first", fname);
	    f = NULL; d = NULL;
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	bu_strlcpy(ebm->name, fname, RT_EBM_NAME_LEN);
    }

    return BRLCAD_OK;
}

int
ecmd_ebm_height(struct rt_solid_edit *s)
{
    bu_clbk_t f = NULL;
    void *d = NULL;
    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    struct rt_ebm_internal *ebm =
	(struct rt_ebm_internal *)s->es_int.idb_ptr;

    RT_EBM_CK_MAGIC(ebm);

    if (s->e_inpara == 1)
	ebm->tallness = s->e_para[0];
    else if (s->e_inpara > 0) {
	bu_vls_printf(s->log_str, "extrusion depth required\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    } else if (s->es_scale > 0.0) {
	ebm->tallness *= s->es_scale;
	s->es_scale = 0.0;
    }

    return BRLCAD_OK;
}

int
rt_solid_edit_ebm_edit(struct rt_solid_edit *s)
{
    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    return rt_solid_edit_generic_sscale(s, &s->es_int);
	case RT_SOLID_EDIT_TRANS:
	    /* translate solid */
	    rt_solid_edit_generic_strans(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_ROT:
	    /* rot solid about vertex */
	    rt_solid_edit_generic_srot(s, &s->es_int);
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
rt_solid_edit_ebm_edit_xy(
	struct rt_solid_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	case RT_SOLID_EDIT_PSCALE:
	case ECMD_EBM_HEIGHT:
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    rt_update_edit_absolute_tran(s, pos_view);

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
