/*                       R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2024 United States Government as represented by
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
/** @file libged/brep/repair.cpp
 *
 * LIBGED brep repair subcommand implementation - attempt various
 * types of repairs to brep data.
 *
 */

#include "common.h"

#include <set>

#include "opennurbs.h"
#include "bu/cmd.h"
#include "./ged_brep.h"

struct _ged_brep_rinfo {
    struct ged *gedp;
    struct bu_vls *vls;
    const ON_Brep *brep;
    ON_Brep *obrep;
    const struct bu_cmdtab *cmds;
};

static int
_brep_repair_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_rinfo *gb = (struct _ged_brep_rinfo *)bs;
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

// EC - regenerate 3D curves of topological edges
extern "C" int
_brep_cmd_edge_curve_repair(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info EC [[index][index-index]]";
    const char *purpose_string = "rebuild invalid 3D edge curves from 2D trim curves";
    if (_brep_repair_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_rinfo *gib = (struct _ged_brep_rinfo *)bs;
    const ON_Brep *obrep = gib->brep;
    ON_Brep brep(*obrep);

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, check all
    if (!elements.size()) {
	for (int i = 0; i < brep.m_E.Count(); i++)
	    elements.insert(i);
    }

    std::set<int>::iterator e_it;

    bool updated = false;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ei = *e_it;
	ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.m_ti.Count() <= 0) {
	    bu_log("No associated trims, could not validate edge curve of %d\n", ei);
	    continue;
	}

	const ON_BrepTrim *trim = NULL;
	const ON_Curve *trimCurve = NULL;
	for (int j = 0; j < edge.m_ti.Count(); j++) {
	    trim = &brep.m_T[edge.m_ti[j]];
	    trimCurve = trim->TrimCurveOf();
	    if (trimCurve)
		break;
	}
	if (!trimCurve) {
	    bu_log("No associated trim curve found, could not validate edge curve of %d\n", ei);
	    continue;
	}
	const ON_BrepFace *f = trim->Face();
	if (!trimCurve) {
	    bu_log("No associated trim face found, could not validate edge curve of %d\n", ei);
	    continue;
	}
	const ON_Surface *s = f->SurfaceOf();
	if (!s) {
	    bu_log("No associated trim surface found, could not validate edge curve of %d\n", ei);
	    continue;
	}

	const ON_Curve *c3 = edge.EdgeCurveOf();
	ON_3dPoint ep = c3->PointAt(c3->Domain().Min());

	ON_Interval tdom = trimCurve->Domain();
	ON_3dPoint p2d1 = trimCurve->PointAt(tdom.Min());
	ON_3dPoint p2d2 = trimCurve->PointAt(tdom.Max());
	ON_3dPoint tp1 = s->PointAt(p2d1.x, p2d1.y);
	ON_3dPoint tp2 = s->PointAt(p2d2.x, p2d2.y);

	ON_3dPoint cp = (tp1.DistanceTo(ep) > tp2.DistanceTo(ep)) ? tp2 : tp1;

	if (cp.DistanceTo(ep) < BN_TOL_DIST)
	    continue;

	// Conditions met - replace curve
	updated = true;
	bu_log("Edge %d - %fmm dist between edge curve (%g %g %g) and trim curve (%g %g %g)\n", ei, cp.DistanceTo(ep), ep.x, ep.y, ep.z, cp.x, cp.y, cp.z);

	ON_NurbsCurve nc;
        trimCurve->GetNurbForm(nc);

	int plotres = nc.KnotCount() * 2;
	ON_3dPointArray spnts;
	for (int k = 0; k <= plotres; ++k) {
	    ON_3dPoint p = trimCurve->PointAt(tdom.ParameterAt((double)k / plotres));
	    ON_3dPoint sp = s->PointAt(p.x, p.y);
	    spnts.Append(sp);
	}

	// Loft a Bezier curve through the points sampled from the trimming
	// curve and replace the existing curve on the ON_BrepEdge.  Right now
	// this is just a naive sample and loft operation - it may be that
	// there are more intelligent methods that would produce better
	// results...
	ON_BezierCurve bc;
	bc.Loft(spnts);
	ON_NurbsCurve *bnc = ON_NurbsCurve::New(bc);
	int c3i = brep.AddEdgeCurve(bnc);
	edge.ChangeEdgeCurve(c3i);
    }

    if (updated) {
	brep.Compact();
	gib->obrep = ON_Brep::New(brep);
    }

    return BRLCAD_OK;
}

static void
_brep_repair_help(struct _ged_brep_rinfo *bs, int argc, const char **argv)
{
    struct _ged_brep_rinfo *gb = (struct _ged_brep_rinfo *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "brep [options] <objname> repair <subcommand> [args]\n");
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

const struct bu_cmdtab _brep_repair_cmds[] = {
    { "EC",           _brep_cmd_edge_curve_repair},
    { (char *)NULL,      NULL}
};

int
brep_repair(struct ged *gedp, const ON_Brep *brep, const char *oname, int argc, const char **argv)
{
    struct _ged_brep_rinfo gib;
    gib.gedp = gedp;
    gib.vls = gedp->ged_result_str;
    gib.brep = brep;
    gib.obrep = NULL;
    gib.cmds = _brep_repair_cmds;

    if (!argc) {
	// In the no arg case, try all repairs
	return BRLCAD_OK;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--;argv++;
	argc--;argv++;
	_brep_repair_help(&gib, argc, argv);
	return BRLCAD_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_repair_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_brep_repair_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_repair_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {

	if (gib.obrep) {

	    bu_log("Writing %s to disk\n", oname);

	    struct rt_brep_internal *bip_out;
	    BU_ALLOC(bip_out, struct rt_brep_internal);
	    bip_out->magic = RT_BREP_INTERNAL_MAGIC;
	    bip_out->brep = gib.obrep;

	    struct rt_db_internal intern;
	    RT_DB_INTERNAL_INIT(&intern);
	    intern.idb_ptr = (void *)bip_out;
	    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    intern.idb_meth = &OBJ[ID_BREP];
	    intern.idb_minor_type = ID_BREP;

	    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	    ret = wdb_export(wdbp, oname, intern.idb_ptr, ID_BREP, 1.0);
	}

	return ret;
    }


    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
