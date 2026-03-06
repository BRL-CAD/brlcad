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
/** @file primitives/edvol.c
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

#define ECMD_VOL_CSIZE		13048	/* set voxel size */
#define ECMD_VOL_FSIZE		13049	/* set VOL file dimensions */
#define ECMD_VOL_THRESH_LO	13050	/* set VOL threshold (lo) */
#define ECMD_VOL_THRESH_HI	13051	/* set VOL threshold (hi) */
#define ECMD_VOL_FNAME		13052	/* set VOL file name */

void
rt_edit_vol_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_VOL_FNAME:
	    break;
	case ECMD_VOL_FSIZE:
	    break;
	case ECMD_VOL_CSIZE:
	case ECMD_VOL_THRESH_LO:
	case ECMD_VOL_THRESH_HI:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);

}

static void
vol_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_set_edflag(s, arg);

    switch (arg) {
	case ECMD_VOL_FNAME:
	    break;
	case ECMD_VOL_FSIZE:
	    break;
	case ECMD_VOL_CSIZE:
	case ECMD_VOL_THRESH_LO:
	case ECMD_VOL_THRESH_HI:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
    }

    // TODO - should we be calling this here?
    rt_edit_process(s);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}

struct rt_edit_menu_item vol_menu[] = {
    {"VOL MENU", NULL, 0 },
    {"File Name", vol_ed, ECMD_VOL_FNAME },
    {"File Size (X Y Z)", vol_ed, ECMD_VOL_FSIZE },
    {"Voxel Size (X Y Z)", vol_ed, ECMD_VOL_CSIZE },
    {"Threshold (low)", vol_ed, ECMD_VOL_THRESH_LO },
    {"Threshold (hi)", vol_ed, ECMD_VOL_THRESH_HI },
    { "", NULL, 0 }
};

struct rt_edit_menu_item *
rt_edit_vol_menu_item(const struct bn_tol *UNUSED(tol))
{
    return vol_menu;
}

/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Voxel (VOL) primitive              */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc vol_fname_params[] = {
    {
	"fname",              /* name         */
	"Filename",           /* label        */
	RT_EDIT_PARAM_STRING, /* type         */
	0,                    /* index (unused for STRING) */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	"name"                /* prim_field   */
    }
};

/* ECMD_VOL_FSIZE expects e_inpara=3: e_para[0]=X, e_para[1]=Y, e_para[2]=Z */
static const struct rt_edit_param_desc vol_fsize_params[] = {
    {
	"xdim",               /* name         */
	"X Dimension",        /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	1.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"count",              /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    },
    {
	"ydim",               /* name         */
	"Y Dimension",        /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	1,                    /* index        */
	1.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"count",              /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    },
    {
	"zdim",               /* name         */
	"Z Dimension",        /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	2,                    /* index        */
	1.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"count",              /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* ECMD_VOL_CSIZE expects e_inpara=3: e_para[0..2] = x,y,z cell size */
static const struct rt_edit_param_desc vol_csize_params[] = {
    {
	"cx",                 /* name         */
	"X Cell Size",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    },
    {
	"cy",                 /* name         */
	"Y Cell Size",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	1,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    },
    {
	"cz",                 /* name         */
	"Z Cell Size",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	2,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc vol_thresh_lo_params[] = {
    {
	"thresh_lo",          /* name         */
	"Low Threshold",      /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	255.0,                /* range_max    */
	"none",               /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc vol_thresh_hi_params[] = {
    {
	"thresh_hi",          /* name         */
	"High Threshold",     /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	255.0,                /* range_max    */
	"none",               /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc vol_cmds[] = {
    {
	ECMD_VOL_FNAME,       /* cmd_id       */
	"File Name",          /* label        */
	"data",               /* category     */
	1,                    /* nparam       */
	vol_fname_params,     /* params       */
	0,                    /* interactive  */
	10                    /* display_order */
    },
    {
	ECMD_VOL_FSIZE,       /* cmd_id       */
	"File Size (X Y Z)",  /* label        */
	"geometry",           /* category     */
	3,                    /* nparam       */
	vol_fsize_params,     /* params       */
	0,                    /* interactive  */
	20                    /* display_order */
    },
    {
	ECMD_VOL_CSIZE,       /* cmd_id       */
	"Voxel Size (X Y Z)", /* label        */
	"geometry",           /* category     */
	3,                    /* nparam       */
	vol_csize_params,     /* params       */
	1,                    /* interactive  */
	30                    /* display_order */
    },
    {
	ECMD_VOL_THRESH_LO,   /* cmd_id       */
	"Threshold (low)",    /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	vol_thresh_lo_params, /* params       */
	1,                    /* interactive  */
	40                    /* display_order */
    },
    {
	ECMD_VOL_THRESH_HI,   /* cmd_id       */
	"Threshold (hi)",     /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	vol_thresh_hi_params, /* params       */
	1,                    /* interactive  */
	50                    /* display_order */
    }
};

static const struct rt_edit_prim_desc vol_prim_desc = {
    "vol",                /* prim_type    */
    "Volumetric Data",    /* prim_label   */
    5,                    /* ncmd         */
    vol_cmds              /* cmds         */
};

const struct rt_edit_prim_desc *
rt_edit_vol_edit_desc(void)
{
    return &vol_prim_desc;
}

/* set voxel size */
void
ecmd_vol_csize(struct rt_edit *s)
{
    struct rt_vol_internal *vol = (struct rt_vol_internal *)s->es_int.idb_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_VOL_CK_MAGIC(vol);

    // Specified numerical input
    if (s->e_inpara) {
	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "x, y, and z cell sizes are required\n");
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return;
	} else {
	    VMOVE(vol->cellsize, s->e_para);
	    return;
	}
    }

    // XY coord (usually mouse) based scaling
    if (s->es_scale > 0.0) {
	VSCALE(vol->cellsize, vol->cellsize, s->es_scale);
	s->es_scale = 0.0;
    }
}

/* set file size */
int
ecmd_vol_fsize(struct rt_edit *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->es_int.idb_ptr;
    struct stat stat_buf;
    b_off_t need_size;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_VOL_CK_MAGIC(vol);

    if (s->e_inpara == 3) {
	if (stat(vol->name, &stat_buf)) {
	    bu_vls_printf(s->log_str, "Cannot get status of file %s\n", vol->name);
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	need_size = s->e_para[0] * s->e_para[1] * s->e_para[2] * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    bu_vls_printf(s->log_str, "File (%s) is too small, set file name first", vol->name);
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	vol->xdim = s->e_para[0];
	vol->ydim = s->e_para[1];
	vol->zdim = s->e_para[2];
    } else if (s->e_inpara > 0) {
	bu_vls_printf(s->log_str, "x, y, and z file sizes are required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
ecmd_vol_thresh_lo(struct rt_edit *s)
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

    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->es_int.idb_ptr;

    RT_VOL_CK_MAGIC(vol);

    size_t i = vol->lo;
    if (s->e_inpara) {
	i = s->e_para[0];
    } else if (s->es_scale > 0.0) {
	i = vol->lo * s->es_scale;
	if (i == vol->lo && s->es_scale > 1.0) {
	    i++;
	} else if (i == vol->lo && s->es_scale < 1.0) {
	    i--;
	}
    }

    if (i > 255)
	i = 255;

    vol->lo = i;

    return 0;
}

int
ecmd_vol_thresh_hi(struct rt_edit *s)
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

    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->es_int.idb_ptr;

    RT_VOL_CK_MAGIC(vol);

    size_t i = vol->hi;
    if (s->e_inpara) {
	i = s->e_para[0];
    } else if (s->es_scale > 0.0) {
	i = vol->hi * s->es_scale;
	if (i == vol->hi && s->es_scale > 1.0) {
	    i++;
	} else if (i == vol->hi && s->es_scale < 1.0) {
	    i--;
	}
    }

    if (i > 255)
	i = 255;

    vol->hi = i;

    return 0;
}

int
ecmd_vol_fname(struct rt_edit *s)
{
    struct rt_vol_internal *vol =
	(struct rt_vol_internal *)s->es_int.idb_ptr;
    char *fname = NULL;
    struct stat stat_buf;
    b_off_t need_size;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_VOL_CK_MAGIC(vol);

    const char *av[2] = {NULL};
    av[0] = vol->name;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_GET_FILENAME, BU_CLBK_DURING);
    if (f)
	(*f)(1, (const char **)av, d, &fname);

    if (fname) {
	if (stat(fname, &stat_buf)) {
	    // We were calling Tcl_SetResult here, which reset the result str, so zero out log_str
	    bu_vls_trunc(s->log_str, 0);
	    bu_vls_printf(s->log_str, "Cannot get status of file %s\n", fname);
	    f = NULL; d = NULL;
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	need_size = vol->xdim * vol->ydim * vol->zdim * sizeof(unsigned char);
	if (stat_buf.st_size < need_size) {
	    // We were calling Tcl_SetResult here, which reset the result str, so zero out log_str
	    bu_vls_trunc(s->log_str, 0);
	    bu_vls_printf(s->log_str, "File (%s) is too small, adjust the file size parameters first", fname);
	    f = NULL; d = NULL;
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}
	bu_strlcpy(vol->name, fname, RT_VOL_NAME_LEN);
    }

    return BRLCAD_OK;
}

static int
rt_edit_vol_pscale(struct rt_edit *s)
{
    /* ECMD_VOL_CSIZE needs 3 params (x, y, z); all others need exactly 1 */
    if (s->edit_flag != ECMD_VOL_CSIZE && s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (s->e_inpara) {
	if (s->e_para[0] <= 0.0) {
	    bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;
    }

    switch (s->edit_flag) {
	case ECMD_VOL_CSIZE:
	    ecmd_vol_csize(s);
	    break;
    };

    return 0;
}

int
rt_edit_vol_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case ECMD_VOL_FSIZE:
	    return ecmd_vol_fsize(s);
	case ECMD_VOL_THRESH_LO:
	    return ecmd_vol_thresh_lo(s);
	case ECMD_VOL_THRESH_HI:
	    return ecmd_vol_thresh_hi(s);
	case ECMD_VOL_FNAME:
	    return ecmd_vol_fname(s);
	case ECMD_VOL_CSIZE:
	    return rt_edit_vol_pscale(s);
	default:
	    return edit_generic(s);
    }
}

int
rt_edit_vol_edit_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_VOL_CSIZE:
	case ECMD_VOL_THRESH_LO:
	case ECMD_VOL_THRESH_HI:
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
