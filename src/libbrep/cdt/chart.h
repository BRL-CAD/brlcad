/*                      C H A R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#ifndef LIBBREP_CDT_CHART_H
#define LIBBREP_CDT_CHART_H

#include "common.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "brep/defines.h"

typedef int64_t cdt_topo_vertex_id;
typedef int64_t cdt_brep_edge_id;
typedef int64_t cdt_edge_sample_id;
typedef int64_t cdt_face_chart_id;
typedef int64_t cdt_chart_vertex_id;

#define CDT_TOPOLOGY_ID_NONE ((int64_t)-1)

enum cdt_face_chart_type {
    CDT_FACE_CHART_NATIVE_UV = 0,
    CDT_FACE_CHART_CONE_WEDGE = 1
};

struct cdt_chart_vertex {
    cdt_chart_vertex_id id = CDT_TOPOLOGY_ID_NONE;
    long native_point = -1;
    cdt_topo_vertex_id topo_vertex = CDT_TOPOLOGY_ID_NONE;
    cdt_brep_edge_id brep_edge = CDT_TOPOLOGY_ID_NONE;
    cdt_edge_sample_id edge_sample = CDT_TOPOLOGY_ID_NONE;
    int seam_side = 0;
    bool singular = false;
};

/* A topology-aware planar adapter for one B-Rep face.  Chart coordinates are
 * used only by the constrained triangulator; native UV remains authoritative
 * for evaluating the original surface. */
class BREP_EXPORT cdt_face_chart {
public:
    bool build(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &outer,
	const std::vector<std::vector<int>> &holes,
	const std::vector<int> &steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices);

    bool native_to_chart(const ON_2dPoint &native_uv,
	ON_2dPoint &chart_uv) const;
    bool chart_to_native(const ON_2dPoint &chart_uv,
	ON_2dPoint &native_uv) const;

    long native_point(int chart_point) const;
    cdt_face_chart_id id() const { return m_id; }
    enum cdt_face_chart_type type() const { return m_type; }
    int singular_side() const { return m_singular_side; }
    int closed_direction() const { return m_closed_dir; }
    cdt_topo_vertex_id pole_topology_vertex() const
	{ return m_pole_topology_vertex; }
    const std::string &failure() const { return m_failure; }

    std::vector<std::pair<double, double>> points;
    std::vector<cdt_chart_vertex> vertices;
    std::vector<int> outer;
    std::vector<std::vector<int>> holes;
    std::vector<int> steiner;
    std::vector<std::pair<int, int>> constraints;

private:
    bool build_native(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<cdt_topo_vertex_id> &topology_vertices);
    bool build_cone(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices);
    bool validate_boundary(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points);

    cdt_face_chart_id m_id = CDT_TOPOLOGY_ID_NONE;
    enum cdt_face_chart_type m_type = CDT_FACE_CHART_NATIVE_UV;
    int m_closed_dir = -1;
    int m_open_dir = -1;
    int m_singular_side = -1;
    cdt_topo_vertex_id m_pole_topology_vertex = CDT_TOPOLOGY_ID_NONE;
    ON_Interval m_closed_domain;
    ON_Interval m_open_domain;
    std::string m_failure;
};

BREP_EXPORT bool cdt_face_uses_cone_chart(const ON_BrepFace &face);

#endif /* LIBBREP_CDT_CHART_H */
