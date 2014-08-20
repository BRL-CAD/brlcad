/*                      N M G _ F U S E . C
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
/** @file primitives/nmg/nmg_fuse.c
 *
 * Routines to "fuse" entities together that are geometrically
 * identical (within tolerance) into entities that share underlying
 * geometry structures, so that the relationship is explicit.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/sort.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"


extern int debug_file_count;

struct pt_list
{
    struct bu_list l;
    point_t xyz;
    fastf_t t;
};


extern void nmg_split_trim(const struct edge_g_cnurb *cnrb,
			   const struct face_g_snurb *snrb,
			   fastf_t t,
			   struct pt_list *pt0, struct pt_list *pt1,
			   const struct bn_tol *tol);

/**
 * Do two faces share by topology at least one loop of 3 or more vertices?
 *
 * Require that at least three distinct edge geometries be involved.
 *
 * XXX Won't catch sharing of faces with only self-loops and no edge loops.
 */
int
nmg_is_common_bigloop(const struct face *f1, const struct face *f2)
{
    const struct faceuse *fu1;
    const struct loopuse *lu1;
    const struct edgeuse *eu1;
    const uint32_t *magic1 = NULL;
    const uint32_t *magic2 = NULL;
    int nverts;
    int nbadv;
    int got_three;

    NMG_CK_FACE(f1);
    NMG_CK_FACE(f2);

    fu1 = f1->fu_p;
    NMG_CK_FACEUSE(fu1);

    /* For all loopuses in fu1 */
    for (BU_LIST_FOR(lu1, loopuse, &fu1->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC)
	    continue;
	nverts = 0;
	nbadv = 0;
	magic1 = NULL;
	magic2 = NULL;
	got_three = 0;
	for (BU_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {
	    nverts++;
	    NMG_CK_EDGE_G_LSEG(eu1->g.lseg_p);
	    if (!magic1) {
		magic1 = eu1->g.magic_p;
	    } else if (!magic2) {
		if (eu1->g.magic_p != magic1) {
		    magic2 = eu1->g.magic_p;
		}
	    } else {
		if (eu1->g.magic_p != magic1 &&
		    eu1->g.magic_p != magic2) {
		    got_three = 1;
		}
	    }
	    if (nmg_is_vertex_in_face(eu1->vu_p->v_p, f2))
		continue;
	    nbadv++;
	    break;
	}
	if (nbadv <= 0 && nverts >= 3 && got_three) return 1;
    }
    return 0;
}


/**
 * Ensure that all the vertices in r1 are still geometrically unique.
 * This will be true after nmg_region_both_vfuse() has been called,
 * and should remain true throughout the intersection process.
 */
void
nmg_shell_v_unique(struct shell *s1, const struct bn_tol *tol)
{
    int i;
    int j;
    struct bu_ptbl t;

    NMG_CK_SHELL(s1);
    BN_CK_TOL(tol);

    nmg_vertex_tabulate(&t, &s1->magic);

    for (i = BU_PTBL_END(&t)-1; i >= 0; i--) {
	register struct vertex *vi;
	vi = (struct vertex *)BU_PTBL_GET(&t, i);
	NMG_CK_VERTEX(vi);
	if (!vi->vg_p) continue;

	for (j = i-1; j >= 0; j--) {
	    register struct vertex *vj;
	    vj = (struct vertex *)BU_PTBL_GET(&t, j);
	    NMG_CK_VERTEX(vj);
	    if (!vj->vg_p) continue;
	    if (!bn_pt3_pt3_equal(vi->vg_p->coord, vj->vg_p->coord, tol))
		continue;
	    /* They are the same */
	    bu_log("nmg_region_v_unique():  2 verts are the same, within tolerance\n");
	    nmg_pr_v(vi, 0);
	    nmg_pr_v(vj, 0);
	    bu_bomb("nmg_region_v_unique()\n");
	}
    }
    bu_ptbl_free(&t);
}


/* compare function for bu_sort within function nmg_ptbl_vfuse */
static int
x_comp(const void *p1, const void *p2, void *UNUSED(arg))
{
    fastf_t i, j;

    i = (*((struct vertex **)p1))->vg_p->coord[X];
    j = (*((struct vertex **)p2))->vg_p->coord[X];

    if (EQUAL(i, j))
	return 0;
    else if (i > j)
	return 1;
    return -1;
}


/**
 * Working from the end to the front, scan for geometric duplications
 * within a single list of vertex structures.
 *
 * Exists primarily as a support routine for nmg_vertex_fuse().
 */
int
nmg_ptbl_vfuse(struct bu_ptbl *t, const struct bn_tol *tol)
{
    int count, fuse;
    register int i, j;
    register fastf_t tmp = tol->dist_sq;
    register fastf_t ab, abx, aby, abz;

    /* sort the vertices in the 't' list by the 'x' coordinate */
    bu_sort(BU_PTBL_BASEADDR(t), BU_PTBL_LEN(t), sizeof(long *), x_comp, NULL);

    count = 0;
    for (i = 0 ; i < BU_PTBL_END(t) ; i++) {
	register struct vertex *vi;
	vi = (struct vertex *)BU_PTBL_GET(t, i);
	if (!vi) continue;
	NMG_CK_VERTEX(vi);
	if (!vi->vg_p) continue;

	for (j = i+1 ; j < BU_PTBL_END(t) ; j++) {
	    register struct vertex *vj;
	    vj = (struct vertex *)BU_PTBL_GET(t, j);
	    if (!vj) continue;
	    NMG_CK_VERTEX(vj);
	    if (!vj->vg_p) continue;

	    if (vi->vg_p == vj->vg_p) {
		/* They are the same, fuse vj into vi */
		nmg_jv(vi, vj);  /* vj gets destroyed */
		BU_PTBL_SET(t, j, NULL);
		count++;
		continue;
	    }

	    fuse = 1;
	    abx = vi->vg_p->coord[X] - vj->vg_p->coord[X];
	    ab = abx * abx;
	    if (ab > tmp) {
		break;  /* no more to test */
	    }

	    aby = vi->vg_p->coord[Y] - vj->vg_p->coord[Y];
	    ab += (aby * aby);
	    if (ab > tmp) {
		fuse = 0;
	    } else {
		abz = vi->vg_p->coord[Z] - vj->vg_p->coord[Z];
		ab += (abz * abz);
		if (ab > tmp) {
		    fuse = 0;
		}
	    }

	    if (fuse) {
		/* They are the same, fuse vj into vi */
		nmg_jv(vi, vj);  /* vj gets destroyed */
		BU_PTBL_SET(t, j, NULL);
		count++;
	    }
	}
    }

    return count;
}


/**
 * For every element in t1, scan t2 for geometric duplications.
 *
 * Deleted elements in t2 are marked by a null vertex pointer,
 * rather than bothering to do a BU_PTBL_RM, which will re-copy the
 * list to compress it.
 *
 * Exists as a support routine for nmg_two_region_vertex_fuse()
 */
int
nmg_region_both_vfuse(struct bu_ptbl *t1, struct bu_ptbl *t2, const struct bn_tol *tol)
{
    int count = 0;
    int i;
    int j;

    /* Verify t2 is good to start with */
    for (j = BU_PTBL_END(t2)-1; j >= 0; j--) {
	register struct vertex *vj;
	vj = (struct vertex *)BU_PTBL_GET(t2, j);
	NMG_CK_VERTEX(vj);
    }

    for (i = BU_PTBL_END(t1)-1; i >= 0; i--) {
	register struct vertex *vi;
	vi = (struct vertex *)BU_PTBL_GET(t1, i);
	NMG_CK_VERTEX(vi);
	if (!vi->vg_p) continue;

	for (j = BU_PTBL_END(t2)-1; j >= 0; j--) {
	    register struct vertex *vj;
	    vj = (struct vertex *)BU_PTBL_GET(t2, j);
	    if (!vj) continue;
	    NMG_CK_VERTEX(vj);
	    if (!vj->vg_p) continue;
	    if (!bn_pt3_pt3_equal(vi->vg_p->coord, vj->vg_p->coord, tol))
		continue;
	    /* They are the same, fuse vj into vi */
	    nmg_jv(vi, vj);
	    BU_PTBL_GET(t2, j) = 0;
	    count++;
	}
    }
    return count;
}


/**
 * Fuse together any vertices that are geometrically identical, within
 * distance tolerance. This function may be passed a pointer to an NMG
 * object or a pointer to a bu_ptbl structure containing a list of
 * pointers to NMG vertex structures. If a bu_ptbl structure was passed
 * into this function, the calling function must free this structure.
 */
int
nmg_vertex_fuse(const uint32_t *magic_p, const struct bn_tol *tol)
{
    struct bu_ptbl *t1;
    struct bu_ptbl tmp;
    size_t t1_len;
    int total = 0;
    const uint32_t *tmp_magic_p;

    BN_CK_TOL(tol);

    if (!magic_p) {
	bu_bomb("nmg_vertex_fuse(): passed null pointer");
    }

    if (*magic_p == BU_PTBL_MAGIC) {
	t1 = (struct bu_ptbl *)magic_p;
	t1_len = BU_PTBL_LEN(t1);
	if (t1_len) {
	    tmp_magic_p = (const uint32_t *)BU_PTBL_GET((struct bu_ptbl *)magic_p, 0);
	    if (*tmp_magic_p != NMG_VERTEX_MAGIC) {
		bu_bomb("nmg_vertex_fuse(): passed bu_ptbl structure not containing vertex");
	    }
	}
    } else {
	t1 = &tmp;
	nmg_vertex_tabulate(t1, magic_p);
	t1_len = BU_PTBL_LEN(t1);
    }

    /* if there are no vertex, do nothing */
    if (!t1_len) {
	return 0;
    }

    total = nmg_ptbl_vfuse(t1, tol);

    /* if bu_ptbl was passed into this function don't free it here */
    if (*magic_p != BU_PTBL_MAGIC) {
	bu_ptbl_free(t1);
    }

    if (nmg_debug & DEBUG_BASIC && total > 0)
	bu_log("nmg_vertex_fuse() %d\n", total);

    return total;
}


/**
 * Checks if cnurb is linear
 *
 * Returns:
 * 1 - cnurb is linear
 * 0 - either cnurb is not linear, or it's not obvious
 */

int
nmg_cnurb_is_linear(const struct edge_g_cnurb *cnrb)
{
    int i;
    int coords;
    int last_index;
    int linear=0;

    NMG_CK_EDGE_G_CNURB(cnrb);

    if (nmg_debug & DEBUG_MESH) {
	bu_log("nmg_cnurb_is_linear(%p)\n", (void *)cnrb);
	nurb_c_print(cnrb);
    }

    if (cnrb->order <= 0) {
	linear = 1;
	goto out;
    }

    if (cnrb->order == 2) {
	if (cnrb->c_size == 2) {
	    linear = 1;
	    goto out;
	}
    }

    coords = RT_NURB_EXTRACT_COORDS(cnrb->pt_type);
    last_index = (cnrb->c_size - 1)*coords;

    /* Check if all control points are either the start point or end point */
    for (i=1; i<cnrb->c_size-2; i++) {
	if (VEQUAL(&cnrb->ctl_points[0], &cnrb->ctl_points[i]))
	    continue;
	if (VEQUAL(&cnrb->ctl_points[last_index], &cnrb->ctl_points[i]))
	    continue;

	goto out;
    }

    linear = 1;

out:
    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_cnurb_is_linear(%p) returning %d\n", (void *)cnrb, linear);

    return linear;
}


/**
 * Checks if snurb surface is planar
 *
 * Returns:
 * 0 - surface is not planar
 * 1 - surface is planar (within tolerance)
 */

int
nmg_snurb_is_planar(const struct face_g_snurb *srf, const struct bn_tol *tol)
{
    plane_t pl = {(fastf_t)0.0};
    int i;
    int coords;
    mat_t matrix;
    mat_t inverse;
    vect_t vsum;
    double det;
    double one_over_vertex_count;
    int planar=0;

    NMG_CK_FACE_G_SNURB(srf);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_MESH) {
	bu_log("nmg_snurb_is_planar(%p)\n", (void *)srf);
	nurb_s_print("", srf);
    }

    if (srf->order[0] == 2 && srf->order[1] == 2) {
	if (srf->s_size[0] == 2 && srf->s_size[1] == 2) {
	    planar = 1;
	    goto out;
	}
    }

    /* build matrix */
    MAT_ZERO(matrix);
    VSET(vsum, 0.0, 0.0, 0.0);

    one_over_vertex_count = 1.0/(double)(srf->s_size[0]*srf->s_size[1]);
    coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

    /* calculate an average plane for all control points */
    for (i=0; i<srf->s_size[0]*srf->s_size[1]; i++) {
	fastf_t *pt;

	pt = &srf->ctl_points[ i*coords ];

	matrix[0] += pt[X] * pt[X];
	matrix[1] += pt[X] * pt[Y];
	matrix[2] += pt[X] * pt[Z];
	matrix[5] += pt[Y] * pt[Y];
	matrix[6] += pt[Y] * pt[Z];
	matrix[10] += pt[Z] * pt[Z];

	vsum[X] += pt[X];
	vsum[Y] += pt[Y];
	vsum[Z] += pt[Z];
    }
    matrix[4] = matrix[1];
    matrix[8] = matrix[2];
    matrix[9] = matrix[6];
    matrix[15] = 1.0;

    /* Check that we don't have a singular matrix */
    det = bn_mat_determinant(matrix);

    if (!ZERO(det)) {
	fastf_t inv_len_pl;

	/* invert matrix */
	bn_mat_inv(inverse, matrix);

	/* get normal vector */
	MAT4X3PNT(pl, inverse, vsum);

	/* unitize direction vector */
	inv_len_pl = 1.0/(MAGNITUDE(pl));
	HSCALE(pl, pl, inv_len_pl);

	/* get average vertex coordinates */
	VSCALE(vsum, vsum, one_over_vertex_count);

	/* get distance from plane to origin */
	pl[H] = VDOT(pl, vsum);

    } else {
	int x_same=1;
	int y_same=1;
	int z_same=1;

	/* singular matrix, may occur if all vertices have the same zero
	 * component.
	 */
	for (i=1; i<srf->s_size[0]*srf->s_size[1]; i++) {
	    if (!ZERO(srf->ctl_points[i*coords+X] - srf->ctl_points[X]))
		x_same = 0;
	    if (!ZERO(srf->ctl_points[i*coords+Y] - srf->ctl_points[Y]))
		y_same = 0;
	    if (!ZERO(srf->ctl_points[i*coords+Z] - srf->ctl_points[Z]))
		z_same = 0;

	    if (!x_same && !y_same && !z_same)
		break;
	}

	if (x_same) {
	    VSET(pl, 1.0, 0.0, 0.0);
	} else if (y_same) {
	    VSET(pl, 0.0, 1.0, 0.0);
	} else if (z_same) {
	    VSET(pl, 0.0, 0.0, 1.0);
	}

	if (x_same || y_same || z_same) {
	    /* get average vertex coordinates */
	    VSCALE(vsum, vsum, one_over_vertex_count);

	    /* get distance from plane to origin */
	    pl[H] = VDOT(pl, vsum);

	} else {
	    bu_log("nmg_snurb_is_plana: Cannot calculate plane for snurb %p\n", (void *)srf);
	    nurb_s_print("", srf);
	    bu_bomb("nmg_snurb_is_plana: Cannot calculate plane for snurb\n");
	}
    }

    /* Now verify that every control point is on this plane */
    for (i=0; i<srf->s_size[0]*srf->s_size[1]; i++) {
	fastf_t *pt;
	fastf_t dist;

	pt = &srf->ctl_points[ i*coords ];

	dist = DIST_PT_PLANE(pt, pl);
	if (dist > tol->dist)
	    goto out;
    }

    planar = 1;
out:
    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_snurb_is_planar(%p) returning %d\n", (void *)srf, planar);

    return planar;

}


void
nmg_eval_linear_trim_curve(const struct face_g_snurb *snrb, const fastf_t *uvw, fastf_t *xyz)
{
    int coords;
    hpoint_t xyz1;

    if (snrb) {
	NMG_CK_FACE_G_SNURB(snrb);
	nurb_s_eval(snrb, uvw[0], uvw[1], xyz1);
	if (RT_NURB_IS_PT_RATIONAL(snrb->pt_type)) {
	    fastf_t inverse_weight;

	    coords = RT_NURB_EXTRACT_COORDS(snrb->pt_type);
	    inverse_weight = 1.0/xyz1[coords-1];

	    VSCALE(xyz, xyz1, inverse_weight);
	} else {
	    VMOVE(xyz, xyz1);
	}
    } else {
	VMOVE(xyz, uvw);
    }

}


void
nmg_eval_trim_curve(const struct edge_g_cnurb *cnrb, const struct face_g_snurb *snrb, const fastf_t t, fastf_t *xyz)
{
    hpoint_t uvw;
    hpoint_t xyz1;
    int coords;

    NMG_CK_EDGE_G_CNURB(cnrb);
    if (snrb) {
	NMG_CK_FACE_G_SNURB(snrb);
    }

    nurb_c_eval(cnrb, t, uvw);

    if (RT_NURB_IS_PT_RATIONAL(cnrb->pt_type)) {
	fastf_t inverse_weight;

	coords = RT_NURB_EXTRACT_COORDS(cnrb->pt_type);
	inverse_weight = 1.0/uvw[coords-1];

	VSCALE(uvw, uvw, inverse_weight);
    }

    if (snrb) {
	nurb_s_eval(snrb, uvw[0], uvw[1], xyz1);
	if (RT_NURB_IS_PT_RATIONAL(snrb->pt_type)) {
	    fastf_t inverse_weight;

	    coords = RT_NURB_EXTRACT_COORDS(snrb->pt_type);
	    inverse_weight = 1.0/xyz1[coords-1];

	    VSCALE(xyz, xyz1, inverse_weight);
	} else {
	    VMOVE(xyz, xyz1);
	}
    } else {
	VMOVE(xyz, uvw);
    }

}


void
nmg_split_trim(const struct edge_g_cnurb *cnrb, const struct face_g_snurb *snrb, fastf_t t, struct pt_list *pt0, struct pt_list *pt1, const struct bn_tol *tol)
{
    struct pt_list *pt_new;
    fastf_t t_sub;
    vect_t seg;

    NMG_CK_EDGE_G_CNURB(cnrb);
    NMG_CK_FACE_G_SNURB(snrb);
    BN_CK_TOL(tol);

    BU_ALLOC(pt_new, struct pt_list);
    pt_new->t = t;

    if (pt_new->t < pt0->t || pt_new->t > pt1->t) {
	bu_log("nmg_split_trim: split parameter (%g) is not between ends (%g and %g)\n",
	       t, pt0->t, pt1->t);
	bu_bomb("nmg_split_trim: split parameters not between ends\n");
    }

    nmg_eval_trim_curve(cnrb, snrb, pt_new->t, pt_new->xyz);

    BU_LIST_INSERT(&pt1->l, &pt_new->l);

    VSUB2(seg, pt0->xyz, pt_new->xyz);
    if (MAGSQ(seg) > tol->dist_sq) {
	t_sub = (pt0->t + pt_new->t)/2.0;
	nmg_split_trim(cnrb, snrb, t_sub, pt0, pt_new, tol);
    }

    VSUB2(seg, pt_new->xyz, pt1->xyz);
    if (MAGSQ(seg) > tol->dist_sq) {
	t_sub = (pt_new->t + pt1->t)/2.0;
	nmg_split_trim(cnrb, snrb, t_sub, pt_new, pt1, tol);
    }
}


void
nmg_eval_trim_to_tol(const struct edge_g_cnurb *cnrb, const struct face_g_snurb *snrb, const fastf_t t0, const fastf_t t1, struct bu_list *head, const struct bn_tol *tol)
{
    fastf_t t;
    struct pt_list *pt0, *pt1;

    NMG_CK_EDGE_G_CNURB(cnrb);
    NMG_CK_FACE_G_SNURB(snrb);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_eval_trim_to_tol(cnrb=%p, snrb=%p, t0=%g, t1=%g) START\n",
	       (void *)cnrb, (void *)snrb, t0, t1);

    BU_ALLOC(pt0, struct pt_list);
    pt0->t = t0;
    nmg_eval_trim_curve(cnrb, snrb, pt0->t, pt0->xyz);
    BU_LIST_INSERT(head, &pt0->l);

    BU_ALLOC(pt1, struct pt_list);
    pt1->t = t1;
    nmg_eval_trim_curve(cnrb, snrb, pt1->t, pt1->xyz);
    BU_LIST_INSERT(head, &pt1->l);

    t = (t0 + t1)/2.0;
    nmg_split_trim(cnrb, snrb, t, pt0, pt1, tol);

    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_eval_trim_to_tol(cnrb=%p, snrb=%p, t0=%g, t1=%g) END\n",
	       (void *)cnrb, (void *)snrb, t0, t1);
}


void
nmg_split_linear_trim(const struct face_g_snurb *snrb, const fastf_t *uvw1, const fastf_t *uvw2, struct pt_list *pt0, struct pt_list *pt1, const struct bn_tol *tol)
{
    struct pt_list *pt_new;
    fastf_t t_sub;
    fastf_t uvw_sub[3];
    vect_t seg;

    if (snrb)
	NMG_CK_FACE_G_SNURB(snrb);
    BN_CK_TOL(tol);
    BU_ALLOC(pt_new, struct pt_list);
    pt_new->t = 0.5*(pt0->t + pt1->t);

    VBLEND2(uvw_sub, 1.0 - pt_new->t, uvw1, pt_new->t, uvw2);
    nmg_eval_linear_trim_curve(snrb, uvw_sub, pt_new->xyz);

    BU_LIST_INSERT(&pt1->l, &pt_new->l);

    VSUB2(seg, pt0->xyz, pt_new->xyz);
    if (MAGSQ(seg) > tol->dist_sq) {
	t_sub = (pt0->t + pt_new->t)/2.0;
	VBLEND2(uvw_sub, 1.0 - t_sub, uvw1, t_sub, uvw2);
	nmg_split_linear_trim(snrb, uvw1, uvw2, pt0, pt_new, tol);
    }

    VSUB2(seg, pt_new->xyz, pt1->xyz);
    if (MAGSQ(seg) > tol->dist_sq) {
	t_sub = (pt_new->t + pt1->t)/2.0;
	VBLEND2(uvw_sub, 1.0 - t_sub, uvw1, t_sub, uvw2);
	nmg_split_linear_trim(snrb, uvw1, uvw2, pt0, pt_new, tol);
    }
}


void
nmg_eval_linear_trim_to_tol(const struct edge_g_cnurb *cnrb, const struct face_g_snurb *snrb, const fastf_t *uvw1, const fastf_t *uvw2, struct bu_list *head, const struct bn_tol *tol)
{
    struct pt_list *pt0, *pt1;

    NMG_CK_EDGE_G_CNURB(cnrb);
    NMG_CK_FACE_G_SNURB(snrb);
    BN_CK_TOL(tol);

    NMG_CK_EDGE_G_CNURB(cnrb);
    NMG_CK_FACE_G_SNURB(snrb);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_eval_linear_trim_to_tol(cnrb=%p, snrb=%p, uvw1=(%g %g %g), uvw2=(%g %g %g)) START\n",
	       (void *)cnrb, (void *)snrb, V3ARGS(uvw1), V3ARGS(uvw2));

    BU_ALLOC(pt0, struct pt_list);
    pt0->t = 0.0;
    nmg_eval_linear_trim_curve(snrb, uvw1, pt0->xyz);
    BU_LIST_INSERT(head, &pt0->l);

    BU_ALLOC(pt1, struct pt_list);
    pt1->t = 1.0;
    nmg_eval_linear_trim_curve(snrb, uvw2, pt1->xyz);
    BU_LIST_INSERT(head, &pt1->l);

    nmg_split_linear_trim(snrb, uvw1, uvw2, pt0, pt1, tol);

    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_eval_linear_trim_to_tol(cnrb=%p, snrb=%p) END\n",
	       (void *)cnrb, (void *)snrb);
}


/* check for coincidence at twenty interior points along a cnurb */
#define CHECK_NUMBER 20

/**
 * Checks if CNURB is coincident with line segment from pt1 to pt2
 * by calculating a number of points along the CNURB and checking
 * if they lie on the line between pt1 and pt2 (within tolerance).
 * NOTE: eu1 must be the EU referencing cnrb!!!!
 *
 * Returns:
 * 0 - not coincident
 * 1 - coincident
 */
int
nmg_cnurb_lseg_coincident(const struct edgeuse *eu1, const struct edge_g_cnurb *cnrb, const struct face_g_snurb *snrb, const fastf_t *pt1, const fastf_t *pt2, const struct bn_tol *tol)
{
    fastf_t t0, t1, t;
    fastf_t delt;
    int coincident=0;
    int i;


    NMG_CK_EDGEUSE(eu1);
    NMG_CK_EDGE_G_CNURB(cnrb);

    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_cnurb_lseg_coincident(eu1=%p, cnrb=%p, snrb=%p, pt1=(%g %g %g), pt2=(%g %g %g)\n",
	       (void *)eu1, (void *)cnrb, (void *)snrb, V3ARGS(pt1), V3ARGS(pt2));

    if (eu1->g.cnurb_p != cnrb) {
	bu_log("nmg_cnurb_lseg_coincident: cnrb %p isn't from eu %p\n",
	       (void *)cnrb, (void *)eu1);
	bu_bomb("nmg_cnurb_lseg_coincident: cnrb and eu1 disagree\n");
    }

    if (snrb)
	NMG_CK_FACE_G_SNURB(snrb);

    BN_CK_TOL(tol);

    if (cnrb->order <= 0) {
	/* cnrb is linear in parameter space */
	struct vertexuse *vu1;
	struct vertexuse *vu2;
	struct vertexuse_a_cnurb *vua1;
	struct vertexuse_a_cnurb *vua2;

	if (!snrb)
	    bu_bomb("nmg_cnurb_lseg_coincident: No CNURB nor SNURB!!\n");

	vu1 = eu1->vu_p;
	NMG_CK_VERTEXUSE(vu1);
	if (!vu1->a.magic_p) {
	    bu_log("nmg_cnurb_lseg_coincident: vu (%p) has no attributes\n",
		   (void *)vu1);
	    bu_bomb("nmg_cnurb_lseg_coincident: vu has no attributes\n");
	}

	if (*vu1->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC) {
	    bu_log("nmg_cnurb_lseg_coincident: vu (%p) from CNURB EU (%p) is not CNURB\n",
		   (void *)vu1, (void *)eu1);
	    bu_bomb("nmg_cnurb_lseg_coincident: vu from CNURB EU is not CNURB\n");
	}

	vua1 = vu1->a.cnurb_p;
	NMG_CK_VERTEXUSE_A_CNURB(vua1);

	vu2 = eu1->eumate_p->vu_p;
	NMG_CK_VERTEXUSE(vu2);
	if (!vu2->a.magic_p) {
	    bu_log("nmg_cnurb_lseg_coincident: vu (%p) has no attributes\n",
		   (void *)vu2);
	    bu_bomb("nmg_cnurb_lseg_coincident: vu has no attributes\n");
	}

	if (*vu2->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC) {
	    bu_log("nmg_cnurb_lseg_coincident: vu (%p) from CNURB EU (%p) is not CNURB\n",
		   (void *)vu2, (void *)eu1);
	    bu_bomb("nmg_cnurb_lseg_coincident: vu from CNURB EU is not CNURB\n");
	}

	vua2 = vu2->a.cnurb_p;
	NMG_CK_VERTEXUSE_A_CNURB(vua2);

	coincident = 1;
	for (i=0; i<CHECK_NUMBER; i++) {
	    point_t uvw;
	    point_t xyz;
	    fastf_t blend;
	    fastf_t dist;
	    point_t pca;

	    blend = (double)(i+1)/(double)(CHECK_NUMBER+1);
	    VBLEND2(uvw, blend, vua1->param, (1.0-blend), vua2->param);

	    nmg_eval_linear_trim_curve(snrb, uvw, xyz);

	    if (bn_dist_pt3_lseg3(&dist, pca, pt1, pt2, xyz, tol) > 2) {
		coincident = 0;
		break;
	    }
	}
	if (nmg_debug & DEBUG_MESH)
	    bu_log("nmg_cnurb_lseg_coincident returning %d\n", coincident);
	return coincident;
    }

    t0 = cnrb->k.knots[0];
    t1 = cnrb->k.knots[cnrb->k.k_size-1];
    delt = (t1 - t0)/(double)(CHECK_NUMBER+1);

    coincident = 1;
    for (i=0; i<CHECK_NUMBER; i++) {
	point_t xyz;
	fastf_t dist;
	point_t pca;

	t = t0 + (double)(i+1)*delt;

	nmg_eval_trim_curve(cnrb, snrb, t, xyz);

	if (bn_dist_pt3_lseg3(&dist, pca, pt1, pt2, xyz, tol) > 2) {
	    coincident = 0;
	    break;
	}
    }
    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_cnurb_lseg_coincident returning %d\n", coincident);
    return coincident;
}


/**
 * Checks if CNURB eu lies on curve contained in list headed at "head"
 * "Head" must contain a list of points (struct pt_list) each within
 * tolerance of the next. (Just checks at "CHECK_NUMBER" points for now).
 *
 * Returns:
 * 0 - cnurb is not on curve;
 * 1 - cnurb is on curve
 */
int
nmg_cnurb_is_on_crv(const struct edgeuse *eu, const struct edge_g_cnurb *cnrb, const struct face_g_snurb *snrb, const struct bu_list *head, const struct bn_tol *tol)
{
    int i;
    int coincident;
    fastf_t t, t0, t1;
    fastf_t delt;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_EDGE_G_CNURB(cnrb);
    if (snrb)
	NMG_CK_FACE_G_SNURB(snrb);
    BU_CK_LIST_HEAD(head);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_cnurb_is_on_crv(eu=%p, cnrb=%p, snrb=%p, head=%p)\n",
	       (void *)eu, (void *)cnrb, (void *)snrb, (void *)head);
    if (cnrb->order <= 0) {
	struct vertexuse *vu1, *vu2;
	struct vertexuse_a_cnurb *vu1a, *vu2a;
	fastf_t blend;
	point_t uvw;
	point_t xyz;

	/* cnurb is linear in parameter space */

	vu1 = eu->vu_p;
	NMG_CK_VERTEXUSE(vu1);
	vu2 = eu->eumate_p->vu_p;
	NMG_CK_VERTEXUSE(vu2);

	if (!vu1->a.magic_p) {
	    bu_log("nmg_cnurb_is_on_crv(): vu (%p) on CNURB EU (%p) has no attributes\n",
		   (void *)vu1, (void *)eu);
	    bu_bomb("nmg_cnurb_is_on_crv(): vu on CNURB EU has no attributes\n");
	}
	if (*vu1->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC) {
	    bu_log("nmg_cnurb_is_on_crv(): vu (%p) on CNURB EU (%p) is not CNURB\n",
		   (void *)vu1, (void *)eu);
	    bu_bomb("nmg_cnurb_is_on_crv(): vu on CNURB EU is not CNURB\n");
	}
	vu1a = vu1->a.cnurb_p;
	NMG_CK_VERTEXUSE_A_CNURB(vu1a);

	if (!vu2->a.magic_p) {
	    bu_log("nmg_cnurb_is_on_crv(): vu (%p) on CNURB EU (%p) has no attributes\n",
		   (void *)vu2, (void *)eu->eumate_p);
	    bu_bomb("nmg_cnurb_is_on_crv(): vu on CNURB EU has no attributes\n");
	}
	if (*vu2->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC) {
	    bu_log("nmg_cnurb_is_on_crv(): vu (%p) on CNURB EU (%p) is not CNURB\n",
		   (void *)vu2, (void *)eu->eumate_p);
	    bu_bomb("nmg_cnurb_is_on_crv(): vu on CNURB EU is not CNURB\n");
	}
	vu2a = vu2->a.cnurb_p;
	NMG_CK_VERTEXUSE_A_CNURB(vu2a);

	coincident = 1;
	for (i=0; i<CHECK_NUMBER; i++) {
	    struct pt_list *pt;
	    int found=0;

	    blend = (double)(i+1)/(double)(CHECK_NUMBER+1);

	    VBLEND2(uvw, blend, vu1a->param, (1.0-blend), vu2a->param);

	    nmg_eval_linear_trim_curve(snrb, uvw, xyz);

	    for (BU_LIST_FOR(pt, pt_list, head)) {
		vect_t diff;

		VSUB2(diff, xyz, pt->xyz);
		if (MAGSQ(diff) <= tol->dist_sq) {
		    found = 1;
		    break;
		}
	    }
	    if (!found) {
		coincident = 0;
		break;
	    }
	}
	if (nmg_debug & DEBUG_MESH)
	    bu_log("nmg_cnurb_is_on_crv() returning %d\n", coincident);
	return coincident;
    }

    coincident = 1;
    t0 = cnrb->k.knots[0];
    t1 = cnrb->k.knots[cnrb->k.k_size-1];
    delt = (t1 - t0)/(double)(CHECK_NUMBER+1);
    for (i=0; i<CHECK_NUMBER; i++) {
	point_t xyz;
	struct pt_list *pt;
	int found;

	t = t0 + (double)(i+1)*delt;

	nmg_eval_trim_curve(cnrb, snrb, t, xyz);

	found = 0;
	for (BU_LIST_FOR(pt, pt_list, head)) {
	    vect_t diff;

	    VSUB2(diff, xyz, pt->xyz);
	    if (MAGSQ(diff) <= tol->dist_sq) {
		found = 1;
		break;
	    }
	}
	if (!found) {
	    coincident = 0;
	    break;
	}
    }

    if (nmg_debug & DEBUG_MESH)
	bu_log("nmg_cnurb_is_on_crv returning %d\n", coincident);
    return coincident;
}

/* compare function for bu_sort within function nmg_edge_fuse */
static int
v_ptr_comp(const void *p1, const void *p2, void *UNUSED(arg))
{
    size_t i, j;

    i = ((size_t *)p1)[1];
    j = ((size_t *)p2)[1];

    if (i == j)
	return 0;
    else if (i > j)
	return 1;
    return -1;
}


/**
 * Note: If a bu_ptbl structure is passed into this function, the
 *       structure must contain edgeuse. Vertices will then be fused
 *       at the shell level. If an NMG structure is passed into this
 *       function, if the structure is an NMG region or model, vertices
 *       will be fused at the model level. If the NMG structure passed
 *       in is a shell or anything lower, vertices will be fused at the
 *       shell level.
 */
int
nmg_edge_fuse(const uint32_t *magic_p, const struct bn_tol *tol)
{
    typedef size_t (*edgeuse_vert_list_t)[2];
    edgeuse_vert_list_t edgeuse_vert_list;
    int count=0;
    size_t nelem;
    struct bu_ptbl *eu_list;
    struct bu_ptbl tmp;
    struct edge *e1;
    struct edgeuse *eu, *eu1;
    struct vertex *v1;
    register size_t i, j;
    register struct edge *e2;
    register struct edgeuse *eu2;
    register struct vertex *v2;
    struct shell *s;
    const uint32_t *tmp_magic_p;

    if (*magic_p == BU_PTBL_MAGIC) {
	tmp_magic_p = (const uint32_t *)BU_PTBL_GET((struct bu_ptbl *)magic_p, 0);
	if (*tmp_magic_p != NMG_EDGEUSE_MAGIC) {
	    bu_bomb("nmg_edge_fuse(): passed bu_ptbl structure not containing edgeuse");
	}
    } else {
	tmp_magic_p = magic_p;
    }

    s = nmg_find_shell(tmp_magic_p);
    (void)nmg_vertex_fuse(&s->magic, tol);

    if (*magic_p == BU_PTBL_MAGIC) {
	eu_list = (struct bu_ptbl *)magic_p;
    } else {
	bu_ptbl_init(&tmp, 64, "eu_list buffer");
	nmg_edgeuse_tabulate(&tmp, magic_p);
	eu_list = &tmp;
    }

    nelem = BU_PTBL_END(eu_list) * 2;
    if (nelem == 0)
	return 0;

    edgeuse_vert_list = (edgeuse_vert_list_t)bu_calloc(nelem, 2 * sizeof(size_t), "edgeuse_vert_list");

    j = 0;
    for (i = 0; i < (size_t)BU_PTBL_END(eu_list) ; i++) {
	eu = (struct edgeuse *)BU_PTBL_GET(eu_list, i);
	edgeuse_vert_list[j][0] = (size_t)eu;
	edgeuse_vert_list[j][1] = (size_t)eu->vu_p->v_p;
	j++;
	edgeuse_vert_list[j][0] = (size_t)eu;
	edgeuse_vert_list[j][1] = (size_t)eu->eumate_p->vu_p->v_p;
	j++;
    }

    bu_sort(&edgeuse_vert_list[0][0], nelem, 2 * sizeof(size_t), v_ptr_comp, NULL);

    for (i = 0; i < nelem ; i++) {

	eu1 = (struct edgeuse *)edgeuse_vert_list[i][0];

	if (!eu1) {
	    continue;
	}

	v1 = (struct vertex *)edgeuse_vert_list[i][1];
	e1 = eu1->e_p;

	for (j = i+1; j < nelem ; j++) {

	    eu2 = (struct edgeuse *)edgeuse_vert_list[j][0];

	    if (!eu2) {
		continue;
	    }

	    v2 = (struct vertex *)edgeuse_vert_list[j][1];
	    e2 = eu2->e_p;

	    if (v1 != v2) {
		break; /* no more to test */
	    }

	    if (e1 == e2) {
		/* we found ourselves, or already fused, mark as fused and continue */
		edgeuse_vert_list[j][0] = (size_t)NULL;
		continue;
	    }

	    if (NMG_ARE_EUS_ADJACENT(eu1, eu2)) {
		count++;
		nmg_radial_join_eu(eu1, eu2, tol);
		edgeuse_vert_list[j][0] = (size_t)NULL; /* mark as fused */
	    }
	}
    }

    bu_free((char *)edgeuse_vert_list, "edgeuse_vert_list");

    /* if bu_ptbl was passed into this function don't free it here */
    if (*magic_p != BU_PTBL_MAGIC) {
	bu_ptbl_free(eu_list);
    }

    return count;
}


/* compare function for bu_sort within function nmg_edge_g_fuse */
static int
e_rr_xyp_comp(const void *p1, const void *p2, void *arg)
{
    fastf_t i, j;
    fastf_t *edge_rr_xyp = (fastf_t *)arg;
    i = edge_rr_xyp[((*((size_t *)p1)))];
    j = edge_rr_xyp[(*((size_t *)p2))];

    if (EQUAL(i, j))
	return 0;
    else if (i > j)
	return 1;
    return -1;
}


/**
 * Fuse edge_g structs.
 */
int
nmg_edge_g_fuse(const uint32_t *magic_p, const struct bn_tol *tol)
{
    register size_t i, j;
    register struct edge_g_lseg *eg1, *eg2;
    register struct edgeuse *eu1, *eu2;
    register struct vertex *eu1v1, *eu1v2, *eu2v1, *eu2v2;
    struct bu_ptbl etab; /* edge table */
    size_t etab_cnt; /* edge count */
    int total;
    fastf_t tmp;

    /* rise over run arrays for the xz and yz planes */
    fastf_t *edge_rr, *edge_rr_xzp, *edge_rr_yzp, *edge_rr_xyp;

    /* index into all arrays sorted by the contents of array edge_rr_xyp */
    size_t *sort_idx_xyp;

    /* arrays containing special case flags for each edge in the xy, xz and yz planes */
    /* 0 = no special case, 1 = infinite ratio, 2 = zero ratio, 3 = point in plane (no ratio) */
    char *edge_sc, *edge_sc_xyp, *edge_sc_xzp, *edge_sc_yzp;

    /* Make a list of all the edge geometry structs in the model */
    nmg_edge_g_tabulate(&etab, magic_p);

    /* number of edges in table etab */
    etab_cnt = BU_PTBL_LEN(&etab);

    if (etab_cnt == 0) {
	return 0;
    }

    sort_idx_xyp = (size_t *)bu_calloc(etab_cnt, sizeof(size_t), "sort_idx_xyp");

    edge_rr = (fastf_t *)bu_calloc(etab_cnt * 3, sizeof(fastf_t), "edge_rr_xyp");
    /* rise over run in xy plane */
    edge_rr_xyp = edge_rr;
    /* rise over run in xz plane */
    edge_rr_xzp = edge_rr + etab_cnt;
    /* rise over run in yz plane */
    edge_rr_yzp = edge_rr + (etab_cnt * 2);

    edge_sc = (char *)bu_calloc(etab_cnt * 3, sizeof(char), "edge_sc_xyp");
    /* special cases in xy plane */
    edge_sc_xyp = edge_sc;
    /* special cases in xz plane */
    edge_sc_xzp = edge_sc + etab_cnt;
    /* special cases in yz plane */
    edge_sc_yzp = edge_sc + (etab_cnt * 2);

    /* load arrays */
    for (i = 0 ; i < etab_cnt ; i++) {
	point_t pt1, pt2;
	register fastf_t xdif, ydif, zdif;
	register fastf_t dist = tol->dist;

	eg1 = (struct edge_g_lseg *)BU_PTBL_GET(&etab, i);
	eu1 = BU_LIST_MAIN_PTR(edgeuse, BU_LIST_FIRST(bu_list, &eg1->eu_hd2), l2);

	VMOVE(pt1, eu1->vu_p->v_p->vg_p->coord);
	VMOVE(pt2, eu1->eumate_p->vu_p->v_p->vg_p->coord);

	xdif = fabs(pt2[X] - pt1[X]);
	ydif = fabs(pt2[Y] - pt1[Y]);
	zdif = fabs(pt2[Z] - pt1[Z]);
	sort_idx_xyp[i] = i;

	if ((xdif < dist) && (ydif > dist)) {
	    edge_rr_xyp[i] = MAX_FASTF;
	    edge_sc_xyp[i] = 1; /* no angle in xy plane, vertical line (along y-axis) */
	} else if ((xdif > dist) && (ydif < dist)) {
	    edge_sc_xyp[i] = 2; /* no angle in xy plane, horz line (along x-axis) */
	} else if ((xdif < dist) && (ydif < dist)) {
	    edge_sc_xyp[i] = 3; /* only a point in the xy plane */
	    edge_rr_xyp[i] = -MAX_FASTF;
	} else {
	    edge_rr_xyp[i] = (pt2[Y] - pt1[Y]) / (pt2[X] - pt1[X]); /* rise over run */
	}

	if ((xdif < dist) && (zdif > dist)) {
	    edge_sc_xzp[i] = 1; /* no angle in xz plane, vertical line (along z-axis) */
	} else if ((xdif > dist) && (zdif < dist)) {
	    edge_sc_xzp[i] = 2; /* no angle in xz plane, horz line (along x-axis) */
	} else if ((xdif < dist) && (zdif < dist)) {
	    edge_sc_xzp[i] = 3; /* only a point in the xz plane */
	} else {
	    edge_rr_xzp[i] = (pt2[Z] - pt1[Z]) / (pt2[X] - pt1[X]); /* rise over run */
	}

	if ((ydif < dist) && (zdif > dist)) {
	    edge_sc_yzp[i] = 1; /* no angle in yz plane, vertical line (along z-axis) */
	} else if ((ydif > dist) && (zdif < dist)) {
	    edge_sc_yzp[i] = 2; /* no angle in yz plane, horz line (along y-axis) */
	} else if ((ydif < dist) && (zdif < dist)) {
	    edge_sc_yzp[i] = 3; /* only a point in the yz plane */
	} else {
	    edge_rr_yzp[i] = (pt2[Z] - pt1[Z]) / (pt2[Y] - pt1[Y]); /* rise over run */
	}
    }

    /* create sort index based on array edge_rr_xyp */
    bu_sort(sort_idx_xyp, etab_cnt, sizeof(size_t), e_rr_xyp_comp, edge_rr_xyp);

    /* main loop */
    total = 0;
    for (i = 0 ; i < etab_cnt ; i++) {

	eg1 = (struct edge_g_lseg *)BU_PTBL_GET(&etab, sort_idx_xyp[i]);

	if (!eg1) {
	    continue;
	}

	if (UNLIKELY(eg1->l.magic == NMG_EDGE_G_CNURB_MAGIC)) {
	    continue;
	}

	eu1 = BU_LIST_MAIN_PTR(edgeuse, BU_LIST_FIRST(bu_list, &eg1->eu_hd2), l2);
	eu1v1 = eu1->vu_p->v_p;
	eu1v2 = eu1->eumate_p->vu_p->v_p;

	for (j = i+1; j < etab_cnt ; j++) {

	    eg2 = (struct edge_g_lseg *)BU_PTBL_GET(&etab, sort_idx_xyp[j]);

	    if (!eg2) {
		continue;
	    }

	    if (UNLIKELY(eg2->l.magic == NMG_EDGE_G_CNURB_MAGIC)) {
		continue;
	    }

	    if (UNLIKELY(eg1 == eg2)) {
		BU_PTBL_SET(&etab, sort_idx_xyp[j], NULL);
		continue;
	    }

	    eu2 = BU_LIST_MAIN_PTR(edgeuse, BU_LIST_FIRST(bu_list, &eg2->eu_hd2), l2);
	    eu2v1 = eu2->vu_p->v_p;
	    eu2v2 = eu2->eumate_p->vu_p->v_p;

	    if (!(((eu1v1 == eu2v2) && (eu1v2 == eu2v1)) || ((eu1v1 == eu2v1) && (eu1v2 == eu2v2)))) {
		/* true when vertices are not fused */

		/* xy plane tests */
		if (edge_sc_xyp[sort_idx_xyp[i]] != edge_sc_xyp[sort_idx_xyp[j]]) {
		    /* not parallel in xy plane */
		    break;
		} else if (edge_sc_xyp[sort_idx_xyp[i]] == 0) {
		    tmp = edge_rr_xyp[sort_idx_xyp[i]] - edge_rr_xyp[sort_idx_xyp[j]];
		    if (fabs(tmp) > tol->dist) {
			/* not parallel in xy plane */
			break;
		    }
		}

		/* xz plane tests */
		if (edge_sc_xzp[sort_idx_xyp[i]] != edge_sc_xzp[sort_idx_xyp[j]]) {
		    /* not parallel in xz plane */
		    continue;
		} else if (edge_sc_xzp[sort_idx_xyp[i]] == 0) {
		    tmp = edge_rr_xzp[sort_idx_xyp[i]] - edge_rr_xzp[sort_idx_xyp[j]];
		    if (fabs(tmp) > tol->dist) {
			/* not parallel in xz plane */
			continue;
		    }
		}

		/* yz plane tests */
		if (edge_sc_yzp[sort_idx_xyp[i]] != edge_sc_yzp[sort_idx_xyp[j]]) {
		    /* not parallel in xz plane */
		    continue;
		} else if (edge_sc_yzp[sort_idx_xyp[i]] == 0) {
		    tmp = edge_rr_yzp[sort_idx_xyp[i]] - edge_rr_yzp[sort_idx_xyp[j]];
		    if (fabs(tmp) > tol->dist) {
			/* not parallel in yz plane */
			continue;
		    }
		}

		if (!nmg_2edgeuse_g_coincident(eu1, eu2, tol)) {
		    continue;
		}
	    }

	    total++;
	    nmg_jeg(eg1, eg2);

	    BU_PTBL_SET(&etab, sort_idx_xyp[j], NULL);
	}
    }

    bu_ptbl_free(&etab);
    bu_free(edge_rr, "edge_rr,");
    bu_free(edge_sc, "edge_sc");

    if (UNLIKELY(nmg_debug & DEBUG_BASIC && total > 0))
	bu_log("nmg_edge_g_fuse(): %d edge_g_lseg's fused\n", total);

    return total;
}


#define TOL_MULTIPLES 1.0
/**
 * Check that all the vertices in fu1 are within tol->dist of fu2's surface.
 * fu1 and fu2 may be the same face, or different.
 *
 * This is intended to be a geometric check only, not a topology check.
 * Topology may have become inappropriately shared.
 *
 * Returns -
 * 0 All is well, or all verts are within TOL_MULTIPLES*tol->dist of fu2
 * count Number of verts *not* on fu2's surface when at least one is
 * more than TOL_MULTIPLES*tol->dist from fu2.
 */
int
nmg_ck_fu_verts(struct faceuse *fu1, struct face *f2, const struct bn_tol *tol)
{
    fastf_t dist = 0.0;
    fastf_t worst = 0.0;
    int count = 0;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu;
    struct vertex *v;
    struct vertex_g *vg;
    plane_t pl2;

    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACE(f2);
    BN_CK_TOL(tol);

    NMG_CK_FACE_G_PLANE(f2->g.plane_p);

    HMOVE(pl2, f2->g.plane_p->N);

    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    NMG_CK_VERTEXUSE(vu);
	    NMG_CK_VERTEX(vu->v_p);

	    v = vu->v_p;
	    vg = v->vg_p;

	    if (!vg) {
		bu_bomb("nmg_ck_fu_verts(): vertex with no geometry?\n");
	    }
	    NMG_CK_VERTEX_G(vg);

	    /* Geometry check */
	    dist = DIST_PT_PLANE(vg->coord, pl2);
	    if (!NEAR_ZERO(dist, tol->dist)) {
		if (nmg_debug & DEBUG_MESH) {
		    bu_log("nmg_ck_fu_verts(%p, %p) v %p off face by %e\n",
			   (void *)fu1, (void *)f2, (void *)v, dist);
		    VPRINT(" pt", vg->coord);
		    PLPRINT(" fg2", pl2);
		}
		count++;

		dist = fabs(dist);
		if (dist > worst) {
		    worst = dist;
		}
	    }
	} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);

		v = eu->vu_p->v_p;
		vg = v->vg_p;

		if (!vg)  {
		    bu_bomb("nmg_ck_fu_verts(): vertex with no geometry?\n");
		}
		NMG_CK_VERTEX_G(vg);

		/* Geometry check */
		dist = DIST_PT_PLANE(vg->coord, pl2);
		if (!NEAR_ZERO(dist, tol->dist)) {
		    if (nmg_debug & DEBUG_MESH) {
			bu_log("nmg_ck_fu_verts(%p, %p) v %p off face by %e\n",
			       (void *)fu1, (void *)f2, (void *)v, dist);
			VPRINT(" pt", vg->coord);
			PLPRINT(" fg2", pl2);
		    }
		    count++;

		    dist = fabs(dist);
		    if (dist > worst) {
			worst = dist;
		    }
		}
	    }
	} else {
	    bu_bomb("nmg_ck_fu_verts(): unknown loopuse child\n");
	}
    }

    if (worst > TOL_MULTIPLES*tol->dist) {
	return count;
    } else {
	return 0;
    }
}


/**
 * Similar to nmg_ck_fu_verts, but checks all vertices that use the same
 * face geometry as fu1
 * fu1 and f2 may be the same face, or different.
 *
 * This is intended to be a geometric check only, not a topology check.
 * Topology may have become inappropriately shared.
 *
 * Returns -
 * 0 All is well.
 * count Number of verts *not* on fu2's surface.
 *
 */
int
nmg_ck_fg_verts(struct faceuse *fu1, struct face *f2, const struct bn_tol *tol)
{
    struct face_g_plane *fg1;
    struct faceuse *fu;
    struct face *f;
    int count=0;

    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACE(f2);
    BN_CK_TOL(tol);

    NMG_CK_FACE(fu1->f_p);
    fg1 = fu1->f_p->g.plane_p;
    NMG_CK_FACE_G_PLANE(fg1);

    for (BU_LIST_FOR(f, face, &fg1->f_hd)) {
	NMG_CK_FACE(f);

	fu = f->fu_p;
	NMG_CK_FACEUSE(fu);

	count += nmg_ck_fu_verts(fu, f2, tol);
    }

    return count;
}


/**
 * XXX A better algorithm would be to compare loop by loop.
 * If the two faces share all the verts of at least one loop of 3 or more
 * vertices, then they should be shared.  Otherwise it will be awkward
 * having shared loop(s) on non-shared faces!!
 *
 * Compare the geometry of two faces, and fuse them if they are the
 * same within tolerance.
 * First compare the plane equations.  If they are "similar" (within tol),
 * then check all verts in f2 to make sure that they are within tol->dist
 * of f1's geometry.  If they are, then fuse the face geometry.
 *
 * Returns -
 * 0 Faces were not fused.
 * >0 Faces were successfully fused.
 */
int
nmg_two_face_fuse(struct face *f1, struct face *f2, const struct bn_tol *tol)
{
    register struct face_g_plane *fg1;
    register struct face_g_plane *fg2;
    int flip2 = 0;
    fastf_t dist;

    fg1 = f1->g.plane_p;
    fg2 = f2->g.plane_p;

    if (!fg1 || !fg2) {
	if (nmg_debug & DEBUG_MESH) {
	    bu_log("nmg_two_face_fuse(%p, %p) null fg fg1=%p, fg2=%p\n",
		   (void *)f1, (void *)f2, (void *)fg1, (void *)fg2);
	}
	return 0;
    }

    /* test if the face geometry (i.e. face plane) is already fused */
    if (fg1 == fg2) {
	if (nmg_debug & DEBUG_MESH) {
	    bu_log("nmg_two_face_fuse(%p, %p) fg already shared\n", (void *)f1, (void *)f2);
	}
	return 0;
    }

    /* if the bounding box of each faceuse is not within distance
     * tolerance of each other, then skip fusing
     */
    if (V3RPP_DISJOINT_TOL(f1->min_pt, f1->max_pt, f2->min_pt, f2->max_pt, tol->dist)) {
	return 0;
    }

    /* check the distance between the planes at their center */
    dist = fabs(fg1->N[W] - fg2->N[W]);
    if (!NEAR_ZERO(dist, tol->dist)) {
	return 0;
    }

    /* check that each vertex of each faceuse is within tolerance of
     * the plane of the other faceuse
     */
    if (nmg_ck_fg_verts(f1->fu_p, f2, tol) || nmg_ck_fg_verts(f2->fu_p, f1, tol)) {
	if (nmg_debug & DEBUG_MESH) {
	    bu_log("nmg_two_face_fuse: verts not within tol of surface, can't fuse\n");
	}
	return 0;
    }

    if (nmg_debug & DEBUG_MESH) {
	bu_log("nmg_two_face_fuse(%p, %p) coplanar faces, flip2=%d\n",
	       (void *)f1, (void *)f2, flip2);
    }

    /* check if normals are pointing in the same direction */
    if (VDOT(fg1->N, fg2->N) >= SMALL_FASTF) {
	/* same direction */
	flip2 = 0;
    } else {
	/* opposite direction */
	flip2 = 1;
    }

    if (flip2 == 0) {
	if (nmg_debug & DEBUG_MESH) {
	    bu_log("joining face geometry (same dir) f1=%p, f2=%p\n", (void *)f1, (void *)f2);
	    PLPRINT(" fg1", fg1->N);
	    PLPRINT(" fg2", fg2->N);
	}
	nmg_jfg(f1, f2);
    } else {
	register struct face *fn;
	if (nmg_debug & DEBUG_MESH) {
	    bu_log("joining face geometry (opposite dirs)\n");

	    bu_log(" f1=%p, flip=%d", (void *)f1, f1->flip);
	    PLPRINT(" fg1", fg1->N);

	    bu_log(" f2=%p, flip=%d", (void *)f2, f2->flip);
	    PLPRINT(" fg2", fg2->N);
	}
	/* Flip flags of faces using fg2, first! */
	for (BU_LIST_FOR(fn, face, &fg2->f_hd)) {
	    fn->flip = !fn->flip;
	    if (nmg_debug & DEBUG_MESH) {
		bu_log("f=%p, new flip=%d\n", (void *)fn, fn->flip);
	    }
	}
	nmg_jfg(f1, f2);
    }
    return 1;
}


/**
 * A routine to find all face geometry structures in an nmg shell that
 * have the same plane equation, and have them share face geometry.
 * (See also nmg_shell_coplanar_face_merge(), which actually moves
 * the loops into one face).
 *
 * The criteria for two face geometry structs being the "same" are:
 * 1) The plane equations must be the same, within tolerance.
 * 2) All the vertices on the 2nd face must lie within the
 * distance tolerance of the 1st face's plane equation.
 */
int
nmg_shell_face_fuse(struct shell *s, const struct bn_tol *tol)
{
    struct bu_ptbl ftab;
    int total = 0;
    register int i, j;

    /* Make a list of all the face structs in the model */
    nmg_face_tabulate(&ftab, &s->magic);

    for (i = BU_PTBL_END(&ftab)-1; i >= 0; i--) {
	register struct face *f1;
	register struct face_g_plane *fg1;
	f1 = (struct face *)BU_PTBL_GET(&ftab, i);

	if (*f1->g.magic_p == NMG_FACE_G_SNURB_MAGIC) {
	    /* XXX Need routine to compare 2 snurbs for equality here */
	    continue;
	}

	fg1 = f1->g.plane_p;

	for (j = i-1; j >= 0; j--) {
	    register struct face *f2;
	    register struct face_g_plane *fg2;

	    f2 = (struct face *)BU_PTBL_GET(&ftab, j);
	    fg2 = f2->g.plane_p;
	    if (!fg2) continue;

	    if (fg1 == fg2) continue;	/* Already shared */

	    if (nmg_two_face_fuse(f1, f2, tol) > 0)
		total++;
	}
    }
    bu_ptbl_free(&ftab);
    if (nmg_debug & DEBUG_BASIC && total > 0)
	bu_log("nmg_model_face_fuse: %d faces fused\n", total);
    return total;
}


int
nmg_break_all_es_on_v(uint32_t *magic_p, struct vertex *v, const struct bn_tol *tol)
{
    struct bu_ptbl eus;
    int i;
    int count=0;
    const char *magic_type;

    if (UNLIKELY(nmg_debug & DEBUG_BOOL)) {
	bu_log("nmg_break_all_es_on_v(magic=%p, v=%p)\n", (void *)magic_p, (void *)v);
    }

    magic_type = bu_identify_magic(*magic_p);
    if (UNLIKELY(BU_STR_EQUAL(magic_type, "NULL") ||
		 BU_STR_EQUAL(magic_type, "Unknown_Magic"))) {
	bu_log("Bad magic pointer passed to nmg_break_all_es_on_v (%s)\n", magic_type);
	bu_bomb("Bad magic pointer passed to nmg_break_all_es_on_v()\n");
    }

    nmg_edgeuse_tabulate(&eus, magic_p);

    for (i = 0; i < BU_PTBL_END(&eus); i++) {
	struct edgeuse *eu;
	struct vertex *va;
	struct vertex *vb;
	fastf_t dist;
	int code;

	eu = (struct edgeuse *)BU_PTBL_GET(&eus, i);

	if (eu->g.magic_p && *eu->g.magic_p == NMG_EDGE_G_CNURB_MAGIC) {
	    continue;
	}
	va = eu->vu_p->v_p;
	vb = eu->eumate_p->vu_p->v_p;

	if (va == v || bn_pt3_pt3_equal(va->vg_p->coord, v->vg_p->coord, tol)) {
	    continue;
	}
	if (vb == v || bn_pt3_pt3_equal(vb->vg_p->coord, v->vg_p->coord, tol)) {
	    continue;
	}
	if (UNLIKELY(va == vb || bn_pt3_pt3_equal(va->vg_p->coord, vb->vg_p->coord, tol))) {
	    bu_bomb("nmg_break_all_es_on_v(): found zero length edgeuse");
	}

	code = bn_isect_pt_lseg(&dist, va->vg_p->coord, vb->vg_p->coord,
				v->vg_p->coord, tol);

	if (code < 1) continue;	/* missed */

	if (UNLIKELY(code == 1 || code == 2)) {
	    bu_bomb("nmg_break_all_es_on_v(): internal error");
	}
	/* Break edge on vertex, but don't fuse yet. */

	if (UNLIKELY(nmg_debug & DEBUG_BOOL)) {
	    bu_log("\tnmg_break_all_es_on_v: breaking eu %p on v %p\n", (void *)eu, (void *)v);
	}
	(void)nmg_ebreak(v, eu);
	count++;
    }
    bu_ptbl_free(&eus);
    return count;
}


/**
 * As the first step in evaluating a boolean formula,
 * before starting to do face/face intersections, compare every
 * edge in the model with every vertex in the model.
 * If the vertex is within tolerance of the edge, break the edge,
 * and enroll the new edge on a list of edges still to be processed.
 *
 * A list of edges and a list of vertices are built, and then processed.
 *
 * Space partitioning could improve the performance of this algorithm.
 * For the moment, a brute-force approach is used.
 *
 * Returns -
 * Number of edges broken.
 */
int
nmg_break_e_on_v(const uint32_t *magic_p, const struct bn_tol *tol)
{
    int count = 0;
    struct bu_ptbl verts;
    struct bu_ptbl edgeuses;
    struct bu_ptbl new_edgeuses;
    register struct edgeuse **eup;
    vect_t e_min_pt, e_max_pt;

    BN_CK_TOL(tol);

    nmg_e_and_v_tabulate(&edgeuses, &verts, magic_p);

    /* Repeat the process until no new edgeuses are created */
    while (BU_PTBL_LEN(&edgeuses) > 0) {
	(void)bu_ptbl_init(&new_edgeuses, 64, " &new_edgeuses");

	for (eup = (struct edgeuse **)BU_PTBL_LASTADDR(&edgeuses);
	     eup >= (struct edgeuse **)BU_PTBL_BASEADDR(&edgeuses);
	     eup--
	    ) {
	    register struct edgeuse *eu;
	    register struct vertex *va;
	    register struct vertex *vb;
	    register struct vertex **vp;

	    eu = *eup;
	    if (eu->g.magic_p && *eu->g.magic_p == NMG_EDGE_G_CNURB_MAGIC)
		continue;
	    va = eu->vu_p->v_p;
	    vb = eu->eumate_p->vu_p->v_p;

	    /* find edge bounding box */
	    VMOVE(e_min_pt, va->vg_p->coord);
	    VMIN(e_min_pt, vb->vg_p->coord);
	    VMOVE(e_max_pt, va->vg_p->coord);
	    VMAX(e_max_pt, vb->vg_p->coord);

	    for (vp = (struct vertex **)BU_PTBL_LASTADDR(&verts);
		 vp >= (struct vertex **)BU_PTBL_BASEADDR(&verts);
		 vp--
		) {
		register struct vertex *v;
		fastf_t dist;
		int code;
		struct edgeuse *new_eu;

		v = *vp;
		if (va == v) continue;
		if (vb == v) continue;

		if (V3PT_OUT_RPP_TOL(v->vg_p->coord, e_min_pt, e_max_pt, tol->dist)) {
		    continue;
		}

		/* A good candidate for inline expansion */
		code = bn_isect_pt_lseg(&dist,
					va->vg_p->coord,
					vb->vg_p->coord,
					v->vg_p->coord, tol);

		if (code < 1) continue;	/* missed */
		if (code == 1 || code == 2) {
		    bu_log("nmg_break_e_on_v() code=%d, why wasn't this vertex fused?\n", code);
		    continue;
		}

		if (nmg_debug & (DEBUG_BOOL|DEBUG_BASIC))
		    bu_log("nmg_break_e_on_v(): breaking eu %p (e=%p) at vertex %p\n",
			   (void *)eu, (void *)eu->e_p, (void *)v);

		/* Break edge on vertex, but don't fuse yet. */
		new_eu = nmg_ebreak(v, eu);
		/* Put new edges into follow-on list */
		bu_ptbl_ins(&new_edgeuses, (long *)&new_eu->l.magic);

		/* reset vertex vb */
		vb = eu->eumate_p->vu_p->v_p;
		count++;
	    }
	}
	bu_ptbl_free(&edgeuses);
	edgeuses = new_edgeuses;		/* struct copy */
    }
    bu_ptbl_free(&edgeuses);
    bu_ptbl_free(&verts);
    if (nmg_debug & (DEBUG_BOOL|DEBUG_BASIC))
	bu_log("nmg_break_e_on_v() broke %d edges\n", count);
    return count;
}


/* DEPRECATED: use nmg_break_e_on_v() */
int
nmg_shell_break_e_on_v(const uint32_t *magic_p, const struct bn_tol *tol)
{
    return nmg_break_e_on_v(magic_p, tol);
}

/**
 * This is the primary application interface to the geometry fusing support.
 * Fuse together all data structures that are equal to each other,
 * within tolerance.
 *
 * The algorithm is three part:
 * 1) Fuse together all vertices.
 * 2) Fuse together all face geometry, where appropriate.
 * 3) Fuse together all edges.
 *
 * Edge fusing is handled last, because the difficult part there is
 * sorting faces radially around the edge.
 * It is important to know whether faces are shared or not
 * at that point.
 *
 * XXX It would be more efficient to build all the ptbl's at once,
 * XXX with a single traversal of the model.
 */
int
nmg_shell_fuse(struct shell *s, const struct bn_tol *tol)
{
    int total = 0;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    /* XXXX vertex fusing and edge breaking can produce vertices that are
     * not within tolerance of their face. Edge breaking needs to be moved
     * to step 1.5, then a routine to make sure all vertices are within
     * tolerance of owning face must be called if "total" is greater than zero.
     * This routine may have to triangulate the face if an appropriate plane
     * cannot be calculated.
     */

    /* Step 1 -- the vertices. */
    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_shell_fuse: vertices\n");
    total += nmg_vertex_fuse(&s->magic, tol);

    /* Step 1.5 -- break edges on vertices, before fusing edges */
    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_shell_fuse: break edges\n");
    total += nmg_break_e_on_v(&s->magic, tol);

    if (total) {
	/* vertices and/or edges have been moved,
	 * may have created out-of-tolerance faces
	 */
	nmg_make_faces_within_tol(s, tol);
    }

    /* Step 2 -- the face geometry */
    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_shell_fuse: faces\n");
    total += nmg_shell_face_fuse(s, tol);

    /* Step 3 -- edges */
    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_shell_fuse: edges\n");
    total += nmg_edge_fuse(&s->magic, tol);

    /* Step 4 -- edge geometry */
    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_shell_fuse: edge geometries\n");
    total += nmg_edge_g_fuse(&s->magic, tol);

    if (nmg_debug & DEBUG_BASIC && total > 0)
	bu_log("nmg_shell_fuse(): %d entities fused\n", total);
    return total;
}


/* -------------------- RADIAL -------------------- */

/**
 * Build sorted list, with 'ang' running from zero to 2*pi.
 * New edgeuses with same angle as an edgeuse already on the list
 * are added AFTER the last existing one, for lack of any better way
 * to break the tie.
 */
void
nmg_radial_sorted_list_insert(struct bu_list *hd, struct nmg_radial *rad)
{
    struct nmg_radial *cur;
    register fastf_t rad_ang;

    BU_CK_LIST_HEAD(hd);
    NMG_CK_RADIAL(rad);

    if (BU_LIST_IS_EMPTY(hd)) {
	BU_LIST_APPEND(hd, &rad->l);
	return;
    }

    /* Put wires at the front */
    if (rad->fu == (struct faceuse *)NULL) {
	/* Add before first element */
	BU_LIST_APPEND(hd, &rad->l);
	return;
    }

    rad_ang = rad->ang;

    /* Check for trivial append at end of list.
     * This is a very common case, when input list is sorted.
     */
    cur = BU_LIST_PREV(nmg_radial, hd);
    if (cur->fu && rad_ang >= cur->ang) {
	BU_LIST_INSERT(hd, &rad->l);
	return;
    }

    /* Brute force search through hd's list, going backwards */
    for (BU_LIST_FOR_BACKWARDS(cur, nmg_radial, hd)) {
	if (cur->fu == (struct faceuse *)NULL) continue;
	if (rad_ang >= cur->ang) {
	    BU_LIST_APPEND(&cur->l, &rad->l);
	    return;
	}
    }

    /* Add before first element */
    BU_LIST_APPEND(hd, &rad->l);
}


/**
 * Not only verity that list is monotone increasing, but that
 * pointer integrity still exists.
 */
void
nmg_radial_verify_pointers(const struct bu_list *hd, const struct bn_tol *tol)
{
    register struct nmg_radial *rad;
    register fastf_t amin = -64;
    register struct nmg_radial *prev;
    register struct nmg_radial *next;

    BU_CK_LIST_HEAD(hd);
    BN_CK_TOL(tol);

    /* Verify pointers increasing */
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	/* Verify pointer integrity */
	prev = BU_LIST_PPREV_CIRC(nmg_radial, rad);
	next = BU_LIST_PNEXT_CIRC(nmg_radial, rad);
	if (rad->eu != prev->eu->radial_p->eumate_p)
	    bu_bomb("nmg_radial_verify_pointers() eu not radial+mate forw from prev\n");
	if (rad->eu->eumate_p != prev->eu->radial_p)
	    bu_bomb("nmg_radial_verify_pointers() eumate not radial from prev\n");
	if (rad->eu != next->eu->eumate_p->radial_p)
	    bu_bomb("nmg_radial_verify_pointers() eu not mate+radial back from next\n");
	if (rad->eu->eumate_p != next->eu->eumate_p->radial_p->eumate_p)
	    bu_bomb("nmg_radial_verify_pointers() eumate not mate+radial+mate back from next\n");

	if (rad->fu == (struct faceuse *)NULL) continue;
	if (rad->ang < amin) {
	    nmg_pr_radial_list(hd, tol);
	    bu_log(" previous angle=%g > current=%g\n",
		   amin*RAD2DEG, rad->ang*RAD2DEG);
	    bu_bomb("nmg_radial_verify_pointers() not monotone increasing\n");
	}
	amin = rad->ang;
    }
}


/**
 *
 * Verify that the angles are monotone increasing.
 * Wire edgeuses are ignored.
 */
void
nmg_radial_verify_monotone(const struct bu_list *hd, const struct bn_tol *tol)
{
    register struct nmg_radial *rad;
    register fastf_t amin = -64;

    BU_CK_LIST_HEAD(hd);
    BN_CK_TOL(tol);

    /* Verify monotone increasing */
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	if (rad->fu == (struct faceuse *)NULL) continue;
	if (rad->ang < amin) {
	    nmg_pr_radial_list(hd, tol);
	    bu_log(" previous angle=%g > current=%g\n",
		   amin*RAD2DEG, rad->ang*RAD2DEG);
	    bu_bomb("nmg_radial_verify_monotone() not monotone increasing\n");
	}
	amin = rad->ang;
    }
}


/**
 * Check if the passed bu_list is in increasing order. If not,
 * reverse the order of the list.
 * XXX Isn't the word "ensure"?
 */
void
nmg_insure_radial_list_is_increasing(struct bu_list *hd, fastf_t amin, fastf_t amax)
{
    struct nmg_radial *rad;
    fastf_t cur_value=(-MAX_FASTF);
    int increasing=1;

    BU_CK_LIST_HEAD(hd);

    /* if we don't have more than 3 entries, it doesn't matter */

    if (bu_list_len(hd) < 3)
	return;

    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	/* skip wire edges */
	if (rad->fu == (struct faceuse *)NULL)
	    continue;

	/* if increasing, just keep checking */
	if (rad->ang >= cur_value) {
	    cur_value = rad->ang;
	    continue;
	}

	/* angle decreases, is it going from max to min?? */
	if (ZERO(rad->ang - amin) && ZERO(cur_value - amax)) {
	    /* O.K., just went from max to min */
	    cur_value = rad->ang;
	    continue;
	}

	/* if we get here, this list is not increasing!!! */
	increasing = 0;
	break;
    }

    if (increasing)	/* all is well */
	return;

    /* reverse order of the list */
    bu_list_reverse(hd);

    /* Need to exchange eu with eu->eumate_p for each eu on the list */
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	rad->eu = rad->eu->eumate_p;
	rad->fu = nmg_find_fu_of_eu(rad->eu);
    }
}


/**
 * The coordinate system is expected to have been chosen in such a
 * way that the radial list of faces around this edge are circularly
 * increasing (CCW) in their angle.  Put them in the list in exactly
 * the order they occur around the edge.  Then, at the end, move the
 * list head to lie between the maximum and minimum angles, so that the
 * list head is crossed as the angle goes around through zero.
 * Now the list is monotone increasing.
 *
 * The edgeuse's radial pointer takes us in the CCW direction.
 *
 * If the list contains nmg_radial structures r1, r2, r3, r4,
 * then going CCW around the edge we will encounter:
 *
 *			(from i-1)			(from i+1)
 * r1->eu->eumate_p	r4->eu->radial_p		r2->eu->eumate_p->radial_p->eumate_p
 * r1->eu		r4->eu->radial_p->eumate_p	r2->eu->eumate_p->radial_p
 * r2->eu->eumate_p	r1->eu->radial_p		r3->eu->eumate_p->radial_p->eumate_p
 * r2->eu		r1->eu->radial_p->eumate_p	r3->eu->eumate_p->radial_p
 * r3->eu->eumate_p	r2->eu->radial_p		r4->eu->eumate_p->radial_p->eumate_p
 * r3->eu		r2->eu->radial_p->eumate_p	r4->eu->eumate_p->radial_p
 * r4->eu->eumate_p	r3->eu->radial_p		r1->eu->eumate_p->radial_p->eumate_p
 * r4->eu		r3->eu->radial_p->eumate_p	r1->eu->eumate_p->radial_p
 */
void
nmg_radial_build_list(struct bu_list *hd, struct bu_ptbl *shell_tbl, int existing, struct edgeuse *eu, const fastf_t *xvec, const fastf_t *yvec, const fastf_t *zvec, const struct bn_tol *tol)

/* may be null */


/* for printing */
{
    struct edgeuse *teu;
    struct nmg_radial *rad;
    fastf_t amin;
    fastf_t amax;
    int non_wire_edges=0;
    struct nmg_radial *rmin = (struct nmg_radial *)NULL;
    struct nmg_radial *rmax = (struct nmg_radial *)NULL;
    struct nmg_radial *first;

    BU_CK_LIST_HEAD(hd);
    if (shell_tbl) BU_CK_PTBL(shell_tbl);
    NMG_CK_EDGEUSE(eu);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_BASIC || nmg_debug & DEBUG_MESH_EU)
	bu_log("nmg_radial_build_list(existing=%d, eu=%p)\n", existing, (void *)eu);

    if (ZERO(MAGSQ(xvec)) || ZERO(MAGSQ(yvec)) || ZERO(MAGSQ(zvec))) {
	bu_log("nmg_radial_build_list(): one or more input vector(s) 'xvec', 'yvec', 'zvec' is zero magnitude.\n");
    }

    amin = 64;
    amax = -64;

    teu = eu;
    for (;;) {
	BU_GET(rad, struct nmg_radial);
	rad->l.magic = NMG_RADIAL_MAGIC;
	rad->eu = teu;
	rad->fu = nmg_find_fu_of_eu(teu);
	if (rad->fu) {
	    /* We depend on ang being strictly in the range 0..2pi */
	    rad->ang = nmg_measure_fu_angle(teu, xvec, yvec, zvec);

	    if (rad->ang < -SMALL_FASTF) {
		bu_bomb("nmg_radial_build_list(): fu_angle should not be negative\n");
	    }
	    if (rad->ang > (M_2PI + SMALL_FASTF)) {
		bu_bomb("nmg_radial_build_list(): fu_angle should not be > 2pi\n");
	    }

	    non_wire_edges++;

	    if ((rad->ang < amin) || (ZERO(rad->ang - amin))) {
		amin = rad->ang;
		rmin = rad;
	    }

	    if ((rad->ang > amax) || (ZERO(rad->ang - amax))) {
		amax = rad->ang;
		rmax = rad;
	    }
	} else {
	    /* Wire edge.  Set a preposterous angle */
	    rad->ang = -M_PI;	/* -180 */
	}
	rad->s = nmg_find_s_of_eu(teu);
	rad->existing_flag = existing;
	rad->needs_flip = 0;	/* not yet determined */
	rad->is_crack = 0;	/* not yet determined */
	rad->is_outie = 0;	/* not yet determined */

	if (nmg_debug & DEBUG_MESH_EU)
	    bu_log("\trad->eu = %p, rad->ang = %g\n", (void *)rad->eu, rad->ang);

	/* the radial list is not always sorted */
	nmg_radial_sorted_list_insert(hd, rad);

	/* Add to list of all shells involved at this edge */
	if (shell_tbl) bu_ptbl_ins_unique(shell_tbl, (long *)&(rad->s->magic));

	/* Advance to next edgeuse pair */
	teu = teu->radial_p->eumate_p;
	if (teu == eu) break;
    }

    /* Increasing, with min or max value possibly repeated */
    if (!rmin) {
	/* Nothing but wire edgeuses, done. */
	return;
    }

    /* At least one non-wire edgeuse was found */
    if (non_wire_edges < 2) {
	/* only one non wire entry in list */
	return;
    }

    if (nmg_debug & DEBUG_MESH_EU) {
	struct nmg_radial *next;

	bu_log("amin=%g min_eu=%p, amax=%g max_eu=%p\n",
	       rmin->ang * RAD2DEG, (void *)rmin->eu,
	       rmax->ang * RAD2DEG, (void *)rmax->eu);

	for (BU_LIST_FOR(next, nmg_radial, hd))
	    bu_log("%p: eu=%p, fu=%p, ang=%g\n", (void *)next, (void *)next->eu, (void *)next->fu, next->ang);
    }

    /* Skip to extremal repeated max&min.  Ignore wires */
    first = rmax;
    for (;;) {
	struct nmg_radial *next;
	next = rmax;
	do {
	    next = BU_LIST_PNEXT_CIRC(nmg_radial, next);
	} while (next->fu == (struct faceuse *)NULL);

	if ((next->ang > amax) || (ZERO(next->ang - amax))) {
	    rmax = next;		/* a repeated max */
	    if (rmax == first)	/* we have gone all the way around (All angles are same) */
		break;
	} else
	    break;
    }
    /* wires before min establish new rmin */
    first = rmin;
    for (;;) {
	struct nmg_radial *next;

	while ((next = BU_LIST_PPREV_CIRC(nmg_radial, rmin))->fu == (struct faceuse *)NULL)
	    rmin = next;
	next = BU_LIST_PPREV_CIRC(nmg_radial, rmin);
	if ((next->ang < amin) || (ZERO(next->ang - amin))) {
	    rmin = next;		/* a repeated min */
	    if (rmin == first) {
		/* all the way round again (All angles are same) */
		/* set rmin to next entry after rmax */
		rmin = BU_LIST_PNEXT_CIRC(nmg_radial, rmax);
		break;
	    }
	} else
	    break;
    }

    /* Move list head so that it is in between min and max entries. */
    if (BU_LIST_PNEXT_CIRC(nmg_radial, rmax) == rmin) {
	/* Maximum entry is followed by minimum.  Ascending --> CCW */
	BU_LIST_DEQUEUE(hd);
	/* Append head after maximum, before minimum */
	BU_LIST_APPEND(&(rmax->l), hd);
    } else {
	bu_log("  %f %f %f --- %f %f %f\n",
	       V3ARGS(eu->vu_p->v_p->vg_p->coord),
	       V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord));
	bu_log("amin=%g min_eu=%p, amax=%g max_eu=%p B\n",
	       rmin->ang * RAD2DEG, (void *)rmin->eu,
	       rmax->ang * RAD2DEG, (void *)rmax->eu);
	nmg_pr_radial_list(hd, tol);
	nmg_pr_fu_around_eu_vecs(eu, xvec, yvec, zvec, tol);
	bu_bomb("nmg_radial_build_list() min and max angle not adjacent in list (or list not monotone increasing)\n");
    }
}


/**
 * Merge all of the src list into the dest list, sorting by angles.
 */
void
nmg_radial_merge_lists(struct bu_list *dest, struct bu_list *src, const struct bn_tol *tol)
{
    struct nmg_radial *rad;

    BU_CK_LIST_HEAD(dest);
    BU_CK_LIST_HEAD(src);
    BN_CK_TOL(tol);

    while (BU_LIST_WHILE(rad, nmg_radial, src)) {
	BU_LIST_DEQUEUE(&rad->l);
	nmg_radial_sorted_list_insert(dest, rad);
    }
}


/**
 * If there is more than one edgeuse of a loopuse along an edge, then
 * it is a "topological crack".  There are two kinds, an "innie",
 * where the crack is a null-area incursion into the interior of the
 * loop, and an "outie", where the crack is a null-area protrusion
 * outside of the interior of the loop.
 *
 *			 "Outie"		 "Innie"
 *			*-------*		*-------*
 *			|       ^		|       ^
 *			v       |		v       |
 *		*<------*       |		*--->*  |
 *		*---M-->*       |		*<-M-*  |
 *			|       |		|       |
 *			v       |		v       |
 *			*------>*		*------>*
 *
 * The algorithm used is to compute the geometric midpoint of the
 * crack edge, "delete" that edge from the loop, and then classify the
 * midpoint ("M") against the remainder of the loop.  If the edge
 * midpoint is inside the remains of the loop, then the crack is an
 * "innie", otherwise it is an "outie".
 *
 * When there are an odd number of edgeuses along the crack, then the
 * situation is "nasty":
 *
 *			 "Nasty"
 *			*-------*
 *			|       ^
 *			v       |
 *		*<------*       |
 *		*------>*       |
 *		*<------*       |
 *		*------>*       |
 *		*<------*       |
 *		|		|
 *		|		|
 *		v		|
 *		*------------->*
 *
 * The caller is responsible for making sure that the edgeuse is not a
 * wire edgeuse (i.e. that the edgeuse is part of a loop).
 *
 * In the "Nasty" case, all the edgeuse pairs are "outies" except for
 * the last lone edgeuse, which should be handled as a "non-crack".
 * Walk the loopuse's edgeuses list in edgeuse order to see which one
 * is the last (non-crack) repeated edgeuse.  For efficiency,
 * detecting and dealing with this condition is left up to the caller,
 * and is not checked for here.
 */
int
nmg_is_crack_outie(const struct edgeuse *eu, const struct bn_tol *tol)
{
    const struct loopuse *lu;
    const struct edge *e;
    point_t midpt;
    const fastf_t *a, *b;
    int nmg_class;

    NMG_CK_EDGEUSE(eu);
    BN_CK_TOL(tol);

    lu = eu->up.lu_p;
    NMG_CK_LOOPUSE(lu);
    e = eu->e_p;
    NMG_CK_EDGE(e);

    /* If ENTIRE loop is a crack, there is no surface area, it's an outie */
    if (nmg_loop_is_a_crack(lu)) return 1;

    a = eu->vu_p->v_p->vg_p->coord;
    b = eu->eumate_p->vu_p->v_p->vg_p->coord;
    VADD2SCALE(midpt, a, b, 0.5);

    /* Ensure edge is long enough so midpoint is not within tol of verts */
    {
	/* all we want here is a classification of the midpoint,
	 * so let's create a temporary tolerance that will work!!! */

	struct bn_tol tmp_tol;
	struct faceuse *fu;
	plane_t pl;
	fastf_t dist;

	tmp_tol = (*tol);
	if (*lu->up.magic_p != NMG_FACEUSE_MAGIC)
	    bu_bomb("Nmg_is_crack_outie called with non-face loop");

	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);
	NMG_GET_FU_PLANE(pl, fu);
	dist = DIST_PT_PLANE(midpt, pl);
	VJOIN1(midpt, midpt, -dist, pl);
	dist = fabs(DIST_PT_PLANE(midpt, pl));
	if (dist > SMALL_FASTF) {
	    tmp_tol.dist = dist*2.0;
	    tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
	} else {
	    tmp_tol.dist = SMALL_FASTF;
	    tmp_tol.dist_sq = SMALL_FASTF * SMALL_FASTF;
	}
	nmg_class = nmg_class_pt_lu_except(midpt, lu, e, &tmp_tol);
    }
    if (nmg_debug & DEBUG_BASIC) {
	bu_log("nmg_is_crack_outie(eu=%p) lu=%p, e=%p, nmg_class=%s\n",
	       (void *)eu, (void *)lu, (void *)e, nmg_class_name(nmg_class));
    }

    if (lu->orientation == OT_SAME) {
	if (nmg_class == NMG_CLASS_AinB || nmg_class == NMG_CLASS_AonBshared)
	    return 0;		/* an "innie" */
	if (nmg_class == NMG_CLASS_AoutB)
	    return 1;		/* an "outie" */
    } else {
	/* It's a hole loop, things work backwards. */
	if (nmg_class == NMG_CLASS_AinB || nmg_class == NMG_CLASS_AonBshared)
	    return 1;		/* an "outie" */
	if (nmg_class == NMG_CLASS_AoutB)
	    return 0;		/* an "innie" */
    }

    /* Other classifications "shouldn't happen". */
    bu_log("nmg_is_crack_outie(eu=%p), lu=%p(%s)\n  midpt_class=%s, midpt=(%g, %g, %g)\n",
	   (void *)eu,
	   (void *)lu, nmg_orientation(lu->orientation),
	   nmg_class_name(nmg_class),
	   V3ARGS(midpt));
    nmg_pr_lu_briefly(lu, 0);
    bu_bomb("nmg_is_crack_outie() got unexpected midpt classification from nmg_class_pt_lu_except()\n");

    return -1; /* make the compiler happy */
}


struct nmg_radial *
nmg_find_radial_eu(const struct bu_list *hd, const struct edgeuse *eu)
{
    register struct nmg_radial *rad;

    BU_CK_LIST_HEAD(hd);
    NMG_CK_EDGEUSE(eu);

    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	if (rad->eu == eu) return rad;
	if (rad->eu->eumate_p == eu) return rad;
    }
    bu_log("nmg_find_radial_eu() eu=%p\n", (void *)eu);
    bu_bomb("nmg_find_radial_eu() given edgeuse not found on list\n");

    return (struct nmg_radial *)NULL;
}


/**
 * Find the next use of either of two edges in the loopuse.
 * The second edge pointer may be NULL.
 */
const struct edgeuse *
nmg_find_next_use_of_2e_in_lu(const struct edgeuse *eu, const struct edge *e1, const struct edge *e2)


/* may be NULL */
{
    register const struct edgeuse *neu;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_LOOPUSE(eu->up.lu_p);	/* sanity */
    NMG_CK_EDGE(e1);
    if (e2) NMG_CK_EDGE(e2);

    neu = eu;
    do {
	neu = BU_LIST_PNEXT_CIRC(edgeuse, &neu->l);
    } while (neu->e_p != e1 && neu->e_p != e2);
    return neu;

}


/**
 * For every edgeuse, if there are other edgeuses around this edge
 * from the same face, then mark them all as part of a "crack".
 *
 * To be a crack the two edgeuses must be from the same loopuse.
 *
 * If the count of repeated ("crack") edgeuses is even, then
 * classify the entire crack as an "innie" or an "outie".
 * If the count is odd, this is a "Nasty" --
 * all but one edgeuse are marked as "outies",
 * and the remaining one is marked as a non-crack.
 * The "outie" edgeuses are marked off in pairs,
 * in the loopuse's edgeuse order.
 */
void
nmg_radial_mark_cracks(struct bu_list *hd, const struct edge *e1, const struct edge *e2, const struct bn_tol *tol)


/* may be NULL */

{
    struct nmg_radial *rad;
    struct nmg_radial *other;
    const struct loopuse *lu;
    const struct edgeuse *eu;
    register int uses;
    int outie = -1;

    BU_CK_LIST_HEAD(hd);
    NMG_CK_EDGE(e1);
    if (e2) NMG_CK_EDGE(e2);
    BN_CK_TOL(tol);

    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	NMG_CK_RADIAL(rad);
	if (rad->is_crack) continue;
	if (!rad->fu) continue;		/* skip wire edges */
	lu = rad->eu->up.lu_p;
	uses = 0;

	/* Search the remainder of the list for other uses */
	for (other = BU_LIST_PNEXT(nmg_radial, rad);
	     BU_LIST_NOT_HEAD(other, hd);
	     other = BU_LIST_PNEXT(nmg_radial, other)
	    ) {
	    if (!other->fu) continue;	/* skip wire edges */
	    /* Only consider edgeuses from the same loopuse */
	    if (other->eu->up.lu_p != lu &&
		other->eu->eumate_p->up.lu_p != lu)
		continue;
	    uses++;
	}
	if (uses <= 0) {
	    /* The main search continues to end of list */
	    continue;		/* not a crack */
	}
	uses++;		/* account for first use too */

	/* OK, we have a crack. Which kind? */
	if ((uses & 1) == 0) {
	    /* Even number of edgeuses. */
	    outie = nmg_is_crack_outie(rad->eu, tol);
	    rad->is_crack = 1;
	    rad->is_outie = outie;
	    if (nmg_debug & DEBUG_MESH_EU) {
		bu_log("nmg_radial_mark_cracks() EVEN crack eu=%p, uses=%d, outie=%d\n",
		       (void *)rad->eu, uses, outie);
	    }
	    /* Mark all the rest of them the same way */
	    for (other = BU_LIST_PNEXT(nmg_radial, rad);
		 BU_LIST_NOT_HEAD(other, hd);
		 other = BU_LIST_PNEXT(nmg_radial, other)
		) {
		if (!other->fu) continue;	/* skip wire edges */
		/* Only consider edgeuses from the same loopuse */
		if (other->eu->up.lu_p != lu &&
		    other->eu->eumate_p->up.lu_p != lu)
		    continue;
		other->is_crack = 1;
		other->is_outie = outie;
	    }
	    if (nmg_debug & DEBUG_MESH_EU) {
		bu_log("Printing loopuse and resulting radial list:\n");
		nmg_pr_lu_briefly(lu, 0);
		nmg_pr_radial_list(hd, tol);
	    }
	    continue;
	}
	/*
	 * Odd number of edgeuses.  Traverse in loopuse order.
	 * All but the last one are "outies", last one is "innie"
	 */
	if (nmg_debug & DEBUG_MESH_EU) {
	    bu_log("nmg_radial_mark_cracks() ODD crack eu=%p, uses=%d, outie=%d\n",
		   (void *)rad->eu, uses, outie);
	}
	/* Mark off pairs of edgeuses, one per trip through loop. */
	eu = rad->eu;
	for (; uses >= 2; uses--) {
	    eu = nmg_find_next_use_of_2e_in_lu(eu, e1, e2);
	    if (nmg_debug & DEBUG_MESH_EU) {
		bu_log("rad->eu=%p, eu=%p, uses=%d\n",
		       (void *)rad->eu, (void *)eu, uses);
	    }
	    if (eu == rad->eu) {
		nmg_pr_lu_briefly(lu, 0);
		nmg_pr_radial_list(hd, tol);
		bu_bomb("nmg_radial_mark_cracks() loop too short!\n");
	    }

	    other = nmg_find_radial_eu(hd, eu);
	    /* Mark 'em as "outies" */
	    other->is_crack = 1;
	    other->is_outie = 1;
	}

	/* Should only be one left, this one is an "innie":  it borders surface area */
	eu = nmg_find_next_use_of_2e_in_lu(eu, e1, e2);
	if (eu != rad->eu) {
	    nmg_pr_lu_briefly(lu, 0);
	    nmg_pr_radial_list(hd, tol);
	    bu_bomb("nmg_radial_mark_cracks() loop didn't return to start\n");
	}

	rad->is_crack = 1;
	rad->is_outie = 0;		/* "innie" */
    }
}


/**
 * Returns -
 * NULL No edgeuses from indicated shell on this list
 * nmg_radial* An original, else first newbie, else a newbie crack.
 */
struct nmg_radial *
nmg_radial_find_an_original(const struct bu_list *hd, const struct shell *s, const struct bn_tol *tol)
{
    register struct nmg_radial *rad;
    struct nmg_radial *fallback = (struct nmg_radial *)NULL;
    int seen_shell = 0;

    BU_CK_LIST_HEAD(hd);
    NMG_CK_SHELL(s);

    /* First choice:  find an original, non-crack, non-wire */
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	NMG_CK_RADIAL(rad);
	if (rad->s != s) continue;
	seen_shell++;
	if (rad->is_outie) {
	    fallback = rad;
	    continue;	/* skip "outie" cracks */
	}
	if (!rad->fu) continue;	/* skip wires */
	if (rad->existing_flag) return rad;
    }
    if (!seen_shell) return (struct nmg_radial *)NULL;	/* No edgeuses from that shell, at all! */

    /* Next, an original crack would be OK */
    if (fallback) return fallback;

    /* If there were no originals, find first newbie */
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	if (rad->s != s) continue;
	if (rad->is_outie) {
	    fallback = rad;
	    continue;	/* skip "outie" cracks */
	}
	if (!rad->fu) {
	    continue;	/* skip wires */
	}
	return rad;
    }
    /* If no ordinary newbies, provide a newbie crack */
    if (fallback) return fallback;

    /* No ordinary newbiew or newbie cracks, any wires? */
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	if (rad->s != s) continue;
	if (rad->is_outie) {
	    continue;	/* skip "outie" cracks */
	}
	if (!rad->fu) {
	    fallback = rad;
	    continue;	/* skip wires */
	}
	return rad;
    }
    if (fallback) return fallback;

    bu_log("nmg_radial_find_an_original() shell=%p\n", (void *)s);
    nmg_pr_radial_list(hd, tol);
    bu_bomb("nmg_radial_find_an_original() No entries from indicated shell\n");

    return (struct nmg_radial *)NULL;
}


/**
 * For a given shell, find an original edgeuse from that shell,
 * and then mark parity violators with a "flip" flag.
 */
int
nmg_radial_mark_flips(struct bu_list *hd, const struct shell *s, const struct bn_tol *tol)
{
    struct nmg_radial *rad;
    struct nmg_radial *orig;
    register int expected_ot;
    int count = 0;
    int nflip = 0;

    BU_CK_LIST_HEAD(hd);
    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    orig = nmg_radial_find_an_original(hd, s, tol);
    NMG_CK_RADIAL(orig);
    if (orig->is_outie) {
	/* Only originals were "outie" cracks.  No flipping */
	return 0;
    }

    if (!orig->fu) {
	/* nothing but wires */
	return 0;
    }
    if (!orig->existing_flag) {
	/* There were no originals.  Do something sensible to check the newbies */
	if (!orig->fu) {
	    /* Nothing but wires */
	    return 0;
	}
    }

    expected_ot = !(orig->fu->orientation == OT_SAME);
    if (!orig->is_outie) count++;	/* Don't count orig if "outie" crack */

    for (BU_LIST_FOR_CIRC(rad, nmg_radial, orig)) {
	if (rad->s != s) continue;
	if (!rad->fu) continue;	/* skip wires */
	if (rad->is_outie) continue;	/* skip "outie" cracks */
	count++;
	if (expected_ot == (rad->fu->orientation == OT_SAME)) {
	    expected_ot = !expected_ot;
	    continue;
	}
	/* Mis-match detected */
	if (nmg_debug & DEBUG_MESH_EU) {
	    bu_log("nmg_radial_mark_flips() Mis-match detected, setting flip flag eu=%p\n", (void *)rad->eu);
	}
	rad->needs_flip = !rad->needs_flip;
	nflip++;
	/* With this one flipped, set expectation for next */
	expected_ot = !expected_ot;
    }

    if (count & 1) {
	return count;
    }

    if (expected_ot == (orig->fu->orientation == OT_SAME))
	return nflip;

    bu_log("nmg_radial_mark_flips() unable to establish proper orientation parity.\n  eu count=%d, shell=%p, expectation=%d\n",
	   count, (void *)s, expected_ot);
    nmg_pr_radial_list(hd, tol);
    bu_bomb("nmg_radial_mark_flips() unable to establish proper orientation parity.\n");

    return 0; /* for compiler */
}


/**
 * For each shell, check orientation parity of edgeuses within that shell.
 */
int
nmg_radial_check_parity(const struct bu_list *hd, const struct bu_ptbl *shells, const struct bn_tol *tol)
{
    struct nmg_radial *rad;
    struct shell **sp;
    struct nmg_radial *orig;
    register int expected_ot;
    int count = 0;

    BU_CK_LIST_HEAD(hd);
    BU_CK_PTBL(shells);
    BN_CK_TOL(tol);

    for (sp = (struct shell **)BU_PTBL_LASTADDR(shells);
	 sp >= (struct shell **)BU_PTBL_BASEADDR(shells); sp--
	) {

	NMG_CK_SHELL(*sp);
	orig = nmg_radial_find_an_original(hd, *sp, tol);
	if (!orig) continue;
	NMG_CK_RADIAL(orig);
	if (!orig->existing_flag) {
	    /* There were no originals.  Do something sensible to check the newbies */
	    if (!orig->fu) continue;	/* Nothing but wires */
	}
	if (orig->is_outie) continue;	/* Loop was nothing but outies */
	expected_ot = !(orig->fu->orientation == OT_SAME);

	for (BU_LIST_FOR_CIRC(rad, nmg_radial, orig)) {
	    if (rad->s != *sp) continue;
	    if (!rad->fu) continue;	/* skip wires */
	    if (rad->is_outie) continue;	/* skip "outie" cracks */
	    if (expected_ot == (rad->fu->orientation == OT_SAME)) {
		expected_ot = !expected_ot;
		continue;
	    }
	    /* Mis-match detected */
	    bu_log("nmg_radial_check_parity() bad parity eu=%p, s=%p\n",
		   (void *)rad->eu, (void *)*sp);
	    count++;
	    /* Set expectation for next */
	    expected_ot = !expected_ot;
	}
	if (expected_ot == (orig->fu->orientation == OT_SAME))
	    continue;
	bu_log("nmg_radial_check_parity() bad parity at END eu=%p, s=%p\n",
	       (void *)rad->eu, (void *)*sp);
	count++;
    }
    return count;
}


/**
 * For all non-original edgeuses in the list, place them in the proper
 * place around the destination edge.
 */
void
nmg_radial_implement_decisions(struct bu_list *hd, const struct bn_tol *tol, struct edgeuse *eu1, fastf_t *xvec, fastf_t *yvec, fastf_t *zvec)

/* for printing */
/* temp */
/*** temp ***/
{
    struct nmg_radial *rad;
    struct nmg_radial *prev;
    int skipped;

    BU_CK_LIST_HEAD(hd);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_radial_implement_decisions() BEGIN\n");

again:
    skipped = 0;
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	struct edgeuse *dest;

	if (rad->existing_flag) continue;
	prev = BU_LIST_PPREV_CIRC(nmg_radial, rad);
	if (!prev->existing_flag) {
	    /* Previous eu isn't in place yet, can't do this one until next pass. */
	    skipped++;
	    continue;
	}

	/*
	 * Insert "rad" CCW radial from "prev".
	 *
	 */
	if (nmg_debug & DEBUG_MESH_EU) {
	    bu_log("Before -- ");
	    nmg_pr_fu_around_eu_vecs(eu1, xvec, yvec, zvec, tol);
	    nmg_pr_radial("prev:", (const struct nmg_radial *)prev);
	    nmg_pr_radial(" rad:", (const struct nmg_radial *)rad);
	}
	dest = prev->eu;
	if (rad->needs_flip) {
	    struct edgeuse *other_eu = rad->eu->eumate_p;

	    nmg_je(dest, rad->eu);
	    rad->eu = other_eu;
	    rad->fu = nmg_find_fu_of_eu(other_eu);
	    rad->needs_flip = 0;
	} else {
	    nmg_je(dest, rad->eu->eumate_p);
	}
	rad->existing_flag = 1;
	if (nmg_debug & DEBUG_MESH_EU) {
	    bu_log("After -- ");
	    nmg_pr_fu_around_eu_vecs(eu1, xvec, yvec, zvec, tol);
	}
    }
    if (skipped) {
	if (nmg_debug & DEBUG_BASIC)
	    bu_log("nmg_radial_implement_decisions() %d remaining, go again\n", skipped);
	goto again;
    }

    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_radial_implement_decisions() END\n");
}


void
nmg_pr_radial(const char *title, const struct nmg_radial *rad)
{
    struct face *f;
    int orient;

    NMG_CK_RADIAL(rad);
    if (!rad->fu) {
	f = (struct face *)NULL;
	orient = 'W';
    } else {
	f = rad->fu->f_p;
	orient = nmg_orientation(rad->fu->orientation)[3];
    }
    bu_log("%s%p, mate of \\/\n",
	   title,
	   (void *)rad->eu->eumate_p
	);
    bu_log("%s%p, f=%p, fu=%p=%c, s=%p %s %c%c%c %g deg\n",
	   title,
	   (void *)rad->eu,
	   (void *)f, (void *)rad->fu, orient,
	   (void *)rad->s,
	   rad->existing_flag ? "old" : "new",
	   rad->needs_flip ? 'F' : '/',
	   rad->is_crack ? 'C' : '/',
	   rad->is_outie ? 'O' : (rad->is_crack ? 'I' : '/'),
	   rad->ang * RAD2DEG
	);
}


/**
 * Patterned after nmg_pr_fu_around_eu_vecs(), with similar format.
 */
void
nmg_pr_radial_list(const struct bu_list *hd, const struct bn_tol *tol)

/* for printing */
{
    struct nmg_radial *rad;

    BU_CK_LIST_HEAD(hd);
    BN_CK_TOL(tol);

    bu_log("nmg_pr_radial_list(hd=%p)\n", (void *)hd);

    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	NMG_CK_RADIAL(rad);
	nmg_pr_radial(" ", rad);
    }
}


/**
 * This routine looks for nmg_radial structures with the same angle,
 * and sorts them to match the order of nmg_radial structures that
 * are not at that same angle
 */
void
nmg_do_radial_flips(struct bu_list *hd)
{
    struct nmg_radial *start_same;
    struct bn_tol tol;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    BU_CK_LIST_HEAD(hd);

    start_same = BU_LIST_FIRST(nmg_radial, hd);
    while (BU_LIST_NOT_HEAD(&start_same->l, hd)) {
	struct nmg_radial *next_after_same;
	struct nmg_radial *end_same;
	struct nmg_radial *same;
	struct nmg_radial *check;

	if (!start_same->fu) {
	    start_same = BU_LIST_PNEXT(nmg_radial, &start_same->l);
	    continue;
	}

	end_same = BU_LIST_PNEXT_CIRC(nmg_radial, &start_same->l);
	while ((ZERO(end_same->ang - start_same->ang) && !ZERO(end_same - start_same))
	       || !end_same->fu)
	    end_same = BU_LIST_PNEXT_CIRC(nmg_radial, &end_same->l);
	end_same = BU_LIST_PPREV_CIRC(nmg_radial, &end_same->l);

	if (start_same == end_same) {
	    start_same = BU_LIST_PNEXT(nmg_radial, &start_same->l);
	    continue;
	}

	/* more than one eu at same angle, sort them according to shell */
	next_after_same = BU_LIST_PNEXT_CIRC(nmg_radial, &end_same->l);
	while (!next_after_same->fu && next_after_same != start_same)
	    next_after_same = BU_LIST_PNEXT_CIRC(nmg_radial, &next_after_same->l);

	if (next_after_same == start_same) {
	    /* no other radials with faces */
	    return;
	}

	check = next_after_same;
	while (start_same != end_same && check != start_same) {
	    same = end_same;
	    while (same->s != check->s && same != start_same)
		same = BU_LIST_PPREV_CIRC(nmg_radial, &same->l);

	    if (same->s != check->s) {
		/* couldn't find any other radial from shell "same->s"
		 * so put look at next radial
		 */

		check = BU_LIST_PNEXT_CIRC(nmg_radial, &check->l);
		continue;
	    }

	    /* same->s matches check->s, so move "same" to right after end_same */
	    if (same == start_same) {
		/* starting radial matches, need to move it and
		 * set pointer to new start_same
		 */
		start_same = BU_LIST_PNEXT_CIRC(nmg_radial, &start_same->l);
		BU_LIST_DEQUEUE(&same->l);
		BU_LIST_APPEND(&end_same->l, &same->l);
	    } else if (same == end_same) {
		/* already in correct place, just move end_same */
		end_same = BU_LIST_PPREV_CIRC(nmg_radial, &end_same->l);
	    } else {
		BU_LIST_DEQUEUE(&same->l);
		BU_LIST_APPEND(&end_same->l, &same->l);
	    }

	    check = BU_LIST_PNEXT_CIRC(nmg_radial, &check->l);
	}

	start_same = BU_LIST_PNEXT(nmg_radial, &end_same->l);
    }
}


/**
 * Perform radial join of edges in list "hd" based on direction with respect
 * to "eu1ref"
 */

void
nmg_do_radial_join(struct bu_list *hd, struct edgeuse *eu1ref, fastf_t *xvec, fastf_t *yvec, fastf_t *zvec, const struct bn_tol *tol)
{
    struct nmg_radial *rad;
    struct nmg_radial *prev;
    vect_t ref_dir;
    int skipped;

    BU_CK_LIST_HEAD(hd);
    NMG_CK_EDGEUSE(eu1ref);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_MESH_EU)
	bu_log("nmg_do_radial_join() START\n");

    nmg_do_radial_flips(hd);

    VSUB2(ref_dir, eu1ref->eumate_p->vu_p->v_p->vg_p->coord, eu1ref->vu_p->v_p->vg_p->coord);

    if (nmg_debug & DEBUG_MESH_EU)
	bu_log("ref_dir = (%g %g %g)\n", V3ARGS(ref_dir));

top:

    if (nmg_debug & DEBUG_MESH_EU) {
	bu_log("At top of nmg_do_radial_join:\n");
	nmg_pr_radial_list(hd, tol);
    }

    skipped = 0;
    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	struct edgeuse *dest;
	struct edgeuse *src;
	vect_t src_dir;
	vect_t dest_dir;

	if (rad->existing_flag)
	    continue;

	prev = BU_LIST_PPREV_CIRC(nmg_radial, rad);
	if (!prev->existing_flag) {
	    skipped++;
	    continue;
	}

	VSUB2(dest_dir, prev->eu->eumate_p->vu_p->v_p->vg_p->coord, prev->eu->vu_p->v_p->vg_p->coord);
	VSUB2(src_dir, rad->eu->eumate_p->vu_p->v_p->vg_p->coord, rad->eu->vu_p->v_p->vg_p->coord);

	if (!prev->fu || !rad->fu) {
	    nmg_je(prev->eu, rad->eu);
	    continue;
	}

	if (VDOT(dest_dir, ref_dir) < -SMALL_FASTF)
	    dest = prev->eu->eumate_p;
	else
	    dest = prev->eu;

	if (VDOT(src_dir, ref_dir) > SMALL_FASTF)
	    src = rad->eu->eumate_p;
	else
	    src = rad->eu;

	if (nmg_debug & DEBUG_MESH_EU) {
	    bu_log("Before -- ");
	    nmg_pr_fu_around_eu_vecs(eu1ref, xvec, yvec, zvec, tol);
	    nmg_pr_radial("prev:", prev);
	    nmg_pr_radial(" rad:", rad);

	    if (VDOT(dest_dir, ref_dir) < -SMALL_FASTF)
		bu_log("dest_dir disagrees with eu1ref\n");
	    else
		bu_log("dest_dir agrees with eu1ref\n");

	    if (VDOT(src_dir, ref_dir) < -SMALL_FASTF)
		bu_log("src_dir disagrees with eu1ref\n");
	    else
		bu_log("src_dir agrees with eu1ref\n");

	    bu_log("Joining dest_eu=%p to src_eu=%p\n", (void *)dest, (void *)src);
	}

	nmg_je(dest, src);
	rad->existing_flag = 1;
	if (nmg_debug & DEBUG_MESH_EU) {
	    bu_log("After -- ");
	    nmg_pr_fu_around_eu_vecs(eu1ref, xvec, yvec, zvec, tol);
	}
    }

    if (skipped)
	goto top;

    if (nmg_debug & DEBUG_MESH_EU)
	bu_log("nmg_do_radial_join() DONE\n\n");
}


/**
 * A new routine, that uses "global information" about the edge
 * to plan the operations to be performed.
 */
void
nmg_radial_join_eu_NEW(struct edgeuse *eu1, struct edgeuse *eu2, const struct bn_tol *tol)
{
    struct edgeuse *eu1ref;		/* reference eu for eu1 */
    struct edgeuse *eu2ref;
    struct faceuse *fu1;
    struct faceuse *fu2;
    struct nmg_radial *rad;
    vect_t xvec, yvec, zvec;
    struct bu_list list1;
    struct bu_list list2;
    struct bu_ptbl shell_tbl;

    NMG_CK_EDGEUSE(eu1);
    NMG_CK_EDGEUSE(eu2);
    BN_CK_TOL(tol);

    if (eu1->e_p == eu2->e_p) return;

    if (!NMG_ARE_EUS_ADJACENT(eu1, eu2))
	bu_bomb("nmg_radial_join_eu_NEW() edgeuses don't share vertices.\n");

    if (eu1->vu_p->v_p == eu1->eumate_p->vu_p->v_p) bu_bomb("nmg_radial_join_eu_NEW(): 0 length edge (topology)\n");

    if (bn_pt3_pt3_equal(eu1->vu_p->v_p->vg_p->coord,
			 eu1->eumate_p->vu_p->v_p->vg_p->coord, tol))
	bu_bomb("nmg_radial_join_eu_NEW(): 0 length edge (geometry)\n");

    /* Ensure faces are of same orientation, if both eu's have faces */
    fu1 = nmg_find_fu_of_eu(eu1);
    fu2 = nmg_find_fu_of_eu(eu2);
    if (fu1 && fu2) {
	if (fu1->orientation != fu2->orientation) {
	    eu2 = eu2->eumate_p;
	    fu2 = nmg_find_fu_of_eu(eu2);
	    if (fu1->orientation != fu2->orientation)
		bu_bomb("nmg_radial_join_eu_NEW(): Cannot find matching orientations for faceuses\n");
	}
    }

    if (eu1->eumate_p->radial_p == eu1 && eu2->eumate_p->radial_p == eu2 &&
	nmg_find_s_of_eu(eu1) == nmg_find_s_of_eu(eu2))
    {
	/* Only joining two edges, let's keep it simple */
	nmg_je(eu1, eu2);
	if (eu1->g.magic_p && eu2->g.magic_p) {
	    if (eu1->g.magic_p != eu2->g.magic_p)
		nmg_jeg(eu1->g.lseg_p, eu2->g.lseg_p);
	} else if (eu1->g.magic_p && !eu2->g.magic_p)
	    (void)nmg_use_edge_g(eu2, eu1->g.magic_p);
	else if (!eu1->g.magic_p && eu2->g.magic_p)
	    (void)nmg_use_edge_g(eu1, eu2->g.magic_p);
	else {
	    nmg_edge_g(eu1);
	    nmg_use_edge_g(eu2, eu1->g.magic_p);
	}
	return;
    }

    /* XXX This angle-based algorithm can't yet handle snurb faces! */
    if (fu1 && fu1->f_p->g.magic_p && *fu1->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC) return;
    if (fu2 && fu2->f_p->g.magic_p && *fu2->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC) return;

    /* Construct local coordinate system for this edge,
     * so all angles can be measured relative to a common reference.
     */
    if (fu1) {
	if (fu1->orientation == OT_SAME)
	    eu1ref = eu1;
	else
	    eu1ref = eu1->eumate_p;
    } else {
	/* eu1 is a wire, find a non-wire, if there are any */
	eu1ref = nmg_find_ot_same_eu_of_e(eu1->e_p);
    }
    if (eu1ref->vu_p->v_p == eu2->vu_p->v_p) {
	eu2ref = eu2;
    } else {
	eu2ref = eu2->eumate_p;
    }
    nmg_eu_2vecs_perp(xvec, yvec, zvec, eu1ref, tol);

    /* Build the primary lists that describe the situation */
    BU_LIST_INIT(&list1);
    BU_LIST_INIT(&list2);
    bu_ptbl_init(&shell_tbl, 64, " &shell_tbl");

    nmg_radial_build_list(&list1, &shell_tbl, 1, eu1ref, xvec, yvec, zvec, tol);
    nmg_radial_build_list(&list2, &shell_tbl, 0, eu2ref, xvec, yvec, zvec, tol);

    if (nmg_debug & DEBUG_MESH_EU) {
	bu_log("nmg_radial_join_eu_NEW(eu1=%p, eu2=%p) e1=%p, e2=%p\n",
	       (void *)eu1, (void *)eu2,
	       (void *)eu1->e_p, (void *)eu2->e_p);
	nmg_euprint("\tJoining", eu1);
	bu_log("Faces around eu1:\n");
	nmg_pr_fu_around_eu_vecs(eu1ref, xvec, yvec, zvec, tol);
	nmg_pr_radial_list(&list1, tol);

	bu_log("Faces around eu2:\n");
	nmg_pr_fu_around_eu_vecs(eu2ref, xvec, yvec, zvec, tol);
	nmg_pr_radial_list(&list2, tol);
	nmg_pr_ptbl("Participating shells", &shell_tbl, 1);
    }

    /* Merge the two lists, sorting by angles */
    nmg_radial_merge_lists(&list1, &list2, tol);
    nmg_radial_verify_monotone(&list1, tol);

    if (nmg_debug & DEBUG_MESH_EU) {
	bu_log("Before nmg_do_radial_join():\n");
	bu_log("xvec=(%g %g %g), yvec=(%g %g %g), zvec=(%g %g %g)\n", V3ARGS(xvec), V3ARGS(yvec), V3ARGS(zvec));
	nmg_pr_fu_around_eu_vecs(eu2ref, xvec, yvec, zvec, tol);
    }
    nmg_do_radial_join(&list1, eu1ref, xvec, yvec, zvec, tol);

    /* Clean up */
    bu_ptbl_free(&shell_tbl);
    while (BU_LIST_WHILE(rad, nmg_radial, &list1)) {
	BU_LIST_DEQUEUE(&rad->l);
	BU_PUT(rad, struct nmg_radial);
    }
    return;
}


/**
 * Exchange eu and eu->eumate_p on the radial list, where marked.
 */
void
nmg_radial_exchange_marked(struct bu_list *hd, const struct bn_tol *tol)

/* for printing */
{
    struct nmg_radial *rad;

    BU_CK_LIST_HEAD(hd);
    BN_CK_TOL(tol);

    for (BU_LIST_FOR(rad, nmg_radial, hd)) {
	struct edgeuse *eu;
	struct edgeuse *eumate;
	struct edgeuse *before;
	struct edgeuse *after;

	if (!rad->needs_flip) continue;

	/*
	 * Initial sequencing is:
	 * before(radial), eu, eumate, after(radial)
	 */
	eu = rad->eu;
	eumate = eu->eumate_p;
	before = eu->radial_p;
	after = eumate->radial_p;

	/*
	 * Rearrange order to be:
	 * before, eumate, eu, after.
	 */
	before->radial_p = eumate;
	eumate->radial_p = before;

	after->radial_p = eu;
	eu->radial_p = after;

	rad->eu = eumate;
	rad->fu = nmg_find_fu_of_eu(rad->eu);
	rad->needs_flip = 0;
    }
}


/**
 * Visit each edge in this shell exactly once.
 * Where the radial edgeuse parity has become disrupted
 * due to a boolean operation or whatever, fix it.
 */
void
nmg_s_radial_harmonize(struct shell *s, const struct bn_tol *tol)
{
    struct bu_ptbl edges;
    struct edgeuse *eu;
    struct bu_list list;
    vect_t xvec, yvec, zvec;
    struct edge **ep;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_s_radial_harmonize(s=%p) BEGIN\n", (void *)s);

    nmg_edge_tabulate(&edges, &s->magic);
    for (ep = (struct edge **)BU_PTBL_LASTADDR(&edges);
	 ep >= (struct edge **)BU_PTBL_BASEADDR(&edges); ep--
	) {
	struct nmg_radial *rad;
	int nflip;

	NMG_CK_EDGE(*ep);
	eu = nmg_find_ot_same_eu_of_e(*ep);
	nmg_eu_2vecs_perp(xvec, yvec, zvec, eu, tol);

	BU_LIST_INIT(&list);

	nmg_radial_build_list(&list, (struct bu_ptbl *)NULL, 1, eu, xvec, yvec, zvec, tol);
	nflip = nmg_radial_mark_flips(&list, s, tol);
	if (nflip) {
	    if (nmg_debug & DEBUG_MESH_EU) {
		bu_log("Flips needed:\n");
		nmg_pr_radial_list(&list, tol);
	    }
	    /* Now, do the flips */
	    nmg_radial_exchange_marked(&list, tol);
	    if (nmg_debug & DEBUG_MESH_EU) {
		nmg_pr_fu_around_eu_vecs(eu, xvec, yvec, zvec, tol);
	    }
	}
	/* Release the storage */
	while (BU_LIST_WHILE(rad, nmg_radial, &list)) {
	    BU_LIST_DEQUEUE(&rad->l);
	    BU_PUT(rad, struct nmg_radial);
	}
    }

    bu_ptbl_free(&edges);

    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_s_radial_harmonize(s=%p) END\n", (void *)s);
}


/**
 * Visit each edge in this shell exactly once, and check it.
 */
void
nmg_s_radial_check(struct shell *s, const struct bn_tol *tol)
{
    struct bu_ptbl edges;
    struct edge **ep;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_s_radial_check(s=%p) BEGIN\n", (void *)s);

    nmg_edge_tabulate(&edges, &s->magic);
    for (ep = (struct edge **)BU_PTBL_LASTADDR(&edges); ep >= (struct edge **)BU_PTBL_BASEADDR(&edges); ep--) {
	NMG_CK_EDGE(*ep);
    }

    bu_ptbl_free(&edges);

    if (nmg_debug & DEBUG_BASIC)
	bu_log("nmg_s_radial_check(s=%p) END\n", (void *)s);
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
