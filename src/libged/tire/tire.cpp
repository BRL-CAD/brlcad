/*                          T I R E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/tire.cpp
 *
 * Tire Generator
 *
 * Program to create basic tire shapes.
 */

#include "common.h"

#include <array>
#include <cmath>
#include <math.h>
#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <utility>
#include <vector>

#include "bu/avs.h"
#include "bu/cmdschema.h"
#include "bu/units.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"

#include "tire_private.h"

namespace {

using tire_private::TireSpec;
using tire_private::TreadSpec;
using tire_private::TreadPatternCountRange;
using tire_private::TreadPatternDefinition;
using tire_private::WheelSpec;
using tire_private::CsgTransaction;
using tire_private::find_predefined_tread_pattern;
using tire_private::generate_tread_sketches;
using tire_private::list_tread_patterns;
using tire_private::load_tread_pattern_file;
using tire_private::make_air_region;
using tire_private::make_tire;
using tire_private::make_wheel_rims;
using tire_private::make_wheel_and_tire;
using tire_private::add_member;
using tire_private::tread_pattern_json;
using tire_private::tread_pattern_count_range;


static int
tire_iso_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    int d1, d2, d3;
    char s1, s2;
    int sret = 0;
    int consumed = 0;
    auto *isoarray = static_cast<std::array<fastf_t, 3> *>(storage);

    if (!arg || arg[0] == '\0') {
	if (msg) bu_vls_printf(msg, "missing ISO tire dimensions\n");
	return -1;
	}

    sret = bu_sscanf(arg, "%d%c%d%c%d%n", &d1, &s1, &d2, &s2, &d3, &consumed);

    if (sret != 5 || arg[consumed] != '\0' || s1 != '/' || (s2 != 'R' && s2 != 'r') || d1 <= 0 || d2 <= 0 || d3 <= 0) {
	if (msg) bu_vls_printf(msg, "Invalid ISO specification string: %s\n", arg);
	return -1;
    }

    if (isoarray) {
	(*isoarray)[0] = d1;
	(*isoarray)[1] = d2;
	(*isoarray)[2] = d3;
    }
    return 0;
}

#define ISO_TIRE_FMT "<width-mm>/<aspect-percent>R<rim-diameter-in>"

static int
tire_tread_shape_validate(struct bu_vls *msg, const char *arg)
{
    char *end = NULL;
    long value = 0;

    if (!arg)
	return -1;
    value = strtol(arg, &end, 0);
    if (!end || *end || value < 0 || value > 2) {
	if (msg)
	    bu_vls_printf(msg, "tread shape profile must be in range 0-2.\n");
	return -1;
    }
    return 0;
}

/* One command-local value object keeps the parser binding reentrant. */
struct TireArgs {
    std::array<fastf_t, 3> isoarray = {{215.0, 55.0, 17.0}};
    fastf_t width = 0.0;
    fastf_t aspect = 0.0;
    fastf_t rim_diameter = 0.0;
    const char *name = NULL;
    int tread_shape = 0;
    int wheel = 1;
    int tread_pattern_count = 0;
    fastf_t tread_depth = 11.0;
    fastf_t tire_thickness = 0.0;
    fastf_t hub_width = 0.0;
    const char *tread_pattern = NULL;
    const char *tread_pattern_file = NULL;
    const char *demo_file = NULL;
    fastf_t max_sidewall_radius = 0.0;
    int help = 0;
    int list_patterns = 0;
};

/* This list is the command's sole authored option schema. */
#define TIRE_OPTION_ROWS(X, A) \
    X(FLAG,          "h", "help",                help,                "",           "Print help and exit") \
    X(STRING,        "n", "obj-name",            name,                "name",       "Set top-level object name") \
    X(BOOL,          "w", "wheel",               wheel,               "bool",       "Enable/disable wheel (default is enabled).") \
    X(CUSTOM,        "d", "dimensions",          isoarray,            ISO_TIRE_FMT,  "Specify tire dimensions using ISO style inputs") \
    X(NUMBER,        "W", "width",               width,               "mm",         "Tire width. Overrides --dimensions.") \
    X(NUMBER,        "R", "aspect-ratio",        aspect,              "percent",    "Aspect ratio. Overrides --dimensions.") \
    X(NUMBER,        "D", "rim-diameter",        rim_diameter,        "in",         "Rim diameter. Overrides --dimensions.") \
    X(NUMBER,        "g", "tread-depth",         tread_depth,         "1/32 in",    "Tread depth") \
    X(NUMBER,        "j", "hub-width",           hub_width,           "in",         "Rim width") \
    X(NUMBER,        "s", "max-sidewall-radius", max_sidewall_radius, "mm",         "Maximum sidewall radius") \
    X(NUMBER,        "u", "tire-thickness",      tire_thickness,      "mm",         "Tire thickness") \
    X(STRING,        "p", "tread-pattern",       tread_pattern,       "name",       "Tread pattern preset name. Legacy aliases 1 and 2 are accepted.") \
    X(INTEGER,       "c", "tread-pattern-cnt",   tread_pattern_count, "#",          "Number of tread patterns around tire") \
    X(INTEGER_RANGE, "t", "tread-shape",         tread_shape,         "0|1|2",      "Tread shape profile") \
    X(FILE,          "",  "tread-pattern-file",  tread_pattern_file,  "file.json",  "Read a custom JSON tread pattern definition") \
    X(FLAG,          "",  "list-tread-patterns",  list_patterns,       "",           "List available tread pattern presets and exit") \
    X(FILE,          "",  "demo-file",           demo_file,           "file.g",     "Generate a grid of varied sample tires") \
    A(SHORT, "?", "help") \
    A(LONG,  "ISO", "dimensions")

#define TIRE_NATIVE_FLAG(s, l, f, a, h) BU_CMD_FLAG(s, l, TireArgs, f, h),
#define TIRE_NATIVE_BOOL(s, l, f, a, h) BU_CMD_BOOL(s, l, TireArgs, f, a, h),
#define TIRE_NATIVE_INTEGER(s, l, f, a, h) BU_CMD_INTEGER(s, l, TireArgs, f, a, h),
#define TIRE_NATIVE_INTEGER_RANGE(s, l, f, a, h) BU_CMD_INTEGER_VALIDATE(s, l, TireArgs, f, tire_tread_shape_validate, a, h),
#define TIRE_NATIVE_NUMBER(s, l, f, a, h) BU_CMD_NUMBER(s, l, TireArgs, f, a, h),
#define TIRE_NATIVE_STRING(s, l, f, a, h) BU_CMD_STRING(s, l, TireArgs, f, a, h),
#define TIRE_NATIVE_FILE(s, l, f, a, h) BU_CMD_FILE(s, l, TireArgs, f, a, h),
#define TIRE_NATIVE_CUSTOM(s, l, f, a, h) BU_CMD_CUSTOM(s, l, TireArgs, f, tire_iso_parse, a, h),
#define TIRE_NATIVE_OPTION(kind, s, l, f, a, h) TIRE_NATIVE_##kind(s, l, f, a, h)
#define TIRE_NATIVE_ALIAS_SHORT(spelling, canonical) BU_CMD_ALIAS_SHORT(spelling, canonical, 1),
#define TIRE_NATIVE_ALIAS_LONG(spelling, canonical) BU_CMD_ALIAS_LONG(spelling, canonical, 1),
#define TIRE_NATIVE_ALIAS(kind, spelling, canonical) TIRE_NATIVE_ALIAS_##kind(spelling, canonical)
static const struct bu_cmd_option tire_options[] = {
    TIRE_OPTION_ROWS(TIRE_NATIVE_OPTION, TIRE_NATIVE_ALIAS)
    BU_CMD_OPTION_NULL
};
#undef TIRE_NATIVE_FLAG
#undef TIRE_NATIVE_BOOL
#undef TIRE_NATIVE_INTEGER
#undef TIRE_NATIVE_INTEGER_RANGE
#undef TIRE_NATIVE_NUMBER
#undef TIRE_NATIVE_STRING
#undef TIRE_NATIVE_FILE
#undef TIRE_NATIVE_CUSTOM
#undef TIRE_NATIVE_OPTION
#undef TIRE_NATIVE_ALIAS_SHORT
#undef TIRE_NATIVE_ALIAS_LONG
#undef TIRE_NATIVE_ALIAS

static const struct bu_cmd_operand tire_operands[] = {
    BU_CMD_OPERAND("obj-name", BU_CMD_VALUE_STRING, 0, 1, "Optional top-level object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema tire_cmd_schema_native = {
    "tire", "Generate tire and wheel geometry", tire_options, tire_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    {NULL}
};

#undef TIRE_OPTION_ROWS

/* Help message printed when -h option is supplied */
static void
_tire_show_help(struct ged *gedp, const char *cmd, const struct bu_cmd_schema *schema)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help = bu_cmd_schema_describe(schema);
    bu_vls_sprintf(&str, "Usage: %s [options] [obj_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(&str, "\nStandard ISO formatting for tire dimensions is of the form %s, where <width> is in mm, <aspect> is a ratio, and <rim diameter> is in inches.\n",
    ISO_TIRE_FMT);
    bu_vls_printf(&str, "\nUse --list-tread-patterns to show named tread presets.  --tread-pattern accepts a preset name; legacy values 1 and 2 map to legacy-rib and legacy-block.\n");
    bu_vls_printf(&str, "\nUse --demo-file file.g to generate a grid of varied tire examples.  When run from MGED, the sampler is written to the current database.\n");

    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&str));
    bu_vls_free(&str);
    return;
}

static int
_tire_validate_positive(struct ged *gedp, const char *name, fastf_t value)
{
    if (!std::isfinite(value) || value <= 0) {
	bu_vls_printf(gedp->ged_result_str, "%s must be greater than zero.\n", name);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static int
_tire_validate_nonnegative(struct ged *gedp, const char *name, fastf_t value)
{
    if (!std::isfinite(value) || value < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s must be non-negative.\n", name);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

struct TireDimensions {
    fastf_t width = 0.0;
    fastf_t aspect = 0.0;
    fastf_t rim_diameter_in = 0.0;
    fastf_t dytred = 0.0;
    fastf_t dztred = 0.0;
    fastf_t d1 = 0.0;
    fastf_t dyside1 = 0.0;
    fastf_t zside1 = 0.0;
    fastf_t ztire = 0.0;
    fastf_t ztire_with_offset = 0.0;
    fastf_t dyhub = 0.0;
    fastf_t zhub = 0.0;
    fastf_t thickness = 0.0;

    TireSpec tire_spec() const
    {
	return {dytred, dztred, d1, dyside1, zside1, ztire, dyhub, zhub, thickness};
    }
};

struct ValidatedTireRequest {
    std::array<fastf_t, 3> iso = {{0.0, 0.0, 0.0}};
    std::string top_name;
    std::string suffix;
    TireDimensions dim;
    TreadSpec tread_spec;
    int usewheel = 1;
};

static TireDimensions
derive_dimensions(const std::array<fastf_t, 3> &iso,
		  fastf_t hub_width_in,
		  fastf_t sidewall_radius,
		  fastf_t tire_thickness,
		  fastf_t tread_depth_mm,
		  int tread_shape)
{
    TireDimensions dim;
    dim.width = iso[0];
    dim.aspect = iso[1];
    dim.rim_diameter_in = iso[2];

    const fastf_t wheel_diam = dim.rim_diameter_in * bu_units_conversion("in");
    dim.dyside1 = dim.width;
    dim.ztire = ((dim.width * dim.aspect / 100) * 2 + wheel_diam) / 2;
    dim.zhub = dim.ztire - dim.width * dim.aspect / 100;
    dim.dytred = 0.8 * dim.width;
    dim.d1 = (dim.ztire - dim.zhub) / 2.5;
    dim.dyhub = ZERO(hub_width_in) ? dim.dytred : hub_width_in * bu_units_conversion("in");
    dim.zside1 = ZERO(sidewall_radius) ? 0.6 * dim.zhub + 0.4 * dim.ztire : sidewall_radius;
    dim.dztred = 0.001 * dim.aspect * dim.zside1;
    dim.thickness = ZERO(tire_thickness) ? dim.dztred : tire_thickness;
    dim.ztire_with_offset = dim.ztire - ((tread_shape != 0) ? tread_depth_mm : 0.0);

    return dim;
}

static std::optional<ValidatedTireRequest>
build_validated_request(struct ged *gedp,
			std::array<fastf_t, 3> isoarray,
			fastf_t width,
			fastf_t aspect,
			fastf_t rim_diam,
			const char *name_arg,
			int usewheel,
			int tread_type,
			int num_tread_ptns,
			fastf_t tread_depth,
			fastf_t tire_thickness,
			fastf_t hub_width,
			const char *pattern_name,
			const char *pattern_file,
			fastf_t zside1,
			int ret_ac,
			const char *argv[])
{
    if (_tire_validate_nonnegative(gedp, "width override", width) != BRLCAD_OK ||
	_tire_validate_nonnegative(gedp, "aspect ratio override", aspect) != BRLCAD_OK ||
	_tire_validate_nonnegative(gedp, "rim diameter override", rim_diam) != BRLCAD_OK ||
	_tire_validate_nonnegative(gedp, "tread depth", tread_depth) != BRLCAD_OK ||
	_tire_validate_nonnegative(gedp, "hub width", hub_width) != BRLCAD_OK ||
	_tire_validate_nonnegative(gedp, "maximum sidewall radius", zside1) != BRLCAD_OK ||
	_tire_validate_nonnegative(gedp, "tire thickness", tire_thickness) != BRLCAD_OK) {
	return std::nullopt;
    }

    if (tread_type < 0 || tread_type > 2) {
	bu_vls_printf(gedp->ged_result_str, "tread shape profile must be in range 0-2.\n");
	return std::nullopt;
    }

    if (pattern_name && pattern_file) {
	bu_vls_printf(gedp->ged_result_str, "--tread-pattern and --tread-pattern-file are mutually exclusive.\n");
	return std::nullopt;
    }

    if (num_tread_ptns < 0) {
	bu_vls_printf(gedp->ged_result_str, "number of tread patterns must be non-negative.\n");
	return std::nullopt;
    }
    if (num_tread_ptns > tire_private::MAX_TREAD_PATTERN_COUNT) {
	bu_vls_printf(gedp->ged_result_str, "number of tread patterns must be no greater than %d.\n",
		      tire_private::MAX_TREAD_PATTERN_COUNT);
	return std::nullopt;
    }

    if (width > 0) isoarray[0] = width;
    if (aspect > 0) isoarray[1] = aspect;
    if (rim_diam > 0) isoarray[2] = rim_diam;

    if (_tire_validate_positive(gedp, "tire width", isoarray[0]) != BRLCAD_OK ||
	_tire_validate_positive(gedp, "aspect ratio", isoarray[1]) != BRLCAD_OK ||
	_tire_validate_positive(gedp, "rim diameter", isoarray[2]) != BRLCAD_OK) {
	return std::nullopt;
    }

    std::string top_name;
    if (name_arg && name_arg[0] != '\0') {
	top_name = name_arg;
    } else if (ret_ac > 0) {
	top_name = argv[0];
	ret_ac--;
    } else {
	top_name = "tire-" + std::to_string(static_cast<int>(isoarray[0])) + "-" +
	    std::to_string(static_cast<int>(isoarray[1])) + "R" +
	    std::to_string(static_cast<int>(isoarray[2]));
    }

    if (ret_ac > 0) {
	bu_vls_sprintf(gedp->ged_result_str, "unknown args supplied.\n");
	return std::nullopt;
    }

    if (db_lookup(gedp->dbip, top_name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "%s already exists.\n", top_name.c_str());
	return std::nullopt;
    }

    TreadPatternDefinition tread_def;
    bool tread_enabled = false;
    if (pattern_file) {
	if (!load_tread_pattern_file(pattern_file, tread_def, gedp->ged_result_str))
	    return std::nullopt;
	tread_enabled = true;
    } else if (pattern_name) {
	const TreadPatternDefinition *preset = find_predefined_tread_pattern(pattern_name);
	if (!preset) {
	    bu_vls_printf(gedp->ged_result_str, "unknown tread pattern '%s'. Use --list-tread-patterns to see available presets.\n", pattern_name);
	    return std::nullopt;
	}
	tread_def = *preset;
	tread_enabled = true;
    } else if (tread_type == 1) {
	tread_def = *find_predefined_tread_pattern("legacy-rib");
	tread_enabled = true;
    } else if (tread_type == 2) {
	tread_def = *find_predefined_tread_pattern("legacy-block");
	tread_enabled = true;
    }

    if (tread_enabled) {
	if (tread_type == 0)
	    tread_type = tread_def.shape;
	if (num_tread_ptns == 0)
	    num_tread_ptns = tread_def.default_count;
	const TreadPatternCountRange count_range = tread_pattern_count_range(tread_def);
	if (num_tread_ptns < count_range.min || num_tread_ptns > count_range.max) {
	    bu_vls_printf(gedp->ged_result_str,
			  "tread pattern count for '%s' must be between %d and %d.\n",
			  tread_def.id.c_str(), count_range.min, count_range.max);
	    return std::nullopt;
	}
    }

    if (tread_type != 0 && num_tread_ptns < 3) {
	bu_vls_printf(gedp->ged_result_str, "number of tread patterns must be at least 3 when tread is enabled.\n");
	return std::nullopt;
    }

    const fastf_t tread_depth_mm = tread_depth / 32.0 * bu_units_conversion("in");
    const TireDimensions dim = derive_dimensions(isoarray, hub_width, zside1, tire_thickness,
						 tread_depth_mm, tread_type);

    if (!std::isfinite(dim.ztire) || !std::isfinite(dim.zhub) || !std::isfinite(dim.dytred) ||
	!std::isfinite(dim.dyhub) || !std::isfinite(dim.zside1) || !std::isfinite(dim.dztred) ||
	!std::isfinite(dim.thickness) || !std::isfinite(dim.ztire_with_offset)) {
	bu_vls_printf(gedp->ged_result_str, "tire parameters produced non-finite geometry.\n");
	return std::nullopt;
    }

    if (dim.zside1 <= dim.zhub || dim.zside1 >= dim.ztire) {
	bu_vls_printf(gedp->ged_result_str, "maximum sidewall radius must be greater than the rim radius and less than the tire radius.\n");
	return std::nullopt;
    }

    if (tread_type != 0 && dim.ztire_with_offset <= dim.zhub) {
	bu_vls_printf(gedp->ged_result_str, "tread depth is too large for the specified tire dimensions.\n");
	return std::nullopt;
    }

    if (dim.thickness <= 0 || dim.thickness >= dim.dyside1 / 2 || dim.thickness >= dim.dyhub / 2 ||
	dim.thickness >= dim.ztire_with_offset - dim.zhub) {
	bu_vls_printf(gedp->ged_result_str, "tire thickness is too large for the specified tire dimensions.\n");
	return std::nullopt;
    }

    TreadSpec tread_spec;
    tread_spec.shape = tread_type;
    tread_spec.count = num_tread_ptns;
    tread_spec.depth_mm = tread_depth_mm;
    if (tread_enabled) {
	tread_def.shape = tread_type;
	tread_def.default_count = num_tread_ptns;
	tread_spec.pattern_id = tread_def.id;
	tread_spec.pattern_json = tread_pattern_json(tread_def);
	tread_spec.sketches = generate_tread_sketches(tread_def);
    }
    if (pattern_file) {
	tread_spec.pattern_source = "file";
	tread_spec.pattern_file = pattern_file;
    } else if (pattern_name) {
	tread_spec.pattern_source = "preset";
    } else if (tread_type == 1 || tread_type == 2) {
	tread_spec.pattern_source = "legacy";
    } else {
	tread_spec.pattern_source = "none";
    }

    ValidatedTireRequest request;
    request.iso = isoarray;
    request.top_name = std::move(top_name);
    request.suffix = request.top_name;
    request.dim = dim;
    request.tread_spec = std::move(tread_spec);
    request.usewheel = usewheel;
    return request;
}

class TireBuilder {
public:
    TireBuilder(rt_wdb *wdbp, std::string top_name, std::string suffix,
		std::array<fastf_t, 3> iso, const TireDimensions &dimensions,
		TreadSpec tread_spec, int usewheel) :
	wdbp_(wdbp),
	top_name_(std::move(top_name)),
	suffix_(std::move(suffix)),
	iso_(iso),
	dim_(dimensions),
	tread_spec_(std::move(tread_spec)),
	usewheel_(usewheel)
    {
    }

    int build(struct bu_vls *msg)
    {
	CsgTransaction preflight_txn(wdbp_, true);
	if (build_into(preflight_txn) != BRLCAD_OK)
	    return fail(preflight_txn, msg, "generated object preflight failed");

	CsgTransaction csg_txn(wdbp_);
	if (build_into(csg_txn) != BRLCAD_OK)
	    return fail(csg_txn, msg, "failed to generate tire geometry");

	csg_txn.commit();
	return BRLCAD_OK;
    }

    int build_into(CsgTransaction &csg_txn)
    {
	if (generate(csg_txn) != BRLCAD_OK)
	    return BRLCAD_ERROR;

	return write_top_attributes(csg_txn);
    }

private:
    int generate(CsgTransaction &csg_txn)
    {
	const TireSpec tire_spec = dim_.tire_spec();

	if (make_tire(csg_txn, suffix_, tire_spec, tread_spec_) != BRLCAD_OK)
	    return BRLCAD_ERROR;

	WheelSpec wheel_spec;
	wheel_spec.rim_thickness = dim_.thickness / 2.0;

	if (usewheel_ != 0) {
	    if (make_wheel_rims(csg_txn, suffix_,
			    dim_.dyhub, dim_.zhub, wheel_spec.bolts, wheel_spec.bolt_diam,
			    wheel_spec.bolt_circ_diam, wheel_spec.spigot_diam,
			    wheel_spec.fixing_offset, wheel_spec.bead_height,
			    wheel_spec.bead_width, wheel_spec.rim_thickness) != BRLCAD_OK)
		return BRLCAD_ERROR;
	}

	if (make_air_region(csg_txn, suffix_, dim_.dyhub, dim_.zhub, usewheel_) != BRLCAD_OK)
	    return BRLCAD_ERROR;

	if (make_wheel_and_tire(csg_txn, top_name_, suffix_, usewheel_) != BRLCAD_OK)
	    return BRLCAD_ERROR;

	return BRLCAD_OK;
    }

    int fail(CsgTransaction &csg_txn, struct bu_vls *msg, const char *message)
    {
	if (msg) {
	    if (csg_txn.error().empty())
		bu_vls_printf(msg, "%s.\n", message);
	    else
		bu_vls_printf(msg, "%s: %s.\n", message, csg_txn.error().c_str());
	}
	(void)csg_txn.rollback(msg);
	return BRLCAD_ERROR;
    }

    static std::string attr_fastf(fastf_t value)
    {
	char buf[64] = {0};
	snprintf(buf, sizeof(buf), "%.17g", value);
	return buf;
    }

    static std::string attr_int(int value)
    {
	return std::to_string(value);
    }

    static void add_attr(struct bu_attribute_value_set *avs, const char *name, const std::string &value)
    {
	(void)bu_avs_add(avs, name, value.c_str());
    }

    int write_top_attributes(CsgTransaction &csg_txn)
    {
	struct bu_attribute_value_set avs;
	bu_avs_init_empty(&avs);

	add_attr(&avs, "tire::generator", "ged tire");
	add_attr(&avs, "tire::schema_version", "1");
	add_attr(&avs, "tire::name", top_name_);
	add_attr(&avs, "tire::iso", attr_int(static_cast<int>(iso_[0])) + "/" +
		 attr_int(static_cast<int>(iso_[1])) + "R" + attr_int(static_cast<int>(iso_[2])));
	add_attr(&avs, "tire::width_mm", attr_fastf(iso_[0]));
	add_attr(&avs, "tire::aspect_ratio", attr_fastf(iso_[1]));
	add_attr(&avs, "tire::rim_diameter_in", attr_fastf(iso_[2]));
	add_attr(&avs, "tire::wheel_enabled", usewheel_ ? "1" : "0");
	add_attr(&avs, "tire::tread_enabled", tread_spec_.shape ? "1" : "0");
	add_attr(&avs, "tire::tread_shape", attr_int(tread_spec_.shape));
	add_attr(&avs, "tire::tread_pattern_count", attr_int(tread_spec_.count));
	add_attr(&avs, "tire::tread_depth_mm", attr_fastf(tread_spec_.depth_mm));
	add_attr(&avs, "tire::tread_pattern_id", tread_spec_.pattern_id.empty() ? "none" : tread_spec_.pattern_id);
	add_attr(&avs, "tire::tread_pattern_source", tread_spec_.pattern_source.empty() ? "none" : tread_spec_.pattern_source);
	if (!tread_spec_.pattern_file.empty())
	    add_attr(&avs, "tire::tread_pattern_file", tread_spec_.pattern_file);
	if (!tread_spec_.pattern_json.empty())
	    add_attr(&avs, "tire::tread_pattern_json", tread_spec_.pattern_json);
	add_attr(&avs, "tire::tread_sketch_count", attr_int(static_cast<int>(tread_spec_.sketches.size())));
	add_attr(&avs, "tire::outer_radius_mm", attr_fastf(dim_.ztire));
	add_attr(&avs, "tire::outer_radius_with_tread_offset_mm", attr_fastf(dim_.ztire_with_offset));
	add_attr(&avs, "tire::rim_radius_mm", attr_fastf(dim_.zhub));
	add_attr(&avs, "tire::tread_width_mm", attr_fastf(dim_.dytred));
	add_attr(&avs, "tire::hub_width_mm", attr_fastf(dim_.dyhub));
	add_attr(&avs, "tire::sidewall_width_mm", attr_fastf(dim_.dyside1));
	add_attr(&avs, "tire::sidewall_max_radius_mm", attr_fastf(dim_.zside1));
	add_attr(&avs, "tire::thickness_mm", attr_fastf(dim_.thickness));

	return csg_txn.update_attributes(top_name_, &avs);
    }

    rt_wdb *wdbp_ = nullptr;
    std::string top_name_;
    std::string suffix_;
    std::array<fastf_t, 3> iso_ = {{0.0, 0.0, 0.0}};
    TireDimensions dim_;
    TreadSpec tread_spec_;
    int usewheel_ = 0;
};

struct DemoTire {
    const char *name = nullptr;
    const char *pattern = nullptr;
    fastf_t width = 0.0;
    fastf_t aspect = 0.0;
    fastf_t rim_diameter = 0.0;
    fastf_t tread_depth = 0.0;
    fastf_t hub_width = 0.0;
    int tread_count = 0;
    fastf_t grid_x = 0.0;
    fastf_t grid_y = 0.0;
};

static void
add_attr(struct bu_attribute_value_set *avs, const char *name, const std::string &value)
{
    (void)bu_avs_add(avs, name, value.c_str());
}

static const std::vector<DemoTire> &
demo_tires()
{
    static const std::vector<DemoTire> tires = {
	{"compact_car", "highway-rib", 175.0, 65.0, 14.0, 9.0, 5.0, 28, -2600.0, -3200.0},
	{"family_sedan", "highway-rib", 215.0, 55.0, 17.0, 10.0, 7.0, 32, -1100.0, -3200.0},
	{"performance_coupe", "highway-rib", 275.0, 35.0, 20.0, 8.0, 9.0, 32, 500.0, -3200.0},
	{"winter_suv", "winter-siped", 235.0, 60.0, 18.0, 12.0, 8.0, 28, 2200.0, -3200.0},
	{"delivery_van", "commercial-rib", 225.0, 75.0, 16.0, 14.0, 7.0, 32, -3100.0, 0.0},
	{"pickup_truck", "all-terrain", 265.0, 70.0, 17.0, 16.0, 8.0, 14, -1200.0, 0.0},
	{"semi_tractor", "commercial-rib", 315.0, 80.0, 22.0, 20.0, 9.0, 32, 900.0, 0.0},
	{"city_bus", "commercial-rib", 275.0, 70.0, 22.0, 18.0, 8.5, 32, 3100.0, 0.0},
	{"farm_tractor", "mud-terrain", 650.0, 65.0, 38.0, 58.0, 22.0, 16, -3100.0, 4200.0},
	{"wheel_loader", "mud-terrain", 875.0, 65.0, 29.0, 70.0, 26.0, 14, -800.0, 4200.0},
	{"mining_haul_truck", "mud-terrain", 1500.0, 80.0, 63.0, 90.0, 44.0, 12, 2800.0, 4200.0}
    };
    return tires;
}

static int
demo_fail(CsgTransaction &csg_txn, struct bu_vls *msg, const char *message)
{
    if (msg) {
	if (csg_txn.error().empty())
	    bu_vls_printf(msg, "%s.\n", message);
	else
	    bu_vls_printf(msg, "%s: %s.\n", message, csg_txn.error().c_str());
    }
    (void)csg_txn.rollback(msg);
    return BRLCAD_ERROR;
}

static int
write_demo_top(CsgTransaction &csg_txn, const std::vector<DemoTire> &samples, const char *demo_file)
{
    struct wmember all;
    BU_LIST_INIT(&all.l);

    for (const DemoTire &sample : samples) {
	mat_t transform;
	MAT_IDN(transform);
	MAT_DELTAS(transform, sample.grid_x, sample.grid_y, 0.0);
	add_member(sample.name, &all.l, transform, WMOP_UNION);
    }

    if (csg_txn.write_lcomb("all", &all, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    add_attr(&avs, "tire::generator", "ged tire");
    add_attr(&avs, "tire::demo", "1");
    add_attr(&avs, "tire::demo_file", demo_file ? demo_file : "");
    add_attr(&avs, "tire::demo_layout", "rows by vehicle class, increasing tire scale left to right");
    add_attr(&avs, "tire::demo_count", std::to_string(samples.size()));
    return csg_txn.update_attributes("all", &avs);
}

static int
generate_demo(struct ged *gedp, const char *demo_file)
{
    const std::vector<DemoTire> &samples = demo_tires();
    std::vector<ValidatedTireRequest> requests;
    requests.reserve(samples.size());

    const char *empty_argv[] = {nullptr};
    for (const DemoTire &sample : samples) {
	std::array<fastf_t, 3> iso = {{sample.width, sample.aspect, sample.rim_diameter}};
	std::optional<ValidatedTireRequest> request = build_validated_request(
	    gedp, iso, 0.0, 0.0, 0.0, sample.name, 1, 0, sample.tread_count,
	    sample.tread_depth, 0.0, sample.hub_width, sample.pattern, NULL, 0.0,
	    0, empty_argv);
	if (!request)
	    return BRLCAD_ERROR;
	requests.push_back(*request);
    }

    rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    CsgTransaction preflight_txn(wdbp, true);
    for (size_t i = 0; i < requests.size(); i++) {
	const ValidatedTireRequest &request = requests[i];
	TireBuilder builder(wdbp, request.top_name, request.suffix, request.iso, request.dim,
			    request.tread_spec, request.usewheel);
	if (builder.build_into(preflight_txn) != BRLCAD_OK)
	    return demo_fail(preflight_txn, gedp->ged_result_str, "generated tire demo preflight failed");
    }
    if (write_demo_top(preflight_txn, samples, demo_file) != BRLCAD_OK)
	return demo_fail(preflight_txn, gedp->ged_result_str, "generated tire demo preflight failed");

    CsgTransaction csg_txn(wdbp);
    for (size_t i = 0; i < requests.size(); i++) {
	const ValidatedTireRequest &request = requests[i];
	TireBuilder builder(wdbp, request.top_name, request.suffix, request.iso, request.dim,
			    request.tread_spec, request.usewheel);
	if (builder.build_into(csg_txn) != BRLCAD_OK)
	    return demo_fail(csg_txn, gedp->ged_result_str, "failed to generate tire demo");
    }
    if (write_demo_top(csg_txn, samples, demo_file) != BRLCAD_OK)
	return demo_fail(csg_txn, gedp->ged_result_str, "failed to generate tire demo");

    csg_txn.commit();
    mk_id(wdbp, "Tire demo");

    bu_vls_printf(gedp->ged_result_str, "generated tire demo sampler with %zu tires", samples.size());
    if (demo_file && demo_file[0] != '\0')
	bu_vls_printf(gedp->ged_result_str, " for %s", demo_file);
    bu_vls_printf(gedp->ged_result_str, ".\n");

    return BRLCAD_OK;
}

} /* namespace */

int
ged_tire_core(struct ged *gedp, int argc, const char *argv[])
{
    TireArgs options;
    struct bu_vls parse_msg = BU_VLS_INIT_ZERO;
    int operand_index = 0;
    int ret_ac = 0;
    const char *cmd_name = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* Skip first arg */
    argv++; argc--;

    operand_index = bu_cmd_schema_parse(&tire_cmd_schema_native, &options, &parse_msg, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&parse_msg));
	bu_vls_free(&parse_msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msg);
    ret_ac = argc - operand_index;
    argv += operand_index;
    if (options.help) {
	_tire_show_help(gedp, cmd_name, &tire_cmd_schema_native);
	return BRLCAD_ERROR;
    }
    if (options.list_patterns) {
	list_tread_patterns(gedp->ged_result_str);
	return BRLCAD_OK;
    }
    if (options.demo_file) {
	if (ret_ac > 0) {
	    bu_vls_sprintf(gedp->ged_result_str, "unknown args supplied.\n");
	    return BRLCAD_ERROR;
	}
	return generate_demo(gedp, options.demo_file);
    }

    std::optional<ValidatedTireRequest> request = build_validated_request(
	gedp, options.isoarray, options.width, options.aspect, options.rim_diameter,
	options.name, options.wheel, options.tread_shape, options.tread_pattern_count,
	options.tread_depth, options.tire_thickness, options.hub_width,
	options.tread_pattern, options.tread_pattern_file, options.max_sidewall_radius,
	ret_ac, argv);
    if (!request)
	return BRLCAD_ERROR;

    rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    bu_vls_printf(gedp->ged_result_str, "width = %f\n", request->iso[0]);
    bu_vls_printf(gedp->ged_result_str, "ratio = %f\n", request->iso[1]);
    bu_vls_printf(gedp->ged_result_str, "radius = %f\n", request->iso[2]);
    bu_vls_printf(gedp->ged_result_str, "radius of sidewall max: %f\n", request->dim.zside1);

    TireBuilder builder(wdbp, request->top_name, request->suffix, request->iso, request->dim,
			std::move(request->tread_spec), request->usewheel);
    if (builder.build(gedp->ged_result_str) != BRLCAD_OK)
	return BRLCAD_ERROR;

    mk_id(wdbp, "Tire");

    return BRLCAD_OK;
}


#include "../include/plugin.h"
#define GED_TIRE_COMMANDS(X, XID) \
    X(tire, ged_tire_core, GED_CMD_DEFAULT, &tire_cmd_schema_native) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_TIRE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_tire", 1, GED_TIRE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
