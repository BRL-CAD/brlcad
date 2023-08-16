/*                        C U R V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @file libged/brep/curve.cpp
 *
 * NURBS curves editing support for brep objects
 *
 */

#include "common.h"

#include "wdb.h"
#include "../ged_private.h"
#include "./ged_brep.h"

using namespace brlcad;

struct _ged_brep_icurve {
    struct _ged_brep_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

static int
_brep_curve_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_icurve *gb = (struct _ged_brep_icurve *)bs;
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

static int
_brep_cmd_curve_create(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve create <x> <y> <z>";
    const char *purpose_string = "create a new NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    argc--;argv++;

    ON_3dPoint position(0, 0, 0);
    if (argc != 0 && argc != 3) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }
    if (argc == 3) {
	position = ON_3dPoint(atof(argv[0]), atof(argv[1]), atof(argv[2]));
    }
    // Create a template nurbs curve
    int curve_id = brep_curve_make(b_ip->brep, position);

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create C3 curve! id = %d", curve_id);
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_in(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve in <is_rational> <order> <cv_count> <cv1_x> <cv1_y> <cv1_z> <cv_w>(if rational) ...";
    const char *purpose_string = "create a new NURBS curve given detailed description";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 4) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    bool is_rational = atoi(argv[1]);
    int order = atoi(argv[2]);
    int cv_count = atoi(argv[3]);

    if (order <= 0 || cv_count <= 0 || cv_count < order) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid order or cv_count\n");
	return BRLCAD_ERROR;
    }

    if (argc < 4 + cv_count * (3 + (is_rational ? 1 : 0))) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments, you need to input %d more args about control vertices\n", 4 + cv_count * (3 + (is_rational ? 1 : 0)) - argc);
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    argc--;argv++;

    std::vector<ON_4dPoint> cv;
    for (int i = 0; i < cv_count; i++) {
	ON_4dPoint p;
	p.x = atof(argv[3 + i * (3 + (is_rational ? 1 : 0))]);
	p.y = atof(argv[3 + i * (3 + (is_rational ? 1 : 0)) + 1]);
	p.z = atof(argv[3 + i * (3 + (is_rational ? 1 : 0)) + 2]);
	if (is_rational) {
	    p.w = atof(argv[3 + i * (3 + (is_rational ? 1 : 0)) + 3]);
	} else {
	    p.w = 1.0;
	}
	cv.push_back(p);
    }
    int curve_id = brep_curve_in(b_ip->brep, is_rational, order, cv_count, cv);
    if (curve_id < 0) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to create curve\n");
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create C3 curve! id = %d", curve_id);
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_interp(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve interp <cv_count> <cv1_x> <cv1_y> <cv1_z> ...";
    const char *purpose_string = "create a new NURBS curve interpolating given control vertices";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 2) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough 	arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", 	usage_string);
	return BRLCAD_ERROR;
    }

    int cv_count = atoi(argv[1]);

    if (cv_count <= 0) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid cv_count\n");
	return BRLCAD_ERROR;
    }

    if (argc < 2 + cv_count * 3) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough 	arguments, you need to input %d more args about control 	vertices\n", 2 + cv_count * 3 - argc);
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", 	usage_string);
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    argc--;argv++;

    std::vector<ON_3dPoint> points;
    for (int i = 0; i < cv_count; i++) {
	ON_3dPoint p;
	p.x = atof(argv[1 + i * 3]);
	p.y = atof(argv[1 + i * 3 + 1]);
	p.z = atof(argv[1 + i * 3 + 2]);
	points.push_back(p);
    }
    int curve_id = brep_curve_interpCrv(b_ip->brep, points);
    if (curve_id < 0) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to create 	curve\n");
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create C3 curve! id = %d", curve_id);
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_copy(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve copy <curve_id>";
    const char *purpose_string = "copy a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 2) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough 	arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", 	usage_string);
	return BRLCAD_ERROR;
    }

    int curve_id = atoi(argv[1]);

    if (curve_id < 0) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid curve_id\n");
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    int res = brep_curve_copy(b_ip->brep, curve_id);
    if (!res) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to copy curve\n");
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "successful copy C3 curve! new curve id = %d", res);
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_remove(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve remove <curve_id>";
    const char *purpose_string = "remove a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 2) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough 	arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", 	usage_string);
	return BRLCAD_ERROR;
    }

    int curve_id = atoi(argv[1]);

    if (curve_id < 0) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid curve_id\n");
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    bool res = brep_curve_remove(b_ip->brep, curve_id);
    if (!res) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to remove curve\n");
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "successful remove C3 curve! id = %d", curve_id);
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_move(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve move  <curve_id> <x> <y> <z>";
    const char *purpose_string = "move a NURBS curve to a specified position";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;
    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 4) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    ON_3dPoint p = ON_3dPoint(atof(argv[1]), atof(argv[2]), atof(argv[3]));
    bool flag = brep_curve_move(b_ip->brep, atoi(argv[0]), p);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to move curve %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_set_cv(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve set_cv <curve_id> <CV_id> <x> <y> <z> [<w>]";
    const char *purpose_string = "set the control vertex of a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;

    if (argc < 6) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    ON_4dPoint p = ON_4dPoint(atof(argv[3]), atof(argv[4]), atof(argv[5]), argc == 7 ? atof(argv[6]) : 1.0);
    bool flag = brep_curve_set_cv(b_ip->brep, atoi(argv[1]), atoi(argv[2]), p);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to move control vertex %s of curve %s\n", argv[2], argv[1]);
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_flip(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> curve flip <curve_id>";
    const char *purpose_string = "Flip the direction of a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 2) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    int curve_id = 0;
    try {
	curve_id = std::stoi(argv[1]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid curve id\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;

    bool flag = brep_curve_reverse(b_ip->brep, curve_id);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to reverse curve %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_insert_knot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> insert_knot <curve_id> <knot_value> <multiplicity>";
    const char *purpose_string = "Insert a knot into a NURBS curve";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }
    argc--;argv++;
    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 3) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    int curve_id = 0;
    try {
	curve_id = std::stoi(argv[0]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid curve id\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    double knot_value = 0;
    try {
	knot_value = std::stod(argv[1]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid knot value\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    int multiplicity = 0;
    try {
	multiplicity = std::stoi(argv[2]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid multiplicity\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    bool flag = brep_curve_insert_knot(b_ip->brep, curve_id, knot_value, multiplicity);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to insert knot into curve %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_trim(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> trim <curve_id> <start_param> <end_param>";
    const char *purpose_string = "trim a NURBS curve using start and end parameters";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }
    argc--;argv++;
    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 3) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    int curve_id = 0;
    try {
	curve_id = std::stoi(argv[0]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid curve id\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    double start_param = 0;
    try {
	start_param = std::stod(argv[1]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid start parameter\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    double end_param = 0;
    try {
	end_param = std::stod(argv[2]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid end parameter\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    bool flag = brep_curve_trim(b_ip->brep, curve_id, start_param, end_param);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to trim curve %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_split(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> split <curve_id> <param>";
    const char *purpose_string = "split a NURBS curve into two at a parameter";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }
    argc--;argv++;
    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 2) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    int curve_id = 0;
    try {
	curve_id = std::stoi(argv[0]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid curve id\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    double param = 0;
    try {
	param = std::stod(argv[1]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid parameter\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    bool flag = brep_curve_split(b_ip->brep, curve_id, param);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to split curve %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "split curve %s at parameter %s. Old curve removed.\n", argv[0], argv[1]);
    return BRLCAD_OK;
}

static int
_brep_cmd_curve_join(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> join <curve_id_1> <curve_id_2>";
    const char *purpose_string = "join end of curve 1 to start of curve 2";
    if (_brep_curve_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }
    argc--;argv++;
    struct _ged_brep_icurve *gib = (struct _ged_brep_icurve *)bs;
    if (argc < 2) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    int curve_id_1 = 0;
    int curve_id_2 = 0;
    try {
	curve_id_1 = std::stoi(argv[0]);
	curve_id_2 = std::stoi(argv[1]);
    } catch (const std::exception &e) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "invalid curve id\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", e.what());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    int flag = brep_curve_join(b_ip->brep, curve_id_1, curve_id_2);
    if (flag < 0) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "failed to join curve %s and curve %s\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "joined curve id %d, old curves deleted.\n", flag);
    return BRLCAD_OK;
}

static void
_brep_curve_help(struct _ged_brep_icurve *bs, int argc, const char **argv)
{
    struct _ged_brep_icurve *gb = (struct _ged_brep_icurve *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "brep [options] <objname> curve <subcommand> [args]\n");
	bu_vls_printf(gb->vls, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gb->vls, " %s\t\t", ctp->ct_name);
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

const struct bu_cmdtab _brep_curve_cmds[] = {
    { "create",          _brep_cmd_curve_create},
    { "in",              _brep_cmd_curve_in},
    { "interp",          _brep_cmd_curve_interp},
    { "copy",            _brep_cmd_curve_copy},
    { "remove",          _brep_cmd_curve_remove},
    { "move",            _brep_cmd_curve_move},
    { "set_cv",          _brep_cmd_curve_set_cv},
    { "flip",            _brep_cmd_curve_flip},
    { "insert_knot",     _brep_cmd_curve_insert_knot},
    { "trim",            _brep_cmd_curve_trim},
    { "split",           _brep_cmd_curve_split},
    { "join",            _brep_cmd_curve_join},
    { (char *)NULL,      NULL}
};

int brep_curve(struct _ged_brep_info *gb, int argc, const char **argv)
{
    struct _ged_brep_icurve gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _brep_curve_cmds;

    const ON_Brep *brep = ((struct rt_brep_internal *)(gb->intern.idb_ptr))->brep;
    if (brep == NULL) {
	bu_vls_printf(gib.vls, "not a brep object\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	_brep_curve_help(&gib, 0, NULL);
	return BRLCAD_OK;
    }

    // TODO: add help flag
    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--;argv++;
	argc--;argv++;
	_brep_curve_help(&gib, argc, argv);
	return BRLCAD_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_curve_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_brep_curve_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_curve_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
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
