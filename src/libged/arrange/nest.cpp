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
#include <utility>

extern "C" {
#include "bu/units.h"
#include "bu/cmdschema.h"
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
    char *schema_help = bu_cmd_schema_help(&ged_arrange_nest_schema, "arrange nest");
    if (schema_help) {
        bu_vls_strcat(gedp->ged_result_str, schema_help);
        bu_free(schema_help, "arrange nest schema help");
    } else {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage);
    }
    bu_vls_strcat(gedp->ged_result_str,
        "\nPack ray-traceable objects into an arbitrary permitted volume.  In 2D\n"
        "mode, sampled orthographic silhouettes are packed in the plane\n"
        "normal to the selected axis.  Repeating a name requests another\n"
        "instance.  The output is a non-region combination of transformed\n"
        "source instances.\n");
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

struct nest_cli_args {
    int help = 0;
    int dry_run = 0;
    int conservative = 0;
    int dimension = 3;
    const char *axis = "z";
    fastf_t cell_size = 0.0;
    fastf_t clearance = 0.0;
    const char *orientation = NULL;
    const char *quality = "balanced";
    point_t gravity = {0.0, 0.0, -1.0};
    std::map<std::string, double> values;
};

static int
parse_value_option(struct bu_vls *msg, const char *arg, void *storage)
{
    std::map<std::string, double> temporary;
    auto &values = storage ? *static_cast<std::map<std::string, double> *>(storage) : temporary;
    if (arg && parse_value(arg, values))
        return 0;
    if (msg)
        bu_vls_printf(msg, "value must have the form name=nonnegative-number\n");
    return -1;
}

static const struct bu_cmd_value_keyword nest_axes[] = {
    {"x", NULL, "X axis"}, {"y", NULL, "Y axis"}, {"z", NULL, "Z axis"},
    {NULL, NULL, NULL}
};
static const struct bu_cmd_value_keyword nest_orientations[] = {
    {"fixed", NULL, "Do not rotate objects"},
    {"upright", NULL, "Rotate around the upright axis"},
    {"orthogonal", NULL, "Use orthogonal rotations"},
    {NULL, NULL, NULL}
};
static const struct bu_cmd_value_keyword nest_qualities[] = {
    {"fast", NULL, "Smaller search budget"},
    {"balanced", NULL, "Default search budget"},
    {"thorough", NULL, "Larger search budget"},
    {NULL, NULL, NULL}
};
static const struct bu_cmd_option nest_options[] = {
    BU_CMD_FLAG("h", "help", struct nest_cli_args, help, "Print help"),
    BU_CMD_FLAG("n", "dry-run", struct nest_cli_args, dry_run, "Report without writing"),
    BU_CMD_FLAG("C", "conservative", struct nest_cli_args, conservative,
        "Use full bounds for collision"),
    BU_CMD_INTEGER_RANGE("d", "dimension", struct nest_cli_args, dimension,
        2, 3, "2|3", "Planar or spatial nesting"),
    BU_CMD_KEYWORD_VALUES("a", "axis", struct nest_cli_args, axis, "x|y|z",
        "Projection and upright axis", nest_axes),
    BU_CMD_POSITIVE_NUMBER("s", "cell-size", struct nest_cli_args, cell_size,
        "size", "Raster cell size in local units"),
    BU_CMD_NONNEGATIVE_NUMBER("c", "clearance", struct nest_cli_args, clearance,
        "distance", "Minimum raster clearance"),
    BU_CMD_KEYWORD_VALUES("o", "orientation", struct nest_cli_args, orientation,
        "mode", "Allowed orientation set", nest_orientations),
    BU_CMD_KEYWORD_VALUES("q", "quality", struct nest_cli_args, quality,
        "level", "Search work budget", nest_qualities),
    BU_CMD_VECTOR3("g", "gravity", struct nest_cli_args, gravity,
        "x,y,z", "Gravity direction"),
    BU_CMD_CUSTOM("", "value", struct nest_cli_args, values, parse_value_option,
        "name=value", "Geometric-knapsack value override"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand nest_operands[] = {
    BU_CMD_OPERAND("output", BU_CMD_VALUE_STRING, 1, 1,
        "New combination name", NULL),
    BU_CMD_OPERAND("container", BU_CMD_VALUE_DB_OBJECT, 1, 1,
        "Permitted volume", "ged.db_object"),
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
        "Objects to pack", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};

bool
parse_options(struct ged *gedp, int argc, const char *argv[], NestOptions &options,
        bool &help)
{
    help = false;
    /* Help is available even when the required geometry names are absent. */
    for (int i = 1; i < argc; ++i) {
        if (BU_STR_EQUAL(argv[i], "--help") || BU_STR_EQUAL(argv[i], "-h")) {
            help = true;
            return true;
        }
        if (argv[i][0] != '-')
            break;
    }

    nest_cli_args args;
    int operand_index = bu_cmd_schema_parse_complete(&ged_arrange_nest_schema,
        &args, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
        return false;
    int first_operand = operand_index + 1;
    options.dimension = args.dimension == 2 ? Dimension::PLANAR : Dimension::SPATIAL;
    options.axis = BU_STR_EQUAL(args.axis, "x") ? X : BU_STR_EQUAL(args.axis, "y") ? Y : Z;
    options.cell_size = args.cell_size;
    options.clearance = args.clearance;
    options.dry_run = args.dry_run != 0;
    options.conservative = args.conservative != 0;
    options.orientation_set = args.orientation != NULL;
    if (args.orientation) {
        options.orientation = BU_STR_EQUAL(args.orientation, "fixed") ? OrientationMode::FIXED :
            BU_STR_EQUAL(args.orientation, "upright") ? OrientationMode::UPRIGHT : OrientationMode::ORTHOGONAL;
    }
    options.quality = BU_STR_EQUAL(args.quality, "fast") ? Quality::FAST :
        BU_STR_EQUAL(args.quality, "thorough") ? Quality::THOROUGH : Quality::BALANCED;
    VMOVE(options.gravity.data(), args.gravity);
    options.gravity_enabled = bu_cmd_schema_option_present(&ged_arrange_nest_schema,
        (size_t)operand_index, argv + 1, "gravity") != 0;
    if (options.gravity_enabled && ZERO(MAGNITUDE(args.gravity))) {
        bu_vls_printf(gedp->ged_result_str, "arrange nest: gravity must be nonzero\n");
        return false;
    }
    options.values = std::move(args.values);
    options.output = argv[first_operand++];
    options.container = argv[first_operand++];
    while (first_operand < argc)
        options.objects.emplace_back(argv[first_operand++]);
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

const struct bu_cmd_schema ged_arrange_nest_schema =
    BU_CMD_SCHEMA_BOUND("nest", "Pack objects into a permitted volume",
        nest_options, nest_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
        NULL, NULL, NULL, NULL);

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

    if (ZERO(options.cell_size)) {
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
