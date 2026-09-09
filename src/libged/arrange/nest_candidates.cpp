/*                 N E S T _ C A N D I D A T E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <tuple>
#include <vector>

#include "arrange_private.h"

namespace arrange {

namespace {

std::vector<int>
axis_samples(int maximum, int stride, bool movable)
{
    if (!movable || maximum <= 0)
        return {0};

    std::vector<int> samples;
    for (int value = 0; value <= maximum; value += stride)
        samples.push_back(value);
    if (samples.back() != maximum)
        samples.push_back(maximum);
    return samples;
}

std::array<Cell, 6>
directional_extrema(const OrientedRaster &orientation)
{
    std::array<Cell, 6> extrema;
    if (orientation.shape.cells.empty())
        return extrema;

    extrema.fill(orientation.shape.cells.front());
    for (const Cell &cell : orientation.shape.cells) {
        if (cell.x < extrema[0].x) extrema[0] = cell;
        if (cell.x > extrema[1].x) extrema[1] = cell;
        if (cell.y < extrema[2].y) extrema[2] = cell;
        if (cell.y > extrema[3].y) extrema[3] = cell;
        if (cell.z < extrema[4].z) extrema[4] = cell;
        if (cell.z > extrema[5].z) extrema[5] = cell;
    }
    return extrema;
}

bool
boundary_cell(const Raster &allowed, const Cell &cell, Dimension dimension,
        int projection_axis)
{
    const int directions[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
        {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
    };
    for (int direction = 0; direction < 6; ++direction) {
        int changed_axis = direction / 2;
        if (dimension == Dimension::PLANAR && changed_axis == projection_axis)
            continue;
        int x = cell.x + directions[direction][X];
        int y = cell.y + directions[direction][Y];
        int z = cell.z + directions[direction][Z];
        if (!allowed.get(x, y, z))
            return true;
    }
    return false;
}

} // namespace

CandidateSet
generate_candidates(const Raster &allowed, const SearchState &state,
        const OrientedRaster &orientation, Dimension dimension,
        int projection_axis, std::size_t budget)
{
    CandidateSet result;
    std::array<int, 3> maximum;
    for (int axis = 0; axis < 3; ++axis) {
        maximum[axis] = allowed.dims[axis] - orientation.collision.dims[axis];
        if (maximum[axis] < 0)
            return result;
        if (dimension == Dimension::PLANAR && axis == projection_axis)
            maximum[axis] = 0;
    }

    double positions = 1.0;
    int active_dimensions = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (dimension == Dimension::PLANAR && axis == projection_axis)
            continue;
        positions *= maximum[axis] + 1.0;
        ++active_dimensions;
    }
    if (positions > static_cast<double>(budget)) {
        double ratio = positions / static_cast<double>(budget);
        result.stride = std::max(1, static_cast<int>(std::ceil(
            std::pow(ratio, 1.0 / active_dimensions))));
    }

    std::set<std::tuple<int, int, int>> unique;
    auto add = [&](int x, int y, int z) {
        if (x < 0 || y < 0 || z < 0 || x > maximum[X] ||
                y > maximum[Y] || z > maximum[Z])
            return;
        if (dimension == Dimension::PLANAR) {
            int coordinate[3] = {x, y, z};
            if (coordinate[projection_axis] != 0)
                return;
        }
        if (unique.emplace(x, y, z).second)
            result.positions.push_back({x, y, z});
    };

    std::vector<int> xs = axis_samples(maximum[X], result.stride,
        dimension == Dimension::SPATIAL || projection_axis != X);
    std::vector<int> ys = axis_samples(maximum[Y], result.stride,
        dimension == Dimension::SPATIAL || projection_axis != Y);
    std::vector<int> zs = axis_samples(maximum[Z], result.stride,
        dimension == Dimension::SPATIAL || projection_axis != Z);
    for (int z : zs)
        for (int y : ys)
            for (int x : xs)
                add(x, y, z);

    const int directions[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
        {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
    };
    std::array<Cell, 6> extrema = directional_extrema(orientation);

    auto add_contacts = [&](const Cell &frontier) {
        for (int direction = 0; direction < 6; ++direction) {
            int axis = direction / 2;
            if (dimension == Dimension::PLANAR && axis == projection_axis)
                continue;
            int object_extreme = direction ^ 1;
            int x = frontier.x + directions[direction][X] -
                extrema[object_extreme].x - orientation.pad[X];
            int y = frontier.y + directions[direction][Y] -
                extrema[object_extreme].y - orientation.pad[Y];
            int z = frontier.z + directions[direction][Z] -
                extrema[object_extreme].z - orientation.pad[Z];
            add(x, y, z);
        }
    };

    std::size_t frontier_step = std::max<std::size_t>(1,
        state.occupied.cells.size() / std::max<std::size_t>(1, budget / 8));
    for (std::size_t i = 0; i < state.occupied.cells.size(); i += frontier_step)
        add_contacts(state.occupied.cells[i]);

    std::size_t boundary_step = std::max<std::size_t>(1,
        allowed.cells.size() / std::max<std::size_t>(1, budget / 8));
    for (std::size_t i = 0; i < allowed.cells.size(); i += boundary_step) {
        if (boundary_cell(allowed, allowed.cells[i], dimension, projection_axis))
            add_contacts(allowed.cells[i]);
    }

    if (budget <= FAST_CANDIDATE_BUDGET)
        return result;

    struct VoidSeed {
        Cell cell;
        int distance = 0;
    };
    std::vector<VoidSeed> seeds;
    int void_stride = std::max(1, result.stride * 2);
    for (int z : axis_samples(allowed.dims[Z] - 1, void_stride,
            dimension == Dimension::SPATIAL || projection_axis != Z)) {
        for (int y : axis_samples(allowed.dims[Y] - 1, void_stride,
                dimension == Dimension::SPATIAL || projection_axis != Y)) {
            for (int x : axis_samples(allowed.dims[X] - 1, void_stride,
                    dimension == Dimension::SPATIAL || projection_axis != X)) {
                if (!allowed.get(x, y, z) || state.occupied.get(x, y, z))
                    continue;
                int distance = std::min({x + 1, y + 1, z + 1,
                    allowed.dims[X] - x, allowed.dims[Y] - y,
                    allowed.dims[Z] - z});
                if (dimension == Dimension::PLANAR) {
                    distance = allowed.dims[X] + allowed.dims[Y] + allowed.dims[Z];
                    for (int axis = 0; axis < 3; ++axis) {
                        if (axis == projection_axis)
                            continue;
                        int coordinate = axis == X ? x : (axis == Y ? y : z);
                        distance = std::min(distance,
                            std::min(coordinate + 1, allowed.dims[axis] - coordinate));
                    }
                }
                constexpr std::size_t maximum_distance_samples = 128;
                std::size_t occupied_step = std::max<std::size_t>(1,
                    state.occupied.cells.size() / maximum_distance_samples);
                for (std::size_t occupied_index = 0;
                        occupied_index < state.occupied.cells.size();
                        occupied_index += occupied_step) {
                    const Cell &occupied = state.occupied.cells[occupied_index];
                    int candidate_distance = std::abs(x - occupied.x) +
                        std::abs(y - occupied.y) + std::abs(z - occupied.z);
                    distance = std::min(distance, candidate_distance);
                }
                seeds.push_back({{x, y, z}, distance});
            }
        }
    }
    std::stable_sort(seeds.begin(), seeds.end(), [](const VoidSeed &left,
            const VoidSeed &right) {
        if (left.distance != right.distance)
            return left.distance > right.distance;
        return std::tie(left.cell.z, left.cell.y, left.cell.x) <
            std::tie(right.cell.z, right.cell.y, right.cell.x);
    });
    std::size_t maximum_seeds = std::min<std::size_t>(16, seeds.size());
    for (std::size_t i = 0; i < maximum_seeds; ++i) {
        add(seeds[i].cell.x - orientation.collision.dims[X] / 2,
            seeds[i].cell.y - orientation.collision.dims[Y] / 2,
            seeds[i].cell.z - orientation.collision.dims[Z] / 2);
    }

    return result;
}

} // namespace arrange
