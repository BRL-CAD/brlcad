/*                     E D B R E P . C P P
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
/** @file primitives/brep/edbrep.cpp
 *
 * Boundary Representation (BREP) solid editing via the rt_edit framework.
 *
 * Supported operations:
 *   ECMD_BREP_SRF_SELECT   — select a NURBS surface control vertex by
 *                            (face_index, cv_i, cv_j).
 *   ECMD_BREP_SRF_CV_MOVE  — translate the selected CV by (dx, dy, dz).
 *   ECMD_BREP_SRF_CV_SET   — place the selected CV at an absolute (x, y, z).
 *
 * NOTE: CV edits do not currently check for influence on trimming curves;
 * users should validate the brep after edits (e.g. using "brep <obj> info").
 */

#include "common.h"

#include <cstring>

#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep/util.h"

#include "../edit_private.h"

/*
 * ECMD numbers for brep editing operations.
 * ID_BREP == 37, so use the 37000 block.
 */
#define ECMD_BREP_SRF_SELECT    37010   /* select a surface CV by (face, i, j) */
#define ECMD_BREP_SRF_CV_MOVE   37011   /* translate selected CV by (dx,dy,dz) */
#define ECMD_BREP_SRF_CV_SET    37012   /* place selected CV at absolute (x,y,z) */

/* Selection state for a single NURBS surface control vertex. */
struct rt_brep_edit {
    int face_index;  /* selected face index (-1 = none) */
    int srf_cv_i;    /* CV row index (-1 = none) */
    int srf_cv_j;    /* CV column index (-1 = none) */
};

/* ------------------------------------------------------------------ *
 * Public interface — all functions must have C linkage so they
 * match the extern "C" { EDIT_DECLARE_INTERFACE(brep) } declarations
 * in edtable.cpp.
 * ------------------------------------------------------------------ */
extern "C" {

void *
rt_edit_brep_prim_edit_create(struct rt_edit *UNUSED(s))
{
    struct rt_brep_edit *b;
    BU_GET(b, struct rt_brep_edit);
    b->face_index = -1;
    b->srf_cv_i   = -1;
    b->srf_cv_j   = -1;
    return (void *)b;
}

void
rt_edit_brep_prim_edit_destroy(void *ptr)
{
    struct rt_brep_edit *b = (struct rt_brep_edit *)ptr;
    if (!b)
	return;
    BU_PUT(b, struct rt_brep_edit);
}

void
rt_edit_brep_prim_edit_reset(struct rt_edit *s)
{
    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;
    b->face_index = -1;
    b->srf_cv_i   = -1;
    b->srf_cv_j   = -1;
}

void
rt_edit_brep_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_BREP_SRF_SELECT:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	case ECMD_BREP_SRF_CV_MOVE:
	case ECMD_BREP_SRF_CV_SET:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	default:
	    break;
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}


/* ------------------------------------------------------------------ *
 * Low-level helpers
 * ------------------------------------------------------------------ */

/*
 * Select a NURBS surface control vertex by face index and (i,j) indices.
 * e_para[0] = face_index (integer encoded as fastf_t)
 * e_para[1] = cv row index i
 * e_para[2] = cv column index j
 * e_inpara  = 3
 */
static void
ecmd_brep_srf_select(struct rt_edit *s)
{
    struct rt_brep_internal *bip = (struct rt_brep_internal *)s->es_int.idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_SELECT: face_index cv_i cv_j required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    ON_Brep *brep = bip->brep;
    int face_index = (int)s->e_para[0];
    int cv_i       = (int)s->e_para[1];
    int cv_j       = (int)s->e_para[2];

    if (face_index < 0 || face_index >= brep->m_F.Count()) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_SELECT: face_index %d out of range [0,%d)\n",
		face_index, brep->m_F.Count());
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    ON_BrepFace *face = brep->Face(face_index);
    const ON_Surface *surf = face->SurfaceOf();
    const ON_NurbsSurface *ns = dynamic_cast<const ON_NurbsSurface *>(surf);
    if (!ns) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_SELECT: face %d does not have a NURBS surface\n",
		face_index);
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    int num_rows = ns->m_cv_count[0];
    int num_cols = ns->m_cv_count[1];
    if (cv_i < 0 || cv_i >= num_rows || cv_j < 0 || cv_j >= num_cols) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_SELECT: cv (%d,%d) out of range; "
		"face %d has %dx%d CVs\n",
		cv_i, cv_j, face_index, num_rows, num_cols);
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    b->face_index = face_index;
    b->srf_cv_i   = cv_i;
    b->srf_cv_j   = cv_j;

    /* Report the selected CV position. */
    double *cv = ns->CV(cv_i, cv_j);
    bu_vls_printf(s->log_str,
	    "Selected brep face %d CV (%d,%d) at (%.9f, %.9f, %.9f)\n",
	    face_index, cv_i, cv_j,
	    cv[0] * s->base2local,
	    cv[1] * s->base2local,
	    cv[2] * s->base2local);
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
    if (f) (*f)(0, NULL, d, NULL);
}


/*
 * Translate the selected CV by a delta vector.
 * e_para[0..2] = (dx, dy, dz) in local units
 * e_inpara     = 3
 */
static void
ecmd_brep_srf_cv_move(struct rt_edit *s)
{
    struct rt_brep_internal *bip = (struct rt_brep_internal *)s->es_int.idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (b->face_index < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_MOVE: no CV selected "
		"(use select_surface_cv first)\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_MOVE: dx dy dz required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    fastf_t dx = s->e_para[0] * s->local2base;
    fastf_t dy = s->e_para[1] * s->local2base;
    fastf_t dz = s->e_para[2] * s->local2base;

    ON_Brep *brep = bip->brep;
    int surface_index = brep->m_F[b->face_index].m_si;
    int ret = brep_translate_scv(brep, surface_index, b->srf_cv_i, b->srf_cv_j,
	    dx, dy, dz);
    if (ret < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_MOVE: brep_translate_scv failed\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
    }
}


/*
 * Set the selected CV to an absolute position.
 * e_para[0..2] = (x, y, z) in local units
 * e_inpara     = 3
 */
static void
ecmd_brep_srf_cv_set(struct rt_edit *s)
{
    struct rt_brep_internal *bip = (struct rt_brep_internal *)s->es_int.idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (b->face_index < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: no CV selected "
		"(use select_surface_cv first)\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: x y z required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    ON_Brep *brep = bip->brep;
    int surface_index = brep->m_F[b->face_index].m_si;

    /* Retrieve current position and compute the required delta. */
    ON_BrepFace *face = brep->Face(b->face_index);
    const ON_Surface *surf = face->SurfaceOf();
    const ON_NurbsSurface *ns = dynamic_cast<const ON_NurbsSurface *>(surf);
    if (!ns) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: face %d surface is not NURBS\n",
		b->face_index);
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    double *cv = ns->CV(b->srf_cv_i, b->srf_cv_j);

    fastf_t new_x = s->e_para[0] * s->local2base;
    fastf_t new_y = s->e_para[1] * s->local2base;
    fastf_t new_z = s->e_para[2] * s->local2base;

    fastf_t dx = new_x - cv[0];
    fastf_t dy = new_y - cv[1];
    fastf_t dz = new_z - cv[2];

    int ret = brep_translate_scv(brep, surface_index, b->srf_cv_i, b->srf_cv_j,
	    dx, dy, dz);
    if (ret < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: brep_translate_scv failed\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
    }
}


/* ================================================================== *
 * Public dispatch functions — must have C linkage to match the      *
 * extern "C" { EDIT_DECLARE_INTERFACE(brep) } in edtable.cpp.      *
 * ================================================================== */

} /* close the extern "C" block opened above */

extern "C" int
rt_edit_brep_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    edit_srot(s);
	    break;
	case ECMD_BREP_SRF_SELECT:
	    ecmd_brep_srf_select(s);
	    break;
	case ECMD_BREP_SRF_CV_MOVE:
	    ecmd_brep_srf_cv_move(s);
	    break;
	case ECMD_BREP_SRF_CV_SET:
	    ecmd_brep_srf_cv_set(s);
	    break;
	default:
	    return edit_generic(s);
    }

    return 0;
}


extern "C" int
rt_edit_brep_edit_xy(struct rt_edit *s, vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_BREP_SRF_SELECT:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case RT_PARAMS_EDIT_TRANS:
	case ECMD_BREP_SRF_CV_MOVE:
	case ECMD_BREP_SRF_CV_SET:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	default:
	    return edit_generic_xy(s, mousevec);
    }

    edit_abs_tra(s, pos_view);
    return 0;
}


/* ------------------------------------------------------------------ *
 * Descriptor
 * ------------------------------------------------------------------ */

/* SELECT: three integer indices (face_index, cv_i, cv_j). */
static const struct rt_edit_param_desc brep_select_params[] = {
    {
	"face_index",               /* name         */
	"Face Index",               /* label        */
	RT_EDIT_PARAM_SCALAR,       /* type         */
	0,                          /* index        */
	0.0,                        /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	"count",                    /* units        */
	0, NULL, NULL,              /* enum (unused) */
	NULL                        /* prim_field   */
    },
    {
	"cv_i",                     /* name         */
	"CV Row (i)",               /* label        */
	RT_EDIT_PARAM_SCALAR,       /* type         */
	1,                          /* index        */
	0.0,                        /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	"count",                    /* units        */
	0, NULL, NULL,              /* enum (unused) */
	NULL                        /* prim_field   */
    },
    {
	"cv_j",                     /* name         */
	"CV Column (j)",            /* label        */
	RT_EDIT_PARAM_SCALAR,       /* type         */
	2,                          /* index        */
	0.0,                        /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	"count",                    /* units        */
	0, NULL, NULL,              /* enum (unused) */
	NULL                        /* prim_field   */
    }
};

/* MOVE: translation delta vector (dx, dy, dz). */
static const struct rt_edit_param_desc brep_vector_param[] = {
    {
	"delta",                    /* name         */
	"Translation",              /* label        */
	RT_EDIT_PARAM_VECTOR,       /* type         */
	0,                          /* index        */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	"length",                   /* units        */
	0, NULL, NULL,              /* enum (unused) */
	NULL                        /* prim_field   */
    }
};

/* SET: absolute position (x, y, z). */
static const struct rt_edit_param_desc brep_point_param[] = {
    {
	"pos",                      /* name         */
	"Position",                 /* label        */
	RT_EDIT_PARAM_POINT,        /* type         */
	0,                          /* index        */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	"length",                   /* units        */
	0, NULL, NULL,              /* enum (unused) */
	NULL                        /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc brep_cmds[] = {
    {
	ECMD_BREP_SRF_SELECT,           /* cmd_id        */
	"Select Surface CV",            /* label         */
	"surface",                      /* category      */
	3,                              /* nparam        */
	brep_select_params,             /* params        */
	0,                              /* interactive   */
	10                              /* display_order */
    },
    {
	ECMD_BREP_SRF_CV_MOVE,
	"Move Surface CV",
	"surface",
	1,
	brep_vector_param,
	1,
	20
    },
    {
	ECMD_BREP_SRF_CV_SET,
	"Set Surface CV Position",
	"surface",
	1,
	brep_point_param,
	1,
	30
    }
};

static const struct rt_edit_prim_desc brep_prim_desc = {
    "brep",         /* prim_type  */
    "BREP",         /* prim_label */
    3,              /* ncmd       */
    brep_cmds       /* cmds       */
};

extern "C" const struct rt_edit_prim_desc *
rt_edit_brep_edit_desc(void)
{
    return &brep_prim_desc;
}


/* ------------------------------------------------------------------ *
 * get_params: return the current value(s) for the given cmd_id
 * ------------------------------------------------------------------ */

extern "C" int
rt_edit_brep_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    struct rt_brep_edit *b;
    struct rt_brep_internal *bip;

    if (!s || !vals)
	return 0;

    b   = (struct rt_brep_edit *)s->ipe_ptr;
    bip = (struct rt_brep_internal *)s->es_int.idb_ptr;

    switch (cmd_id) {
	case ECMD_BREP_SRF_SELECT:
	    /* Return the three integer indices as scalars. */
	    vals[0] = (fastf_t)b->face_index;
	    vals[1] = (fastf_t)b->srf_cv_i;
	    vals[2] = (fastf_t)b->srf_cv_j;
	    return 3;

	case ECMD_BREP_SRF_CV_MOVE:
	case ECMD_BREP_SRF_CV_SET:
	    /* Return the current world-space position of the selected CV. */
	    if (b->face_index < 0 || !bip || !bip->brep)
		return 0;
	    {
		ON_Brep *brep = bip->brep;
		if (b->face_index >= brep->m_F.Count())
		    return 0;
		ON_BrepFace *face = brep->Face(b->face_index);
		const ON_Surface *surf = face->SurfaceOf();
		const ON_NurbsSurface *ns =
		    dynamic_cast<const ON_NurbsSurface *>(surf);
		if (!ns)
		    return 0;
		double *cv = ns->CV(b->srf_cv_i, b->srf_cv_j);
		vals[0] = cv[0] * s->base2local;
		vals[1] = cv[1] * s->base2local;
		vals[2] = cv[2] * s->base2local;
		return 3;
	    }

	default:
	    return 0;
    }
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
