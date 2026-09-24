/*                      E D M E T A B A L L . C
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
/** @file primitives/edmetaball.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"

#define ECMD_METABALL_PT_NEXT		30121
#define ECMD_METABALL_PT_PREV		30122
#define ECMD_METABALL_PT_ADD		36089	/* add a metaball control point */
#define ECMD_METABALL_PT_DEL		36088	/* delete a metaball control point */
#define ECMD_METABALL_PT_FLDSTR		36087	/* scale a metaball control point's field strength */
#define ECMD_METABALL_PT_MOV		36086	/* move a metaball control point */
#define ECMD_METABALL_PT_PICK		36085	/* pick a metaball control point */
#define ECMD_METABALL_PT_SCALE_BLOBBINESS	30119	/* scale blobbiness of selected control point */
#define ECMD_METABALL_PT_SET_BLOBBINESS		30120	/* set blobbiness of selected control point */
#define ECMD_METABALL_RMET		36090	/* set the metaball render method */
#define ECMD_METABALL_SET_METHOD	36084	/* set the rendering method */
#define ECMD_METABALL_SET_THRESHOLD	36083	/* overall metaball threshold value */

struct rt_metaball_edit {
    struct wdb_metaball_pnt *es_metaball_pnt; /* Currently selected METABALL Point */
};

C_DECL void *
rt_edit_metaball_prim_edit_create(struct rt_edit *UNUSED(s))
{
    struct rt_metaball_edit *m;
    BU_GET(m, struct rt_metaball_edit);

    m->es_metaball_pnt = NULL;

    return (void *)m;
}

C_DECL void
rt_edit_metaball_prim_edit_destroy(void *ptr)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)ptr;
    if (!m)
	return;

    // Sanity
    m->es_metaball_pnt = NULL;

    BU_PUT(m, struct rt_metaball_edit);
}

C_DECL void
rt_edit_metaball_prim_edit_reset(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    m->es_metaball_pnt = NULL;
}

C_DECL void
rt_edit_metaball_set_edit_mode(struct rt_edit *s, int mode)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    struct wdb_metaball_pnt *next, *prev;

    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_METABALL_SET_THRESHOLD:
	case ECMD_METABALL_SET_METHOD:
	case ECMD_METABALL_PT_SCALE_BLOBBINESS:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	case ECMD_METABALL_PT_PICK:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	case ECMD_METABALL_PT_NEXT:
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		return;
	    }
	    next = BU_LIST_NEXT(wdb_metaball_pnt, &m->es_metaball_pnt->l);
	    if (next->l.magic == BU_LIST_HEAD_MAGIC) {
		bu_vls_printf(s->log_str, "Current point is the last\n");
		return;
	    }
	    m->es_metaball_pnt = next;
	    rt_metaball_pnt_print(m->es_metaball_pnt, s->base2local);
	    rt_edit_set_edflag(s, RT_EDIT_IDLE);
	    break;
	case ECMD_METABALL_PT_PREV:
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		return;
	    }
	    prev = BU_LIST_PREV(wdb_metaball_pnt, &m->es_metaball_pnt->l);
	    if (prev->l.magic == BU_LIST_HEAD_MAGIC) {
		bu_vls_printf(s->log_str, "Current point is the first\n");
		return;
	    }
	    m->es_metaball_pnt = prev;
	    rt_metaball_pnt_print(m->es_metaball_pnt, s->base2local);
	    rt_edit_set_edflag(s, RT_EDIT_IDLE);
	    break;
	case ECMD_METABALL_PT_MOV:
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_METABALL_PT_FLDSTR:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    break;
	case ECMD_METABALL_PT_SET_BLOBBINESS:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    break;
	case ECMD_METABALL_PT_DEL:
	case ECMD_METABALL_PT_ADD:
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
metaball_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    struct wdb_metaball_pnt *next, *prev;

    rt_edit_set_edflag(s, arg);

    switch (arg) {
	case ECMD_METABALL_SET_THRESHOLD:
	case ECMD_METABALL_SET_METHOD:
	case ECMD_METABALL_PT_SCALE_BLOBBINESS:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	case ECMD_METABALL_PT_PICK:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	case ECMD_METABALL_PT_NEXT:
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		return;
	    }
	    next = BU_LIST_NEXT(wdb_metaball_pnt, &m->es_metaball_pnt->l);
	    if (next->l.magic == BU_LIST_HEAD_MAGIC) {
		bu_vls_printf(s->log_str, "Current point is the last\n");
		return;
	    }
	    m->es_metaball_pnt = next;
	    rt_metaball_pnt_print(m->es_metaball_pnt, s->base2local);
	    rt_edit_set_edflag(s, RT_EDIT_IDLE);
	    /* Advance to the next control point; trigger immediate display update. */
	    rt_edit_process(s);
	    break;
	case ECMD_METABALL_PT_PREV:
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		return;
	    }
	    prev = BU_LIST_PREV(wdb_metaball_pnt, &m->es_metaball_pnt->l);
	    if (prev->l.magic == BU_LIST_HEAD_MAGIC) {
		bu_vls_printf(s->log_str, "Current point is the first\n");
		return;
	    }
	    m->es_metaball_pnt = prev;
	    rt_metaball_pnt_print(m->es_metaball_pnt, s->base2local);
	    rt_edit_set_edflag(s, RT_EDIT_IDLE);
	    /* Step to the previous control point; trigger immediate display update. */
	    rt_edit_process(s);
	    break;
	case ECMD_METABALL_PT_MOV:
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_METABALL_PT_FLDSTR:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    break;
	case ECMD_METABALL_PT_SET_BLOBBINESS:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    if (!m->es_metaball_pnt) {
		bu_vls_printf(s->log_str, "No Metaball Point selected\n");
		rt_edit_set_edflag(s, RT_EDIT_IDLE);
		return;
	    }
	    break;
	case ECMD_METABALL_PT_DEL:
	    /* Deletion is handled inside ft_edit; trigger it immediately. */
	    rt_edit_process(s);
	    break;
	case ECMD_METABALL_PT_ADD:
	    break;
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}

struct rt_edit_menu_item metaball_menu[] = {
    { "METABALL MENU", NULL, 0 },
    { "Set Threshold", metaball_ed, ECMD_METABALL_SET_THRESHOLD },
    { "Set Render Method", metaball_ed, ECMD_METABALL_SET_METHOD },
    { "Select Point", metaball_ed, ECMD_METABALL_PT_PICK},
    { "Next Point", metaball_ed, ECMD_METABALL_PT_NEXT },
    { "Previous Point", metaball_ed, ECMD_METABALL_PT_PREV },
    { "Move Point", metaball_ed, ECMD_METABALL_PT_MOV },
    { "Scale Point Field Strength", metaball_ed, ECMD_METABALL_PT_FLDSTR },
    { "Scale Point Blobbiness", metaball_ed, ECMD_METABALL_PT_SCALE_BLOBBINESS },
    { "Set Point Blobbiness", metaball_ed, ECMD_METABALL_PT_SET_BLOBBINESS },
    { "Delete Point", metaball_ed, ECMD_METABALL_PT_DEL },
    { "Add Point", metaball_ed, ECMD_METABALL_PT_ADD },
    { "", NULL, 0 }
};

C_DECL struct rt_edit_menu_item *
rt_edit_metaball_menu_item(const struct bn_tol *UNUSED(tol))
{
    return metaball_menu;
}


/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Metaball primitive                 */
/* ------------------------------------------------------------------ */

/* Scalar param used by SET_THRESHOLD and SET_METHOD */
static const struct rt_edit_param_desc metaball_threshold_params[] = {
    {
	"threshold",          /* name         */
	"Threshold",          /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"",                   /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc metaball_method_params[] = {
    {
	"method",             /* name         */
	"Render Method",      /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	METABALL_BLOB,        /* range_max    */
	"",                   /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* POINT param for pt_pick, pt_mov, pt_add */
static const struct rt_edit_param_desc metaball_point_params[] = {
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

/* SCALAR param for field_strength (per-point) */
static const struct rt_edit_param_desc metaball_field_strength_params[] = {
    {
	"field_strength",     /* name         */
	"Field Strength",     /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"",                   /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* SCALAR param for blobbiness (per-point, blob method) */
static const struct rt_edit_param_desc metaball_blobbiness_params[] = {
    {
	"blobbiness",         /* name         */
	"Blobbiness",         /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"",                   /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc metaball_cmds[] = {
    /* --- global metaball parameters --------------------------------- */
    {
	ECMD_METABALL_SET_THRESHOLD, /* cmd_id  */
	"Set Threshold",      /* label        */
	"metaball",           /* category     */
	1,                    /* nparam       */
	metaball_threshold_params, /* params  */
	1,                    /* interactive  */
	10                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_METABALL_SET_METHOD,    /* cmd_id  */
	"Set Render Method",  /* label        */
	"metaball",           /* category     */
	1,                    /* nparam       */
	metaball_method_params, /* params     */
	1,                    /* interactive  */
	20                    /* display_order */,
	NULL                  /* req_types */
    },
    /* --- point selection / navigation -------------------------------- */
    {
	ECMD_METABALL_PT_PICK,       /* cmd_id  */
	"Select Point",       /* label        */
	"select",             /* category     */
	1,                    /* nparam       */
	metaball_point_params, /* params      */
	1,                    /* interactive  */
	30                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_METABALL_PT_NEXT,       /* cmd_id  */
	"Next Point",         /* label        */
	"select",             /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	31                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_METABALL_PT_PREV,       /* cmd_id  */
	"Previous Point",     /* label        */
	"select",             /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	32                    /* display_order */,
	NULL                  /* req_types */
    },
    /* --- point geometry manipulation --------------------------------- */
    {
	ECMD_METABALL_PT_MOV,        /* cmd_id  */
	"Move Point",         /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_point_params, /* params      */
	1,                    /* interactive  */
	40                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_METABALL_PT_ADD,        /* cmd_id  */
	"Add Point",          /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_point_params, /* params      */
	1,                    /* interactive  */
	50                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_METABALL_PT_DEL,        /* cmd_id  */
	"Delete Point",       /* label        */
	"point",              /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	60                    /* display_order */,
	NULL                  /* req_types */
    },
    /* --- per-point scalar parameters --------------------------------- */
    {
	ECMD_METABALL_PT_FLDSTR,     /* cmd_id  */
	"Scale Point Field Strength", /* label  */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_field_strength_params, /* params */
	1,                    /* interactive  */
	70                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_METABALL_PT_SCALE_BLOBBINESS, /* cmd_id */
	"Scale Point Blobbiness", /* label    */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_blobbiness_params, /* params */
	1,                    /* interactive  */
	80                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_METABALL_PT_SET_BLOBBINESS, /* cmd_id */
	"Set Point Blobbiness", /* label      */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_blobbiness_params, /* params */
	1,                    /* interactive  */
	90                    /* display_order */,
	NULL                  /* req_types */
    }
};

static const struct rt_edit_prim_desc metaball_prim_desc = {
    "metaball",           /* prim_type    */
    "Metaball",           /* prim_label   */
    11,                   /* ncmd         */
    metaball_cmds         /* cmds         */,
    0,                    /* nopt         */
    NULL                  /* opts         */
};

C_DECL const struct rt_edit_prim_desc *
rt_edit_metaball_edit_desc(void)
{
    return &metaball_prim_desc;
}


/* get_params: return current value(s) for the given cmd_id */
C_DECL int
rt_edit_metaball_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    struct rt_metaball_internal *ball;
    struct rt_metaball_edit *m;

    if (!s || !vals)
	return -1;

    ball = (struct rt_metaball_internal *)s->es_int.idb_ptr;
    RT_METABALL_CK_MAGIC(ball);
    m = (struct rt_metaball_edit *)s->ipe_ptr;

    switch (cmd_id) {
	case ECMD_METABALL_SET_THRESHOLD:
	    vals[0] = ball->threshold;
	    return 1;
	case ECMD_METABALL_SET_METHOD:
	    vals[0] = (fastf_t)ball->method;
	    return 1;
	case ECMD_METABALL_PT_FLDSTR:
	    if (!m->es_metaball_pnt)
		return 0;
	    vals[0] = m->es_metaball_pnt->field_strength * s->base2local;
	    return 1;
	case ECMD_METABALL_PT_SCALE_BLOBBINESS:
	case ECMD_METABALL_PT_SET_BLOBBINESS:
	    if (!m->es_metaball_pnt)
		return 0;
	    vals[0] = m->es_metaball_pnt->blobbiness;
	    return 1;
	case ECMD_METABALL_PT_PICK:
	case ECMD_METABALL_PT_MOV:
	case ECMD_METABALL_PT_ADD:
	    if (!m->es_metaball_pnt)
		return 0;
	    VSCALE(vals, m->es_metaball_pnt->coord, s->base2local);
	    return 3;
	default:
	    return 0;
    }
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

C_DECL void
rt_edit_metaball_write_params(
	struct bu_vls *p,
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_metaball_internal *ball =
	(struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(ball);
    struct wdb_metaball_pnt *mbpt;
    int n = 0;

    bu_vls_printf(p, "method: %d\n", ball->method);
    bu_vls_printf(p, "threshold: %.9f\n", ball->threshold);

    for (BU_LIST_FOR(mbpt, wdb_metaball_pnt, &ball->metaball_ctrl_head)) {
	bu_vls_printf(p, "point[%d]: %.9f %.9f %.9f field_strength=%.9f blobbiness=%.9f\n",
		      n,
		      V3BASE2LOCAL(mbpt->coord),
		      mbpt->field_strength * base2local,
		      mbpt->blobbiness);
	n++;
    }
}

static void
metaball_edit_clear_points(struct bu_list *head)
{
    while (BU_LIST_NON_EMPTY(head)) {
	struct wdb_metaball_pnt *point = BU_LIST_FIRST(wdb_metaball_pnt, head);
	BU_LIST_DQ(&point->l);
	BU_PUT(point, struct wdb_metaball_pnt);
    }
}

C_DECL int
rt_edit_metaball_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    struct rt_metaball_internal *ball =
	(struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(ball);

    if (!fc || !isfinite(local2base) || local2base <= 0.0)
	return BRLCAD_ERROR;

    char *buffer = bu_strdup(fc);
    char *cursor = buffer;
    char *line;
    int method, end = 0, expected_index = 0;
    double threshold;
    struct rt_metaball_internal staged = {0};
    BU_LIST_INIT(&staged.metaball_ctrl_head);

    line = edit_param_next_line(&cursor);
    if (!line || sscanf(line, "method: %d %n", &method, &end) != 1 ||
	!end || line[end] || method < METABALL_METABALL ||
	method > METABALL_BLOB)
	goto failure;

    end = 0;
    line = edit_param_next_line(&cursor);
    if (!line || sscanf(line, "threshold: %lf %n", &threshold, &end) != 1 ||
	!end || line[end] || !isfinite(threshold) || threshold <= 0.0)
	goto failure;

    while ((line = edit_param_next_line(&cursor)) != NULL) {
	int index;
	double x, y, z, strength = 1.0, blobbiness = 1.0;
	point_t point;
	end = 0;
	if (sscanf(line, "point[%d]: %lf %lf %lf%n",
		&index, &x, &y, &z, &end) != 4 ||
	    !end || index != expected_index)
	    goto failure;

	const char *rest = line + end;
	rest += strspn(rest, " \t");
	if (*rest) {
	    int consumed = 0;
	    if (sscanf(rest, "field_strength=%lf%n", &strength,
		&consumed) != 1 || !consumed)
		goto failure;
	    rest += consumed;
	    rest += strspn(rest, " \t");
	    if (*rest) {
		consumed = 0;
		if (sscanf(rest, "blobbiness=%lf%n", &blobbiness,
		    &consumed) != 1 || !consumed)
		    goto failure;
		rest += consumed;
		rest += strspn(rest, " \t");
	    }
	}
	if (*rest ||
	    !isfinite(x) || !isfinite(y) || !isfinite(z) ||
	    !isfinite(strength) || !isfinite(blobbiness))
	    goto failure;

	VSET(point, x * local2base, y * local2base, z * local2base);
	strength *= local2base;
	if (!isfinite(point[X]) || !isfinite(point[Y]) ||
	    !isfinite(point[Z]) || !isfinite(strength) ||
	    rt_metaball_add_point(&staged, (const point_t *)&point,
		(fastf_t)strength, (fastf_t)blobbiness))
	    goto failure;
	++expected_index;
    }

    metaball_edit_clear_points(&ball->metaball_ctrl_head);
    BU_LIST_APPEND_LIST(&ball->metaball_ctrl_head, &staged.metaball_ctrl_head);
    ball->method = method;
    ball->threshold = threshold;
    bu_free(buffer, "metaball parameter text");
    return BRLCAD_OK;

failure:
    metaball_edit_clear_points(&staged.metaball_ctrl_head);
    bu_free(buffer, "metaball parameter text");
    return BRLCAD_ERROR;
}


C_DECL void
rt_edit_metaball_labels(
	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_edit *s,
	struct bn_tol *UNUSED(tol))
{
    struct rt_db_internal *ip = &s->es_int;
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    point_t pos_view;
    int npl = 0;

    RT_CK_DB_INTERNAL(ip);

#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }

    struct rt_metaball_internal *metaball =
	(struct rt_metaball_internal *)ip->idb_ptr;

    RT_METABALL_CK_MAGIC(metaball);

    if (m->es_metaball_pnt) {
	BU_CKMAG(m->es_metaball_pnt, WDB_METABALLPT_MAGIC, "wdb_metaball_pnt");

	MAT4X3PNT(pos_view, xform, m->es_metaball_pnt->coord);
	POINT_LABEL_STR(pos_view, "pt");
    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}

C_DECL const char *
rt_edit_metaball_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	struct rt_edit *s,
	const struct bn_tol *UNUSED(tol))
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    struct rt_db_internal *ip = &s->es_int;
    RT_CK_DB_INTERNAL(ip);
    point_t mpt = VINIT_ZERO;
    VSETALL(mpt, 0.0);
    static char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);

    struct rt_metaball_internal *metaball = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(metaball);

    if (m->es_metaball_pnt==NULL) {
	snprintf(buf, BUFSIZ, "no point selected");
    } else {
	VMOVE(mpt, m->es_metaball_pnt->coord);
	snprintf(buf, BUFSIZ, "V %f", m->es_metaball_pnt->field_strength);
    }

    MAT4X3PNT(*pt, mat, mpt);
    return (const char *)buf;
}

int
ecmd_metaball_set_threshold(struct rt_edit *s)
{
    if (!s->e_inpara)
	return BRLCAD_OK;
    if (!isfinite(s->e_para[0]) || s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "Threshold must be finite and positive\n");
	return BRLCAD_ERROR;
    }

    struct rt_metaball_internal *ball =
	(struct rt_metaball_internal *)s->es_int.idb_ptr;
    RT_METABALL_CK_MAGIC(ball);
    ball->threshold = s->e_para[0];

    return 0;
}

int
ecmd_metaball_set_method(struct rt_edit *s)
{
    if (!s->e_inpara)
	return BRLCAD_OK;
    fastf_t method = s->e_para[0];
    if (!isfinite(method) || method < METABALL_METABALL ||
	method > METABALL_BLOB || !EQUAL(method, floor(method))) {
	bu_vls_printf(s->log_str,
	    "Render method must be an integer from %d to %d\n",
	    METABALL_METABALL, METABALL_BLOB);
	return BRLCAD_ERROR;
    }

    struct rt_metaball_internal *ball =
	(struct rt_metaball_internal *)s->es_int.idb_ptr;
    RT_METABALL_CK_MAGIC(ball);
    ball->method = (int)method;

    return 0;
}

static fastf_t
metaball_point_scale(const struct rt_edit *s)
{
    /* No interactive multiplier leaves es_scale at zero. */
    return s->e_para[0] * ((s->es_scale > SMALL_FASTF) ? s->es_scale : 1.0);
}

int
ecmd_metaball_pt_set_goo(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    if (!isfinite(s->e_para[0]) || s->e_para[0] < 0.0 ||
	!isfinite(s->es_scale)) {
	bu_vls_printf(s->log_str, "Invalid blobbiness scale\n");
	return BRLCAD_ERROR;
    }

    if (!m->es_metaball_pnt || !s->e_inpara) {
	bu_vls_printf(s->log_str, "pscale: no metaball point selected for scaling blobbiness\n");
	return BRLCAD_ERROR;
    }
    m->es_metaball_pnt->blobbiness *= metaball_point_scale(s);

    return 0;
}

int
ecmd_metaball_pt_sweat(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;

    if (!m->es_metaball_pnt || !s->e_inpara) {
	bu_vls_printf(s->log_str, "no metaball point selected for setting blobbiness\n");
	return BRLCAD_ERROR;
    }
    if (!isfinite(s->e_para[0]) || s->e_para[0] < 0.0) {
	bu_vls_printf(s->log_str, "Blobbiness must be finite and nonnegative\n");
	return BRLCAD_ERROR;
    }
    m->es_metaball_pnt->blobbiness = s->e_para[0];
    return 0;
}

int
ecmd_metaball_pt_fldstr(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    if (!isfinite(s->e_para[0]) || s->e_para[0] <= 0.0 ||
	!isfinite(s->es_scale)) {
	bu_vls_printf(s->log_str, "Invalid field-strength scale\n");
	return BRLCAD_ERROR;
    }

    if (!m->es_metaball_pnt || !s->e_inpara) {
	bu_vls_printf(s->log_str, "pscale: no metaball point selected for scaling strength\n");
	return BRLCAD_ERROR;
    }

    m->es_metaball_pnt->field_strength *= metaball_point_scale(s);

    return 0;
}

int
ecmd_metaball_pt_pick(struct rt_edit *s)
{
    struct rt_metaball_internal *metaball=
	(struct rt_metaball_internal *)s->es_int.idb_ptr;
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    point_t new_pt;
    struct wdb_metaball_pnt *ps;
    struct wdb_metaball_pnt *nearest=(struct wdb_metaball_pnt *)NULL;
    fastf_t min_dist = MAX_FASTF;
    vect_t dir, work;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_METABALL_CK_MAGIC(metaball);

    if (s->e_mvalid) {
	VMOVE(new_pt, s->e_mparam);
    } else if (s->e_inpara == 3) {
	VSCALE(new_pt, s->e_para, s->local2base);
    } else if (s->e_inpara) {
	bu_vls_printf(s->log_str, "x y z coordinates required for control point selection\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    } else {
	return BRLCAD_OK;
    }

    /* Without a view, numeric picking projects along model Z. */
    VSET(work, 0.0, 0.0, 1.0);
    if (s->vp)
	MAT4X3VEC(dir, s->vp->gv_view2model, work);
    else
	VMOVE(dir, work);

    for (BU_LIST_FOR(ps, wdb_metaball_pnt, &metaball->metaball_ctrl_head)) {
	fastf_t dist;

	dist = bg_dist_line3_pnt3(new_pt, dir, ps->coord);
	if (dist < min_dist) {
	    min_dist = dist;
	    nearest = ps;
	}
    }

    m->es_metaball_pnt = nearest;

    if (!m->es_metaball_pnt) {
	bu_vls_printf(s->log_str, "No METABALL control point selected\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return BRLCAD_ERROR;
    } else {
	rt_metaball_pnt_print(m->es_metaball_pnt, s->base2local);
    }
    return BRLCAD_OK;
}

int
ecmd_metaball_pt_mov(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    if (!m->es_metaball_pnt) {
	bu_vls_printf(s->log_str, "Must select a point to move\n");
	return BRLCAD_ERROR;
    }
    if (s->e_mvalid) {
	VMOVE(m->es_metaball_pnt->coord, s->e_mparam);
	s->e_mvalid = 0;
	return BRLCAD_OK;
    }
    if (!s->e_inpara)
	return BRLCAD_OK;
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str, "Must provide dx dy dz\n");
	return BRLCAD_ERROR;
    }
    vect_t delta;
    for (int i = 0; i < ELEMENTS_PER_VECT; ++i) {
	if (!isfinite(s->e_para[i])) {
	    bu_vls_printf(s->log_str, "Point delta must be finite\n");
	    return BRLCAD_ERROR;
	}
    }
    VSCALE(delta, s->e_para, s->local2base);
    VADD2(m->es_metaball_pnt->coord, m->es_metaball_pnt->coord, delta);
    return BRLCAD_OK;
}

int
ecmd_metaball_pt_del(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    struct wdb_metaball_pnt *tmp = m->es_metaball_pnt, *p;

    if (m->es_metaball_pnt == NULL) {
	bu_vls_printf(s->log_str, "No point selected\n");
	return BRLCAD_ERROR;
    }
    p = BU_LIST_PREV(wdb_metaball_pnt, &m->es_metaball_pnt->l);
    if (p->l.magic == BU_LIST_HEAD_MAGIC) {
	m->es_metaball_pnt = BU_LIST_NEXT(wdb_metaball_pnt, &m->es_metaball_pnt->l);
	/* 0 point metaball... allow it for now. */
	if (m->es_metaball_pnt->l.magic == BU_LIST_HEAD_MAGIC)
	    m->es_metaball_pnt = NULL;
    } else
	m->es_metaball_pnt = p;
    BU_LIST_DQ(&tmp->l);
    BU_PUT(tmp, struct wdb_metaball_pnt);
    if (!m->es_metaball_pnt)
	bu_log("WARNING: Last point of this metaball has been deleted.");
    return BRLCAD_OK;
}

int
ecmd_metaball_pt_add(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    struct rt_metaball_internal *metaball= (struct rt_metaball_internal *)s->es_int.idb_ptr;
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str, "Must provide x y z\n");
	return BRLCAD_ERROR;
    }

    point_t model_point;
    for (int i = 0; i < ELEMENTS_PER_VECT; ++i) {
	if (!isfinite(s->e_para[i])) {
	    bu_vls_printf(s->log_str, "Point coordinates must be finite\n");
	    return BRLCAD_ERROR;
	}
    }
    VSCALE(model_point, s->e_para, s->local2base);

    struct wdb_metaball_pnt *n;
    BU_GET(n, struct wdb_metaball_pnt);
    m->es_metaball_pnt = BU_LIST_FIRST(wdb_metaball_pnt, &metaball->metaball_ctrl_head);
    VMOVE(n->coord, model_point);
    n->l.magic = WDB_METABALLPT_MAGIC;
    n->field_strength = 1.0;
    n->blobbiness = 1.0;
    BU_LIST_APPEND(&m->es_metaball_pnt->l, &n->l);
    m->es_metaball_pnt = n;
    return BRLCAD_OK;
}

static int
rt_edit_metaball_pscale(struct rt_edit *s)
{
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    switch (s->edit_flag) {
	case ECMD_METABALL_SET_THRESHOLD:
	    return ecmd_metaball_set_threshold(s);
	case ECMD_METABALL_SET_METHOD:
	    return ecmd_metaball_set_method(s);
	case ECMD_METABALL_PT_SCALE_BLOBBINESS:
	    return ecmd_metaball_pt_set_goo(s);
	case ECMD_METABALL_PT_SET_BLOBBINESS:
	    return ecmd_metaball_pt_sweat(s);
	case ECMD_METABALL_PT_FLDSTR:
	    return ecmd_metaball_pt_fldstr(s);
    };

    return 0;
}

C_DECL int
rt_edit_metaball_edit(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    m->es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    /* translate solid */
	    m->es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    /* rot solid about vertex */
	    m->es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
	    edit_srot(s);
	    break;
	case ECMD_METABALL_PT_PICK:
	    return ecmd_metaball_pt_pick(s);
	case ECMD_METABALL_PT_MOV:
	    return ecmd_metaball_pt_mov(s);
	case ECMD_METABALL_PT_DEL:
	    return ecmd_metaball_pt_del(s);
	case ECMD_METABALL_PT_ADD:
	    return ecmd_metaball_pt_add(s);
	case ECMD_METABALL_SET_THRESHOLD:
	case ECMD_METABALL_SET_METHOD:
	case ECMD_METABALL_PT_SCALE_BLOBBINESS:
	case ECMD_METABALL_PT_SET_BLOBBINESS:
	case ECMD_METABALL_PT_FLDSTR:
	    return rt_edit_metaball_pscale(s);
	default:
	    return edit_generic(s);
    }

    return 0;
}

C_DECL int
rt_edit_metaball_edit_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_METABALL_PT_NEXT:
	case ECMD_METABALL_PT_PREV:
	case ECMD_METABALL_PT_DEL:
	case ECMD_METABALL_PT_FLDSTR:
	case ECMD_METABALL_PT_SCALE_BLOBBINESS:
	case ECMD_METABALL_PT_SET_BLOBBINESS:
	case ECMD_METABALL_RMET:
	case ECMD_METABALL_SET_METHOD:
	case ECMD_METABALL_SET_THRESHOLD:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_METABALL_PT_PICK:
	case ECMD_METABALL_PT_MOV:
	case ECMD_METABALL_PT_ADD:
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
