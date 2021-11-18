/*                   C H E C K _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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

#ifndef LIBGED_CHECK_PRIVATE_H
#define LIBGED_CHECK_PRIVATE_H

#include "common.h"
#include "analyze.h"

__BEGIN_DECLS

#define ANALYSIS_VOLUME 1
#define ANALYSIS_CENTROIDS 2
#define ANALYSIS_SURF_AREA 4
#define ANALYSIS_MASS 8
#define ANALYSIS_OVERLAPS 16
#define ANALYSIS_MOMENTS 32
#define ANALYSIS_BOX 64
#define ANALYSIS_GAP 128
#define ANALYSIS_EXP_AIR 256 /* exposed air */
#define ANALYSIS_ADJ_AIR 512
#define ANALYSIS_FIRST_AIR 1024
#define ANALYSIS_LAST_AIR 2048
#define ANALYSIS_UNCONF_AIR 4096

/**
 * This structure holds the name of a unit value, and the conversion
 * factor necessary to convert from/to BRL-CAD standard units.
 *
 * The standard units are millimeters, cubic millimeters, and grams.
 *
 * XXX this section should be extracted to libbu/units.c
 */
struct cvt_tab {
    double val;
    char name[32];
};


static const struct cvt_tab units_tab[3][40] = {
    {
	/* length, stolen from bu/units.c with the "none" value
	 * removed Values for converting from given units to mm
	 */
	{1.0,		"mm"}, /* default */
	/* {0.0,		"none"}, */ /* this is removed to force a certain
					     * amount of error checking for the user
					     */
	{1.0e-7,	"angstrom"},
	{1.0e-7,	"decinanometer"},
	{1.0e-6,	"nm"},
	{1.0e-6,	"nanometer"},
	{1.0e-3,	"um"},
	{1.0e-3,	"micrometer"},
	{1.0e-3,	"micron"},
	{1.0,		"millimeter"},
	{10.0,		"cm"},
	{10.0,		"centimeter"},
	{1000.0,	"m"},
	{1000.0,	"meter"},
	{1000000.0,	"km"},
	{1000000.0,	"kilometer"},
	{25.4,		"in"},
	{25.4,		"inch"},
	{25.4,		"inches"},		/* for plural */
	{304.8,		"ft"},
	{304.8,		"foot"},
	{304.8,		"feet"},
	{456.2,		"cubit"},
	{914.4,		"yd"},
	{914.4,		"yard"},
	{5029.2,	"rd"},
	{5029.2,	"rod"},
	{1609344.0,	"mi"},
	{1609344.0,	"mile"},
	{1852000.0,	"nmile"},
	{1852000.0,	"nautical mile"},
	{1.495979e+14,	"AU"},
	{1.495979e+14,	"astronomical unit"},
	{9.460730e+18,	"lightyear"},
	{3.085678e+19,	"pc"},
	{3.085678e+19,	"parsec"},
	{0.0,		""}			/* LAST ENTRY */
    },
    {
	/* volume
	 * Values for converting from given units to mm^3
	 */
	{1.0, "cu mm"}, /* default */

	{1.0, "mm"},
	{1.0, "mm^3"},

	{1.0e3, "cm"},
	{1.0e3, "cm^3"},
	{1.0e3, "cu cm"},
	{1.0e3, "cc"},

	{1.0e6, "l"},
	{1.0e6, "liter"},
	{1.0e6, "litre"},

	{1.0e9, "m"},
	{1.0e9, "m^3"},
	{1.0e9, "cu m"},

	{16387.064, "in"},
	{16387.064, "in^3"},
	{16387.064, "cu in"},

	{28316846.592, "ft"},

	{28316846.592, "ft^3"},
	{28316846.592, "cu ft"},

	{764554857.984, "yds"},
	{764554857.984, "yards"},
	{764554857.984, "cu yards"},

	{0.0,		""}			/* LAST ENTRY */
    },
    {
	/* mass
	 * Values for converting given units to grams
	 */
	{1.0, "grams"}, /* default */

	{1.0, "g"},
	{0.0648, "gr"},
	{0.0648, "grains"},

	{1.0e3, "kg"},
	{1.0e3, "kilos"},
	{1.0e3, "kilograms"},

	{28.35, "oz"},
	{28.35, "ounce"},

	{453.6, "lb"},
	{453.6, "lbs"},
	{0.0,		""}			/* LAST ENTRY */
    }
};


/* this table keeps track of the "current" or "user selected units and
 * the associated conversion values
 */
#define LINE 0
#define VOL 1
#define MASS 2
#define COMMA ','

struct check_parameters {
    size_t require_num_hits;
    int ncpu;
    int print_per_region_stats;
    int use_air;
    int num_views;
    int getfromview;
    int overlaps_overlay_flag;
    int rpt_overlap_flag;
    int plot_files;
    int verbose;
    int debug;
    char *densityFileName;
    fastf_t azimuth_deg, elevation_deg;
    fastf_t gridSpacing, gridSpacingLimit;
    fastf_t samples_per_model_axis;
    fastf_t overlap_tolerance;
    fastf_t volume_tolerance;
    fastf_t mass_tolerance;
    fastf_t surf_area_tolerance;
    const struct cvt_tab *units[3];
    struct bu_vls *debug_str;
    struct bu_vls *verbose_str;
};

struct regions_list {
    struct bu_list l;
    char* region1;
    char* region2;
    unsigned long count;
    double max_dist;
    vect_t coord;
};


extern void
add_to_list(struct regions_list *list,
	    const char *r1,
	    const char *r2,
	    double dist,
	    point_t pt);

extern void
print_list(struct regions_list *list, const struct cvt_tab *units[3], char* name);

extern void
clear_list(struct regions_list *list);

extern void
print_verbose_debug(struct check_parameters *options);

typedef int check_functions_t(struct current_state *state,
			      struct db_i *dbip,
			      char **tobjtab,
			      int tnobjs,
			      struct check_parameters *options);

extern check_functions_t check_adj_air;

extern check_functions_t check_centroid;

extern check_functions_t check_exp_air;

extern check_functions_t check_gap;

extern check_functions_t check_mass;

extern check_functions_t check_moments;

extern check_functions_t check_overlaps;

extern check_functions_t check_surf_area;

extern check_functions_t check_unconf_air;

extern check_functions_t check_volume;
__END_DECLS

#endif /* LIBGED_CHECK_PRIVATE_H */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
