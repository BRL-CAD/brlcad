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

#include "bu/avs.h"
#include "bu/opt.h"
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
using tire_private::tread_pattern_json;


static int
_opt_tire_iso(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    int d1, d2, d3;
    char s1, s2;
    int sret = 0;
    int consumed = 0;
    auto *isoarray = static_cast<fastf_t *>(set_var);

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "ISO tire dimensions");

    sret = bu_sscanf(argv[0], "%d%c%d%c%d%n", &d1, &s1, &d2, &s2, &d3, &consumed);

    if (sret != 5 || argv[0][consumed] != '\0' || s1 != '/' || (s2 != 'R' && s2 != 'r') || d1 <= 0 || d2 <= 0 || d3 <= 0) {
	if (msg) bu_vls_printf(msg, "Invalid ISO specification string: %s\n", argv[0]);
	return -1;
    }

    if (isoarray) {
	isoarray[0] = d1;
	isoarray[1] = d2;
	isoarray[2] = d3;
    }
    return 1;
}

#define ISO_TIRE_FMT "<width-mm>/<aspect-percent>R<rim-diameter-in>"

/* Help message printed when -h option is supplied */
static void
_tire_show_help(struct ged *gedp, const char *cmd, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(&str, "Usage: %s [options] [obj_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(&str, "\nStandard ISO formatting for tire dimensions is of the form %s, where <width> is in mm, <aspect> is a ratio, and <rim diameter> is in inches.\n",
    ISO_TIRE_FMT);
    bu_vls_printf(&str, "\nUse --list-tread-patterns to show named tread presets.  --tread-pattern accepts a preset name; legacy values 1 and 2 map to legacy-rib and legacy-block.\n");

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
	if (generate(preflight_txn) != BRLCAD_OK)
	    return fail(preflight_txn, msg, "generated object preflight failed");

	CsgTransaction csg_txn(wdbp_);
	if (generate(csg_txn) != BRLCAD_OK)
	    return fail(csg_txn, msg, "failed to generate tire geometry");
	if (write_top_attributes(csg_txn) != BRLCAD_OK)
	    return fail(csg_txn, msg, "failed to document generated tire");

	csg_txn.commit();
	return BRLCAD_OK;
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

} /* namespace */

int
ged_tire_core(struct ged *gedp, int argc, const char *argv[])
{
    std::array<fastf_t, 3> isoarray = {{215.0, 55.0, 17.0}};
    fastf_t width = 0.0;
    fastf_t aspect = 0.0;
    fastf_t rim_diam = 0.0;
    const char *name_arg = NULL;
    struct bu_vls parse_msg = BU_VLS_INIT_ZERO;
    int tread_type = 0;
    int usewheel = 1;
    int num_tread_ptns = 0;
    fastf_t tread_depth = 11;
    fastf_t tire_thickness = 0;
    fastf_t hub_width = 0;
    const char *pattern_name = NULL;
    const char *pattern_file = NULL;
    fastf_t zside1 = 0;
    int print_help = 0;
    int list_patterns = 0;
    int ret_ac = 0;
    const char *cmd_name = argv[0];

    struct bu_opt_desc d[19];
    BU_OPT(d[0],  "h", "help",                "",           NULL,             &print_help,     "Print help and exit");
    BU_OPT(d[1],  "?", "",                    "",           NULL,             &print_help,     "");
    BU_OPT(d[2],  "n", "obj-name",            "name",       &bu_opt_str,      &name_arg,       "Set top-level object name");
    BU_OPT(d[3],  "w", "wheel",               "bool",       &bu_opt_bool,     &usewheel,       "Enable/disable wheel (default is enabled).");
    BU_OPT(d[4],  "d", "dimensions",          ISO_TIRE_FMT, &_opt_tire_iso,   isoarray.data(), "Specify tire dimensions using ISO style inputs");
    BU_OPT(d[5],  "",  "ISO",                 ISO_TIRE_FMT, &_opt_tire_iso,   isoarray.data(), "");
    BU_OPT(d[6],  "W", "width",               "#",          &bu_opt_fastf_t,  &width,          "Tire width (mm).  Overrides -d");
    BU_OPT(d[7],  "R", "aspect-ratio",        "#",          &bu_opt_fastf_t,  &aspect,         "Aspect ratio (#/100). Overrides -d.");
    BU_OPT(d[8],  "D", "rim-diameter",        "#",          &bu_opt_fastf_t,  &rim_diam,       "Rim diameter (inches). Overrides -d.");
    BU_OPT(d[9],  "g", "tread-depth",         "#",          &bu_opt_fastf_t,  &tread_depth,    "Tread depth (1/32 inch)");
    BU_OPT(d[10], "j", "hub-width",           "#",          &bu_opt_fastf_t,  &hub_width,      "Rim width (inches)");
    BU_OPT(d[11], "s", "max-sidewall-radius", "#",          &bu_opt_fastf_t,  &zside1,         "Maximum sidewall radius (mm)");
    BU_OPT(d[12], "u", "tire-thickness",      "#",          &bu_opt_fastf_t,  &tire_thickness, "Tire thickness (mm)");
    BU_OPT(d[13], "p", "tread-pattern",       "name",       &bu_opt_str,      &pattern_name,   "Tread pattern preset name. Legacy aliases 1 and 2 are accepted.");
    BU_OPT(d[14], "c", "tread-pattern-cnt",   "#",          &bu_opt_int,      &num_tread_ptns, "Number of tread patterns around tire");
    BU_OPT(d[15], "t", "tread-shape",         "#",          &bu_opt_int,      &tread_type,     "Tread shape profile (integer id, range 1 - 2)");
    BU_OPT(d[16], "",  "tread-pattern-file",  "file",       &bu_opt_str,      &pattern_file,   "Read a custom JSON tread pattern definition");
    BU_OPT(d[17], "",  "list-tread-patterns", "",           NULL,             &list_patterns,  "List available tread pattern presets and exit");
    BU_OPT_NULL(d[18]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* Skip first arg */
    argv++; argc--;

    ret_ac = bu_opt_parse(&parse_msg, argc, argv, d);
    if (ret_ac < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&parse_msg));
	bu_vls_free(&parse_msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msg);
    if (print_help) {
	_tire_show_help(gedp, cmd_name, d);
	return BRLCAD_ERROR;
    }
    if (list_patterns) {
	list_tread_patterns(gedp->ged_result_str);
	return BRLCAD_OK;
    }

    std::optional<ValidatedTireRequest> request = build_validated_request(
	gedp, isoarray, width, aspect, rim_diam, name_arg, usewheel,
	tread_type, num_tread_ptns, tread_depth, tire_thickness, hub_width,
	pattern_name, pattern_file, zside1, ret_ac, argv);
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
    X(tire, ged_tire_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_TIRE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_tire", 1, GED_TIRE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
