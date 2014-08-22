/*                    B O T T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

#include "./soup.h"
#include "./tri_intersect.h"


struct gcv_data {
    void (*func)(struct shell *, const struct db_full_path *, int, int, float [3]);
};


/* assume 4096, seems common enough. a portable way to get to PAGE_SIZE might be
 * better. */
const int faces_per_page = 4096 / sizeof(struct face_s);


int
soup_rm_face(struct soup_s *s, unsigned long int i)
{
    if (i>=s->nfaces) {
	bu_log("trying to remove a nonexistent face? %lu/%lu\n", i, s->nfaces);
	bu_bomb("Asploding\n");
    }
    memcpy(&s->faces[i], &s->faces[s->nfaces-1], sizeof(struct face_s));
    return s->nfaces--;
}


int
soup_add_face_precomputed(struct soup_s *s, point_t a, point_t b , point_t c, plane_t d, uint32_t foo)
{
    struct face_s *f;
    vect_t e1, e2, x;

    VSUB2(e1, b, a);
    VSUB2(e2, c, a);
    VCROSS(x, e1, e2);

    /* grow face array if needed */
    if (s->nfaces >= s->maxfaces)
	s->faces = (struct face_s *)bu_realloc(s->faces, (s->maxfaces += faces_per_page) * sizeof(struct face_s), "bot soup faces");
    f = s->faces + s->nfaces;

    VMOVE(f->vert[0], a);
    if (VDOT(x, d) <= 0) {
	VMOVE(f->vert[1], b);
	VMOVE(f->vert[2], c);
    } else {
	VMOVE(f->vert[1], c);
	VMOVE(f->vert[2], b);
    }

    HMOVE(f->plane, d);

    /* solve the bounding box (should this be VMINMAX?) */
    VMOVE(f->min, f->vert[0]); VMOVE(f->max, f->vert[0]);
    VMIN(f->min, f->vert[1]); VMAX(f->max, f->vert[1]);
    VMIN(f->min, f->vert[2]); VMAX(f->max, f->vert[2]);
    /* fluff the bounding box for fp fuzz */
    f->min[X]-=.1; f->min[Y]-=.1; f->min[Z]-=.1;
    f->max[X]+=.1; f->max[Y]+=.1; f->max[Z]+=.1;

    f->foo = foo;

    s->nfaces++;
    return 0;
}


int
soup_add_face(struct soup_s *s, point_t a, point_t b, point_t c, const struct bn_tol *tol) {
    plane_t p;

    /* solve the plane */
    bn_mk_plane_3pts(p, a, b, c, tol);

    return soup_add_face_precomputed(s, a, b, c, p, OUTSIDE);
}


long int splitz = 0;
long int splitty = 0;

int
split_face_single(struct soup_s *s, unsigned long int fid, point_t isectpt[2], struct face_s *opp_face, const struct bn_tol *tol)
{
    struct face_s *f = s->faces+fid;
    int a, i, j, isv[2] = {0, 0};

#define VERT_INT 0x10
#define LINE_INT 0x20
#define FACE_INT 0x40
#define ALL_INT  (VERT_INT|LINE_INT|FACE_INT)

    /****** START hoistable ******/
    for (i=0;i<2;i++) for (j=0;j<3;j++) {
	if (isv[i] == 0) {
	    fastf_t dist;

	    switch ( bn_isect_pt_lseg( &dist, (fastf_t *)&f->vert[j], (fastf_t *)&f->vert[j==2?0:j+1], (fastf_t *)&isectpt[i], tol) ) {
		case -2: case -1: continue;
		case 1: isv[i] = VERT_INT|j; break;
		case 2: isv[i] = VERT_INT|(j==2?0:j+1); break;
		case 3: isv[i] = LINE_INT|j; break;
		default: bu_log("Whu?\n"); break;
	    }
	}
    }

    /*** test if intersect is middle of face ***/
    for (i=0;i<2;i++)
	/* test for face in plane */
	if (isv[i] == 0)	/* assume that the isectpt is necessarily on the vert, line or
			   face... if it's not seen on the vert or line, it must be face.
			   This should probably be a real check. */
	    isv[i] = FACE_INT;

    if (isv[0] == 0 || isv[1] == 0) {
	bu_log("Something real bad %x %x\n", isv[0], isv[1]);
	return -1;
    }

    if ((isv[0]&ALL_INT) > (isv[1]&ALL_INT)) {
	int tmpi;
	point_t tmpp;
	VMOVE(tmpp, isectpt[0]);
	VMOVE(isectpt[0], isectpt[1]);
	VMOVE(isectpt[1], tmpp);
	tmpi = isv[0];
	isv[0]=isv[1];
	isv[1]=tmpi;
    }

    /****** END hoistable ******/

    /* test if both ends of the intersect line are on vertices */
    /* if VERT+VERT, abort */
    if ((isv[0]&VERT_INT) && (isv[1]&VERT_INT)) {
	return 1;
    }

    a = isv[0]&~ALL_INT;
    if ( a != 0 && a != 1 && a != 2) {
	bu_log("Bad a value: %d\n", a);
	bu_bomb("Exploding\n");
    }
    /* if VERT+LINE, break into 2 */
    if (isv[0]&VERT_INT && isv[1]&LINE_INT) {
	vect_t muh;
	int meh;

	VSUB2(muh, isectpt[1], f->vert[a==2?0:a+1]);
	meh = VDOT(opp_face->plane, muh) > 0;
	soup_add_face_precomputed(s, f->vert[a], isectpt[1], f->vert[a==2?0:a+1], f->plane, meh == 1 ? OUTSIDE : INSIDE);
	soup_add_face_precomputed(s, f->vert[a], f->vert[a==0?2:a-1], isectpt[1], f->plane, meh == 1 ? INSIDE : OUTSIDE);

	soup_rm_face(s, fid);
	return 2;
    }

    /* if LINE+LINE, break into 3, figure out which side has two verts and cut * that */
    if (isv[0]&LINE_INT && isv[1]&LINE_INT) {
	return 1;
    }

    /* if VERT+FACE, break into 3, intersect is one line, other two to the * opposing verts */
    if (isv[0]&VERT_INT ) {
	soup_add_face_precomputed(s, f->vert[0], f->vert[1], isectpt[1], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[1], f->vert[2], isectpt[1], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[2], f->vert[0], isectpt[1], f->plane, 0);
	soup_rm_face(s, fid);
	return 3;
    }

    /* if LINE+FACE, break into 4 */
    if (isv[0]&LINE_INT ) {
	soup_add_face_precomputed(s, f->vert[a], isectpt[0], isectpt[1], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[a==2?0:a+1], isectpt[1], isectpt[0], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[a==2?0:a+1], f->vert[a==0?2:a-1], isectpt[1], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[a], isectpt[1], f->vert[a==0?2:a-1], f->plane, 0);
	soup_rm_face(s, fid);
	return 4;
    }

    /* if FACE+FACE, break into 3 */
    if (isv[0]&FACE_INT ) {
	/* extend intersect line to triangle edges, could be 2 or 3? */

	/* make sure isectpt[0] is closest to vert[0] */
	if (DIST_PT_PT_SQ(f->vert[0], isectpt[0]) > DIST_PT_PT_SQ(f->vert[0], isectpt[1])) {
	    point_t tmp;
	    VMOVE(tmp, isectpt[1]);
	    VMOVE(isectpt[1], isectpt[0]);
	    VMOVE(isectpt[0], tmp);
	}

	soup_add_face_precomputed(s, f->vert[0], isectpt[0], f->vert[2], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[0], f->vert[1], isectpt[0], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[1], isectpt[1], isectpt[0], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[2], isectpt[0], isectpt[1], f->plane, 0);
	soup_add_face_precomputed(s, f->vert[1], f->vert[2], isectpt[1], f->plane, 0);
	return 5;
    }
#undef VERT_INT
#undef LINE_INT
#undef ALL_INT
#undef FACE_INT
    /* this should never be reached */
    bu_log("derp?\n");

    return 0;
}


/* returns 0 to continue, 1 if the left face was split, 2 if the right face was
 * split */
int
split_face(struct soup_s *left, unsigned long int left_face, struct soup_s *right, unsigned long int right_face, const struct bn_tol *tol) {
    struct face_s *lf, *rf;
    vect_t isectpt[2] = {{0, 0, 0}, {0, 0, 0}};
    int coplanar, r = 0;

    lf = left->faces+left_face;
    rf = right->faces+right_face;

    splitz++;
    if (gcv_tri_tri_intersect_with_isectline(left, right, lf, rf, &coplanar, (point_t *)isectpt, tol) != 0 && !VNEAR_EQUAL(isectpt[0], isectpt[1], tol->dist)) {
	splitty++;

	if (split_face_single(left, left_face, isectpt, &right->faces[right_face], tol) > 1) r|=0x1;
	if (split_face_single(right, right_face, isectpt, &left->faces[left_face], tol) > 1) r|=0x2;
    }

    return r;
}


struct soup_s *
bot2soup(struct rt_bot_internal *bot, const struct bn_tol *tol)
{
    struct soup_s *s;
    unsigned long int i;

    RT_BOT_CK_MAGIC(bot);

    if (bot->orientation != RT_BOT_CCW)
	bu_bomb("Bad orientation out of nmg_bot\n");

    BU_ALLOC(s, struct soup_s);
    s->magic = SOUP_MAGIC;
    s->nfaces = 0;
    s->maxfaces = ceil(bot->num_faces / (double)faces_per_page) * faces_per_page;
    s->faces = (struct face_s *)bu_malloc(sizeof(struct face_s) * s->maxfaces, "bot soup faces");

    for (i=0;i<bot->num_faces;i++)
	soup_add_face(s, bot->vertices+3*bot->faces[i*3+0], bot->vertices+3*bot->faces[i*3+1], bot->vertices+3*bot->faces[i*3+2], tol);

    return s;
}


void
free_soup(struct soup_s *s) {
    if (s == NULL)
	bu_bomb("null soup");
    if (s->faces)
	bu_free(s->faces, "bot soup faces");
    bu_free(s, "bot soup");
    return;
}


union tree *
invert(union tree *tree)
{
    struct soup_s *s;
    unsigned long int i;

    RT_CK_TREE(tree);
    if (tree->tr_op != OP_NMG_TESS) {
	bu_log("Erm, this isn't an nmg tess\n");
	return tree;
    }
    s = (struct soup_s *)tree->tr_d.td_s;
    SOUP_CKMAG(s);

    for (i=0;i<s->nfaces;i++) {
	struct face_s *f = s->faces+i;
	point_t t;
	VMOVE(t, f->vert[0]);
	VMOVE(f->vert[0], f->vert[1]);
	VMOVE(f->vert[0], t);
	/* flip the inverted bit. */
	f->foo^=INVERTED;
    }

    return tree;
}


void
split_faces(union tree *left_tree, union tree *right_tree, const struct bn_tol *tol)
{
    struct soup_s *l, *r;
    unsigned long int i, j;

    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct soup_s *)left_tree->tr_d.td_s;
    r = (struct soup_s *)right_tree->tr_d.td_s;
    SOUP_CKMAG(l);
    SOUP_CKMAG(r);

    /* this is going to be big and hairy. Has to walk both meshes finding
     * all intersections and split intersecting faces so there are edges at
     * the intersections. Initially going to be O(n^2), then change to use
     * space partitioning (binning? octree? kd?). */
    for (i=0;i<l->nfaces;i++) {
	struct face_s *lf = l->faces+i, *rf = NULL;
	int ret;

	for (j=0;j<r->nfaces;j++) {
	    rf = r->faces+j;
	    /* quick bounding box test */
	    if (lf->min[X]>rf->max[X] || lf->max[X]>lf->max[X] ||
	       lf->min[Y]>rf->max[Y] || lf->max[Y]>lf->max[Y] ||
	       lf->min[Z]>rf->max[Z] || lf->max[Z]>lf->max[Z])
		continue;
	    /* two possibly overlapping faces found */
	    ret = split_face(l, i, r, j, tol);
	    if (ret&0x1) i--;
	    if (ret&0x2) j--;
	}
    }
}


union tree *
compose(union tree *left_tree, union tree *right_tree, unsigned long int UNUSED(face_status1), unsigned long int UNUSED(face_status2), unsigned long int UNUSED(face_status3))
{
    struct soup_s *l, *r;
    int i;

    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct soup_s *)left_tree->tr_d.td_s;
    r = (struct soup_s *)right_tree->tr_d.td_s;
    printf("Composing! %lu %lu\n", l->nfaces, r->nfaces);

    bu_log("solid /left\n");
    for (i=0; i < (int)l->nfaces; i++) {
	bu_log("  facet normal %f %f %f\n", V3ARGS(l->faces[i].plane));
	bu_log("    outer loop\n");
	bu_log("      vertex %f %f %f\n", V3ARGS(l->faces[i].vert[0]));
	bu_log("      vertex %f %f %f\n", V3ARGS(l->faces[i].vert[1]));
	bu_log("      vertex %f %f %f\n", V3ARGS(l->faces[i].vert[2]));
	bu_log("    endloop\n");
	bu_log("  endfacet\n");
    }
    bu_log("endsolid /left\n");

    bu_log("solid /right\n");
    for (i=0; i < (int)r->nfaces; i++) {
	bu_log("  facet normal %f %f %f\n", V3ARGS(r->faces[i].plane));
	bu_log("    outer loop\n");
	bu_log("      vertex %f %f %f\n", V3ARGS(r->faces[i].vert[0]));
	bu_log("      vertex %f %f %f\n", V3ARGS(r->faces[i].vert[1]));
	bu_log("      vertex %f %f %f\n", V3ARGS(r->faces[i].vert[2]));
	bu_log("    endloop\n");
	bu_log("  endfacet\n");
    }
    bu_log("endsolid /right\n");
    return left_tree;

}


union tree *
evaluate(union tree *tr, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    RT_CK_TREE(tr);

    switch (tr->tr_op) {
	case OP_NOP:
	    return tr;
	case OP_NMG_TESS:
	    /* ugh, keep it as nmg_tess and just shove the rt_bot_internal ptr
	     * in as nmgregion. :/ Also, only doing the first shell of the first
	     * model. Primitives should only provide a single shell, right? */
	    {
		struct rt_db_internal ip;
		struct shell *nmgs = tr->tr_d.td_s;
		/* the bot temporary format may be unnecessary if we can walk
		 * the nmg shells and generate soup from them directly. */
		struct rt_bot_internal *bot = rt_nmg_bot(nmgs, tol);

		/* causes a crash.
		   nmg_kr(nmgr);
		   free(nmgr);
		*/

		tr->tr_d.td_s = (struct shell *)bot2soup(bot, tol);
		SOUP_CKMAG((struct soup_s *)tr->tr_d.td_s);

		/* fill in a db_internal with our new bot so we can free it */
		RT_DB_INTERNAL_INIT(&ip);
		ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		ip.idb_minor_type = ID_BOT;
		ip.idb_meth = &OBJ[ID_BOT];
		ip.idb_ptr = bot;
		ip.idb_meth->ft_ifree(&ip);
	    }
	    return tr;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    RT_CK_TREE(tr->tr_b.tb_left);
	    RT_CK_TREE(tr->tr_b.tb_right);
	    tr->tr_b.tb_left = evaluate(tr->tr_b.tb_left, ttol, tol);
	    tr->tr_b.tb_right = evaluate(tr->tr_b.tb_right, ttol, tol);
	    RT_CK_TREE(tr->tr_b.tb_left);
	    RT_CK_TREE(tr->tr_b.tb_right);
	    SOUP_CKMAG(tr->tr_b.tb_left->tr_d.td_s);
	    SOUP_CKMAG(tr->tr_b.tb_right->tr_d.td_s);
	    split_faces(tr->tr_b.tb_left, tr->tr_b.tb_right, tol);
	    RT_CK_TREE(tr->tr_b.tb_left);
	    RT_CK_TREE(tr->tr_b.tb_right);
	    SOUP_CKMAG(tr->tr_b.tb_left->tr_d.td_s);
	    SOUP_CKMAG(tr->tr_b.tb_right->tr_d.td_s);
	    break;
	default:
	    bu_bomb("bottess evaluate(): bad op (first pass)\n");
    }

    switch (tr->tr_op) {
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


long int lsplitz=0;
long int lsplitty=0;

union tree *
gcv_bottess_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    union tree *ret_tree = TREE_NULL;
    void (*write_region)(struct shell *, const struct db_full_path *, int, int, float [3]);

    if (!tsp || !curtree || !pathp || !client_data) {
	bu_log("INTERNAL ERROR: gcv_region_end missing parameters\n");
	return TREE_NULL;
    }

    write_region = ((struct gcv_data *)client_data)->func;
    if (!write_region) {
	bu_log("INTERNAL ERROR: gcv_region_end missing conversion callback function\n");
	return TREE_NULL;
    }

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_SHELL(*tsp->ts_s);

    splitz=0;
    splitty=0;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_SHELL(*tsp->ts_s);

    if (curtree->tr_op == OP_NOP)
	return curtree;

    ret_tree = evaluate(curtree, tsp->ts_ttol, tsp->ts_tol);

    lsplitz+=splitz;
    lsplitz+=splitz;
    lsplitty+=splitty;
    splitz=0;
    splitty=0;

    return ret_tree;
}


union tree *
gcv_bottess(int argc, const char **argv, struct db_i *dbip, struct rt_tess_tol *ttol)
{
    struct db_tree_state tree_state = rt_initial_tree_state;
    tree_state.ts_ttol = ttol;

    if (db_walk_tree(dbip, argc, argv, 1, &tree_state, NULL, gcv_bottess_region_end, rt_nmg_booltree_leaf_tess, NULL) < 0)
	bu_log("gcv_bottess: db_walk_tree failure\n");

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
