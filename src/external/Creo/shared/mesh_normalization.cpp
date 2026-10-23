/**
 *     M E S H _ N O R M A L I Z A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/**
 * @file shared/mesh_normalization.cpp
 *
 * Mesh acceptance policy shared by Creo capture and snapshot replay.
 */

#include "shared/mesh_normalization.h"

#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdint.h>
#include <unordered_map>
#include <utility>


const double CREO_BRL_MESH_VERTEX_TOLERANCE_MM = 0.0005;


namespace {

typedef std::array<int64_t, CREO_BRL_SNAPSHOT_COORDINATE_COUNT> mesh_cell;


struct mesh_cell_hash {
    size_t operator()(const mesh_cell& cell) const
    {
        static const size_t HASH_MULTIPLIER = 31u;
        size_t hash = 0u;

        for (size_t coordinate = 0;
             coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            hash = hash * HASH_MULTIPLIER + std::hash<int64_t>()(cell[coordinate]);
        }
        return hash;
    }
};


class mesh_vertex_index {
public:
    mesh_vertex_index() : tolerance_squared_(CREO_BRL_MESH_VERTEX_TOLERANCE_MM *
                                             CREO_BRL_MESH_VERTEX_TOLERANCE_MM)
    {
    }

    int add(const struct creo_brl_snapshot_vector& vertex, uint32_t *index)
    {
        static const int CELL_NEIGHBOR_RADIUS = 1;
        mesh_cell cell = {};
        uint32_t match = UINT32_MAX;

        if (!index || !vertex_is_finite(vertex))
            return 0;
        cell = vertex_cell(vertex);

        for (int x = -CELL_NEIGHBOR_RADIUS; x <= CELL_NEIGHBOR_RADIUS; ++x) {
            for (int y = -CELL_NEIGHBOR_RADIUS; y <= CELL_NEIGHBOR_RADIUS; ++y) {
                for (int z = -CELL_NEIGHBOR_RADIUS; z <= CELL_NEIGHBOR_RADIUS; ++z) {
                    mesh_cell neighbor = cell;
                    neighbor[0] += x;
                    neighbor[1] += y;
                    neighbor[2] += z;
                    const cell_map::const_iterator bucket = cells_.find(neighbor);

                    if (bucket == cells_.end())
                        continue;
                    for (size_t candidate_index = 0;
                         candidate_index < bucket->second.size(); ++candidate_index) {
                        const uint32_t candidate = bucket->second[candidate_index];

                        if (candidate < match && vertices_are_equivalent(
                                                     vertex, vertices_[candidate]))
                            match = candidate;
                    }
                }
            }
        }

        if (match != UINT32_MAX) {
            *index = match;
            return 1;
        }
        if (vertices_.size() >= UINT32_MAX)
            return 0;

        *index = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back(vertex);
        cells_[cell].push_back(*index);
        return 1;
    }

    const std::vector<struct creo_brl_snapshot_vector>& vertices() const
    {
        return vertices_;
    }

private:
    typedef std::unordered_map<mesh_cell, std::vector<uint32_t>, mesh_cell_hash> cell_map;

    static int64_t coordinate_cell(double coordinate)
    {
        const int64_t minimum_cell = (std::numeric_limits<int64_t>::min)() + 1;
        const int64_t maximum_cell = (std::numeric_limits<int64_t>::max)() - 1;
        const long double scaled = std::floor(
            static_cast<long double>(coordinate) /
            static_cast<long double>(CREO_BRL_MESH_VERTEX_TOLERANCE_MM));

        if (!std::isfinite(scaled))
            return coordinate < 0.0 ? minimum_cell : maximum_cell;
        if (scaled <= static_cast<long double>(minimum_cell))
            return minimum_cell;
        if (scaled >= static_cast<long double>(maximum_cell))
            return maximum_cell;
        return static_cast<int64_t>(scaled);
    }

    static mesh_cell vertex_cell(const struct creo_brl_snapshot_vector& vertex)
    {
        mesh_cell cell = {};

        for (size_t coordinate = 0;
             coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate)
            cell[coordinate] = coordinate_cell(vertex.coordinates[coordinate]);
        return cell;
    }

    static int vertex_is_finite(const struct creo_brl_snapshot_vector& vertex)
    {
        for (size_t coordinate = 0;
             coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            if (!std::isfinite(vertex.coordinates[coordinate]))
                return 0;
        }
        return 1;
    }

    int vertices_are_equivalent(
        const struct creo_brl_snapshot_vector& left,
        const struct creo_brl_snapshot_vector& right) const
    {
        double distance_squared = 0.0;

        for (size_t coordinate = 0;
             coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            const double difference = left.coordinates[coordinate] -
                                      right.coordinates[coordinate];

            if (std::fabs(difference) > CREO_BRL_MESH_VERTEX_TOLERANCE_MM)
                return 0;
            distance_squared += difference * difference;
        }
        return distance_squared <= tolerance_squared_;
    }

    double tolerance_squared_;
    std::vector<struct creo_brl_snapshot_vector> vertices_;
    cell_map cells_;
};


static int
mesh_triangle_is_accepted(
    const struct creo_brl_snapshot_triangle& triangle,
    const std::vector<struct creo_brl_snapshot_vector>& vertices)
{
    const uint32_t first = triangle.vertices[0];
    const uint32_t second = triangle.vertices[1];
    const uint32_t third = triangle.vertices[2];

    if (first == second || second == third || first == third)
        return 0;

    const uint32_t edges[CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT][2] = {
        {first, second},
        {second, third},
        {third, first}
    };
    for (size_t edge = 0; edge < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++edge) {
        double length_squared = 0.0;

        for (size_t coordinate = 0;
             coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            const double difference =
                vertices[edges[edge][0]].coordinates[coordinate] -
                vertices[edges[edge][1]].coordinates[coordinate];
            length_squared += difference * difference;
        }
        if (length_squared < CREO_BRL_MESH_VERTEX_TOLERANCE_MM *
                                 CREO_BRL_MESH_VERTEX_TOLERANCE_MM)
            return 0;
    }

    return 1;
}


static int
mesh_is_solid(const std::vector<struct creo_brl_snapshot_vector>& vertices,
              const std::vector<struct creo_brl_snapshot_triangle>& triangles)
{
    static const size_t MINIMUM_SOLID_VERTEX_COUNT = 4u;
    static const size_t MINIMUM_SOLID_TRIANGLE_COUNT = 4u;
    struct edge_use {
        size_t count;
        int orientation;
    };
    std::map<std::pair<uint32_t, uint32_t>, struct edge_use> edges;

    if (vertices.size() < MINIMUM_SOLID_VERTEX_COUNT ||
        triangles.size() < MINIMUM_SOLID_TRIANGLE_COUNT)
        return 0;

    for (size_t triangle_index = 0; triangle_index < triangles.size(); ++triangle_index) {
        const struct creo_brl_snapshot_triangle& triangle = triangles[triangle_index];

        for (size_t corner = 0;
             corner < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++corner) {
            const uint32_t from = triangle.vertices[corner];
            const uint32_t to = triangle.vertices[
                (corner + 1u) % CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT];

            if (from >= vertices.size() || to >= vertices.size() || from == to)
                return 0;
            const std::pair<uint32_t, uint32_t> key =
                from < to ? std::make_pair(from, to) : std::make_pair(to, from);
            struct edge_use& use = edges[key];

            ++use.count;
            use.orientation += from < to ? 1 : -1;
        }
    }

    for (std::map<std::pair<uint32_t, uint32_t>, struct edge_use>::const_iterator edge =
             edges.begin(); edge != edges.end(); ++edge) {
        if (edge->second.count != 2u || edge->second.orientation != 0)
            return 0;
    }
    return !edges.empty();
}

} // namespace


int
creo_brl_normalize_mesh(
    const std::vector<struct creo_brl_snapshot_vector>& source_vertices,
    const std::vector<struct creo_brl_snapshot_triangle>& source_triangles,
    struct creo_brl_mesh_normalization *mesh)
{
    struct creo_brl_mesh_normalization normalized = {};
    mesh_vertex_index vertex_index;

    if (!mesh)
        return 0;
    *mesh = creo_brl_mesh_normalization();
    if (source_vertices.empty() || source_triangles.empty())
        return 0;

    normalized.triangles.reserve(source_triangles.size());
    normalized.source_triangle_indices.reserve(source_triangles.size());
    for (size_t triangle_index = 0;
         triangle_index < source_triangles.size(); ++triangle_index) {
        const struct creo_brl_snapshot_triangle& source =
            source_triangles[triangle_index];
        struct creo_brl_snapshot_triangle triangle = {};

        for (size_t corner = 0;
             corner < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++corner) {
            if (source.vertices[corner] >= source_vertices.size() ||
                !vertex_index.add(source_vertices[source.vertices[corner]],
                                  &triangle.vertices[corner]))
                return 0;
        }

        if (!mesh_triangle_is_accepted(triangle, vertex_index.vertices()))
            continue;
        normalized.triangles.push_back(triangle);
        normalized.source_triangle_indices.push_back(triangle_index);
    }

    if (normalized.triangles.empty())
        return 0;

    normalized.vertices = vertex_index.vertices();
    normalized.is_solid = mesh_is_solid(normalized.vertices,
                                        normalized.triangles);
    *mesh = std::move(normalized);
    return 1;
}
