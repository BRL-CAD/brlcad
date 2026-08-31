/*                 T R I M E S H _ S P L I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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
/** @file trimesh_split.cpp
 *
 * Identify edge-connected components in a triangle mesh.
 */

#include "common.h"

#include <algorithm>
#include <map>
#include <vector>

#include "bu/malloc.h"
#include "bg/trimesh.h"

namespace {

class tm_split_uedge {
  public:
    tm_split_uedge(int vert1, int vert2) :
	v1(std::min(vert1, vert2)), v2(std::max(vert1, vert2))
    {
    }

    bool operator<(const tm_split_uedge &other) const
    {
	return v1 < other.v1 || (v1 == other.v1 && v2 < other.v2);
    }

  private:
    int v1;
    int v2;
};

class tm_split_disjoint_sets {
  public:
    explicit tm_split_disjoint_sets(size_t count) : parents(count), ranks(count, 0)
    {
	for (size_t i = 0; i < count; ++i)
	    parents[i] = i;
    }

    size_t root(size_t entry)
    {
	if (parents[entry] != entry)
	    parents[entry] = root(parents[entry]);
	return parents[entry];
    }

    void join(size_t first, size_t second)
    {
	first = root(first);
	second = root(second);
	if (first == second)
	    return;

	if (ranks[first] < ranks[second])
	    std::swap(first, second);
	parents[second] = first;
	if (ranks[first] == ranks[second])
	    ++ranks[first];
    }

  private:
    std::vector<size_t> parents;
    std::vector<unsigned char> ranks;
};

} // namespace


extern "C" int
bg_trimesh_separate(int **face_indices, int **component_offsets,
	const int *faces, int face_count)
{
    if (!face_indices || !component_offsets || face_count < 0 ||
	(face_count > 0 && !faces))
	return -1;

    *face_indices = NULL;
    *component_offsets = NULL;
    if (face_count == 0)
	return 0;

    tm_split_disjoint_sets components((size_t)face_count);
    std::map<tm_split_uedge, size_t> edge_owner;
    for (int face = 0; face < face_count; ++face) {
	const int *vertices = &faces[face * 3];
	tm_split_uedge edges[] = {
	    tm_split_uedge(vertices[0], vertices[1]),
	    tm_split_uedge(vertices[1], vertices[2]),
	    tm_split_uedge(vertices[2], vertices[0])
	};

	for (const tm_split_uedge &edge : edges) {
	    auto insertion = edge_owner.emplace(edge, (size_t)face);
	    if (!insertion.second)
		components.join((size_t)face, insertion.first->second);
	}
    }

    // Assign compact component numbers in first-input-face order.  This also
    // makes both component and intra-component ordering deterministic.
    std::vector<int> root_component((size_t)face_count, -1);
    std::vector<int> face_component((size_t)face_count, -1);
    std::vector<int> component_counts;
    for (int face = 0; face < face_count; ++face) {
	size_t root = components.root((size_t)face);
	if (root_component[root] < 0) {
	    root_component[root] = (int)component_counts.size();
	    component_counts.push_back(0);
	}
	int component = root_component[root];
	face_component[(size_t)face] = component;
	++component_counts[(size_t)component];
    }

    int *offsets = (int *)bu_calloc(component_counts.size() + 1,
	sizeof(int), "trimesh component offsets");
    for (size_t component = 0; component < component_counts.size(); ++component)
	offsets[component + 1] = offsets[component] + component_counts[component];

    int *indices = (int *)bu_calloc((size_t)face_count, sizeof(int),
	"trimesh component face indices");
    std::vector<int> positions(offsets, offsets + component_counts.size());
    for (int face = 0; face < face_count; ++face) {
	int component = face_component[(size_t)face];
	indices[(size_t)positions[(size_t)component]++] = face;
    }

    *face_indices = indices;
    *component_offsets = offsets;
    return (int)component_counts.size();
}


extern "C" int
bg_trimesh_split(int ***output_sets, int **output_counts, int *faces, int face_count)
{
    // Preserve the historical validation behavior of this compatibility API.
    if (!output_sets || !output_counts || !faces || face_count < 0)
	return -1;

    *output_sets = NULL;
    *output_counts = NULL;

    int *face_indices = NULL;
    int *component_offsets = NULL;
    int component_count = bg_trimesh_separate(&face_indices,
	&component_offsets, faces, face_count);
    if (component_count <= 0)
	return component_count;

    int **sets = (int **)bu_calloc((size_t)component_count, sizeof(int *),
	"trimesh face sets");
    int *counts = (int *)bu_calloc((size_t)component_count, sizeof(int),
	"trimesh face counts");
    for (int component = 0; component < component_count; ++component) {
	int count = component_offsets[component + 1] - component_offsets[component];
	counts[component] = count;
	sets[component] = (int *)bu_calloc((size_t)count, 3 * sizeof(int),
	    "trimesh face set");
	for (int position = 0; position < count; ++position) {
	    int face = face_indices[component_offsets[component] + position];
	    std::copy_n(&faces[face * 3], 3, &sets[component][position * 3]);
	}
    }

    bu_free(face_indices, "trimesh component face indices");
    bu_free(component_offsets, "trimesh component offsets");
    *output_sets = sets;
    *output_counts = counts;
    return component_count;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
