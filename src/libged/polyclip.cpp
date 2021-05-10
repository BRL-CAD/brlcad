/*                    P O L Y C L I P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/polyclip.cpp
 *
 * An interface to clipper.
 *
 */
/** @} */

#include "common.h"

#include "bu/sort.h"
#include "bg/polygon.h"
#include "bview/util.h"
#include "ged.h"

int
ged_export_polygon(struct ged *gedp, bview_data_polygon_state *gdpsp, size_t polygon_i, const char *sname)
{
    size_t j, k, n;
    size_t num_verts = 0;
    struct rt_db_internal internal;
    struct rt_sketch_internal *sketch_ip;
    struct line_seg *lsg;
    struct directory *dp;
    vect_t view;
    point_t vorigin;
    mat_t invRot;

    GED_CHECK_EXISTS(gedp, sname, LOOKUP_QUIET, GED_ERROR);
    RT_DB_INTERNAL_INIT(&internal);

    if (polygon_i >= gdpsp->gdps_polygons.num_polygons ||
	gdpsp->gdps_polygons.polygon[polygon_i].num_contours < 1)
	return GED_ERROR;

    for (j = 0; j < gdpsp->gdps_polygons.polygon[polygon_i].num_contours; ++j)
	num_verts += gdpsp->gdps_polygons.polygon[polygon_i].contour[j].num_points;

    if (num_verts < 3)
	return GED_ERROR;

    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_SKETCH;
    internal.idb_meth = &OBJ[ID_SKETCH];

    BU_ALLOC(internal.idb_ptr, struct rt_sketch_internal);
    sketch_ip = (struct rt_sketch_internal *)internal.idb_ptr;
    sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch_ip->vert_count = num_verts;
    sketch_ip->verts = (point2d_t *)bu_calloc(sketch_ip->vert_count, sizeof(point2d_t), "sketch_ip->verts");
    sketch_ip->curve.count = num_verts;
    sketch_ip->curve.reverse = (int *)bu_calloc(sketch_ip->curve.count, sizeof(int), "sketch_ip->curve.reverse");
    sketch_ip->curve.segment = (void **)bu_calloc(sketch_ip->curve.count, sizeof(void *), "sketch_ip->curve.segment");

    bn_mat_inv(invRot, gdpsp->gdps_rotation);
    VSET(view, 1.0, 0.0, 0.0);
    MAT4X3PNT(sketch_ip->u_vec, invRot, view);

    VSET(view, 0.0, 1.0, 0.0);
    MAT4X3PNT(sketch_ip->v_vec, invRot, view);

    /* should already be unit vectors */
    VUNITIZE(sketch_ip->u_vec);
    VUNITIZE(sketch_ip->v_vec);

    /* Project the origin onto the front of the viewing cube */
    MAT4X3PNT(vorigin, gdpsp->gdps_model2view, gdpsp->gdps_origin);
    vorigin[Z] = gdpsp->gdps_data_vZ;

    /* Convert back to model coordinates for storage */
    MAT4X3PNT(sketch_ip->V, gdpsp->gdps_view2model, vorigin);

    n = 0;
    for (j = 0; j < gdpsp->gdps_polygons.polygon[polygon_i].num_contours; ++j) {
	size_t cstart = n;

	for (k = 0; k < gdpsp->gdps_polygons.polygon[polygon_i].contour[j].num_points; ++k) {
	    point_t vpt;
	    vect_t vdiff;

	    MAT4X3PNT(vpt, gdpsp->gdps_model2view, gdpsp->gdps_polygons.polygon[polygon_i].contour[j].point[k]);
	    VSUB2(vdiff, vpt, vorigin);
	    VSCALE(vdiff, vdiff, gdpsp->gdps_scale);
	    V2MOVE(sketch_ip->verts[n], vdiff);

	    if (k) {
		BU_ALLOC(lsg, struct line_seg);
		sketch_ip->curve.segment[n-1] = (void *)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = n-1;
		lsg->end = n;
	    }

	    ++n;
	}

	if (k) {
	    BU_ALLOC(lsg, struct line_seg);
	    sketch_ip->curve.segment[n-1] = (void *)lsg;
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = n-1;
	    lsg->end = cstart;
	}
    }


    GED_DB_DIRADD(gedp, dp, sname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    return GED_OK;
}


struct segment_node {
    struct bu_list l;
    int reverse;
    int used;
    void *segment;
};


struct contour_node {
    struct bu_list l;
    struct bu_list head;
};

struct bg_polygon *
ged_import_polygon(struct ged *gedp, const char *sname)
{
    size_t j, n;
    size_t ncontours = 0;
    struct rt_db_internal intern;
    struct rt_sketch_internal *sketch_ip;
    struct bu_list HeadSegmentNodes;
    struct bu_list HeadContourNodes;
    struct segment_node *all_segment_nodes;
    struct segment_node *curr_snode;
    struct contour_node *curr_cnode;
    struct bg_polygon *gpp;

    if (wdb_import_from_path(gedp->ged_result_str, &intern, sname, gedp->ged_wdbp) & GED_ERROR)
	return (struct bg_polygon *)0;

    sketch_ip = (rt_sketch_internal *)intern.idb_ptr;
    if (sketch_ip->vert_count < 3 || sketch_ip->curve.count < 1) {
	rt_db_free_internal(&intern);
	return (struct bg_polygon *)0;
    }

    all_segment_nodes = (struct segment_node *)bu_calloc(sketch_ip->curve.count, sizeof(struct segment_node), "all_segment_nodes");

    BU_LIST_INIT(&HeadSegmentNodes);
    BU_LIST_INIT(&HeadContourNodes);
    for (n = 0; n < sketch_ip->curve.count; ++n) {
	all_segment_nodes[n].segment = sketch_ip->curve.segment[n];
	all_segment_nodes[n].reverse = sketch_ip->curve.reverse[n];
	BU_LIST_INSERT(&HeadSegmentNodes, &all_segment_nodes[n].l);
    }

    curr_cnode = (struct contour_node *)0;
    while (BU_LIST_NON_EMPTY(&HeadSegmentNodes)) {
	struct segment_node *unused_snode = BU_LIST_FIRST(segment_node, &HeadSegmentNodes);
	uint32_t *magic = (uint32_t *)unused_snode->segment;
	struct line_seg *unused_lsg;

	BU_LIST_DEQUEUE(&unused_snode->l);

	/* For the moment, skipping everything except line segments */
	if (*magic != CURVE_LSEG_MAGIC)
	    continue;

	unused_lsg = (struct line_seg *)unused_snode->segment;
	if (unused_snode->reverse) {
	    int tmp = unused_lsg->start;
	    unused_lsg->start = unused_lsg->end;
	    unused_lsg->end = tmp;
	}

	/* Find a contour to add the unused segment to. */
	for (BU_LIST_FOR(curr_cnode, contour_node, &HeadContourNodes)) {
	    for (BU_LIST_FOR(curr_snode, segment_node, &curr_cnode->head)) {
		struct line_seg *curr_lsg = (struct line_seg *)curr_snode->segment;

		if (unused_lsg->start == curr_lsg->end) {
		    unused_snode->used = 1;
		    BU_LIST_APPEND(&curr_snode->l, &unused_snode->l);
		    goto end;
		}

		if (unused_lsg->end == curr_lsg->start) {
		    unused_snode->used = 1;
		    BU_LIST_INSERT(&curr_snode->l, &unused_snode->l);
		    goto end;
		}
	    }
	}

    end:

	if (!unused_snode->used) {
	    ++ncontours;
	    BU_ALLOC(curr_cnode, struct contour_node);
	    BU_LIST_INSERT(&HeadContourNodes, &curr_cnode->l);
	    BU_LIST_INIT(&curr_cnode->head);
	    BU_LIST_INSERT(&curr_cnode->head, &unused_snode->l);
	}
    }

    BU_ALLOC(gpp, struct bg_polygon);
    gpp->num_contours = ncontours;
    gpp->hole = (int *)bu_calloc(ncontours, sizeof(int), "gp_hole");
    gpp->contour = (struct bg_poly_contour *)bu_calloc(ncontours, sizeof(struct bg_poly_contour), "gp_contour");

    j = 0;
    while (BU_LIST_NON_EMPTY(&HeadContourNodes)) {
	size_t k = 0;
	size_t npoints = 0;
	struct line_seg *curr_lsg = NULL;

	curr_cnode = BU_LIST_FIRST(contour_node, &HeadContourNodes);
	BU_LIST_DEQUEUE(&curr_cnode->l);

	/* Count the number of segments in this contour */
	for (BU_LIST_FOR(curr_snode, segment_node, &curr_cnode->head))
	    ++npoints;

	gpp->contour[j].num_points = npoints;
	gpp->contour[j].point = (point_t *)bu_calloc(npoints, sizeof(point_t), "gpc_point");

	while (BU_LIST_NON_EMPTY(&curr_cnode->head)) {
	    curr_snode = BU_LIST_FIRST(segment_node, &curr_cnode->head);
	    BU_LIST_DEQUEUE(&curr_snode->l);

	    curr_lsg = (struct line_seg *)curr_snode->segment;

	    /* Convert from UV space to model space */
	    VJOIN2(gpp->contour[j].point[k], sketch_ip->V,
		   sketch_ip->verts[curr_lsg->start][0], sketch_ip->u_vec,
		   sketch_ip->verts[curr_lsg->start][1], sketch_ip->v_vec);
	    ++k;
	}

	/* free contour node */
	bu_free((void *)curr_cnode, "curr_cnode");

	++j;
    }

    /* Clean up */
    bu_free((void *)all_segment_nodes, "all_segment_nodes");
    rt_db_free_internal(&intern);

    return gpp;
}

typedef struct {
    size_t	pc_num_points;
    point2d_t	*pc_point;
} poly_contour_2d;

typedef struct {
    size_t 		p_num_contours;
    int			*p_hole;
    poly_contour_2d	*p_contour;
} polygon_2d;


int
ged_polygons_overlap(struct ged *gedp, struct bg_polygon *polyA, struct bg_polygon *polyB)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);

    return bg_polygons_overlap(polyA, polyB, gedp->ged_gvp->gv_model2view, &gedp->ged_wdbp->wdb_tol, gedp->ged_gvp->gv_scale);
}

HIDDEN int
sort_by_X(const void *p1, const void *p2, void *UNUSED(arg))
{
    point2d_t *pt1 = (point2d_t *)p1;
    point2d_t *pt2 = (point2d_t *)p2;

    if (*pt1[X] < *pt2[X]) {
	return -1;
    }

    if (*pt1[X] > *pt2[X]) {
	return 1;
    }

    return 0;
}


void
ged_polygon_fill_segments(struct ged *gedp, struct bg_polygon *poly, vect2d_t vfilldir, fastf_t vfilldelta)
{
    size_t i, j;
    fastf_t vx, vy, vZ = 0.0;
    size_t begin, end;
    point2d_t pt_2d;
    point_t pt;
    point_t hit_pt;
    polygon_2d poly_2d;
    size_t ecount;
    size_t icount;
    size_t final_icount;
    struct bu_vls result_vls;
    point2d_t *isect2;
    point2d_t *final_isect2;
    int tweakCount;
    static size_t isectSize = 8;
    static int maxTweaks = 10;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (poly->num_contours < 1 || poly->contour[0].num_points < 3)
	return;

    if (vfilldelta < 0)
	vfilldelta = -vfilldelta;

    isect2 = (point2d_t *)bu_calloc(isectSize, sizeof(point2d_t), "isect2");
    final_isect2 = (point2d_t *)bu_calloc(isectSize, sizeof(point2d_t), "final_isect2");

    /* Project poly onto the view plane */
    poly_2d.p_num_contours = poly->num_contours;
    poly_2d.p_hole = (int *)bu_calloc(poly->num_contours, sizeof(int), "p_hole");
    poly_2d.p_contour = (poly_contour_2d *)bu_calloc(poly->num_contours, sizeof(poly_contour_2d), "p_contour");

    for (i = 0; i < poly->num_contours; ++i) {
	poly_2d.p_hole[i] = poly->hole[i];
	poly_2d.p_contour[i].pc_num_points = poly->contour[i].num_points;
	poly_2d.p_contour[i].pc_point = (point2d_t *)bu_calloc(poly->contour[i].num_points, sizeof(point2d_t), "pc_point");

	for (j = 0; j < poly->contour[i].num_points; ++j) {
	    point_t vpoint;

	    MAT4X3PNT(vpoint, gedp->ged_gvp->gv_model2view, poly->contour[i].point[j]);
	    V2MOVE(poly_2d.p_contour[i].pc_point[j], vpoint);
	    vZ = vpoint[Z];
	}
    }

    V2UNITIZE(vfilldir);
    bu_vls_init(&result_vls);

    /* Intersect diagonal view lines with the 2d polygon */
    pt_2d[X] = -1.0;
    for (vy = 1.0 - vfilldelta; vy > -1.0; vy -= vfilldelta) {
	tweakCount = 0;
	pt_2d[Y] = vy;

    try_y_tweak:
        ecount = 0;
        icount = 0;
	for (i = 0; i < poly_2d.p_num_contours; ++i) {
	    for (begin = 0; begin < poly_2d.p_contour[i].pc_num_points; ++begin) {
		vect2d_t distvec;
		vect2d_t pdir;
		int ret;

		/* wrap around */
		if (begin == poly_2d.p_contour[i].pc_num_points-1)
		    end = 0;
		else
		    end = begin + 1;

		V2SUB2(pdir, poly_2d.p_contour[i].pc_point[end], poly_2d.p_contour[i].pc_point[begin]);

		if ((ret = bg_isect_line2_lseg2(distvec,
						pt_2d, vfilldir,
						poly_2d.p_contour[i].pc_point[begin], pdir,
						&gedp->ged_wdbp->wdb_tol)) >= 0) {
		    /* We have an intersection */
		    V2JOIN1(pt, poly_2d.p_contour[i].pc_point[begin], distvec[1], pdir);

		    if (ret == 1 || ret == 2) {
			++ecount;

			if (ecount > 1 && tweakCount < maxTweaks) {
			    ++tweakCount;
			    pt_2d[Y] = pt_2d[Y] - vfilldelta*0.01;
			    goto try_y_tweak;
			}
		    }

		    if (icount >= isectSize) {
			isectSize *= 2;
			isect2 = (point2d_t *)bu_realloc((void *)isect2, isectSize*sizeof(point2d_t), "isect2");
			final_isect2 = (point2d_t *)bu_realloc((void *)final_isect2, isectSize*sizeof(point2d_t), "final_isect2");
		    }

		    V2MOVE(isect2[icount], pt);
		    ++icount;
		}
	    }
	}

	if (icount) {
	    final_icount = icount;

	    bu_sort(isect2, icount, sizeof(point2d_t), sort_by_X, NULL);
	    V2MOVE(final_isect2[0], isect2[0]);

	    /* remove duplicates */
	    j = 1;
	    for (i = 1; i < icount; ++i) {
		if (V2NEAR_EQUAL(final_isect2[j-1], isect2[i], SMALL_FASTF)) {
		    --final_icount;
		    continue;
		}

		V2MOVE(final_isect2[j], isect2[i]);
		++j;
	    }

	    if (!(final_icount%2)) {
		for (i = 0; i < final_icount; ++i) {
		    V2MOVE(pt, final_isect2[i]);
		    pt[Z] = vZ;
		    MAT4X3PNT(hit_pt, gedp->ged_gvp->gv_view2model, pt);
		    bu_vls_printf(&result_vls, "{%lf %lf %lf} ", V3ARGS(hit_pt));
		}
	    } else if (tweakCount < maxTweaks) {
		++tweakCount;
		pt_2d[Y] = pt_2d[Y] - vfilldelta*0.01;
		goto try_y_tweak;
	    }
	}
    }

    if (vfilldir[X]*vfilldir[Y] < 0)
	pt_2d[Y] = 1.0;
    else
	pt_2d[Y] = -1.0;

    for (vx = -1.0; vx < 1.0; vx += vfilldelta) {
	tweakCount = 0;
	pt_2d[X] = vx;

    try_x_tweak:
        ecount = 0;
        icount = 0;
	for (i = 0; i < poly_2d.p_num_contours; ++i) {
	    for (begin = 0; begin < poly_2d.p_contour[i].pc_num_points; ++begin) {
		vect2d_t distvec;
		vect2d_t pdir;
		int ret;

		/* wrap around */
		if (begin == poly_2d.p_contour[i].pc_num_points-1)
		    end = 0;
		else
		    end = begin + 1;


		V2SUB2(pdir, poly_2d.p_contour[i].pc_point[end], poly_2d.p_contour[i].pc_point[begin]);

		if ((ret = bg_isect_line2_lseg2(distvec,
						pt_2d, vfilldir,
						poly_2d.p_contour[i].pc_point[begin], pdir,
						&gedp->ged_wdbp->wdb_tol)) >= 0) {
		    /* We have an intersection */
		    V2JOIN1(pt, poly_2d.p_contour[i].pc_point[begin], distvec[1], pdir);

		    if (ret == 1 || ret == 2) {
			++ecount;

			if (ecount > 1 && tweakCount < maxTweaks) {
			    ++tweakCount;
			    pt_2d[X] = pt_2d[X] + vfilldelta*0.01;
			    goto try_x_tweak;
			}
		    }

		    if (icount >= isectSize) {
			isectSize *= 2;
			isect2 = (point2d_t *)bu_realloc((void *)isect2, isectSize*sizeof(point2d_t), "isect2");
			final_isect2 = (point2d_t *)bu_realloc((void *)final_isect2, isectSize*sizeof(point2d_t), "final_isect2");
		    }

		    V2MOVE(isect2[icount], pt);
		    ++icount;
		}
	    }
	}

	if (icount) {
	    final_icount = icount;

	    bu_sort(isect2, icount, sizeof(point2d_t), sort_by_X, NULL);
	    V2MOVE(final_isect2[0], isect2[0]);

	    /* remove duplicates */
	    j = 1;
	    for (i = 1; i < icount; ++i) {
		if (V2NEAR_EQUAL(final_isect2[j-1], isect2[i], SMALL_FASTF)) {
		    --final_icount;
		    continue;
		}

		V2MOVE(final_isect2[j], isect2[i]);
		++j;
	    }

	    if (!(final_icount%2)) {
		for (i = 0; i < final_icount; ++i) {
		    V2MOVE(pt, final_isect2[i]);
		    pt[Z] = vZ;
		    MAT4X3PNT(hit_pt, gedp->ged_gvp->gv_view2model, pt);
		    bu_vls_printf(&result_vls, "{%lf %lf %lf} ", V3ARGS(hit_pt));
		}
	    } else if (tweakCount < maxTweaks) {
		++tweakCount;
		pt_2d[X] = pt_2d[X] + vfilldelta*0.01;
		goto try_x_tweak;
	    }
	}
    }

    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&result_vls));
    bu_free((void *)isect2, "isect2");
    bu_free((void *)final_isect2, "final_isect2");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

