/*                    N E S T _ R A S T E R . C P P
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
#include <limits>
#include <set>

extern "C" {
#include "analyze.h"
#include "raytrace.h"
}

#include "arrange_private.h"

namespace arrange {

namespace {

std::size_t
word_index(const Raster &raster, int x, int y, int z)
{
    return (static_cast<std::size_t>(z) * raster.dims[Y] + y) *
        raster.row_words + x / 64;
}

int
voxel_count(double minimum, double maximum, double size)
{
    double ratio = (maximum - minimum) / size;
    int count = static_cast<int>(ratio) + 1;
    if (EQUAL(static_cast<double>(count - 1), ratio))
        --count;
    return std::max(1, count);
}

struct RasterCallbackData {
    Raster *raster = nullptr;
};

void
collect_voxel(void *callback_data, int x, int y, int z,
        const char *region_name, fastf_t fill)
{
    auto *data = static_cast<RasterCallbackData *>(callback_data);
    if (region_name && fill > 0.0)
        data->raster->set(x, y, z);
}

int
permutation_parity(const std::array<int, 3> &permutation)
{
    int inversions = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = i + 1; j < 3; ++j)
            if (permutation[i] > permutation[j])
                ++inversions;
    return (inversions % 2) ? -1 : 1;
}

AxisTransform
quarter_turn(int axis, int turns)
{
    AxisTransform transform;
    turns = ((turns % 4) + 4) % 4;
    if (turns == 0)
        return transform;

    int first = (axis + 1) % 3;
    int second = (axis + 2) % 3;
    if (turns == 1) {
        transform.source_axis[first] = second;
        transform.sign[first] = -1;
        transform.source_axis[second] = first;
    } else if (turns == 2) {
        transform.sign[first] = -1;
        transform.sign[second] = -1;
    } else {
        transform.source_axis[first] = second;
        transform.source_axis[second] = first;
        transform.sign[second] = -1;
    }
    return transform;
}

Raster
crop_raster(const Raster &source, double cell_size)
{
    if (source.cells.empty())
        return source;

    std::array<int, 3> minimum = {{source.dims[X], source.dims[Y], source.dims[Z]}};
    std::array<int, 3> maximum = {{-1, -1, -1}};
    for (const Cell &cell : source.cells) {
        const int coordinate[3] = {cell.x, cell.y, cell.z};
        for (int axis = 0; axis < 3; ++axis) {
            minimum[axis] = std::min(minimum[axis], coordinate[axis]);
            maximum[axis] = std::max(maximum[axis], coordinate[axis]);
        }
    }

    std::array<int, 3> dims;
    for (int axis = 0; axis < 3; ++axis)
        dims[axis] = maximum[axis] - minimum[axis] + 1;
    Raster cropped(dims);
    for (int axis = 0; axis < 3; ++axis) {
        cropped.model_min[axis] = source.model_min[axis] + minimum[axis] * cell_size;
        cropped.model_max[axis] = source.model_min[axis] +
            (maximum[axis] + 1) * cell_size;
    }
    for (const Cell &cell : source.cells)
        cropped.set(cell.x - minimum[X], cell.y - minimum[Y], cell.z - minimum[Z]);
    return cropped;
}

} // namespace

Raster::Raster(const std::array<int, 3> &sizes) : dims(sizes)
{
    row_words = (dims[X] + 63) / 64;
    if (valid()) {
        std::size_t words = static_cast<std::size_t>(row_words) * dims[Y] * dims[Z];
        bits.assign(words, 0);
    }
}

bool
Raster::valid() const
{
    return dims[X] > 0 && dims[Y] > 0 && dims[Z] > 0;
}

bool
Raster::contains(int x, int y, int z) const
{
    return x >= 0 && y >= 0 && z >= 0 && x < dims[X] && y < dims[Y] &&
        z < dims[Z];
}

bool
Raster::get(int x, int y, int z) const
{
    if (!contains(x, y, z))
        return false;
    std::uint64_t mask = UINT64_C(1) << (x & 63);
    return (bits[word_index(*this, x, y, z)] & mask) != 0;
}

bool
Raster::set(int x, int y, int z)
{
    if (!contains(x, y, z))
        return false;
    std::uint64_t mask = UINT64_C(1) << (x & 63);
    std::uint64_t &word = bits[word_index(*this, x, y, z)];
    if (word & mask)
        return false;
    word |= mask;
    cells.push_back({x, y, z});
    return true;
}

std::size_t
Raster::count() const
{
    return cells.size();
}

bool
rasterize_object(struct db_i *dbip, const std::string &name, double cell_size,
        Dimension dimension, int projection_axis, Raster &result,
        std::string &error, bool preserve_bounds)
{
    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip) {
        error = "unable to create ray-trace instance";
        return false;
    }
    rtip->useair = 0;
    if (rt_gettree(rtip, name.c_str()) < 0) {
        error = "unable to prepare object '" + name + "'";
        rt_i_destroy(rtip);
        return false;
    }

    rt_prep_parallel(rtip, 1);
    std::array<int, 3> dims;
    for (int axis = 0; axis < 3; ++axis)
        dims[axis] = voxel_count(rtip->mdl_min[axis], rtip->mdl_max[axis], cell_size);

    std::size_t grid_cells = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (grid_cells > MAX_GRID_CELLS / static_cast<std::size_t>(dims[axis])) {
            error = "object '" + name + "' exceeds the raster cell limit";
            rt_i_destroy(rtip);
            return false;
        }
        grid_cells *= static_cast<std::size_t>(dims[axis]);
    }

    fastf_t voxel_size[3] = {cell_size, cell_size, cell_size};
    Raster spatial;
    auto sample = [&](int samples_per_axis) {
        spatial = Raster(dims);
        for (int axis = 0; axis < 3; ++axis) {
            spatial.model_min[axis] = rtip->mdl_min[axis];
            spatial.model_max[axis] = rtip->mdl_max[axis];
        }
        RasterCallbackData callback_data = {&spatial};
        voxelize(rtip, voxel_size, samples_per_axis, collect_voxel,
            &callback_data);
    };
    sample(VOXEL_SAMPLES_PER_AXIS);
    if (spatial.count() == 0)
        sample(VOXEL_RETRY_SAMPLES_PER_AXIS);
    if (spatial.count() == 0)
        sample(VOXEL_FINAL_RETRY_SAMPLES_PER_AXIS);
    rt_i_destroy(rtip);

    if (spatial.count() == 0) {
        error = "object '" + name + "' has no occupied cells at the requested resolution";
        return false;
    }

    if (!preserve_bounds)
        spatial = crop_raster(spatial, cell_size);

    if (dimension == Dimension::SPATIAL) {
        result = std::move(spatial);
        return true;
    }

    dims = spatial.dims;
    dims[projection_axis] = 1;
    Raster projected(dims);
    projected.model_min = spatial.model_min;
    projected.model_max = spatial.model_max;
    for (const Cell &cell : spatial.cells) {
        std::array<int, 3> coordinate = {{cell.x, cell.y, cell.z}};
        coordinate[projection_axis] = 0;
        projected.set(coordinate[X], coordinate[Y], coordinate[Z]);
    }
    result = std::move(projected);
    return true;
}

Raster
orient_raster(const Raster &source, const AxisTransform &transform)
{
    std::array<int, 3> dims;
    for (int axis = 0; axis < 3; ++axis)
        dims[axis] = source.dims[transform.source_axis[axis]];

    Raster oriented(dims);
    oriented.model_min = source.model_min;
    oriented.model_max = source.model_max;
    for (const Cell &cell : source.cells) {
        std::array<int, 3> input = {{cell.x, cell.y, cell.z}};
        std::array<int, 3> output;
        for (int axis = 0; axis < 3; ++axis) {
            int value = input[transform.source_axis[axis]];
            output[axis] = transform.sign[axis] > 0 ? value : dims[axis] - 1 - value;
        }
        oriented.set(output[X], output[Y], output[Z]);
    }
    return oriented;
}

Raster
dilate_raster(const Raster &source, int radius, Dimension dimension,
        int projection_axis, std::array<int, 3> &pad)
{
    pad = {{radius, radius, radius}};
    if (dimension == Dimension::PLANAR)
        pad[projection_axis] = 0;

    std::array<int, 3> dims;
    for (int axis = 0; axis < 3; ++axis)
        dims[axis] = source.dims[axis] + 2 * pad[axis];
    Raster dilated(dims);
    dilated.model_min = source.model_min;
    dilated.model_max = source.model_max;

    for (const Cell &cell : source.cells) {
        for (int dz = -pad[Z]; dz <= pad[Z]; ++dz) {
            for (int dy = -pad[Y]; dy <= pad[Y]; ++dy) {
                for (int dx = -pad[X]; dx <= pad[X]; ++dx) {
                    dilated.set(cell.x + pad[X] + dx, cell.y + pad[Y] + dy,
                        cell.z + pad[Z] + dz);
                }
            }
        }
    }
    return dilated;
}

std::vector<AxisTransform>
orientation_transforms(OrientationMode mode, int upright_axis)
{
    if (mode == OrientationMode::FIXED)
        return {AxisTransform()};
    if (mode == OrientationMode::UPRIGHT) {
        std::vector<AxisTransform> transforms;
        for (int turns = 0; turns < 4; ++turns)
            transforms.push_back(quarter_turn(upright_axis, turns));
        return transforms;
    }

    std::vector<AxisTransform> transforms;
    std::array<int, 3> permutation = {{0, 1, 2}};
    do {
        int parity = permutation_parity(permutation);
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                for (int sz : {-1, 1}) {
                    if (parity * sx * sy * sz != 1)
                        continue;
                    AxisTransform transform;
                    transform.source_axis = permutation;
                    transform.sign = {{sx, sy, sz}};
                    transforms.push_back(transform);
                }
            }
        }
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    return transforms;
}

std::array<double, 16>
placement_matrix(const Raster &source, const OrientedRaster &orientation,
        const Raster &container, const Cell &position, double cell_size,
        Dimension dimension, int projection_axis)
{
    std::array<double, 16> matrix = {{0.0}};
    matrix[15] = 1.0;
    for (int output_axis = 0; output_axis < 3; ++output_axis) {
        int source_axis = orientation.transform.source_axis[output_axis];
        matrix[output_axis * 4 + source_axis] = orientation.transform.sign[output_axis];
    }

    std::array<double, 3> center;
    std::array<double, 3> oriented_min;
    std::array<int, 3> coordinate = {{position.x, position.y, position.z}};
    for (int axis = 0; axis < 3; ++axis) {
        center[axis] = 0.5 * (source.model_min[axis] + source.model_max[axis]);
        oriented_min[axis] = center[axis] -
            0.5 * orientation.shape.dims[axis] * cell_size;
    }

    for (int axis = 0; axis < 3; ++axis) {
        double rotated_center = 0.0;
        for (int source_axis = 0; source_axis < 3; ++source_axis)
            rotated_center += matrix[axis * 4 + source_axis] * center[source_axis];
        double shift = 0.0;
        if (dimension == Dimension::SPATIAL || axis != projection_axis) {
            double target_min = container.model_min[axis] +
                (coordinate[axis] + orientation.pad[axis]) * cell_size;
            shift = target_min - oriented_min[axis];
        }
        matrix[axis * 4 + 3] = center[axis] - rotated_center + shift;
    }
    return matrix;
}

} // namespace arrange
