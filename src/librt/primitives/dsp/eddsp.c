/*                         E D D S P . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2026 United States Government as represented by
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
/*
 * Toggle the dsp_smooth flag.
 * e_para[0] = 0 to disable, non-zero to enable; e_inpara = 1.
 * (If e_inpara == 0 the current value is toggled.)
 */
#define ECMD_DSP_SET_SMOOTH     25061
/*
 * Switch data source type.
 * e_para[0] = 0 or RT_DSP_SRC_FILE ('f') for file data,
 *             1 or RT_DSP_SRC_OBJ  ('o') for in-database object data.
 * e_inpara = 1.
 */
#define ECMD_DSP_SET_DATASRC    25062

void
rt_edit_dsp_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_DSP_SCALE_X:
	case ECMD_DSP_SCALE_Y:
	case ECMD_DSP_SCALE_ALT:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    }

    rt_edit_process(s);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}

static void
dsp_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_dsp_set_edit_mode(s, arg);
}

struct rt_edit_menu_item dsp_menu[] = {
    {"DSP MENU", NULL, 0 },
    {"Name", dsp_ed, ECMD_DSP_FNAME },
    {"Set X", dsp_ed, ECMD_DSP_SCALE_X },
    {"Set Y", dsp_ed, ECMD_DSP_SCALE_Y },
    {"Set ALT", dsp_ed, ECMD_DSP_SCALE_ALT },
    {"Toggle Smooth", dsp_ed, ECMD_DSP_SET_SMOOTH },
    {"Set Data Source", dsp_ed, ECMD_DSP_SET_DATASRC },
    { "", NULL, 0 }
};

struct rt_edit_menu_item *
rt_edit_dsp_menu_item(const struct bn_tol *UNUSED(tol))
{
    return dsp_menu;
}

/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Displacement Map primitive          */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc dsp_fname_params[] = {
    {
	"fname",              /* name         */
	"Filename",           /* label        */
	RT_EDIT_PARAM_STRING, /* type         */
	0,                    /* index (unused for STRING) */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	"dsp_name"            /* prim_field   */
    }
};

static const struct rt_edit_param_desc dsp_scale_x_params[] = {
    {
	"xs",                 /* name         */
	"X Cell Size",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc dsp_scale_y_params[] = {
    {
	"ys",                 /* name         */
	"Y Cell Size",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc dsp_scale_alt_params[] = {
    {
	"alts",               /* name         */
	"Altitude Scale",     /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc dsp_smooth_params[] = {
    {
	"smooth",             /* name         */
	"Smooth",             /* label        */
	RT_EDIT_PARAM_BOOLEAN, /* type        */
	0,                    /* index        */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* DSP data source enum: 'f'=102 (file), 'o'=111 (object) */
static const char * const dsp_datasrc_labels[] = { "File", "Object" };
static const int dsp_datasrc_ids[] = { 'f', 'o' };

static const struct rt_edit_param_desc dsp_datasrc_params[] = {
    {
	"datasrc",            /* name         */
	"Data Source",        /* label        */
	RT_EDIT_PARAM_ENUM,   /* type         */
	0,                    /* index        */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	2,                    /* nenum        */
	dsp_datasrc_labels,   /* enum_labels  */
	dsp_datasrc_ids,      /* enum_ids     */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc dsp_cmds[] = {
    {
	ECMD_DSP_FNAME,       /* cmd_id       */
	"Name",               /* label        */
	"data",               /* category     */
	1,                    /* nparam       */
	dsp_fname_params,     /* params       */
	0,                    /* interactive  */
	10                    /* display_order */
    },
    {
	ECMD_DSP_SCALE_X,     /* cmd_id       */
	"Set X",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	dsp_scale_x_params,   /* params       */
	1,                    /* interactive  */
	20                    /* display_order */
    },
    {
	ECMD_DSP_SCALE_Y,     /* cmd_id       */
	"Set Y",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	dsp_scale_y_params,   /* params       */
	1,                    /* interactive  */
	30                    /* display_order */
    },
    {
	ECMD_DSP_SCALE_ALT,   /* cmd_id       */
	"Set ALT",            /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	dsp_scale_alt_params, /* params       */
	1,                    /* interactive  */
	40                    /* display_order */
    },
    {
	ECMD_DSP_SET_SMOOTH,  /* cmd_id       */
	"Toggle Smooth",      /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	dsp_smooth_params,    /* params       */
	0,                    /* interactive  */
	50                    /* display_order */
    },
    {
	ECMD_DSP_SET_DATASRC, /* cmd_id       */
	"Set Data Source",    /* label        */
	"data",               /* category     */
	1,                    /* nparam       */
	dsp_datasrc_params,   /* params       */
	0,                    /* interactive  */
	20                    /* display_order */
    }
};

static const struct rt_edit_prim_desc dsp_prim_desc = {
    "dsp",                /* prim_type    */
    "Displacement Map",   /* prim_label   */
    6,                    /* ncmd         */
    dsp_cmds              /* cmds         */
};

const struct rt_edit_prim_desc *
rt_edit_dsp_edit_desc(void)
{
    return &dsp_prim_desc;
}

static void
dsp_scale(struct rt_edit *s, struct rt_dsp_internal *dsp, int idx)
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
ecmd_dsp_scale_x(struct rt_edit *s)
{
    if (!s->e_inpara && s->es_scale <= 0.0) {
	return BRLCAD_OK;
    }
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (s->e_inpara && s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    dsp_scale(s, (struct rt_dsp_internal *)s->es_int.idb_ptr, MSX);

    return 0;
}

int
ecmd_dsp_scale_y(struct rt_edit *s)
{
    if (!s->e_inpara && s->es_scale <= 0.0) {
	return BRLCAD_OK;
    }
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (s->e_inpara && s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    dsp_scale(s, (struct rt_dsp_internal *)s->es_int.idb_ptr, MSY);

    return 0;
}

int
ecmd_dsp_scale_alt(struct rt_edit *s)
{
    if (!s->e_inpara && s->es_scale <= 0.0) {
	return BRLCAD_OK;
    }
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (s->e_inpara && s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    dsp_scale(s, (struct rt_dsp_internal *)s->es_int.idb_ptr, MSZ);

    return 0;
}

int
ecmd_dsp_fname(struct rt_edit *s)
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
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_GET_FILENAME, BU_CLBK_DURING);
    if (f)
	(*f)(1, (const char **)av, d, &fname);

    if (!fname)
	return BRLCAD_OK;

    if (stat(fname, &stat_buf)) {
	bu_vls_printf(s->log_str, "Cannot get status of file %s\n", fname);
	f = NULL; d = NULL;
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    }

    need_size = dsp->dsp_xcnt * dsp->dsp_ycnt * 2;
    if (stat_buf.st_size < need_size) {
	bu_vls_printf(s->log_str, "File (%s) is too small, adjust the file size parameters first", fname);
	f = NULL; d = NULL;
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    }
    bu_vls_strcpy(&dsp->dsp_name, fname);

    return BRLCAD_OK;
}

/* Toggle/set the smooth-normals flag.
 * e_para[0] = 0 → disable, non-zero → enable; e_inpara = 1.
 * With e_inpara == 0, the current value is toggled. */
static int
ecmd_dsp_set_smooth(struct rt_edit *s)
{
    struct rt_dsp_internal *dsp =
	(struct rt_dsp_internal *)s->es_int.idb_ptr;
    RT_DSP_CK_MAGIC(dsp);

    if (s->e_inpara >= 1) {
	dsp->dsp_smooth = (unsigned short)(!NEAR_ZERO(s->e_para[0], SMALL_FASTF) ? 1 : 0);
	s->e_inpara = 0;
    } else {
	/* toggle */
	dsp->dsp_smooth = dsp->dsp_smooth ? 0 : 1;
    }
    bu_vls_printf(s->log_str,
	    "ECMD_DSP_SET_SMOOTH: smooth=%u\n", dsp->dsp_smooth);
    return BRLCAD_OK;
}

/* Set the data source type.
 * e_para[0] = RT_DSP_SRC_FILE ('f'=102) or RT_DSP_SRC_OBJ ('o'=111); e_inpara = 1.
 * For convenience accept 0 for RT_DSP_SRC_FILE and 1 for RT_DSP_SRC_OBJ. */
static int
ecmd_dsp_set_datasrc(struct rt_edit *s)
{
    struct rt_dsp_internal *dsp =
	(struct rt_dsp_internal *)s->es_int.idb_ptr;
    RT_DSP_CK_MAGIC(dsp);

    if (!s->e_inpara || s->e_inpara < 1) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_DSP_SET_DATASRC: data source type required "
		"(0 or 'f'=file, 1 or 'o'=object)\n");
	return BRLCAD_ERROR;
    }
    int src = (int)s->e_para[0];
    char dsrc;
    if (src == 0 || src == RT_DSP_SRC_FILE)
	dsrc = RT_DSP_SRC_FILE;
    else if (src == 1 || src == RT_DSP_SRC_OBJ)
	dsrc = RT_DSP_SRC_OBJ;
    else {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_DSP_SET_DATASRC: invalid type %d "
		"(use 0/'f'=file or 1/'o'=object)\n", src);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    dsp->dsp_datasrc = dsrc;
    s->e_inpara = 0;
    bu_vls_printf(s->log_str,
	    "ECMD_DSP_SET_DATASRC: datasrc='%c'\n", dsp->dsp_datasrc);
    return BRLCAD_OK;
}

int
rt_edit_dsp_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    /* translate solid */
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    /* rot solid about vertex */
	    edit_srot(s);
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
	case ECMD_DSP_SET_SMOOTH:
	    return ecmd_dsp_set_smooth(s);
	case ECMD_DSP_SET_DATASRC:
	    return ecmd_dsp_set_datasrc(s);
	default:
	    return edit_generic(s);
    };

    return 0;
}

int
rt_edit_dsp_edit_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_DSP_FNAME:
	case ECMD_DSP_FSIZE:
	case ECMD_DSP_SCALE_X:
	case ECMD_DSP_SCALE_Y:
	case ECMD_DSP_SCALE_ALT:
	case ECMD_DSP_SET_SMOOTH:
	case ECMD_DSP_SET_DATASRC:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	default:
	    return edit_generic_xy(s, mousevec);
    }

    edit_abs_tra(s, pos_view);

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
