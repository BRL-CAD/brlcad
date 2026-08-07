/*                     B R E P - A U D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep-audit.cpp
 *
 * Isolated realization checks for BRep wireframes and fast shaded meshes.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#  include <sys/resource.h>
#endif

#include "bu/app.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/datetime.h"
#include "bv/vlist.h"
#include "brep/cdt.h"
#include "brep/util.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"

struct geom_result {
    int ret = BRLCAD_ERROR;
    size_t vertices = 0;
    size_t primitives = 0;
    size_t commands = 0;
    size_t invalid_indices = 0;
    bool finite = true;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    std::vector<std::string> issues;
};

static size_t
peak_rss_bytes()
{
#ifndef _WIN32
    struct rusage usage_info;
    if (getrusage(RUSAGE_SELF, &usage_info) != 0)
	return 0;
#  ifdef __APPLE__
    return (size_t)usage_info.ru_maxrss;
#  else
    return (size_t)usage_info.ru_maxrss * 1024U;
#  endif
#else
    return 0;
#endif
}

static bool
set_memory_limit(long memory_limit_mib)
{
    if (memory_limit_mib <= 0)
	return true;
#ifndef _WIN32
    const rlim_t scale = (rlim_t)1024 * (rlim_t)1024;
    const rlim_t limit = (rlim_t)memory_limit_mib * scale;
    if (limit / scale != (rlim_t)memory_limit_mib)
	return false;
    struct rlimit current;
    if (getrlimit(RLIMIT_AS, &current) != 0)
	return false;
    if (current.rlim_max != RLIM_INFINITY && limit > current.rlim_max)
	return false;
    current.rlim_cur = limit;
    return setrlimit(RLIMIT_AS, &current) == 0;
#else
    return false;
#endif
}

static std::string
json_quote(const char *str)
{
    std::ostringstream out;
    out << '"';
    const unsigned char *p = (const unsigned char *)(str ? str : "");
    while (*p) {
	switch (*p) {
	    case '"': out << "\\\""; break;
	    case '\\': out << "\\\\"; break;
	    case '\b': out << "\\b"; break;
	    case '\f': out << "\\f"; break;
	    case '\n': out << "\\n"; break;
	    case '\r': out << "\\r"; break;
	    case '\t': out << "\\t"; break;
	    default:
		if (*p < 0x20) {
		    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
			<< (unsigned int)*p << std::dec << std::setfill(' ');
		} else {
		    out << (char)*p;
		}
	}
	p++;
    }
    out << '"';
    return out.str();
}

static void
bbox_add(point_t bmin, point_t bmax, bool *have_bbox, const point_t p)
{
    if (!*have_bbox) {
	VMOVE(bmin, p);
	VMOVE(bmax, p);
	*have_bbox = true;
	return;
    }
    VMINMAX(bmin, bmax, p);
}

static bool
drawable_vlist_cmd(int cmd)
{
    switch (cmd) {
	case BV_VLIST_LINE_MOVE:
	case BV_VLIST_LINE_DRAW:
	case BV_VLIST_POLY_MOVE:
	case BV_VLIST_POLY_DRAW:
	case BV_VLIST_POLY_END:
	case BV_VLIST_TRI_MOVE:
	case BV_VLIST_TRI_DRAW:
	case BV_VLIST_TRI_END:
	case BV_VLIST_POINT_DRAW:
	    return true;
	default:
	    return false;
    }
}

static struct db_i *
open_db(const char *path)
{
    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (dbip == DBI_NULL)
	return DBI_NULL;
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	return DBI_NULL;
    }
    return dbip;
}

static int
load_brep(struct db_i *dbip, struct directory *dp, struct rt_db_internal *intern)
{
    RT_DB_INTERNAL_INIT(intern);
    if (rt_db_get_internal(intern, dp, dbip, NULL) < 0)
	return BRLCAD_ERROR;
    if (intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	rt_db_free_internal(intern);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static geom_result
wireframe_result(struct db_i *dbip, struct directory *dp,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }

    struct bu_list vhead;
    BU_LIST_INIT(&vhead);
    int64_t start = bu_gettime();
    result.ret = rt_brep_plot_poly(&vhead, dp, &intern, ttol, tol, NULL);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();

    struct bv_vlist *vp;
    for (BU_LIST_FOR(vp, bv_vlist, &vhead)) {
	for (size_t i = 0; i < vp->nused; i++) {
	    result.commands++;
	    int cmd = vp->cmd[i];
	    if (!drawable_vlist_cmd(cmd))
		continue;
	    const point_t &p = vp->pt[i];
	    if (!std::isfinite(p[X]) || !std::isfinite(p[Y]) || !std::isfinite(p[Z])) {
		result.finite = false;
		continue;
	    }
	    result.vertices++;
	    bbox_add(result.bmin, result.bmax, &result.have_bbox, p);
	    if (cmd == BV_VLIST_LINE_DRAW || cmd == BV_VLIST_POLY_DRAW ||
		    cmd == BV_VLIST_POLY_END || cmd == BV_VLIST_TRI_DRAW ||
		    cmd == BV_VLIST_TRI_END || cmd == BV_VLIST_POINT_DRAW)
		result.primitives++;
	}
    }

    if (result.ret != BRLCAD_OK)
	result.issues.push_back("generation_failed");
    if (!result.vertices || !result.primitives)
	result.issues.push_back("empty_geometry");
    if (!result.finite)
	result.issues.push_back("non_finite_coordinates");
    if (!result.have_bbox)
	result.issues.push_back("missing_bbox");

    BV_FREE_VLIST(&rt_vlfree, &vhead);
    rt_db_free_internal(&intern);
    return result;
}

static geom_result
shaded_result(struct db_i *dbip, struct directory *dp,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    int *faces = NULL;
    int face_cnt = 0;
    vect_t *normals = NULL;
    point_t *points = NULL;
    int point_cnt = 0;
    int64_t start = bu_gettime();
    result.ret = brep_cdt_fast(&faces, &face_cnt, &normals, &points,
	    &point_cnt, bi->brep, -1, ttol, tol);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();

    if (face_cnt > 0)
	result.primitives = (size_t)face_cnt;
    if (point_cnt > 0)
	result.vertices = (size_t)point_cnt;
    for (int i = 0; points && i < point_cnt; i++) {
	if (!std::isfinite(points[i][X]) || !std::isfinite(points[i][Y]) ||
		!std::isfinite(points[i][Z])) {
	    result.finite = false;
	    continue;
	}
	bbox_add(result.bmin, result.bmax, &result.have_bbox, points[i]);
    }
    for (int i = 0; faces && i < face_cnt * 3; i++) {
	if (faces[i] < 0 || faces[i] >= point_cnt)
	    result.invalid_indices++;
    }
    for (int i = 0; normals && i < face_cnt * 3; i++) {
	if (!std::isfinite(normals[i][X]) || !std::isfinite(normals[i][Y]) ||
		!std::isfinite(normals[i][Z]))
	    result.finite = false;
    }

    if (result.ret != BRLCAD_OK)
	result.issues.push_back("generation_failed");
    if (!result.vertices || !result.primitives)
	result.issues.push_back("empty_geometry");
    if (result.invalid_indices)
	result.issues.push_back("invalid_face_indices");
    if (!result.finite)
	result.issues.push_back("non_finite_coordinates_or_normals");
    if (!result.have_bbox)
	result.issues.push_back("missing_bbox");

    bu_free(faces, "brep audit faces");
    bu_free(normals, "brep audit normals");
    bu_free(points, "brep audit points");
    rt_db_free_internal(&intern);
    return result;
}

static void
dims(vect_t d, const point_t bmin, const point_t bmax)
{
    VSUB2(d, bmax, bmin);
    d[X] = fabs(d[X]);
    d[Y] = fabs(d[Y]);
    d[Z] = fabs(d[Z]);
}

static void
check_dimensions(geom_result *result, const vect_t ref_dims, double ref_diag,
	double ratio_min, double ratio_max, const char *prefix)
{
    if (!result->have_bbox)
	return;
    vect_t gdims;
    dims(gdims, result->bmin, result->bmax);
    double active_tol = std::max(BN_TOL_DIST, ref_diag * 1.0e-9);
    for (int axis = 0; axis < 3; axis++) {
	if (ref_dims[axis] <= active_tol)
	    continue;
	double ratio = gdims[axis] / ref_dims[axis];
	if (!std::isfinite(ratio) || ratio < ratio_min || ratio > ratio_max) {
	    std::ostringstream issue;
	    issue << prefix << "_bbox_axis_" << "xyz"[axis] << "_ratio_out_of_range";
	    result->issues.push_back(issue.str());
	}
    }
    double gdiag = MAGNITUDE(gdims);
    if (ref_diag > active_tol) {
	double ratio = gdiag / ref_diag;
	if (!std::isfinite(ratio) || ratio < ratio_min || ratio > ratio_max) {
	    std::ostringstream issue;
	    issue << prefix << "_bbox_diagonal_ratio_out_of_range";
	    result->issues.push_back(issue.str());
	}
    }
}

static void
print_num(double value)
{
    if (std::isfinite(value))
	std::cout << std::setprecision(17) << value;
    else
	std::cout << "null";
}

static void
print_vec(const fastf_t *v)
{
    std::cout << "[";
    print_num(v[X]);
    std::cout << ",";
    print_num(v[Y]);
    std::cout << ",";
    print_num(v[Z]);
    std::cout << "]";
}

static void
print_issues(const std::vector<std::string> &issues)
{
    std::cout << "[";
    for (size_t i = 0; i < issues.size(); i++) {
	if (i)
	    std::cout << ",";
	std::cout << json_quote(issues[i].c_str());
    }
    std::cout << "]";
}

static void
print_result(const geom_result &result, const vect_t ref_dims)
{
    vect_t gdims = VINIT_ZERO;
    if (result.have_bbox)
	dims(gdims, result.bmin, result.bmax);
    std::cout << "{\"return_code\":" << result.ret
	<< ",\"seconds\":";
    print_num(result.seconds);
    std::cout << ",\"vertices\":" << result.vertices
	<< ",\"primitives\":" << result.primitives
	<< ",\"commands\":" << result.commands
	<< ",\"peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"invalid_indices\":" << result.invalid_indices
	<< ",\"finite\":" << (result.finite ? "true" : "false")
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (result.have_bbox) print_vec(gdims); else std::cout << "null";
    std::cout << ",\"dimension_ratios\":[";
    for (int axis = 0; axis < 3; axis++) {
	if (axis)
	    std::cout << ",";
	if (result.have_bbox && ref_dims[axis] > BN_TOL_DIST)
	    print_num(gdims[axis] / ref_dims[axis]);
	else
	    std::cout << "null";
    }
    std::cout << "],\"issues\":";
    print_issues(result.issues);
    std::cout << "}";
}

static int
list_breps(const char *db_path)
{
    struct db_i *dbip = open_db(db_path);
    if (dbip == DBI_NULL)
	return 2;
    std::vector<std::string> names;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP)
	    names.push_back(dp->d_namep);
    } FOR_ALL_DIRECTORY_END;
    std::sort(names.begin(), names.end());
    for (const auto &name : names)
	std::cout << name << "\n";
    db_close(dbip);
    return 0;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    argc--; argv++;

    int print_help = 0;
    int list_only = 0;
    double ratio_min = 0.5;
    double ratio_max = 2.0;
    double tess_abs = 0.0;
    double tess_rel = 0.01;
    double tess_norm = 0.0;
    long memory_limit_mib = 0;
    struct bu_opt_desc d[9];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help and exit");
    BU_OPT(d[1], "l", "list", "", NULL, &list_only, "List BRep primitive names");
    BU_OPT(d[2], "", "ratio-min", "#", &bu_opt_fastf_t, &ratio_min, "Minimum acceptable generated/reference dimension ratio");
    BU_OPT(d[3], "", "ratio-max", "#", &bu_opt_fastf_t, &ratio_max, "Maximum acceptable generated/reference dimension ratio");
    BU_OPT(d[4], "", "tess-abs", "#", &bu_opt_fastf_t, &tess_abs, "Absolute shaded tessellation tolerance");
    BU_OPT(d[5], "", "tess-rel", "#", &bu_opt_fastf_t, &tess_rel, "Relative shaded tessellation tolerance");
    BU_OPT(d[6], "", "tess-norm", "#", &bu_opt_fastf_t, &tess_norm, "Normal shaded tessellation tolerance");
    BU_OPT(d[7], "", "memory-limit-mib", "#", &bu_opt_long, &memory_limit_mib, "Process address-space limit in MiB (zero disables)");
    BU_OPT_NULL(d[8]);
    int ac = bu_opt_parse(NULL, argc, argv, d);
    const char *usage = "Usage: brep-audit [options] [--list] file.g [brep]\n";
    if (print_help || (list_only && ac != 1) || (!list_only && ac != 2) ||
	    ratio_min <= 0.0 || ratio_max < ratio_min || tess_abs < 0.0 ||
	    tess_rel < 0.0 || tess_norm < 0.0 || memory_limit_mib < 0) {
	std::cerr << usage;
	return print_help ? 0 : 2;
    }
    if (!set_memory_limit(memory_limit_mib)) {
	std::cerr << "Unable to set memory limit to " << memory_limit_mib << " MiB\n";
	return 2;
    }
    if (!bu_file_exists(argv[0], NULL)) {
	std::cerr << "Database does not exist: " << argv[0] << "\n";
	return 2;
    }
    if (list_only)
	return list_breps(argv[0]);

    struct db_i *dbip = open_db(argv[0]);
    if (dbip == DBI_NULL) {
	std::cerr << "Unable to open database: " << argv[0] << "\n";
	return 2;
    }
    struct directory *dp = db_lookup(dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	std::cerr << "Not a BRep primitive: " << argv[1] << "\n";
	db_close(dbip);
	return 2;
    }

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    ttol.abs = tess_abs;
    ttol.rel = tess_rel;
    ttol.norm = tess_norm;
    std::cerr << "brep-audit: phase=reference" << std::endl;
    point_t ref_min = VINIT_ZERO;
    point_t ref_max = VINIT_ZERO;
    bool ref_valid = false;
    int ref_faces = 0;
    int ref_face_failures = 0;
    std::vector<std::string> top_issues;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) == BRLCAD_OK) {
	struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
	ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
	ref_faces = bi->brep->m_F.Count();
	for (int i = 0; i < ref_faces; i++) {
	    ON_BoundingBox face_bbox = ON_BoundingBox::EmptyBoundingBox;
	    if (!face_GetBoundingBox(bi->brep->m_F[i], face_bbox, false) ||
		    !face_bbox.IsValid()) {
		ref_face_failures++;
		continue;
	    }
	    if (bbox.IsValid())
		bbox.Union(face_bbox);
	    else
		bbox = face_bbox;
	}
	if (bbox.IsValid()) {
	    VMOVE(ref_min, bbox.m_min);
	    VMOVE(ref_max, bbox.m_max);
	    ref_valid = true;
	} else {
	    top_issues.push_back("trimmed_bbox_failed");
	}
	if (ref_face_failures)
	    top_issues.push_back("trimmed_bbox_face_failures");
	rt_db_free_internal(&intern);
    } else {
	top_issues.push_back("database_internal_load_failed");
    }

    std::cerr << "brep-audit: phase=wireframe" << std::endl;
    geom_result wire = wireframe_result(dbip, dp, &ttol, &tol);
    std::cerr << "brep-audit: phase=shaded" << std::endl;
    geom_result shaded = shaded_result(dbip, dp, &ttol, &tol);
    vect_t ref_dims = VINIT_ZERO;
    double ref_diag = 0.0;
    if (ref_valid) {
	dims(ref_dims, ref_min, ref_max);
	ref_diag = MAGNITUDE(ref_dims);
	check_dimensions(&wire, ref_dims, ref_diag, ratio_min, ratio_max, "wireframe");
	check_dimensions(&shaded, ref_dims, ref_diag, ratio_min, ratio_max, "shaded");
    }
    bool okay = ref_valid && top_issues.empty() && wire.issues.empty() && shaded.issues.empty();

    std::cerr << "brep-audit: phase=report" << std::endl;
    std::cout << "{\"format\":\"brlcad-brep-realization-audit-v1\",\"database\":"
	<< json_quote(argv[0]) << ",\"object\":" << json_quote(argv[1])
	<< ",\"status\":" << json_quote(okay ? "ok" : "fail")
	<< ",\"ratio_limits\":[" << std::setprecision(17) << ratio_min << "," << ratio_max << "]"
	<< ",\"tessellation_tolerance\":{\"abs\":" << tess_abs
	<< ",\"rel\":" << tess_rel << ",\"norm\":" << tess_norm << "}"
	<< ",\"memory_limit_mib\":" << memory_limit_mib
	<< ",\"generators\":{\"wireframe\":\"rt_brep_plot_poly\""
	<< ",\"shaded\":\"brep_cdt_fast\"}"
	<< ",\"reference\":{\"method\":\"face_GetBoundingBox_trim_parameter_envelopes\""
	<< ",\"face_count\":" << ref_faces
	<< ",\"failed_faces\":" << ref_face_failures
	<< ",\"bbox_valid\":" << (ref_valid ? "true" : "false")
	<< ",\"bbox_min\":";
    if (ref_valid) print_vec(ref_min); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (ref_valid) print_vec(ref_max); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (ref_valid) print_vec(ref_dims); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (ref_valid) print_num(ref_diag); else std::cout << "null";
    std::cout << "},\"wireframe\":";
    print_result(wire, ref_dims);
    std::cout << ",\"shaded\":";
    print_result(shaded, ref_dims);
    std::cout << ",\"issues\":";
    print_issues(top_issues);
    std::cout << "}\n";

    db_close(dbip);
    return okay ? 0 : 1;
}
