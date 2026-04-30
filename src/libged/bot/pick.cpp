/*                        P I C K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file libged/bot/pick.cpp
 *
 * Visual selection of individual subcomponents of BoT objects.
 *
 */

#include "common.h"

#include <cfloat>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "RTree.h"
#include "bu/cmd.h"
#include "bg/plane.h"
#include "bg/tri_ray.h"
#include "rt/geom.h"

#include "./ged_bot.h"

struct _ged_bot_ipick {
    struct _ged_bot_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

static int
_bot_pick_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_bot_ipick *gib = (struct _ged_bot_ipick *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gib->vls, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gib->vls, "%s\n", ps);
	return 1;
    }
    return 0;
}

/* Compute the axis-aligned bounding box diagonal length of a BOT mesh */
static double
_bot_bbox_diag(const struct rt_bot_internal *bot)
{
    if (!bot->num_vertices)
	return 1.0;

    point_t bmin, bmax;
    VSETALL(bmin, DBL_MAX);
    VSETALL(bmax, -DBL_MAX);

    for (size_t i = 0; i < bot->num_vertices; i++) {
	double x = bot->vertices[3*i+0];
	double y = bot->vertices[3*i+1];
	double z = bot->vertices[3*i+2];
	if (x < bmin[X]) bmin[X] = x;
	if (y < bmin[Y]) bmin[Y] = y;
	if (z < bmin[Z]) bmin[Z] = z;
	if (x > bmax[X]) bmax[X] = x;
	if (y > bmax[Y]) bmax[Y] = y;
	if (z > bmax[Z]) bmax[Z] = z;
    }

    vect_t diag;
    VSUB2(diag, bmax, bmin);
    double d = MAGNITUDE(diag);
    return (d < SMALL_FASTF) ? 1.0 : d;
}

/* Set up ray from GED viewport or from explicit 6-parameter specification.
 * When using the viewport ray, origin is backed up by diag_len along -dir
 * so the starting point is guaranteed to be outside the mesh bounding box. */
static int
_bot_pick_ray(struct _ged_bot_ipick *gib, int argc, const char **argv,
	      point_t origin, vect_t dir, double diag_len)
{
    struct ged *gedp = gib->gb->gedp;

    if (argc == 6) {
	/* Explicit ray specification: px py pz dx dy dz */
	char *endp;
	for (int i = 0; i < 3; i++) {
	    origin[i] = strtod(argv[i], &endp);
	    if (endp == argv[i]) {
		bu_vls_printf(gib->vls, "invalid ray value: %s\n", argv[i]);
		return BRLCAD_ERROR;
	    }
	}
	for (int i = 0; i < 3; i++) {
	    dir[i] = strtod(argv[3+i], &endp);
	    if (endp == argv[3+i]) {
		bu_vls_printf(gib->vls, "invalid ray value: %s\n", argv[3+i]);
		return BRLCAD_ERROR;
	    }
	}
	VUNITIZE(dir);
    } else {
	/* Get ray from GED viewport */
	if (!gedp->ged_gvp) {
	    bu_vls_printf(gib->vls, "no viewport available and no ray specified\n");
	    return BRLCAD_ERROR;
	}
	VSET(origin,
	    -gedp->ged_gvp->gv_center[MDX],
	    -gedp->ged_gvp->gv_center[MDY],
	    -gedp->ged_gvp->gv_center[MDZ]);
	VSCALE(origin, origin, gedp->dbip->dbi_base2local);
	VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
	VSCALE(dir, dir, -1.0);
	/* Back origin outside the shape using bbox diagonal */
	for (int i = 0; i < 3; i++) {
	    origin[i] = origin[i] + diag_len * -1.0 * dir[i];
	}
    }

    return BRLCAD_OK;
}


/* V - 3D vertices */
extern "C" int
_bot_cmd_vertex_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> pick V [--first] [px py pz dx dy dz]";
    const char *purpose_string = "pick closest vertex to line";
    if (_bot_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_ipick *gib = (struct _ged_bot_ipick *)bs;
    const struct rt_bot_internal *bot =
	(const struct rt_bot_internal *)(gib->gb->intern->idb_ptr);

    /* --first: among vertices near the ray, pick the one closest to the
     * interrogation start point rather than closest to the ray line. */
    int use_first = 0;
    if (argc > 0 && BU_STR_EQUAL(argv[0], "--first")) {
	use_first = 1;
	argc--; argv++;
    }

    if (argc > 0 && argc != 6) {
	bu_vls_printf(gib->vls,
		      "need six values for point and direction, or no values to use viewport\n");
	return BRLCAD_ERROR;
    }

    if (!bot->num_vertices) {
	bu_vls_printf(gib->vls, "BoT has no vertices\n");
	return BRLCAD_OK;
    }

    double diag_len = _bot_bbox_diag(bot);

    point_t origin;
    vect_t dir;
    if (_bot_pick_ray(gib, argc, argv, origin, dir, diag_len) != BRLCAD_OK)
	return BRLCAD_ERROR;

    /* Find the vertex with the smallest perpendicular distance to the ray
     * across all vertices in the mesh.  In wireframe views (the normal case)
     * any vertex in the mesh can be visible regardless of face occlusion, so
     * we must consider all vertices rather than limiting the search to faces
     * struck by the ray.  The perpendicular distance from a vertex to the ray
     * line is exactly the 2D screen-space distance to the mouse click point,
     * making this the correct selection metric. */
    struct bn_tol tol = BN_TOL_INIT_TOL;
    double dmin = DBL_MAX;
    int cvert = -1;
    double reported_dist;

    if (!use_first) {
	for (size_t i = 0; i < bot->num_vertices; i++) {
	    point_t vp;
	    VMOVE(vp, &bot->vertices[3*i]);

	    fastf_t vdist;
	    point_t pca;
	    bg_dist_pnt3_line3(&vdist, pca, origin, vp, dir, &tol);
	    if (vdist < dmin) {
		dmin = vdist;
		cvert = (int)i;
	    }
	}
	reported_dist = dmin;
    } else {
	/* Pass 1: find the minimum perpendicular distance to the ray. */
	for (size_t i = 0; i < bot->num_vertices; i++) {
	    point_t vp;
	    VMOVE(vp, &bot->vertices[3*i]);

	    fastf_t vdist;
	    point_t pca;
	    bg_dist_pnt3_line3(&vdist, pca, origin, vp, dir, &tol);
	    if (vdist < dmin)
		dmin = vdist;
	}
	/* Pass 2: among vertices within tolerance of dmin, select the one
	 * with the smallest distance from the interrogation start point. */
	double near_thresh = dmin + tol.dist;
	double tmin = DBL_MAX;
	for (size_t i = 0; i < bot->num_vertices; i++) {
	    point_t vp;
	    VMOVE(vp, &bot->vertices[3*i]);

	    fastf_t vdist;
	    point_t pca;
	    bg_dist_pnt3_line3(&vdist, pca, origin, vp, dir, &tol);
	    if (vdist <= near_thresh) {
		vect_t diff;
		VSUB2(diff, vp, origin);
		double t = VDOT(diff, dir);
		if (t < tmin) {
		    tmin = t;
		    cvert = (int)i;
		}
	    }
	}
	reported_dist = tmin;
    }

    if (cvert < 0) {
	bu_vls_printf(gib->vls, "no vertex found\n");
	return BRLCAD_OK;
    }

    if (gib->gb->verbosity) {
	bu_vls_printf(gib->vls, "V[%d]: %g\n", cvert, reported_dist);
    } else {
	bu_vls_printf(gib->vls, "%d\n", cvert);
    }

    return BRLCAD_OK;
}


/* E - edges
 * A BoT mesh has no explicit edge table; edges are derived from the triangle
 * face array.  Each unique sorted vertex-index pair (lo, hi) defines one
 * edge.  The pick command returns the vertex-index pair of the closest edge.
 */
extern "C" int
_bot_cmd_edge_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> pick E [--first] [px py pz dx dy dz]";
    const char *purpose_string = "pick closest edge to line";
    if (_bot_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_ipick *gib = (struct _ged_bot_ipick *)bs;
    const struct rt_bot_internal *bot =
	(const struct rt_bot_internal *)(gib->gb->intern->idb_ptr);

    /* --first: among edges near the ray, pick the one whose closest approach
     * point is nearest to the interrogation start point. */
    int use_first = 0;
    if (argc > 0 && BU_STR_EQUAL(argv[0], "--first")) {
	use_first = 1;
	argc--; argv++;
    }

    if (argc > 0 && argc != 6) {
	bu_vls_printf(gib->vls,
		      "need six values for point and direction, or no values to use viewport\n");
	return BRLCAD_ERROR;
    }

    if (!bot->num_faces) {
	bu_vls_printf(gib->vls, "BoT has no faces\n");
	return BRLCAD_OK;
    }

    double diag_len = _bot_bbox_diag(bot);

    point_t origin;
    vect_t dir;
    if (_bot_pick_ray(gib, argc, argv, origin, dir, diag_len) != BRLCAD_OK)
	return BRLCAD_ERROR;

    /* Build sorted list of unique edges from the face array */
    std::map<std::pair<int,int>, int> edge_map;
    std::vector<std::pair<int,int> > edges;

    for (size_t fi = 0; fi < bot->num_faces; fi++) {
	int v[3];
	v[0] = bot->faces[3*fi+0];
	v[1] = bot->faces[3*fi+1];
	v[2] = bot->faces[3*fi+2];
	for (int k = 0; k < 3; k++) {
	    int ea = v[k];
	    int eb = v[(k+1)%3];
	    std::pair<int,int> e(std::min(ea, eb), std::max(ea, eb));
	    if (edge_map.find(e) == edge_map.end()) {
		edge_map[e] = (int)edges.size();
		edges.push_back(e);
	    }
	}
    }

    /* Build RTree with edge bounding boxes inflated by a uniform fraction of
     * the mesh bounding-box diagonal.  In wireframe views (the norm) any edge
     * at any depth in the mesh can be the intended target, so the inflation
     * must be large enough that the ray query returns all edges that could
     * plausibly be the closest one in screen space.  Using a global value
     * (rather than per-edge length) gives consistent behaviour regardless of
     * local edge density. */
    const double inflate = diag_len * 0.1;

    RTree<int, double, 3> edge_bboxes;
    for (size_t ei = 0; ei < edges.size(); ei++) {
	const double *va = &bot->vertices[3*edges[ei].first];
	const double *vb = &bot->vertices[3*edges[ei].second];

	double p1[3], p2[3];
	for (int k = 0; k < 3; k++) {
	    p1[k] = std::min(va[k], vb[k]) - inflate;
	    p2[k] = std::max(va[k], vb[k]) + inflate;
	}

	edge_bboxes.Insert(p1, p2, (int)ei);
    }

    RTree<int, double, 3>::Ray ray;
    for (int i = 0; i < 3; i++) {
	ray.o[i] = origin[i];
	ray.d[i] = dir[i];
	ray.di[i] = 1.0 / ray.d[i];
    }

    std::set<int> aedges;
    edge_bboxes.Intersects(&ray, &aedges);

    /* If the inflated-bbox ray query missed everything (can happen when the
     * ray passes through a very sparse region of the mesh), fall back to
     * testing every edge so the closest is never missed. */
    bool use_all = aedges.empty();
    if (use_all) {
	for (size_t ei = 0; ei < edges.size(); ei++)
	    aedges.insert((int)ei);
    }

    /* Find closest edge to the ray */
    struct bn_tol tol = BN_TOL_INIT_TOL;
    double dmin = DBL_MAX;
    int cedge = -1;

    /* Lambda: compute 3D distance between the ray and edge ei, and return
     * the parametric distance along the ray (ray_t) to the closest approach
     * point.  Used by both the standard pass and the --first second pass. */
    auto edge_dist_and_t = [&](int ei, double &edge_dist, double &ray_t) {
	point_t va, vb;
	VMOVE(va, &bot->vertices[3*edges[ei].first]);
	VMOVE(vb, &bot->vertices[3*edges[ei].second]);

	fastf_t dist[2];
	int ret = bg_dist_line3_lseg3(dist, origin, dir, va, vb, &tol);

	if (ret < -1) {
	    /* Parallel and collinear: distance is zero along the line.
	     * Use distance from segment endpoint to the ray as a proxy.
	     * For depth, project the midpoint of the segment onto the ray. */
	    fastf_t d0;
	    point_t pca;
	    bg_dist_pnt3_line3(&d0, pca, origin, va, dir, &tol);
	    edge_dist = (double)d0;
	    vect_t mid, diff;
	    VADD2SCALE(mid, va, vb, 0.5);
	    VSUB2(diff, mid, origin);
	    ray_t = VDOT(diff, dir);
	} else {
	    /* Compute actual 3D distance between closest points on ray and
	     * segment.  dist[0] is the parametric distance along the ray;
	     * dist[1] is the parametric ratio along the segment [0,1]. */
	    point_t closest_ray;
	    VJOIN1(closest_ray, origin, dist[0], dir);

	    double seg_t = (double)dist[1];
	    if (seg_t < 0.0) seg_t = 0.0;
	    if (seg_t > 1.0) seg_t = 1.0;

	    vect_t edge_vec;
	    VSUB2(edge_vec, vb, va);
	    point_t closest_seg;
	    VJOIN1(closest_seg, va, seg_t, edge_vec);

	    vect_t diff;
	    VSUB2(diff, closest_ray, closest_seg);
	    edge_dist = MAGNITUDE(diff);
	    ray_t = (double)dist[0];
	}
    };

    if (!use_first) {
	/* Standard: find edge with minimum 3D distance to the ray. */
	for (std::set<int>::iterator it = aedges.begin(); it != aedges.end(); it++) {
	    int ei = *it;
	    double edge_dist, ray_t;
	    edge_dist_and_t(ei, edge_dist, ray_t);

	    if (edge_dist < dmin) {
		dmin = edge_dist;
		cedge = ei;
	    }
	}
    } else {
	/* --first: among edges near the ray, pick the one whose closest
	 * approach point on the ray is nearest to the start point.
	 * Pass 1: find the minimum 3D distance to the ray. */
	for (std::set<int>::iterator it = aedges.begin(); it != aedges.end(); it++) {
	    int ei = *it;
	    double edge_dist, ray_t;
	    edge_dist_and_t(ei, edge_dist, ray_t);
	    if (edge_dist < dmin)
		dmin = edge_dist;
	}
	/* Pass 2: among edges within tolerance of dmin, select the one
	 * whose closest approach on the ray is nearest to origin. */
	double near_thresh = dmin + tol.dist;
	double tmin = DBL_MAX;
	for (std::set<int>::iterator it = aedges.begin(); it != aedges.end(); it++) {
	    int ei = *it;
	    double edge_dist, ray_t;
	    edge_dist_and_t(ei, edge_dist, ray_t);
	    if (edge_dist <= near_thresh && ray_t < tmin) {
		tmin = ray_t;
		cedge = ei;
	    }
	}
    }

    if (cedge < 0) {
	bu_vls_printf(gib->vls, "no edge found\n");
	return BRLCAD_OK;
    }

    if (gib->gb->verbosity) {
	bu_vls_printf(gib->vls, "E[%d %d]: %g\n",
		      edges[cedge].first, edges[cedge].second, dmin);
    } else {
	bu_vls_printf(gib->vls, "%d %d\n",
		      edges[cedge].first, edges[cedge].second);
    }

    return BRLCAD_OK;
}


/* F - triangle faces */
extern "C" int
_bot_cmd_face_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> pick F [px py pz dx dy dz]";
    const char *purpose_string = "pick closest face to line";
    if (_bot_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_ipick *gib = (struct _ged_bot_ipick *)bs;
    const struct rt_bot_internal *bot =
	(const struct rt_bot_internal *)(gib->gb->intern->idb_ptr);

    if (argc > 0 && argc != 6) {
	bu_vls_printf(gib->vls,
		      "need six values for point and direction, or no values to use viewport\n");
	return BRLCAD_ERROR;
    }

    if (!bot->num_faces) {
	bu_vls_printf(gib->vls, "BoT has no faces\n");
	return BRLCAD_OK;
    }

    double diag_len = _bot_bbox_diag(bot);

    point_t origin;
    vect_t dir;
    if (_bot_pick_ray(gib, argc, argv, origin, dir, diag_len) != BRLCAD_OK)
	return BRLCAD_ERROR;

    /* Build RTree with inflated triangle bounding boxes */
    RTree<int, double, 3> face_bboxes;

    for (size_t fi = 0; fi < bot->num_faces; fi++) {
	int vi0 = bot->faces[3*fi+0];
	int vi1 = bot->faces[3*fi+1];
	int vi2 = bot->faces[3*fi+2];

	const double *v0 = &bot->vertices[3*vi0];
	const double *v1 = &bot->vertices[3*vi1];
	const double *v2 = &bot->vertices[3*vi2];

	double p1[3], p2[3];
	for (int k = 0; k < 3; k++) {
	    p1[k] = std::min(std::min(v0[k], v1[k]), v2[k]);
	    p2[k] = std::max(std::max(v0[k], v1[k]), v2[k]);
	    /* Inflate to avoid zero-volume boxes for degenerate triangles */
	    double inflate = (p2[k] - p1[k]) * 0.1;
	    if (inflate < SMALL_FASTF)
		inflate = diag_len * 0.0001;
	    p1[k] -= inflate;
	    p2[k] += inflate;
	}

	face_bboxes.Insert(p1, p2, (int)fi);
    }

    RTree<int, double, 3>::Ray ray;
    for (int i = 0; i < 3; i++) {
	ray.o[i] = origin[i];
	ray.d[i] = dir[i];
	ray.di[i] = 1.0 / ray.d[i];
    }

    std::set<int> afaces;
    size_t fcnt = face_bboxes.Intersects(&ray, &afaces);
    if (fcnt == 0) {
	bu_vls_printf(gib->vls, "no nearby faces found\n");
	return BRLCAD_OK;
    }

    /* First pass: prefer faces that the ray directly intersects.  Among
     * all hit faces keep the one with the smallest t (closest to the ray
     * origin). */
    double tmin = DBL_MAX;
    int cface = -1;

    for (std::set<int>::iterator it = afaces.begin(); it != afaces.end(); it++) {
	int fi = *it;
	point_t v0, v1, v2;
	VMOVE(v0, &bot->vertices[3*bot->faces[3*fi+0]]);
	VMOVE(v1, &bot->vertices[3*bot->faces[3*fi+1]]);
	VMOVE(v2, &bot->vertices[3*bot->faces[3*fi+2]]);

	point_t isect;
	if (bg_isect_tri_ray(origin, dir, v0, v1, v2, &isect)) {
	    vect_t diff;
	    VSUB2(diff, isect, origin);
	    double t = VDOT(diff, dir);
	    if (t < tmin) {
		tmin = t;
		cface = fi;
	    }
	}
    }

    /* Second pass: if no direct ray hit, fall back to the face whose
     * centroid is closest to the ray line. */
    double dmin = DBL_MAX;
    if (cface < 0) {
	struct bn_tol tol = BN_TOL_INIT_TOL;
	for (std::set<int>::iterator it = afaces.begin(); it != afaces.end(); it++) {
	    int fi = *it;
	    int vi0 = bot->faces[3*fi+0];
	    int vi1 = bot->faces[3*fi+1];
	    int vi2 = bot->faces[3*fi+2];

	    point_t centroid;
	    centroid[X] = (bot->vertices[3*vi0+0] + bot->vertices[3*vi1+0] + bot->vertices[3*vi2+0]) / 3.0;
	    centroid[Y] = (bot->vertices[3*vi0+1] + bot->vertices[3*vi1+1] + bot->vertices[3*vi2+1]) / 3.0;
	    centroid[Z] = (bot->vertices[3*vi0+2] + bot->vertices[3*vi1+2] + bot->vertices[3*vi2+2]) / 3.0;

	    fastf_t cdist;
	    point_t pca;
	    bg_dist_pnt3_line3(&cdist, pca, origin, centroid, dir, &tol);
	    if ((double)cdist < dmin) {
		dmin = (double)cdist;
		cface = fi;
	    }
	}
    }

    if (gib->gb->verbosity) {
	if (tmin < DBL_MAX) {
	    bu_vls_printf(gib->vls, "F[%d]: %g\n", cface, tmin);
	} else {
	    bu_vls_printf(gib->vls, "F[%d]: %g\n", cface, dmin);
	}
    } else {
	bu_vls_printf(gib->vls, "%d\n", cface);
    }

    return BRLCAD_OK;
}


static void
_bot_pick_help(struct _ged_bot_ipick *bs, int argc, const char **argv)
{
    struct _ged_bot_ipick *gib = (struct _ged_bot_ipick *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gib->vls, "bot [options] <objname> pick <subcommand> [args]\n");
	bu_vls_printf(gib->vls, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	for (ctp = gib->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gib->vls, "  %s\t\t", ctp->ct_name);
	    helpflag[0] = ctp->ct_name;
	    bu_cmd(gib->cmds, 2, helpflag, 0, (void *)gib, &ret);
	}
    } else {
	int ret;
	const char *helpflag[2];
	helpflag[0] = argv[0];
	helpflag[1] = HELPFLAG;
	bu_cmd(gib->cmds, 2, helpflag, 0, (void *)gib, &ret);
    }
}

const struct bu_cmdtab _bot_pick_cmds[] = {
    { "E",          _bot_cmd_edge_pick},
    { "F",          _bot_cmd_face_pick},
    { "V",          _bot_cmd_vertex_pick},
    { (char *)NULL, NULL}
};

int
bot_pick(struct _ged_bot_info *gb, int argc, const char **argv)
{
    struct _ged_bot_ipick gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _bot_pick_cmds;

    if (!gb->intern) {
	bu_vls_printf(gib.vls, "attempting to pick, but no BoT data loaded\n");
	return BRLCAD_ERROR;
    }

    const struct rt_bot_internal *bot =
	(const struct rt_bot_internal *)(gb->intern->idb_ptr);
    RT_BOT_CK_MAGIC(bot);

    if (!argc) {
	_bot_pick_help(&gib, 0, NULL);
	return BRLCAD_OK;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--; argv++;
	argc--; argv++;
	_bot_pick_help(&gib, argc, argv);
	return BRLCAD_OK;
    }

    /* Must have a valid subcommand */
    if (bu_cmd_valid(_bot_pick_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid pick type \"%s\" specified\n", argv[0]);
	_bot_pick_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_bot_pick_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }
    return BRLCAD_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
