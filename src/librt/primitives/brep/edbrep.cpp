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

#include <cmath>
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

static bool
brep_finite3(fastf_t x, fastf_t y, fastf_t z)
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

static ON_BrepFace *
brep_selected_face(struct rt_edit *s)
{
    if (!s || !s->ipe_ptr || !s->es_int.idb_ptr)
	return NULL;

    struct rt_brep_edit *selection = (struct rt_brep_edit *)s->ipe_ptr;
    struct rt_brep_internal *internal = (struct rt_brep_internal *)s->es_int.idb_ptr;
    ON_Brep *brep = internal->brep;
    if (!brep || selection->face_index < 0 ||
	selection->face_index >= brep->m_F.Count())
	return NULL;

    return brep->Face(selection->face_index);
}

/*
 * Select a NURBS surface control vertex by face index and (i,j) indices.
 * e_para[0] = face_index (integer encoded as fastf_t)
 * e_para[1] = cv row index i
 * e_para[2] = cv column index j
 * e_inpara  = 3
 */
static int
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
	return BRLCAD_ERROR;
    }

    ON_Brep *brep = bip->brep;
    fastf_t face_input = s->e_para[0];
    if (!brep || !std::isfinite(face_input) || face_input < 0 ||
	face_input >= brep->m_F.Count() ||
	!EQUAL(face_input, std::trunc(face_input))) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_SELECT: invalid face index %g\n", face_input);
	return BRLCAD_ERROR;
    }
    int face_index = (int)face_input;

    ON_BrepFace *face = brep->Face(face_index);
    const ON_Surface *surf = face ? face->SurfaceOf() : NULL;
    const ON_NurbsSurface *ns = dynamic_cast<const ON_NurbsSurface *>(surf);
    if (!ns) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_SELECT: face %d does not have a NURBS surface\n",
		face_index);
	return BRLCAD_ERROR;
    }

    int num_rows = ns->m_cv_count[0];
    int num_cols = ns->m_cv_count[1];
    fastf_t cv_i_input = s->e_para[1], cv_j_input = s->e_para[2];
    if (!std::isfinite(cv_i_input) || !std::isfinite(cv_j_input) ||
	cv_i_input < 0 || cv_i_input >= num_rows ||
	cv_j_input < 0 || cv_j_input >= num_cols ||
	!EQUAL(cv_i_input, std::trunc(cv_i_input)) ||
	!EQUAL(cv_j_input, std::trunc(cv_j_input))) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_SELECT: invalid CV (%g,%g); "
		"face %d has %dx%d CVs\n",
		cv_i_input, cv_j_input, face_index, num_rows, num_cols);
	return BRLCAD_ERROR;
    }
    int cv_i = (int)cv_i_input, cv_j = (int)cv_j_input;

    ON_3dPoint cv;
    if (!ns->GetCV(cv_i, cv_j, cv)) {
	bu_vls_printf(s->log_str, "ECMD_BREP_SRF_SELECT: cannot read CV\n");
	return BRLCAD_ERROR;
    }

    b->face_index = face_index;
    b->srf_cv_i   = cv_i;
    b->srf_cv_j   = cv_j;

    /* Report the selected CV position. */
    bu_vls_printf(s->log_str,
	    "Selected brep face %d CV (%d,%d) at (%.9f, %.9f, %.9f)\n",
	    face_index, cv_i, cv_j,
	    cv.x * s->base2local,
	    cv.y * s->base2local,
	    cv.z * s->base2local);
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
    if (f) (*f)(0, NULL, d, NULL);
    return BRLCAD_OK;
}


/*
 * Translate the selected CV by a delta vector.
 * e_para[0..2] = (dx, dy, dz) in local units
 * e_inpara     = 3
 */
static int
ecmd_brep_srf_cv_move(struct rt_edit *s)
{
    struct rt_brep_internal *bip = (struct rt_brep_internal *)s->es_int.idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;

    ON_BrepFace *face = brep_selected_face(s);
    if (!face) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_MOVE: no valid CV selected "
		"(use select_surface_cv first)\n");
	return BRLCAD_ERROR;
    }
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_MOVE: dx dy dz required\n");
	return BRLCAD_ERROR;
    }

    fastf_t dx = s->e_para[0] * s->local2base;
    fastf_t dy = s->e_para[1] * s->local2base;
    fastf_t dz = s->e_para[2] * s->local2base;

    if (!brep_finite3(dx, dy, dz)) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_MOVE: finite dx dy dz required\n");
	return BRLCAD_ERROR;
    }

    ON_Brep *brep = bip->brep;
    int surface_index = face->m_si;
    int ret = brep_translate_scv(brep, surface_index, b->srf_cv_i, b->srf_cv_j,
	    dx, dy, dz);
    if (ret < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_MOVE: brep_translate_scv failed\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


/*
 * Set the selected CV to an absolute position.
 * e_para[0..2] = (x, y, z) in local units
 * e_inpara     = 3
 */
static int
ecmd_brep_srf_cv_set(struct rt_edit *s)
{
    struct rt_brep_internal *bip = (struct rt_brep_internal *)s->es_int.idb_ptr;
    RT_BREP_CK_MAGIC(bip);
    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;

    ON_BrepFace *face = brep_selected_face(s);
    if (!face) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: no valid CV selected "
		"(use select_surface_cv first)\n");
	return BRLCAD_ERROR;
    }
    if (!s->e_mvalid && s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: x y z required\n");
	return BRLCAD_ERROR;
    }

    ON_Brep *brep = bip->brep;
    int surface_index = face->m_si;

    /* Retrieve current position and compute the required delta. */
    const ON_Surface *surf = face->SurfaceOf();
    const ON_NurbsSurface *ns = dynamic_cast<const ON_NurbsSurface *>(surf);
    if (!ns) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: face %d surface is not NURBS\n",
		b->face_index);
	return BRLCAD_ERROR;
    }

    ON_3dPoint cv;
    if (!ns->GetCV(b->srf_cv_i, b->srf_cv_j, cv)) {
	bu_vls_printf(s->log_str, "ECMD_BREP_SRF_CV_SET: cannot read CV\n");
	return BRLCAD_ERROR;
    }

    fastf_t new_x = s->e_mvalid ? s->e_mparam[X] : s->e_para[0] * s->local2base;
    fastf_t new_y = s->e_mvalid ? s->e_mparam[Y] : s->e_para[1] * s->local2base;
    fastf_t new_z = s->e_mvalid ? s->e_mparam[Z] : s->e_para[2] * s->local2base;

    if (!brep_finite3(new_x, new_y, new_z)) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: finite x y z required\n");
	return BRLCAD_ERROR;
    }

    fastf_t dx = new_x - cv.x;
    fastf_t dy = new_y - cv.y;
    fastf_t dz = new_z - cv.z;

    int ret = brep_translate_scv(brep, surface_index, b->srf_cv_i, b->srf_cv_j,
	    dx, dy, dz);
    if (ret < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_BREP_SRF_CV_SET: brep_translate_scv failed\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


/* ================================================================== *
 * Public dispatch functions — must have C linkage to match the      *
 * extern "C" { EDIT_DECLARE_INTERFACE(brep) } in edtable.cpp.      *
 * ================================================================== */

} /* close the extern "C" block opened above */

static int
brep_mouse_target_selected_cv(struct rt_edit *s, const vect_t mousevec)
{
    struct rt_brep_internal *bip = (struct rt_brep_internal *)s->es_int.idb_ptr;
    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;
    RT_BREP_CK_MAGIC(bip);
    ON_BrepFace *face = brep_selected_face(s);
    if (!s->vp || !face) {
	bu_vls_printf(s->log_str, "BREP mouse CV edit requires a selected CV\n");
	return BRLCAD_ERROR;
    }

    const ON_NurbsSurface *ns =
	dynamic_cast<const ON_NurbsSurface *>(face->SurfaceOf());
    ON_3dPoint cv;
    if (!ns || !ns->GetCV(b->srf_cv_i, b->srf_cv_j, cv)) {
	bu_vls_printf(s->log_str, "BREP mouse CV edit cannot read selected CV\n");
	return BRLCAD_ERROR;
    }

    point_t cv_local, cv_model, pos_view, target_model, target_local;
    VSET(cv_local, cv.x, cv.y, cv.z);
    MAT4X3PNT(cv_model, s->e_mat, cv_local);
    MAT4X3PNT(pos_view, s->vp->gv_model2view, cv_model);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(target_model, s->vp->gv_view2model, pos_view);
    MAT4X3PNT(target_local, s->e_invmat, target_model);

    VMOVE(s->e_mparam, target_local);
    s->e_mvalid = 1;
    return BRLCAD_OK;
}

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
	    return ecmd_brep_srf_select(s);
	case ECMD_BREP_SRF_CV_MOVE:
	    /* Mouse coordinates are absolute; typed MOVE parameters are deltas. */
	    if (s->e_mvalid)
		return ecmd_brep_srf_cv_set(s);
	    else
		return ecmd_brep_srf_cv_move(s);
	case ECMD_BREP_SRF_CV_SET:
	    return ecmd_brep_srf_cv_set(s);
	default:
	    return edit_generic(s);
    }

    return 0;
}


extern "C" int
rt_edit_brep_edit_xy(struct rt_edit *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_BREP_SRF_SELECT:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_BREP_SRF_CV_MOVE:
	case ECMD_BREP_SRF_CV_SET:
	    return brep_mouse_target_selected_cv(s, mousevec);
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
	RT_EDIT_PARAM_INTEGER,      /* type         */
	0,                          /* index        */
	0.0,                        /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	NULL,                       /* units        */
	0, NULL, NULL,              /* enum (unused) */
	NULL                        /* prim_field   */
    },
    {
	"cv_i",                     /* name         */
	"CV Row (i)",               /* label        */
	RT_EDIT_PARAM_INTEGER,      /* type         */
	1,                          /* index        */
	0.0,                        /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	NULL,                       /* units        */
	0, NULL, NULL,              /* enum (unused) */
	NULL                        /* prim_field   */
    },
    {
	"cv_j",                     /* name         */
	"CV Column (j)",            /* label        */
	RT_EDIT_PARAM_INTEGER,      /* type         */
	2,                          /* index        */
	0.0,                        /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max    */
	NULL,                       /* units        */
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
	10                              /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_BREP_SRF_CV_MOVE,
	"Move Surface CV",
	"surface",
	1,
	brep_vector_param,
	1,
	20,
	NULL
    },
    {
	ECMD_BREP_SRF_CV_SET,
	"Set Surface CV Position",
	"surface",
	1,
	brep_point_param,
	1,
	30,
	NULL
    }
};

static const struct rt_edit_prim_desc brep_prim_desc = {
    "brep",         /* prim_type  */
    "BREP",         /* prim_label */
    3,              /* ncmd       */
    brep_cmds       /* cmds       */,
    0,                    /* nopt         */
    NULL                  /* opts         */
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
    if (!s || !vals || !s->ipe_ptr)
	return 0;

    struct rt_brep_edit *b = (struct rt_brep_edit *)s->ipe_ptr;

    switch (cmd_id) {
	case ECMD_BREP_SRF_SELECT:
	    /* Return the three integer indices as scalars. */
	    vals[0] = (fastf_t)b->face_index;
	    vals[1] = (fastf_t)b->srf_cv_i;
	    vals[2] = (fastf_t)b->srf_cv_j;
	    return 3;

	case ECMD_BREP_SRF_CV_MOVE:
	case ECMD_BREP_SRF_CV_SET: {
	    /* Return the current world-space position of the selected CV. */
	    ON_BrepFace *face = brep_selected_face(s);
	    if (!face)
		return 0;
	    const ON_NurbsSurface *ns =
		dynamic_cast<const ON_NurbsSurface *>(face->SurfaceOf());
	    ON_3dPoint cv;
	    if (!ns || !ns->GetCV(b->srf_cv_i, b->srf_cv_j, cv))
		return 0;
	    vals[0] = cv.x * s->base2local;
	    vals[1] = cv.y * s->base2local;
	    vals[2] = cv.z * s->base2local;
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
