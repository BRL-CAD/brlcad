/*                     G Q A _ A N A L Y Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file gqa.cpp
 * Invocation-local quantitative analysis and reporting engine.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "analyze.h"
#include "analyze/gqa.h"
#include "bn/mat.h"
#include "bn/tol.h"
#include "bu/app.h"
#include "bu/datetime.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/process.h"
#include "../libbg/RTree.h"
#include "raytrace.h"
#include "../libbu/json.hpp"

namespace {

static const size_t DEFAULT_CROFTON_RAYS = 100000;
static const size_t UNCERTAINTY_REPLICATES = 16;
static const size_t ADAPTIVE_PILOT_RAYS = 100000;
static const size_t MAX_GRID_REFINEMENT_LEVELS = 20;
static const size_t DEFAULT_GRID_SAMPLES_PER_DIAGONAL = 128;
static const size_t GRID_PARALLEL_BATCH_RAYS = 16384;
static const size_t GRID_WORK_CHUNK_RAYS = 32;
static const size_t GRID_STABLE_INTERVALS = 2;
static const size_t BOX_STABILITY_BATCH_RAYS = 2000;
static const size_t BOX_DIRECTION_MIN_RAYS = 16;
static const double REPORT_CONFIDENCE = 0.95;
static const double ADAPTIVE_PLANNING_SAFETY_FACTOR = 2.0;
static const double ADAPTIVE_DEADLINE_UTILIZATION = 0.8;
static const double DEFAULT_CROFTON_TIME_MS = 60000.0;
static const double DEFAULT_GRID_TIME_MS = 60000.0;
static const double AUTOMATIC_TOLERANCE_STABILITY_SCALE = 0.1;
static const double AUTOMATIC_SMALL_MODEL_STABILITY_SCALE = 0.001;
static const double PROGRESS_INTERVAL_MS = 1000.0;
static const double DEFAULT_GRID_SPACING = 50.0; /* mm */
static const double DEFAULT_GRID_AZIMUTH_DEGREES = 35.0;
static const double DEFAULT_GRID_ELEVATION_DEGREES = 25.0;
static const double GRID_DEADLINE_UTILIZATION = 0.9;
static const double MIN_GRID_REFINEMENT_FACTOR = 1.05;
/* Deterministic, irrational increments decorrelate successive grid phases. */
static const double GRID_PHASE_U_INCREMENT = 0.6180339887498948482;
static const double GRID_PHASE_V_INCREMENT = 0.4142135623730950488;
static const double DEFAULT_ISSUE_TOLERANCE = 0.0; /* mm */
static const char REPORT_TEMP_MARKER[] = ".gqa-tmp-";

enum Measure { VOLUME = 1, MASS = 2, AREA = 4, CENTROID = 8, MOMENTS = 16 };
enum Check { OVERLAPS = 1, GAPS = 2, ADJACENT_AIR = 4, EXPOSED_AIR = 8 };
enum Sampler { GRID_AXIS, GRID_ROTATED, CROFTON };
enum GridStopReason {
    GRID_STOP_NONE,
    GRID_STOP_REFINEMENT_LIMIT,
    GRID_STOP_STABILITY,
    GRID_STOP_TIME,
    GRID_STOP_MAX_LEVELS
};

struct Options {
    int measures = 0;
    int checks = 0;
    Sampler sampler = GRID_AXIS;
    enum rt_crofton_sequence sequence = RT_CROFTON_SEQUENCE_DEFAULT;
    size_t rays = DEFAULT_CROFTON_RAYS;
    bool rays_set = false;
    double spacing = DEFAULT_GRID_SPACING;
    bool spacing_set = false;
    bool spacing_defaulted = false;
    double tolerance = DEFAULT_ISSUE_TOLERANCE;
    double stability = 0.0;
    bool stability_set = false;
    bool stability_defaulted = false;
    double time_ms = 0.0;
    bool time_set = false;
    bool time_defaulted = false;
    bool include_air = true;
    size_t refine_levels = 0;
    bool refine_set = false;
    uint64_t seed = 0;
    bool seed_set = false;
    double azimuth_degrees = DEFAULT_GRID_AZIMUTH_DEGREES;
    double elevation_degrees = DEFAULT_GRID_ELEVATION_DEGREES;
    bool azimuth_set = false;
    bool elevation_set = false;
    bool uncertainty = false;
    double relative_error = 0.0;
    std::map<std::string, double> absolute_error;
    bool accuracy_scope_all = false;
    std::string json_path;
    std::string invalid_path;
    std::string density_path;
    std::vector<std::string> objects;
};

struct Metric {
    double volume = 0.0;
    double mass = 0.0;
    double area = 0.0;
    std::array<double, 3> first = {{0.0, 0.0, 0.0}};
    std::array<double, 6> second = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    size_t hits = 0;
};

struct Issue {
    std::string type;
    std::string first;
    std::string second;
    long first_instance;
    long second_instance;
    int first_bit;
    int second_bit;
    double depth;
    std::array<double, 3> start;
    std::array<double, 3> end;
    std::array<double, 3> ray_origin;
    std::array<double, 3> ray_direction;
    size_t ray_id;
};

struct OverlapCandidate {
    struct region *first;
    struct region *second;
    vect_t min;
    vect_t max;
    double volume;
};

struct CandidateResult {
    std::string first;
    std::string second;
    int first_bit = -1;
    int second_bit = -1;
    size_t allocated_rays = 0;
    size_t fired_rays = 0;
    size_t overlap_events = 0;
    double stability_mm = 0.0;
    bool stability_evaluated = false;
    bool stability_reached = false;
    bool calibration_reached = false;
    double volume_error = 0.0;
    double area_error = 0.0;
    std::array<size_t, 3> direction_rays = {{0, 0, 0}};
    double event_probability_upper_bound = 0.0;
    bool event_probability_bound_available = false;
};

struct GridPass {
    double spacing = 0.0;
    size_t ray_count = 0;
    double volume = 0.0;
    double mass = 0.0;
    double area = 0.0;
    std::array<double, 3> view_volume = {{0.0, 0.0, 0.0}};
    std::array<double, 3> phase_u = {{0.0, 0.0, 0.0}};
    std::array<double, 3> phase_v = {{0.0, 0.0, 0.0}};
    double directional_spread = 0.0;
    double refinement_stability = 0.0;
    bool refinement_evaluated = false;
    size_t stable_intervals = 0;
    double elapsed_time_ms = 0.0;
};

struct UncertaintyInterval {
    std::string metric;
    double estimate = 0.0;
    double half_width = 0.0;
    bool available = false;
    bool target_applies = false;
    double target_tolerance = 0.0;
    bool target_met = false;
};

struct SampleValues {
    std::map<std::string, double> scalars;
    std::map<std::string, std::pair<double, double>> ratios;
};

struct Run {
    Options opts;
    struct db_i *dbip = NULL;
    struct bu_vls *result = NULL;
    struct rt_i *rtip = NULL;
    struct analyze_densities *densities = NULL;
    std::map<const struct region *, Metric> regions;
    std::vector<Issue> issues;
    std::vector<nlohmann::json> invalid_rays;
    std::map<std::string, std::pair<size_t, size_t>> groups;
    std::set<std::string> bad_materials;
    double model_volume = 0.0;
    double model_mass = 0.0;
    double model_area = 0.0;
    double crofton_total_chord = 0.0;
    double view_volume[3] = {0.0, 0.0, 0.0};
    double view_mass[3] = {0.0, 0.0, 0.0};
    std::array<std::array<double, 3>, 3> grid_directions = {{{{1.0, 0.0, 0.0}},
        {{0.0, 1.0, 0.0}}, {{0.0, 0.0, 1.0}}}};
    point_t grid_min = VINIT_ZERO;
    point_t grid_max = VINIT_ZERO;
    size_t ray_id = 0;
    double observed_stability = 0.0;
    bool observed_stability_evaluated = false;
    size_t invalid_partitions = 0;
    size_t overlap_candidate_count = 0;
    std::vector<CandidateResult> candidate_results;
    const struct region *active_candidate_first = NULL;
    const struct region *active_candidate_second = NULL;
    bool targeted_overlap = false;
    bool collect_box = false;
    point_t box_min = VINIT_ZERO;
    point_t box_max = VINIT_ZERO;
    double box_chord = 0.0;
    size_t box_crossings = 0;
    size_t box_rays = 0;
    double box_stability_mm = 0.0;
    bool box_stability_reached = false;
    bool box_calibration_reached = false;
    double box_exact_volume = 0.0;
    double box_exact_area = 0.0;
    double box_volume_error = 0.0;
    double box_area_error = 0.0;
    size_t candidate_overlap_events = 0;
    double overlap_miss_probability = 0.0;
    std::array<size_t, 3> box_direction_rays = {{0, 0, 0}};
    std::vector<GridPass> grid_passes;
    std::vector<UncertaintyInterval> uncertainty;
    std::string accuracy_status;
    size_t pilot_rays = 0;
    size_t production_rays = 0;
    size_t unused_rays = 0;
    size_t forecast_production_rays = 0;
    size_t planned_production_rays = 0;
    double pilot_time_ms = 0.0;
    double production_time_ms = 0.0;
    double pilot_throughput_rays_per_second = 0.0;
    double elapsed_time_ms = 0.0;
    int64_t analysis_start_us = 0;
    int64_t deadline_us = 0;
    bool deadline_limited = false;
    bool deadline_hit = false;
    bool ray_limit_limited = false;
    enum rt_crofton_stop_reason crofton_stop_reason = RT_CROFTON_STOP_NONE;
    GridStopReason grid_stop_reason = GRID_STOP_NONE;
    size_t discarded_grid_rays = 0;
    analyze_gqa_progress_handler report_progress = NULL;
    void *progress_data = NULL;
    int64_t last_progress_us = 0;
};

static double
elapsed_ms(int64_t start_us)
{
    return static_cast<double>(bu_gettime() - start_us) / 1000.0;
}

static double
remaining_deadline_ms(const Run &run)
{
    if (!run.deadline_us)
        return std::numeric_limits<double>::infinity();
    return std::max(0.0,
        static_cast<double>(run.deadline_us - bu_gettime()) / 1000.0);
}

static bool
parse_list(const std::string &value, const std::map<std::string, int> &choices,
	   int &flags)
{
    flags = 0;
    size_t start = 0;
    while (start < value.size()) {
	const size_t end = value.find(',', start);
	const std::string item = value.substr(start, end - start);
	auto found = choices.find(item);
	if (found == choices.end()) return false;
	flags |= found->second;
	if (end == std::string::npos) return true;
	start = end + 1;
    }
    return false;
}

static bool
parse_finite_double(const char *value, double &out)
{
    char *end = NULL;
    errno = 0;
    out = std::strtod(value, &end);
    return errno == 0 && end != value && *end == '\0' &&
	std::isfinite(out);
}

static bool
parse_positive_size(const char *value, size_t &out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno || value[0] == '-' || end == value || *end != '\0' || !parsed ||
	parsed > static_cast<unsigned long long>(SIZE_MAX)) return false;
    out = static_cast<size_t>(parsed);
    return true;
}

static bool
parse_nonnegative_size(const char *value, size_t &out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno || value[0] == '-' || end == value || *end != '\0' ||
	parsed > static_cast<unsigned long long>(SIZE_MAX)) return false;
    out = static_cast<size_t>(parsed);
    return true;
}

static bool
parse_uint64(const char *value, uint64_t &out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno || value[0] == '-' || end == value || *end != '\0')
        return false;
    out = static_cast<uint64_t>(parsed);
    return true;
}

static bool
parse_absolute_errors(const std::string &value,
    std::map<std::string, double> &targets)
{
    static const std::set<std::string> names = {
        "volume", "mass", "area", "centroid", "moments"};
    targets.clear();
    size_t start = 0;
    while (start < value.size()) {
        const size_t end = value.find(',', start);
        const std::string item = value.substr(start, end - start);
        const size_t separator = item.find('=');
        if (separator == std::string::npos || !separator ||
            separator + 1 == item.size())
            return false;
        const std::string name = item.substr(0, separator);
        double tolerance = 0.0;
        if (names.find(name) == names.end() ||
            !parse_finite_double(item.c_str() + separator + 1, tolerance) ||
            tolerance <= 0.0 || targets.find(name) != targets.end())
            return false;
        targets[name] = tolerance;
        if (end == std::string::npos) return true;
        start = end + 1;
    }
    return false;
}

static bool
absolute_path(const char *path)
{
    if (path[0] == '/' || path[0] == BU_DIR_SEPARATOR) return true;
    return std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
	(path[2] == '/' || path[2] == BU_DIR_SEPARATOR);
}

static bool
normalized_path(const char *path, std::string &normalized)
{
    char resolved[MAXPATHLEN] = {0};
    bu_file_realpath(path, resolved);
    std::string full_path(resolved);
    if (!absolute_path(resolved)) {
	char cwd[MAXPATHLEN] = {0};
	if (!bu_getcwd(cwd, sizeof(cwd))) return false;
	full_path.assign(cwd).append(1, BU_DIR_SEPARATOR).append(resolved);
    }
    const char *normalized_result = bu_path_normalize(full_path.c_str());
    if (!normalized_result) return false;
    normalized.assign(normalized_result);
    return true;
}

static bool
paths_conflict(const std::string &first, const char *second)
{
    if (first.empty() || !second || !second[0]) return false;
    if (first == second || bu_file_same(first.c_str(), second)) return true;
    /* bu_file_same cannot compare two output paths before either exists. */
    std::string normalized_first;
    std::string normalized_second;
    return normalized_path(first.c_str(), normalized_first) &&
	normalized_path(second, normalized_second) &&
	normalized_first == normalized_second;
}

enum AnalysisOption {
    ANALYSIS_MEASURE,
    ANALYSIS_CHECK,
    ANALYSIS_SAMPLER,
    ANALYSIS_SEQUENCE,
    ANALYSIS_RAYS,
    ANALYSIS_SPACING,
    ANALYSIS_STABILITY,
    ANALYSIS_TIME,
    ANALYSIS_AIR,
    ANALYSIS_REFINE,
    ANALYSIS_SEED,
    ANALYSIS_AZIMUTH,
    ANALYSIS_ELEVATION,
    ANALYSIS_RELATIVE_ERROR,
    ANALYSIS_ABSOLUTE_ERROR,
    ANALYSIS_ACCURACY_SCOPE,
    ANALYSIS_TOLERANCE,
    ANALYSIS_JSON,
    ANALYSIS_INVALID_RAYS,
    ANALYSIS_DENSITY,
    ANALYSIS_OPTION_COUNT
};

struct AnalysisOptionState {
    Options *opts = NULL;
    bool measure_set = false;
    bool check_set = false;
    bool sequence_set = false;
    bool rays_set = false;
    bool spacing_set = false;
    bool stability_set = false;
    bool time_set = false;
    bool refine_set = false;
    bool accuracy_scope_set = false;
    int uncertainty = 0;
};

struct AnalysisOptionTarget {
    AnalysisOptionState *state = NULL;
    AnalysisOption option = ANALYSIS_OPTION_COUNT;
    const char *name = NULL;
};

struct AnalysisOptionDefinition {
    AnalysisOption option;
    const char *name;
    const char *argument;
    const char *help;
};

static int
parse_analysis_option(struct bu_vls *msg, size_t argc, const char **argv,
    void *set_var)
{
    static const std::map<std::string, int> measures = {
	{"volume", VOLUME}, {"mass", MASS}, {"area", AREA},
	{"centroid", CENTROID}, {"moments", MOMENTS}};
    static const std::map<std::string, int> checks = {
	{"overlaps", OVERLAPS}, {"gaps", GAPS},
	{"adjacent-air", ADJACENT_AIR}, {"exposed-air", EXPOSED_AIR}};
    AnalysisOptionTarget *target = static_cast<AnalysisOptionTarget *>(set_var);
    if (!target || !target->state || !target->state->opts) {
	if (msg) bu_vls_printf(msg, "Internal GQA option parser error.\n");
	return -1;
    }
    BU_OPT_CHECK_ARGV0(msg, argc, argv, target->name);

    AnalysisOptionState &state = *target->state;
    Options &opts = *state.opts;
    const char *value = argv[0];
    bool valid = true;
    switch (target->option) {
	case ANALYSIS_MEASURE:
	    state.measure_set = true;
	    valid = parse_list(value, measures, opts.measures);
	    break;
	case ANALYSIS_CHECK:
	    state.check_set = true;
	    valid = parse_list(value, checks, opts.checks);
	    break;
	case ANALYSIS_SAMPLER:
	    valid = !bu_strcmp(value, "grid") ||
		!bu_strcmp(value, "grid-rotated") ||
		!bu_strcmp(value, "crofton");
	    opts.sampler = !bu_strcmp(value, "crofton") ? CROFTON :
		(!bu_strcmp(value, "grid-rotated") ? GRID_ROTATED : GRID_AXIS);
	    break;
	case ANALYSIS_SEQUENCE:
	    state.sequence_set = true;
	    if (!bu_strcmp(value, "random"))
		opts.sequence = RT_CROFTON_SEQUENCE_RANDOM;
	    else if (!bu_strcmp(value, "qmc"))
		opts.sequence = RT_CROFTON_SEQUENCE_QMC;
	    else
		valid = false;
	    break;
	case ANALYSIS_RAYS:
	    state.rays_set = true;
	    opts.rays_set = true;
	    valid = parse_positive_size(value, opts.rays);
	    break;
	case ANALYSIS_SPACING:
	    state.spacing_set = true;
	    opts.spacing_set = true;
	    valid = parse_finite_double(value, opts.spacing) && opts.spacing > 0.0;
	    break;
	case ANALYSIS_STABILITY:
	    state.stability_set = true;
	    opts.stability_set = true;
	    valid = parse_finite_double(value, opts.stability) && opts.stability > 0.0;
	    break;
	case ANALYSIS_TIME:
	    state.time_set = true;
	    opts.time_set = true;
	    valid = parse_finite_double(value, opts.time_ms) && opts.time_ms > 0.0;
	    break;
	case ANALYSIS_AIR:
	    valid = !bu_strcmp(value, "include") || !bu_strcmp(value, "exclude");
	    opts.include_air = !bu_strcmp(value, "include");
	    break;
	case ANALYSIS_REFINE:
	    state.refine_set = true;
	    opts.refine_set = true;
	    valid = parse_nonnegative_size(value, opts.refine_levels) &&
		opts.refine_levels <= MAX_GRID_REFINEMENT_LEVELS;
	    break;
	case ANALYSIS_SEED:
	    opts.seed_set = true;
	    valid = parse_uint64(value, opts.seed);
	    break;
	case ANALYSIS_AZIMUTH:
	    opts.azimuth_set = true;
	    valid = parse_finite_double(value, opts.azimuth_degrees);
	    break;
	case ANALYSIS_ELEVATION:
	    opts.elevation_set = true;
	    valid = parse_finite_double(value, opts.elevation_degrees) &&
		opts.elevation_degrees >= -90.0 &&
		opts.elevation_degrees <= 90.0;
	    break;
	case ANALYSIS_RELATIVE_ERROR:
	    valid = parse_finite_double(value, opts.relative_error) &&
		opts.relative_error > 0.0;
	    opts.uncertainty = valid;
	    break;
	case ANALYSIS_ABSOLUTE_ERROR:
	    valid = parse_absolute_errors(value, opts.absolute_error);
	    opts.uncertainty = valid;
	    break;
	case ANALYSIS_ACCURACY_SCOPE:
	    state.accuracy_scope_set = true;
	    valid = !bu_strcmp(value, "model") || !bu_strcmp(value, "all");
	    opts.accuracy_scope_all = !bu_strcmp(value, "all");
	    break;
	case ANALYSIS_TOLERANCE:
	    valid = parse_finite_double(value, opts.tolerance) &&
		opts.tolerance >= 0.0;
	    break;
	case ANALYSIS_JSON:
	    opts.json_path = value;
	    valid = value[0] != '\0';
	    break;
	case ANALYSIS_INVALID_RAYS:
	    opts.invalid_path = value;
	    valid = value[0] != '\0';
	    break;
	case ANALYSIS_DENSITY:
	    opts.density_path = value;
	    valid = value[0] != '\0';
	    break;
	case ANALYSIS_OPTION_COUNT:
	    valid = false;
	    break;
    }
    if (!valid) {
	if (msg)
	    bu_vls_printf(msg, "Invalid value for --%s: %s\n",
		target->name, value);
	return -1;
    }
    return 1;
}

static void
set_analysis_option(struct bu_opt_desc &descriptor,
    AnalysisOptionTarget &target, AnalysisOptionState &state,
    AnalysisOption option, const char *name, const char *argument,
    const char *help)
{
    target.state = &state;
    target.option = option;
    target.name = name;
    BU_OPT(descriptor, "", name, argument, &parse_analysis_option, &target,
	help);
}

static int
parse_analysis_arguments(struct bu_vls *msg, std::vector<const char *> &args,
    AnalysisOptionState &state)
{
    static const AnalysisOptionDefinition definitions[ANALYSIS_OPTION_COUNT] = {
	{ANALYSIS_MEASURE, "measure", "list", "Select reported measures"},
	{ANALYSIS_CHECK, "check", "list", "Select geometry checks"},
	{ANALYSIS_SAMPLER, "sampler", "grid|grid-rotated|crofton", "Select sampler"},
	{ANALYSIS_SEQUENCE, "sequence", "random|qmc", "Select ray sequence"},
	{ANALYSIS_RAYS, "rays", "count", "Set Crofton ray count"},
	{ANALYSIS_SPACING, "spacing", "mm", "Set grid spacing"},
	{ANALYSIS_STABILITY, "stability", "mm", "Set stability tolerance"},
	{ANALYSIS_TIME, "time", "ms", "Set time limit"},
	{ANALYSIS_AIR, "air", "include|exclude", "Control air regions"},
	{ANALYSIS_REFINE, "refine", "levels", "Set grid refinement levels"},
	{ANALYSIS_SEED, "seed", "value", "Set randomization seed"},
	{ANALYSIS_AZIMUTH, "azimuth", "degrees", "Set rotated-grid azimuth"},
	{ANALYSIS_ELEVATION, "elevation", "degrees", "Set rotated-grid elevation"},
	{ANALYSIS_RELATIVE_ERROR, "relative-error", "fraction",
	    "Set relative accuracy target"},
	{ANALYSIS_ABSOLUTE_ERROR, "absolute-error", "targets",
	    "Set absolute accuracy targets"},
	{ANALYSIS_ACCURACY_SCOPE, "accuracy-scope", "model|all",
	    "Set accuracy target scope"},
	{ANALYSIS_TOLERANCE, "tolerance", "mm", "Set issue tolerance"},
	{ANALYSIS_JSON, "json", "file", "Write JSON report"},
	{ANALYSIS_INVALID_RAYS, "invalid-rays", "file", "Write invalid rays"},
	{ANALYSIS_DENSITY, "density", "file", "Read density table"}
    };
    struct bu_opt_desc descriptors[ANALYSIS_OPTION_COUNT + 2];
    AnalysisOptionTarget targets[ANALYSIS_OPTION_COUNT];

    for (size_t i = 0; i < ANALYSIS_OPTION_COUNT; i++) {
	const AnalysisOptionDefinition &definition = definitions[i];
	set_analysis_option(descriptors[i], targets[i], state, definition.option,
	    definition.name, definition.argument, definition.help);
    }
    BU_OPT(descriptors[ANALYSIS_OPTION_COUNT], "", "uncertainty", "", NULL,
	&state.uncertainty, "Report uncertainty");
    BU_OPT_NULL(descriptors[ANALYSIS_OPTION_COUNT + 1]);

    const char *empty = NULL;
    return bu_opt_parse(msg, args.size(), args.empty() ? &empty : args.data(),
	descriptors);
}

static bool
is_unknown_analysis_option(const char *argument)
{
    return argument && std::strlen(argument) > 2 && argument[0] == '-' &&
	argument[1] == '-';
}

static bool
parse_options(struct db_i *dbip, struct bu_vls *result, int argc,
    const char *argv[], Options &opts)
{
    if (!argv || argc < 2) return false;
    std::vector<const char *> args;
    for (int i = 2; i < argc; i++) {
	if (!argv[i]) return false;
	args.push_back(argv[i]);
    }
    AnalysisOptionState state;
    state.opts = &opts;
    const int positional = parse_analysis_arguments(result, args, state);
    if (positional < 0) return false;
    opts.uncertainty = opts.uncertainty || state.uncertainty;
    for (int i = 0; i < positional; i++) {
	if (is_unknown_analysis_option(args[i])) {
	    bu_vls_printf(result, "Invalid analysis option: %s\n", args[i]);
	    return false;
	}
	opts.objects.push_back(args[i]);
    }
    if (!state.measure_set && !state.check_set) opts.measures = VOLUME | AREA;
    bool absolute_targets_valid = true;
    for (const auto &target : opts.absolute_error) {
        const int required_measure =
            target.first == "volume" ? VOLUME :
            target.first == "mass" ? MASS :
            target.first == "area" ? AREA :
            target.first == "centroid" ? CENTROID : MOMENTS;
        if (!(opts.measures & required_measure)) {
            absolute_targets_valid = false;
            break;
        }
    }
    const bool crofton = opts.sampler == CROFTON;
    const bool rotated = opts.sampler == GRID_ROTATED;
    if (opts.objects.empty() ||
	(!crofton && (state.sequence_set || state.rays_set || opts.seed_set)) ||
	(crofton && state.spacing_set) ||
	(crofton && state.refine_set) ||
	(!rotated && (opts.azimuth_set || opts.elevation_set)) ||
	(opts.uncertainty && opts.time_ms > 0.0 &&
	    opts.relative_error <= 0.0 && opts.absolute_error.empty()) ||
	(opts.uncertainty && (!crofton || opts.checks ||
	    opts.stability > 0.0 || opts.measures == 0 ||
	    opts.rays < 2 * UNCERTAINTY_REPLICATES ||
	    !opts.invalid_path.empty())) ||
        !absolute_targets_valid ||
	(state.accuracy_scope_set && !opts.uncertainty) ||
	((opts.measures & (CENTROID | MOMENTS)) && !(opts.measures & MASS))) {
	bu_vls_printf(result,
	    "Usage: gqa --analyze [--measure volume,mass,area,centroid,moments] "
	    "[--check overlaps,gaps,adjacent-air,exposed-air] "
	    "[--sampler grid|grid-rotated|crofton] [--sequence random|qmc] "
	    "[--spacing mm|--rays count] [--stability mm] [--time ms] "
	    "[--refine levels] [--azimuth degrees] [--elevation degrees] "
	    "[--air include|exclude] [--seed value] "
	    "[--uncertainty] [--relative-error fraction] "
	    "[--absolute-error measure=value[,measure=value...]] "
	    "[--accuracy-scope model|all] [--tolerance mm] [--density file] "
	    "[--json file] [--invalid-rays file] object [objects...]\n");
	return false;
    }
    if (paths_conflict(opts.json_path, dbip->dbi_filename) ||
        paths_conflict(opts.invalid_path, dbip->dbi_filename) ||
        paths_conflict(opts.json_path, opts.density_path.c_str()) ||
        paths_conflict(opts.invalid_path, opts.density_path.c_str()) ||
        paths_conflict(opts.json_path, opts.invalid_path.c_str())) {
	bu_vls_printf(result,
	    "Analysis output files must be distinct from inputs and each other.\n");
	return false;
    }
    if (crofton && opts.sequence == RT_CROFTON_SEQUENCE_QMC &&
	!rt_crofton_qmc_available()) {
	bu_vls_printf(result, "QMC is unavailable in this build.\n");
	return false;
    }
    return true;
}

static void add_metric(Metric &to, const Metric &from);
static void reset_measurements(Run &run);

static std::string
issue_key(const Issue &issue)
{
    return issue.type + "\n" + issue.first + "\n" +
	std::to_string(issue.first_bit) + "\n" + issue.second + "\n" +
	std::to_string(issue.second_bit);
}

static Issue
make_issue(const char *type, const struct region *first,
    const struct region *second, double depth, const point_t start,
    const point_t end, const struct xray *ray, size_t ray_id)
{
    Issue issue;
    issue.type = type;
    issue.first = first ? first->reg_name : "";
    issue.second = second ? second->reg_name : "";
    issue.first_instance = first ? first->reg_instnum : -1;
    issue.second_instance = second ? second->reg_instnum : -1;
    issue.first_bit = first ? first->reg_bit : -1;
    issue.second_bit = second ? second->reg_bit : -1;
    if (issue.type == "overlap" &&
	std::make_pair(issue.second, issue.second_bit) <
	std::make_pair(issue.first, issue.first_bit)) {
	std::swap(issue.first, issue.second);
	std::swap(issue.first_instance, issue.second_instance);
	std::swap(issue.first_bit, issue.second_bit);
    }
    issue.depth = depth;
    issue.start = {{start[X], start[Y], start[Z]}};
    issue.end = {{end[X], end[Y], end[Z]}};
    issue.ray_origin = {{ray->r_pt[X], ray->r_pt[Y], ray->r_pt[Z]}};
    issue.ray_direction = {{ray->r_dir[X], ray->r_dir[Y], ray->r_dir[Z]}};
    issue.ray_id = ray_id;
    return issue;
}

static void
store_issue(std::vector<Issue> &issues,
    std::map<std::string, std::pair<size_t, size_t>> &groups,
    bool keep_all, const Issue &issue)
{
    auto &group = groups[issue_key(issue)];
    if (!keep_all) {
	if (group.first == 0) {
	    group.second = issues.size();
	    issues.push_back(issue);
	} else if (issue.depth > issues[group.second].depth) {
	    issues[group.second] = issue;
	}
    } else {
	if (group.first == 0 || issue.depth > issues[group.second].depth)
	    group.second = issues.size();
	issues.push_back(issue);
    }
    group.first++;
}

static void
record_issue(Run *run, const char *type, const struct region *first,
		     const struct region *second,
		     double depth, const point_t start, const point_t end,
             const struct xray *ray, size_t ray_id)
{
    const Issue issue = make_issue(type, first, second, depth, start, end,
	ray, ray_id);
    store_issue(run->issues, run->groups, !run->opts.json_path.empty(), issue);
}

static double
density_for(Run *run, std::set<std::string> &bad_materials,
    const struct region *region)
{
    if (!(run->opts.measures & MASS)) return 0.0;
    const double density = analyze_densities_density(run->densities, region->reg_gmater);
    if (density < 0.0 || region->reg_los < 1) {
	bad_materials.insert(region->reg_name);
	return 0.0;
    }
    return density * region->reg_los * 0.01;
}

static double
density_for(Run *run, const struct region *region)
{
    return density_for(run, run->bad_materials, region);
}

static void
accumulate_segment(Metric &metric, double volume, double mass,
		   const point_t in_point, const point_t out_point,
		   const double transverse_covariance[6])
{
    metric.volume += volume;
    metric.mass += mass;
    metric.hits++;
    double mid[3], delta[3];
    for (int i = 0; i < 3; i++) {
	mid[i] = 0.5 * (in_point[i] + out_point[i]);
	delta[i] = out_point[i] - in_point[i];
	metric.first[i] += mass * mid[i];
    }
    /* Grid rays represent clipped rectangular cells; Crofton rays have no
     * transverse cell extent. */
    const double x2 = mid[0]*mid[0] + delta[0]*delta[0]/12.0 +
	transverse_covariance[0];
    const double y2 = mid[1]*mid[1] + delta[1]*delta[1]/12.0 +
	transverse_covariance[1];
    const double z2 = mid[2]*mid[2] + delta[2]*delta[2]/12.0 +
	transverse_covariance[2];
    const double xy = mid[0]*mid[1] + delta[0]*delta[1]/12.0 +
	transverse_covariance[3];
    const double xz = mid[0]*mid[2] + delta[0]*delta[2]/12.0 +
	transverse_covariance[4];
    const double yz = mid[1]*mid[2] + delta[1]*delta[2]/12.0 +
	transverse_covariance[5];
    metric.second[0] += mass*(y2+z2);
    metric.second[1] += mass*(x2+z2);
    metric.second[2] += mass*(x2+y2);
    metric.second[3] -= mass*xy;
    metric.second[4] -= mass*xz;
    metric.second[5] -= mass*yz;
}

struct GridView {
    point_t center = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
    vect_t u = VINIT_ZERO;
    vect_t v = VINIT_ZERO;
    double direction_min = 0.0;
    double u_min = 0.0;
    double u_max = 0.0;
    double v_min = 0.0;
    double v_max = 0.0;
    double u_start = 0.0;
    double v_start = 0.0;
    double spacing = 0.0;
    double phase_u = 0.0;
    double phase_v = 0.0;
    size_t u_count = 0;
    size_t v_count = 0;
    size_t ray_count = 0;
};

struct GridAccumulator {
    Run *run = NULL;
    std::map<const struct region *, Metric> regions;
    std::vector<Issue> issues;
    std::map<std::string, std::pair<size_t, size_t>> groups;
    std::set<std::string> bad_materials;
    double view_volume[3] = {0.0, 0.0, 0.0};
    double view_mass[3] = {0.0, 0.0, 0.0};
    double cell_area = 0.0;
    double cell_covariance[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    size_t ray_count = 0;
    size_t ray_id = 0;
    int view = 0;
};

struct GridWorker {
    struct resource resource;
    GridAccumulator accumulator;
    bool active = false;
};

struct GridParallelState {
    const GridView *grid = NULL;
    Run *run = NULL;
    std::vector<GridWorker> *workers = NULL;
    std::atomic<size_t> next;
    size_t end = 0;
    size_t ray_id_base = 0;
    int view = 0;
};

static void
record_grid_issue(GridAccumulator *accumulator, const char *type,
    const struct region *first, const struct region *second, double depth,
    const point_t start, const point_t end, const struct xray *ray)
{
    const Issue issue = make_issue(type, first, second, depth, start, end,
	ray, accumulator->ray_id);
    store_issue(accumulator->issues, accumulator->groups,
	!accumulator->run->opts.json_path.empty(), issue);
}

static int
grid_overlap(struct application *ap, struct partition *part,
	     struct region *first, struct region *second,
	     struct partition *UNUSED(head))
{
    GridAccumulator *accumulator = static_cast<GridAccumulator *>(ap->a_uptr);
    Run *run = accumulator->run;
    if (first->reg_aircode && !second->reg_aircode) return 2;
    if (second->reg_aircode && !first->reg_aircode) return 1;
    const double depth = part->pt_outhit->hit_dist - part->pt_inhit->hit_dist;
    if ((run->opts.checks & OVERLAPS) && depth > run->opts.tolerance) {
	point_t start, end;
	VJOIN1(start, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(end, ap->a_ray.r_pt, part->pt_outhit->hit_dist, ap->a_ray.r_dir);
	record_grid_issue(accumulator, "overlap", first, second,
	    depth, start, end, &ap->a_ray);
    }
    return 1;
}

static int
grid_hit(struct application *ap, struct partition *head,
	 struct seg *UNUSED(segs))
{
    GridAccumulator *accumulator = static_cast<GridAccumulator *>(ap->a_uptr);
    Run *run = accumulator->run;
    const int view = accumulator->view;
    const double cell_area = accumulator->cell_area;
    const struct partition *previous = NULL;
    for (struct partition *part = head->pt_forw; part != head; part = part->pt_forw) {
	const struct region *region = part->pt_regionp;
	const double depth = part->pt_outhit->hit_dist - part->pt_inhit->hit_dist;
	if (depth <= 0.0) continue;
	point_t in_point, out_point;
	VJOIN1(in_point, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(out_point, ap->a_ray.r_pt, part->pt_outhit->hit_dist, ap->a_ray.r_dir);
	if (previous) {
	    point_t previous_out;
	    VJOIN1(previous_out, ap->a_ray.r_pt,
		previous->pt_outhit->hit_dist, ap->a_ray.r_dir);
	    const double gap = part->pt_inhit->hit_dist - previous->pt_outhit->hit_dist;
	    if ((run->opts.checks & GAPS) && gap > run->opts.tolerance) {
		const bool same_region = previous->pt_regionp == region;
		record_grid_issue(accumulator,
		    same_region ? "internal-void" : "gap",
		    previous->pt_regionp, same_region ? NULL : region,
		    gap, previous_out, in_point, &ap->a_ray);
	    }
	    if ((run->opts.checks & ADJACENT_AIR) &&
		previous->pt_regionp->reg_aircode && region->reg_aircode &&
		previous->pt_regionp->reg_aircode != region->reg_aircode &&
		gap <= run->opts.tolerance)
		    record_grid_issue(accumulator, "adjacent-air",
			previous->pt_regionp, region, gap, previous_out, in_point,
			&ap->a_ray);
	}
	if ((run->opts.checks & EXPOSED_AIR) && region->reg_aircode &&
	    (!previous || part->pt_inhit->hit_dist > previous->pt_outhit->hit_dist))
	    record_grid_issue(accumulator, "exposed-air", region, NULL,
		depth, in_point, out_point, &ap->a_ray);
	previous = part;
	if (!run->opts.include_air && region->reg_aircode) continue;
	const double volume = depth * cell_area / 3.0;
	const double mass = volume * density_for(run,
	    accumulator->bad_materials, region);
	Metric &metric = accumulator->regions[region];
	accumulate_segment(metric, volume, mass, in_point, out_point,
	    accumulator->cell_covariance);
	accumulator->view_volume[view] += depth * cell_area;
	accumulator->view_mass[view] += mass * 3.0;
	if (run->opts.measures & AREA) {
	    /* A view's hit density is proportional to |n dot direction|.
	     * Dividing by the sum for the orthonormal triad apportions each
	     * surface once across the three grids. */
	    vect_t normal;
	    struct soltab *in_solid = part->pt_inseg->seg_stp;
	    struct soltab *out_solid = part->pt_outseg->seg_stp;
	    RT_HIT_NORMAL(normal, part->pt_inhit, in_solid, &ap->a_ray, part->pt_inflip);
	    double in_den = 0.0;
	    for (int i = 0; i < 3; i++)
		in_den += std::fabs(VDOT(normal, run->grid_directions[i].data()));
	    if (in_den > SMALL_FASTF) metric.area += cell_area / in_den;
	    RT_HIT_NORMAL(normal, part->pt_outhit, out_solid, &ap->a_ray, part->pt_outflip);
	    double out_den = 0.0;
	    for (int i = 0; i < 3; i++)
		out_den += std::fabs(VDOT(normal, run->grid_directions[i].data()));
	    if (out_den > SMALL_FASTF) metric.area += cell_area / out_den;
	}
    }
    if ((run->opts.checks & EXPOSED_AIR) && previous &&
	previous->pt_regionp->reg_aircode) {
	point_t in_point, out_point;
	VJOIN1(in_point, ap->a_ray.r_pt, previous->pt_inhit->hit_dist,
	    ap->a_ray.r_dir);
	VJOIN1(out_point, ap->a_ray.r_pt, previous->pt_outhit->hit_dist,
	    ap->a_ray.r_dir);
	record_grid_issue(accumulator, "exposed-air", previous->pt_regionp, NULL,
	    previous->pt_outhit->hit_dist - previous->pt_inhit->hit_dist,
	    in_point, out_point, &ap->a_ray);
    }
    return 1;
}

static int
grid_miss(struct application *UNUSED(ap))
{
    return 0;
}

static double
fractional_part(double value)
{
    return value - std::floor(value);
}

static void
setup_grid_directions(Run &run)
{
    if (run.opts.sampler == GRID_AXIS) return;

    const double azimuth = run.opts.azimuth_degrees * DEG2RAD;
    const double elevation = run.opts.elevation_degrees * DEG2RAD;
    vect_t first, second, third;
    VSET(first, std::cos(elevation) * std::cos(azimuth),
	std::cos(elevation) * std::sin(azimuth), std::sin(elevation));
    VUNITIZE(first);
    bn_vec_perp(second, first);
    VUNITIZE(second);
    VCROSS(third, first, second);
    VUNITIZE(third);
    for (int coordinate = 0; coordinate < 3; coordinate++) {
	run.grid_directions[0][coordinate] = first[coordinate];
	run.grid_directions[1][coordinate] = second[coordinate];
	run.grid_directions[2][coordinate] = third[coordinate];
    }
}

static bool
setup_grid_view(GridView &grid, const Run &run, int view, size_t level,
    double spacing)
{
    VADD2SCALE(grid.center, run.grid_min, run.grid_max, 0.5);
    VMOVE(grid.direction, run.grid_directions[view].data());
    VMOVE(grid.u, run.grid_directions[(view + 1) % 3].data());
    VMOVE(grid.v, run.grid_directions[(view + 2) % 3].data());
    grid.spacing = spacing;
    if (run.opts.sampler == GRID_ROTATED) {
	/* Changing the phase avoids repeatedly sampling the same lattice
	 * alignment at every refinement.  These reproducible toroidal shifts
	 * are motivated by Cranley and Patterson, "Randomization of Number
	 * Theoretic Methods for Multiple Integration" (1976),
	 * https://doi.org/10.1137/0713071.  They are deterministic here and do
	 * not imply that paper's randomized-error guarantees. */
	grid.phase_u = fractional_part((level + 1) * GRID_PHASE_U_INCREMENT +
	    (view + 1) * GRID_PHASE_V_INCREMENT);
	grid.phase_v = fractional_part((level + 1) * GRID_PHASE_V_INCREMENT +
	    (view + 1) * GRID_PHASE_U_INCREMENT);
    }

    grid.direction_min = grid.u_min = grid.v_min =
	std::numeric_limits<double>::infinity();
    grid.u_max = grid.v_max = -std::numeric_limits<double>::infinity();
    for (int x = 0; x < 2; x++) {
	for (int y = 0; y < 2; y++) {
	    for (int z = 0; z < 2; z++) {
		point_t corner;
		VSET(corner, x ? run.grid_max[X] : run.grid_min[X],
		    y ? run.grid_max[Y] : run.grid_min[Y],
		    z ? run.grid_max[Z] : run.grid_min[Z]);
		vect_t delta;
		VSUB2(delta, corner, grid.center);
		grid.direction_min = std::min(grid.direction_min,
		    VDOT(delta, grid.direction));
		const double projected_u = VDOT(delta, grid.u);
		const double projected_v = VDOT(delta, grid.v);
		grid.u_min = std::min(grid.u_min, projected_u);
		grid.u_max = std::max(grid.u_max, projected_u);
		grid.v_min = std::min(grid.v_min, projected_v);
		grid.v_max = std::max(grid.v_max, projected_v);
	    }
	}
    }
    grid.u_start = grid.u_min - grid.phase_u * spacing;
    grid.v_start = grid.v_min - grid.phase_v * spacing;
    const double u_samples = std::ceil((grid.u_max - grid.u_start) / spacing);
    const double v_samples = std::ceil((grid.v_max - grid.v_start) / spacing);
    bool invalid = !std::isfinite(u_samples) || !std::isfinite(v_samples) ||
	u_samples < 1.0 || v_samples < 1.0 ||
	u_samples >= static_cast<double>(SIZE_MAX) ||
	v_samples >= static_cast<double>(SIZE_MAX);
    if (!invalid) {
	grid.u_count = static_cast<size_t>(u_samples);
	grid.v_count = static_cast<size_t>(v_samples);
	invalid = grid.u_count > SIZE_MAX / grid.v_count;
    }
    if (invalid) return false;
    grid.ray_count = grid.u_count * grid.v_count;
    return true;
}

static void
grid_cell(const GridView &grid, size_t index, struct xray &ray,
    double &area, double covariance[6])
{
    const size_t u_index = index / grid.v_count;
    const size_t v_index = index % grid.v_count;
    const double cell_u_min = std::max(grid.u_min,
	grid.u_start + u_index * grid.spacing);
    const double cell_u_max = std::min(grid.u_max,
	grid.u_start + (u_index + 1) * grid.spacing);
    const double cell_v_min = std::max(grid.v_min,
	grid.v_start + v_index * grid.spacing);
    const double cell_v_max = std::min(grid.v_max,
	grid.v_start + (v_index + 1) * grid.spacing);
    const double u_width = cell_u_max - cell_u_min;
    const double v_width = cell_v_max - cell_v_min;
    const double u_coordinate = 0.5 * (cell_u_min + cell_u_max);
    const double v_coordinate = 0.5 * (cell_v_min + cell_v_max);
    VJOIN3(ray.r_pt, grid.center, u_coordinate, grid.u,
	v_coordinate, grid.v, grid.direction_min - grid.spacing,
	grid.direction);
    VMOVE(ray.r_dir, grid.direction);
    area = u_width * v_width;
    const double u_variance = u_width * u_width / 12.0;
    const double v_variance = v_width * v_width / 12.0;
    covariance[0] = u_variance * grid.u[X] * grid.u[X] +
	v_variance * grid.v[X] * grid.v[X];
    covariance[1] = u_variance * grid.u[Y] * grid.u[Y] +
	v_variance * grid.v[Y] * grid.v[Y];
    covariance[2] = u_variance * grid.u[Z] * grid.u[Z] +
	v_variance * grid.v[Z] * grid.v[Z];
    covariance[3] = u_variance * grid.u[X] * grid.u[Y] +
	v_variance * grid.v[X] * grid.v[Y];
    covariance[4] = u_variance * grid.u[X] * grid.u[Z] +
	v_variance * grid.v[X] * grid.v[Z];
    covariance[5] = u_variance * grid.u[Y] * grid.u[Z] +
	v_variance * grid.v[Y] * grid.v[Z];
}

static void
grid_worker(int cpu, void *data)
{
    GridParallelState *state = static_cast<GridParallelState *>(data);
    if (cpu < 0 || cpu >= MAX_PSW) return;
    GridWorker &worker = state->workers->at(static_cast<size_t>(cpu));
    GridAccumulator &accumulator = worker.accumulator;
    if (!worker.active) {
	worker.active = true;
	accumulator.run = state->run;
	memset(&worker.resource, 0, sizeof(worker.resource));
	rt_init_resource(&worker.resource, cpu, state->run->rtip);
    }
    accumulator.view = state->view;
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = accumulator.run->rtip;
    ap.a_resource = &worker.resource;
    ap.a_uptr = &accumulator;
    ap.a_hit = grid_hit;
    ap.a_miss = grid_miss;
    ap.a_overlap = grid_overlap;
    ap.a_logoverlap = rt_silent_logoverlap;
    while (true) {
	const size_t begin = state->next.fetch_add(GRID_WORK_CHUNK_RAYS);
	if (begin >= state->end) break;
	const size_t end = std::min(begin + GRID_WORK_CHUNK_RAYS, state->end);
	for (size_t index = begin; index < end; index++) {
	    grid_cell(*state->grid, index, ap.a_ray, accumulator.cell_area,
		accumulator.cell_covariance);
	    accumulator.ray_id = state->ray_id_base + index;
	    rt_shootray(&ap);
	    accumulator.ray_count++;
	}
    }
}

static size_t
grid_accumulated_rays(const std::vector<GridWorker> &workers)
{
    size_t rays = 0;
    for (const auto &worker : workers)
	rays += worker.accumulator.ray_count;
    return rays;
}

static std::array<double, 3>
grid_accumulated_view_volumes(const std::vector<GridWorker> &workers)
{
    std::array<double, 3> volumes = {{0.0, 0.0, 0.0}};
    for (const auto &worker : workers) {
	for (int view = 0; view < 3; view++)
	    volumes[view] += worker.accumulator.view_volume[view];
    }
    return volumes;
}

static void
merge_grid_issues(Run &run, const GridAccumulator &accumulator)
{
    run.bad_materials.insert(accumulator.bad_materials.begin(),
	accumulator.bad_materials.end());
    if (!run.opts.json_path.empty()) {
	for (const auto &issue : accumulator.issues)
	    store_issue(run.issues, run.groups, true, issue);
	return;
    }
    for (const auto &item : accumulator.groups) {
	const Issue &example = accumulator.issues[item.second.second];
	auto &destination = run.groups[item.first];
	if (!destination.first) {
	    destination.second = run.issues.size();
	    run.issues.push_back(example);
	} else if (example.depth > run.issues[destination.second].depth) {
	    run.issues[destination.second] = example;
	}
	destination.first += item.second.first;
    }
}

static void
merge_grid_measurements(Run &run, const std::vector<GridWorker> &workers)
{
    reset_measurements(run);
    for (const auto &worker : workers) {
	const GridAccumulator &accumulator = worker.accumulator;
	for (const auto &item : accumulator.regions)
	    add_metric(run.regions[item.first], item.second);
	for (int view = 0; view < 3; view++) {
	    run.view_volume[view] += accumulator.view_volume[view];
	    run.view_mass[view] += accumulator.view_mass[view];
	}
    }
    for (int view = 0; view < 3; view++) {
	run.model_volume += run.view_volume[view] / 3.0;
	run.model_mass += run.view_mass[view] / 3.0;
    }
    for (const auto &item : run.regions)
	run.model_area += item.second.area;
}

static void
report_grid_progress(Run &run, size_t level, double spacing, int view,
    size_t pass_rays, size_t pass_total,
    const std::vector<GridWorker> &workers)
{
    if (!run.report_progress) return;
    const int64_t now_us = bu_gettime();
    if (run.last_progress_us &&
	static_cast<double>(now_us - run.last_progress_us) / 1000.0 <
	    PROGRESS_INTERVAL_MS) return;
    run.last_progress_us = now_us;
    const double total_elapsed_ms = elapsed_ms(run.analysis_start_us);
    const double rays_per_second = total_elapsed_ms > 0.0 ?
	1000.0 * run.ray_id / total_elapsed_ms : 0.0;
    const std::array<double, 3> volumes =
	grid_accumulated_view_volumes(workers);
    char message[768] = {0};
    snprintf(message, sizeof(message),
	"gqa grid progress: %.1f s, level %zu, spacing %.9g mm, "
	"view %d/3, %zu/%zu pass rays, %zu total rays, %.3g rays/s, "
	"view volumes [%.9g, %.9g, %.9g] mm^3",
	total_elapsed_ms / 1000.0, level, spacing, view + 1, pass_rays,
	pass_total, run.ray_id, rays_per_second, volumes[0], volumes[1],
	volumes[2]);
    run.report_progress(message, run.progress_data);
}

enum GridPassStatus { GRID_PASS_ERROR, GRID_PASS_COMPLETE, GRID_PASS_TIME };

static GridPassStatus
run_grid_pass(Run &run, size_t level, double spacing, GridPass &pass)
{
    GridView views[3];
    size_t pass_total = 0;
    for (int view = 0; view < 3; view++) {
	if (!setup_grid_view(views[view], run, view, level, spacing) ||
	    views[view].ray_count > SIZE_MAX - pass_total) {
	    bu_vls_printf(run.result,
		"Grid dimensions are invalid; increase --spacing.\n");
	    return GRID_PASS_ERROR;
	}
	pass_total += views[view].ray_count;
	pass.phase_u[view] = views[view].phase_u;
	pass.phase_v[view] = views[view].phase_v;
    }

    size_t parallel_count = std::max(static_cast<size_t>(1), bu_avail_cpus());
    parallel_count = std::min(parallel_count,
	static_cast<size_t>(MAX_PSW - 1));
    std::vector<GridWorker> workers(MAX_PSW);

    const size_t initial_ray_count = run.ray_id;
    size_t view_ray_offset = 0;
    GridPassStatus status = GRID_PASS_COMPLETE;
    for (int view = 0; view < 3 && status == GRID_PASS_COMPLETE; view++) {
	for (size_t begin = 0; begin < views[view].ray_count;
	     begin += GRID_PARALLEL_BATCH_RAYS) {
	    GridParallelState state;
	    state.grid = &views[view];
	    state.run = &run;
	    state.workers = &workers;
	    state.end = std::min(begin + GRID_PARALLEL_BATCH_RAYS,
		views[view].ray_count);
	    state.next.store(begin);
	    state.ray_id_base = initial_ray_count + view_ray_offset;
	    state.view = view;
	    bu_parallel(grid_worker, parallel_count, &state);
	    const size_t accumulated = grid_accumulated_rays(workers);
	    run.ray_id = initial_ray_count + accumulated;
	    report_grid_progress(run, level, spacing, view, accumulated,
		pass_total, workers);
	    if (run.deadline_us && remaining_deadline_ms(run) <= 0.0 &&
		(state.end < views[view].ray_count || view < 2)) {
		status = GRID_PASS_TIME;
		break;
	    }
	}
	view_ray_offset += views[view].ray_count;
    }

    for (const auto &worker : workers)
	merge_grid_issues(run, worker.accumulator);
    if (status == GRID_PASS_COMPLETE)
	merge_grid_measurements(run, workers);
    else
	run.discarded_grid_rays += run.ray_id - initial_ray_count;

    for (size_t i = 0; i < workers.size(); i++) {
	if (workers[i].active &&
	    workers[i].resource.re_magic == RESOURCE_MAGIC) {
	    rt_clean_resource_basic(run.rtip, &workers[i].resource);
	    BU_PTBL_SET(&run.rtip->rti_resources, i, NULL);
	}
    }
    pass.spacing = spacing;
    pass.ray_count = run.ray_id - initial_ray_count;
    return status;
}

static void
reset_measurements(Run &run)
{
    run.model_volume = 0.0;
    run.model_mass = 0.0;
    run.model_area = 0.0;
    run.crofton_total_chord = 0.0;
    run.observed_stability = 0.0;
    run.observed_stability_evaluated = false;
    for (int axis = 0; axis < 3; axis++) {
        run.view_volume[axis] = 0.0;
        run.view_mass[axis] = 0.0;
    }
    for (auto &item : run.regions)
        item.second = Metric();
}

static bool
run_grid(Run &run)
{
    setup_grid_directions(run);
    const size_t maximum_level = run.opts.refine_set ?
	run.opts.refine_levels :
	((run.opts.time_ms > 0.0 || run.opts.stability > 0.0) ?
	    MAX_GRID_REFINEMENT_LEVELS : 0);
    double spacing = run.opts.spacing;
    for (size_t level = 0; level <= maximum_level; level++) {
	GridPass pass;
	const int64_t pass_start_us = bu_gettime();
	const GridPassStatus status = run_grid_pass(run, level, spacing, pass);
	pass.elapsed_time_ms = elapsed_ms(pass_start_us);
	if (status == GRID_PASS_ERROR) return false;
	if (status == GRID_PASS_TIME) {
	    run.grid_stop_reason = GRID_STOP_TIME;
	    run.deadline_hit = true;
	    if (run.grid_passes.empty()) {
		bu_vls_printf(run.result,
		    "Grid deadline expired before one complete pass.\n");
		return false;
	    }
	    break;
	}
	pass.volume = run.model_volume;
	pass.mass = run.model_mass;
	pass.area = run.model_area;
	for (int view = 0; view < 3; view++)
	    pass.view_volume[view] = run.view_volume[view];
	double minimum_radius = std::numeric_limits<double>::infinity();
	double maximum_radius = 0.0;
	for (double volume : pass.view_volume) {
	    const double radius = volume > 0.0 ?
		std::cbrt(3.0 * volume / (4.0 * M_PI)) : 0.0;
	    minimum_radius = std::min(minimum_radius, radius);
	    maximum_radius = std::max(maximum_radius, radius);
	}
	pass.directional_spread = maximum_radius - minimum_radius;
	if (!run.grid_passes.empty()) {
	    const GridPass &previous = run.grid_passes.back();
	    double stability = 0.0;
	    if (run.opts.measures & AREA) {
		const double current_radius = pass.area > 0.0 ?
		    std::sqrt(pass.area / (4.0 * M_PI)) : 0.0;
		const double previous_radius = previous.area > 0.0 ?
		    std::sqrt(previous.area / (4.0 * M_PI)) : 0.0;
		stability = std::fabs(current_radius - previous_radius);
	    }
	    if (run.opts.measures & (VOLUME | MASS | CENTROID | MOMENTS)) {
		const double current_radius = pass.volume > 0.0 ?
		    std::cbrt(3.0 * pass.volume / (4.0 * M_PI)) : 0.0;
		const double previous_radius = previous.volume > 0.0 ?
		    std::cbrt(3.0 * previous.volume / (4.0 * M_PI)) : 0.0;
		stability = std::max(stability,
		    std::fabs(current_radius - previous_radius));
	    }
	    pass.refinement_stability = stability;
	    pass.refinement_evaluated = true;
	    if (run.opts.stability > 0.0 &&
		stability <= run.opts.stability &&
		pass.directional_spread <= run.opts.stability)
		pass.stable_intervals = previous.stable_intervals + 1;
	}
	run.grid_passes.push_back(pass);
	run.observed_stability = std::max(pass.refinement_stability,
	    pass.directional_spread);
	run.observed_stability_evaluated = pass.refinement_evaluated;
	if (pass.stable_intervals >= GRID_STABLE_INTERVALS) {
	    run.grid_stop_reason = GRID_STOP_STABILITY;
	    break;
	}
	if (level == maximum_level) {
	    run.grid_stop_reason = run.opts.refine_set ?
		GRID_STOP_REFINEMENT_LIMIT : GRID_STOP_MAX_LEVELS;
	    break;
	}

	double next_spacing = spacing * 0.5;
	if (run.deadline_us) {
	    const double remaining_ms = remaining_deadline_ms(run);
	    if (remaining_ms <= 0.0 || pass.elapsed_time_ms <= 0.0) {
		run.grid_stop_reason = GRID_STOP_TIME;
		run.deadline_hit = remaining_ms <= 0.0;
		break;
	    }
	    const double throughput = pass.ray_count / pass.elapsed_time_ms;
	    const double affordable_rays = throughput * remaining_ms *
		GRID_DEADLINE_UTILIZATION;
	    const double minimum_refinement_rays = pass.ray_count *
		MIN_GRID_REFINEMENT_FACTOR * MIN_GRID_REFINEMENT_FACTOR;
	    if (affordable_rays < minimum_refinement_rays) {
		run.grid_stop_reason = GRID_STOP_TIME;
		run.deadline_limited = true;
		break;
	    }
	    const double half_spacing_rays = 4.0 * pass.ray_count;
	    if (affordable_rays < half_spacing_rays) {
		next_spacing = spacing *
		    std::sqrt(pass.ray_count / affordable_rays);
		next_spacing = std::max(spacing * 0.5,
		    std::min(spacing / MIN_GRID_REFINEMENT_FACTOR,
			next_spacing));
		run.deadline_limited = true;
	    }
	}
	spacing = next_spacing;
    }
    return true;
}

static void
visit_segment(const struct rt_crofton_segment *segment, void *data)
{
    Run *run = static_cast<Run *>(data);
    const struct region *region = segment->region;
    if (!region) return;
    static const double no_transverse_variance[6] =
	{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const double thickness = segment->thickness;
    run->crofton_total_chord += thickness;
    if (!run->opts.include_air && region->reg_aircode) return;
    Metric &metric = run->regions[region];
    accumulate_segment(metric, thickness,
	thickness * density_for(run, region), segment->in_point,
	segment->out_point, no_transverse_variance);
    metric.area += 1.0;
}

static bool
line_box_interval(const point_t origin, const vect_t direction,
    const point_t box_min, const point_t box_max,
    double &near_distance, double &far_distance)
{
    near_distance = -INFINITY;
    far_distance = INFINITY;
    for (int axis = 0; axis < 3; axis++) {
        if (ZERO(direction[axis])) {
            if (origin[axis] < box_min[axis] ||
                origin[axis] > box_max[axis]) return false;
            continue;
        }
        double t1 = (box_min[axis] - origin[axis]) / direction[axis];
        double t2 = (box_max[axis] - origin[axis]) / direction[axis];
        if (t1 > t2) std::swap(t1, t2);
        near_distance = std::max(near_distance, t1);
        far_distance = std::min(far_distance, t2);
    }
    return far_distance > near_distance;
}

static double
ray_box_chord(const struct rt_crofton_ray *ray, const point_t box_min,
    const point_t box_max)
{
    double near_distance, far_distance;
    return line_box_interval(ray->origin, ray->direction, box_min, box_max,
        near_distance, far_distance) ? far_distance - near_distance : 0.0;
}

static void
visit_ray(const struct rt_crofton_ray *ray, void *data)
{
    Run *run = static_cast<Run *>(data);
    static const double no_transverse_variance[6] =
	{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    run->ray_id = ray->ray_id;
    if (run->collect_box) {
        const double chord = ray_box_chord(ray, run->box_min, run->box_max);
        run->box_rays++;
        if (chord > 0.0) {
            run->box_chord += chord;
            run->box_crossings += 2;
            size_t direction = 0;
            if (fabs(ray->direction[Y]) > fabs(ray->direction[X])) direction = 1;
            if (fabs(ray->direction[Z]) > fabs(ray->direction[direction])) direction = 2;
            run->box_direction_rays[direction]++;
        }
    }
    struct xray sampled_ray;
    VMOVE(sampled_ray.r_pt, ray->origin);
    VMOVE(sampled_ray.r_dir, ray->direction);
    const struct rt_crofton_segment *previous = NULL;
    for (size_t i = 0; i < ray->segment_count; i++) {
	const struct rt_crofton_segment *segment = &ray->segments[i];
	const struct region *region = segment->region;
	if (!region) continue;
	const double thickness = segment->thickness;
	run->crofton_total_chord += thickness;
	if (run->opts.include_air || !region->reg_aircode) {
	    Metric &metric = run->regions[region];
	    accumulate_segment(metric, thickness,
		thickness * density_for(run, region), segment->in_point,
		segment->out_point, no_transverse_variance);
	    metric.area += 1.0;
	}
	if (previous && previous->region) {
	    const double gap = segment->in_distance - previous->out_distance;
	    if ((run->opts.checks & GAPS) && gap > run->opts.tolerance) {
		const bool same_region = previous->region == region;
		record_issue(run, same_region ? "internal-void" : "gap",
		    previous->region, same_region ? NULL : region, gap,
		    previous->out_point, segment->in_point, &sampled_ray,
		    ray->ray_id);
	    } else if ((run->opts.checks & ADJACENT_AIR) &&
		previous->region->reg_aircode && region->reg_aircode &&
		previous->region->reg_aircode != region->reg_aircode &&
		gap <= run->opts.tolerance) {
		record_issue(run, "adjacent-air", previous->region, region, gap,
		    previous->out_point, segment->in_point, &sampled_ray,
		    ray->ray_id);
	    }
	}
	if ((run->opts.checks & EXPOSED_AIR) && region->reg_aircode &&
	    (!previous || segment->in_distance > previous->out_distance))
	    record_issue(run, "exposed-air", region, NULL, thickness,
		segment->in_point, segment->out_point, &sampled_ray, ray->ray_id);
	previous = segment;
    }
    if ((run->opts.checks & EXPOSED_AIR) && previous && previous->region &&
	previous->region->reg_aircode)
	record_issue(run, "exposed-air", previous->region, NULL,
	    previous->thickness, previous->in_point, previous->out_point,
	    &sampled_ray, ray->ray_id);
}

static void
visit_invalid(const struct rt_crofton_invalid_ray *ray, void *data)
{
    Run *run = static_cast<Run *>(data);
    run->invalid_rays.push_back({
        {"ray_id", ray->ray_id},
        {"origin_mm", {ray->origin[0], ray->origin[1], ray->origin[2]}},
        {"direction", {ray->direction[0], ray->direction[1], ray->direction[2]}},
        {"in_distance_mm", ray->in_distance},
        {"out_distance_mm", ray->out_distance},
        {"thickness_mm", ray->thickness},
        {"region", ray->region ? ray->region->reg_name : ""},
        {"reason", ray->reason ? ray->reason : "unknown"}});
}

static std::vector<OverlapCandidate>
find_overlap_candidates(struct rt_i *rtip, double tolerance)
{
    std::vector<struct region *> regions;
    RTree<size_t, double, 3> index;
    struct region *region;
    for (BU_LIST_FOR (region, region, &rtip->HeadRegion)) {
        vect_t min, max;
        if (!region->reg_treetop || rt_bound_tree(region->reg_treetop, min, max) < 0)
            continue;
        if (!std::isfinite(min[X]) || !std::isfinite(min[Y]) || !std::isfinite(min[Z]) ||
            !std::isfinite(max[X]) || !std::isfinite(max[Y]) || !std::isfinite(max[Z]) ||
            min[X] > max[X] || min[Y] > max[Y] || min[Z] > max[Z])
            continue;
        const size_t id = regions.size();
        regions.push_back(region);
        double rmin[3] = {min[X] - tolerance, min[Y] - tolerance, min[Z] - tolerance};
        double rmax[3] = {max[X] + tolerance, max[Y] + tolerance, max[Z] + tolerance};
        index.Insert(rmin, rmax, id);
    }

    std::vector<OverlapCandidate> candidates;
    for (size_t i = 0; i < regions.size(); i++) {
        vect_t first_min, first_max;
        if (rt_bound_tree(regions[i]->reg_treetop, first_min, first_max) < 0)
            continue;
        double query_min[3] = {first_min[X] - tolerance, first_min[Y] - tolerance,
                               first_min[Z] - tolerance};
        double query_max[3] = {first_max[X] + tolerance, first_max[Y] + tolerance,
                               first_max[Z] + tolerance};
        index.Search(query_min, query_max,
            [&](const size_t &j, void *) {
                if (j <= i) return true;
                vect_t second_min, second_max;
                if (rt_bound_tree(regions[j]->reg_treetop, second_min, second_max) < 0)
                    return true;
                OverlapCandidate candidate;
                candidate.first = regions[i];
                candidate.second = regions[j];
                VSET(candidate.min,
                    std::max(first_min[X], second_min[X]),
                    std::max(first_min[Y], second_min[Y]),
                    std::max(first_min[Z], second_min[Z]));
                VSET(candidate.max,
                    std::min(first_max[X], second_max[X]),
                    std::min(first_max[Y], second_max[Y]),
                    std::min(first_max[Z], second_max[Z]));
                if (candidate.min[X] < candidate.max[X] &&
                    candidate.min[Y] < candidate.max[Y] &&
                    candidate.min[Z] < candidate.max[Z]) {
                    candidate.volume = (candidate.max[X] - candidate.min[X]) *
                        (candidate.max[Y] - candidate.min[Y]) *
                        (candidate.max[Z] - candidate.min[Z]);
                    candidates.push_back(candidate);
                }
                return true;
            }, NULL);
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const OverlapCandidate &left, const OverlapCandidate &right) {
            if (left.volume > right.volume) return true;
            if (left.volume < right.volume) return false;
            const int first_name = bu_strcmp(left.first->reg_name,
                right.first->reg_name);
            if (first_name) return first_name < 0;
            if (left.first->reg_bit != right.first->reg_bit)
                return left.first->reg_bit < right.first->reg_bit;
            const int second_name = bu_strcmp(left.second->reg_name,
                right.second->reg_name);
            if (second_name) return second_name < 0;
            return left.second->reg_bit < right.second->reg_bit;
        });
    return candidates;
}

static void
visit_overlap(const struct rt_crofton_overlap *overlap, void *data)
{
    Run *run = static_cast<Run *>(data);
    if ((overlap->first_region->reg_aircode &&
         !overlap->second_region->reg_aircode) ||
        (overlap->second_region->reg_aircode &&
         !overlap->first_region->reg_aircode))
        return;

    double depth = overlap->depth;
    point_t start, end;
    VMOVE(start, overlap->in_point);
    VMOVE(end, overlap->out_point);
    if (run->collect_box) {
        const bool active_pair =
            (overlap->first_region == run->active_candidate_first &&
             overlap->second_region == run->active_candidate_second) ||
            (overlap->first_region == run->active_candidate_second &&
             overlap->second_region == run->active_candidate_first);
        if (!active_pair) return;

        double box_near, box_far;
        if (!line_box_interval(overlap->origin, overlap->direction,
                run->box_min, run->box_max, box_near, box_far))
            return;
        vect_t from_origin;
        VSUB2(from_origin, overlap->in_point, overlap->origin);
        const double overlap_near = VDOT(from_origin, overlap->direction);
        VSUB2(from_origin, overlap->out_point, overlap->origin);
        const double overlap_far = VDOT(from_origin, overlap->direction);
        const double clipped_near = std::max(box_near, overlap_near);
        const double clipped_far = std::min(box_far, overlap_far);
        depth = clipped_far - clipped_near;
        if (depth <= run->opts.tolerance) return;
        VJOIN1(start, overlap->origin, clipped_near, overlap->direction);
        VJOIN1(end, overlap->origin, clipped_far, overlap->direction);
        run->candidate_overlap_events++;
    } else if (depth <= run->opts.tolerance) {
        return;
    }

    struct xray ray;
    VMOVE(ray.r_pt, overlap->origin);
    VMOVE(ray.r_dir, overlap->direction);
    record_issue(run, "overlap", overlap->first_region, overlap->second_region,
        depth, start, end, &ray, overlap->ray_id);
}

static bool
run_crofton_targeted_overlaps(Run &run, const std::vector<OverlapCandidate> &candidates)
{
    run.targeted_overlap = true;
    run.overlap_candidate_count = candidates.size();
    if (candidates.empty()) return true;
    const size_t candidate_count = candidates.size();
    const size_t rays_per_candidate = run.opts.rays / candidate_count;
    const size_t extra_ray_count = run.opts.rays % candidate_count;
    size_t ray_offset = 0;
    for (size_t candidate_index = 0; candidate_index < candidate_count; candidate_index++) {
        const auto &candidate = candidates[candidate_index];
        const size_t candidate_budget = rays_per_candidate +
            (candidate_index < extra_ray_count ? 1 : 0);
        CandidateResult candidate_result;
        candidate_result.first = candidate.first->reg_name;
        candidate_result.second = candidate.second->reg_name;
        candidate_result.first_bit = candidate.first->reg_bit;
        candidate_result.second_bit = candidate.second->reg_bit;
        candidate_result.allocated_rays = candidate_budget;
        if (!candidate_budget) {
            run.candidate_results.push_back(candidate_result);
            continue;
        }

        VMOVE(run.box_min, candidate.min);
        VMOVE(run.box_max, candidate.max);
        run.active_candidate_first = candidate.first;
        run.active_candidate_second = candidate.second;
        run.collect_box = true;
        run.box_chord = 0.0;
        run.box_crossings = 0;
        run.box_rays = 0;
        run.box_stability_reached = false;
        run.box_calibration_reached = false;
        run.box_stability_mm = 0.0;
        run.box_volume_error = 0.0;
        run.box_area_error = 0.0;
        run.candidate_overlap_events = 0;
        run.overlap_miss_probability = 0.0;
        run.box_exact_volume = (candidate.max[X] - candidate.min[X]) *
            (candidate.max[Y] - candidate.min[Y]) *
            (candidate.max[Z] - candidate.min[Z]);
        run.box_exact_area = 2.0 * ((candidate.max[X] - candidate.min[X]) *
            (candidate.max[Y] - candidate.min[Y]) +
            (candidate.max[X] - candidate.min[X]) * (candidate.max[Z] - candidate.min[Z]) +
            (candidate.max[Y] - candidate.min[Y]) * (candidate.max[Z] - candidate.min[Z]));
        run.box_direction_rays = {{0, 0, 0}};
        double previous_radius = -1.0;
        size_t candidate_fired = 0;
        const size_t invalid_before = run.invalid_partitions;
        do {
            const size_t batch = run.opts.stability > 0.0
                ? std::min(BOX_STABILITY_BATCH_RAYS, candidate_budget - candidate_fired)
                : candidate_budget;
            struct rt_crofton_stats stats;
            struct rt_crofton_params params = {
                batch, 0.0, 0.0, RT_CROFTON_STABILITY_DEFAULT, NULL, NULL};
            const int result = rt_crofton_visit_rays_seeded_ex(&stats,
                run.rtip, &params, ray_offset, candidate.min, candidate.max,
                run.opts.sequence, run.opts.seed, 0, visit_ray, visit_overlap,
                !run.opts.invalid_path.empty() ? visit_invalid : NULL, &run);
            if (result < 0) {
                run.collect_box = false;
                run.active_candidate_first = NULL;
                run.active_candidate_second = NULL;
                return false;
            }
            ray_offset += stats.ray_count;
            run.invalid_partitions += stats.invalid_partition_count;
            candidate_fired += stats.ray_count;
            if (run.opts.stability > 0.0 && run.box_rays && run.box_chord > 0.0) {
                const double dx = candidate.max[X] - candidate.min[X];
                const double dy = candidate.max[Y] - candidate.min[Y];
                const double dz = candidate.max[Z] - candidate.min[Z];
                const double radius = 0.5 * sqrt(dx * dx + dy * dy + dz * dz);
                const double estimate = M_PI * radius * radius *
                    run.box_chord / static_cast<double>(run.box_rays);
                const double area_estimate = 2.0 * M_PI * radius * radius *
                    static_cast<double>(run.box_crossings) /
                    static_cast<double>(run.box_rays);
                run.box_volume_error = run.box_exact_volume > 0.0
                    ? fabs(estimate - run.box_exact_volume) / run.box_exact_volume : 0.0;
                run.box_area_error = run.box_exact_area > 0.0
                    ? fabs(area_estimate - run.box_exact_area) / run.box_exact_area : 0.0;
                const double equivalent_radius = cbrt(estimate * 3.0 /
                    (4.0 * M_PI));
                const double calibration_tolerance = std::max(0.001,
                    run.opts.stability / std::max(radius, 1.0));
                const bool directional_coverage =
                    run.box_direction_rays[0] >= BOX_DIRECTION_MIN_RAYS &&
                    run.box_direction_rays[1] >= BOX_DIRECTION_MIN_RAYS &&
                    run.box_direction_rays[2] >= BOX_DIRECTION_MIN_RAYS;
                run.box_calibration_reached = run.box_volume_error <= calibration_tolerance &&
                    run.box_area_error <= 2.0 * calibration_tolerance && directional_coverage;
                if (previous_radius >= 0.0) {
                    const double change = fabs(equivalent_radius - previous_radius);
                    run.box_stability_mm = change;
                    if (change <= run.opts.stability && run.box_calibration_reached) {
                        run.box_stability_reached = true;
                        break;
                    }
                }
                previous_radius = equivalent_radius;
            }
        } while (candidate_fired < candidate_budget);
        if (run.opts.sequence == RT_CROFTON_SEQUENCE_RANDOM &&
            run.candidate_overlap_events == 0 && candidate_fired > 0) {
            /* Solve (1-p)^N = 1-confidence for the exact zero-event bound. */
            run.overlap_miss_probability = -expm1(
                log(1.0 - REPORT_CONFIDENCE) /
                static_cast<double>(candidate_fired));
        }
        candidate_result.fired_rays = candidate_fired;
        candidate_result.overlap_events = run.candidate_overlap_events;
        candidate_result.stability_mm = run.box_stability_mm;
        candidate_result.stability_evaluated =
            run.opts.stability > 0.0 && candidate_fired > BOX_STABILITY_BATCH_RAYS;
        candidate_result.stability_reached = run.box_stability_reached;
        candidate_result.calibration_reached = run.box_calibration_reached;
        candidate_result.volume_error = run.box_volume_error;
        candidate_result.area_error = run.box_area_error;
        candidate_result.direction_rays = run.box_direction_rays;
        candidate_result.event_probability_upper_bound =
            run.overlap_miss_probability;
        candidate_result.event_probability_bound_available =
            run.opts.sequence == RT_CROFTON_SEQUENCE_RANDOM &&
            run.candidate_overlap_events == 0 && candidate_fired > 0 &&
            run.invalid_partitions == invalid_before;
        run.candidate_results.push_back(candidate_result);
        run.collect_box = false;
        run.active_candidate_first = NULL;
        run.active_candidate_second = NULL;
    }
    run.ray_id = ray_offset;
    return true;
}

static void
report_crofton_progress(size_t ray_count, size_t UNUSED(crossing_count),
    double UNUSED(surface_area), double volume, double stability_mm,
    int stability_evaluated, double UNUSED(sampling_elapsed_ms), void *data)
{
    Run *run = static_cast<Run *>(data);
    if (!run->report_progress)
        return;
    const int64_t now_us = bu_gettime();
    if (run->last_progress_us &&
        static_cast<double>(now_us - run->last_progress_us) / 1000.0 <
            PROGRESS_INTERVAL_MS)
        return;
    run->last_progress_us = now_us;
    const double total_elapsed_ms = elapsed_ms(run->analysis_start_us);
    const size_t total_rays = run->ray_id + ray_count;
    const double rays_per_second = total_elapsed_ms > 0.0 ?
        1000.0 * total_rays / total_elapsed_ms : 0.0;
    char message[512] = {0};
    if (stability_evaluated && run->opts.stability > 0.0) {
        snprintf(message, sizeof(message),
            "gqa progress: %.1f s, %zu rays, %.3g rays/s, "
            "current volume %.9g mm^3, stability %.6g / %.6g mm",
            total_elapsed_ms / 1000.0, total_rays, rays_per_second,
            volume, stability_mm, run->opts.stability);
    } else {
        snprintf(message, sizeof(message),
            "gqa progress: %.1f s, %zu rays, %.3g rays/s, "
            "current volume %.9g mm^3",
            total_elapsed_ms / 1000.0, total_rays, rays_per_second, volume);
    }
    run->report_progress(message, run->progress_data);
}

static bool
run_crofton_single(Run &run, size_t ray_count, uint64_t stream_id,
    struct rt_crofton_session *session = NULL, double time_ms = -1.0)
{
    struct rt_crofton_stats stats;
    const double sampling_time_ms = time_ms >= 0.0 ?
        time_ms : run.opts.time_ms;
    unsigned int stability_metrics = RT_CROFTON_STABILITY_DEFAULT;
    if (run.opts.measures & AREA)
        stability_metrics |= RT_CROFTON_STABILITY_SURFACE_AREA;
    if (run.opts.measures & (VOLUME | MASS | CENTROID | MOMENTS))
        stability_metrics |= RT_CROFTON_STABILITY_VOLUME;
    struct rt_crofton_params params = {
        ray_count, run.opts.stability, sampling_time_ms,
        stability_metrics,
        run.report_progress ? report_crofton_progress : NULL,
        run.report_progress ? &run : NULL};
    const size_t initial_ray_count = run.ray_id;
    const bool use_ray_diagnostics = run.opts.checks != 0;
    int visit_result;
    if (use_ray_diagnostics) {
        visit_result = rt_crofton_visit_rays_seeded_ex(&stats, run.rtip,
            &params, 0, NULL, NULL, run.opts.sequence, run.opts.seed,
            stream_id, visit_ray,
            (run.opts.checks & OVERLAPS) ? visit_overlap : NULL,
            !run.opts.invalid_path.empty() ? visit_invalid : NULL, &run);
    } else if (session) {
        visit_result = rt_crofton_session_visit_seeded(session, &stats,
            &params, 0, NULL, NULL, run.opts.sequence, run.opts.seed,
            stream_id, visit_segment, NULL, &run);
    } else {
        visit_result = rt_crofton_visit_seeded(&stats, run.rtip, &params,
            0, NULL, NULL, run.opts.sequence, run.opts.seed, stream_id,
            visit_segment, !run.opts.invalid_path.empty() ?
            visit_invalid : NULL, &run);
    }
    if (visit_result < 0) {
	bu_vls_printf(run.result,
	    "Crofton sampling failed.\n");
	return false;
    }
    if (!std::isfinite(stats.volume) || !std::isfinite(stats.surface_area)) {
	bu_vls_printf(run.result,
	    "Crofton time budget produced a non-finite estimate.\n");
	return false;
    }
    run.model_volume = run.opts.include_air ? stats.volume : 0.0;
    run.model_area = run.opts.include_air ? stats.surface_area : 0.0;
    run.ray_id = initial_ray_count + stats.ray_count;
    run.observed_stability = stats.stability_mm;
    run.observed_stability_evaluated = stats.stability_evaluated != 0;
    run.crofton_stop_reason = stats.stop_reason;
    run.invalid_partitions += stats.invalid_partition_count;
    const double volume_scale = run.crofton_total_chord > 0.0 ?
	stats.volume/run.crofton_total_chord : 0.0;
    const double area_scale = stats.crossing_count ?
	stats.surface_area/(stats.crossing_count/2.0) : 0.0;
    for (auto &item : run.regions) {
	Metric &metric = item.second;
	metric.volume *= volume_scale;
	metric.mass *= volume_scale;
	for (double &value : metric.first) value *= volume_scale;
	for (double &value : metric.second) value *= volume_scale;
	metric.area *= area_scale;
	if (!run.opts.include_air) {
	    run.model_volume += metric.volume;
	    run.model_area += metric.area;
	}
	run.model_mass += metric.mass;
    }
    return true;
}

static void
divide_metric(Metric &metric, double divisor)
{
    metric.volume /= divisor;
    metric.mass /= divisor;
    metric.area /= divisor;
    for (double &value : metric.first) value /= divisor;
    for (double &value : metric.second) value /= divisor;
    metric.hits = static_cast<size_t>(
        static_cast<double>(metric.hits) / divisor);
}

static Metric
model_metric(const Run &run)
{
    Metric total;
    for (const auto &item : run.regions) add_metric(total, item.second);
    total.volume = run.model_volume;
    total.mass = run.model_mass;
    total.area = run.model_area;
    return total;
}

static void
append_metric_values(SampleValues &values,
    const std::string &prefix, const Metric &metric, int measures)
{
    if (measures & VOLUME)
        values.scalars[prefix + "volume_mm3"] = metric.volume;
    if (measures & MASS)
        values.scalars[prefix + "mass_g"] = metric.mass;
    if (measures & AREA)
        values.scalars[prefix + "area_mm2"] = metric.area;
    if (measures & CENTROID) {
        values.ratios[prefix + "centroid_x_mm"] =
            std::make_pair(metric.first[0], metric.mass);
        values.ratios[prefix + "centroid_y_mm"] =
            std::make_pair(metric.first[1], metric.mass);
        values.ratios[prefix + "centroid_z_mm"] =
            std::make_pair(metric.first[2], metric.mass);
    }
    if (measures & MOMENTS) {
        static const char *names[6] = {
            "inertia_xx_g_mm2", "inertia_yy_g_mm2", "inertia_zz_g_mm2",
            "inertia_xy_g_mm2", "inertia_xz_g_mm2", "inertia_yz_g_mm2"};
        for (size_t i = 0; i < 6; i++)
            values.scalars[prefix + names[i]] = metric.second[i];
    }
}

static bool
assign_object(const Options &opts, const char *region_path,
    std::string &assigned)
{
    size_t matches = 0;
    const std::string path = region_path;
    for (const auto &object : opts.objects) {
        const std::string prefix = object.empty() || object[0] == '/' ?
            object : "/" + object;
        if (path.compare(0, prefix.size(), prefix) == 0 &&
            (path.size() == prefix.size() || path[prefix.size()] == '/')) {
            assigned = object;
            matches++;
        }
    }
    return matches == 1;
}

static SampleValues
sample_values(const Run &run)
{
    SampleValues values;
    append_metric_values(values, "model.", model_metric(run),
        run.opts.measures);
    if (run.opts.accuracy_scope_all) {
        std::map<std::string, Metric> object_totals;
        bool object_membership_clear = true;
        for (const auto &object : run.opts.objects) object_totals[object];
        for (const auto &item : run.regions) {
            const std::string prefix = "region:" +
                std::string(item.first->reg_name) + "#" +
                std::to_string(item.first->reg_bit) + ".";
            append_metric_values(values, prefix, item.second,
                run.opts.measures);
            std::string assigned;
            if (assign_object(run.opts, item.first->reg_name, assigned))
                add_metric(object_totals[assigned], item.second);
            else
                object_membership_clear = false;
        }
        if (object_membership_clear) {
            for (const auto &item : object_totals) {
                append_metric_values(values, "object:" + item.first + ".",
                    item.second, run.opts.measures);
            }
        }
    }
    return values;
}

static double
absolute_tolerance(const Options &opts, const std::string &metric)
{
    static const std::map<std::string, std::string> suffix_to_target = {
        {".volume_mm3", "volume"},
        {".mass_g", "mass"},
        {".area_mm2", "area"},
        {".centroid_x_mm", "centroid"},
        {".centroid_y_mm", "centroid"},
        {".centroid_z_mm", "centroid"},
        {".inertia_xx_g_mm2", "moments"},
        {".inertia_yy_g_mm2", "moments"},
        {".inertia_zz_g_mm2", "moments"},
        {".inertia_xy_g_mm2", "moments"},
        {".inertia_xz_g_mm2", "moments"},
        {".inertia_yz_g_mm2", "moments"}};
    for (const auto &entry : suffix_to_target) {
        if (metric.size() >= entry.first.size() &&
            metric.compare(metric.size() - entry.first.size(),
                entry.first.size(), entry.first) == 0) {
            auto target = opts.absolute_error.find(entry.second);
            return target == opts.absolute_error.end() ? 0.0 : target->second;
        }
    }
    return 0.0;
}

static void
set_interval_target(UncertaintyInterval &interval, const Options &opts)
{
    const double relative_tolerance = opts.relative_error > 0.0 ?
        opts.relative_error * fabs(interval.estimate) : 0.0;
    interval.target_tolerance = std::max(relative_tolerance,
        absolute_tolerance(opts, interval.metric));
    interval.target_applies = interval.target_tolerance > 0.0;
    interval.target_met = interval.available && interval.target_applies &&
        interval.half_width <= interval.target_tolerance;
}

static std::vector<UncertaintyInterval>
calculate_intervals(
    const std::vector<SampleValues> &replicates,
    const Options &opts)
{
    std::vector<UncertaintyInterval> intervals;
    if (replicates.size() != UNCERTAINTY_REPLICATES) return intervals;
    for (const auto &item : replicates.front().scalars) {
        UncertaintyInterval interval;
        interval.metric = item.first;
        std::vector<double> estimates;
        estimates.reserve(replicates.size());
        for (const auto &replicate : replicates) {
            auto value = replicate.scalars.find(item.first);
            if (value == replicate.scalars.end() ||
                !std::isfinite(value->second)) {
                estimates.clear();
                break;
            }
            estimates.push_back(value->second);
        }
        if (estimates.size() == replicates.size()) {
            struct analyze_sampling_interval result;
            interval.available = analyze_sampling_interval95(&result,
                estimates.data(), estimates.size()) == ANALYZE_OK;
            if (interval.available) {
                interval.estimate = result.estimate;
                interval.half_width = result.half_width;
            }
        }
        set_interval_target(interval, opts);
        intervals.push_back(interval);
    }
    for (const auto &item : replicates.front().ratios) {
        UncertaintyInterval interval;
        interval.metric = item.first;
        std::vector<double> numerators;
        std::vector<double> denominators;
        numerators.reserve(replicates.size());
        denominators.reserve(replicates.size());
        for (const auto &replicate : replicates) {
            auto value = replicate.ratios.find(item.first);
            if (value == replicate.ratios.end()) {
                numerators.clear();
                denominators.clear();
                break;
            }
            numerators.push_back(value->second.first);
            denominators.push_back(value->second.second);
        }
        if (numerators.size() == replicates.size()) {
            struct analyze_sampling_interval result;
            interval.available = analyze_sampling_ratio_interval95(&result,
                numerators.data(), denominators.data(), numerators.size()) ==
                ANALYZE_OK;
            if (interval.available) {
                interval.estimate = result.estimate;
                interval.half_width = result.half_width;
            }
        }
        set_interval_target(interval, opts);
        intervals.push_back(interval);
    }
    return intervals;
}

class CroftonSessionGuard
{
    public:
        explicit CroftonSessionGuard(struct rt_i *rtip) :
            session(rt_crofton_session_create(rtip))
        {
        }

        ~CroftonSessionGuard()
        {
            rt_crofton_session_destroy(session);
        }

        CroftonSessionGuard(const CroftonSessionGuard &) = delete;
        CroftonSessionGuard &operator=(const CroftonSessionGuard &) = delete;

        struct rt_crofton_session *get() const
        {
            return session;
        }

    private:
        struct rt_crofton_session *session;
};

static bool
run_crofton_replicates(Run &run, size_t rays_per_replicate,
    uint64_t first_stream, struct rt_crofton_session *session,
    std::vector<SampleValues> &values,
    Metric &model_sum,
    std::map<const struct region *, Metric> &region_sums)
{
    for (size_t replicate = 0; replicate < UNCERTAINTY_REPLICATES;
         replicate++) {
        reset_measurements(run);
        if (!run_crofton_single(run, rays_per_replicate,
                first_stream + replicate, session, 0.0))
            return false;
        const Metric total = model_metric(run);
        add_metric(model_sum, total);
        for (const auto &item : run.regions)
            add_metric(region_sums[item.first], item.second);
        values.push_back(sample_values(run));
    }
    return true;
}

static bool
run_crofton_uncertainty(Run &run)
{
    CroftonSessionGuard session(run.rtip);
    if (!session.get()) {
        bu_vls_printf(run.result,
            "Cannot create Crofton worker session.\n");
        return false;
    }

    const bool accuracy_requested = run.opts.relative_error > 0.0 ||
        !run.opts.absolute_error.empty();
    const bool deadline_without_ray_limit = accuracy_requested &&
        run.opts.time_ms > 0.0 && !run.opts.rays_set;
    const size_t total_budget = deadline_without_ray_limit ?
        SIZE_MAX : run.opts.rays;
    size_t pilot_per_replicate = 0;
    size_t production_per_replicate =
        total_budget / UNCERTAINTY_REPLICATES;
    uint64_t production_stream = 0;

    if (accuracy_requested) {
        const size_t pilot_budget = deadline_without_ray_limit ?
            ADAPTIVE_PILOT_RAYS :
            std::min(ADAPTIVE_PILOT_RAYS, total_budget / 4);
        pilot_per_replicate = std::max(static_cast<size_t>(1),
            pilot_budget / UNCERTAINTY_REPLICATES);
        std::vector<SampleValues> pilot_values;
        Metric ignored_model;
        std::map<const struct region *, Metric> ignored_regions;
        const int64_t pilot_start_us = bu_gettime();
        if (!run_crofton_replicates(run, pilot_per_replicate, 0,
                session.get(), pilot_values, ignored_model, ignored_regions))
            return false;
        run.pilot_time_ms = elapsed_ms(pilot_start_us);
        run.pilot_rays =
            pilot_per_replicate * UNCERTAINTY_REPLICATES;
        if (run.pilot_time_ms > 0.0)
            run.pilot_throughput_rays_per_second =
                1000.0 * run.pilot_rays / run.pilot_time_ms;
        const std::vector<UncertaintyInterval> pilot_intervals =
            calculate_intervals(pilot_values, run.opts);
        double required_scale = 1.0;
        for (const auto &interval : pilot_intervals) {
            if (!interval.available || !interval.target_applies) {
                required_scale = std::numeric_limits<double>::infinity();
                break;
            }
            const double ratio =
                interval.half_width / interval.target_tolerance;
            /*
             * The squared ratio is a root-n planning heuristic, not a
             * convergence claim, and the safety factor is an empirical
             * engineering margin.  QMC error need not follow a stable power
             * law.  Only the fresh independent production streams below
             * determine the reported status.  That separation follows
             * A. B. Owen's replication-based error-estimation guidance:
             * "Error estimation for quasi-Monte Carlo" (2025),
             * https://arxiv.org/abs/2501.00150v3, section 5.
             */
            required_scale = std::max(required_scale, ratio * ratio);
        }
        const size_t maximum_per_replicate =
            SIZE_MAX / UNCERTAINTY_REPLICATES;
        const double required = ceil(pilot_per_replicate * required_scale *
            ADAPTIVE_PLANNING_SAFETY_FACTOR);
        const size_t forecast_per_replicate =
            !std::isfinite(required) ||
            required >= static_cast<double>(maximum_per_replicate) ?
            maximum_per_replicate : static_cast<size_t>(required);
        production_per_replicate = forecast_per_replicate;
        run.forecast_production_rays = forecast_per_replicate *
            UNCERTAINTY_REPLICATES;

        if (!deadline_without_ray_limit) {
            const size_t remaining_budget = total_budget - run.pilot_rays;
            const size_t available_per_replicate =
                remaining_budget / UNCERTAINTY_REPLICATES;
            if (production_per_replicate > available_per_replicate) {
                production_per_replicate = available_per_replicate;
                run.ray_limit_limited = true;
            }
        }
        if (run.deadline_us) {
            const double deadline_capacity =
                run.pilot_throughput_rays_per_second *
                remaining_deadline_ms(run) *
                ADAPTIVE_DEADLINE_UTILIZATION /
                (1000.0 * UNCERTAINTY_REPLICATES);
            const size_t deadline_per_replicate =
                !std::isfinite(deadline_capacity) ||
                deadline_capacity >=
                    static_cast<double>(maximum_per_replicate) ?
                maximum_per_replicate :
                static_cast<size_t>(std::max(0.0,
                    floor(deadline_capacity)));
            if (production_per_replicate > deadline_per_replicate) {
                production_per_replicate = deadline_per_replicate;
                run.deadline_limited = true;
            }
        }
        production_stream = UNCERTAINTY_REPLICATES;
    }
    run.planned_production_rays = production_per_replicate *
        UNCERTAINTY_REPLICATES;

    if (!production_per_replicate) {
        bu_vls_printf(run.result,
            "Time or ray budget is too small for uncertainty production samples.\n");
        return false;
    }

    std::vector<SampleValues> production_values;
    Metric model_sum;
    std::map<const struct region *, Metric> region_sums;
    const int64_t production_start_us = bu_gettime();
    if (!run_crofton_replicates(run, production_per_replicate,
            production_stream, session.get(), production_values, model_sum,
            region_sums))
        return false;
    run.production_time_ms = elapsed_ms(production_start_us);
    run.production_rays =
        production_per_replicate * UNCERTAINTY_REPLICATES;
    if (!deadline_without_ray_limit)
        run.unused_rays = total_budget - run.pilot_rays -
            run.production_rays;
    run.deadline_hit = run.deadline_us &&
        remaining_deadline_ms(run) <= 0.0;
    run.uncertainty =
        calculate_intervals(production_values, run.opts);

    reset_measurements(run);
    divide_metric(model_sum, static_cast<double>(UNCERTAINTY_REPLICATES));
    run.model_volume = model_sum.volume;
    run.model_mass = model_sum.mass;
    run.model_area = model_sum.area;
    for (auto &item : run.regions) {
        Metric mean = region_sums[item.first];
        divide_metric(mean, static_cast<double>(UNCERTAINTY_REPLICATES));
        item.second = mean;
    }

    if (accuracy_requested) {
        bool available = !run.uncertainty.empty();
        bool target_met = available;
        bool target_applies = false;
        for (const auto &interval : run.uncertainty) {
            available = available && interval.available;
            if (interval.target_applies) {
                target_applies = true;
                target_met = target_met && interval.target_met;
            }
        }
        if (!available || !target_applies)
            run.accuracy_status = "insufficient_evidence";
        else if (target_met)
            run.accuracy_status = "target_met";
        else if (run.deadline_hit || run.deadline_limited)
            run.accuracy_status = "time_limit";
        else
            run.accuracy_status = "target_not_met";
    } else {
        run.accuracy_status = "not_requested";
    }
    return true;
}

static bool
run_crofton(Run &run)
{
    if (run.opts.checks == OVERLAPS && run.opts.measures == 0 &&
        ZERO(run.opts.time_ms)) {
        const std::vector<OverlapCandidate> candidates =
            find_overlap_candidates(run.rtip, run.opts.tolerance);
        return run_crofton_targeted_overlaps(run, candidates);
    }
    if (run.opts.uncertainty)
        return run_crofton_uncertainty(run);
    const bool ray_count_is_limit = run.opts.rays_set ||
        (run.opts.time_ms <= 0.0 && run.opts.stability <= 0.0);
    double sampling_time_ms = -1.0;
    if (run.deadline_us) {
        sampling_time_ms = remaining_deadline_ms(run);
        if (sampling_time_ms <= 0.0) {
            bu_vls_printf(run.result,
                "Crofton deadline expired during setup.\n");
            return false;
        }
    }
    return run_crofton_single(run,
        ray_count_is_limit ? run.opts.rays : 0, 0, NULL, sampling_time_ms);
}

static bool
has_active_matrix(struct db_i *dbip, const std::string &name)
{
    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    if (db_string_to_path(&path, dbip, name.c_str()) != 0) {
	db_free_full_path(&path);
	return true;
    }
    mat_t previous, current;
    MAT_IDN(previous);
    bool active = false;
    for (size_t depth = 1; depth < path.fp_len; depth++) {
	/* A singular prefix can hide a later arc transform in the product. */
	if (ZERO(bn_mat_determinant(previous))) { active = true; break; }
	if (!db_path_to_mat(dbip, &path, current, static_cast<int>(depth))) { active = true; break; }
	for (int k = 0; k < 16; k++) {
	    if (!EQUAL(current[k], previous[k])) { active = true; break; }
	}
	if (active) break;
	MAT_COPY(previous, current);
    }
    db_free_full_path(&path);
    return active;
}

static std::string
summary_path(struct db_i *dbip, const std::string &full,
	     const std::set<std::string> &all_paths)
{
    if (has_active_matrix(dbip, full)) return full;
    size_t at = full.rfind('/');
    while (at != std::string::npos) {
	const std::string suffix = full.substr(at+1);
	size_t matches = 0;
	for (const auto &other : all_paths) {
	    if (other.size() >= suffix.size() &&
		other.compare(other.size()-suffix.size(), suffix.size(), suffix) == 0 &&
		(other.size() == suffix.size() ||
		 other[other.size()-suffix.size()-1] == '/'))
		matches++;
	}
	if (matches == 1) return suffix;
	if (at == 0) break;
	at = full.rfind('/', at-1);
    }
    return full;
}

static void
add_metric(Metric &to, const Metric &from)
{
    to.volume += from.volume;
    to.mass += from.mass;
    to.area += from.area;
    for (int i = 0; i < 3; i++) to.first[i] += from.first[i];
    for (int i = 0; i < 6; i++) to.second[i] += from.second[i];
    to.hits += from.hits;
}

static nlohmann::json
metric_json(const Metric &m, int measures)
{
    nlohmann::json j;
    if (measures & VOLUME) j["volume_mm3"] = m.volume;
    if (measures & MASS) j["mass_g"] = m.mass;
    if (measures & AREA) j["area_mm2"] = m.area;
    if ((measures & CENTROID) && m.mass > 0.0)
	j["centroid_mm"] = {m.first[0]/m.mass, m.first[1]/m.mass, m.first[2]/m.mass};
    if (measures & MOMENTS)
	j["inertia_tensor_g_mm2"] = {
	    {m.second[0], m.second[3], m.second[4]},
	    {m.second[3], m.second[1], m.second[5]},
	    {m.second[4], m.second[5], m.second[2]}};
    j["observed_segments"] = m.hits;
    return j;
}

static bool
write_json_file(struct bu_vls *out, const std::string &path,
                const nlohmann::json &contents, const char *description)
{
    const std::string temporary = path + REPORT_TEMP_MARKER +
        std::to_string(bu_pid()) + "-" + std::to_string(bu_gettime());
    std::ofstream stream(temporary.c_str(), std::ios::out | std::ios::trunc);
    if (!stream) {
        bu_vls_printf(out, "Cannot open temporary %s output for %s\n",
            description, path.c_str());
        return false;
    }
    stream << contents.dump(2) << '\n';
    stream.flush();
    const bool write_ok = stream.good();
    stream.close();
    if (!write_ok || !stream.good()) {
        bu_vls_printf(out, "Cannot write %s output %s\n",
            description, path.c_str());
        bu_file_delete(temporary.c_str());
        return false;
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        bu_vls_printf(out, "Cannot replace %s output %s: %s\n",
            description, path.c_str(), strerror(errno));
        bu_file_delete(temporary.c_str());
        return false;
    }
    return true;
}

static const char *
crofton_stop_reason_name(enum rt_crofton_stop_reason reason)
{
    switch (reason) {
        case RT_CROFTON_STOP_RAYS:
            return "ray_limit";
        case RT_CROFTON_STOP_STABILITY:
            return "stable";
        case RT_CROFTON_STOP_TIME:
            return "time_limit";
        default:
            return "none";
    }
}

static const char *
grid_stop_reason_name(GridStopReason reason)
{
    switch (reason) {
	case GRID_STOP_REFINEMENT_LIMIT:
	    return "refinement_limit";
	case GRID_STOP_STABILITY:
	    return "stable";
	case GRID_STOP_TIME:
	    return "time_limit";
	case GRID_STOP_MAX_LEVELS:
	    return "maximum_refinement_levels";
	default:
	    return "none";
    }
}

static const char *
sampler_name(Sampler sampler)
{
    switch (sampler) {
	case GRID_ROTATED:
	    return "grid-rotated";
	case CROFTON:
	    return "crofton";
	default:
	    return "grid";
    }
}

static bool
report(Run &run)
{
    struct bu_vls *out = run.result;
    if (run.analysis_start_us)
        run.elapsed_time_ms = elapsed_ms(run.analysis_start_us);
    if (run.deadline_us) {
        run.deadline_hit = remaining_deadline_ms(run) <= 0.0;
        if (run.deadline_hit && run.accuracy_status == "target_not_met")
            run.accuracy_status = "time_limit";
    }
    const bool crofton = run.opts.sampler == CROFTON;
    const enum rt_crofton_sequence resolved_sequence = crofton ?
        rt_crofton_resolve_sequence(run.opts.sequence) :
        RT_CROFTON_SEQUENCE_DEFAULT;
    const char *sequence_name =
        resolved_sequence == RT_CROFTON_SEQUENCE_QMC ? "qmc" :
        (resolved_sequence == RT_CROFTON_SEQUENCE_RANDOM ? "random" : "none");
    Metric total;
    for (const auto &item : run.regions) add_metric(total, item.second);
    total.volume = run.model_volume;
    total.mass = run.model_mass;
    total.area = run.model_area;
    bu_vls_printf(out, "gqa analysis (%s%s), %zu rays\n",
	sampler_name(run.opts.sampler),
	crofton ? (resolved_sequence == RT_CROFTON_SEQUENCE_QMC ?
	    ", qmc" : ", random") : "",
	run.ray_id);
    if (run.opts.measures & VOLUME) bu_vls_printf(out, "Volume: %.9g mm^3\n", run.model_volume);
    if (run.opts.measures & MASS) bu_vls_printf(out, "Mass: %.9g g\n", run.model_mass);
    if (run.opts.measures & AREA) bu_vls_printf(out, "Area: %.9g mm^2\n", run.model_area);
    if (run.opts.measures & CENTROID) {
	if (total.mass > 0.0)
	    bu_vls_printf(out, "Centroid: %.9g %.9g %.9g mm\n",
		total.first[0]/total.mass, total.first[1]/total.mass,
		total.first[2]/total.mass);
	else
	    bu_vls_printf(out, "Centroid: undefined (zero mass)\n");
    }
    if (run.opts.measures & MOMENTS)
	bu_vls_printf(out, "Inertia tensor (g mm^2): [%.9g %.9g %.9g; %.9g %.9g %.9g; %.9g %.9g %.9g]\n",
	    total.second[0], total.second[3], total.second[4],
	    total.second[3], total.second[1], total.second[5],
	    total.second[4], total.second[5], total.second[2]);
    if (run.opts.uncertainty) {
        bu_vls_printf(out,
            "Approximate 95%% sampling intervals (%zu independent replicates):\n",
            UNCERTAINTY_REPLICATES);
        for (const auto &interval : run.uncertainty) {
            if (interval.metric.compare(0, 6, "model.") != 0) continue;
            if (interval.available)
                bu_vls_printf(out, "  %s: %.9g +/- %.9g\n",
                    interval.metric.c_str() + 6, interval.estimate,
                    interval.half_width);
            else
                bu_vls_printf(out, "  %s: unavailable\n",
                    interval.metric.c_str() + 6);
        }
        if (run.opts.relative_error > 0.0 ||
            !run.opts.absolute_error.empty())
            bu_vls_printf(out, "Accuracy target: %s\n",
                run.accuracy_status.c_str());
        if (run.pilot_rays) {
            bu_vls_printf(out,
                "Adaptive plan: %zu pilot rays in %.3g ms, %zu forecast rays, "
                "%zu production rays in %.3g ms (safety factor %.3g)\n",
                run.pilot_rays, run.pilot_time_ms,
                run.forecast_production_rays, run.production_rays,
                run.production_time_ms, ADAPTIVE_PLANNING_SAFETY_FACTOR);
        }
        if (run.deadline_us) {
            bu_vls_printf(out,
                "Deadline: %.3g ms, elapsed %.3g ms%s%s\n",
                run.opts.time_ms, run.elapsed_time_ms,
                run.deadline_limited ? ", production limited" : "",
                run.deadline_hit ? ", reached" : "");
        }
    }
    if (run.grid_passes.size() > 1) {
        const GridPass &previous =
            run.grid_passes[run.grid_passes.size() - 2];
        const GridPass &current = run.grid_passes.back();
        if (run.opts.measures & VOLUME)
            bu_vls_printf(out, "Final refinement volume change: %.9g mm^3\n",
                fabs(current.volume - previous.volume));
        if (run.opts.measures & MASS)
            bu_vls_printf(out, "Final refinement mass change: %.9g g\n",
                fabs(current.mass - previous.mass));
        if (run.opts.measures & AREA)
            bu_vls_printf(out, "Final refinement area change: %.9g mm^2\n",
                fabs(current.area - previous.area));
    }
    bu_vls_printf(out, "Regions: %zu\n", run.regions.size());
    if (crofton) {
        if (run.targeted_overlap) {
            bu_vls_printf(out, "Overlap candidate boxes: %zu\n", run.overlap_candidate_count);
            size_t sampled_candidates = 0;
            for (const auto &candidate : run.candidate_results)
                if (candidate.fired_rays) sampled_candidates++;
            bu_vls_printf(out, "Overlap candidate boxes sampled: %zu\n",
                sampled_candidates);
        }
	if (run.observed_stability_evaluated)
	    bu_vls_printf(out, "Observed stability: %.9g mm\n",
		run.observed_stability);
	else if (run.opts.stability > 0.0)
	    bu_vls_printf(out, "Observed stability: unavailable\n");
	if (!run.opts.uncertainty && run.opts.stability > 0.0)
	    bu_vls_printf(out, "Stability target: %.9g mm%s\n",
		run.opts.stability,
		run.opts.stability_defaulted ? " (automatic)" : "");
	if (!run.opts.uncertainty &&
	    run.crofton_stop_reason != RT_CROFTON_STOP_NONE)
	    bu_vls_printf(out, "Stopping condition: %s\n",
		crofton_stop_reason_name(run.crofton_stop_reason));
	if (!run.opts.uncertainty && run.deadline_us)
	    bu_vls_printf(out, "Deadline: %.3g ms, elapsed %.3g ms%s\n",
		run.opts.time_ms, run.elapsed_time_ms,
		run.deadline_hit ? ", reached" : "");
	if (run.invalid_partitions)
	    bu_vls_printf(out, "Invalid partitions rejected: %zu\n",
		run.invalid_partitions);
    } else if (!run.grid_passes.empty()) {
	const GridPass &final_pass = run.grid_passes.back();
	bu_vls_printf(out,
	    "Grid view volumes: %.9g %.9g %.9g mm^3\n",
	    final_pass.view_volume[0], final_pass.view_volume[1],
	    final_pass.view_volume[2]);
	bu_vls_printf(out, "Grid directional spread: %.9g mm\n",
	    final_pass.directional_spread);
	if (run.opts.sampler == GRID_ROTATED)
	    bu_vls_printf(out, "Grid orientation: azimuth %.9g deg, elevation %.9g deg\n",
		run.opts.azimuth_degrees, run.opts.elevation_degrees);
	if (run.observed_stability_evaluated)
	    bu_vls_printf(out, "Observed stability: %.9g mm\n",
		run.observed_stability);
	else if (run.opts.stability > 0.0)
	    bu_vls_printf(out, "Observed stability: unavailable\n");
	if (run.opts.stability > 0.0)
	    bu_vls_printf(out, "Stability target: %.9g mm%s\n",
		run.opts.stability,
		run.opts.stability_defaulted ? " (automatic)" : "");
	bu_vls_printf(out, "Stopping condition: %s\n",
	    grid_stop_reason_name(run.grid_stop_reason));
	if (run.deadline_us)
	    bu_vls_printf(out, "Deadline: %.3g ms, elapsed %.3g ms%s%s\n",
		run.opts.time_ms, run.elapsed_time_ms,
		run.deadline_limited ? ", final refinement limited" : "",
		run.deadline_hit ? ", reached" : "");
	if (run.discarded_grid_rays)
	    bu_vls_printf(out, "Incomplete refinement rays discarded: %zu\n",
		run.discarded_grid_rays);
    }
    if (!run.bad_materials.empty()) {
	bu_vls_printf(out, "Missing density or invalid LOS for:");
	for (const auto &name : run.bad_materials) bu_vls_printf(out, " %s", name.c_str());
	bu_vls_printf(out, "\n");
	return false;
    }
    std::vector<const struct region *> region_order;
    for (const auto &item : run.regions) region_order.push_back(item.first);
    std::sort(region_order.begin(), region_order.end(),
	[](const struct region *a, const struct region *b) {
	    const int cmp = bu_strcmp(a->reg_name, b->reg_name);
	    return cmp < 0 || (cmp == 0 && a->reg_bit < b->reg_bit);
	});
    std::set<std::string> paths;
    std::map<std::string, size_t> path_counts;
    for (const auto &item : run.regions) {
	paths.insert(item.first->reg_name);
	path_counts[item.first->reg_name]++;
    }
    for (const auto &issue : run.issues) {
	paths.insert(issue.first);
	if (!issue.second.empty()) paths.insert(issue.second);
    }
    for (const auto &group : run.groups) {
	const Issue &example = run.issues[group.second.second];
	const std::string first = path_counts[example.first] > 1 ?
	    example.first + " [region " + std::to_string(example.first_bit) + "]" :
	    summary_path(run.dbip, example.first, paths);
	const std::string second = example.second.empty() ? "" :
	    (path_counts[example.second] > 1 ?
		example.second + " [region " + std::to_string(example.second_bit) + "]" :
		summary_path(run.dbip, example.second, paths));
	const char *extent = (example.type == "gap" || example.type == "internal-void") ? "gap width" :
	    (example.type == "adjacent-air" ? "separation" :
	    (example.type == "exposed-air" ? "air path length" : "overlap depth"));
	const char *join = example.type == "gap" ? " -> " : " and ";
	bu_vls_printf(out, "%s: %s%s%s (%zu observations, largest %s %.6g mm; e.g. %.6g %.6g %.6g)\n",
	    example.type.c_str(), first.c_str(), second.empty() ? "" : join,
	    second.c_str(), group.second.first, extent, example.depth,
	    example.start[0], example.start[1], example.start[2]);
    }
    if (run.opts.json_path.empty()) {
        if (!run.opts.invalid_path.empty()) {
            nlohmann::json invalid;
            invalid["schema_version"] = 2;
            invalid["model"] = run.opts.objects;
            invalid["rays"] = run.invalid_rays;
            if (!write_json_file(out, run.opts.invalid_path, invalid,
                    "invalid-ray"))
                return false;
        }
        return true;
    }
    nlohmann::json j;
    j["schema_version"] = 2;
    j["sampler"] = sampler_name(run.opts.sampler);
    j["sequence"] = sequence_name;
    j["ray_count"] = run.ray_id;
    j["objects"] = run.opts.objects;
    const bool unlimited_rays = crofton && !run.opts.rays_set &&
        run.opts.time_ms > 0.0 &&
        (!run.opts.uncertainty || run.opts.relative_error > 0.0 ||
            !run.opts.absolute_error.empty());
    j["settings"] = {{"rays", !crofton || unlimited_rays ?
	    nlohmann::json(nullptr) : nlohmann::json(run.opts.rays)},
        {"spacing_mm", run.opts.spacing},
	{"spacing_defaulted", run.opts.spacing_defaulted},
	{"tolerance_mm", run.opts.tolerance},
	{"refine_levels", run.opts.refine_levels},
	{"refine_levels_set", run.opts.refine_set},
	{"seed", run.opts.seed},
	{"azimuth_degrees", run.opts.sampler == GRID_ROTATED ?
	    nlohmann::json(run.opts.azimuth_degrees) : nlohmann::json(nullptr)},
	{"elevation_degrees", run.opts.sampler == GRID_ROTATED ?
	    nlohmann::json(run.opts.elevation_degrees) : nlohmann::json(nullptr)},
	{"targeted_overlap", run.targeted_overlap},
	{"overlap_candidate_count", run.overlap_candidate_count},
	{"stability_mm", run.opts.stability},
	{"stability_defaulted", run.opts.stability_defaulted},
	{"time_ms", run.opts.time_ms},
	{"time_defaulted", run.opts.time_defaulted},
	{"stopping_condition", run.opts.uncertainty ?
	    "none" :
	    (crofton ? crofton_stop_reason_name(run.crofton_stop_reason) :
		grid_stop_reason_name(run.grid_stop_reason))},
	{"observed_stability_mm", run.observed_stability_evaluated ?
	    nlohmann::json(run.observed_stability) : nlohmann::json(nullptr)},
	{"invalid_partition_count", run.invalid_partitions},
	{"discarded_grid_rays", run.discarded_grid_rays}};
    j["grid_view_directions"] = nlohmann::json::array();
    if (!crofton) {
	for (const auto &direction : run.grid_directions)
	    j["grid_view_directions"].push_back(direction);
    }
    j["sampling_uncertainty"] = {
        {"enabled", run.opts.uncertainty},
        {"confidence", REPORT_CONFIDENCE},
        {"coverage", "pointwise"},
        {"replicates", run.opts.uncertainty ?
            UNCERTAINTY_REPLICATES : 0},
        {"relative_error_target", run.opts.relative_error > 0.0 ?
            nlohmann::json(run.opts.relative_error) : nlohmann::json(nullptr)},
        {"absolute_error_targets", run.opts.absolute_error},
        {"accuracy_scope", run.opts.accuracy_scope_all ? "all" : "model"},
        {"accuracy_status", run.opts.uncertainty ?
            run.accuracy_status : "not_requested"},
        {"pilot_rays", run.pilot_rays},
        {"pilot_time_ms", run.pilot_time_ms},
        {"pilot_throughput_rays_per_second",
            run.pilot_throughput_rays_per_second > 0.0 ?
            nlohmann::json(run.pilot_throughput_rays_per_second) :
            nlohmann::json(nullptr)},
        {"forecast_production_rays", run.forecast_production_rays},
        {"planned_production_rays", run.planned_production_rays},
        {"production_rays", run.production_rays},
        {"production_time_ms", run.production_time_ms},
        {"unused_rays", run.unused_rays},
        {"planning_safety_factor", run.pilot_rays ?
            nlohmann::json(ADAPTIVE_PLANNING_SAFETY_FACTOR) :
            nlohmann::json(nullptr)},
        {"deadline_ms", run.deadline_us ?
            nlohmann::json(run.opts.time_ms) : nlohmann::json(nullptr)},
        {"elapsed_time_ms", run.elapsed_time_ms},
        {"deadline_limited", run.deadline_limited},
        {"deadline_hit", run.deadline_hit},
        {"ray_limit_limited", run.ray_limit_limited},
        {"intervals", nlohmann::json::array()}};
    for (const auto &interval : run.uncertainty) {
        j["sampling_uncertainty"]["intervals"].push_back({
            {"metric", interval.metric},
            {"estimate", interval.available ?
                nlohmann::json(interval.estimate) : nlohmann::json(nullptr)},
            {"half_width", interval.available ?
                nlohmann::json(interval.half_width) :
                nlohmann::json(nullptr)},
            {"target_tolerance", interval.target_applies ?
                nlohmann::json(interval.target_tolerance) :
                nlohmann::json(nullptr)},
            {"target_met", interval.target_applies ?
                nlohmann::json(interval.target_met) :
                nlohmann::json(nullptr)}});
    }
    j["grid_refinement"] = nlohmann::json::array();
    for (size_t i = 0; i < run.grid_passes.size(); i++) {
        const GridPass &pass = run.grid_passes[i];
        nlohmann::json entry = {
            {"level", i}, {"spacing_mm", pass.spacing},
            {"ray_count", pass.ray_count},
	    {"elapsed_time_ms", pass.elapsed_time_ms},
	    {"view_volumes_mm3", pass.view_volume},
	    {"directional_spread_mm", pass.directional_spread},
	    {"phase_u", pass.phase_u}, {"phase_v", pass.phase_v},
	    {"refinement_stability_mm", pass.refinement_evaluated ?
		nlohmann::json(pass.refinement_stability) :
		nlohmann::json(nullptr)},
	    {"stable_intervals", pass.stable_intervals}};
        if (run.opts.measures & VOLUME) entry["volume_mm3"] = pass.volume;
        if (run.opts.measures & MASS) entry["mass_g"] = pass.mass;
        if (run.opts.measures & AREA) entry["area_mm2"] = pass.area;
        if (i) {
            const GridPass &previous = run.grid_passes[i - 1];
            if (run.opts.measures & VOLUME)
                entry["volume_change_mm3"] =
                    fabs(pass.volume - previous.volume);
            if (run.opts.measures & MASS)
                entry["mass_change_g"] = fabs(pass.mass - previous.mass);
            if (run.opts.measures & AREA)
                entry["area_change_mm2"] = fabs(pass.area - previous.area);
        }
        j["grid_refinement"].push_back(entry);
    }
    j["measurement_conventions"] = {
	{"modeled_air", run.opts.include_air ? "included" : "excluded"},
	{"surface_area", "region partition boundaries"},
	{"inertia_reference", "model origin"}};
    j["overlap_candidates"] = nlohmann::json::array();
    for (const auto &candidate : run.candidate_results) {
	nlohmann::json entry = {
	    {"paths", {candidate.first, candidate.second}},
	    {"region_bits", {candidate.first_bit, candidate.second_bit}},
	    {"allocated_rays", candidate.allocated_rays},
	    {"fired_rays", candidate.fired_rays},
	    {"overlap_events", candidate.overlap_events},
	    {"status", candidate.fired_rays ? "sampled" : "unsampled"},
	    {"stability_mm", candidate.stability_evaluated ?
		nlohmann::json(candidate.stability_mm) : nlohmann::json(nullptr)},
	    {"stability_reached", candidate.stability_reached},
	    {"calibration_reached", candidate.calibration_reached},
	    {"box_volume_error", candidate.fired_rays ?
		nlohmann::json(candidate.volume_error) : nlohmann::json(nullptr)},
	    {"box_area_error", candidate.fired_rays ?
		nlohmann::json(candidate.area_error) : nlohmann::json(nullptr)},
	    {"box_direction_ray_counts", candidate.direction_rays},
	    {"event_probability_upper_bound_95",
		candidate.event_probability_bound_available ?
		nlohmann::json(candidate.event_probability_upper_bound) :
		nlohmann::json(nullptr)}};
	j["overlap_candidates"].push_back(entry);
    }
    j["model"] = metric_json(total, run.opts.measures);
    j["object_results"] = nlohmann::json::array();
    std::map<std::string, Metric> object_totals;
    bool object_membership_clear = true;
    for (const auto &object : run.opts.objects) object_totals[object];
    for (const auto &item : run.regions) {
	std::string assigned;
	if (assign_object(run.opts, item.first->reg_name, assigned))
	    add_metric(object_totals[assigned], item.second);
	else object_membership_clear = false;
    }
    if (object_membership_clear) {
	for (const auto &item : object_totals) {
	    nlohmann::json entry = metric_json(item.second, run.opts.measures);
	    entry["object"] = item.first;
	    j["object_results"].push_back(entry);
	}
    } else {
	j["object_rollup_status"] = "ambiguous membership";
    }
    j["regions"] = nlohmann::json::array();
    for (const struct region *region : region_order) {
	nlohmann::json entry = metric_json(run.regions.at(region), run.opts.measures);
	entry["path"] = region->reg_name;
	entry["instance_number"] = region->reg_instnum;
	entry["region_bit"] = region->reg_bit;
	j["regions"].push_back(entry);
    }
    j["observations"] = nlohmann::json::array();
    for (const auto &issue : run.issues) {
	j["observations"].push_back({{"type", issue.type},
	    {"paths", {issue.first, issue.second}},
	    {"instance_numbers", {issue.first_instance, issue.second_instance}},
	    {"region_bits", {issue.first_bit, issue.second_bit}},
	    {"depth_mm", issue.depth},
	    {"start_point_mm", issue.start}, {"end_point_mm", issue.end},
            {"ray_origin_mm", issue.ray_origin},
	    {"ray_direction", issue.ray_direction}, {"ray_id", issue.ray_id}});
    }
    if (!write_json_file(out, run.opts.json_path, j, "JSON"))
        return false;
    if (!run.opts.invalid_path.empty()) {
        nlohmann::json invalid;
        invalid["schema_version"] = 2;
        invalid["model"] = run.opts.objects;
        invalid["rays"] = run.invalid_rays;
        if (!write_json_file(out, run.opts.invalid_path, invalid,
                "invalid-ray")) {
            bu_vls_printf(out,
                "JSON output was published before invalid-ray output failed.\n");
            return false;
        }
    }
    return true;
}

} // namespace

extern "C" int
analyze_gqa_model_argument(int argc, const char *argv[])
{
    if (!argv || argc < 3 || !argv[1] ||
	bu_strcmp(argv[1], "--analyze"))
	return -1;
    std::vector<const char *> args;
    for (int i = 2; i < argc; i++) {
	if (!argv[i]) return -1;
	args.push_back(argv[i]);
    }
    Options opts;
    AnalysisOptionState state;
    state.opts = &opts;
    if (parse_analysis_arguments(NULL, args, state) < 1 ||
	is_unknown_analysis_option(args[0]))
	return -1;
    for (int i = 2; i < argc; i++)
	if (argv[i] == args[0]) return i;
    return -1;
}

extern "C" int
analyze_gqa(const struct analyze_gqa_context *context, int argc,
    const char *argv[])
{
    if (!context || !context->dbip || !context->result)
	return ANALYZE_ERROR;
    bu_vls_trunc(context->result, 0);
    Run run;
    run.dbip = context->dbip;
    run.result = context->result;
    if (!parse_options(run.dbip, run.result, argc, argv, run.opts))
	return ANALYZE_ERROR;
    const bool accuracy_requested = run.opts.relative_error > 0.0 ||
        !run.opts.absolute_error.empty();
    const bool crofton = run.opts.sampler == CROFTON;
    const bool resource_control_set = crofton ? run.opts.rays_set :
	run.opts.refine_set;
    const bool default_control_eligible =
        run.opts.measures != 0 && run.opts.checks == 0 &&
	!resource_control_set && (!run.opts.uncertainty || accuracy_requested);
    if (default_control_eligible && !run.opts.time_set) {
	run.opts.time_ms = crofton ? DEFAULT_CROFTON_TIME_MS :
	    DEFAULT_GRID_TIME_MS;
	run.opts.time_defaulted = true;
    }
    if (default_control_eligible && !run.opts.uncertainty &&
        !run.opts.stability_set)
        run.opts.stability_defaulted = true;
    run.analysis_start_us = bu_gettime();
    run.last_progress_us = run.analysis_start_us;
    run.report_progress = context->report_progress;
    run.progress_data = context->progress_data;
    if (run.opts.time_ms > 0.0) {
        const double duration_us = run.opts.time_ms * 1000.0;
        const int64_t maximum_deadline =
            std::numeric_limits<int64_t>::max();
        run.deadline_us = duration_us >= static_cast<double>(
            maximum_deadline - run.analysis_start_us) ? maximum_deadline :
            run.analysis_start_us + static_cast<int64_t>(duration_us);
    }
    if (run.opts.measures & MASS) {
	char *source = NULL;
	if (!context->load_densities ||
	    context->load_densities(&run.densities, &source,
		run.opts.density_path.empty() ? NULL :
		run.opts.density_path.c_str(), context->density_data) != ANALYZE_OK) {
	    bu_vls_printf(run.result,
		"Mass requires a readable density table.\n");
	    if (source) bu_free(source, "density source");
	    return ANALYZE_ERROR;
	}
	if (source && (paths_conflict(run.opts.json_path, source) ||
	    paths_conflict(run.opts.invalid_path, source))) {
	    bu_vls_printf(run.result,
		"Analysis output cannot replace the density source.\n");
	    bu_free(source, "density source");
	    analyze_densities_destroy(run.densities);
	    return ANALYZE_ERROR;
	}
	if (source) bu_free(source, "density source");
    }
    run.rtip = rt_i_create(run.dbip);
    if (!run.rtip) {
	bu_vls_printf(run.result, "Cannot create raytrace instance.\n");
	if (run.densities) analyze_densities_destroy(run.densities);
	return ANALYZE_ERROR;
    }
    run.rtip->useair = 1;
    bool valid = true;
    for (const auto &name : run.opts.objects) {
	if (rt_gettree(run.rtip, name.c_str()) < 0) {
	    bu_vls_printf(run.result, "Cannot load %s\n", name.c_str());
	    valid = false;
	    break;
	}
    }
    if (valid) {
	rt_prep_parallel(run.rtip, bu_avail_cpus());
	struct soltab *solid;
	bool has_halfspace = false;
	point_t tight_min, tight_max;
	VSETALL(tight_min, MAX_FASTF);
	VSETALL(tight_max, -MAX_FASTF);
	RT_VISIT_ALL_SOLTABS_START(solid, run.rtip) {
	    if (solid->st_id == ID_HALF) has_halfspace = true;
	    VMIN(tight_min, solid->st_min);
	    VMAX(tight_max, solid->st_max);
	} RT_VISIT_ALL_SOLTABS_END;
	VMOVE(run.grid_min, tight_min);
	VMOVE(run.grid_max, tight_max);
	if (has_halfspace) {
	    bu_vls_printf(run.result,
		"Experimental analysis does not support half-space primitives.\n");
	    valid = false;
	}
	if (valid && run.opts.stability_defaulted) {
	    double smallest_span = std::numeric_limits<double>::infinity();
	    for (int axis = 0; axis < 3; axis++) {
		const double span = tight_max[axis] - tight_min[axis];
		if (std::isfinite(span) && span > 0.0)
		    smallest_span = std::min(smallest_span, span);
	    }
	    if (!std::isfinite(smallest_span)) {
		bu_vls_printf(run.result,
		    "Cannot determine a finite model scale for automatic stability.\n");
		valid = false;
	    } else {
		/* BN_TOL_DIST is librt's practical Boolean distance tolerance.
		 * Scale sooner than that fixed floor would dominate a small but
		 * still traceable model's dimensions. */
		run.opts.stability = std::min(
		    AUTOMATIC_TOLERANCE_STABILITY_SCALE * BN_TOL_DIST,
		    AUTOMATIC_SMALL_MODEL_STABILITY_SCALE * smallest_span);
	    }
	}
	if (valid && !crofton && !run.opts.spacing_set) {
	    vect_t diagonal;
	    VSUB2(diagonal, tight_max, tight_min);
	    const double scale_spacing = MAGNITUDE(diagonal) /
		DEFAULT_GRID_SAMPLES_PER_DIAGONAL;
	    if (!std::isfinite(scale_spacing)) {
		bu_vls_printf(run.result,
		    "Cannot determine a finite model scale for grid spacing.\n");
		valid = false;
	    } else {
		run.opts.spacing = std::max(DEFAULT_GRID_SPACING,
		    scale_spacing);
		run.opts.spacing_defaulted = true;
	    }
	}
	if (valid) {
	    struct region *region;
	    for (BU_LIST_FOR (region, region, &run.rtip->HeadRegion))
		run.regions.emplace(region, Metric());
	    if (run.regions.empty()) {
		bu_vls_printf(run.result,
		    "No regions found in selected objects.\n");
		valid = false;
	    } else {
		valid = crofton ? run_crofton(run) : run_grid(run);
	    }
	}
    }
    if (valid) valid = report(run);
    rt_i_destroy(run.rtip);
    if (run.densities) analyze_densities_destroy(run.densities);
    return valid ? ANALYZE_OK : ANALYZE_ERROR;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
