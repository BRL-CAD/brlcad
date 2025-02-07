/*                         E D D S P . C
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
/** @file primitives/eddsp.c
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

#define ECMD_DSP_FNAME		25056	/* set DSP file name */
#define ECMD_DSP_FSIZE		25057	/* set DSP file size */
#define ECMD_DSP_SCALE_X        25058	/* Scale DSP x size */
#define ECMD_DSP_SCALE_Y        25059	/* Scale DSP y size */
#define ECMD_DSP_SCALE_ALT      25060	/* Scale DSP Altitude size */

static void
dsp_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_solid_edit_set_edflag(s, -1);
    s->edit_flag = arg;

    switch (arg) {
	case ECMD_DSP_FNAME:
	    s->edit_flag = ECMD_DSP_FNAME;
	    break;
	case ECMD_DSP_FSIZE:
	    s->edit_flag = ECMD_DSP_FSIZE;  // Unimplemented.  Expects 2 parameters
	    break;
	case ECMD_DSP_SCALE_X:
	    s->edit_flag = ECMD_DSP_SCALE_X;
	    s->solid_edit_scale = 1;
	    break;
	case ECMD_DSP_SCALE_Y:
	    s->edit_flag = ECMD_DSP_SCALE_Y;
	    s->solid_edit_scale = 1;
	    break;
	case ECMD_DSP_SCALE_ALT:
	    s->edit_flag = ECMD_DSP_SCALE_ALT;
	    s->solid_edit_scale = 1;
	    break;
    }

    rt_solid_edit_process(s);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item dsp_menu[] = {
    {"DSP MENU", NULL, 0 },
    {"Name", dsp_ed, ECMD_DSP_FNAME },
    {"Set X", dsp_ed, ECMD_DSP_SCALE_X },
    {"Set Y", dsp_ed, ECMD_DSP_SCALE_Y },
    {"Set ALT", dsp_ed, ECMD_DSP_SCALE_ALT },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *
rt_solid_edit_dsp_menu_item(const struct bn_tol *UNUSED(tol))
{
    return dsp_menu;
}

static void
dsp_scale(struct rt_solid_edit *s, struct rt_dsp_internal *dsp, int idx)
{
    mat_t m, scalemat;

    RT_DSP_CK_MAGIC(dsp);

    MAT_IDN(m);

    if (s->e_mvalid) {
	bu_log("s->e_mvalid %g %g %g\n", V3ARGS(s->e_mparam));
    }

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    if (s->e_inpara > 0) {
	m[idx] = s->e_para[0];
	bu_log("Keyboard %g\n", s->e_para[0]);
    } else if (!ZERO(s->es_scale)) {
	m[idx] *= s->es_scale;
	bu_log("s->es_scale %g\n", s->es_scale);
	s->es_scale = 0.0;
    }

    bn_mat_xform_about_pnt(scalemat, m, s->e_keypoint);

    bn_mat_mul(m, dsp->dsp_stom, scalemat);
    MAT_COPY(dsp->dsp_stom, m);

    bn_mat_mul(m, scalemat, dsp->dsp_mtos);
    MAT_COPY(dsp->dsp_mtos, m);

}

int
ecmd_dsp_scale_x(struct rt_solid_edit *s)
{
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

    dsp_scale(s, (struct rt_dsp_internal *)s->es_int.idb_ptr, MSX);

    return 0;
}

int
ecmd_dsp_scale_y(struct rt_solid_edit *s)
{
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

    dsp_scale(s, (struct rt_dsp_internal *)s->es_int.idb_ptr, MSY);

    return 0;
}

int
ecmd_dsp_scale_alt(struct rt_solid_edit *s)
{
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

    dsp_scale(s, (struct rt_dsp_internal *)s->es_int.idb_ptr, MSZ);

    return 0;
}

int
ecmd_dsp_fname(struct rt_solid_edit *s)
{
    struct rt_dsp_internal *dsp =
	(struct rt_dsp_internal *)s->es_int.idb_ptr;
    const char *fname = NULL;
    struct stat stat_buf;
    b_off_t need_size;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_DSP_CK_MAGIC(dsp);

    const char *av[2] = {NULL};
    av[0] = bu_vls_cstr(&dsp->dsp_name);
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_GET_FILENAME, 0, BU_CLBK_DURING);
    if (f)
	(*f)(1, (const char **)av, d, &fname);

    if (!fname)
	return BRLCAD_OK;

    if (stat(fname, &stat_buf)) {
	bu_vls_printf(s->log_str, "Cannot get status of file %s\n", fname);
	f = NULL; d = NULL;
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    }

    need_size = dsp->dsp_xcnt * dsp->dsp_ycnt * 2;
    if (stat_buf.st_size < need_size) {
	bu_vls_printf(s->log_str, "File (%s) is too small, adjust the file size parameters first", fname);
	f = NULL; d = NULL;
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    }
    bu_vls_strcpy(&dsp->dsp_name, fname);

    return BRLCAD_OK;
}

int
rt_solid_edit_dsp_edit(struct rt_solid_edit *s)
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
	case ECMD_DSP_SCALE_X:
	    return ecmd_dsp_scale_x(s);
	case ECMD_DSP_SCALE_Y:
	    return ecmd_dsp_scale_y(s);
	case ECMD_DSP_SCALE_ALT:
	    return ecmd_dsp_scale_alt(s);
	case ECMD_DSP_FNAME:
	    if (ecmd_dsp_fname(s) != BRLCAD_OK)
		return -1;
	    break;
    };

    return 0;
}

int
rt_solid_edit_dsp_edit_xy(
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
	case ECMD_DSP_SCALE_X:
	case ECMD_DSP_SCALE_Y:
	case ECMD_DSP_SCALE_ALT:
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    rt_solid_edit_dsp_edit(s);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    rt_update_edit_absolute_tran(s, pos_view);
    rt_solid_edit_dsp_edit(s);

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
