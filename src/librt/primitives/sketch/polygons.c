/*                      P O L Y G O N S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file polygons.c
 *
 * Sketch routines for view polygons.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/avs.h"
#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bv.h"
#include "bg/polygon.h"
#include "rt/defines.h"
#include "rt/directory.h"
#include "rt/db_attr.h"
#include "rt/db_internal.h"
#include "rt/db5.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/primitives/sketch.h"

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

struct bv_scene_obj *
db_sketch_to_scene_obj(const char *sname, struct db_i *dbip, struct directory *dp, struct bview *sv, int flags)
{
    if (!sv)
	return NULL;

    // Begin import
    size_t ncontours = 0;
    struct bu_list HeadSegmentNodes;
    struct bu_list HeadContourNodes;
    struct segment_node *all_segment_nodes;
    struct segment_node *curr_snode;
    struct contour_node *curr_cnode;

    struct rt_db_internal intern;
    struct rt_sketch_internal *sketch_ip;
    mat_t mat;
    MAT_IDN(mat);
    if (rt_db_get_internal(&intern, dp, dbip, mat, &rt_uniresource) < 0) {
	return NULL;
    }
    sketch_ip = (struct rt_sketch_internal *)intern.idb_ptr;
    RT_SKETCH_CK_MAGIC(sketch_ip);

    if (sketch_ip->vert_count < 3 || sketch_ip->curve.count < 1) {
	rt_db_free_internal(&intern);
	return NULL;
    }

    // Have a sketch - create an empty polygon
    struct bv_polygon *p;
    BU_GET(p, struct bv_polygon);

    /* Start translating the sketch info into a polygon */
    all_segment_nodes = (struct segment_node *)bu_calloc(sketch_ip->curve.count, sizeof(struct segment_node), "all_segment_nodes");

    BU_LIST_INIT(&HeadSegmentNodes);
    BU_LIST_INIT(&HeadContourNodes);
    for (size_t n = 0; n < sketch_ip->curve.count; ++n) {
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

    p->polygon.num_contours = ncontours;
    p->polygon.hole = (int *)bu_calloc(ncontours, sizeof(int), "gp_hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(ncontours, sizeof(struct bg_poly_contour), "gp_contour");

    size_t j = 0;
    fastf_t dmax = 0.0;
    while (BU_LIST_NON_EMPTY(&HeadContourNodes)) {
	size_t k = 0;
	size_t npoints = 0;
	struct line_seg *curr_lsg = NULL;

	curr_cnode = BU_LIST_FIRST(contour_node, &HeadContourNodes);
	BU_LIST_DEQUEUE(&curr_cnode->l);

	/* Count the number of segments in this contour */
	for (BU_LIST_FOR(curr_snode, segment_node, &curr_cnode->head))
	    ++npoints;

	p->polygon.contour[j].num_points = npoints;
	p->polygon.contour[j].open = 0;
	p->polygon.contour[j].point = (point_t *)bu_calloc(npoints, sizeof(point_t), "gpc_point");

	while (BU_LIST_NON_EMPTY(&curr_cnode->head)) {
	    curr_snode = BU_LIST_FIRST(segment_node, &curr_cnode->head);
	    BU_LIST_DEQUEUE(&curr_snode->l);

	    curr_lsg = (struct line_seg *)curr_snode->segment;

	    /* Convert from UV space to model space */
	    VJOIN2(p->polygon.contour[j].point[k], sketch_ip->V,
		    sketch_ip->verts[curr_lsg->start][0], sketch_ip->u_vec,
		    sketch_ip->verts[curr_lsg->start][1], sketch_ip->v_vec);
	    fastf_t dtmp = DIST_PNT_PNT(sketch_ip->V, p->polygon.contour[j].point[k]);
	    if (dtmp > dmax)
		dmax = dtmp;
	    ++k;
	}

	/* free contour node */
	bu_free((void *)curr_cnode, "curr_cnode");

	++j;
    }

    /* Unlike interactive sketch creation, the plane of an imported sketch comes from the
     * sketch parameters. */
    vect_t pn;
    VCROSS(pn, sketch_ip->u_vec, sketch_ip->v_vec);
    bg_plane_pt_nrml(&p->vp, sketch_ip->V, pn);

    /* Clean up */
    bu_free((void *)all_segment_nodes, "all_segment_nodes");

    /* Create the scene object here so we can read a default color */
    struct bv_scene_obj *s = bv_create_polygon_obj(sv, flags, p);
    if (!s) {
	bg_polygon_free(&p->polygon);
	BU_PUT(p, struct bv_polygon);
	return NULL;
    }
    bu_vls_init(&s->s_name);
    bu_vls_printf(&s->s_name, "%s", sname);

    // check attributes for visual properties
    int have_view = 1;
    struct bu_attribute_value_set lavs;
    bu_avs_init_empty(&lavs);
    if (!db5_get_attributes(dbip, &lavs, dp)) {
	const char *val = NULL;
	// Check for various polygon properties
	val = bu_avs_get(&lavs, "POLYGON_EDGE_COLOR");
	if (val) {
	    struct bu_color bc;
	    if (bu_opt_color(NULL, 1, (const char **)&val, (void *)&bc) == 1) {
		bu_color_to_rgb_chars(&bc, s->s_color);
	    }
	}
	val = bu_avs_get(&lavs, "POLYGON_FILL_COLOR");
	if (val) {
	    bu_opt_color(NULL, 1, (const char **)&val, (void *)&p->fill_color);
	}
	val = bu_avs_get(&lavs, "POLYGON_FILL");
	if (val && BU_STR_EQUAL(val, "1")) {
	    p->fill_flag = 1;
	}
	val = bu_avs_get(&lavs, "POLYGON_FILL_SLOPE_X");
	if (val) {
	    bu_opt_fastf_t(NULL, 1, (const char **)&val, (void *)&p->fill_dir[0]);
	}
	val = bu_avs_get(&lavs, "POLYGON_FILL_SLOPE_Y");
	if (val) {
	    bu_opt_fastf_t(NULL, 1, (const char **)&val, (void *)&p->fill_dir[1]);
	}
	val = bu_avs_get(&lavs, "POLYGON_FILL_DELTA");
	if (val) {
	    bu_opt_fastf_t(NULL, 1, (const char **)&val, (void *)&p->fill_delta);
	}
	val = bu_avs_get(&lavs, "POLYGON_TYPE");
	if (BU_STR_EQUAL(val, "CIRCLE")) {
	    p->type = BV_POLYGON_CIRCLE;
	}
	if (BU_STR_EQUAL(val, "ELLIPSE")) {
	    p->type = BV_POLYGON_ELLIPSE;
	}
	if (BU_STR_EQUAL(val, "RECTANGLE")) {
	    p->type = BV_POLYGON_RECTANGLE;
	}
	if (BU_STR_EQUAL(val, "SQUARE")) {
	    p->type = BV_POLYGON_SQUARE;
	}
	if (BU_STR_EQUAL(val, "GENERAL")) {
	    p->type = BV_POLYGON_GENERAL;
	}

	// See if we have a stored view
	if (have_view) {
	    val = bu_avs_get(&lavs, "VIEWSCALE");
	    if (val) {
		bu_opt_fastf_t(NULL, 1, (const char **)&val, (void *)&sv->gv_scale);
	    } else {
		have_view = 0;
	    }
	}
	if (have_view) {
	    val = bu_avs_get(&lavs, "ROTATION");
	    if (val) {
		quat_t quat;
		char *av[5] = {NULL};
		char *lp = bu_strdup(val);
		if (bu_argv_from_string(av, 4, lp) != 4) {
		    have_view = 0;
		} else {
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[0], (void *)&quat[0]);
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[1], (void *)&quat[1]);
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[2], (void *)&quat[2]);
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[3], (void *)&quat[3]);
		    quat_quat2mat(sv->gv_rotation, quat);
		}
		bu_free(lp, "val cpy");
	    } else {
		have_view = 0;
	    }
	}
	if (have_view) {
	    val = bu_avs_get(&lavs, "CENTER");
	    if (val) {
		quat_t quat;
		char *av[5] = {NULL};
		char *lp = bu_strdup(val);
		if (bu_argv_from_string(av, 4, lp) != 4) {
		    have_view = 0;
		} else {
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[0], (void *)&quat[0]);
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[1], (void *)&quat[1]);
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[2], (void *)&quat[2]);
		    bu_opt_fastf_t(NULL, 1, (const char **)&av[3], (void *)&quat[3]);
		    quat_quat2mat(sv->gv_center, quat);
		}
		bu_free(lp, "val cpy");
	    } else {
		have_view = 0;
	    }
	}
    }
    bu_avs_free(&lavs);

    /* TODO - if we didn't have a saved version, construct an appropriate plane from the
     * sketch's 3D info so we can snap to it. */
    if (!have_view) {
    }

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    rt_db_free_internal(&intern);
    return s;
}

struct directory *
db_scene_obj_to_sketch(struct db_i *dbip, const char *sname, struct bv_scene_obj *s)
{
    // Make sure we have a view polygon
    if (!(s->s_type_flags & BV_VIEWONLY) || !(s->s_type_flags & BV_POLYGONS)) {
	return NULL;
    }

    if (db_lookup(dbip, sname, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_log("Object %s already exists\n", sname);
	return NULL;
    }

    if (!s->s_v)
	return NULL;

    size_t num_verts = 0;
    struct rt_db_internal internal;
    struct rt_sketch_internal *sketch_ip;
    struct line_seg *lsg;

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    for (size_t j = 0; j < p->polygon.num_contours; ++j)
	num_verts += p->polygon.contour[j].num_points;

    if (num_verts < 3) {
	return NULL;
    }

    RT_DB_INTERNAL_INIT(&internal);
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


    bg_plane_pt_at(&sketch_ip->u_vec, p->vp, 0, 1);
    bg_plane_pt_at(&sketch_ip->v_vec, p->vp, 1, 0);

    /* should already be unit vectors */
    VUNITIZE(sketch_ip->u_vec);
    VUNITIZE(sketch_ip->v_vec);

    /* Plane origin is sketch origin */
    bg_plane_pt_at(&sketch_ip->V, p->vp, 0, 0);

    int n = 0;
    for (size_t j = 0; j < p->polygon.num_contours; ++j) {
	size_t cstart = n;
	size_t k = 0;
	for (k = 0; k < p->polygon.contour[j].num_points; ++k) {
	    bg_plane_closest_pt(&sketch_ip->verts[n][1], &sketch_ip->verts[n][0], p->vp, p->polygon.contour[j].point[k]);

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



    struct directory *dp = db_diradd(dbip, sname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type);
    if (dp == RT_DIR_NULL)
	return NULL;

    if (rt_db_put_internal(dp, dbip, &internal, &rt_uniresource) < 0) {
	return NULL;
    }

    // write attributes to save visual properties

    struct bu_attribute_value_set lavs;
    bu_avs_init_empty(&lavs);
    if (!db5_get_attributes(dbip, &lavs, dp)) {
	struct bu_vls val = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&val, "%d/%d/%d", s->s_color[0], s->s_color[1], s->s_color[2]);
	bu_avs_add(&lavs, "POLYGON_EDGE_COLOR", bu_vls_cstr(&val));
	unsigned char rgb[3];
	bu_color_to_rgb_chars(&p->fill_color, rgb);
	bu_vls_sprintf(&val, "%d/%d/%d", rgb[0], rgb[1], rgb[2]);
	bu_avs_add(&lavs, "POLYGON_FILL_COLOR", bu_vls_cstr(&val));
	bu_vls_sprintf(&val, "%d", p->fill_flag);
	bu_avs_add(&lavs, "POLYGON_FILL", bu_vls_cstr(&val));
	bu_vls_sprintf(&val, "%g", p->fill_dir[0]);
	bu_avs_add(&lavs, "POLYGON_FILL_SLOPE_X", bu_vls_cstr(&val));
	bu_vls_sprintf(&val, "%g", p->fill_dir[1]);
	bu_avs_add(&lavs, "POLYGON_FILL_SLOPE_Y", bu_vls_cstr(&val));
	bu_vls_sprintf(&val, "%g", p->fill_delta);
	bu_avs_add(&lavs, "POLYGON_FILL_DELTA", bu_vls_cstr(&val));
	switch (p->type) {
	    case BV_POLYGON_CIRCLE:
		bu_vls_sprintf(&val, "CIRCLE");
		break;
	    case BV_POLYGON_ELLIPSE:
		bu_vls_sprintf(&val, "ELLIPSE");
		break;
	    case BV_POLYGON_RECTANGLE:
		bu_vls_sprintf(&val, "RECTANGLE");
		break;
	    case BV_POLYGON_SQUARE:
		bu_vls_sprintf(&val, "SQUARE");
		break;
	    default:
		bu_vls_sprintf(&val, "GENERAL");
		break;
	}
	bu_avs_add(&lavs, "POLYGON_TYPE", bu_vls_cstr(&val));
	// Save view
	bu_vls_sprintf(&val, "%.15e", s->s_v->gv_scale);
	bu_avs_add(&lavs, "VIEWSCALE", bu_vls_cstr(&val));
	quat_t rquat;
	quat_mat2quat(rquat, s->s_v->gv_rotation);
	bu_vls_sprintf(&val, "%.15e %.15e %.15e %.15e", V4ARGS(rquat));
	bu_avs_add(&lavs, "ROTATION", bu_vls_cstr(&val));
	quat_t cquat;
	quat_mat2quat(cquat, s->s_v->gv_center);
	bu_vls_sprintf(&val, "%.15e %.15e %.15e %.15e", V4ARGS(cquat));
	bu_avs_add(&lavs, "CENTER", bu_vls_cstr(&val));
    }
    db5_update_attributes(dp, &lavs, dbip);
    bu_avs_free(&lavs);

    return dp;
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
