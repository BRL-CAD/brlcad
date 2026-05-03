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

void *
rt_edit_metaball_prim_edit_create(struct rt_edit *UNUSED(s))
{
    struct rt_metaball_edit *m;
    BU_GET(m, struct rt_metaball_edit);

    m->es_metaball_pnt = NULL;

    return (void *)m;
}

void
rt_edit_metaball_prim_edit_destroy(struct rt_metaball_edit *m)
{
    if (!m)
	return;

    // Sanity
    m->es_metaball_pnt = NULL;

    BU_PUT(m, struct rt_metaball_edit);
}

void
rt_edit_metaball_prim_edit_reset(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    m->es_metaball_pnt = NULL;
}

void
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
	    /* Mode is set; update axes to selected point before waiting for mouse. */
	    rt_edit_process(s);
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

struct rt_edit_menu_item *
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
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	0.0,                  /* range_min    */
	2.0,                  /* range_max (0=Metaball,1=Isopotential,2=Blob) */
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
	10                    /* display_order */
    },
    {
	ECMD_METABALL_SET_METHOD,    /* cmd_id  */
	"Set Render Method",  /* label        */
	"metaball",           /* category     */
	1,                    /* nparam       */
	metaball_method_params, /* params     */
	1,                    /* interactive  */
	20                    /* display_order */
    },
    /* --- point selection / navigation -------------------------------- */
    {
	ECMD_METABALL_PT_PICK,       /* cmd_id  */
	"Select Point",       /* label        */
	"select",             /* category     */
	1,                    /* nparam       */
	metaball_point_params, /* params      */
	1,                    /* interactive  */
	30                    /* display_order */
    },
    {
	ECMD_METABALL_PT_NEXT,       /* cmd_id  */
	"Next Point",         /* label        */
	"select",             /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	31                    /* display_order */
    },
    {
	ECMD_METABALL_PT_PREV,       /* cmd_id  */
	"Previous Point",     /* label        */
	"select",             /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	32                    /* display_order */
    },
    /* --- point geometry manipulation --------------------------------- */
    {
	ECMD_METABALL_PT_MOV,        /* cmd_id  */
	"Move Point",         /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_point_params, /* params      */
	1,                    /* interactive  */
	40                    /* display_order */
    },
    {
	ECMD_METABALL_PT_ADD,        /* cmd_id  */
	"Add Point",          /* label        */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_point_params, /* params      */
	1,                    /* interactive  */
	50                    /* display_order */
    },
    {
	ECMD_METABALL_PT_DEL,        /* cmd_id  */
	"Delete Point",       /* label        */
	"point",              /* category     */
	0,                    /* nparam       */
	NULL,                 /* params       */
	0,                    /* interactive  */
	60                    /* display_order */
    },
    /* --- per-point scalar parameters --------------------------------- */
    {
	ECMD_METABALL_PT_FLDSTR,     /* cmd_id  */
	"Scale Point Field Strength", /* label  */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_field_strength_params, /* params */
	1,                    /* interactive  */
	70                    /* display_order */
    },
    {
	ECMD_METABALL_PT_SCALE_BLOBBINESS, /* cmd_id */
	"Scale Point Blobbiness", /* label    */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_blobbiness_params, /* params */
	1,                    /* interactive  */
	80                    /* display_order */
    },
    {
	ECMD_METABALL_PT_SET_BLOBBINESS, /* cmd_id */
	"Set Point Blobbiness", /* label      */
	"point",              /* category     */
	1,                    /* nparam       */
	metaball_blobbiness_params, /* params */
	1,                    /* interactive  */
	90                    /* display_order */
    }
};

static const struct rt_edit_prim_desc metaball_prim_desc = {
    "metaball",           /* prim_type    */
    "Metaball",           /* prim_label   */
    11,                   /* ncmd         */
    metaball_cmds         /* cmds         */
};

const struct rt_edit_prim_desc *
rt_edit_metaball_edit_desc(void)
{
    return &metaball_prim_desc;
}


/* get_params: return current value(s) for the given cmd_id */
int
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
	    vals[0] = m->es_metaball_pnt->field_strength;
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
	    VMOVE(vals, m->es_metaball_pnt->coord);
	    return 3;
	default:
	    return 0;
    }
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
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
		      mbpt->field_strength,
		      mbpt->blobbiness);
	n++;
    }
}

#define read_params_line_incr \
    lc = (ln) ? (ln + lcj) : NULL; \
    if (!lc) { \
	bu_free(wc, "wc"); \
	return BRLCAD_ERROR; \
    } \
    ln = strchr(lc, tc); \
    if (ln) *ln = '\0'; \
    while (lc && strchr(lc, ':')) lc++

int
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

    if (!fc)
	return BRLCAD_ERROR;

    const char *crpos = strchr(fc, '\r');
    int crlf = (crpos && crpos[1] == '\n') ? 1 : 0;
    char tc = (crlf) ? '\r' : '\n';
    int lcj = (crlf) ? 2 : 1;

    char *ln = NULL;
    char *wc = bu_strdup(fc);
    char *lc = wc;

    /* method line */
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';
    while (lc && strchr(lc, ':')) lc++;
    {
	int method = 0;
	sscanf(lc, "%d", &method);
	ball->method = method;
    }

    /* threshold line */
    read_params_line_incr;
    {
	double threshold = 0.0;
	sscanf(lc, "%lf", &threshold);
	ball->threshold = threshold;
    }

    /* clear existing points */
    while (BU_LIST_NON_EMPTY(&ball->metaball_ctrl_head)) {
	struct wdb_metaball_pnt *pt =
	    BU_LIST_FIRST(wdb_metaball_pnt, &ball->metaball_ctrl_head);
	BU_LIST_DQ(&pt->l);
	BU_PUT(pt, struct wdb_metaball_pnt);
    }

    /* read point lines: "point[N]: x y z field_strength=fs blobbiness=bl" */
    while (1) {
	lc = (ln) ? (ln + lcj) : NULL;
	if (!lc || *lc == '\0') break;
	ln = strchr(lc, tc);
	if (ln) *ln = '\0';

	/* Only process lines that start with "point[" */
	if (!bu_strncmp(lc, "point[", 6)) {
	    /* skip to closing bracket, then past the colon separator */
	    const char *after = strchr(lc, ']');
	    if (!after) break;
	    const char *coords = strchr(after, ':');
	    if (!coords) break;
	    coords++; /* step past ':' */
	    double x = 0.0, y = 0.0, z = 0.0, fs = 1.0, bl = 1.0;
	    int nread = sscanf(coords,
			      " %lf %lf %lf field_strength=%lf blobbiness=%lf",
			      &x, &y, &z, &fs, &bl);
	    if (nread < 3) {
		bu_log("rt_edit_metaball_read_params: malformed point line, skipping\n");
		continue;
	    }
	    point_t loc;
	    VSET(loc, x * local2base, y * local2base, z * local2base);
	    rt_metaball_add_point(ball, (const point_t *)&loc, (fastf_t)fs, (fastf_t)bl);
	}
    }

    bu_free(wc, "wc");
    return BRLCAD_OK;
}


void
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

const char *
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
    if (s->e_para[0] < 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR < 0\n");
	s->e_inpara = 0;
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
    if (s->e_para[0] < 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR < 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct rt_metaball_internal *ball =
	(struct rt_metaball_internal *)s->es_int.idb_ptr;
    RT_METABALL_CK_MAGIC(ball);
    ball->method = s->e_para[0];

    return 0;
}

int
ecmd_metaball_pt_set_goo(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    if (s->e_para[0] < 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR < 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (!m->es_metaball_pnt || !s->e_inpara) {
	bu_vls_printf(s->log_str, "pscale: no metaball point selected for scaling blobbiness\n");
	return BRLCAD_ERROR;
    }
    m->es_metaball_pnt->blobbiness *= *s->e_para * ((s->es_scale > -SMALL_FASTF) ? s->es_scale : 1.0);

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
    if (s->e_para[0] < 0.0) {
	bu_vls_printf(s->log_str, "ERROR: blobbiness value must be >= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    m->es_metaball_pnt->blobbiness = s->e_para[0];
    return 0;
}

int
ecmd_metaball_pt_fldstr(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    if (s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (!m->es_metaball_pnt || !s->e_inpara) {
	bu_vls_printf(s->log_str, "pscale: no metaball point selected for scaling strength\n");
	return BRLCAD_ERROR;
    }

    m->es_metaball_pnt->field_strength *= *s->e_para * ((s->es_scale > -SMALL_FASTF) ? s->es_scale : 1.0);

    return 0;
}

void
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
	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;
	VMOVE(new_pt, s->e_para);
    } else if (s->e_inpara) {
	bu_vls_printf(s->log_str, "x y z coordinates required for control point selection\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    } else {
	return;
    }

    /* get a direction vector in model space corresponding to z-direction in view */
    VSET(work, 0.0, 0.0, 1.0);
    MAT4X3VEC(dir, s->vp->gv_view2model, work);

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
    } else {
	rt_metaball_pnt_print(m->es_metaball_pnt, s->base2local);
    }
}

void
ecmd_metaball_pt_mov(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    if (!m->es_metaball_pnt) {
	bu_log("Must select a point to move"); return; }
    if (s->e_inpara != 3) {
	bu_log("Must provide dx dy dz"); return; }
    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;
    VADD2(m->es_metaball_pnt->coord, m->es_metaball_pnt->coord, s->e_para);
}

void
ecmd_metaball_pt_del(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    struct wdb_metaball_pnt *tmp = m->es_metaball_pnt, *p;

    if (m->es_metaball_pnt == NULL) {
	bu_log("No point selected");
	return;
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
}

void
ecmd_metaball_pt_add(struct rt_edit *s)
{
    struct rt_metaball_edit *m = (struct rt_metaball_edit *)s->ipe_ptr;
    struct rt_metaball_internal *metaball= (struct rt_metaball_internal *)s->es_int.idb_ptr;
    struct wdb_metaball_pnt *n;
    BU_GET(n, struct wdb_metaball_pnt);

    if (s->e_inpara != 3) {
	bu_log("Must provide x y z");
	BU_PUT(n, struct wdb_metaball_pnt);
	return;
    }

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    m->es_metaball_pnt = BU_LIST_FIRST(wdb_metaball_pnt, &metaball->metaball_ctrl_head);
    VMOVE(n->coord, s->e_para);
    n->l.magic = WDB_METABALLPT_MAGIC;
    n->field_strength = 1.0;
    BU_LIST_APPEND(&m->es_metaball_pnt->l, &n->l);
    m->es_metaball_pnt = n;
}

static int
rt_edit_metaball_pscale(struct rt_edit *s)
{
    if (s->e_inpara > 1) {
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

int
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
	    ecmd_metaball_pt_pick(s);
	    break;
	case ECMD_METABALL_PT_MOV:
	    ecmd_metaball_pt_mov(s);
	    break;
	case ECMD_METABALL_PT_DEL:
	    ecmd_metaball_pt_del(s);
	    break;
	case ECMD_METABALL_PT_ADD:
	    ecmd_metaball_pt_add(s);
	    break;
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

int
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
