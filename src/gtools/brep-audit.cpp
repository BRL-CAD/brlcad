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
#include "brep/surfacetree.h"
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

struct prep_face_failure {
    int face_index = -1;
    brlcad::SurfaceTree::FailureReason reason =
	brlcad::SurfaceTree::FAILURE_NONE;
    ON_Interval u = ON_Interval(ON_UNSET_VALUE, ON_UNSET_VALUE);
    ON_Interval v = ON_Interval(ON_UNSET_VALUE, ON_UNSET_VALUE);
    int depth = -1;
};

struct prep_result {
    int ret = BRLCAD_ERROR;
    int face_count = 0;
    const char *failure_reason = "database_internal_load";
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    std::vector<prep_face_failure> failures;
};

struct surface_tree_profile {
    prep_face_failure failure;
    int surface_index = -1;
    int loop_count = 0;
    int trim_count = 0;
    double seconds = 0.0;
    size_t nodes = 0;
    size_t leaves = 0;
    size_t curve_leaves = 0;
    int maximum_depth = 0;
    const char *surface_type = "none";
    bool native_nurbs = false;
    bool rational = false;
    int order[2] = {0, 0};
    int cv_count[2] = {0, 0};
    int span_count[2] = {0, 0};
    ON_Interval domain[2];
    int nurb_form_status = 0;
    bool nurb_form_available = false;
    bool nurb_form_rational = false;
    int nurb_form_order[2] = {0, 0};
    int nurb_form_cv_count[2] = {0, 0};
    int nurb_form_span_count[2] = {0, 0};
};

struct surface_tree_result {
    int ret = BRLCAD_ERROR;
    int face_count = 0;
    int depth_limit = BREP_MAX_FT_DEPTH;
    const char *failure_reason = "database_internal_load";
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    std::vector<surface_tree_profile> faces;
};

struct surface_tree_totals {
    size_t objects = 0;
    size_t successful_objects = 0;
    size_t failed_objects = 0;
    size_t faces = 0;
    size_t failed_faces = 0;
    size_t loops = 0;
    size_t trims = 0;
    size_t nodes = 0;
    size_t leaves = 0;
    size_t curve_leaves = 0;
    size_t native_nurbs_faces = 0;
    size_t rational_faces = 0;
    size_t nurb_form_unavailable = 0;
    size_t nurb_form_status_one = 0;
    size_t nurb_form_status_two = 0;
    size_t nurb_form_status_other = 0;
    int maximum_depth = 0;
    size_t maximum_leaves = 0;
    int maximum_leaves_face = -1;
    double maximum_seconds = 0.0;
    int maximum_seconds_face = -1;
    int maximum_order[2] = {0, 0};
    int maximum_cv_count[2] = {0, 0};
    int maximum_span_count[2] = {0, 0};
    int maximum_nurb_form_order[2] = {0, 0};
    int maximum_nurb_form_cv_count[2] = {0, 0};
    int maximum_nurb_form_span_count[2] = {0, 0};
    double seconds = 0.0;
};


static void
surface_tree_node_counts(const brlcad::BBNode *node, int depth,
    size_t &nodes, size_t &leaves, int &maximum_depth)
{
    if (!node)
	return;
    nodes++;
    maximum_depth = std::max(maximum_depth, depth);
    const std::vector<brlcad::BBNode *> &children = node->get_children();
    if (children.empty()) {
	leaves++;
	return;
    }
    for (std::vector<brlcad::BBNode *>::const_iterator child =
	    children.begin(); child != children.end(); ++child)
	surface_tree_node_counts(*child, depth + 1, nodes, leaves,
	    maximum_depth);
}

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

static const char *
surface_tree_failure_name(brlcad::SurfaceTree::FailureReason reason)
{
    switch (reason) {
	case brlcad::SurfaceTree::FAILURE_NONE:
	    return "none";
	case brlcad::SurfaceTree::FAILURE_NULL_SURFACE:
	    return "null_surface";
	case brlcad::SurfaceTree::FAILURE_BOUNDING_BOX:
	    return "bounding_box";
	case brlcad::SurfaceTree::FAILURE_NORMAL_EVALUATION:
	    return "normal_evaluation";
	case brlcad::SurfaceTree::FAILURE_SUBDIVISION:
	    return "subdivision";
    }
    return "unknown";
}

static void
diagnose_surface_tree_failures(struct db_i *dbip, struct directory *dp,
	prep_result *result)
{
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return;
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    result->face_count = bi->brep->m_F.Count();
    for (int i = 0; i < result->face_count; ++i) {
	brlcad::SurfaceTree tree(&bi->brep->m_F[i], true, 8);
	if (tree.Valid())
	    continue;
	prep_face_failure failure;
	failure.face_index = i;
	failure.reason = tree.Failure();
	failure.u = tree.FailureDomain(0);
	failure.v = tree.FailureDomain(1);
	failure.depth = tree.FailureDepth();
	result->failures.push_back(failure);
    }
    rt_db_free_internal(&intern);
}

static prep_result
raytrace_prep_result(struct db_i *dbip, struct directory *dp)
{
    prep_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return result;
    result.failure_reason = "none";
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    const ON_BoundingBox bbox = bi->brep->BoundingBox();
    if (bbox.IsValid()) {
	VMOVE(result.bmin, bbox.m_min);
	VMOVE(result.bmax, bbox.m_max);
	result.have_bbox = true;
    }
    result.face_count = bi->brep->m_F.Count();

    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    struct soltab *stp = (struct soltab *)bu_calloc(1,
	sizeof(struct soltab), "brep audit prep soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    stp->st_id = ID_BREP;
    stp->st_meth = &OBJ[ID_BREP];

    int64_t start = bu_gettime();
    result.ret = rtip ? OBJ[ID_BREP].ft_prep(stp, &intern, rtip) :
	BRLCAD_ERROR;
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    if (result.ret == BRLCAD_OK) {
	VMOVE(result.bmin, stp->st_min);
	VMOVE(result.bmax, stp->st_max);
	result.have_bbox = true;
    }
    if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	stp->st_meth->ft_free(stp);
    bu_free(stp, "brep audit prep soltab");
    if (rtip)
	rt_i_destroy(rtip);
    rt_db_free_internal(&intern);

    if (result.ret != BRLCAD_OK)
	diagnose_surface_tree_failures(dbip, dp, &result);
    if (result.ret != BRLCAD_OK) {
	if (result.face_count == 0)
	    result.failure_reason = "empty_brep";
	else if (!result.failures.empty())
	    result.failure_reason = "surface_tree";
	else
	    result.failure_reason = "unknown_prep";
    }
    return result;
}

static surface_tree_result
surface_tree_profile_result(struct db_i *dbip, struct directory *dp,
    int depth_limit)
{
    surface_tree_result result;
    result.depth_limit = depth_limit;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return result;
    result.failure_reason = "none";
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    const ON_BoundingBox bbox = bi->brep->BoundingBox();
    if (bbox.IsValid()) {
	VMOVE(result.bmin, bbox.m_min);
	VMOVE(result.bmax, bbox.m_max);
	result.have_bbox = true;
    }
    result.face_count = bi->brep->m_F.Count();
    result.ret = result.face_count ? BRLCAD_OK : BRLCAD_ERROR;
    if (!result.face_count)
	result.failure_reason = "empty_brep";
    int64_t total_start = bu_gettime();
    result.faces.reserve((size_t)result.face_count);
    for (int i = 0; i < result.face_count; ++i) {
	const ON_BrepFace &face = bi->brep->m_F[i];
	const ON_Surface *surface = face.SurfaceOf();
	surface_tree_profile profile;
	profile.failure.face_index = i;
	profile.surface_index = face.m_si;
	profile.loop_count = face.LoopCount();
	for (int loop_index = 0; loop_index < profile.loop_count;
		++loop_index)
	    profile.trim_count += face.Loop(loop_index)->TrimCount();
	if (surface) {
	    const ON_ClassId *class_id = surface->ClassId();
	    profile.surface_type = class_id ? class_id->ClassName() :
		"unknown";
	    profile.domain[0] = surface->Domain(0);
	    profile.domain[1] = surface->Domain(1);
	    const ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(surface);
	    if (nurbs) {
		profile.native_nurbs = true;
		profile.rational = nurbs->IsRational();
		for (int direction = 0; direction < 2; ++direction) {
		    profile.order[direction] = nurbs->Order(direction);
		    profile.cv_count[direction] = nurbs->CVCount(direction);
		    profile.span_count[direction] = nurbs->SpanCount(direction);
		}
	    }
	    ON_NurbsSurface nurb_form;
	    profile.nurb_form_status = surface->GetNurbForm(nurb_form);
	    if (profile.nurb_form_status > 0) {
		profile.nurb_form_available = true;
		profile.nurb_form_rational = nurb_form.IsRational();
		for (int direction = 0; direction < 2; ++direction) {
		    profile.nurb_form_order[direction] =
			nurb_form.Order(direction);
		    profile.nurb_form_cv_count[direction] =
			nurb_form.CVCount(direction);
		    profile.nurb_form_span_count[direction] =
			nurb_form.SpanCount(direction);
		}
	    }
	}
	int64_t start = bu_gettime();
	brlcad::SurfaceTree tree(&face, true, depth_limit);
	profile.seconds = (bu_gettime() - start) / 1000000.0;
	if (tree.Valid()) {
	    surface_tree_node_counts(tree.getRootNode(), 0, profile.nodes,
		profile.leaves, profile.maximum_depth);
	    if (tree.m_ctree) {
		std::list<const brlcad::BRNode *> curve_leaves;
		tree.m_ctree->getLeaves(curve_leaves);
		profile.curve_leaves = curve_leaves.size();
	    }
	} else {
	    result.ret = BRLCAD_ERROR;
	    result.failure_reason = "surface_tree";
	    profile.failure.reason = tree.Failure();
	    profile.failure.u = tree.FailureDomain(0);
	    profile.failure.v = tree.FailureDomain(1);
	    profile.failure.depth = tree.FailureDepth();
	}
	result.faces.push_back(profile);
    }
    result.seconds = (bu_gettime() - total_start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    rt_db_free_internal(&intern);
    return result;
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

static void
print_interval(const ON_Interval &interval)
{
    std::cout << "[";
    print_num(interval.Min());
    std::cout << ",";
    print_num(interval.Max());
    std::cout << "]";
}

static void
print_prep_result(const char *db_path, const char *object,
	const prep_result &result)
{
    std::cout << "{\"format\":\"brlcad-brep-ray-prep-audit-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(object)
	<< ",\"status\":" << json_quote(result.ret == BRLCAD_OK ?
	    "ok" : "fail")
	<< ",\"return_code\":" << result.ret
	<< ",\"failure_reason\":" << json_quote(result.failure_reason)
	<< ",\"seconds\":";
    print_num(result.seconds);
    std::cout << ",\"peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"face_count\":" << result.face_count
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"surface_tree_failures\":[";
    for (size_t i = 0; i < result.failures.size(); ++i) {
	if (i)
	    std::cout << ",";
	const prep_face_failure &failure = result.failures[i];
	std::cout << "{\"face_index\":" << failure.face_index
	    << ",\"reason\":" << json_quote(
		surface_tree_failure_name(failure.reason))
	    << ",\"depth\":" << failure.depth << ",\"u\":";
	print_interval(failure.u);
	std::cout << ",\"v\":";
	print_interval(failure.v);
	std::cout << "}";
    }
    std::cout << "]}\n";
}

static surface_tree_totals
surface_tree_collect_totals(const surface_tree_result &result)
{
    surface_tree_totals totals;
    totals.objects = 1;
    totals.successful_objects = result.ret == BRLCAD_OK ? 1 : 0;
    totals.failed_objects = result.ret == BRLCAD_OK ? 0 : 1;
    totals.faces = result.faces.size();
    totals.seconds = result.seconds;
    for (size_t i = 0; i < result.faces.size(); ++i) {
	const surface_tree_profile &profile = result.faces[i];
	if (profile.failure.reason != brlcad::SurfaceTree::FAILURE_NONE)
	    totals.failed_faces++;
	totals.loops += (size_t)std::max(0, profile.loop_count);
	totals.trims += (size_t)std::max(0, profile.trim_count);
	totals.nodes += profile.nodes;
	totals.leaves += profile.leaves;
	totals.curve_leaves += profile.curve_leaves;
	totals.native_nurbs_faces += profile.native_nurbs ? 1 : 0;
	totals.rational_faces += profile.rational ||
	    profile.nurb_form_rational ? 1 : 0;
	if (profile.nurb_form_status <= 0)
	    totals.nurb_form_unavailable++;
	else if (profile.nurb_form_status == 1)
	    totals.nurb_form_status_one++;
	else if (profile.nurb_form_status == 2)
	    totals.nurb_form_status_two++;
	else
	    totals.nurb_form_status_other++;
	totals.maximum_depth = std::max(totals.maximum_depth,
	    profile.maximum_depth);
	if (profile.leaves > totals.maximum_leaves) {
	    totals.maximum_leaves = profile.leaves;
	    totals.maximum_leaves_face = profile.failure.face_index;
	}
	if (profile.seconds > totals.maximum_seconds) {
	    totals.maximum_seconds = profile.seconds;
	    totals.maximum_seconds_face = profile.failure.face_index;
	}
	for (int direction = 0; direction < 2; ++direction) {
	    totals.maximum_order[direction] = std::max(
		totals.maximum_order[direction], profile.order[direction]);
	    totals.maximum_cv_count[direction] = std::max(
		totals.maximum_cv_count[direction], profile.cv_count[direction]);
	    totals.maximum_span_count[direction] = std::max(
		totals.maximum_span_count[direction],
		profile.span_count[direction]);
	    totals.maximum_nurb_form_order[direction] = std::max(
		totals.maximum_nurb_form_order[direction],
		profile.nurb_form_order[direction]);
	    totals.maximum_nurb_form_cv_count[direction] = std::max(
		totals.maximum_nurb_form_cv_count[direction],
		profile.nurb_form_cv_count[direction]);
	    totals.maximum_nurb_form_span_count[direction] = std::max(
		totals.maximum_nurb_form_span_count[direction],
		profile.nurb_form_span_count[direction]);
	}
    }
    return totals;
}

static void
surface_tree_add_totals(surface_tree_totals &aggregate,
    const surface_tree_totals &object)
{
    aggregate.objects += object.objects;
    aggregate.successful_objects += object.successful_objects;
    aggregate.failed_objects += object.failed_objects;
    aggregate.faces += object.faces;
    aggregate.failed_faces += object.failed_faces;
    aggregate.loops += object.loops;
    aggregate.trims += object.trims;
    aggregate.nodes += object.nodes;
    aggregate.leaves += object.leaves;
    aggregate.curve_leaves += object.curve_leaves;
    aggregate.native_nurbs_faces += object.native_nurbs_faces;
    aggregate.rational_faces += object.rational_faces;
    aggregate.nurb_form_unavailable += object.nurb_form_unavailable;
    aggregate.nurb_form_status_one += object.nurb_form_status_one;
    aggregate.nurb_form_status_two += object.nurb_form_status_two;
    aggregate.nurb_form_status_other += object.nurb_form_status_other;
    aggregate.maximum_depth = std::max(aggregate.maximum_depth,
	object.maximum_depth);
    aggregate.maximum_leaves = std::max(aggregate.maximum_leaves,
	object.maximum_leaves);
    aggregate.maximum_seconds = std::max(aggregate.maximum_seconds,
	object.maximum_seconds);
    aggregate.seconds += object.seconds;
    for (int direction = 0; direction < 2; ++direction) {
	aggregate.maximum_order[direction] = std::max(
	    aggregate.maximum_order[direction], object.maximum_order[direction]);
	aggregate.maximum_cv_count[direction] = std::max(
	    aggregate.maximum_cv_count[direction],
	    object.maximum_cv_count[direction]);
	aggregate.maximum_span_count[direction] = std::max(
	    aggregate.maximum_span_count[direction],
	    object.maximum_span_count[direction]);
	aggregate.maximum_nurb_form_order[direction] = std::max(
	    aggregate.maximum_nurb_form_order[direction],
	    object.maximum_nurb_form_order[direction]);
	aggregate.maximum_nurb_form_cv_count[direction] = std::max(
	    aggregate.maximum_nurb_form_cv_count[direction],
	    object.maximum_nurb_form_cv_count[direction]);
	aggregate.maximum_nurb_form_span_count[direction] = std::max(
	    aggregate.maximum_nurb_form_span_count[direction],
	    object.maximum_nurb_form_span_count[direction]);
    }
}

static void
print_surface_tree_totals(const surface_tree_totals &totals)
{
    std::cout << "{\"objects\":" << totals.objects
	<< ",\"successful_objects\":" << totals.successful_objects
	<< ",\"failed_objects\":" << totals.failed_objects
	<< ",\"faces\":" << totals.faces
	<< ",\"failed_faces\":" << totals.failed_faces
	<< ",\"loops\":" << totals.loops
	<< ",\"trims\":" << totals.trims
	<< ",\"nodes\":" << totals.nodes
	<< ",\"leaves\":" << totals.leaves
	<< ",\"curve_leaves\":" << totals.curve_leaves
	<< ",\"native_nurbs_faces\":" << totals.native_nurbs_faces
	<< ",\"rational_faces\":" << totals.rational_faces
	<< ",\"nurb_form_status_counts\":{\"unavailable\":"
	<< totals.nurb_form_unavailable << ",\"exact\":"
	<< totals.nurb_form_status_one << ",\"reparameterized\":"
	<< totals.nurb_form_status_two << ",\"other\":"
	<< totals.nurb_form_status_other << "}"
	<< ",\"maximum_depth\":" << totals.maximum_depth
	<< ",\"maximum_leaves\":" << totals.maximum_leaves
	<< ",\"maximum_face_seconds\":";
    print_num(totals.maximum_seconds);
    std::cout << ",\"maximum_order\":[" << totals.maximum_order[0]
	<< "," << totals.maximum_order[1] << "]"
	<< ",\"maximum_cv_count\":[" << totals.maximum_cv_count[0]
	<< "," << totals.maximum_cv_count[1] << "]"
	<< ",\"maximum_span_count\":[" << totals.maximum_span_count[0]
	<< "," << totals.maximum_span_count[1] << "]"
	<< ",\"maximum_nurb_form_order\":["
	<< totals.maximum_nurb_form_order[0] << ","
	<< totals.maximum_nurb_form_order[1] << "]"
	<< ",\"maximum_nurb_form_cv_count\":["
	<< totals.maximum_nurb_form_cv_count[0] << ","
	<< totals.maximum_nurb_form_cv_count[1] << "]"
	<< ",\"maximum_nurb_form_span_count\":["
	<< totals.maximum_nurb_form_span_count[0] << ","
	<< totals.maximum_nurb_form_span_count[1] << "]"
	<< ",\"seconds\":";
    print_num(totals.seconds);
    std::cout << "}";
}

static void
print_surface_tree_summary(const char *object,
    const surface_tree_result &result)
{
    const surface_tree_totals totals = surface_tree_collect_totals(result);
    vect_t dimensions = VINIT_ZERO;
    if (result.have_bbox)
	dims(dimensions, result.bmin, result.bmax);
    std::cout << "{\"object\":" << json_quote(object)
	<< ",\"status\":" << json_quote(result.ret == BRLCAD_OK ?
	    "ok" : "fail")
	<< ",\"return_code\":" << result.ret
	<< ",\"failure_reason\":" << json_quote(result.failure_reason)
	<< ",\"adaptive_depth_limit\":" << result.depth_limit
	<< ",\"process_peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (result.have_bbox) print_vec(dimensions); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (result.have_bbox) print_num(MAGNITUDE(dimensions));
    else std::cout << "null";
    std::cout << ",\"maximum_leaves_face\":"
	<< totals.maximum_leaves_face
	<< ",\"maximum_seconds_face\":"
	<< totals.maximum_seconds_face
	<< ",\"face_failures\":[";
    bool first_failure = true;
    for (size_t face_index = 0; face_index < result.faces.size();
	    ++face_index) {
	const prep_face_failure &failure = result.faces[face_index].failure;
	if (failure.reason == brlcad::SurfaceTree::FAILURE_NONE)
	    continue;
	if (!first_failure)
	    std::cout << ",";
	first_failure = false;
	std::cout << "{\"face_index\":" << failure.face_index
	    << ",\"reason\":" << json_quote(
		surface_tree_failure_name(failure.reason))
	    << ",\"depth\":" << failure.depth << ",\"u\":";
	print_interval(failure.u);
	std::cout << ",\"v\":";
	print_interval(failure.v);
	std::cout << "}";
    }
    std::cout << "]";
    std::cout << ",\"totals\":";
    print_surface_tree_totals(totals);
    std::cout << "}";
}

static void
print_surface_tree_result(const char *db_path, const char *object,
	const surface_tree_result &result)
{
    size_t total_nodes = 0;
    size_t total_leaves = 0;
    size_t total_curve_leaves = 0;
    int maximum_depth = 0;
    size_t maximum_leaves = 0;
    int maximum_leaves_face = -1;
    double maximum_seconds = 0.0;
    int maximum_seconds_face = -1;
    size_t nurb_form_unavailable = 0;
    size_t nurb_form_status_one = 0;
    size_t nurb_form_status_two = 0;
    size_t nurb_form_status_other = 0;
    for (size_t i = 0; i < result.faces.size(); ++i) {
	const surface_tree_profile &profile = result.faces[i];
	if (profile.nurb_form_status <= 0)
	    nurb_form_unavailable++;
	else if (profile.nurb_form_status == 1)
	    nurb_form_status_one++;
	else if (profile.nurb_form_status == 2)
	    nurb_form_status_two++;
	else
	    nurb_form_status_other++;
	total_nodes += profile.nodes;
	total_leaves += profile.leaves;
	total_curve_leaves += profile.curve_leaves;
	maximum_depth = std::max(maximum_depth, profile.maximum_depth);
	if (profile.leaves > maximum_leaves) {
	    maximum_leaves = profile.leaves;
	    maximum_leaves_face = profile.failure.face_index;
	}
	if (profile.seconds > maximum_seconds) {
	    maximum_seconds = profile.seconds;
	    maximum_seconds_face = profile.failure.face_index;
	}
    }
    vect_t dimensions = VINIT_ZERO;
    if (result.have_bbox)
	dims(dimensions, result.bmin, result.bmax);
    std::cout << "{\"format\":\"brlcad-brep-surface-tree-audit-v2\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(object)
	<< ",\"status\":" << json_quote(result.ret == BRLCAD_OK ?
	    "ok" : "fail")
	<< ",\"return_code\":" << result.ret
	<< ",\"failure_reason\":" << json_quote(result.failure_reason)
	<< ",\"seconds\":";
    print_num(result.seconds);
    std::cout << ",\"peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"face_count\":" << result.face_count
	<< ",\"depth_limit\":" << result.depth_limit
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (result.have_bbox) print_vec(dimensions); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (result.have_bbox) print_num(MAGNITUDE(dimensions));
    else std::cout << "null";
    std::cout
	<< ",\"maximum_depth\":" << maximum_depth
	<< ",\"total_nodes\":" << total_nodes
	<< ",\"total_leaves\":" << total_leaves
	<< ",\"total_curve_leaves\":" << total_curve_leaves
	<< ",\"maximum_leaves\":" << maximum_leaves
	<< ",\"maximum_leaves_face\":" << maximum_leaves_face
	<< ",\"nurb_form_status_counts\":{\"unavailable\":"
	<< nurb_form_unavailable << ",\"exact\":" << nurb_form_status_one
	<< ",\"reparameterized\":" << nurb_form_status_two
	<< ",\"other\":" << nurb_form_status_other << "}"
	<< ",\"maximum_face_seconds\":";
    print_num(maximum_seconds);
    std::cout << ",\"maximum_seconds_face\":" << maximum_seconds_face
	<< ",\"faces\":[";
    for (size_t i = 0; i < result.faces.size(); ++i) {
	if (i)
	    std::cout << ",";
	const surface_tree_profile &profile = result.faces[i];
	std::cout << "{\"face_index\":" << profile.failure.face_index
	    << ",\"surface_index\":" << profile.surface_index
	    << ",\"loop_count\":" << profile.loop_count
	    << ",\"trim_count\":" << profile.trim_count
	    << ",\"status\":" << json_quote(
		profile.failure.reason == brlcad::SurfaceTree::FAILURE_NONE ?
		"ok" : "fail")
	    << ",\"seconds\":";
	print_num(profile.seconds);
	std::cout << ",\"nodes\":" << profile.nodes
	    << ",\"leaves\":" << profile.leaves
	    << ",\"curve_leaves\":" << profile.curve_leaves
	    << ",\"maximum_depth\":" << profile.maximum_depth
	    << ",\"surface_type\":" << json_quote(profile.surface_type)
	    << ",\"native_nurbs\":" <<
		(profile.native_nurbs ? "true" : "false")
	    << ",\"rational\":" << (profile.rational ? "true" : "false")
	    << ",\"order\":[" << profile.order[0] << ","
	    << profile.order[1] << "]"
	    << ",\"cv_count\":[" << profile.cv_count[0] << ","
	    << profile.cv_count[1] << "]"
	    << ",\"span_count\":[" << profile.span_count[0] << ","
	    << profile.span_count[1] << "]"
	    << ",\"nurb_form_status\":" << profile.nurb_form_status
	    << ",\"nurb_form_available\":" <<
		(profile.nurb_form_available ? "true" : "false")
	    << ",\"nurb_form_rational\":" <<
		(profile.nurb_form_rational ? "true" : "false")
	    << ",\"nurb_form_order\":[" << profile.nurb_form_order[0]
	    << "," << profile.nurb_form_order[1] << "]"
	    << ",\"nurb_form_cv_count\":[" <<
		profile.nurb_form_cv_count[0] << "," <<
		profile.nurb_form_cv_count[1] << "]"
	    << ",\"nurb_form_span_count\":[" <<
		profile.nurb_form_span_count[0] << "," <<
		profile.nurb_form_span_count[1] << "]"
	    << ",\"domain\":[";
	print_interval(profile.domain[0]);
	std::cout << ",";
	print_interval(profile.domain[1]);
	std::cout << "]";
	if (profile.failure.reason != brlcad::SurfaceTree::FAILURE_NONE) {
	    std::cout << ",\"failure_reason\":" << json_quote(
		surface_tree_failure_name(profile.failure.reason))
		<< ",\"failure_depth\":" << profile.failure.depth
		<< ",\"failure_u\":";
	    print_interval(profile.failure.u);
	    std::cout << ",\"failure_v\":";
	    print_interval(profile.failure.v);
	}
	std::cout << "}";
    }
    std::cout << "]}\n";
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

static int
profile_all_surface_trees(struct db_i *dbip, const char *db_path,
    int depth_limit)
{
    if (dbip == DBI_NULL || !db_path)
	return 2;
    std::vector<std::string> names;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP)
	    names.push_back(dp->d_namep);
    } FOR_ALL_DIRECTORY_END;
    std::sort(names.begin(), names.end());

    surface_tree_totals aggregate;
    size_t failed_objects = 0;
    std::cout << "{\"format\":"
	<< "\"brlcad-brep-surface-tree-database-summary-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"database_file_bytes\":" << bu_file_size(db_path)
	<< ",\"adaptive_depth_limit\":" << depth_limit
	<< ",\"objects\":[";
    for (size_t object_index = 0; object_index < names.size();
	    ++object_index) {
	if (object_index)
	    std::cout << ",";
	dp = db_lookup(dbip, names[object_index].c_str(), LOOKUP_QUIET);
	surface_tree_result result;
	if (dp != RT_DIR_NULL)
	    result = surface_tree_profile_result(dbip, dp, depth_limit);
	print_surface_tree_summary(names[object_index].c_str(), result);
	const surface_tree_totals totals = surface_tree_collect_totals(result);
	surface_tree_add_totals(aggregate, totals);
	failed_objects += result.ret == BRLCAD_OK ? 0 : 1;
    }
    std::cout << "],\"totals\":";
    print_surface_tree_totals(aggregate);
    std::cout << ",\"process_peak_rss_bytes\":" << peak_rss_bytes()
	<< "}\n";
    if (names.empty())
	return 1;
    return failed_objects ? 1 : 0;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    argc--; argv++;

    int print_help = 0;
    int list_only = 0;
    int prep_only = 0;
    int surface_trees_only = 0;
    int surface_trees_all = 0;
    double ratio_min = 0.5;
    double ratio_max = 2.0;
    double tess_abs = 0.0;
    double tess_rel = 0.01;
    double tess_norm = 0.0;
    long memory_limit_mib = 0;
    long surface_tree_depth = BREP_MAX_FT_DEPTH;
    struct bu_opt_desc d[13];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help and exit");
    BU_OPT(d[1], "l", "list", "", NULL, &list_only, "List BRep primitive names");
    BU_OPT(d[2], "", "ratio-min", "#", &bu_opt_fastf_t, &ratio_min, "Minimum acceptable generated/reference dimension ratio");
    BU_OPT(d[3], "", "ratio-max", "#", &bu_opt_fastf_t, &ratio_max, "Maximum acceptable generated/reference dimension ratio");
    BU_OPT(d[4], "", "tess-abs", "#", &bu_opt_fastf_t, &tess_abs, "Absolute shaded tessellation tolerance");
    BU_OPT(d[5], "", "tess-rel", "#", &bu_opt_fastf_t, &tess_rel, "Relative shaded tessellation tolerance");
    BU_OPT(d[6], "", "tess-norm", "#", &bu_opt_fastf_t, &tess_norm, "Normal shaded tessellation tolerance");
    BU_OPT(d[7], "", "memory-limit-mib", "#", &bu_opt_long, &memory_limit_mib, "Process address-space limit in MiB (zero disables)");
    BU_OPT(d[8], "", "prep-only", "", NULL, &prep_only, "Audit raytrace preparation only");
    BU_OPT(d[9], "", "surface-trees-only", "", NULL, &surface_trees_only, "Profile raytrace SurfaceTrees serially");
    BU_OPT(d[10], "", "surface-trees-all", "", NULL, &surface_trees_all, "Summarize SurfaceTrees for every BREP in a database");
    BU_OPT(d[11], "", "surface-tree-depth", "#", &bu_opt_long, &surface_tree_depth, "Adaptive SurfaceTree depth after structural splits");
    BU_OPT_NULL(d[12]);
    int ac = bu_opt_parse(NULL, argc, argv, d);
    const char *usage = "Usage: brep-audit [options] "
	"[--list|--prep-only|--surface-trees-only|--surface-trees-all] "
	"file.g [brep]\n";
    const bool database_only = list_only || surface_trees_all;
    if (print_help || (database_only && ac != 1) ||
	    (!database_only && ac != 2) ||
	    ((list_only ? 1 : 0) + (prep_only ? 1 : 0) +
	     (surface_trees_only ? 1 : 0) +
	     (surface_trees_all ? 1 : 0) > 1) ||
	    ratio_min <= 0.0 || ratio_max < ratio_min || tess_abs < 0.0 ||
	    tess_rel < 0.0 || tess_norm < 0.0 || memory_limit_mib < 0 ||
	    surface_tree_depth < 0 ||
	    surface_tree_depth > BREP_MAX_FT_DEPTH ||
	    (!surface_trees_only && !surface_trees_all &&
	     surface_tree_depth != BREP_MAX_FT_DEPTH)) {
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
    if (surface_trees_all) {
	const int ret = profile_all_surface_trees(dbip, argv[0],
	    (int)surface_tree_depth);
	db_close(dbip);
	return ret;
    }
    struct directory *dp = db_lookup(dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	std::cerr << "Not a BRep primitive: " << argv[1] << "\n";
	db_close(dbip);
	return 2;
    }

    if (prep_only) {
	prep_result prep = raytrace_prep_result(dbip, dp);
	print_prep_result(argv[0], argv[1], prep);
	db_close(dbip);
	return prep.ret == BRLCAD_OK ? 0 : 1;
    }

    if (surface_trees_only) {
	surface_tree_result trees = surface_tree_profile_result(dbip, dp,
	    (int)surface_tree_depth);
	print_surface_tree_result(argv[0], argv[1], trees);
	db_close(dbip);
	return trees.ret == BRLCAD_OK ? 0 : 1;
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
