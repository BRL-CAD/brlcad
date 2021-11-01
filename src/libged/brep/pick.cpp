/*                        P I C K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file libged/brep/pick.cpp
 *
 * Visual selection of individual subcomponents of brep objects.
 *
 */

#include "common.h"

#include <set>

#include "../../libbrep/cdt/RTree.h"
#include "opennurbs.h"
#include "bu/cmd.h"
#include "./ged_brep.h"

struct _ged_brep_ipick {
    struct _ged_brep_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

static int
_brep_pick_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_ipick *gb = (struct _ged_brep_ipick *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->vls, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->vls, "%s\n", ps);
	return 1;
    }
    return 0;
}

// E - topological edges
extern "C" int
_brep_cmd_edge_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> pick E [px py pz dx dy dz]";
    const char *purpose_string = "pick closest 3D edge to line";
    if (_brep_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_ipick *gib = (struct _ged_brep_ipick *)bs;
    struct ged *gedp = gib->gb->gedp;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;

    point_t origin;
    vect_t dir;

    if (argc && argc != 6) {
	bu_vls_printf(gib->vls, "need six values for point and direction\n");
	return GED_ERROR;
    } else {
	// If not explicitly specified, get the ray from GED
	VSET(origin, -gedp->ged_gvp->gv_center[MDX], -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);
	VSCALE(origin, origin, gedp->dbip->dbi_base2local);
	VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
	VSCALE(dir, dir, -1.0);
	// Back outside the shape using the brep bounding box diagonal length
	ON_BoundingBox brep_bb = brep->BoundingBox();
	for (int i = 0; i < 3; i++) {
	    origin[i] = origin[i] + brep_bb.Diagonal().Length() * -1 * dir[i];
	}
    }

    // We're looking at edges - assemble bounding boxes for an initial cull
    RTree<int, double, 3> edge_bboxes;
    for (int i = 0; i < brep->m_E.Count(); i++) {
	ON_BoundingBox bb = brep->m_E[i].BoundingBox();

	// Make sure we have some volume...
	double dist = 0.1*bb.m_max.DistanceTo(bb.m_min);
	double xdist = bb.m_max.x - bb.m_min.x;
	double ydist = bb.m_max.y - bb.m_min.y;
	double zdist = bb.m_max.z - bb.m_min.z;
	if (xdist < dist) {
	    bb.m_min.x = bb.m_min.x - 0.5*dist;
	    bb.m_max.x = bb.m_max.x + 0.5*dist;
	} else {
	    bb.m_min.x = bb.m_min.x - 0.05*dist;
	    bb.m_max.x = bb.m_max.x + 0.05*dist;
	}
	if (ydist < dist) {
	    bb.m_min.y = bb.m_min.y - 0.5*dist;
	    bb.m_max.y = bb.m_max.y + 0.5*dist;
	} else {
	    bb.m_min.y = bb.m_min.y - 0.05*dist;
	    bb.m_max.y = bb.m_max.y + 0.05*dist;
	}

	if (zdist < dist) {
	    bb.m_min.z = bb.m_min.z - 0.5*dist;
	    bb.m_max.z = bb.m_max.z + 0.5*dist;
	} else {
	    bb.m_min.z = bb.m_min.z - 0.05*dist;
	    bb.m_max.z = bb.m_max.z + 0.05*dist;
	}


	double p1[3];
	p1[0] = bb.Min().x;
	p1[1] = bb.Min().y;
	p1[2] = bb.Min().z;
	double p2[3];
	p2[0] = bb.Max().x;
	p2[1] = bb.Max().y;
	p2[2] = bb.Max().z;
	edge_bboxes.Insert(p1, p2, i);
    }

    RTree<int, double, 3>::Ray ray;
    for (int i = 0; i < 3; i++) {
	ray.o[i] = origin[i];
	ray.d[i] = dir[i];
	ray.di[i] = 1/ray.d[i];
    }

    std::set<int> aedges;
    size_t fcnt = edge_bboxes.Intersects(&ray, &aedges);
    if (fcnt == 0) {
	bu_vls_printf(gib->vls, "no nearby edges found\n");
	return GED_OK;
    }

    //edge_bboxes.plot("tree.plot3");

    // Construct ON_Line
    ON_BoundingBox brep_bb = brep->BoundingBox();
    ON_3dVector vdir(dir[0], dir[1], dir[2]);
    ON_3dPoint porigin(origin[0], origin[1], origin[2]);
    vdir.Unitize();
    vdir = vdir * (brep_bb.Diagonal().Length() * 2);
    ON_3dPoint lp1 = porigin - vdir;
    ON_3dPoint lp2 = porigin + vdir;
    ON_Line l(lp1, lp2);

    double dmin = DBL_MAX;
    int cedge = -1;

    std::set<int>::iterator a_it;
    for (a_it = aedges.begin(); a_it != aedges.end(); a_it++) {
	const ON_BrepEdge &edge = brep->m_E[*a_it];
	const ON_Curve *curve = edge.EdgeCurveOf();
	if (!curve) continue;
	ON_NurbsCurve nc;
	curve->GetNurbForm(nc);
	double ndist = 0.0;
	if (ON_NurbsCurve_ClosestPointToLineSegment(&ndist, NULL, &nc, l, brep_bb.Diagonal().Length(), NULL)) {
	    if (ndist < dmin) {
		dmin = ndist;
		cedge = *a_it;
	    }
	}
    }

    if (gib->gb->verbosity) {
	bu_vls_printf(gib->vls, "m_E[%d]: %g\n", cedge, dmin);
    } else {
	bu_vls_printf(gib->vls, "%d\n", cedge);
    }

    return GED_OK;
}

// F - topological faces
//
// TODO: May be too difficult to implement, but should take a look at work in:
//
// Jingjing Shen, Laurent Busé, Pierre Alliez, Neil Dodgson,
// A line/trimmed NURBS surface intersection algorithm using matrix representations,
// Computer Aided Geometric Design, Volume 48, 2016, Pages 1-16
// https://hal.inria.fr/hal-01268109v2/document
//
// The "M-rep" approach sounds quite interesting as an alternative to current
// ray solving methods, but the implementation will require quite a bit of
// knowledge about translating academic discussions of matrix operations to
// working code...
//
// Related:
// https://hal.inria.fr/hal-00847802/document
extern "C" int
_brep_cmd_face_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> pick F [px py pz dx dy dz]";
    const char *purpose_string = "pick closest face";
    if (_brep_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_ipick *gib = (struct _ged_brep_ipick *)bs;
    struct ged *gedp = gib->gb->gedp;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;

    point_t origin;
    vect_t dir;

    if (argc && argc != 6) {
	bu_vls_printf(gib->vls, "need six values for point and direction\n");
	return GED_ERROR;
    } else {
	// If not explicitly specified, get the ray from GED
	VSET(origin, -gedp->ged_gvp->gv_center[MDX], -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);
	VSCALE(origin, origin, gedp->dbip->dbi_base2local);
	VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
	VSCALE(dir, dir, -1.0);
	// Back outside the shape using the brep bounding box diagonal length
	ON_BoundingBox brep_bb = brep->BoundingBox();
	for (int i = 0; i < 3; i++) {
	    origin[i] = origin[i] + brep_bb.Diagonal().Length() * -1 * dir[i];
	}
    }

    // We're looking at faces - assemble bounding boxes for an initial cull
    RTree<int, double, 3> face_bboxes;
    for (int i = 0; i < brep->m_F.Count(); i++) {
	ON_BoundingBox bb = brep->m_F[i].BoundingBox();
	double p1[3];
	p1[0] = bb.Min().x;
	p1[1] = bb.Min().y;
	p1[2] = bb.Min().z;
	double p2[3];
	p2[0] = bb.Max().x;
	p2[1] = bb.Max().y;
	p2[2] = bb.Max().z;
	face_bboxes.Insert(p1, p2, i);
    }

    RTree<int, double, 3>::Ray ray;
    for (int i = 0; i < 3; i++) {
	ray.o[i] = origin[i];
	ray.d[i] = dir[i];
	ray.di[i] = 1/ray.d[i];
    }

    face_bboxes.plot("tree.plot3");

    std::set<int> afaces;
    size_t fcnt = face_bboxes.Intersects(&ray, &afaces);
    if (fcnt == 0) {
	bu_vls_printf(gib->vls, "no nearby faces found\n");
	return GED_OK;
    }


    // Construct ON_Line
    ON_BoundingBox brep_bb = brep->BoundingBox();
    ON_3dVector vdir(dir[0], dir[1], dir[2]);
    ON_3dPoint porigin(origin[0], origin[1], origin[2]);
    vdir.Unitize();
    vdir = vdir * (brep_bb.Diagonal().Length() * 2);
    ON_3dPoint lp1 = porigin - vdir;
    ON_3dPoint lp2 = porigin + vdir;
    ON_Line l(lp1, lp2);

    double dmin = DBL_MAX;
    int cface = -1;

    std::set<int>::iterator a_it;
    for (a_it = afaces.begin(); a_it != afaces.end(); a_it++) {
	bu_vls_printf(gib->vls, "%d\n", *a_it);
	ON_BrepLoop *loop = brep->m_F[*a_it].OuterLoop();
	ON_3dPoint cedge_pt;
	double edist = DBL_MAX;
	for (int i = 0; i < loop->TrimCount(); i++) {
	    ON_BrepTrim *trim = loop->Trim(i);
	    ON_BrepEdge *edge = trim->Edge();
	    if (edge) {
		const ON_Curve *curve = edge->EdgeCurveOf();
		if (!curve) continue;
		ON_NurbsCurve nc;
		curve->GetNurbForm(nc);
		double ndist = 0.0;
		double t;
		bool have_closest_pt = ON_NurbsCurve_ClosestPointToLineSegment(&ndist, &t, &nc, l, brep_bb.Diagonal().Length(), NULL);
		if (have_closest_pt) {
		    if (ndist < edist) {
			edist = ndist;
			cedge_pt = curve->PointAt(t);
		    }
		}
	    }
	}

	// See if there is a workable iteration to find the closest surface to
	// the line (maybe starting with the closest point on the line to the
	// face's closest edge curve to the line, find the closest surface
	// point to that point, use the closest surface point to get a new
	// closest point on the line, and iterate?)

	ON_3dPoint l3d;
	ON_2dPoint s2d;
	ON_3dPoint s3d;
	double sdist = DBL_MAX;
	bool have_hit = false;
	double cdist = DBL_MAX;
	double idist = 0;
	double lt;
	l.ClosestPointTo(cedge_pt, &lt);
	l3d = l.PointAt(lt);
	bu_log("%d l3d: %f %f %f\n", *a_it, l3d.x, l3d.y, l3d.z);
	if (!surface_GetClosestPoint3dFirstOrder(brep->m_F[*a_it].SurfaceOf(), l3d, s2d, s3d, sdist)) {
	    bu_log("%d: get closest surface point failed??\n", *a_it);
	    continue;
	}
	bu_log("%d s3d: %f %f %f\n", *a_it, s3d.x, s3d.y, s3d.z);
	cdist = l3d.DistanceTo(s3d);
	while (!NEAR_ZERO(fabs(cdist-idist), ON_ZERO_TOLERANCE)) {
	    idist = cdist;
	    l.ClosestPointTo(s3d, &lt);
	    l3d = l.PointAt(lt);
	    bu_log("%d l3d+: %f %f %f\n", *a_it, l3d.x, l3d.y, l3d.z);
	    if (!surface_GetClosestPoint3dFirstOrder(brep->m_F[*a_it].SurfaceOf(), l3d, s2d, s3d, sdist)) {
		bu_log("%d: get closest surface point failed??\n", *a_it);
		break;;
	    }
	    bu_log("%d s3d+: %f %f %f\n", *a_it, s3d.x, s3d.y, s3d.z);
	    cdist = l3d.DistanceTo(s3d);
	}
	bu_log("%d cdist: %f\n", *a_it, cdist);
	bu_log("%d sdist: %f\n", *a_it, sdist);

	if (NEAR_ZERO(sdist, ON_ZERO_TOLERANCE)) {
	    double hdist = porigin.DistanceTo(s3d);
	    bu_log("%d hdist: %f\n", *a_it, hdist);
	    bu_log("center %f %f %f\n", s3d.x, s3d.y, s3d.z);
	    if (hdist < dmin) {
		dmin = hdist;
		cface = *a_it;
	    }
	    continue;
	}

	if (!have_hit && cdist < dmin) {
	    dmin = cdist;
	    cface = *a_it;
	}
    }

    if (gib->gb->verbosity) {
	bu_vls_printf(gib->vls, "m_F[%d]: %g\n", cface, dmin);
    } else {
	bu_vls_printf(gib->vls, "%d\n", cface);
    }


    return GED_OK;
}

// V - 3D vertices
extern "C" int
_brep_cmd_vertex_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> pick E [px py pz dx dy dz]";
    const char *purpose_string = "pick closest 3D vertex to line";
    if (_brep_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_ipick *gib = (struct _ged_brep_ipick *)bs;
    struct ged *gedp = gib->gb->gedp;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    ON_BoundingBox brep_bb = brep->BoundingBox();

    point_t origin;
    vect_t dir;

    if (argc && argc != 6) {
	bu_vls_printf(gib->vls, "need six values for point and direction\n");
	return GED_ERROR;
    } else {
	// If not explicitly specified, get the ray from GED
	VSET(origin, -gedp->ged_gvp->gv_center[MDX], -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);
	VSCALE(origin, origin, gedp->dbip->dbi_base2local);
	VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
	VSCALE(dir, dir, -1.0);
	// Back outside the shape using the brep bounding box diagonal length
	for (int i = 0; i < 3; i++) {
	    origin[i] = origin[i] + brep_bb.Diagonal().Length() * -1 * dir[i];
	}
    }

    // We're looking at vertices - assemble bounding boxes for an initial cull
    RTree<int, double, 3> vert_bboxes;
    for (int i = 0; i < brep->m_V.Count(); i++) {
	const ON_BrepVertex &v = brep->m_V[i];
	double vlen = 0;
	// A vertex by itself has no concept of volume - we need to deduce one
	// from its neighborhood geometry.

	if (!v.EdgeCount()) {
	    // If a vertex has no associated edges (??) all we can do is use some
	    // fraction of the brep bounding box volume.
	    vlen = brep_bb.Diagonal().Length() * 0.05;
	} else {
	    // If a vertex has associated edges, find the shortest associated
	    // control polyline length.  We can be more generous with box sizes
	    // when other vertices are far away - if things are close, we need
	    // tighter boxes.
	    double edge_lens = 0.0;
	    int edge_cnts = 0;
	    for (int ec = 0; ec < v.EdgeCount(); ec++) {
		const ON_BrepEdge &edge = brep->m_E[v.m_ei[ec]];
		const ON_Curve *curve = edge.EdgeCurveOf();
		if (!curve) continue;
		ON_NurbsCurve nc;
		curve->GetNurbForm(nc);
		edge_lens += nc.ControlPolygonLength();
		edge_cnts++;
	    }
	    vlen = edge_lens/((double)edge_cnts);
	}


	ON_BoundingBox vbb(v.Point(), v.Point());
	vbb.m_min.x = vbb.m_min.x - 0.2*vlen;
	vbb.m_max.x = vbb.m_max.x + 0.2*vlen;
	vbb.m_min.y = vbb.m_min.y - 0.2*vlen;
	vbb.m_max.y = vbb.m_max.y + 0.2*vlen;
	vbb.m_min.z = vbb.m_min.z - 0.2*vlen;
	vbb.m_max.z = vbb.m_max.z + 0.2*vlen;

	double p1[3];
	p1[0] = vbb.Min().x;
	p1[1] = vbb.Min().y;
	p1[2] = vbb.Min().z;
	double p2[3];
	p2[0] = vbb.Max().x;
	p2[1] = vbb.Max().y;
	p2[2] = vbb.Max().z;
	vert_bboxes.Insert(p1, p2, i);
    }

    vert_bboxes.plot("tree.plot3");

    RTree<int, double, 3>::Ray ray;
    for (int i = 0; i < 3; i++) {
	ray.o[i] = origin[i];
	ray.d[i] = dir[i];
	ray.di[i] = 1/ray.d[i];
    }

    std::set<int> averts;
    size_t fcnt = vert_bboxes.Intersects(&ray, &averts);
    if (fcnt == 0) {
	bu_vls_printf(gib->vls, "no nearby vertices found\n");
	return GED_OK;
    }

    // Construct ON_Line
    ON_3dVector vdir(dir[0], dir[1], dir[2]);
    ON_3dPoint porigin(origin[0], origin[1], origin[2]);
    vdir.Unitize();
    vdir = vdir * (brep_bb.Diagonal().Length() * 2);
    ON_3dPoint lp1 = porigin - vdir;
    ON_3dPoint lp2 = porigin + vdir;
    ON_Line l(lp1, lp2);

    double dmin = DBL_MAX;
    int cvert = -1;

    std::set<int>::iterator a_it;
    for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	const ON_BrepVertex &v = brep->m_V[*a_it];
	double vmin = l.MinimumDistanceTo(v.Point());
	if (vmin < dmin) {
	    cvert = *a_it;
	    dmin = vmin;
	}
    }	

    if (gib->gb->verbosity) {
	bu_vls_printf(gib->vls, "m_V[%d]: %g\n", cvert, dmin);
    } else {
	bu_vls_printf(gib->vls, "%d\n", cvert);
    }
    return GED_OK;
}

static void
_brep_pick_help(struct _ged_brep_ipick *bs, int argc, const char **argv)
{
    struct _ged_brep_ipick *gb = (struct _ged_brep_ipick *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "brep [options] <objname> plot <subcommand> [args]\n");
	bu_vls_printf(gb->vls, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gb->vls, "  %s\t\t", ctp->ct_name);
	    helpflag[0] = ctp->ct_name;
	    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
	}
    } else {
	int ret;
	const char *helpflag[2];
	helpflag[0] = argv[0];
	helpflag[1] = HELPFLAG;
	bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
}

const struct bu_cmdtab _brep_pick_cmds[] = {
    { "E",          _brep_cmd_edge_pick},
    { "F",          _brep_cmd_face_pick},
    { "V",          _brep_cmd_vertex_pick},
    { (char *)NULL, NULL}
};

int
brep_pick(struct _ged_brep_info *gb, int argc, const char **argv)
{
    struct _ged_brep_ipick gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _brep_pick_cmds;

    const ON_Brep *brep = ((struct rt_brep_internal *)(gb->intern.idb_ptr))->brep;
    if (brep == NULL) {
	bu_vls_printf(gib.vls, "attempting to pick, but no ON_Brep data present\n");
	return GED_ERROR;
    }

    if (!argc) {
	_brep_pick_help(&gib, 0, NULL);
	return GED_OK;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--;argv++;
	argc--;argv++;
	_brep_pick_help(&gib, argc, argv);
	return GED_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_pick_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid pick type \"%s\" specified\n", argv[0]);
	_brep_pick_help(&gib, 0, NULL);
	return GED_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_pick_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }
    return GED_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
