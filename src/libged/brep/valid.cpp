/*                        V A L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file libged/brep/info.cpp
 *
 * LIBGED brep valid subcommand implementation - reports detailed information
 * on the validity of individual breps.
 *
 */

#include "common.h"

#include <set>

#include "opennurbs.h"
#include "bu/cmd.h"
#include "./ged_brep.h"

struct _ged_brep_ivalid {
    struct bu_vls *vls;
    const ON_Brep *brep;
    const struct bu_cmdtab *cmds;
};

static int
_brep_valid_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_ivalid *gb = (struct _ged_brep_ivalid *)bs;
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


// C2 - 2D parameter curves
extern "C" int
_brep_cmd_curve_2d_valid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info C2 [[index][index-index]]";
    const char *purpose_string = "2D parameter space geometric curves";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_C2.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ci = *e_it;

	if (!((ci >= 0) && (ci < brep->m_C2.Count()))) {
	    bu_vls_printf(gvb->vls, "Invalid index: %d\n", ci);
	    return BRLCAD_ERROR;
	}

	const ON_Curve *curve = brep->m_C2[ci];
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_C2[%d] invalid:\n", ci);
	if (!curve->IsValid(&dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

// C3 - 3D edge curves
extern "C" int
_brep_cmd_curve_3d_valid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info C3 [[index][index-index]]";
    const char *purpose_string = "3D parameter space geometric curves";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_C3.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ci = *e_it;

	if (!((ci >= 0) && (ci < brep->m_C3.Count()))) {
	    bu_vls_printf(gvb->vls, "Invalid index: %d\n", ci);
	    return BRLCAD_ERROR;
	}

	const ON_Curve *curve = brep->m_C3[ci];
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_C3[%d] invalid:\n", ci);
	if (!curve->IsValid(&dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

// E - topological edges
extern "C" int
_brep_cmd_edge_valid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info E [[index][index-index]]";
    const char *purpose_string = "topological 3D edges";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_E.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ei = *e_it;

	if (!((ei >= 0) && (ei < brep->m_E.Count()))) {
	    bu_vls_printf(gvb->vls, "Invalid index: %d\n", ei);
	    return BRLCAD_ERROR;
	}

	const ON_BrepEdge &edge = brep->m_E[ei];
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_E[%d] invalid:\n", ei);
	if (!edge.IsValid(&dump) || !brep->IsValidEdge(ei, &dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

// F - topological faces
extern "C" int
_brep_cmd_face_valid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info F [[index][index-index]]";
    const char *purpose_string = "topological faces";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	if (!((fi >= 0) && (fi < brep->m_F.Count()))) {
	    bu_vls_printf(gvb->vls, "Invalid index: %d\n", fi);
	    return BRLCAD_ERROR;
	}

	const ON_BrepFace& face = brep->m_F[fi];
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_F[%d] invalid:\n", fi);
	if (!face.IsValid(&dump) || !brep->IsValidFace(fi, &dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

// L - 2D parameter space topological trimming loops
extern "C" int
_brep_cmd_loop_valid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info L [[index][index-index]]";
    const char *purpose_string = "2D parameter space topological trimming loops";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_L.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int li = *e_it;

	if (!((li >= 0) && (li < brep->m_L.Count()))) {
	    bu_vls_printf(gvb->vls, "Invalid index: %d\n", li);
	    return BRLCAD_ERROR;
	}

	const ON_BrepLoop &loop = brep->m_L[li];
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_L[%d] invalid:\n", li);
	if (!loop.IsValid(&dump) || !brep->IsValidLoop(li, &dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

// S - surfaces
extern "C" int
_brep_cmd_surface_valid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info S [[index][index-index]]";
    const char *purpose_string = "surfaces";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_S.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int si = *e_it;
	ON_wString wonstr;
	ON_TextLog info_output(wonstr);
	if (!((si >= 0) && (si < brep->m_S.Count()))) {
	    return BRLCAD_ERROR;
	}
	const ON_Surface* srf = brep->m_S[si];
	if (!srf) continue;
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_S[%d] invalid:\n", si);
	if (!srf->IsValid(&dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

// T - 2D topological trims
extern "C" int
_brep_cmd_trim_valid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> info T [[index][index-index]]";
    const char *purpose_string = "2D parameter space topological trims";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_T.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ti = *e_it;
	if (!((ti >= 0) && (ti < brep->m_T.Count()))) {
	    return BRLCAD_ERROR;
	}
	const ON_BrepTrim &trim = brep->m_T[ti];
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_T[%d] invalid:\n", ti);
	if (!trim.IsValid(&dump) || !brep->IsValidTrim(ti, &dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

// V - 3D vertices
extern "C" int
_brep_cmd_vertex_valid(void *bs, int argc, const char **argv)
{
    //struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const char *usage_string = "brep [options] <objname1> info V [[index][index-index]]";
    const char *purpose_string = "3D vertices";
    if (_brep_valid_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;

    struct _ged_brep_ivalid *gvb = (struct _ged_brep_ivalid *)bs;
    const ON_Brep *brep = gvb->brep;

    std::set<int> elements;
    if (_brep_indices(elements, gvb->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_V.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    bool valid = true;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int vi = *e_it;

	if (!((vi >= 0) && (vi < brep->m_V.Count()))) {
	    return BRLCAD_ERROR;
	}
	const ON_BrepVertex &vertex = brep->m_V[vi];
	ON_wString wstr;
	ON_TextLog dump(wstr);
	dump.Print("m_V[%d] invalid:\n", vi);
	if (!vertex.IsValid(&dump) || !brep->IsValidVertex(vi, &dump)) {
	    ON_String ss = wstr;
	    bu_vls_printf(gvb->vls, "%s\n", ss.Array());
	    valid = false;
	}
    }

    return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
}

static void
_brep_valid_help(struct _ged_brep_ivalid *bs, int argc, const char **argv)
{
    struct _ged_brep_ivalid *gb = (struct _ged_brep_ivalid *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "brep [options] <objname> valid [subcommand] [args]\n");
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

const struct bu_cmdtab _brep_valid_cmds[] = {
    { "C2",          _brep_cmd_curve_2d_valid},
    { "C3",          _brep_cmd_curve_3d_valid},
    { "E",           _brep_cmd_edge_valid},
    { "F",           _brep_cmd_face_valid},
    { "L",           _brep_cmd_loop_valid},
    { "S",           _brep_cmd_surface_valid},
    { "T",           _brep_cmd_trim_valid},
    { "V",           _brep_cmd_vertex_valid},
    { (char *)NULL,      NULL}
};

int
brep_valid(struct bu_vls *vls, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct _ged_brep_ivalid gvb;
    gvb.vls = vls;
    struct rt_brep_internal *bi = (struct rt_brep_internal*)intern->idb_ptr;
    gvb.brep = bi->brep;
    gvb.cmds = _brep_valid_cmds;

    if (!argc) {
	// No arg case is easy - just report overall status
	int valid = rt_brep_valid(vls, intern, 0);
	return (valid) ? BRLCAD_OK : BRLCAD_ERROR;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--;argv++;
	argc--;argv++;
	_brep_valid_help(&gvb, argc, argv);
	return BRLCAD_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_valid_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gvb.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_brep_valid_help(&gvb, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_valid_cmds, argc, argv, 0, (void *)&gvb, &ret) == BRLCAD_OK) {
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
