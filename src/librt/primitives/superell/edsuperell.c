/*                         E D S U P E R E L L . C
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
/** @file primitives/edsuperell.c
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

#define ECMD_SUPERELL_SCALE_A	35113
#define ECMD_SUPERELL_SCALE_B	35114
#define ECMD_SUPERELL_SCALE_C	35115
#define ECMD_SUPERELL_SCALE_ABC	35116

static void
superell_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_SCALE);
    s->edit_flag = arg;

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item superell_menu[] = {
    { "SUPERELLIPSOID MENU", NULL, 0 },
    { "Set A", superell_ed, ECMD_SUPERELL_SCALE_A },
    { "Set B", superell_ed, ECMD_SUPERELL_SCALE_B },
    { "Set C", superell_ed, ECMD_SUPERELL_SCALE_C },
    { "Set A,B,C", superell_ed, ECMD_SUPERELL_SCALE_ABC },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *
rt_solid_edit_superell_menu_item(const struct bn_tol *UNUSED(tol))
{
    return superell_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
rt_solid_edit_superell_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(superell->v));
    bu_vls_printf(p, "A: %.9f %.9f %.9f\n", V3BASE2LOCAL(superell->a));
    bu_vls_printf(p, "B: %.9f %.9f %.9f\n", V3BASE2LOCAL(superell->b));
    bu_vls_printf(p, "C: %.9f %.9f %.9f\n", V3BASE2LOCAL(superell->c));
    bu_vls_printf(p, "<n, e>: <%.9f, %.9f>\n", superell->n, superell->e);
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
rt_solid_edit_superell_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);

    if (!fc)
	return BRLCAD_ERROR;

    // We're getting the file contents as a string, so we need to split it up
    // to process lines. See https://stackoverflow.com/a/17983619

    // Figure out if we need to deal with Windows line endings
    const char *crpos = strchr(fc, '\r');
    int crlf = (crpos && crpos[1] == '\n') ? 1 : 0;
    char tc = (crlf) ? '\r' : '\n';
    // If we're CRLF jump ahead another character.
    int lcj = (crlf) ? 2 : 1;

    char *ln = NULL;
    char *wc = bu_strdup(fc);
    char *lc = wc;

    // Set up initial line (Vertex)
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(superell->v, a, b, c);
    VSCALE(superell->v, superell->v, local2base);

    // Set up A line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(superell->a, a, b, c);
    VSCALE(superell->a, superell->a, local2base);

    // Set up B line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(superell->b, a, b, c);
    VSCALE(superell->b, superell->b, local2base);

    // Set up C line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(superell->c, a, b, c);
    VSCALE(superell->c, superell->c, local2base);

    // Set up n, e line
    read_params_line_incr;

    sscanf(lc, "%lf %lf", &superell->n, &superell->e);

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale vector A */
void
ecmd_superell_scale_a(struct rt_solid_edit *s)
{
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->es_scale = s->e_para[0] * s->e_mat[15] /
	    MAGNITUDE(superell->a);
    }
    VSCALE(superell->a, superell->a, s->es_scale);
}

/* scale vector B */
void
ecmd_superell_scale_b(struct rt_solid_edit *s)
{
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->es_scale = s->e_para[0] * s->e_mat[15] /
	    MAGNITUDE(superell->b);
    }
    VSCALE(superell->b, superell->b, s->es_scale);
}

/* scale vector C */
void
ecmd_superell_scale_c(struct rt_solid_edit *s)
{
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->es_scale = s->e_para[0] * s->e_mat[15] /
	    MAGNITUDE(superell->c);
    }
    VSCALE(superell->c, superell->c, s->es_scale);
}

/* set A, B, and C length the same */
void
ecmd_superell_scale_abc(struct rt_solid_edit *s)
{
    fastf_t ma, mb;
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->es_scale = s->e_para[0] * s->e_mat[15] /
	    MAGNITUDE(superell->a);
    }
    VSCALE(superell->a, superell->a, s->es_scale);
    ma = MAGNITUDE(superell->a);
    mb = MAGNITUDE(superell->b);
    VSCALE(superell->b, superell->b, ma/mb);
    mb = MAGNITUDE(superell->c);
    VSCALE(superell->c, superell->c, ma/mb);
}

static int
rt_solid_edit_superell_pscale(struct rt_solid_edit *s)
{
    if (s->e_inpara > 1) {
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

    switch (s->edit_flag) {
	case ECMD_SUPERELL_SCALE_A:
	    ecmd_superell_scale_a(s);
	    break;
	case ECMD_SUPERELL_SCALE_B:
	    ecmd_superell_scale_b(s);
	    break;
	case ECMD_SUPERELL_SCALE_C:
	    ecmd_superell_scale_c(s);
	    break;
	case ECMD_SUPERELL_SCALE_ABC:
	    ecmd_superell_scale_abc(s);
	    break;
    };

    return 0;
}

int
rt_solid_edit_superell_edit(struct rt_solid_edit *s)
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
	default:
	    return rt_solid_edit_superell_pscale(s);
    }
    return 0;
}

int
rt_solid_edit_superell_edit_xy(
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
	case ECMD_SUPERELL_SCALE_A:
	case ECMD_SUPERELL_SCALE_B:
	case ECMD_SUPERELL_SCALE_C:
	case ECMD_SUPERELL_SCALE_ABC:
            rt_solid_edit_generic_sscale_xy(s, mousevec);
            return 0;
        case RT_SOLID_EDIT_TRANS:
            rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
            rt_update_edit_absolute_tran(s, pos_view);
            return 0;
        default:
            bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
            rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
            if (f)
                (*f)(0, NULL, d, NULL);
            return BRLCAD_ERROR;
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
