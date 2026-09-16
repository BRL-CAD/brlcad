/*              S T E P T E S S E L L A T E D M E S H . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPTESSELLATEDMESH_H
#define CONV_STEP_STEPTESSELLATEDMESH_H

#include "common.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "vmath.h"

namespace brlcad {
namespace step {

struct TessellatedCoordinateData {
    int64_t entity_id = 0;
    std::vector<fastf_t> points;
};

/** Schema-neutral validation and construction for STEP triangle meshes.
 * Generated AP bindings are responsible only for decoding their aggregate
 * attributes into the vectors accepted here. */
class TessellatedMeshBuilder
{
public:
    TessellatedMeshBuilder(double length_factor, bool allow_winding_repair,
	double vertex_merge_tolerance = 0.0,
	bool allow_repeated_triangle_repair = false);

    const TessellatedCoordinateData *DefineCoordinates(int64_t entity_id,
	const std::vector<std::vector<double> > &rows, size_t expected_points);
    bool ValidateIndexing(const TessellatedCoordinateData &coordinates,
	int pnmax, const std::vector<int> &pnindex,
	const std::vector<std::vector<double> > &normals);
    bool AddTriangle(const TessellatedCoordinateData &coordinates,
	const std::vector<int> &pnindex, int pnmax,
	const std::vector<std::vector<double> > &normals,
	int first, int second, int third);
    bool Closed() const;

    std::vector<fastf_t> vertices;
    std::vector<int> faces;
    size_t reversed_by_normals = 0;
    size_t merged_by_tolerance = 0;
    size_t discarded_repeated_triangles = 0;
    std::string error;

private:
    int Vertex(const TessellatedCoordinateData &coordinates,
	int coordinate_index);

    double length = 1.0;
    bool repair_winding = false;
    bool repair_repeated_triangles = false;
    double merge_tolerance = 0.0;
    std::map<int64_t, TessellatedCoordinateData> coordinate_cache;
    std::map<std::pair<int64_t, int>, int> vertex_map;
    std::map<std::array<fastf_t, 3>, int> exact_vertices;
    std::map<std::array<int64_t, 3>, std::vector<int> > vertex_buckets;
};

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPTESSELLATEDMESH_H */
