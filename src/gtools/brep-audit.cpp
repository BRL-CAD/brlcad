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
 * Isolated checks for BRep wireframes, display meshes, and certified meshes.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef __GLIBC__
#  include <malloc.h>
#endif

#ifndef _WIN32
#  include <sys/resource.h>
#endif

#include "bu/app.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/datetime.h"
#include "bg/trimesh.h"
#include "bv/vlist.h"
#include "brep/cdt.h"
#include "brep/util.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"
#include "rt/vlist.h"

struct face_failure {
    int face_index = -1;
    int result = 0;
    int stage = 0;
    std::string message;
};

struct geom_result {
    int ret = BRLCAD_ERROR;
    size_t vertices = 0;
    size_t primitives = 0;
    size_t commands = 0;
    size_t invalid_indices = 0;
    int diagnostic_result = 0;
    int diagnostic_stage = 0;
    int diagnostic_face = -1;
    std::string diagnostic_message;
    bool solid_checked = false;
    bool solid = false;
    int degenerate_faces = 0;
    int unmatched_edges = 0;
    int excess_edges = 0;
    int misoriented_edges = 0;
    size_t unique_edges = 0;
    int connected_components = 0;
    int invalid_vertex_links = 0;
    int geometric_degenerate_faces = 0;
    long long euler_characteristic = 0;
    double minimum_angle_degrees =
	std::numeric_limits<double>::quiet_NaN();
    double maximum_aspect_ratio =
	std::numeric_limits<double>::quiet_NaN();
    int requested_items = 0;
    int completed_items = 0;
    int failed_items = 0;
    int skipped_items = 0;
    bool hit_time_limit = false;
    bool hit_memory_limit = false;
    bool hit_point_limit = false;
    bool finite = true;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    std::vector<int> failed_faces;
    std::vector<face_failure> face_failures;
    std::vector<int> skipped_faces;
    std::vector<int> unprocessed_faces;
    std::vector<int> failed_edges;
    std::vector<int> unprocessed_edges;
    std::vector<int> failed_surface_cues;
    std::vector<int> unprocessed_surface_cues;
    size_t omitted_failed_faces = 0;
    size_t omitted_skipped_faces = 0;
    size_t omitted_unprocessed_faces = 0;
    size_t omitted_failed_edges = 0;
    size_t omitted_unprocessed_edges = 0;
    size_t omitted_failed_surface_cues = 0;
    size_t omitted_unprocessed_surface_cues = 0;
    std::vector<std::string> issues;
};

static const size_t MAX_REPORTED_ITEM_INDICES = 16384;

static void
capture_index(std::vector<int> *indices, size_t *omitted, int index)
{
    if (indices->size() < MAX_REPORTED_ITEM_INDICES)
	indices->push_back(index);
    else
	(*omitted)++;
}

static void
wire_item_status(int item_type, int item_index, int status, void *data)
{
    geom_result *result = (geom_result *)data;
    if (status == RT_BREP_DRAW_ITEM_COMPLETED)
	return;
    if (item_type == RT_BREP_DRAW_EDGE) {
	if (status == RT_BREP_DRAW_ITEM_FAILED)
	    capture_index(&result->failed_edges,
		&result->omitted_failed_edges, item_index);
	else
	    capture_index(&result->unprocessed_edges,
		&result->omitted_unprocessed_edges, item_index);
	return;
    }
    if (status == RT_BREP_DRAW_ITEM_FAILED)
	capture_index(&result->failed_surface_cues,
	    &result->omitted_failed_surface_cues, item_index);
    else
	capture_index(&result->unprocessed_surface_cues,
	    &result->omitted_unprocessed_surface_cues, item_index);
}

static void
shaded_face_status(int face_index, int status, void *data)
{
    geom_result *result = (geom_result *)data;
    switch (status) {
	case BREP_CDT_FAST_FACE_FAILED:
	    capture_index(&result->failed_faces,
		&result->omitted_failed_faces, face_index);
	    break;
	case BREP_CDT_FAST_FACE_SKIPPED_DEGENERATE:
	    capture_index(&result->skipped_faces,
		&result->omitted_skipped_faces, face_index);
	    break;
	case BREP_CDT_FAST_FACE_NOT_PROCESSED:
	    capture_index(&result->unprocessed_faces,
		&result->omitted_unprocessed_faces, face_index);
	    break;
	default:
	    break;
    }
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
face_boundary_bbox(const ON_BrepFace &face, ON_BoundingBox &bbox)
{
    bbox = ON_BoundingBox::EmptyBoundingBox;
    const auto include_point = [&](const ON_3dPoint &point) {
	if (!point.IsValid())
	    return;
	if (bbox.IsValid())
	    bbox.Set(point, true);
	else
	    bbox = ON_BoundingBox(point, point);
    };
    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	if (!loop)
	    continue;
	for (int trim_index = 0; trim_index < loop->TrimCount(); ++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    if (!trim)
		continue;
	    const ON_BrepVertex *first = trim->Vertex(0);
	    const ON_BrepVertex *second = trim->Vertex(1);
	    if (first)
		include_point(first->Point());
	    if (second)
		include_point(second->Point());
	    const ON_BrepEdge *edge = trim->Edge();
	    ON_BoundingBox edge_bbox = ON_BoundingBox::EmptyBoundingBox;
	    if (edge && edge->GetTightBoundingBox(edge_bbox) &&
		    edge_bbox.IsValid()) {
		if (bbox.IsValid())
		    bbox.Union(edge_bbox);
		else
		    bbox = edge_bbox;
	    }
	}
    }
    return bbox.IsValid();
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
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const struct rt_brep_draw_options *options)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }

    struct bu_list vhead;
    BU_LIST_INIT(&vhead);
    struct rt_brep_draw_options active_options = *options;
    active_options.item_status = wire_item_status;
    active_options.item_status_data = &result;
    int64_t start = bu_gettime();
    struct rt_brep_draw_report report = {};
    result.ret = rt_brep_plot_ex(&vhead, &intern, ttol, tol, NULL,
	&active_options, &report);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    result.requested_items = report.requested_edges +
	report.requested_surface_cues;
    result.completed_items = report.completed_edges +
	report.completed_surface_cues;
    result.failed_items = report.failed_edges +
	report.requested_surface_cues - report.completed_surface_cues;
    result.hit_time_limit = report.hit_time_limit;
    result.hit_memory_limit = report.hit_memory_limit;
    result.hit_point_limit = report.hit_point_limit;

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

    if (result.ret == RT_BREP_DRAW_PARTIAL)
	result.issues.push_back("partial_geometry");
    else if (result.ret == RT_BREP_DRAW_LIMIT)
	result.issues.push_back("resource_limit");
    else if (result.ret != RT_BREP_DRAW_OK)
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
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const struct brep_cdt_fast_options *options, int face_index)
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
    struct brep_cdt_fast_options active_options = *options;
    active_options.face_status = shaded_face_status;
    active_options.face_status_data = &result;
    int64_t start = bu_gettime();
    struct brep_cdt_fast_report report = {};
    result.ret = brep_cdt_fast_ex(&faces, &face_cnt, &normals, &points,
	    &point_cnt, bi->brep, face_index, ttol, tol, &active_options,
	    &report);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    result.requested_items = report.requested_faces;
    result.completed_items = report.completed_faces;
    result.failed_items = report.failed_faces;
    result.skipped_items = report.skipped_degenerate_faces;
    result.hit_time_limit = report.hit_time_limit;
    result.hit_memory_limit = report.hit_memory_limit;
    result.hit_point_limit = report.hit_point_limit;

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

    if (result.ret == BREP_CDT_FAST_PARTIAL)
	result.issues.push_back("partial_geometry");
    else if (result.ret == BREP_CDT_FAST_LIMIT)
	result.issues.push_back("resource_limit");
    else if (result.ret != BREP_CDT_FAST_OK)
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
mesh_quality_metrics(geom_result *result, int vertex_count, int face_count,
	const fastf_t *vertices, const int *faces, bool require_closed_links)
{
    if (!result || vertex_count <= 0 || face_count <= 0 || !vertices ||
	    !faces)
	return;
    typedef std::pair<int, int> mesh_edge;
    std::map<mesh_edge, std::vector<int>> edge_faces;
    std::vector<std::vector<int>> vertex_faces((size_t)vertex_count);
    std::vector<std::vector<int>> face_neighbors((size_t)face_count);
    double minimum_angle = std::numeric_limits<double>::infinity();
    double maximum_aspect = 0.0;
    for (int fi = 0; fi < face_count; ++fi) {
	const int *triangle = &faces[3 * fi];
	point_t p[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = triangle[corner];
	    if (vertex < 0 || vertex >= vertex_count)
		return;
	    VSET(p[corner], vertices[3 * vertex],
		vertices[3 * vertex + 1], vertices[3 * vertex + 2]);
	    vertex_faces[(size_t)vertex].push_back(fi);
	    const int other = triangle[(corner + 1) % 3];
	    edge_faces[vertex < other ? mesh_edge(vertex, other) :
		mesh_edge(other, vertex)].push_back(fi);
	}
	vect_t ab;
	vect_t ac;
	VSUB2(ab, p[1], p[0]);
	VSUB2(ac, p[2], p[0]);
	vect_t cross;
	VCROSS(cross, ab, ac);
	const double doubled_area = MAGNITUDE(cross);
	double lengths[3] = {
	    DIST_PNT_PNT(p[0], p[1]),
	    DIST_PNT_PNT(p[1], p[2]),
	    DIST_PNT_PNT(p[2], p[0])
	};
	const double longest = std::max(lengths[0],
	    std::max(lengths[1], lengths[2]));
	if (!(longest > 0.0) || doubled_area <=
		64.0 * std::numeric_limits<double>::epsilon() *
		longest * longest) {
	    result->geometric_degenerate_faces++;
	    continue;
	}
	maximum_aspect = std::max(maximum_aspect,
	    longest * longest / doubled_area);
	for (int corner = 0; corner < 3; ++corner) {
	    const double adjacent_a = lengths[corner];
	    const double adjacent_b = lengths[(corner + 2) % 3];
	    const double opposite = lengths[(corner + 1) % 3];
	    double cosine = (adjacent_a * adjacent_a +
		adjacent_b * adjacent_b - opposite * opposite) /
		(2.0 * adjacent_a * adjacent_b);
	    cosine = std::max(-1.0, std::min(1.0, cosine));
	    minimum_angle = std::min(minimum_angle, acos(cosine) *
		180.0 / M_PI);
	}
    }
    result->unique_edges = edge_faces.size();
    result->euler_characteristic = (long long)vertex_count -
	(long long)result->unique_edges + (long long)face_count;
    if (std::isfinite(minimum_angle))
	result->minimum_angle_degrees = minimum_angle;
    if (maximum_aspect > 0.0)
	result->maximum_aspect_ratio = maximum_aspect;

    for (const auto &entry : edge_faces) {
	const std::vector<int> &incident = entry.second;
	for (size_t i = 0; i < incident.size(); ++i) {
	    for (size_t j = i + 1; j < incident.size(); ++j) {
		face_neighbors[(size_t)incident[i]].push_back(incident[j]);
		face_neighbors[(size_t)incident[j]].push_back(incident[i]);
	    }
	}
    }
    std::vector<bool> visited_faces((size_t)face_count, false);
    for (int seed = 0; seed < face_count; ++seed) {
	if (visited_faces[(size_t)seed])
	    continue;
	result->connected_components++;
	std::queue<int> work;
	work.push(seed);
	visited_faces[(size_t)seed] = true;
	while (!work.empty()) {
	    const int current = work.front();
	    work.pop();
	    for (int neighbor : face_neighbors[(size_t)current]) {
		if (!visited_faces[(size_t)neighbor]) {
		    visited_faces[(size_t)neighbor] = true;
		    work.push(neighbor);
		}
	    }
	}
    }

    if (!require_closed_links)
	return;
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	const std::vector<int> &incident = vertex_faces[(size_t)vertex];
	if (incident.empty())
	    continue;
	const std::set<int> incident_set(incident.begin(), incident.end());
	bool valid_link = true;
	std::set<int> reached;
	std::queue<int> work;
	work.push(incident[0]);
	reached.insert(incident[0]);
	while (!work.empty()) {
	    const int current = work.front();
	    work.pop();
	    const int *triangle = &faces[3 * current];
	    int degree = 0;
	    for (int corner = 0; corner < 3; ++corner) {
		if (triangle[corner] != vertex)
		    continue;
		for (int offset : {1, 2}) {
		    const int other = triangle[(corner + offset) % 3];
		    const mesh_edge edge = vertex < other ?
			mesh_edge(vertex, other) : mesh_edge(other, vertex);
		    const auto edge_entry = edge_faces.find(edge);
		    if (edge_entry == edge_faces.end() ||
			    edge_entry->second.size() != 2)
			continue;
		    const int neighbor = edge_entry->second[0] == current ?
			edge_entry->second[1] : edge_entry->second[0];
		    if (incident_set.find(neighbor) == incident_set.end())
			continue;
		    ++degree;
		    if (reached.insert(neighbor).second)
			work.push(neighbor);
		}
		break;
	    }
	    valid_link = valid_link && degree == 2;
	}
	if (!valid_link || reached.size() != incident_set.size())
	    result->invalid_vertex_links++;
    }
}

static geom_result
quality_result(struct db_i *dbip, struct directory *dp,
	const struct bg_tess_tol *ttol, int face_index)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    result.requested_items = face_index >= 0 ? 1 : bi->brep->m_F.Count();
    if (face_index >= bi->brep->m_F.Count()) {
	result.issues.push_back("face_index_out_of_range");
	rt_db_free_internal(&intern);
	return result;
    }

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(bi->brep,
	dp->d_namep);
    if (!state) {
	result.issues.push_back("state_creation_failed");
	rt_db_free_internal(&intern);
	return result;
    }
    ON_Brep_CDT_Tol_Set(state, ttol);
    int selected_face = face_index;
    const int selected_count = face_index >= 0 ? 1 : 0;
    int64_t start = bu_gettime();
    result.ret = ON_Brep_CDT_Tessellate(state, selected_count,
	selected_count ? &selected_face : NULL);

    struct brep_cdt_diagnostic diagnostic = {};
    if (ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0) {
	result.diagnostic_result = diagnostic.result;
	result.diagnostic_stage = diagnostic.stage;
	result.diagnostic_face = diagnostic.face_index;
	result.diagnostic_message = diagnostic.message;
	result.completed_items = diagnostic.completed_faces;
	result.failed_items = diagnostic.failed_faces;
	if (diagnostic.face_index >= 0 && diagnostic.failed_faces > 0)
	    capture_index(&result.failed_faces,
		&result.omitted_failed_faces, diagnostic.face_index);
    }
    const int failed_face_count = ON_Brep_CDT_Failed_Faces(NULL, 0,
	state);
    if (failed_face_count > 0) {
	std::vector<int> failed_face_indices((size_t)failed_face_count);
	ON_Brep_CDT_Failed_Faces(failed_face_indices.data(),
	    failed_face_count, state);
	result.failed_faces.clear();
	result.omitted_failed_faces = 0;
	for (int failed_face : failed_face_indices)
	    capture_index(&result.failed_faces,
		&result.omitted_failed_faces, failed_face);
	for (int failed_face : failed_face_indices) {
	    struct brep_cdt_diagnostic face_diagnostic = {};
	    if (ON_Brep_CDT_Face_Diagnostic(&face_diagnostic,
		    failed_face, state) != 0)
		continue;
	    face_failure detail;
	    detail.face_index = failed_face;
	    detail.result = face_diagnostic.result;
	    detail.stage = face_diagnostic.stage;
	    detail.message = face_diagnostic.message;
	    result.face_failures.push_back(detail);
	}
    }

    const bool tessellated = face_index >= 0 ? result.ret == 1 :
	result.ret == 0;
    int *faces = NULL;
    int face_count = 0;
    fastf_t *vertices = NULL;
    int vertex_count = 0;
    if (tessellated && ON_Brep_CDT_Mesh(&faces, &face_count, &vertices,
	    &vertex_count, NULL, NULL, NULL, NULL, state, selected_count,
	    selected_count ? &selected_face : NULL) < 0) {
	result.issues.push_back("mesh_export_failed");
    }
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    result.primitives = face_count > 0 ? (size_t)face_count : 0;
    result.vertices = vertex_count > 0 ? (size_t)vertex_count : 0;

    /* The exported arrays own all data needed by the remaining checks.  Drop
     * the much larger face-local CDT state before allocating independent
     * topology metrics for million-triangle corpus meshes. */
    ON_Brep_CDT_Destroy(state);
    state = NULL;

    for (int i = 0; vertices && i < vertex_count; ++i) {
	point_t point;
	VSET(point, vertices[3 * i], vertices[3 * i + 1],
	    vertices[3 * i + 2]);
	if (!std::isfinite(point[X]) || !std::isfinite(point[Y]) ||
		!std::isfinite(point[Z])) {
	    result.finite = false;
	    continue;
	}
	bbox_add(result.bmin, result.bmax, &result.have_bbox, point);
    }
    for (int i = 0; faces && i < face_count * 3; ++i) {
	if (faces[i] < 0 || faces[i] >= vertex_count)
	    result.invalid_indices++;
    }

    if (!result.invalid_indices && result.finite && faces && vertices)
	mesh_quality_metrics(&result, vertex_count, face_count, vertices,
	    faces, face_index < 0);

    if (face_index < 0 && tessellated && !result.invalid_indices &&
	    result.finite && faces && vertices) {
	struct bg_trimesh_solid_errors errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	result.solid_checked = true;
	result.solid = bg_trimesh_solid2(vertex_count, face_count, vertices,
	    faces, &errors) == 0;
	result.degenerate_faces = errors.degenerate.count;
	result.unmatched_edges = errors.unmatched.count;
	result.excess_edges = errors.excess.count;
	result.misoriented_edges = errors.misoriented.count;
	bg_free_trimesh_solid_errors(&errors);
    }

    if (!tessellated)
	result.issues.push_back("generation_failed");
    if (!result.vertices || !result.primitives)
	result.issues.push_back("empty_geometry");
    if (result.invalid_indices)
	result.issues.push_back("invalid_face_indices");
    if (!result.finite)
	result.issues.push_back("non_finite_coordinates");
    if (!result.have_bbox)
	result.issues.push_back("missing_bbox");
    if (result.solid_checked && !result.solid)
	result.issues.push_back("non_solid_mesh");
    if (result.solid_checked && result.invalid_vertex_links)
	result.issues.push_back("invalid_vertex_links");
    if (result.geometric_degenerate_faces)
	result.issues.push_back("geometric_degenerate_faces");

    bu_free(faces, "brep quality audit faces");
    bu_free(vertices, "brep quality audit vertices");
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
print_indices(const std::vector<int> &indices)
{
    std::cout << "[";
    for (size_t i = 0; i < indices.size(); i++) {
	if (i)
	    std::cout << ",";
	std::cout << indices[i];
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
	<< ",\"diagnostic\":{\"result\":" << result.diagnostic_result
	<< ",\"stage\":" << result.diagnostic_stage
	<< ",\"face_index\":" << result.diagnostic_face
	<< ",\"message\":"
	<< json_quote(result.diagnostic_message.c_str()) << "}"
	<< ",\"solid_validation\":{\"checked\":"
	<< (result.solid_checked ? "true" : "false")
	<< ",\"solid\":" << (result.solid ? "true" : "false")
	<< ",\"degenerate_faces\":" << result.degenerate_faces
	<< ",\"unmatched_edges\":" << result.unmatched_edges
	<< ",\"excess_edges\":" << result.excess_edges
	<< ",\"misoriented_edges\":" << result.misoriented_edges << "}"
	<< ",\"mesh_metrics\":{\"unique_edges\":" << result.unique_edges
	<< ",\"connected_components\":" << result.connected_components
	<< ",\"invalid_vertex_links\":" << result.invalid_vertex_links
	<< ",\"geometric_degenerate_faces\":"
	<< result.geometric_degenerate_faces
	<< ",\"euler_characteristic\":" << result.euler_characteristic
	<< ",\"minimum_angle_degrees\":";
    print_num(result.minimum_angle_degrees);
    std::cout << ",\"maximum_aspect_ratio\":";
    print_num(result.maximum_aspect_ratio);
    std::cout << "}"
	<< ",\"requested_items\":" << result.requested_items
	<< ",\"completed_items\":" << result.completed_items
	<< ",\"failed_items\":" << result.failed_items
	<< ",\"skipped_items\":" << result.skipped_items
	<< ",\"failed_faces\":";
    print_indices(result.failed_faces);
    std::cout << ",\"failed_faces_omitted\":"
	<< result.omitted_failed_faces << ",\"failure_details\":[";
    for (size_t i = 0; i < result.face_failures.size(); ++i) {
	if (i)
	    std::cout << ",";
	const face_failure &failure = result.face_failures[i];
	std::cout << "{\"face_index\":" << failure.face_index
	    << ",\"result\":" << failure.result
	    << ",\"stage\":" << failure.stage
	    << ",\"message\":" << json_quote(failure.message.c_str())
	    << "}";
    }
    std::cout << "],\"skipped_faces\":";
    print_indices(result.skipped_faces);
    std::cout << ",\"skipped_faces_omitted\":"
	<< result.omitted_skipped_faces << ",\"unprocessed_faces\":";
    print_indices(result.unprocessed_faces);
    std::cout << ",\"unprocessed_faces_omitted\":"
	<< result.omitted_unprocessed_faces << ",\"failed_edges\":";
    print_indices(result.failed_edges);
    std::cout << ",\"failed_edges_omitted\":"
	<< result.omitted_failed_edges << ",\"unprocessed_edges\":";
    print_indices(result.unprocessed_edges);
    std::cout << ",\"unprocessed_edges_omitted\":"
	<< result.omitted_unprocessed_edges
	<< ",\"failed_surface_cues\":";
    print_indices(result.failed_surface_cues);
    std::cout << ",\"failed_surface_cues_omitted\":"
	<< result.omitted_failed_surface_cues
	<< ",\"unprocessed_surface_cues\":";
    print_indices(result.unprocessed_surface_cues);
    std::cout << ",\"unprocessed_surface_cues_omitted\":"
	<< result.omitted_unprocessed_surface_cues
	<< ",\"limits\":{\"time\":" << (result.hit_time_limit ? "true" : "false")
	<< ",\"memory\":" << (result.hit_memory_limit ? "true" : "false")
	<< ",\"points\":" << (result.hit_point_limit ? "true" : "false") << "}"
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

struct audit_config {
    double ratio_min;
    double ratio_max;
    double tess_abs;
    double tess_rel;
    double tess_norm;
    long memory_limit_mib;
    long jobs;
    long max_time_ms;
    long max_result_mib;
    long max_points;
    long face_index;
    bool valid_solids_only;
};

static int
audit_brep(struct db_i *dbip, struct directory *dp, const char *db_path,
	const char *mode_name, const audit_config &config, long task_index)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    ttol.abs = config.tess_abs;
    ttol.rel = config.tess_rel;
    ttol.norm = config.tess_norm;
    struct brep_cdt_fast_options fast_options;
    brep_cdt_fast_options_default(&fast_options);
    struct rt_brep_draw_options draw_options;
    rt_brep_draw_options_default(&draw_options);
    if (config.jobs > 0)
	fast_options.max_workers = draw_options.max_workers =
	    (size_t)config.jobs;
    if (config.max_time_ms > 0) {
	fast_options.max_time_ms = config.max_time_ms;
	draw_options.max_time_ms = config.max_time_ms;
    }
    if (config.max_result_mib > 0) {
	fast_options.max_result_bytes =
	    (size_t)config.max_result_mib * 1024 * 1024;
	draw_options.max_result_bytes =
	    (size_t)config.max_result_mib * 1024 * 1024;
    }
    if (config.max_points > 0) {
	fast_options.max_points = (size_t)config.max_points;
	draw_options.max_points = (size_t)config.max_points;
    }

    std::cerr << "brep-audit: phase=reference" << std::endl;
    point_t ref_min = VINIT_ZERO;
    point_t ref_max = VINIT_ZERO;
    bool ref_valid = false;
    point_t boundary_min = VINIT_ZERO;
    point_t boundary_max = VINIT_ZERO;
    bool boundary_valid = false;
    int ref_faces = 0;
    int ref_face_failures = 0;
    std::vector<int> ref_failed_faces;
    std::vector<std::string> top_issues;
    bool input_loaded = false;
    bool input_valid = false;
    bool input_manifold = false;
    bool input_oriented = false;
    bool input_has_boundary = true;
    bool input_solid = false;
    bool input_two_trim_edges = false;
    bool quality_eligible = false;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) == BRLCAD_OK) {
	struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
	input_loaded = true;
	ON_wString validity_log;
	ON_TextLog validity_output(validity_log);
	input_valid = bi->brep->IsValid(&validity_output);
	input_manifold = bi->brep->IsManifold(&input_oriented,
	    &input_has_boundary);
	input_solid = bi->brep->IsSolid();
	input_two_trim_edges = true;
	for (int edge = 0; edge < bi->brep->m_E.Count(); ++edge) {
	    if (bi->brep->m_E[edge].TrimCount() != 2) {
		input_two_trim_edges = false;
		break;
	    }
	}
	quality_eligible = input_valid && input_manifold && input_oriented &&
	    !input_has_boundary && input_solid && input_two_trim_edges;
	ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
	ON_BoundingBox boundary_bbox = ON_BoundingBox::EmptyBoundingBox;
	const int brep_faces = bi->brep->m_F.Count();
	int first_ref_face = 0;
	int end_ref_face = brep_faces;
	if (config.face_index >= 0) {
	    if (config.face_index >= brep_faces) {
		top_issues.push_back("face_index_out_of_range");
		end_ref_face = 0;
	    } else {
		first_ref_face = (int)config.face_index;
		end_ref_face = first_ref_face + 1;
	    }
	}
	ref_faces = end_ref_face - first_ref_face;
	const bool excluded = config.valid_solids_only && !quality_eligible;
	for (int i = first_ref_face; !excluded && i < end_ref_face; i++) {
	    ON_BoundingBox face_bbox = ON_BoundingBox::EmptyBoundingBox;
	    if (!face_GetBoundingBox(bi->brep->m_F[i], face_bbox, false) ||
		    !face_bbox.IsValid()) {
		ref_face_failures++;
		ref_failed_faces.push_back(i);
		continue;
	    }
	    if (bbox.IsValid())
		bbox.Union(face_bbox);
	    else
		bbox = face_bbox;
	    ON_BoundingBox face_boundary = ON_BoundingBox::EmptyBoundingBox;
	    if (face_boundary_bbox(bi->brep->m_F[i], face_boundary)) {
		if (boundary_bbox.IsValid())
		    boundary_bbox.Union(face_boundary);
		else
		    boundary_bbox = face_boundary;
	    }
	}
	if (bbox.IsValid()) {
	    VMOVE(ref_min, bbox.m_min);
	    VMOVE(ref_max, bbox.m_max);
	    ref_valid = true;
	} else if (!excluded) {
	    top_issues.push_back("trimmed_bbox_failed");
	}
	if (boundary_bbox.IsValid()) {
	    VMOVE(boundary_min, boundary_bbox.m_min);
	    VMOVE(boundary_max, boundary_bbox.m_max);
	    boundary_valid = true;
	}
	if (ref_face_failures)
	    top_issues.push_back("trimmed_bbox_face_failures");
	rt_db_free_internal(&intern);
    } else {
	top_issues.push_back("database_internal_load_failed");
    }

    const bool run_wireframe = BU_STR_EQUAL(mode_name, "wireframe") ||
	BU_STR_EQUAL(mode_name, "both") || BU_STR_EQUAL(mode_name, "all");
    const bool run_shaded = BU_STR_EQUAL(mode_name, "shaded") ||
	BU_STR_EQUAL(mode_name, "both") || BU_STR_EQUAL(mode_name, "all");
    const bool run_quality = BU_STR_EQUAL(mode_name, "quality") ||
	BU_STR_EQUAL(mode_name, "all");
    const bool excluded = config.valid_solids_only && input_loaded &&
	!quality_eligible;
    geom_result wire;
    geom_result shaded;
    geom_result quality;
    if (run_wireframe && !excluded) {
	std::cerr << "brep-audit: phase=wireframe" << std::endl;
	wire = wireframe_result(dbip, dp, &ttol, &tol, &draw_options);
    }
    if (run_shaded && !excluded) {
	std::cerr << "brep-audit: phase=shaded" << std::endl;
	shaded = shaded_result(dbip, dp, &ttol, &tol, &fast_options,
	    (int)config.face_index);
    }
    if (run_quality && !excluded) {
	std::cerr << "brep-audit: phase=quality" << std::endl;
	quality = quality_result(dbip, dp, &ttol,
	    (int)config.face_index);
    }
    vect_t ref_dims = VINIT_ZERO;
    vect_t boundary_dims = VINIT_ZERO;
    double ref_diag = 0.0;
    double boundary_diag = 0.0;
    if (boundary_valid) {
	dims(boundary_dims, boundary_min, boundary_max);
	boundary_diag = MAGNITUDE(boundary_dims);
    }
    if (ref_valid) {
	dims(ref_dims, ref_min, ref_max);
	ref_diag = MAGNITUDE(ref_dims);
	if (run_wireframe)
	    check_dimensions(&wire, ref_dims, ref_diag, 0.0,
		config.ratio_max, "wireframe");
	if (run_shaded) {
	    check_dimensions(&shaded, ref_dims, ref_diag, 0.0,
		config.ratio_max, "shaded");
	    if (boundary_valid)
		check_dimensions(&shaded, boundary_dims, boundary_diag,
		    config.ratio_min, DBL_MAX, "shaded_boundary");
	}
	if (run_quality) {
	    check_dimensions(&quality, ref_dims, ref_diag, 0.0,
		config.ratio_max, "quality");
	    if (boundary_valid)
		check_dimensions(&quality, boundary_dims, boundary_diag,
		    config.ratio_min, DBL_MAX, "quality_boundary");
	}
    }

    bool okay = !excluded && ref_valid && top_issues.empty() &&
	(!run_wireframe || wire.issues.empty()) &&
	(!run_shaded || shaded.issues.empty()) &&
	(!run_quality || quality.issues.empty());

    std::cerr << "brep-audit: phase=report" << std::endl;
    std::cout << "{\"format\":\"brlcad-brep-realization-audit-v1\",\"database\":"
	<< json_quote(db_path) << ",\"object\":" << json_quote(dp->d_namep)
	<< ",\"task_index\":" << task_index
	<< ",\"status\":" << json_quote(excluded ? "excluded" :
	    (okay ? "ok" : "fail"))
	<< ",\"ratio_limits\":[" << std::setprecision(17)
	<< config.ratio_min << "," << config.ratio_max << "]"
	<< ",\"tessellation_tolerance\":{\"abs\":" << config.tess_abs
	<< ",\"rel\":" << config.tess_rel << ",\"norm\":"
	<< config.tess_norm << "}"
	<< ",\"memory_limit_mib\":" << config.memory_limit_mib
	<< ",\"mode\":" << json_quote(mode_name)
	<< ",\"face_index\":" << config.face_index
	<< ",\"input\":{\"loaded\":" << (input_loaded ? "true" : "false")
	<< ",\"valid\":" << (input_valid ? "true" : "false")
	<< ",\"manifold\":" << (input_manifold ? "true" : "false")
	<< ",\"oriented\":" << (input_oriented ? "true" : "false")
	<< ",\"has_boundary\":" << (input_has_boundary ? "true" : "false")
	<< ",\"solid\":" << (input_solid ? "true" : "false")
	<< ",\"two_trim_edges\":" <<
	    (input_two_trim_edges ? "true" : "false")
	<< ",\"quality_eligible\":" <<
	    (quality_eligible ? "true" : "false") << "}"
	<< ",\"fast_options\":{\"jobs\":" << fast_options.max_workers
	<< ",\"max_time_ms\":" << fast_options.max_time_ms
	<< ",\"max_result_bytes\":" << fast_options.max_result_bytes
	<< ",\"max_points\":" << fast_options.max_points << "}"
	<< ",\"wire_options\":{\"jobs\":" << draw_options.max_workers
	<< ",\"max_time_ms\":" << draw_options.max_time_ms
	<< ",\"max_result_bytes\":" << draw_options.max_result_bytes
	<< ",\"max_points\":" << draw_options.max_points << "}"
	<< ",\"generators\":{\"wireframe\":\"rt_brep_plot\""
	<< ",\"shaded\":\"brep_cdt_fast\""
	<< ",\"quality\":\"ON_Brep_CDT_Tessellate\"}"
	<< ",\"reference\":{\"method\":\"face_GetBoundingBox_trim_parameter_envelope\""
	<< ",\"face_count\":" << ref_faces
	<< ",\"failed_faces\":" << ref_face_failures
	<< ",\"failed_face_indices\":";
    print_indices(ref_failed_faces);
    std::cout << ",\"bbox_valid\":" << (ref_valid ? "true" : "false")
	<< ",\"bbox_min\":";
    if (ref_valid) print_vec(ref_min); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (ref_valid) print_vec(ref_max); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (ref_valid) print_vec(ref_dims); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (ref_valid) print_num(ref_diag); else std::cout << "null";
    std::cout << ",\"role\":\"upper_extent_guard\""
	<< ",\"boundary_method\":\"brep_edge_tight_bounding_boxes\""
	<< ",\"boundary_bbox_valid\":"
	<< (boundary_valid ? "true" : "false")
	<< ",\"boundary_bbox_min\":";
    if (boundary_valid) print_vec(boundary_min); else std::cout << "null";
    std::cout << ",\"boundary_bbox_max\":";
    if (boundary_valid) print_vec(boundary_max); else std::cout << "null";
    std::cout << ",\"boundary_dimensions\":";
    if (boundary_valid) print_vec(boundary_dims); else std::cout << "null";
    std::cout << ",\"boundary_diagonal\":";
    if (boundary_valid) print_num(boundary_diag); else std::cout << "null";
    std::cout << "},\"wireframe\":";
    if (run_wireframe && !excluded) print_result(wire, ref_dims);
    else std::cout << "null";
    std::cout << ",\"shaded\":";
    if (run_shaded && !excluded) print_result(shaded, ref_dims);
    else std::cout << "null";
    std::cout << ",\"quality\":";
    if (run_quality && !excluded) print_result(quality, ref_dims);
    else std::cout << "null";
    std::cout << ",\"issues\":";
    print_issues(top_issues);
    std::cout << "}" << std::endl;
    rt_vlist_cleanup();
#ifdef __GLIBC__
    malloc_trim(0);
#endif
    return excluded || okay ? 0 : 1;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    argc--; argv++;

    int print_help = 0;
    int list_only = 0;
    int batch = 0;
    double ratio_min = 0.5;
    double ratio_max = 2.0;
    double tess_abs = 0.0;
    double tess_rel = 0.01;
    double tess_norm = 0.0;
    long memory_limit_mib = 0;
    long jobs = 0;
    long max_time_ms = 0;
    long max_result_mib = 0;
    long max_points = 0;
    long batch_start = 0;
    long face_index = -1;
    int valid_solids_only = 0;
    const char *mode_name = "both";
    struct bu_opt_desc d[18];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help and exit");
    BU_OPT(d[1], "l", "list", "", NULL, &list_only, "List BRep primitive names");
    BU_OPT(d[2], "", "ratio-min", "#", &bu_opt_fastf_t, &ratio_min, "Minimum acceptable generated/reference dimension ratio");
    BU_OPT(d[3], "", "ratio-max", "#", &bu_opt_fastf_t, &ratio_max, "Maximum acceptable generated/reference dimension ratio");
    BU_OPT(d[4], "", "tess-abs", "#", &bu_opt_fastf_t, &tess_abs, "Absolute shaded tessellation tolerance");
    BU_OPT(d[5], "", "tess-rel", "#", &bu_opt_fastf_t, &tess_rel, "Relative shaded tessellation tolerance");
    BU_OPT(d[6], "", "tess-norm", "#", &bu_opt_fastf_t, &tess_norm, "Normal shaded tessellation tolerance");
    BU_OPT(d[7], "", "memory-limit-mib", "#", &bu_opt_long, &memory_limit_mib, "Process address-space limit in MiB (zero disables)");
    BU_OPT(d[8], "m", "mode", "wireframe|shaded|quality|both|all", &bu_opt_str, &mode_name, "Select generators; quality runs the conversion CDT");
    BU_OPT(d[9], "j", "jobs", "#", &bu_opt_long, &jobs, "Maximum shaded face workers (zero selects the default)");
    BU_OPT(d[10], "", "max-time-ms", "#", &bu_opt_long, &max_time_ms, "Shaded generation deadline checked between faces");
    BU_OPT(d[11], "", "max-result-mib", "#", &bu_opt_long, &max_result_mib, "Maximum retained shaded result size");
    BU_OPT(d[12], "", "max-points", "#", &bu_opt_long, &max_points, "Maximum retained shaded points");
    BU_OPT(d[13], "", "batch", "", NULL, &batch, "Audit all BReps in one database process");
    BU_OPT(d[14], "", "batch-start", "#", &bu_opt_long, &batch_start, "First flattened batch task index");
    BU_OPT(d[15], "", "face-index", "#", &bu_opt_long, &face_index, "Shade only one BRep face (non-batch mode)");
    BU_OPT(d[16], "", "valid-solids-only", "", NULL,
	&valid_solids_only,
	"Exclude inputs outside the rigorous CDT topology contract");
    BU_OPT_NULL(d[17]);
    int ac = bu_opt_parse(NULL, argc, argv, d);
    const char *usage =
	"Usage: brep-audit [options] [--list|--batch] file.g [brep]\n";
    if (print_help || (list_only && (batch || ac != 1)) ||
	    (batch && ac != 1) || (!list_only && !batch && ac != 2) ||
	    ratio_min <= 0.0 || ratio_max < ratio_min || tess_abs < 0.0 ||
	    tess_rel < 0.0 || tess_norm < 0.0 || memory_limit_mib < 0 ||
	    jobs < 0 || max_time_ms < 0 || max_result_mib < 0 ||
	    max_points < 0 || batch_start < 0 || face_index < -1 ||
	    (batch && face_index != -1) ||
	    (face_index != -1 && BU_STR_EQUAL(mode_name, "wireframe")) ||
	    (!BU_STR_EQUAL(mode_name, "wireframe") &&
	     !BU_STR_EQUAL(mode_name, "shaded") &&
	     !BU_STR_EQUAL(mode_name, "quality") &&
	     !BU_STR_EQUAL(mode_name, "both") &&
	     !BU_STR_EQUAL(mode_name, "all"))) {
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
    audit_config config = {
	ratio_min, ratio_max, tess_abs, tess_rel, tess_norm,
	memory_limit_mib, jobs, max_time_ms, max_result_mib, max_points,
	face_index, valid_solids_only != 0
    };

    if (batch) {
	std::vector<struct directory *> breps;
	struct directory *entry;
	FOR_ALL_DIRECTORY_START(entry, dbip) {
	    if (entry->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP)
		breps.push_back(entry);
	} FOR_ALL_DIRECTORY_END;
	std::sort(breps.begin(), breps.end(),
	    [](const struct directory *a, const struct directory *b) {
		return std::string(a->d_namep) < std::string(b->d_namep);
	    });
	std::vector<const char *> modes;
	if (BU_STR_EQUAL(mode_name, "wireframe") ||
		BU_STR_EQUAL(mode_name, "both") ||
		BU_STR_EQUAL(mode_name, "all"))
	    modes.push_back("wireframe");
	if (BU_STR_EQUAL(mode_name, "shaded") ||
		BU_STR_EQUAL(mode_name, "both") ||
		BU_STR_EQUAL(mode_name, "all"))
	    modes.push_back("shaded");
	if (BU_STR_EQUAL(mode_name, "quality") ||
		BU_STR_EQUAL(mode_name, "all"))
	    modes.push_back("quality");
	long task_index = 0;
	for (struct directory *brep : breps) {
	    for (const char *task_mode : modes) {
		if (task_index++ < batch_start)
		    continue;
		const long active_index = task_index - 1;
		std::cout
		    << "{\"format\":\"brlcad-brep-audit-progress-v1\""
		    << ",\"database\":" << json_quote(argv[0])
		    << ",\"object\":" << json_quote(brep->d_namep)
		    << ",\"mode\":" << json_quote(task_mode)
		    << ",\"task_index\":" << active_index
		    << ",\"status\":\"started\"}" << std::endl;
		audit_brep(dbip, brep, argv[0], task_mode, config,
		    active_index);
	    }
	}
	db_close(dbip);
	return 0;
    }

    struct directory *dp = db_lookup(dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	std::cerr << "Not a BRep primitive: " << argv[1] << "\n";
	db_close(dbip);
	return 2;
    }
    int ret = audit_brep(dbip, dp, argv[0], mode_name, config, -1);
    db_close(dbip);
    return ret;
}
