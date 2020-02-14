/*                        P I C K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
    const char *usage_string = "brep [options] <objname1> plot E [px py pz dx dy dz]";
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
	VSCALE(origin, origin, gedp->ged_wdbp->dbip->dbi_base2local);
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
    ON_3dPoint ptol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
    for (int i = 0; i < brep->m_E.Count(); i++) {
	ON_BoundingBox bb = brep->m_E[i].BoundingBox();
	ON_3dPoint min2 = bb.m_min - ptol;
	ON_3dPoint max2 = bb.m_max + ptol;
	bb.Set(min2, true);
	bb.Set(max2, true);

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
	if (ON_NurbsCurve_ClosestPointToLineSegment(&ndist, NULL, &nc, l, brep_bb.Diagonal().Length())) {
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
extern "C" int
_brep_cmd_face_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot F [[index][index-index]]";
    const char *purpose_string = "topological faces";
    if (_brep_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_ipick *gib = (struct _ged_brep_ipick *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bn_vlblock *vbp = gib->gb->vbp;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepFace& face = brep->m_F[fi];
	if (!face.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	if (color) {
	    plotface(face, vbp, plotres, true, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotface(face, vbp, plotres, true, 10, 10, 10);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_F_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// V - 3D vertices
extern "C" int
_brep_cmd_vertex_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot V [[index][index-index]]";
    const char *purpose_string = "3D vertices";
    if (_brep_pick_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_ipick *gib = (struct _ged_brep_ipick *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bn_vlblock *vbp = gib->gb->vbp;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_V.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int vi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepVertex &vertex = brep->m_V[vi];
	if (!vertex.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "vertex %d is not valid, skipping", vi);
	    continue;
	}
	if (color) {
	    plotpoint(vertex.Point(), vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotpoint(vertex.Point(), vbp, GREEN);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_V_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

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
