/*                     N M G _ T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2024 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/nmg/nmg.c
 *
 * Convert an NMG solid to a BoT.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/cv.h"
#include "bg/polygon.h"
#include "nmg.h"
#include "rt/db4.h"
#include "rt/conv.h"
#include "rt/nmg_conv.h"
#include "raytrace.h"
#include "../../librt_private.h"

/**
 * XXX This routine is deprecated in favor of BoTs
 */
int
nmg_to_poly(const struct model *m, struct rt_pg_internal *poly_int, struct bu_list *vlfree, const struct bn_tol *tol)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct model *dup_m;
    struct nmgregion *dup_r;
    struct shell *dup_s;
    int max_count;
    int count_npts;
    int face_count=0;

    NMG_CK_MODEL(m);

    BN_CK_TOL(tol);

    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    if (nmg_check_closed_shell(s, tol))
		return 0;
	}
    }

    dup_m = nmg_mm();
    dup_r = nmg_mrsv(dup_m);
    dup_s = BU_LIST_FIRST(shell, &dup_r->s_hd);

    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
		if (fu->orientation != OT_SAME)
		    continue;
		(void)nmg_dup_face(fu, dup_s);
	    }
	}
    }

    for (BU_LIST_FOR (dup_r, nmgregion, &dup_m->r_hd)) {
	for (BU_LIST_FOR (dup_s, shell, &dup_r->s_hd)) {
	    for (BU_LIST_FOR (fu, faceuse, &dup_s->fu_hd)) {
		NMG_CK_FACEUSE(fu);

		/* only do OT_SAME faces */
		if (fu->orientation != OT_SAME)
		    continue;

		/* count vertices in loops */
		max_count = 0;
		for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		    if (lu->orientation != OT_SAME) {
			/* triangulate holes */
			max_count = 6;
			break;
		    }

		    count_npts = 0;
		    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
			count_npts++;

		    if (count_npts > 5) {
			max_count = count_npts;
			break;
		    }
		    if (!nmg_lu_is_convex(lu, vlfree, tol)) {
			/* triangulate non-convex faces */
			max_count = 6;
			break;
		    }
		}

		/* if any loop has more than 5 vertices, triangulate the face */
		if (max_count > 5) {
		    if (nmg_debug & NMG_DEBUG_BASIC)
			bu_log("nmg_to_poly: triangulating fu %p\n", (void *)fu);
		    nmg_triangulate_fu(fu, vlfree, tol);
		}

		for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		    face_count++;
		}
	    }
	}
    }
    poly_int->npoly = face_count;
    poly_int->poly = (struct rt_pg_face_internal *)bu_calloc(face_count,
							     sizeof(struct rt_pg_face_internal), "nmg_to_poly: poly");

    face_count = 0;
    for (BU_LIST_FOR (dup_r, nmgregion, &dup_m->r_hd)) {
	for (BU_LIST_FOR (dup_s, shell, &dup_r->s_hd)) {
	    for (BU_LIST_FOR (fu, faceuse, &dup_s->fu_hd)) {
		vect_t norm;

		NMG_CK_FACEUSE(fu);

		/* only do OT_SAME faces */
		if (fu->orientation != OT_SAME)
		    continue;

		NMG_GET_FU_NORMAL(norm, fu);

		for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		    int pt_no=0;

		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		    /* count vertices in loop */
		    count_npts = 0;
		    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
			count_npts++;

		    poly_int->poly[face_count].npts = count_npts;
		    poly_int->poly[face_count].verts = (fastf_t *) bu_calloc(3*count_npts, sizeof(fastf_t), "nmg_to_poly: verts");
		    poly_int->poly[face_count].norms = (fastf_t *) bu_calloc(3*count_npts, sizeof(fastf_t), "nmg_to_poly: norms");

		    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
			struct vertex_g *vg;

			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);

			VMOVE(&(poly_int->poly[face_count].verts[pt_no*3]), vg->coord);
			VMOVE(&(poly_int->poly[face_count].norms[pt_no*3]), norm);

			pt_no++;
		    }
		    face_count++;
		}
	    }
	}
    }

    poly_int->magic = RT_PG_INTERNAL_MAGIC;
    nmg_km(dup_m);

    return 1;
}


static int
_nmg_shell_append(struct rt_bot_internal *bot, struct bu_ptbl *nmg_vertices, struct bu_ptbl *nmg_faces, size_t *voffset, size_t *foffset)
{
    size_t i;
    size_t vo = (voffset) ? (unsigned int)*voffset : 0;
    size_t fo = (foffset) ? (unsigned int)*foffset : 0;
    size_t face_no;
    struct vertex *v;

    /* fill in the vertices */
    for (i = 0; i < BU_PTBL_LEN(nmg_vertices); i++) {
	struct vertex_g *vg;

	v = (struct vertex *)BU_PTBL_GET(nmg_vertices, i);
	NMG_CK_VERTEX(v);

	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);

	VMOVE(&bot->vertices[(i + vo)*3], vg->coord);
    }
    if (voffset) (*voffset) += (int)BU_PTBL_LEN(nmg_vertices);

    /* fill in the faces */
    face_no = 0;
    for (i = 0; i < BU_PTBL_LEN(nmg_faces); i++) {
	struct face *f;
	struct faceuse *fu;
	struct loopuse *lu;

	f = (struct face *)BU_PTBL_GET(nmg_faces, i);
	NMG_CK_FACE(f);

	fu = f->fu_p;

	if (fu->orientation != OT_SAME) {
	    fu = fu->fumate_p;
	    if (fu->orientation != OT_SAME) {
		bu_log("nmg_bot(): Face has no OT_SAME use!\n");
		return -1;
	    }
	}

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;
	    size_t vertex_no=0;

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		if (vertex_no > 2) {
		    bu_log("nmg_bot(): Face has not been triangulated!\n");
		    return -1;
		}

		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);

		bot->faces[ (face_no + fo)*3 + vertex_no ] = bu_ptbl_locate(nmg_vertices, (long *)v);

		vertex_no++;
	    }

	    face_no++;
	}
    }

    if (foffset) (*foffset) += face_no;

    return 0;
}

static size_t
_nmg_tri_count(struct bu_ptbl *nmg_faces)
{
    size_t i;
    size_t num_faces = 0;
    /* count the number of triangles */
    for (i=0; i<BU_PTBL_LEN(nmg_faces); i++) {
	struct face *f;
	struct faceuse *fu;
	struct loopuse *lu;

	f = (struct face *)BU_PTBL_GET(nmg_faces, i);
	NMG_CK_FACE(f);

	fu = f->fu_p;

	if (fu->orientation != OT_SAME) {
	    fu = fu->fumate_p;
	    if (fu->orientation != OT_SAME) {
		return 0;
	    }
	}

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;
	    num_faces++;
	}
    }
    return num_faces;
}

/**
 * Convert an NMG to a BOT solid
 */
struct rt_bot_internal *
nmg_bot(struct shell *s, struct bu_list *vlfree, const struct bn_tol *tol)
{
    struct rt_bot_internal *bot;
    struct bu_ptbl nmg_vertices;
    struct bu_ptbl nmg_faces;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    /* first convert the NMG to triangles */
    (void)nmg_triangulate_shell(s, vlfree, tol);

    /* make a list of all the vertices */
    nmg_vertex_tabulate(&nmg_vertices, &s->l.magic, vlfree);

    /* and a list of all the faces */
    nmg_face_tabulate(&nmg_faces, &s->l.magic, vlfree);

    /* now build the BOT */
    BU_ALLOC(bot, struct rt_bot_internal);

    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;

    bot->num_vertices = BU_PTBL_LEN(&nmg_vertices);
    bot->num_faces = 0;

    /* count the number of triangles */
    bot->num_faces = _nmg_tri_count(&nmg_faces);

    bot->faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "BOT faces");
    bot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "BOT vertices");

    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    if (_nmg_shell_append(bot, &nmg_vertices, &nmg_faces, NULL, NULL) < 0) {
	bu_log("nmg_bot(): Failed to process shell!\n");
	return (struct rt_bot_internal *)NULL;
    }

    bu_ptbl_free(&nmg_vertices);
    bu_ptbl_free(&nmg_faces);

    return bot;
}

static void
_nmg_shell_tabulate(struct bu_ptbl *va, struct bu_ptbl *fa, struct shell *s, struct bu_list *vlfree)
{
    struct bu_ptbl *nmg_vertices, *nmg_faces;

    NMG_CK_SHELL(s);

    BU_ALLOC(nmg_vertices, struct bu_ptbl);
    BU_ALLOC(nmg_faces, struct bu_ptbl);

    /* make a list of all the vertices */
    nmg_vertex_tabulate(nmg_vertices, &s->l.magic, vlfree);
    bu_ptbl_ins(va, (long *)nmg_vertices);
    /* and a list of all the faces */
    nmg_face_tabulate(nmg_faces, &s->l.magic, vlfree);

    bu_ptbl_ins(fa, (long *)nmg_faces);
}


/**
 * Convert all shells of an NMG to a BOT solid
 */
struct rt_bot_internal *
nmg_mdl_to_bot(struct model *m, struct bu_list *vlfree, const struct bn_tol *tol)
{
    unsigned int i = 0;
    struct nmgregion *r;
    struct shell *s;
    struct rt_bot_internal *bot;
    struct bu_ptbl vert_arrays = BU_PTBL_INIT_ZERO;
    struct bu_ptbl face_arrays = BU_PTBL_INIT_ZERO;
    size_t vert_cnt = 0;
    size_t face_cnt = 0;

    BN_CK_TOL(tol);
    NMG_CK_MODEL(m);

    /* first convert the NMG to triangles */
    nmg_triangulate_model(m, vlfree, tol);

    /* For each shell, tabulate faces and vertices */
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    _nmg_shell_tabulate(&vert_arrays, &face_arrays, s, vlfree);
	}
    }

    /* Count up the verts */
    for (i = 0; i < BU_PTBL_LEN(&vert_arrays); i++) {
	struct bu_ptbl *vac = (struct bu_ptbl *)BU_PTBL_GET(&vert_arrays, i);
	vert_cnt += BU_PTBL_LEN(vac);
    }

    /* Count up the faces */
    for (i = 0; i < BU_PTBL_LEN(&face_arrays); i++) {
	struct bu_ptbl *nfaces = (struct bu_ptbl *)BU_PTBL_GET(&face_arrays, i);
	face_cnt += _nmg_tri_count(nfaces);
    }

    /* now build the BOT */
    BU_ALLOC(bot, struct rt_bot_internal);

    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;

    bot->num_vertices = vert_cnt;
    bot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "BOT vertices");
    bot->num_faces = face_cnt;
    bot->faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "BOT faces");

    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    vert_cnt = 0;
    face_cnt = 0;
    for (i = 0; i < BU_PTBL_LEN(&vert_arrays); i++) {
	struct bu_ptbl *va = (struct bu_ptbl *)BU_PTBL_GET(&vert_arrays, i);
	struct bu_ptbl *fa = (struct bu_ptbl *)BU_PTBL_GET(&face_arrays, i);
	if (_nmg_shell_append(bot, va, fa, &vert_cnt, &face_cnt) < 0) {
	    bu_log("nmg_mdl_to_bot(): Failed to process shell!\n");
	    bu_free((char *)bot->vertices, "BOT vertices");
	    bu_free((char *)bot->faces, "BOT faces");
	    bu_free((char *)bot, "BOT");
	    return (struct rt_bot_internal *)NULL;
	}
    }

    for (i = 0; i < BU_PTBL_LEN(&vert_arrays); i++) {
	struct bu_ptbl *va = (struct bu_ptbl *)BU_PTBL_GET(&vert_arrays, i);
	struct bu_ptbl *fa = (struct bu_ptbl *)BU_PTBL_GET(&face_arrays, i);
	bu_ptbl_free(va);
	bu_ptbl_free(fa);
	bu_free(va, "va container");
	bu_free(fa, "va container");
    }
    bu_ptbl_free(&vert_arrays);
    bu_ptbl_free(&face_arrays);
    return bot;
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
