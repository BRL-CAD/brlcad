/*                         J T _ T O P O L O G Y . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef LIBGCV_JT_TOPOLOGY_H
#define LIBGCV_JT_TOPOLOGY_H

#include "common.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace jt {

/* The 22 decoded symbol fields preceding the vertex records in a JT 9/10
 * Topologically Compressed Rep.  The packet and predictor decoding remains
 * the responsibility of the file parser. */
struct TopologySymbols {
    std::array<std::vector<int32_t>, 8> face_degrees;
    std::vector<int32_t> vertex_valences;
    std::vector<int32_t> vertex_groups;
    std::vector<int32_t> vertex_flags;
    std::array<std::vector<uint64_t>, 8> face_attribute_masks;
    std::vector<uint32_t> high_degree_attribute_masks;
    std::vector<int32_t> split_face_offsets;
    std::vector<int32_t> split_face_positions;
};

struct TopologyMesh {
    /* One polygon per non-cover dual vertex.  Values index the topological
     * vertex coordinate records in the compressed vertex array. */
    std::vector<std::vector<int32_t>> polygons;
    std::vector<int32_t> polygon_groups;
    /* Attribute record indices parallel to each polygon's coordinate ring.
     * An entry of -1 means that no attribute was bound at that corner. */
    std::vector<std::vector<int32_t>> attribute_indices;
    /* Fan-triangulated form of polygons, ready for BOT face indices. */
    std::vector<int32_t> triangles;
};

bool decode_topology(const TopologySymbols &symbols, TopologyMesh &mesh, std::string &error);

} // namespace jt

#endif
