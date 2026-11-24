/*                     G Q A _ S C H E M A . H
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

#ifndef LIBGED_GQA_SCHEMA_H
#define LIBGED_GQA_SCHEMA_H

#include <limits.h>

#include "bu/arg.h"
#include "vmath.h"

struct gqa_args {
    const char *analyses;
    const char *azimuth;
    int debug;
    const char *elevation;
    const char *density_file;
    const char *grid_spacing;
    int overlap_assemblies;
    int required_hits;
    int views;
    const char *plot_prefix;
    int cpus;
    int quiet;
    int region_stats;
    fastf_t samples;
    const char *overlap_tolerance;
    int use_air;
    const char *report_units;
    int verbose;
    const char *volume_tolerance;
    const char *weight_tolerance;
    int help;
};

static const char * const gqa_analysis_candidates[] = {
    "A", "a", "b", "c", "e", "g", "m", "o", "p", "v", "w", NULL
};

static const struct bu_arg_option gqa_options[] = {
    {"A", "analysis", "analysis", "types", "Select analyses",
	BU_ARG_VALUE_STRING, BU_ARG_STORAGE_OFFSET(struct gqa_args, analyses),
	NULL, NULL, NULL, NULL, 0, 0, gqa_analysis_candidates,
	BU_ARG_REQUIRED, NULL, NULL, NULL, BU_ARG_VALUE_RANGE_NONE,
	BU_ARG_SHAPE_SCALAR},
    BU_ARG_VALUE("a", "azimuth", struct gqa_args, azimuth,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "angle", "Set the azimuth angle"),
    BU_ARG_FLAG("d", "debug", struct gqa_args, debug,
	"Enable diagnostic output"),
    BU_ARG_VALUE("e", "elevation", struct gqa_args, elevation,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "angle", "Set the elevation angle"),
    {"f", "density-file", "density-file", "file",
	"Read material densities from this file", BU_ARG_VALUE_FILE,
	BU_ARG_STORAGE_OFFSET(struct gqa_args, density_file), NULL, NULL,
	"ged.file_path", NULL, 0, 0, NULL, BU_ARG_REQUIRED, NULL, NULL, NULL,
	BU_ARG_VALUE_RANGE_NONE, BU_ARG_SHAPE_SCALAR},
    BU_ARG_VALUE("g", "grid-spacing", struct gqa_args, grid_spacing,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "spacing",
	"Set the grid-spacing limit or initial and limiting spacings"),
    BU_ARG_FLAG("G", "overlap-assemblies", struct gqa_args,
	overlap_assemblies, "Create overlap assemblies (not implemented)"),
    BU_ARG_INTEGER_RANGE("n", "required-hits", struct gqa_args,
	required_hits, 1, 0, 1, INT_MAX, "count",
	"Require at least this many ray hits per region"),
    BU_ARG_INTEGER_RANGE("N", "views", struct gqa_args, views,
	1, 1, 1, 3, "count", "Compute the first one, two, or three views"),
    BU_ARG_VALUE("p", "plot-prefix", struct gqa_args, plot_prefix,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "prefix",
	"Write analysis plot files using this prefix"),
    BU_ARG_INTEGER_RANGE("P", "processors", struct gqa_args, cpus,
	1, 1, 1, INT_MAX, "count", "Use at most this many processors"),
    BU_ARG_FLAG("q", "quiet", struct gqa_args, quiet,
	"Suppress reporting for regions that were not hit"),
    BU_ARG_FLAG("r", "region-stats", struct gqa_args, region_stats,
	"Print per-region weight and volume statistics"),
    BU_ARG_VALUE("S", "samples", struct gqa_args, samples,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "count",
	"Set the minimum samples per model axis (greater than 1)"),
    BU_ARG_VALUE("t", "overlap-tolerance", struct gqa_args,
	overlap_tolerance, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "distance",
	"Ignore overlaps shorter than this nonnegative distance"),
    BU_ARG_VALUE("U", "use-air", struct gqa_args, use_air,
	BU_ARG_VALUE_BOOL, BU_ARG_REQUIRED, "boolean",
	"Include or exclude air regions"),
    BU_ARG_VALUE("u", "units", struct gqa_args, report_units,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "length,volume,weight",
	"Set the reporting units"),
    BU_ARG_FLAG("v", "verbose", struct gqa_args, verbose,
	"Report analysis progress"),
    BU_ARG_VALUE("V", "volume-tolerance", struct gqa_args,
	volume_tolerance, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "volume",
	"Set the convergence tolerance for volume calculations"),
    BU_ARG_VALUE("W", "weight-tolerance", struct gqa_args,
	weight_tolerance, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "weight",
	"Set the convergence tolerance for weight calculations"),
    BU_ARG_FLAG("h", "help", struct gqa_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

#ifdef GQA_SCHEMA_STANDALONE
static const struct bu_arg_operand gqa_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_PATH, 1,
	BU_ARG_COUNT_UNLIMITED, "Objects or paths to analyze", "ged.db_path"),
    BU_ARG_OPERAND_NULL
};
#else
static const struct bu_arg_operand gqa_operands[] = {
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_PATH, 1,
	BU_ARG_COUNT_UNLIMITED, "Objects or paths to analyze", "ged.db_path"),
    BU_ARG_OPERAND_NULL
};
#endif

static const struct bu_arg_operand gqa_help_operands[] = {
    BU_ARG_OPERAND("raw_arguments", BU_ARG_VALUE_RAW, 0,
	BU_ARG_COUNT_UNLIMITED, "Arguments ignored when requesting help", NULL),
    BU_ARG_OPERAND_NULL
};
static const char * const gqa_help_option[] = {"help", NULL};
static const struct bu_arg_schema_case gqa_cases[] = {
    BU_ARG_SCHEMA_CASE("help", "Display command help",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, gqa_help_option,
	gqa_help_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("objects", "Analyze database objects",
	gqa_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};
static const struct bu_arg_schema gqa_schema =
    BU_ARG_SCHEMA_CASED("gqa",
	"Analyze geometric overlaps, gaps, volume, and mass", gqa_options,
	BU_ARG_PARSE_OPTIONS_FIRST, gqa_cases);

#endif /* LIBGED_GQA_SCHEMA_H */
