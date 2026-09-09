/*                           N E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/arrange/nest.cpp
 *
 * Arbitrary-shape spatial nesting for the arrange command.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "rt/calc.h"
#include "wdb.h"
}

#include "arrange_private.h"
#include "../ged_private.h"

namespace arrange {

namespace {

const char *usage =
    "arrange nest [-n] [-C] [-d 2|3] [-a x|y|z] [-s cell_size] [-c clearance]\n"
    "             [-o fixed|upright|orthogonal] [-q fast|balanced|thorough]\n"
    "             [-g x,y,z] [--value name=value]...\n"
    "             output container object [object ...]";

void
print_help(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str,
        "Usage: %s\n\n"
        "Pack ray-traceable objects into an arbitrary permitted volume.  In 2D\n"
        "mode, sampled orthographic silhouettes are packed in the plane\n"
        "normal to -a.  Repeating a name requests another instance.\n\n"
        "Options:\n"
        "  -n                         dry run; report without writing output\n"
        "  -C, --conservative         use full object bounds for collision\n"
        "  -d 2|3                     planar or spatial nesting (default: 3)\n"
        "  -a x|y|z                   projection/upright axis (default: z)\n"
        "  -s size                    isotropic raster cell size in local units\n"
        "  -c clearance               minimum raster clearance in local units\n"
        "  -o fixed|upright|orthogonal allowed orientation set\n"
        "  -q fast|balanced|thorough search work budget\n"
        "  -g x,y,z                   settle feasible candidates along gravity\n"
        "  --value name=value         geometric-knapsack value override\n\n"
        "The output is a non-region combination of transformed source instances.\n"
        "Containment and separation are guaranteed at the reported raster size.\n",
        usage);
}

bool
parse_double(const char *text, double &value)
{
    if (!text || !text[0])
        return false;
    errno = 0;
    char *end = nullptr;
    value = std::strtod(text, &end);
    return errno == 0 && end && *end == '\0' && std::isfinite(value);
}

bool
parse_axis(const char *text, int &axis)
{
    if (BU_STR_EQUAL(text, "x")) axis = X;
    else if (BU_STR_EQUAL(text, "y")) axis = Y;
    else if (BU_STR_EQUAL(text, "z")) axis = Z;
    else return false;
    return true;
}

bool
parse_gravity(const char *text, std::array<double, 3> &gravity)
{
    std::string normalized(text ? text : "");
    std::replace(normalized.begin(), normalized.end(), '/', ',');
    std::replace(normalized.begin(), normalized.end(), ' ', ',');
    char trailing = '\0';
    int matched = std::sscanf(normalized.c_str(), "%lf,%lf,%lf%c",
        &gravity[X], &gravity[Y], &gravity[Z], &trailing);
    return matched == 3 && std::isfinite(gravity[X]) &&
        std::isfinite(gravity[Y]) && std::isfinite(gravity[Z]) &&
        (gravity[X] != 0.0 || gravity[Y] != 0.0 || gravity[Z] != 0.0);
}

bool
parse_value(const std::string &argument, std::map<std::string, double> &values)
{
    std::size_t separator = argument.rfind('=');
    if (separator == std::string::npos || separator == 0 ||
            separator + 1 == argument.size())
        return false;
    double value = 0.0;
    if (!parse_double(argument.c_str() + separator + 1, value) || value < 0.0)
        return false;
    values[argument.substr(0, separator)] = value;
    return true;
}

bool
parse_options(struct ged *gedp, int argc, const char *argv[], NestOptions &options,
        bool &help)
{
    help = false;
    int index = 1;
    while (index < argc) {
        const char *argument = argv[index];
        if (BU_STR_EQUAL(argument, "--")) {
            ++index;
            break;
        }
        if (argument[0] != '-')
            break;
        if (BU_STR_EQUAL(argument, "--help") || BU_STR_EQUAL(argument, "-h")) {
            help = true;
            return true;
        }
        if (BU_STR_EQUAL(argument, "-n")) {
            options.dry_run = true;
            ++index;
            continue;
        }
        if (BU_STR_EQUAL(argument, "-C") ||
                BU_STR_EQUAL(argument, "--conservative")) {
            options.conservative = true;
            ++index;
            continue;
        }

        const char *value = nullptr;
        bool inline_value = false;
        if (std::strncmp(argument, "--value=", 8) == 0) {
            value = argument + 8;
            inline_value = true;
        } else {
            if (index + 1 >= argc) {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: option '%s' requires an argument\n", argument);
                return false;
            }
            value = argv[index + 1];
        }

        if (BU_STR_EQUAL(argument, "-d")) {
            if (BU_STR_EQUAL(value, "2")) options.dimension = Dimension::PLANAR;
            else if (BU_STR_EQUAL(value, "3")) options.dimension = Dimension::SPATIAL;
            else {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: dimension must be 2 or 3\n");
                return false;
            }
        } else if (BU_STR_EQUAL(argument, "-a")) {
            if (!parse_axis(value, options.axis)) {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: axis must be x, y, or z\n");
                return false;
            }
        } else if (BU_STR_EQUAL(argument, "-s")) {
            if (!parse_double(value, options.cell_size) || options.cell_size <= 0.0) {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: cell size must be positive\n");
                return false;
            }
        } else if (BU_STR_EQUAL(argument, "-c")) {
            if (!parse_double(value, options.clearance) || options.clearance < 0.0) {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: clearance must be nonnegative\n");
                return false;
            }
        } else if (BU_STR_EQUAL(argument, "-o")) {
            options.orientation_set = true;
            if (BU_STR_EQUAL(value, "fixed"))
                options.orientation = OrientationMode::FIXED;
            else if (BU_STR_EQUAL(value, "upright"))
                options.orientation = OrientationMode::UPRIGHT;
            else if (BU_STR_EQUAL(value, "orthogonal"))
                options.orientation = OrientationMode::ORTHOGONAL;
            else {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: unknown orientation set '%s'\n", value);
                return false;
            }
        } else if (BU_STR_EQUAL(argument, "-q")) {
            if (BU_STR_EQUAL(value, "fast")) options.quality = Quality::FAST;
            else if (BU_STR_EQUAL(value, "balanced")) options.quality = Quality::BALANCED;
            else if (BU_STR_EQUAL(value, "thorough")) options.quality = Quality::THOROUGH;
            else {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: quality must be fast, balanced, or thorough\n");
                return false;
            }
        } else if (BU_STR_EQUAL(argument, "-g")) {
            if (!parse_gravity(value, options.gravity)) {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: gravity must be a nonzero x,y,z vector\n");
                return false;
            }
            options.gravity_enabled = true;
        } else if (BU_STR_EQUAL(argument, "--value") || inline_value) {
            if (!parse_value(value, options.values)) {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: value must have the form name=nonnegative-number\n");
                return false;
            }
        } else {
            bu_vls_printf(gedp->ged_result_str,
                "arrange nest: unknown option '%s'\n", argument);
            return false;
        }
        index += inline_value ? 1 : 2;
    }

    if (argc - index < 3) {
        bu_vls_printf(gedp->ged_result_str,
            "arrange nest: output, container, and at least one object are required\n");
        return false;
    }
    options.output = argv[index++];
    options.container = argv[index++];
    while (index < argc)
        options.objects.emplace_back(argv[index++]);
    if (options.dimension == Dimension::PLANAR && !options.orientation_set)
        options.orientation = OrientationMode::UPRIGHT;
    if (options.dimension == Dimension::PLANAR &&
            options.orientation == OrientationMode::ORTHOGONAL) {
        bu_vls_printf(gedp->ged_result_str,
            "arrange nest: 2D nesting supports fixed or upright orientations\n");
        return false;
    }
    return true;
}

const char *
quality_name(Quality quality)
{
    switch (quality) {
        case Quality::FAST: return "fast";
        case Quality::BALANCED: return "balanced";
        case Quality::THOROUGH: return "thorough";
    }
    return "balanced";
}

bool
same_raster(const Raster &left, const Raster &right)
{
    return left.dims == right.dims && left.bits == right.bits;
}

std::vector<OrientedRaster>
build_orientations(const Raster &source, const NestOptions &options,
        int clearance_cells)
{
    std::vector<OrientedRaster> result;
    std::vector<AxisTransform> transforms = orientation_transforms(
        options.orientation, options.axis);
    for (std::size_t index = 0; index < transforms.size(); ++index) {
        OrientedRaster orientation;
        orientation.transform = transforms[index];
        orientation.orientation_index = static_cast<int>(index);
        orientation.shape = orient_raster(source, transforms[index]);
        if (options.conservative) {
            Raster bounds(orientation.shape.dims);
            bounds.model_min = orientation.shape.model_min;
            bounds.model_max = orientation.shape.model_max;
            for (int z = 0; z < bounds.dims[Z]; ++z)
                for (int y = 0; y < bounds.dims[Y]; ++y)
                    for (int x = 0; x < bounds.dims[X]; ++x)
                        bounds.set(x, y, z);
            orientation.shape = std::move(bounds);
        }
        bool duplicate = false;
        for (const OrientedRaster &existing : result) {
            if (same_raster(existing.shape, orientation.shape)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        orientation.collision = dilate_raster(orientation.shape, clearance_cells,
            options.dimension, options.axis, orientation.pad);
        result.push_back(std::move(orientation));
    }
    return result;
}

} // namespace

int
ged_arrange_nest(struct ged *gedp, int argc, const char *argv[])
{
    NestOptions options;
    bool help = false;
    if (!parse_options(gedp, argc, argv, options, help)) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage);
        return BRLCAD_ERROR;
    }
    if (help) {
        print_help(gedp);
        return GED_HELP;
    }

    if (!options.dry_run)
        GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    if (db_lookup(gedp->dbip, options.output.c_str(), LOOKUP_QUIET) != RT_DIR_NULL) {
        bu_vls_printf(gedp->ged_result_str,
            "arrange nest: output '%s' already exists\n", options.output.c_str());
        return BRLCAD_ERROR;
    }

    std::set<std::string> required = {options.container};
    required.insert(options.objects.begin(), options.objects.end());
    for (const std::string &name : required) {
        if (db_lookup(gedp->dbip, name.c_str(), LOOKUP_QUIET) == RT_DIR_NULL) {
            bu_vls_printf(gedp->ged_result_str,
                "arrange nest: object '%s' does not exist\n", name.c_str());
            return BRLCAD_ERROR;
        }
    }
    for (const auto &entry : options.values) {
        if (!required.count(entry.first) || entry.first == options.container) {
            bu_vls_printf(gedp->ged_result_str,
                "arrange nest: value names an input object, not '%s'\n",
                entry.first.c_str());
            return BRLCAD_ERROR;
        }
    }

    if (options.cell_size == 0.0) {
        point_t minimum;
        point_t maximum;
        const char *container_name = options.container.c_str();
        struct bu_vls messages = BU_VLS_INIT_ZERO;
        if (rt_obj_bounds(&messages, gedp->dbip, 1, &container_name, 0,
                minimum, maximum) == BRLCAD_ERROR) {
            bu_vls_printf(gedp->ged_result_str,
                "arrange nest: cannot bound container '%s': %s",
                container_name, bu_vls_cstr(&messages));
            bu_vls_free(&messages);
            return BRLCAD_ERROR;
        }
        bu_vls_free(&messages);
        double longest = std::max({maximum[X] - minimum[X],
            maximum[Y] - minimum[Y], maximum[Z] - minimum[Z]});
        options.cell_size = longest / DEFAULT_LONG_AXIS_CELLS /
            gedp->dbip->dbi_local2base;
    }

    options.cell_size *= gedp->dbip->dbi_local2base;
    options.clearance *= gedp->dbip->dbi_local2base;
    int clearance_cells = static_cast<int>(std::ceil(
        options.clearance / options.cell_size));

    Raster allowed;
    std::string error;
    if (!rasterize_object(gedp->dbip, options.container, options.cell_size,
            options.dimension, options.axis, allowed, error)) {
        bu_vls_printf(gedp->ged_result_str, "arrange nest: %s\n", error.c_str());
        return BRLCAD_ERROR;
    }

    std::map<std::string, std::shared_ptr<const ItemGeometry>> geometry_cache;
    std::vector<Item> items;
    items.reserve(options.objects.size());
    for (std::size_t index = 0; index < options.objects.size(); ++index) {
        const std::string &name = options.objects[index];
        auto cached = geometry_cache.find(name);
        if (cached == geometry_cache.end()) {
            auto geometry = std::make_shared<ItemGeometry>();
            if (!rasterize_object(gedp->dbip, name, options.cell_size,
                    options.dimension, options.axis, geometry->source, error,
                    options.conservative)) {
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: %s\n", error.c_str());
                return BRLCAD_ERROR;
            }
            geometry->orientations = build_orientations(geometry->source, options,
                clearance_cells);
            cached = geometry_cache.emplace(name, std::move(geometry)).first;
        }

        Item item;
        item.name = name;
        item.source_index = index;
        item.geometry = cached->second;
        auto value = options.values.find(name);
        item.value = value == options.values.end() ?
            static_cast<double>(item.geometry->source.count()) : value->second;
        items.push_back(std::move(item));
    }

    SearchState state = search_arrangement(allowed, items, options);
    if (state.placements.empty()) {
        bu_vls_printf(gedp->ged_result_str,
            "arrange nest: no input object fits the container at cell size %.9g %s\n",
            options.cell_size * gedp->dbip->dbi_base2local,
            bu_units_string(gedp->dbip->dbi_local2base));
        return BRLCAD_ERROR;
    }

    std::vector<bool> placed(items.size(), false);
    for (Placement &placement : state.placements) {
        const Item &item = items[placement.item_index];
        const OrientedRaster &orientation =
            item.geometry->orientations[placement.orientation_index];
        placement.matrix = placement_matrix(item.geometry->source, orientation,
            allowed, placement.position, options.cell_size, options.dimension,
            options.axis);
        placed[placement.item_index] = true;
    }

    if (!options.dry_run) {
        struct wmember members;
        BU_LIST_INIT(&members.l);
        for (const Placement &placement : state.placements) {
            mat_t matrix;
            for (int i = 0; i < 16; ++i)
                matrix[i] = placement.matrix[i];
            if (!mk_addmember(items[placement.item_index].name.c_str(),
                    &members.l, matrix, WMOP_UNION)) {
                mk_freemembers(&members.l);
                bu_vls_printf(gedp->ged_result_str,
                    "arrange nest: unable to assemble output members\n");
                return BRLCAD_ERROR;
            }
        }
        struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
        if (mk_comb(wdbp, options.output.c_str(), &members.l, 0, NULL, NULL,
                NULL, 0, 0, 0, 0, 0, 0, 0) < 0) {
            bu_vls_printf(gedp->ged_result_str,
                "arrange nest: failed to write output '%s'\n",
                options.output.c_str());
            return BRLCAD_ERROR;
        }
    }

    double fill = allowed.count() ?
        100.0 * state.occupied_cells / allowed.count() : 0.0;
    bu_vls_printf(gedp->ged_result_str,
        "arrange nest: placed %zu of %zu object%s; value %.9g; raster fill %.2f%%\n"
        "  dimension: %dD; cell size: %.9g %s; clearance: %d cell%s; quality: %s%s%s\n",
        state.placements.size(), items.size(), items.size() == 1 ? "" : "s",
        state.value, fill, static_cast<int>(options.dimension),
        options.cell_size * gedp->dbip->dbi_base2local,
        bu_units_string(gedp->dbip->dbi_local2base), clearance_cells,
        clearance_cells == 1 ? "" : "s", quality_name(options.quality),
        options.conservative ? "; conservative bounds" : "",
        options.dry_run ? "; dry run (no output written)" : "");

    if (state.placements.size() != items.size()) {
        bu_vls_printf(gedp->ged_result_str, "  unplaced:");
        for (std::size_t i = 0; i < items.size(); ++i)
            if (!placed[i])
                bu_vls_printf(gedp->ged_result_str, " %s", items[i].name.c_str());
        bu_vls_printf(gedp->ged_result_str, "\n");
    }
    return BRLCAD_OK;
}

} // namespace arrange
