/*                      E D A N N O T . C
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
/** @file primitives/annot/edannot.c
 *
 * Annotation (ANNOT) primitive editing via the rt_edit descriptor framework.
 *
 * The rt_annot_internal structure contains:
 *   V          — 3-D anchor point (origin in 2-D system).
 *   verts[]    — 2-D control points for the text label and leader segments.
 *   ant.count  — number of segments (currently only ANN_TSEG_MAGIC text segs).
 *
 * NOTE: BRL-CAD's current annotation representation has exactly one segment
 * type (ANN_TSEG_MAGIC / struct txt_seg).  Leader polyline vertices are
 * stored in verts[] with indices referenced from txt_seg::ref_pt.
 *
 * Modification via 'edit <annot_obj> <op>…' is the preferred path for
 * existing annotations.  The top-level 'annotate' command remains the
 * creation path and is left untouched.
 *
 * Supported operations:
 *   ECMD_ANNOT_SET_TEXT   — replace the label text of segment 0.
 *   ECMD_ANNOT_SET_POS    — move the 3-D anchor point V.
 *   ECMD_ANNOT_SET_TSEG_TXT_SIZE — set text size of segment 0.
 *   ECMD_ANNOT_VERT_MOVE  — move a 2-D control vertex by (du, dv).
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/magic.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"

/*
 * ID_ANNOT == 42, so use the 42000 block.
 */
#define ECMD_ANNOT_SET_TEXT          42010  /* set label text (seg 0) */
#define ECMD_ANNOT_SET_POS           42011  /* move 3-D anchor V */
#define ECMD_ANNOT_SET_TSEG_TXT_SIZE 42012  /* set txt_size of seg 0 */
#define ECMD_ANNOT_VERT_MOVE         42013  /* move a 2-D vertex by (du,dv) */

/* No per-edit state needed (stateless opset). */

static struct txt_seg *
_annot_first_tseg(struct rt_annot_internal *aip)
{
    if (!aip->ant.count || !aip->ant.segments)
	return NULL;
    for (size_t i = 0; i < aip->ant.count; i++) {
	uint32_t *magic = (uint32_t *)aip->ant.segments[i];
	if (*magic == ANN_TSEG_MAGIC)
	    return (struct txt_seg *)aip->ant.segments[i];
    }
    return NULL;
}


static void
ecmd_annot_set_text(struct rt_edit *s)
{
    struct rt_annot_internal *aip =
	(struct rt_annot_internal *)s->es_int.idb_ptr;
    RT_ANNOT_CK_MAGIC(aip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    const char *txt = (s->e_nstr > 0 && s->e_str[0][0]) ? s->e_str[0] : NULL;
    if (!txt) {
	bu_vls_printf(s->log_str,
		"ECMD_ANNOT_SET_TEXT: text string required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    struct txt_seg *tsg = _annot_first_tseg(aip);
    if (!tsg) {
	bu_vls_printf(s->log_str,
		"ECMD_ANNOT_SET_TEXT: no text segment found\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    bu_vls_trunc(&tsg->label, 0);
    bu_vls_strcat(&tsg->label, txt);
}

static void
ecmd_annot_set_pos(struct rt_edit *s)
{
    struct rt_annot_internal *aip =
	(struct rt_annot_internal *)s->es_int.idb_ptr;
    RT_ANNOT_CK_MAGIC(aip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_ANNOT_SET_POS: x y z required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    VSET(aip->V,
	 s->e_para[0] * s->local2base,
	 s->e_para[1] * s->local2base,
	 s->e_para[2] * s->local2base);
}

static void
ecmd_annot_set_tseg_txt_size(struct rt_edit *s)
{
    struct rt_annot_internal *aip =
	(struct rt_annot_internal *)s->es_int.idb_ptr;
    RT_ANNOT_CK_MAGIC(aip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str,
		"ECMD_ANNOT_SET_TSEG_TXT_SIZE: size required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    struct txt_seg *tsg = _annot_first_tseg(aip);
    if (!tsg) {
	bu_vls_printf(s->log_str,
		"ECMD_ANNOT_SET_TSEG_TXT_SIZE: no text segment found\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    tsg->txt_size = s->e_para[0];
}

static void
ecmd_annot_vert_move(struct rt_edit *s)
{
    struct rt_annot_internal *aip =
	(struct rt_annot_internal *)s->es_int.idb_ptr;
    RT_ANNOT_CK_MAGIC(aip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	/* expect: vert_index, du, dv */
	bu_vls_printf(s->log_str,
		"ECMD_ANNOT_VERT_MOVE: vert_index du dv required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    int vi = (int)s->e_para[0];
    if (vi < 0 || (size_t)vi >= aip->vert_count) {
	bu_vls_printf(s->log_str,
		"ECMD_ANNOT_VERT_MOVE: vertex %d out of range [0,%zu)\n",
		vi, aip->vert_count);
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    aip->verts[vi][0] += s->e_para[1] * s->local2base;
    aip->verts[vi][1] += s->e_para[2] * s->local2base;
}


/* ================================================================== *
 * Public interface                                                    *
 * ================================================================== */

void *
rt_edit_annot_prim_edit_create(struct rt_edit *UNUSED(s))
{
    return NULL;
}

void
rt_edit_annot_prim_edit_destroy(void *UNUSED(ptr))
{
}

void
rt_edit_annot_prim_edit_reset(struct rt_edit *UNUSED(s))
{
}

void
rt_edit_annot_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);
    s->edit_mode = RT_PARAMS_EDIT_TRANS;

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f) (*f)(0, NULL, d, &flag);
}

int
rt_edit_annot_edit(struct rt_edit *s)
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
	case ECMD_ANNOT_SET_TEXT:
	    ecmd_annot_set_text(s);
	    break;
	case ECMD_ANNOT_SET_POS:
	    ecmd_annot_set_pos(s);
	    break;
	case ECMD_ANNOT_SET_TSEG_TXT_SIZE:
	    ecmd_annot_set_tseg_txt_size(s);
	    break;
	case ECMD_ANNOT_VERT_MOVE:
	    ecmd_annot_vert_move(s);
	    break;
	default:
	    return edit_generic(s);
    }
    return 0;
}

int
rt_edit_annot_edit_xy(struct rt_edit *s, vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	default:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
    }
    edit_abs_tra(s, pos_view);
    return 0;
}


/* ------------------------------------------------------------------ *
 * Descriptor
 * ------------------------------------------------------------------ */

static const struct rt_edit_param_desc annot_text_param[] = {
    { "text", "Label text", RT_EDIT_PARAM_STRING, 0,
      0.0, 0.0, "", 0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc annot_pos_param[] = {
    { "pos", "Anchor position (3-D)", RT_EDIT_PARAM_POINT, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc annot_txtsize_param[] = {
    { "txt_size", "Text size", RT_EDIT_PARAM_SCALAR, 0,
      0.0, RT_EDIT_PARAM_NO_LIMIT, "length", 0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc annot_vert_move_params[] = {
    { "vert_index", "Vertex index",  RT_EDIT_PARAM_SCALAR, 0,
      0.0, RT_EDIT_PARAM_NO_LIMIT, "count", 0, NULL, NULL, NULL },
    { "delta",      "2-D delta (du,dv)", RT_EDIT_PARAM_VECTOR, 1,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_cmd_desc annot_cmds[] = {
    { ECMD_ANNOT_SET_TEXT,          "Set Text",            "text",   1, annot_text_param,        1, 10 },
    { ECMD_ANNOT_SET_POS,           "Set Anchor Position", "layout", 1, annot_pos_param,          1, 20 },
    { ECMD_ANNOT_SET_TSEG_TXT_SIZE, "Set Text Size",       "text",   1, annot_txtsize_param,      1, 30 },
    { ECMD_ANNOT_VERT_MOVE,         "Move Vertex",         "layout", 2, annot_vert_move_params,   1, 40 }
};

static const struct rt_edit_prim_desc annot_prim_desc = {
    "annot", "Annotation", 4, annot_cmds
};

const struct rt_edit_prim_desc *
rt_edit_annot_edit_desc(void)
{
    return &annot_prim_desc;
}


int
rt_edit_annot_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    if (!s || !vals) return 0;

    struct rt_annot_internal *aip =
	(struct rt_annot_internal *)s->es_int.idb_ptr;

    switch (cmd_id) {
	case ECMD_ANNOT_SET_POS:
	    vals[0] = aip->V[0] * s->base2local;
	    vals[1] = aip->V[1] * s->base2local;
	    vals[2] = aip->V[2] * s->base2local;
	    return 3;

	case ECMD_ANNOT_SET_TSEG_TXT_SIZE: {
	    struct txt_seg *tsg = _annot_first_tseg(aip);
	    if (!tsg) return 0;
	    vals[0] = tsg->txt_size;
	    return 1;
	}

	default:
	    return 0;
    }
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
