/*                    T I R E _ T R E A D _ D E F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libged/tire_tread_def.cpp
 *
 * Tread pattern preset, JSON parsing, and sketch-generation support for
 * the tire command.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <exception>
#include <fstream>
#include <unordered_set>

#include "../../libbu/json.hpp"

#include "bu/vls.h"

#include "tire_private.h"

namespace tire_private {

static fastf_t
clamp_fastf(fastf_t value, fastf_t low, fastf_t high)
{
    return std::max(low, std::min(value, high));
}

static bool
str_equal(const std::string &lhs, const char *rhs)
{
    return lhs == rhs;
}

static std::string
lower_copy(const char *str)
{
    std::string ret = str ? str : "";
    std::transform(ret.begin(), ret.end(), ret.begin(),
		   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ret;
}

static TreadSketch
rect_sketch(fastf_t x0, fastf_t y0, fastf_t x1, fastf_t y1)
{
    if (x1 < x0)
	std::swap(x0, x1);
    if (y1 < y0)
	std::swap(y0, y1);
    return {{TreadPoint{x0, y0}, TreadPoint{x1, y0}, TreadPoint{x1, y1}, TreadPoint{x0, y1}}};
}

static TreadSketch
parallelogram_sketch(fastf_t x0, fastf_t y0, fastf_t x1, fastf_t y1, fastf_t shift)
{
    return {{TreadPoint{x0, y0}, TreadPoint{x0 + shift, y0}, TreadPoint{x1 - shift, y1}, TreadPoint{x1, y1}}};
}

static std::vector<TreadSketch>
legacy_pattern1_sketches()
{
    return {
	{{TreadPoint{.9, 0}, TreadPoint{.6, .3}, TreadPoint{.94, .7}, TreadPoint{.45, 1},
	  TreadPoint{.58, 1}, TreadPoint{1.07, .7}, TreadPoint{.73, .3}, TreadPoint{1.03, 0}}},
	{{TreadPoint{.3, 0}, TreadPoint{.0, .3}, TreadPoint{.34, .7}, TreadPoint{-.15, 1},
	  TreadPoint{.05, 1}, TreadPoint{.47, .7}, TreadPoint{.13, .3}, TreadPoint{.43, 0}}},
	{{TreadPoint{-.1, .1}, TreadPoint{-.1, .12}, TreadPoint{1.1, .12}, TreadPoint{1.1, .1}}},
	{{TreadPoint{-.1, .20}, TreadPoint{-.1, .23}, TreadPoint{1.1, .23}, TreadPoint{1.1, .20}}},
	{{TreadPoint{-.1, .37}, TreadPoint{-.1, .4}, TreadPoint{1.1, .4}, TreadPoint{1.1, .37}}},
	{{TreadPoint{-.1, .49}, TreadPoint{-.1, .51}, TreadPoint{1.1, .51}, TreadPoint{1.1, .49}}},
	{{TreadPoint{-.1, .6}, TreadPoint{-.1, .63}, TreadPoint{1.1, .63}, TreadPoint{1.1, .6}}},
	{{TreadPoint{-.1, .77}, TreadPoint{-.1, .80}, TreadPoint{1.1, .80}, TreadPoint{1.1, .77}}},
	{{TreadPoint{-.1, .88}, TreadPoint{-.1, .9}, TreadPoint{1.1, .9}, TreadPoint{1.1, .88}}}
    };
}

static std::vector<TreadSketch>
legacy_pattern2_sketches()
{
    return {
	{{TreadPoint{0, 0}, TreadPoint{0, .1}, TreadPoint{.66, .34}, TreadPoint{.34, .66},
	  TreadPoint{1, .9}, TreadPoint{1, 1}, TreadPoint{1.4, 1}, TreadPoint{1.33, .9},
	  TreadPoint{.66, .66}, TreadPoint{1, .34}, TreadPoint{.33, .1}, TreadPoint{.4, 0}}},
	{{TreadPoint{.2, .13}, TreadPoint{-.6, .2}, TreadPoint{-.5, .27}, TreadPoint{.4, .2}}},
	{{TreadPoint{.5, .45}, TreadPoint{-.5, .4}, TreadPoint{-.6, .5}, TreadPoint{.5, .55}}},
	{{TreadPoint{.6, .73}, TreadPoint{-.3, .8}, TreadPoint{-.1, .87}, TreadPoint{.8, .8}}}
    };
}

struct TreadGeometryModel {
    fastf_t void_ratio = 0.25;
    int grooves = 4;
    fastf_t groove_angle = 0.0;
    fastf_t block_pitch = 0.25;
    fastf_t primary_width = 0.04;
    fastf_t secondary_width = 0.02;
    fastf_t sipe_width = 0.006;
    std::vector<fastf_t> pitch_edges = {0.0, 1.0};
};

static fastf_t
block_pitch_for_size(const std::string &block)
{
    if (str_equal(block, "small"))
	return 0.16;
    if (str_equal(block, "large"))
	return 0.34;
    return 0.24;
}

static std::vector<fastf_t>
make_pitch_edges(const TreadPatternDefinition &def, fastf_t base_pitch)
{
    (void)def;
    (void)base_pitch;

    /* The master pattern is rotated count times around the tire.  Keep one
     * pitch cell in the master so count remains the actual circumferential
     * tread pitch count instead of being multiplied by pitch_seq entries.
     */
    return {0.0, 1.0};
}

static TreadGeometryModel
resolve_tread_geometry(const TreadPatternDefinition &def)
{
    TreadGeometryModel ret;
    ret.void_ratio = def.void_ratio;
    if (str_equal(def.terrain, "MT"))
	ret.void_ratio += 0.08;
    else if (str_equal(def.terrain, "AT"))
	ret.void_ratio += 0.03;
    else
	ret.void_ratio -= 0.03;

    if (str_equal(def.axle, "drive"))
	ret.void_ratio += 0.025;
    else if (str_equal(def.axle, "trailer"))
	ret.void_ratio -= 0.025;

    ret.void_ratio = clamp_fastf(ret.void_ratio, 0.12, 0.62);
    ret.grooves = std::max(1, std::min(def.grooves, 8));
    ret.groove_angle = clamp_fastf(def.groove_angle, 0.0, 60.0);
    ret.block_pitch = block_pitch_for_size(def.block);
    ret.primary_width = clamp_fastf(ret.void_ratio / (ret.grooves + 3.0), 0.022, 0.095);
    ret.secondary_width = clamp_fastf(ret.primary_width * 0.70, 0.020, 0.070);
    ret.sipe_width = str_equal(def.sipes, "dense") ? 0.024 : 0.018;
    ret.pitch_edges = make_pitch_edges(def, ret.block_pitch);
    return ret;
}

static void
add_center_safe_groove(std::vector<TreadSketch> &sketches, fastf_t x0, fastf_t y0, fastf_t x1, fastf_t y1,
		       const std::string &center, fastf_t tie_bar_width)
{
    if (str_equal(center, "continuous") && y0 < 0.55 && y1 > 0.45) {
	if (y0 < 0.45)
	    sketches.push_back(rect_sketch(x0, y0, x1, 0.45));
	if (y1 > 0.55)
	    sketches.push_back(rect_sketch(x0, 0.55, x1, y1));
	return;
    }

    if (tie_bar_width > 0.0 && x0 < 0.5 && x1 > 0.5) {
	sketches.push_back(rect_sketch(x0, y0, 0.5 - tie_bar_width / 2.0, y1));
	sketches.push_back(rect_sketch(0.5 + tie_bar_width / 2.0, y0, x1, y1));
	return;
    }

    sketches.push_back(rect_sketch(x0, y0, x1, y1));
}

static void
add_longitudinal_grooves(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
			 const TreadGeometryModel &resolved)
{
    const fastf_t shoulder_margin = str_equal(def.shoulder, "closed") ? 0.09 : 0.06;
    for (int i = 1; i <= resolved.grooves; i++) {
	const fastf_t y = shoulder_margin + (1.0 - 2.0 * shoulder_margin) *
	    static_cast<fastf_t>(i) / static_cast<fastf_t>(resolved.grooves + 1);
	const fastf_t width = resolved.primary_width * (str_equal(def.center, "ribbed") && NEAR_EQUAL(y, 0.5, 0.08) ? 0.65 : 1.0);
	sketches.push_back(rect_sketch(-0.08, y - width / 2.0, 1.08, y + width / 2.0));
    }
}

static void
add_segmented_longitudinal_grooves(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
				   const TreadGeometryModel &resolved)
{
    const fastf_t shoulder_margin = str_equal(def.shoulder, "closed") ? 0.11 : 0.08;
	const fastf_t segment_gap = str_equal(def.terrain, "AT") ? 0.28 : 0.14;
    for (int i = 1; i <= resolved.grooves; i++) {
	const fastf_t y = shoulder_margin + (1.0 - 2.0 * shoulder_margin) *
	    static_cast<fastf_t>(i) / static_cast<fastf_t>(resolved.grooves + 1);
	const fastf_t width = resolved.primary_width * (str_equal(def.center, "broken") && NEAR_EQUAL(y, 0.5, 0.12) ? 0.70 : 1.0);
	const fastf_t x0 = (i % 2) ? 0.00 : segment_gap;
	const fastf_t x1 = (i % 2) ? 1.0 - segment_gap : 1.0;
	sketches.push_back(rect_sketch(x0, y - width / 2.0, x1, y + width / 2.0));
    }
}

static void
add_lateral_block_grooves(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
			  const TreadGeometryModel &resolved, fastf_t y0, fastf_t y1, fastf_t phase)
{
    const fastf_t tie_bar_width = def.tie_bars ? 0.10 : 0.0;
    const fastf_t skew = clamp_fastf(resolved.groove_angle / 60.0, 0.0, 1.0) * 0.18;
    for (size_t i = 0; i + 1 < resolved.pitch_edges.size(); i++) {
	const fastf_t start = resolved.pitch_edges[i];
	const fastf_t end = resolved.pitch_edges[i + 1];
	const fastf_t span = end - start;
	const fastf_t x = clamp_fastf(start + span * (0.5 + phase), start + span * 0.18, end - span * 0.18);
	const fastf_t width = std::min(resolved.secondary_width * 2.2, span * 0.40);
	if (resolved.groove_angle > 3.0) {
	    sketches.push_back(parallelogram_sketch(x - width / 2.0, y0, x + width / 2.0, y1, skew));
	} else {
	    add_center_safe_groove(sketches, x - width / 2.0, y0, x + width / 2.0, y1, def.center, tie_bar_width);
	}
    }
}

static void
add_directional_lugs(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
		     const TreadGeometryModel &resolved)
{
    const fastf_t angle = clamp_fastf(resolved.groove_angle / 60.0, 0.15, 1.0);
    const fastf_t shoulder_width = str_equal(def.shoulder, "open") ? 0.33 : 0.27;
    const fastf_t center_gap = str_equal(def.center, "continuous") ? 0.10 : 0.035;
    const fastf_t cut_width = clamp_fastf(resolved.primary_width * 1.35, 0.045, 0.12);

    for (size_t i = 0; i + 1 < resolved.pitch_edges.size(); i++) {
	const fastf_t x0 = resolved.pitch_edges[i];
	const fastf_t x1 = resolved.pitch_edges[i + 1];
	const fastf_t span = x1 - x0;
	const fastf_t inset = span * 0.10;
	const fastf_t lead = (i % 2) ? span * 0.08 : 0.0;

	sketches.push_back({{
	    TreadPoint{x0 + inset + lead, 0.0},
	    TreadPoint{x0 + inset + lead + cut_width, 0.0},
	    TreadPoint{x1 - inset, 0.5 - center_gap - angle * 0.08},
	    TreadPoint{x1 - inset - cut_width, 0.5 - center_gap - angle * 0.08}
	}});
	sketches.push_back({{
	    TreadPoint{x0 + inset, 1.0},
	    TreadPoint{x0 + inset + cut_width, 1.0},
	    TreadPoint{x1 - inset - lead, 0.5 + center_gap + angle * 0.08},
	    TreadPoint{x1 - inset - lead - cut_width, 0.5 + center_gap + angle * 0.08}
	}});

	if (str_equal(def.shoulder, "open")) {
	    sketches.push_back(rect_sketch(x0 + span * 0.18, 0.0, x0 + span * 0.58, shoulder_width * 0.45));
	    sketches.push_back(rect_sketch(x0 + span * 0.42, 1.0 - shoulder_width * 0.45, x0 + span * 0.82, 1.0));
	}
    }
}

static void
add_asymmetric_lugs(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
		    const TreadGeometryModel &resolved)
{
    add_lateral_block_grooves(sketches, def, resolved, 0.03, 0.44, -0.12);
    add_lateral_block_grooves(sketches, def, resolved, 0.56, 0.97, 0.15);
    const fastf_t shift = clamp_fastf(resolved.groove_angle / 60.0, 0.0, 1.0) * 0.26;
    sketches.push_back(parallelogram_sketch(0.02, 0.16, 0.98, 0.34, shift));
    sketches.push_back(parallelogram_sketch(0.02, 0.66, 0.98, 0.84, -shift));
}

static void
add_center_features(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
		    const TreadGeometryModel &resolved)
{
    if (str_equal(def.center, "continuous"))
	return;

    if (str_equal(def.center, "ribbed")) {
	const fastf_t width = resolved.secondary_width * 0.7;
	sketches.push_back(rect_sketch(-0.05, 0.5 - width / 2.0, 1.05, 0.5 + width / 2.0));
	return;
    }

    for (size_t i = 0; i + 1 < resolved.pitch_edges.size(); i += 2) {
	const fastf_t x0 = resolved.pitch_edges[i];
	const fastf_t x1 = resolved.pitch_edges[i + 1];
	const fastf_t pad = (x1 - x0) * 0.22;
	sketches.push_back(rect_sketch(x0 + pad, 0.43, x1 - pad, 0.57));
    }
}

static int
sipe_count_for_density(const std::string &density)
{
    if (str_equal(density, "low")) return 1;
    if (str_equal(density, "medium")) return 2;
    if (str_equal(density, "dense")) return 3;
    return 0;
}

static void
add_sipes(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
	  const TreadGeometryModel &resolved)
{
    const int count = sipe_count_for_density(def.sipes);
    if (count == 0)
	return;

    const std::array<std::pair<fastf_t, fastf_t>, 3> lanes = {{
	{0.12, 0.36}, {0.42, 0.58}, {0.64, 0.88}
    }};

    for (size_t p = 0; p + 1 < resolved.pitch_edges.size(); p++) {
	const fastf_t x0 = resolved.pitch_edges[p];
	const fastf_t x1 = resolved.pitch_edges[p + 1];
	const fastf_t span = x1 - x0;
	for (const auto &lane : lanes) {
	    for (int i = 0; i < count; i++) {
		const fastf_t x = x0 + span * static_cast<fastf_t>(i + 1) / static_cast<fastf_t>(count + 1);
		const fastf_t stagger = ((p + i) % 2) ? span * 0.06 : -span * 0.06;
		if (resolved.groove_angle > 8.0) {
		    sketches.push_back(parallelogram_sketch(x - resolved.sipe_width / 2.0, lane.first,
							    x + resolved.sipe_width / 2.0, lane.second, stagger));
		} else {
		    sketches.push_back(rect_sketch(x - resolved.sipe_width / 2.0, lane.first,
						   x + resolved.sipe_width / 2.0, lane.second));
		}
	    }
	}
    }
}

static void
add_shoulder_features(std::vector<TreadSketch> &sketches, const TreadPatternDefinition &def,
		      const TreadGeometryModel &resolved)
{
    if (str_equal(def.shoulder, "closed"))
	return;

    const fastf_t depth = str_equal(def.shoulder, "open") ? 0.18 : 0.13;
    for (size_t i = 0; i + 1 < resolved.pitch_edges.size(); i++) {
	const fastf_t x0 = resolved.pitch_edges[i];
	const fastf_t x1 = resolved.pitch_edges[i + 1];
	const fastf_t span = x1 - x0;
	const fastf_t left0 = x0 + span * 0.12;
	const fastf_t left1 = x0 + span * (str_equal(def.shoulder, "open") ? 0.58 : 0.42);
	const fastf_t right0 = x0 + span * (str_equal(def.shoulder, "open") ? 0.42 : 0.58);
	const fastf_t right1 = x0 + span * 0.88;
	sketches.push_back(rect_sketch(left0, 0.0, left1, depth));
	sketches.push_back(rect_sketch(right0, 1.0 - depth, right1, 1.0));
    }
}

std::vector<TreadSketch>
generate_tread_sketches(const TreadPatternDefinition &def)
{
    /* Explicit sketches are the geometry override.  The remaining fields are
     * retained as documented pattern metadata and for round-trip editing.
     */
    if (!def.sketches.empty())
	return def.sketches;

    std::vector<TreadSketch> sketches;
    const TreadGeometryModel resolved = resolve_tread_geometry(def);

    if (str_equal(def.type, "rib")) {
	add_longitudinal_grooves(sketches, def, resolved);
    } else if (str_equal(def.type, "lug")) {
	if (str_equal(def.symmetry, "directional"))
	    add_directional_lugs(sketches, def, resolved);
	else
	    add_asymmetric_lugs(sketches, def, resolved);
    } else if (str_equal(def.type, "block")) {
	add_segmented_longitudinal_grooves(sketches, def, resolved);
	add_lateral_block_grooves(sketches, def, resolved, 0.08, 0.92, 0.0);
    } else {
	if (str_equal(def.symmetry, "asymmetric"))
	    add_asymmetric_lugs(sketches, def, resolved);
	else
	    add_directional_lugs(sketches, def, resolved);
    }

    add_center_features(sketches, def, resolved);
    add_sipes(sketches, def, resolved);
    add_shoulder_features(sketches, def, resolved);

    return sketches;
}

static const std::vector<TreadPatternDefinition> &
predefined_tread_patterns()
{
    static const std::vector<TreadPatternDefinition> defs = {
	{TREAD_PATTERN_SCHEMA_VERSION, "legacy-rib", "Original -p 1 ribbed visualization tread", "hybrid", "symmetric", "HT", "all", 0.30, 7, 8.0, "medium", "uniform", 0, {}, "low", "closed", "ribbed", false, 1, 30, legacy_pattern1_sketches()},
	{TREAD_PATTERN_SCHEMA_VERSION, "legacy-block", "Original -p 2 block/lug visualization tread", "block", "symmetric", "AT", "all", 0.36, 4, 18.0, "medium", "uniform", 0, {}, "none", "semi-open", "broken", false, 2, 30, legacy_pattern2_sketches()},
	{TREAD_PATTERN_SCHEMA_VERSION, "highway-rib", "Highway rib tread with closed shoulders", "rib", "symmetric", "HT", "steer", 0.23, 4, 3.0, "small", "uniform", 0, {}, "medium", "closed", "continuous", false, 1, 32, {}},
	{TREAD_PATTERN_SCHEMA_VERSION, "all-terrain", "All-terrain hybrid tread with semi-open shoulders", "hybrid", "asymmetric", "AT", "all", 0.35, 2, 18.0, "medium", "variable", 3, {1.0, 1.12, 0.88}, "low", "semi-open", "broken", false, 2, 14, {}},
	{TREAD_PATTERN_SCHEMA_VERSION, "mud-terrain", "Mud-terrain directional lug tread", "lug", "directional", "MT", "drive", 0.46, 3, 30.0, "large", "variable", 4, {1.15, 0.85, 1.05, 0.95}, "low", "open", "broken", false, 2, 16, {}},
	{TREAD_PATTERN_SCHEMA_VERSION, "winter-siped", "Winter block tread with dense sipes", "block", "symmetric", "HT", "all", 0.32, 5, 8.0, "small", "variable", 3, {0.9, 1.1, 1.0}, "dense", "closed", "broken", true, 1, 28, {}},
	{TREAD_PATTERN_SCHEMA_VERSION, "commercial-rib", "Commercial trailer-style straight rib tread", "rib", "symmetric", "HT", "trailer", 0.20, 5, 0.0, "small", "uniform", 0, {}, "low", "closed", "continuous", true, 1, 32, {}}
    };
    return defs;
}

const TreadPatternDefinition *
find_predefined_tread_pattern(const std::string &id)
{
    const std::string lid = lower_copy(id.c_str());
    const std::vector<TreadPatternDefinition> &defs = predefined_tread_patterns();

    if (lid == "1")
	return &defs[0];
    if (lid == "2")
	return &defs[1];

    for (const TreadPatternDefinition &def : defs) {
	if (lower_copy(def.id.c_str()) == lid)
	    return &def;
    }

    return nullptr;
}

TreadPatternCountRange
tread_pattern_count_range(const TreadPatternDefinition &def)
{
    TreadPatternCountRange range;

    if (str_equal(def.type, "rib")) {
	range.min = 12;
	range.max = 64;
    } else if (str_equal(def.type, "lug")) {
	range.min = 6;
	range.max = 30;
    } else if (str_equal(def.type, "block")) {
	range.min = 10;
	range.max = 44;
    } else {
	range.min = 10;
	range.max = 36;
    }

    if (str_equal(def.terrain, "MT")) {
	range.min = std::max(range.min, 6);
	range.max = std::min(range.max, 28);
    } else if (str_equal(def.terrain, "AT")) {
	range.max = std::min(range.max, 36);
    }

    if (str_equal(def.block, "large"))
	range.max = std::min(range.max, 24);
    else if (str_equal(def.block, "small"))
	range.max = std::min(range.max, 56);

    if (str_equal(def.sipes, "dense"))
	range.max = std::min(range.max, 32);

    return range;
}

static bool
valid_enum(const std::string &value, const std::vector<const char *> &allowed)
{
    for (const char *entry : allowed) {
	if (value == entry)
	    return true;
    }
    return false;
}

static bool
validate_tread_definition(const TreadPatternDefinition &def, struct bu_vls *msg)
{
    if (def.schema_version != TREAD_PATTERN_SCHEMA_VERSION) {
	if (msg) bu_vls_printf(msg, "unsupported tread pattern schema_version %d.\n", def.schema_version);
	return false;
    }
    if (!valid_enum(def.type, {"rib", "lug", "block", "hybrid"}) ||
	!valid_enum(def.symmetry, {"symmetric", "directional", "asymmetric"}) ||
	!valid_enum(def.terrain, {"HT", "AT", "MT"}) ||
	!valid_enum(def.axle, {"steer", "drive", "trailer", "all"}) ||
	!valid_enum(def.block, {"small", "medium", "large"}) ||
	!valid_enum(def.sipes, {"none", "low", "medium", "dense"}) ||
	!valid_enum(def.shoulder, {"closed", "semi-open", "open"}) ||
	!valid_enum(def.center, {"continuous", "broken", "ribbed"})) {
	if (msg) bu_vls_printf(msg, "tread pattern contains an unsupported enum value.\n");
	return false;
    }
    if (!std::isfinite(def.void_ratio) || def.void_ratio < 0.1 || def.void_ratio > 0.7 ||
	def.grooves < 1 || def.grooves > 8 ||
	!std::isfinite(def.groove_angle) || def.groove_angle < 0.0 || def.groove_angle > 60.0 ||
	def.shape < 1 || def.shape > 2 ||
	def.default_count < 3 || def.default_count > MAX_TREAD_PATTERN_COUNT) {
	if (msg) bu_vls_printf(msg, "tread pattern numeric values are outside supported ranges.\n");
	return false;
    }
    const TreadPatternCountRange count_range = tread_pattern_count_range(def);
    if (def.default_count < count_range.min || def.default_count > count_range.max) {
	if (msg) bu_vls_printf(msg, "tread pattern_count must be between %d and %d for this pattern.\n",
			       count_range.min, count_range.max);
	return false;
    }
    if (!valid_enum(def.pitch, {"uniform", "variable"})) {
	if (msg) bu_vls_printf(msg, "tread pattern pitch must be uniform or variable.\n");
	return false;
    }
    if (str_equal(def.pitch, "variable") && def.pitch_count != 0 &&
	(def.pitch_count < 2 || def.pitch_count > 8)) {
	if (msg) bu_vls_printf(msg, "tread pattern pitch count is outside supported range.\n");
	return false;
    }
    if (!def.pitch_sequence.empty()) {
	if (def.pitch_sequence.size() < 2 || def.pitch_sequence.size() > 16) {
	    if (msg) bu_vls_printf(msg, "tread pattern pitch_seq length is outside supported range.\n");
	    return false;
	}
	for (fastf_t entry : def.pitch_sequence) {
	    if (!std::isfinite(entry) || entry < 0.5 || entry > 2.0) {
		if (msg) bu_vls_printf(msg, "tread pattern pitch_seq entries must be between 0.5 and 2.0.\n");
		return false;
	    }
	}
    }
    for (const TreadSketch &sketch : def.sketches) {
	if (sketch.size() < 3) {
	    if (msg) bu_vls_printf(msg, "tread sketch polygons must have at least three points.\n");
	    return false;
	}
	for (const TreadPoint &pt : sketch) {
	    if (!std::isfinite(pt[0]) || !std::isfinite(pt[1])) {
		if (msg) bu_vls_printf(msg, "tread sketch points must be finite.\n");
		return false;
	    }
	}
    }
    return true;
}

bool
load_tread_pattern_file(const char *path, TreadPatternDefinition &def, struct bu_vls *msg)
{
    try {
	std::ifstream file(path);
	if (!file) {
	    if (msg) bu_vls_printf(msg, "unable to open tread pattern file: %s\n", path);
	    return false;
	}

	nlohmann::json j;
	file >> j;

	if (!j.is_object()) {
	    if (msg) bu_vls_printf(msg, "tread pattern file must contain a JSON object: %s\n", path);
	    return false;
	}

	const std::unordered_set<std::string> allowed_keys = {
	    "$schema", "schema_version", "id", "description", "type", "symmetry", "terrain", "axle", "void",
	    "grooves", "groove_angle", "block", "pitch", "pitch_seq", "sipes",
	    "shoulder", "center", "tie_bars", "shape", "pattern_count", "sketches"
	};
	for (auto it = j.begin(); it != j.end(); ++it) {
	    if (allowed_keys.find(it.key()) == allowed_keys.end()) {
		if (msg) bu_vls_printf(msg, "unsupported tread pattern field '%s' in %s\n", it.key().c_str(), path);
		return false;
	    }
	}

	def = TreadPatternDefinition{};
	def.schema_version = j.value("schema_version", TREAD_PATTERN_SCHEMA_VERSION);
	def.id = j.value("id", std::string("custom"));
	def.description = j.value("description", std::string("Custom tread pattern"));
	def.type = j.at("type").get<std::string>();
	def.symmetry = j.at("symmetry").get<std::string>();
	def.terrain = j.value("terrain", def.terrain);
	def.axle = j.value("axle", def.axle);
	def.void_ratio = j.value("void", def.void_ratio);
	def.grooves = j.value("grooves", def.grooves);
	def.groove_angle = j.value("groove_angle", def.groove_angle);
	def.block = j.value("block", def.block);
	def.sipes = j.value("sipes", def.sipes);
	def.shoulder = j.value("shoulder", def.shoulder);
	def.center = j.value("center", def.center);
	def.tie_bars = j.value("tie_bars", def.tie_bars);
	def.shape = j.value("shape", def.shape);
	const bool has_pattern_count = j.contains("pattern_count");
	def.default_count = j.value("pattern_count", def.default_count);

	if (j.contains("pitch")) {
	    if (j["pitch"].is_string()) {
		def.pitch = j["pitch"].get<std::string>();
	    } else if (j["pitch"].is_object()) {
		if (!j["pitch"].contains("type") || !j["pitch"].contains("count")) {
		    if (msg) bu_vls_printf(msg, "tread pattern pitch object requires type and count.\n");
		    return false;
		}
		def.pitch = j["pitch"].value("type", def.pitch);
		def.pitch_count = j["pitch"].value("count", def.pitch_count);
	    } else {
		if (msg) bu_vls_printf(msg, "tread pattern pitch must be a string or object.\n");
		return false;
	    }
	}
	if (j.contains("pitch_seq")) {
	    for (const nlohmann::json &entry : j["pitch_seq"]) {
		def.pitch_sequence.push_back(entry.get<fastf_t>());
	    }
	}

	if (j.contains("sketches")) {
	    for (const nlohmann::json &poly : j["sketches"]) {
		TreadSketch sketch;
		for (const nlohmann::json &pt : poly) {
		    if (!pt.is_array() || pt.size() != 2) {
			if (msg) bu_vls_printf(msg, "invalid tread sketch point in %s\n", path);
			return false;
		    }
		    sketch.push_back(TreadPoint{pt[0].get<fastf_t>(), pt[1].get<fastf_t>()});
		}
		if (sketch.size() < 3) {
		    if (msg) bu_vls_printf(msg, "tread sketch polygons must have at least three points.\n");
		    return false;
		}
		def.sketches.push_back(sketch);
	    }
	}
	if (!has_pattern_count) {
	    const TreadPatternCountRange count_range = tread_pattern_count_range(def);
	    def.default_count = std::max(count_range.min, std::min(def.default_count, count_range.max));
	}
    } catch (const std::exception &e) {
	if (msg) bu_vls_printf(msg, "failed to parse tread pattern file %s: %s\n", path, e.what());
	return false;
    }

    return validate_tread_definition(def, msg);
}

std::string
tread_pattern_json(const TreadPatternDefinition &def)
{
    nlohmann::json j;
    j["$schema"] = "https://brlcad.org/schemas/tire-tread-pattern.schema.json";
    j["schema_version"] = def.schema_version;
    j["id"] = def.id;
    j["description"] = def.description;
    j["type"] = def.type;
    j["symmetry"] = def.symmetry;
    j["terrain"] = def.terrain;
    j["axle"] = def.axle;
    j["void"] = def.void_ratio;
    j["grooves"] = def.grooves;
    j["groove_angle"] = def.groove_angle;
    j["block"] = def.block;
    if (str_equal(def.pitch, "variable") && def.pitch_count != 0) {
	j["pitch"] = {{"type", def.pitch}, {"count", def.pitch_count}};
    } else {
	j["pitch"] = def.pitch;
    }
    if (!def.pitch_sequence.empty())
	j["pitch_seq"] = def.pitch_sequence;
    j["sipes"] = def.sipes;
    j["shoulder"] = def.shoulder;
    j["center"] = def.center;
    j["tie_bars"] = def.tie_bars;
    j["shape"] = def.shape;
    j["pattern_count"] = def.default_count;
    if (!def.sketches.empty()) {
	nlohmann::json sketches = nlohmann::json::array();
	for (const TreadSketch &sketch : def.sketches) {
	    nlohmann::json poly = nlohmann::json::array();
	    for (const TreadPoint &pt : sketch)
		poly.push_back({pt[0], pt[1]});
	    sketches.push_back(poly);
	}
	j["sketches"] = sketches;
    }

    return j.dump(2);
}

void
list_tread_patterns(struct bu_vls *out)
{
    bu_vls_printf(out, "Available tread patterns:\n");
    for (const TreadPatternDefinition &def : predefined_tread_patterns()) {
	bu_vls_printf(out, "  %-16s %s (type=%s, terrain=%s, shape=%d, count=%d)\n",
		      def.id.c_str(), def.description.c_str(), def.type.c_str(),
		      def.terrain.c_str(), def.shape, def.default_count);
    }
}

} /* namespace tire_private */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
