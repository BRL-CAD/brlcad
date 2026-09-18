/*                   A R R A N G E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGED_ARRANGE_PRIVATE_H
#define LIBGED_ARRANGE_PRIVATE_H

#include "common.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "ged.h"
#include "bu/cmdschema.h"
#include "vmath.h"
}

namespace arrange {

constexpr int DEFAULT_LONG_AXIS_CELLS = 128;
constexpr int VOXEL_SAMPLES_PER_AXIS = 8;
constexpr int VOXEL_RETRY_SAMPLES_PER_AXIS = 32;
constexpr int VOXEL_FINAL_RETRY_SAMPLES_PER_AXIS = 128;
constexpr std::size_t FAST_CANDIDATE_BUDGET = 512;
constexpr std::size_t BALANCED_CANDIDATE_BUDGET = 4000;
constexpr std::size_t THOROUGH_CANDIDATE_BUDGET = 12000;
constexpr std::size_t BALANCED_BEAM_WIDTH = 4;
constexpr std::size_t THOROUGH_BEAM_WIDTH = 6;
constexpr std::size_t THOROUGH_RESTARTS = 3;
constexpr std::size_t THOROUGH_LNS_STEPS = 8;
constexpr std::size_t MAX_GRID_CELLS = 64 * 1024 * 1024;

enum class Dimension {
    PLANAR = 2,
    SPATIAL = 3
};

enum class OrientationMode {
    FIXED,
    UPRIGHT,
    ORTHOGONAL
};

enum class Quality {
    FAST,
    BALANCED,
    THOROUGH
};

struct Cell {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct Raster {
    std::array<int, 3> dims = {{0, 0, 0}};
    std::array<double, 3> model_min = {{0.0, 0.0, 0.0}};
    std::array<double, 3> model_max = {{0.0, 0.0, 0.0}};
    int row_words = 0;
    std::vector<std::uint64_t> bits;
    std::vector<Cell> cells;

    Raster() = default;
    explicit Raster(const std::array<int, 3> &sizes);

    bool valid() const;
    bool contains(int x, int y, int z) const;
    bool get(int x, int y, int z) const;
    bool set(int x, int y, int z);
    std::size_t count() const;
};

struct AxisTransform {
    std::array<int, 3> source_axis = {{0, 1, 2}};
    std::array<int, 3> sign = {{1, 1, 1}};
};

struct OrientedRaster {
    Raster shape;
    Raster collision;
    AxisTransform transform;
    std::array<int, 3> pad = {{0, 0, 0}};
    int orientation_index = 0;
};

struct ItemGeometry {
    Raster source;
    std::vector<OrientedRaster> orientations;
};

struct Item {
    std::string name;
    std::size_t source_index = 0;
    double value = 0.0;
    std::shared_ptr<const ItemGeometry> geometry;
};

struct Placement {
    std::size_t item_index = 0;
    std::size_t orientation_index = 0;
    Cell position;
    std::array<double, 16> matrix = {{0.0}};
    int contact_faces = 0;
    std::size_t envelope = 0;
};

struct SearchState {
    Raster occupied;
    std::vector<Placement> placements;
    double value = 0.0;
    std::size_t occupied_cells = 0;
    long long contact_faces = 0;
    std::array<int, 3> envelope_min = {{0, 0, 0}};
    std::array<int, 3> envelope_max = {{-1, -1, -1}};
};

struct NestOptions {
    Dimension dimension = Dimension::SPATIAL;
    OrientationMode orientation = OrientationMode::ORTHOGONAL;
    Quality quality = Quality::BALANCED;
    int axis = Z;
    double cell_size = 0.0;
    double clearance = 0.0;
    bool dry_run = false;
    bool conservative = false;
    bool orientation_set = false;
    bool gravity_enabled = false;
    std::array<double, 3> gravity = {{0.0, 0.0, -1.0}};
    std::map<std::string, double> values;
    std::string output;
    std::string container;
    std::vector<std::string> objects;
};

struct CandidateSet {
    std::vector<Cell> positions;
    int stride = 1;
};

extern const struct bu_cmd_schema ged_arrange_nest_schema;
int ged_arrange_nest(struct ged *gedp, int argc, const char *argv[]);

bool rasterize_object(struct db_i *dbip, const std::string &name, double cell_size,
        Dimension dimension, int projection_axis, Raster &result,
        std::string &error, bool preserve_bounds = false);
Raster orient_raster(const Raster &source, const AxisTransform &transform);
Raster dilate_raster(const Raster &source, int radius, Dimension dimension,
        int projection_axis, std::array<int, 3> &pad);
std::vector<AxisTransform> orientation_transforms(OrientationMode mode,
        int upright_axis);
std::array<double, 16> placement_matrix(const Raster &source,
        const OrientedRaster &orientation, const Raster &container,
        const Cell &position, double cell_size, Dimension dimension,
        int projection_axis);

CandidateSet generate_candidates(const Raster &allowed,
        const SearchState &state, const OrientedRaster &orientation,
        Dimension dimension, int projection_axis, std::size_t budget);

bool placement_feasible(const Raster &allowed, const SearchState &state,
        const OrientedRaster &orientation, const Cell &position);
std::vector<Placement> rank_placements(const Raster &allowed,
        const SearchState &state, const Item &item, std::size_t item_index,
        const NestOptions &options, std::size_t maximum_results);
void apply_placement(SearchState &state, const Item &item,
        const Placement &placement);
SearchState search_arrangement(const Raster &allowed,
        const std::vector<Item> &items, const NestOptions &options);
bool better_state(const SearchState &left, const SearchState &right);

} // namespace arrange

#endif /* LIBGED_ARRANGE_PRIVATE_H */
