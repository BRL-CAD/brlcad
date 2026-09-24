/*                        E D P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2026 United States Government as represented by
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
/** @file primitives/edpipe.c
 *
 * Functions -
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "rt/primitives/pipe.h"
#include "wdb.h"

#include "../edit_private.h"

#define ECMD_PIPE_SELECT	15028	/* Pick pipe point */
#define ECMD_PIPE_SPLIT		15029	/* Split a pipe segment into two */
#define ECMD_PIPE_PT_ADD	15030	/* Add a pipe point to end of pipe */
#define ECMD_PIPE_PT_INS	15031	/* Add a pipe point to start of pipe */
#define ECMD_PIPE_PT_DEL	15032	/* Delete a pipe point */
#define ECMD_PIPE_PT_MOVE	15033	/* Move a pipe point */

#define ECMD_PIPE_NEXT_PT	15062
#define ECMD_PIPE_PREV_PT	15063
#define ECMD_PIPE_PT_OD		15065
#define ECMD_PIPE_PT_ID		15066
#define ECMD_PIPE_SCALE_OD	15067
#define ECMD_PIPE_SCALE_ID	15068
#define ECMD_PIPE_PT_RADIUS	15073
#define ECMD_PIPE_SCALE_RADIUS	15074

static enum rt_constraint_edit_policy
pipe_edit_policy(void)
{
    return RT_CONSTRAINT_EDIT_SNAP;
}

static int
pipe_apply_cedit(struct rt_edit *s, const struct rt_constraint_edit_op *op)
{
    struct rt_constraint_edit_ctx ctx;
    struct rt_constraint_edit_result res;
    int ret;

    memset(&ctx, 0, sizeof(ctx));
    ctx.policy = pipe_edit_policy();
    rt_constraint_edit_result_init(&res);
    ret = rt_pipe_project_apply(&res, &s->es_int, op, &ctx);
    if (ret != BRLCAD_OK) {
        if (bu_vls_strlen(&res.summary) > 0)
            bu_vls_printf(s->log_str, "%s\n", bu_vls_addr(&res.summary));
    }
    rt_constraint_edit_result_free(&res);
    return ret;
}

C_DECL void *
rt_edit_pipe_prim_edit_create(struct rt_edit *UNUSED(s))
{
    struct rt_pipe_edit *p;
    BU_GET(p, struct rt_pipe_edit);

    p->es_pipe_pnt = NULL;

    return (void *)p;
}

C_DECL void
rt_edit_pipe_prim_edit_destroy(void *ptr)
{
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)ptr;
    if (!p)
	return;

    // Sanity
    p->es_pipe_pnt = NULL;

    BU_PUT(p, struct rt_pipe_edit);
}

C_DECL void
rt_edit_pipe_prim_edit_reset(struct rt_edit *s)
{
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    p->es_pipe_pnt = NULL;
}


C_DECL void
rt_edit_pipe_set_edit_mode(struct rt_edit *s, int mode)
{
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_PIPE_SELECT:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	case ECMD_PIPE_NEXT_PT:
	case ECMD_PIPE_PREV_PT:
	    rt_edit_process(s);
	    return;
	case ECMD_PIPE_PT_MOVE:
	    if (!p->es_pipe_pnt) {
		bu_vls_printf(s->log_str, "No Pipe Segment selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_PIPE_PT_OD:
	case ECMD_PIPE_PT_ID:
	case ECMD_PIPE_PT_RADIUS:
	    if (!p->es_pipe_pnt) {
		bu_vls_printf(s->log_str, "No Pipe Segment selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	case ECMD_PIPE_SCALE_OD:
	case ECMD_PIPE_SCALE_ID:
	case ECMD_PIPE_SCALE_RADIUS:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	case ECMD_PIPE_PT_ADD:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_PIPE_PT_INS:
	case ECMD_PIPE_SPLIT:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_PIPE_PT_DEL:
	    /* Deletion is handled inside ft_edit; trigger it immediately. */
	    rt_edit_process(s);
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
pipe_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_pipe_set_edit_mode(s, arg);
}

struct rt_edit_menu_item pipe_menu[] = {
    { "PIPE MENU", NULL, 0 },
    { "Select Point", pipe_ed, ECMD_PIPE_SELECT },
    { "Next Point", pipe_ed, ECMD_PIPE_NEXT_PT },
    { "Previous Point", pipe_ed, ECMD_PIPE_PREV_PT },
    { "Move Point", pipe_ed, ECMD_PIPE_PT_MOVE},
    { "Delete Point", pipe_ed, ECMD_PIPE_PT_DEL},
    { "Append Point", pipe_ed, ECMD_PIPE_PT_ADD},
    { "Prepend Point", pipe_ed, ECMD_PIPE_PT_INS},
    { "Split Segment", pipe_ed, ECMD_PIPE_SPLIT},
    { "Set Point OD", pipe_ed, ECMD_PIPE_PT_OD },
    { "Set Point ID", pipe_ed, ECMD_PIPE_PT_ID },
    { "Set Point Bend", pipe_ed, ECMD_PIPE_PT_RADIUS },
    { "Set Pipe OD", pipe_ed, ECMD_PIPE_SCALE_OD },
    { "Set Pipe ID", pipe_ed, ECMD_PIPE_SCALE_ID },
    { "Set Pipe Bend", pipe_ed, ECMD_PIPE_SCALE_RADIUS },
    { "", NULL, 0 }
};

C_DECL struct rt_edit_menu_item *
rt_edit_pipe_menu_item(const struct bn_tol *UNUSED(tol))
{
    return pipe_menu;
}

/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Pipe primitive                     */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc pipe_pt_od_params[] = {
    {
	"od",                 /* name         */
	"Outer Diameter",     /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc pipe_pt_id_params[] = {
    {
	"id",                 /* name         */
	"Inner Diameter",     /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc pipe_pt_radius_params[] = {
    {
	"bend_radius",        /* name         */
	"Bend Radius",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* POINT parameter used by ops that take a 3D location: SELECT, PT_MOVE,
 * PT_ADD, PT_INS, and SPLIT all supply the target world-space coordinate. */
static const struct rt_edit_param_desc pipe_point_params[] = {
    {
	"pt",                 /* name         */
	"Point X Y Z",        /* label        */
	RT_EDIT_PARAM_POINT,  /* type         */
	0,                    /* index        */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc pipe_cmds[] = {
    /* --- point selection / navigation -------------------------------- */
    {
	ECMD_PIPE_SELECT,     /* cmd_id       */
	"Select Point",       /* label        */
	"select",             /* category     */
	1,                    /* nparam       */
	pipe_point_params,    /* params       */
	1,                    /* interactive  */
	5                     /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_NEXT_PT,    /* cmd_id       */
	"Next Point",         /* label        */
	"select",             /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	6                     /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_PREV_PT,    /* cmd_id       */
	"Previous Point",     /* label        */
	"select",             /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	7                     /* display_order */,
	NULL                  /* req_types */
    },
    /* --- point geometry manipulation --------------------------------- */
    {
	ECMD_PIPE_PT_MOVE,    /* cmd_id       */
	"Move Point",         /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	pipe_point_params,    /* params       */
	1,                    /* interactive  */
	8                     /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_PT_DEL,     /* cmd_id       */
	"Delete Point",       /* label        */
	"point",              /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	9                     /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_PT_ADD,     /* cmd_id       */
	"Append Point",       /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	pipe_point_params,    /* params       */
	1,                    /* interactive  */
	10                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_PT_INS,     /* cmd_id       */
	"Prepend Point",      /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	pipe_point_params,    /* params       */
	1,                    /* interactive  */
	11                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_SPLIT,      /* cmd_id       */
	"Split Segment",      /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	pipe_point_params,    /* params       */
	1,                    /* interactive  */
	12                    /* display_order */,
	NULL                  /* req_types */
    },
    /* --- per-point cross-section dimensions -------------------------- */
    {
	ECMD_PIPE_PT_OD,      /* cmd_id       */
	"Set Point OD",       /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	pipe_pt_od_params,    /* params       */
	1,                    /* interactive  */
	20                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_PT_ID,      /* cmd_id       */
	"Set Point ID",       /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	pipe_pt_id_params,    /* params       */
	1,                    /* interactive  */
	30                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_PT_RADIUS,  /* cmd_id       */
	"Set Point Bend",     /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	pipe_pt_radius_params, /* params      */
	1,                    /* interactive  */
	40                    /* display_order */,
	NULL                  /* req_types */
    },
    /* --- whole-pipe cross-section dimensions ------------------------- */
    {
	ECMD_PIPE_SCALE_OD,   /* cmd_id       */
	"Set Pipe OD",        /* label        */
	"pipe",               /* category     */
	1,                    /* nparam       */
	pipe_pt_od_params,    /* params       */
	1,                    /* interactive  */
	50                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_SCALE_ID,   /* cmd_id       */
	"Set Pipe ID",        /* label        */
	"pipe",               /* category     */
	1,                    /* nparam       */
	pipe_pt_id_params,    /* params       */
	1,                    /* interactive  */
	60                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PIPE_SCALE_RADIUS, /* cmd_id     */
	"Set Pipe Bend",      /* label        */
	"pipe",               /* category     */
	1,                    /* nparam       */
	pipe_pt_radius_params, /* params      */
	1,                    /* interactive  */
	70                    /* display_order */,
	NULL                  /* req_types */
    }
};

static const struct rt_edit_prim_desc pipe_prim_desc = {
    "pipe",               /* prim_type    */
    "Pipe",               /* prim_label   */
    14,                   /* ncmd         */
    pipe_cmds             /* cmds         */,
    0,                    /* nopt         */
    NULL                  /* opts         */
};

C_DECL const struct rt_edit_prim_desc *
rt_edit_pipe_edit_desc(void)
{
    return &pipe_prim_desc;
}


static int
pipe_split_pnt(struct rt_pipe_internal *pipeip, struct wdb_pipe_pnt *ps,
	       point_t new_pt)
{
    struct wdb_pipe_pnt *next;
    struct wdb_pipe_pnt *new_ps;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe point");

    next = BU_LIST_NEXT(wdb_pipe_pnt, &ps->l);
    if (next->l.magic == BU_LIST_HEAD_MAGIC) {
	return BRLCAD_ERROR;
    }

    /* Allocate and initialise a new point between ps and next. */
    BU_ALLOC(new_ps, struct wdb_pipe_pnt);
    new_ps->l.magic = WDB_PIPESEG_MAGIC;
    VMOVE(new_ps->pp_coord, new_pt);
    /* Interpolate cross-section parameters from the two neighbours */
    new_ps->pp_od          = (ps->pp_od          + next->pp_od)          * 0.5;
    new_ps->pp_id          = (ps->pp_id          + next->pp_id)          * 0.5;
    new_ps->pp_bendradius  = (ps->pp_bendradius  + next->pp_bendradius)  * 0.5;

    /* Insert the new point after ps (i.e. between ps and next) */
    BU_LIST_APPEND(&ps->l, &new_ps->l);

    /* Validate the modified pipe; remove the new point if it makes an
     * invalid configuration. */
    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
	BU_LIST_DEQUEUE(&new_ps->l);
	bu_free(new_ps, "pipe_split_pnt: new_ps");
	return BRLCAD_ERROR;
    }
    pipeip->pipe_count++;
    return BRLCAD_OK;
}


C_DECL const char *
rt_edit_pipe_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	struct rt_edit *s,
	const struct bn_tol *UNUSED(tol))
{
    struct rt_db_internal *ip = &s->es_int;
    static const char *strp = "V";
    point_t mpt = VINIT_ZERO;
    RT_CK_DB_INTERNAL(ip);
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    struct rt_pipe_internal *pipeip =(struct rt_pipe_internal *)ip->idb_ptr;
    struct wdb_pipe_pnt *pipe_seg;
    RT_PIPE_CK_MAGIC(pipeip);
    if (p->es_pipe_pnt == (struct wdb_pipe_pnt *)NULL) {
	pipe_seg = BU_LIST_FIRST(wdb_pipe_pnt, &pipeip->pipe_segs_head);
	VMOVE(mpt, pipe_seg->pp_coord);
    } else {
	VMOVE(mpt, p->es_pipe_pnt->pp_coord);
    }
    MAT4X3PNT(*pt, mat, mpt);
    return strp;
}

C_DECL void
rt_edit_pipe_labels(
	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_edit *s,
	struct bn_tol *UNUSED(tol))
{
    struct rt_db_internal *ip = &s->es_int;
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    point_t pos_view;
    int npl = 0;


#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }

    RT_CK_DB_INTERNAL(ip);
    //struct rt_pipe_internal *pipeip = (struct rt_pipe_internal *)ip->idb_ptr;
    //RT_PIPE_CK_MAGIC(pipeip);

    // Conditional labeling
    if (p->es_pipe_pnt) {
	BU_CKMAG(p->es_pipe_pnt, WDB_PIPESEG_MAGIC, "wdb_pipe_pnt");

	MAT4X3PNT(pos_view, xform, p->es_pipe_pnt->pp_coord);
	POINT_LABEL_STR(pos_view, "pt");
    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}

/* scale OD of one pipe segment */
static fastf_t
pipe_input_length(const struct rt_edit *s)
{
    /* Numeric lengths are local; e_mat[15] accounts for path scaling. */
    return s->e_para[0] * s->local2base * s->e_mat[15];
}

static int
pipe_set_selected_dimension(struct rt_edit *s, enum rt_constraint_edit_op_kind kind)
{
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    if (!p->es_pipe_pnt) {
	bu_vls_printf(s->log_str, "No pipe segment selected\n");
	return BRLCAD_ERROR;
    }
    if (s->e_inpara && (s->e_para[0] < 0.0 ||
	(kind == RT_CONSTRAINT_EDIT_OP_SET_BEND && ZERO(s->e_para[0])))) {
	bu_vls_printf(s->log_str, "Pipe dimension must be positive\n");
	return BRLCAD_ERROR;
    }

    fastf_t current;
    switch (kind) {
	case RT_CONSTRAINT_EDIT_OP_SET_OD:
	    current = p->es_pipe_pnt->pp_od;
	    break;
	case RT_CONSTRAINT_EDIT_OP_SET_ID:
	    current = p->es_pipe_pnt->pp_id;
	    break;
	case RT_CONSTRAINT_EDIT_OP_SET_BEND:
	    current = p->es_pipe_pnt->pp_bendradius;
	    break;
	default:
	    return BRLCAD_ERROR;
    }

    struct rt_constraint_edit_op op;
    memset(&op, 0, sizeof(op));
    op.kind = kind;
    op.point_index = rt_pipe_get_i_seg(
	(struct rt_pipe_internal *)s->es_int.idb_ptr, p->es_pipe_pnt);
    if (s->e_inpara) {
	op.proposed_scalar = pipe_input_length(s);
	s->es_scale = current > 0.0 ? op.proposed_scalar / current : 1.0;
    } else {
	op.proposed_scalar = current * s->es_scale;
    }
    return pipe_apply_cedit(s, &op);
}

static int
ecmd_pipe_pt_od(struct rt_edit *s)
{
    return pipe_set_selected_dimension(s, RT_CONSTRAINT_EDIT_OP_SET_OD);
}

static int
ecmd_pipe_pt_id(struct rt_edit *s)
{
    return pipe_set_selected_dimension(s, RT_CONSTRAINT_EDIT_OP_SET_ID);
}

static int
ecmd_pipe_pt_radius(struct rt_edit *s)
{
    return pipe_set_selected_dimension(s, RT_CONSTRAINT_EDIT_OP_SET_BEND);
}

static int
pipe_set_all_dimension(struct rt_edit *s, enum rt_constraint_edit_op_kind scale_kind)
{
    struct rt_pipe_internal *pipeip =
	(struct rt_pipe_internal *)s->es_int.idb_ptr;
    enum rt_constraint_edit_op_kind absolute_kind;
    struct rt_constraint_edit_op op;
    struct wdb_pipe_pnt *point;

    RT_PIPE_CK_MAGIC(pipeip);

    switch (scale_kind) {
	case RT_CONSTRAINT_EDIT_OP_SCALE_OD:
	    absolute_kind = RT_CONSTRAINT_EDIT_OP_SET_ALL_OD;
	    break;
	case RT_CONSTRAINT_EDIT_OP_SCALE_ID:
	    absolute_kind = RT_CONSTRAINT_EDIT_OP_SET_ALL_ID;
	    break;
	case RT_CONSTRAINT_EDIT_OP_SCALE_BEND:
	    absolute_kind = RT_CONSTRAINT_EDIT_OP_SET_ALL_BEND;
	    break;
	default:
	    return BRLCAD_ERROR;
    }

    memset(&op, 0, sizeof(op));
    op.point_index = -1;
    op.kind = scale_kind;
    op.proposed_scalar = s->es_scale;

    if (s->e_inpara) {
	if (s->e_para[0] < 0.0 ||
	    (scale_kind != RT_CONSTRAINT_EDIT_OP_SCALE_ID && ZERO(s->e_para[0]))) {
	    bu_vls_printf(s->log_str, "Invalid pipe dimension\n");
	    return BRLCAD_ERROR;
	}
	if (BU_LIST_IS_EMPTY(&pipeip->pipe_segs_head)) {
	    bu_vls_printf(s->log_str, "Pipe has no points\n");
	    return BRLCAD_ERROR;
	}

	fastf_t reference = 0.0;
	for (BU_LIST_FOR(point, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	    switch (scale_kind) {
		case RT_CONSTRAINT_EDIT_OP_SCALE_OD:
		    reference = point->pp_od;
		    break;
		case RT_CONSTRAINT_EDIT_OP_SCALE_ID:
		    reference = point->pp_id;
		    break;
		case RT_CONSTRAINT_EDIT_OP_SCALE_BEND:
		    reference = point->pp_bendradius;
		    break;
		default:
		    break;
	    }
	    if (reference > 0.0)
		break;
	}

	fastf_t target = pipe_input_length(s);
	if (reference > 0.0) {
	    s->es_scale = target / reference;
	    op.proposed_scalar = s->es_scale;
	} else {
	    /* A factor cannot change an all-zero dimension. */
	    s->es_scale = 1.0;
	    op.kind = absolute_kind;
	    op.proposed_scalar = target;
	}
    }

    return pipe_apply_cedit(s, &op);
}

static int
ecmd_pipe_scale_od(struct rt_edit *s)
{
    return pipe_set_all_dimension(s, RT_CONSTRAINT_EDIT_OP_SCALE_OD);
}

static int
ecmd_pipe_scale_id(struct rt_edit *s)
{
    return pipe_set_all_dimension(s, RT_CONSTRAINT_EDIT_OP_SCALE_ID);
}

static int
ecmd_pipe_scale_radius(struct rt_edit *s)
{
    return pipe_set_all_dimension(s, RT_CONSTRAINT_EDIT_OP_SCALE_BEND);
}

enum pipe_point_input_result {
    PIPE_POINT_INPUT_ERROR,
    PIPE_POINT_INPUT_NONE,
    PIPE_POINT_INPUT_READY
};

static enum pipe_point_input_result
pipe_point_from_edit(struct rt_edit *s, point_t point, const char *operation)
{
    if (s->e_mvalid) {
	VMOVE(point, s->e_mparam);
	return PIPE_POINT_INPUT_READY;
    }

    if (!s->e_inpara)
	return PIPE_POINT_INPUT_NONE;
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str, "%s: x y z coordinates required\n", operation);
	return PIPE_POINT_INPUT_ERROR;
    }

    point_t input_base;
    VSCALE(input_base, s->e_para, s->local2base);
    if (s->mv_context)
	MAT4X3PNT(point, s->e_invmat, input_base);
    else
	VMOVE(point, input_base);
    return PIPE_POINT_INPUT_READY;
}

static int
ecmd_pipe_pick(struct rt_edit *s)
{
    struct rt_pipe_internal *pipeip =
	(struct rt_pipe_internal *)s->es_int.idb_ptr;
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    point_t new_pt;

    RT_PIPE_CK_MAGIC(pipeip);

    enum pipe_point_input_result input =
	pipe_point_from_edit(s, new_pt, "select point");
    if (input != PIPE_POINT_INPUT_READY)
	return input == PIPE_POINT_INPUT_NONE ? BRLCAD_OK : BRLCAD_ERROR;

    p->es_pipe_pnt = rt_pipe_find_pnt_nearest_pnt(
	&pipeip->pipe_segs_head, new_pt, s->vp->gv_view2model);
    if (!p->es_pipe_pnt) {
	bu_vls_printf(s->log_str, "No PIPE segment selected\n");
	return BRLCAD_ERROR;
    }
    rt_pipe_pnt_print(p->es_pipe_pnt, s->base2local);
    return BRLCAD_OK;
}

static int
ecmd_pipe_split(struct rt_edit *s)
{
    struct rt_pipe_internal *pipeip =
	(struct rt_pipe_internal *)s->es_int.idb_ptr;
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    point_t new_pt;

    RT_PIPE_CK_MAGIC(pipeip);

    enum pipe_point_input_result input =
	pipe_point_from_edit(s, new_pt, "split segment");
    if (input != PIPE_POINT_INPUT_READY)
	return input == PIPE_POINT_INPUT_NONE ? BRLCAD_OK : BRLCAD_ERROR;
    if (!p->es_pipe_pnt) {
	bu_vls_printf(s->log_str, "No pipe segment selected\n");
	return BRLCAD_ERROR;
    }

    if (pipe_split_pnt(pipeip, p->es_pipe_pnt, new_pt) != BRLCAD_OK) {
	bu_vls_printf(s->log_str, "Cannot split this pipe segment\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
ecmd_pipe_pt_move(struct rt_edit *s)
{
    struct rt_pipe_internal *pipeip =
	(struct rt_pipe_internal *)s->es_int.idb_ptr;
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    point_t new_pt;

    RT_PIPE_CK_MAGIC(pipeip);

    enum pipe_point_input_result input =
	pipe_point_from_edit(s, new_pt, "move point");
    if (input != PIPE_POINT_INPUT_READY)
	return input == PIPE_POINT_INPUT_NONE ? BRLCAD_OK : BRLCAD_ERROR;
    if (!p->es_pipe_pnt) {
	bu_vls_printf(s->log_str, "No pipe segment selected\n");
	return BRLCAD_ERROR;
    }

    struct rt_constraint_edit_op op;
    memset(&op, 0, sizeof(op));
    op.kind = RT_CONSTRAINT_EDIT_OP_MOVE_POINT;
    op.point_index = rt_pipe_get_i_seg(pipeip, p->es_pipe_pnt);
    VMOVE(op.proposed_coord, new_pt);
    return pipe_apply_cedit(s, &op);
}

static int
ecmd_pipe_pt_add(struct rt_edit *s)
{
    struct rt_pipe_internal *pipeip =
	(struct rt_pipe_internal *)s->es_int.idb_ptr;
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    point_t new_pt;

    RT_PIPE_CK_MAGIC(pipeip);

    enum pipe_point_input_result input =
	pipe_point_from_edit(s, new_pt, "append point");
    if (input != PIPE_POINT_INPUT_READY)
	return input == PIPE_POINT_INPUT_NONE ? BRLCAD_OK : BRLCAD_ERROR;

    struct wdb_pipe_pnt *added = rt_pipe_add_pnt(pipeip, p->es_pipe_pnt, new_pt);
    if (!added || added == p->es_pipe_pnt) {
	bu_vls_printf(s->log_str, "Cannot append point to pipe\n");
	return BRLCAD_ERROR;
    }
    p->es_pipe_pnt = added;
    return BRLCAD_OK;
}

static int
ecmd_pipe_pt_ins(struct rt_edit *s)
{
    struct rt_pipe_internal *pipeip =
	(struct rt_pipe_internal *)s->es_int.idb_ptr;
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    point_t new_pt;

    RT_PIPE_CK_MAGIC(pipeip);

    enum pipe_point_input_result input =
	pipe_point_from_edit(s, new_pt, "prepend point");
    if (input != PIPE_POINT_INPUT_READY)
	return input == PIPE_POINT_INPUT_NONE ? BRLCAD_OK : BRLCAD_ERROR;

    struct wdb_pipe_pnt *inserted = rt_pipe_ins_pnt(pipeip, p->es_pipe_pnt, new_pt);
    if (!inserted || inserted == p->es_pipe_pnt) {
	bu_vls_printf(s->log_str, "Cannot prepend point to pipe\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
ecmd_pipe_pt_del(struct rt_edit *s)
{
    struct rt_pipe_internal *pipeip =
	(struct rt_pipe_internal *)s->es_int.idb_ptr;
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    RT_PIPE_CK_MAGIC(pipeip);
    if (!p->es_pipe_pnt) {
	bu_vls_printf(s->log_str, "No pipe segment selected\n");
	return BRLCAD_ERROR;
    }

    struct wdb_pipe_pnt *selected = p->es_pipe_pnt;
    p->es_pipe_pnt = rt_pipe_delete_pnt(selected);
    if (p->es_pipe_pnt == selected) {
	bu_vls_printf(s->log_str, "Cannot delete this pipe point\n");
	return BRLCAD_ERROR;
    }
    pipeip->pipe_count--;
    return BRLCAD_OK;
}

static int
rt_edit_pipe_pscale(struct rt_edit *s)
{
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    switch (s->edit_flag) {
	case ECMD_PIPE_PT_OD:   /* scale OD of one pipe segment */
	    return ecmd_pipe_pt_od(s);
	case ECMD_PIPE_PT_ID:   /* scale ID of one pipe segment */
	    return ecmd_pipe_pt_id(s);
	case ECMD_PIPE_PT_RADIUS:       /* scale bend radius at selected point */
	    return ecmd_pipe_pt_radius(s);
	case ECMD_PIPE_SCALE_OD:        /* scale entire pipe OD */
	    return ecmd_pipe_scale_od(s);
	case ECMD_PIPE_SCALE_ID:        /* scale entire pipe ID */
	    return ecmd_pipe_scale_id(s);
	case ECMD_PIPE_SCALE_RADIUS:    /* scale entire pipr bend radius */
	    return ecmd_pipe_scale_radius(s);
    };

    return 0;
}

C_DECL int
rt_edit_pipe_edit(struct rt_edit *s)
{
    struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    p->es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset p->es_pipe_pnt */
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    /* translate solid */
	    p->es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset p->es_pipe_pnt */
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    /* rot solid about vertex */
	    p->es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset p->es_pipe_pnt */
	    edit_srot(s);
	    break;
	case ECMD_PIPE_SELECT:
	    return ecmd_pipe_pick(s);
	case ECMD_PIPE_NEXT_PT:
	{
	    bu_clbk_t f = NULL;
	    void *d = NULL;
	    if (!p->es_pipe_pnt) {
		bu_vls_printf(s->log_str, "No Pipe Segment selected\n");
		rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
		if (f) (*f)(0, NULL, d, NULL);
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return BRLCAD_ERROR;
	    }
	    {
		struct wdb_pipe_pnt *next = BU_LIST_NEXT(wdb_pipe_pnt, &p->es_pipe_pnt->l);
		if (next->l.magic == BU_LIST_HEAD_MAGIC) {
		    bu_vls_printf(s->log_str, "Current segment is the last\n");
		    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
		    if (f) (*f)(0, NULL, d, NULL);
		    rt_edit_set_edflag(s, RT_EDIT_IDLE);
		    return BRLCAD_ERROR;
		}
		p->es_pipe_pnt = next;
	    }
	    rt_pipe_pnt_print(p->es_pipe_pnt, s->base2local);
	    rt_edit_set_edflag(s, RT_EDIT_IDLE);
	    break;
	}
	case ECMD_PIPE_PREV_PT:
	{
	    bu_clbk_t f = NULL;
	    void *d = NULL;
	    if (!p->es_pipe_pnt) {
		bu_vls_printf(s->log_str, "No Pipe Segment selected\n");
		rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
		if (f) (*f)(0, NULL, d, NULL);
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return BRLCAD_ERROR;
	    }
	    {
		struct wdb_pipe_pnt *prev = BU_LIST_PREV(wdb_pipe_pnt, &p->es_pipe_pnt->l);
		if (prev->l.magic == BU_LIST_HEAD_MAGIC) {
		    bu_vls_printf(s->log_str, "Current segment is the first\n");
		    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
		    if (f) (*f)(0, NULL, d, NULL);
		    rt_edit_set_edflag(s, RT_EDIT_IDLE);
		    return BRLCAD_ERROR;
		}
		p->es_pipe_pnt = prev;
	    }
	    rt_pipe_pnt_print(p->es_pipe_pnt, s->base2local);
	    rt_edit_set_edflag(s, RT_EDIT_IDLE);
	    break;
	}
	case ECMD_PIPE_SPLIT:
	    return ecmd_pipe_split(s);
	case ECMD_PIPE_PT_MOVE:
	    return ecmd_pipe_pt_move(s);
	case ECMD_PIPE_PT_ADD:
	    return ecmd_pipe_pt_add(s);
	case ECMD_PIPE_PT_INS:
	    return ecmd_pipe_pt_ins(s);
	case ECMD_PIPE_PT_DEL:
	    return ecmd_pipe_pt_del(s);
	case ECMD_PIPE_PT_OD:
	case ECMD_PIPE_PT_ID:
	case ECMD_PIPE_PT_RADIUS:
	case ECMD_PIPE_SCALE_OD:
	case ECMD_PIPE_SCALE_ID:
	case ECMD_PIPE_SCALE_RADIUS:
	    return rt_edit_pipe_pscale(s);
	default:
	    return edit_generic(s);
    }

    return 0;
}

C_DECL int
rt_edit_pipe_edit_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_PIPE_NEXT_PT:
	case ECMD_PIPE_PREV_PT:
	case ECMD_PIPE_PT_OD:
	case ECMD_PIPE_PT_ID:
	case ECMD_PIPE_SCALE_OD:
	case ECMD_PIPE_SCALE_ID:
	case ECMD_PIPE_PT_RADIUS:
	case ECMD_PIPE_SCALE_RADIUS:
	case ECMD_PIPE_PT_DEL:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_PIPE_SELECT:
	case ECMD_PIPE_SPLIT:
	case ECMD_PIPE_PT_ADD:
	case ECMD_PIPE_PT_INS:
	case ECMD_PIPE_PT_MOVE:
	    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
	    MAT4X3PNT(s->e_mparam, s->e_invmat, temp);
	    s->e_mvalid = 1;
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
