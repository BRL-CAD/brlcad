/*                         E D P A R T . C
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
/** @file primitives/edpart.c
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

#define ECMD_PART_H		16088
#define ECMD_PART_v		16089
#define ECMD_PART_h		16090

static void
part_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_solid_edit_set_edflag(s, arg);

    switch (arg) {
	case ECMD_PART_H:
	case ECMD_PART_v:
	case ECMD_PART_h:
	    s->solid_edit_scale = 1;
	    break;
	default:
	    break;
    };

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item part_menu[] = {
    { "Particle MENU", NULL, 0 },
    { "Set H", part_ed, ECMD_PART_H },
    { "Set v", part_ed, ECMD_PART_v },
    { "Set h", part_ed, ECMD_PART_h },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *
rt_solid_edit_part_menu_item(const struct bn_tol *UNUSED(tol))
{
    return part_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
rt_solid_edit_part_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_part_internal *part = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(part);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(part->part_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(part->part_H));
    bu_vls_printf(p, "v radius: %.9f\n", part->part_vrad * base2local);
    bu_vls_printf(p, "h radius: %.9f\n", part->part_hrad * base2local);
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
rt_solid_edit_part_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_part_internal *part = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(part);

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

    // Read the numbers
    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(part->part_V, a, b, c);
    VSCALE(part->part_V, part->part_V, local2base);

    // Set up Height line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(part->part_H, a, b, c);
    VSCALE(part->part_H, part->part_H, local2base);

    // Set up v radius line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf", &a);
    part->part_vrad = a * local2base;

    // Set up h radius line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf", &a);
    part->part_hrad = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale vector H */
void
ecmd_part_h(struct rt_solid_edit *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(part->part_H);
    }
    VSCALE(part->part_H, part->part_H, s->es_scale);
}

/* scale v end radius */
void
ecmd_part_v(struct rt_solid_edit *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / part->part_vrad;
    }
    part->part_vrad *= s->es_scale;
}

/* scale h end radius */
void
ecmd_part_h_end_r(struct rt_solid_edit *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / part->part_hrad;
    }
    part->part_hrad *= s->es_scale;
}

static int
rt_solid_edit_part_pscale(struct rt_solid_edit *s)
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
	case ECMD_PART_H:
	    ecmd_part_h(s);
	    break;
	case ECMD_PART_v:
	    ecmd_part_v(s);
	    break;
	case ECMD_PART_h:
	    ecmd_part_h_end_r(s);
	    break;
    };

    return 0;
}

int
rt_solid_edit_part_edit(struct rt_solid_edit *s)
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
	    return rt_solid_edit_part_pscale(s);
    }

    return 0;
}

int
rt_solid_edit_part_edit_xy(
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
	case ECMD_PART_H:
	case ECMD_PART_v:
	case ECMD_PART_h:
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
