/*                    B O T T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

/** @file libgcv/region_end.c
 *
 * Generate a tree of resolved BoT's, one per region. Roughly based
 * on UnBBoolean's j3dbool (and associated papers).
 *
 */

#include "common.h"

#include <math.h>	/* ceil */
#include <string.h>	/* memcpy */

#include "bn.h"
#include "raytrace.h"
#include "nmg.h"
#include "gcv.h"

void rt_bot_ifree2(struct rt_bot_internal *);

/* hijack the top four bits of mode. For these operations, the mode should
 * necessarily be 0x02 */
#define INSIDE		0x01
#define OUTSIDE		0x02
#define SAME		0x04
#define OPPOSITE	0x08
#define INVERTED	0x10

#define SOUP_MAGIC	0x534F5550	/* SOUP */
#define SOUP_CKMAG(_ptr) BU_CKMAG(_ptr, SOUP_MAGIC, "soup")

struct face_s {
    point_t vert[3], min, max;
    plane_t plane;
    uint32_t foo;
};

struct soup_s {
    unsigned long magic;
    struct face_s *faces;
    unsigned long int nfaces, maxfaces;
};

/* assume 4096, seems common enough. a portable way to get to PAGE_SIZE might be
 * better. */
static const int faces_per_page = 4096 / sizeof(struct face_s);


static int
soup_rm_face(struct soup_s *s, unsigned long int i)
{
    if(i>=s->nfaces) {
	bu_log("trying to remove a nonexisant face? %d/%d\n", i, s->nfaces);
	bu_bomb("Asploding\n");
    }
    memcpy(&s->faces[i], &s->faces[s->nfaces-1], sizeof(struct face_s));
    return s->nfaces--;
}

static int
soup_add_face(struct soup_s *s, point_t a, point_t b, point_t c, const struct bn_tol *tol) {
    struct face_s *f;

    /* grow face array if needed */
    if(s->nfaces >= s->maxfaces) {
	bu_log("Resizing, %d aint' enough\n", s->nfaces);
	s->faces = bu_realloc(s->faces, (s->maxfaces += faces_per_page) * sizeof(struct face_s), "bot soup faces");
    }
    f = s->faces + s->nfaces;

    VMOVE(f->vert[0], a);
    VMOVE(f->vert[1], b);
    VMOVE(f->vert[2], c);

    /* solve the bounding box (should this be VMINMAX?) */
    VMOVE(f->min, f->vert[0]); VMOVE(f->max, f->vert[0]);
    VMIN(f->min, f->vert[1]); VMAX(f->max, f->vert[1]);
    VMIN(f->min, f->vert[2]); VMAX(f->max, f->vert[2]);
    /* fluff the bounding box for fp fuzz */
    f->min[X]-=.1; f->min[Y]-=.1; f->min[Z]-=.1;
    f->max[X]+=.1; f->max[Y]+=.1; f->max[Z]+=.1;

    /* solve the plane */
    bn_mk_plane_3pts(f->plane, f->vert[0], f->vert[1], f->vert[2], tol);

    /* set flags */
    f->foo = OUTSIDE;
    s->nfaces++;
    return 0;
}

static int
split_face(struct soup_s *left, unsigned long int left_face, struct soup_s *right, unsigned long int right_face, const struct bn_tol *tol) {
    struct face_s *lf, *rf;
    fastf_t dot;

    lf = left->faces+left_face;
    rf = right->faces+right_face;

    dot = VDOT(lf->plane, rf->plane);

    if(BN_VECT_ARE_PARALLEL(dot, tol)) {
	if(!NEAR_EQUAL(lf->plane[H],rf->plane[H],tol->dist))
	    return 1;	/* parallel and not colocated. */
	/* hm, colocated, need to figure out how to splice correctly? */
	return 2;
    }

    /* test if the intersect line is on both triangles,  if so, alter the soup */

    return 0;
}

static struct soup_s *
bot2soup(struct rt_bot_internal *bot, const struct bn_tol *tol)
{
    struct soup_s *s;
    unsigned long int i;

    RT_BOT_CK_MAGIC(bot);

    if(bot->orientation != RT_BOT_CCW)
	bu_bomb("Bad orientation out of nmg_bot\n");

    s = bu_malloc(sizeof(struct soup_s), "bot soup");
    s->magic = SOUP_MAGIC;
    s->nfaces = 0;
    s->maxfaces = ceil(bot->num_faces / (double)faces_per_page) * faces_per_page;
    s->faces = bu_malloc(sizeof(struct face_s) * s->maxfaces, "bot soup faces");

    for(i=0;i<bot->num_faces;i++)
	soup_add_face(s, bot->vertices+3*bot->faces[i*3+0], bot->vertices+3*bot->faces[i*3+1], bot->vertices+3*bot->faces[i*3+2], tol);

    return s;
}

static void
free_soup(struct soup_s *s) {
    if(s == NULL)
	bu_bomb("null soup");
    if(s->faces)
	bu_free(s->faces, "bot soup faces");
    bu_free(s, "bot soup");
    return;
}

static union tree *
invert(union tree *tree)
{
    struct soup_s *s;
    unsigned long int i;

    RT_CK_TREE(tree);
    if(tree->tr_op != OP_NMG_TESS) {
	bu_log("Erm, this isn't an nmg tess\n");
	return tree;
    }
    s = (struct soup_s *)tree->tr_d.td_r->m_p;
    SOUP_CKMAG(s);

    for(i=0;i<s->nfaces;i++) {
	struct face_s *f = s->faces+i;
	point_t t;
	VMOVE(t, f->vert[0]);
	VMOVE(f->vert[0], f->vert[1]);
	VMOVE(f->vert[0], t);
	/* flip the inverted bit. */
	f->foo=(f->foo&~INVERTED)|((~(f->foo&INVERTED))&INVERTED);
    }

    return tree;
}

static void
split_faces(union tree *left_tree, union tree *right_tree, const struct bn_tol *tol)
{
    struct soup_s *l, *r;
    unsigned long int i, j;

    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct soup_s *)left_tree->tr_d.td_r->m_p;
    r = (struct soup_s *)right_tree->tr_d.td_r->m_p;
    SOUP_CKMAG(l);
    SOUP_CKMAG(r);

    /* this is going to be big and hairy. Has to walk both meshes finding
     * all intersections and split intersecting faces so there are edges at
     * the intersections. Initially going to be O(n^2), then change to use
     * space partitioning (binning? octree? kd?). */
    for(i=0;i<l->nfaces;i++) {
	struct face_s *lf = l->faces+i, *rf = NULL;
	for(j=0;j<r->nfaces;j++) {
	    rf = r->faces+j;
	    /* quick bounding box test */
	    if(lf->min[X]>rf->max[X] || lf->max[X]>lf->max[X] ||
		lf->min[Y]>rf->max[Y] || lf->max[Y]>lf->max[Y] ||
		lf->min[Z]>rf->max[Z] || lf->max[Z]>lf->max[Z])
		continue;
	    /* two possibly overlapping faces found */
	    split_face(l, i, r, j, tol);
	}
    }
}

static void
classify_faces(union tree *left_tree, union tree *right_tree)
{
    struct soup_s *l, *r;
    unsigned long int i;

    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct soup_s *)left_tree->tr_d.td_r->m_p;
    r = (struct soup_s *)right_tree->tr_d.td_r->m_p;
    SOUP_CKMAG(l);
    SOUP_CKMAG(r);
    /* walk the two trees, marking each face as being inside or outside.
     * O(n)? n^2? */
    for(i=0;i<l->nfaces;i++)
	l->faces[i].foo = 0;
    for(i=0;i<r->nfaces;i++)
	r->faces[i].foo = 0;
}

static union tree *
compose(union tree *left_tree, union tree *right_tree, unsigned long int face_status1, unsigned long int face_status2, unsigned long int face_status3)
{
    struct soup_s *l, *r;
    int i;

    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct soup_s *)left_tree->tr_d.td_r->m_p;
    r = (struct soup_s *)right_tree->tr_d.td_r->m_p;

    /* remove unnecessary faces and compose a single new internal */
    for(i=l->nfaces;i<=0;i--)
	if(l->faces[i].foo != face_status1)
	    soup_rm_face(l,i);
    for(i=r->nfaces;i<=0;i--) {
	if(r->faces[i].foo != face_status3)
	    soup_rm_face(l,i);
    }
    face_status1=face_status2=face_status3;

    free_soup(r);
    bu_free(right_tree, "union tree");
    return left_tree;
}

static union tree *
evaluate(union tree *tr, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    RT_CK_TREE(tr);

    switch(tr->tr_op) {
	case OP_NOP:
	    return tr;
	case OP_NMG_TESS:
	    /* ugh, keep it as nmg_tess and just shove the rt_bot_internal ptr
	     * in as nmgregion. :/ Also, only doing the first shell of the first
	     * model. Primitives should only provide a single shell, right? */
	    {
		struct nmgregion *nmgr = BU_LIST_FIRST(nmgregion, &tr->tr_d.td_r->m_p->r_hd);
		/* the bot temporary format may be unnecessary if we can walk
		 * the nmg shells and generate soup from them directly. */
		struct rt_bot_internal *bot = nmg_bot(BU_LIST_FIRST(shell, &nmgr->s_hd), tol);

		/* causes a crash.
		nmg_kr(nmgr);
		free(nmgr);
		*/

		tr->tr_d.td_r->m_p = (struct model *)bot2soup(bot, tol);
		SOUP_CKMAG((struct soup_s *)tr->tr_d.td_r->m_p);

		/* rt_bot_ifree2 doesn't have an RT_EXPORT line, so is unavailable on windows. Just leak instead. */
#ifndef _WIN32
		rt_bot_ifree2(bot);
#endif
	    }
	    return tr;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    tr->tr_b.tb_left = evaluate(tr->tr_b.tb_left, ttol, tol);
	    tr->tr_b.tb_right = evaluate(tr->tr_b.tb_right, ttol, tol);
	    RT_CK_TREE(tr->tr_b.tb_left);
	    RT_CK_TREE(tr->tr_b.tb_right);
	    SOUP_CKMAG(tr->tr_b.tb_left->tr_d.td_r->m_p);
	    SOUP_CKMAG(tr->tr_b.tb_right->tr_d.td_r->m_p);
	    split_faces(tr->tr_b.tb_left, tr->tr_b.tb_right, tol);
	    RT_CK_TREE(tr->tr_b.tb_left);
	    RT_CK_TREE(tr->tr_b.tb_right);
	    SOUP_CKMAG(tr->tr_b.tb_left->tr_d.td_r->m_p);
	    SOUP_CKMAG(tr->tr_b.tb_right->tr_d.td_r->m_p);
	    classify_faces(tr->tr_b.tb_left, tr->tr_b.tb_right);
	    RT_CK_TREE(tr->tr_b.tb_left);
	    RT_CK_TREE(tr->tr_b.tb_right);
	    SOUP_CKMAG(tr->tr_b.tb_left->tr_d.td_r->m_p);
	    SOUP_CKMAG(tr->tr_b.tb_right->tr_d.td_r->m_p);
	    break;
	default:
	    bu_bomb("bottess evaluate(): bad op (first pass)\n");
    }

    switch(tr->tr_op) {
	case OP_UNION:
	    return compose(tr->tr_b.tb_left, tr->tr_b.tb_right, OUTSIDE, SAME, OUTSIDE);
	case OP_INTERSECT:
	    return compose(tr->tr_b.tb_left, tr->tr_b.tb_right, INSIDE, SAME, INSIDE);
	case OP_SUBTRACT:
	    return invert(compose(tr->tr_b.tb_left, invert(tr->tr_b.tb_right), OUTSIDE, OPPOSITE, INSIDE));
	default:
	    bu_bomb("bottess evaluate(): bad op (second pass, CSG)\n");
    }
    bu_bomb("Got somewhere I shouldn't have\n");
    return NULL;
}

union tree *
gcv_bottess_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
    union tree *ret_tree = NULL;

    bu_bomb("No\n");

    if (!tsp || !curtree || !pathp || !client_data) {
	bu_log("INTERNAL ERROR: gcv_bottess_region_end missing parameters\n");
	return TREE_NULL;
    }

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    if (curtree->tr_op == OP_NOP)
	return curtree;

    evaluate(curtree, tsp->ts_ttol, tsp->ts_tol);

    /* convert it to a BoT or something. Since we seperated vertices, we may
     * need to cluster them and recompact it. */
    /*
    rt_bot_tess(curtree->tr_d.td_r
    */

    return ret_tree;
}

union tree *
gcv_bottess(int argc, const char **argv, struct db_i *dbip, struct rt_tess_tol *ttol)
{
    struct db_tree_state tree_state = rt_initial_tree_state;
    tree_state.ts_ttol = ttol;

    db_walk_tree(dbip, argc, argv, 1, &tree_state, NULL, gcv_bottess_region_end, nmg_booltree_leaf_tess, NULL);

    return NULL;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
