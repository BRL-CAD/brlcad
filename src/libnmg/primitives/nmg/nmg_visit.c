/*                     N M G _ V I S I T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nmg */
/** @{ */
/** @file primitives/nmg/nmg_visit.c
 *
 * A generalized, object-oriented subroutine family to
 * visit all the data structures "below" a given structure.
 *
 * The caller provides a pointer to the structure to start at,
 * a table of "handlers" for each kind of structure,
 * and a generic pointer for private state which will be sent along
 * to the user's handlers.
 * For non-leaf structures, there are two handlers, one called
 * before any recursion starts, and the other called when
 * recursion is finished.  Either or both may be omitted.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"


void
nmg_visit_vertex(struct vertex *v, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    NMG_CK_VERTEX(v);

    if (htab->vis_vertex) htab->vis_vertex((uint32_t *)v, state, 0);

    if (htab->vis_vertex_g && v->vg_p)
	htab->vis_vertex_g((uint32_t *)v->vg_p, state, 0);
}


void
nmg_visit_vertexuse(struct vertexuse *vu, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    NMG_CK_VERTEXUSE(vu);

    if (htab->bef_vertexuse) htab->bef_vertexuse((uint32_t *)vu, state, 0);

    nmg_visit_vertex(vu->v_p, htab, state);

    if (htab->vis_vertexuse_a && vu->a.magic_p)
	htab->vis_vertexuse_a((uint32_t *)vu->a.magic_p, state, 0);

    if (htab->aft_vertexuse) htab->aft_vertexuse((uint32_t *)vu, state, 1);
}


void
nmg_visit_edge(struct edge *e, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    NMG_CK_EDGE(e);

    if (htab->vis_edge) htab->vis_edge((uint32_t *)e, state, 0);
}


void
nmg_visit_edgeuse(struct edgeuse *eu, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    NMG_CK_EDGEUSE(eu);

    if (htab->bef_edgeuse) htab->bef_edgeuse((uint32_t *)eu, state, 0);

    nmg_visit_vertexuse(eu->vu_p, htab, state);
    nmg_visit_edge(eu->e_p, htab, state);

    if (htab->vis_edge_g && eu->g.magic_p)
	htab->vis_edge_g((uint32_t *)eu->g.magic_p, state, 0);

    if (htab->aft_edgeuse) htab->aft_edgeuse((uint32_t *)eu, state, 1);
}


void
nmg_visit_loop(struct loop *l, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    NMG_CK_LOOP(l);

    if (htab->vis_loop) htab->vis_loop((uint32_t *)l, state, 0);

    if (htab->vis_loop_g && l->lg_p)
	htab->vis_loop_g((uint32_t *)l->lg_p, state, 0);
}


void
nmg_visit_loopuse(struct loopuse *lu, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    NMG_CK_LOOPUSE(lu);

    if (htab->bef_loopuse) htab->bef_loopuse((uint32_t *)lu, state, 0);

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	struct vertexuse *vu;
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	nmg_visit_vertexuse(vu, htab, state);
    } else {
	struct edgeuse *eu;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    nmg_visit_edgeuse(eu, htab, state);
	}
    }
    nmg_visit_loop(lu->l_p, htab, state);

    if (htab->aft_loopuse) htab->aft_loopuse((uint32_t *)lu, state, 1);
}


void
nmg_visit_face(struct face *f, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{

    if (htab->vis_face) htab->vis_face((uint32_t *)f, state, 0);

    if (htab->vis_face_g && f->g.plane_p)
	htab->vis_face_g((uint32_t *)f->g.plane_p, state, 0);
}


void
nmg_visit_faceuse(struct faceuse *fu, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    struct loopuse *lu;

    NMG_CK_FACEUSE(fu);

    if (htab->bef_faceuse) htab->bef_faceuse((uint32_t *)fu, state, 0);

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	nmg_visit_loopuse(lu, htab, state);
    }

    nmg_visit_face(fu->f_p, htab, state);

    if (htab->aft_faceuse) htab->aft_faceuse((uint32_t *)fu, state, 1);
}


void
nmg_visit_shell(struct shell *s, const struct nmg_visit_handlers *htab, void *state)


/* Handler's private state */
{
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;

    NMG_CK_SHELL(s);

    if (htab->bef_shell) htab->bef_shell((uint32_t *)s, state, 0);

    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	nmg_visit_faceuse(fu, htab, state);
    }
    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	nmg_visit_loopuse(lu, htab, state);
    }
    for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
	nmg_visit_edgeuse(eu, htab, state);
    }
    if (s->vu_p) nmg_visit_vertexuse(s->vu_p, htab, state);
    if (htab->vis_shell_a && s->sa_p)
	htab->vis_shell_a((uint32_t *)s->sa_p, state, 0);

    if (htab->aft_shell) htab->aft_shell((uint32_t *)s, state, 1);
}


void
nmg_visit(const uint32_t *magicp, const struct nmg_visit_handlers *htab, void *state)
/* Handler's private state */
{
    switch (*magicp) {
	default:
	    bu_log("nmg_visit() Can't visit %s directly\n", bu_identify_magic(*magicp));
	    bu_bomb("nmg_visit()\n");
	    /* NOTREACHED */
	case NMG_SHELL_MAGIC:
	    nmg_visit_shell((struct shell *)magicp, htab, state);
	    break;
	case NMG_FACEUSE_MAGIC:
	    nmg_visit_faceuse((struct faceuse *)magicp, htab, state);
	    break;
	case NMG_LOOPUSE_MAGIC:
	    nmg_visit_loopuse((struct loopuse *)magicp, htab, state);
	    break;
	case NMG_EDGEUSE_MAGIC:
	    nmg_visit_edgeuse((struct edgeuse *)magicp, htab, state);
	    break;
	case NMG_VERTEXUSE_MAGIC:
	    nmg_visit_vertexuse((struct vertexuse *)magicp, htab, state);
	    break;
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
