/*                   C H E C K _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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
#include "bu/units.h"
#include "analyze.h"

__BEGIN_DECLS

/**
 * Alias the libbu unit-conversion table type for use within the check command
 * sources.  The canonical definition is struct bu_cvt_tab in bu/units.h.
 */
typedef struct bu_cvt_tab cvt_tab;

/**
 * Shared unit-conversion tables (length / volume / mass) from libbu.
 * Indexed with BU_UNITS_LENGTH, BU_UNITS_VOLUME, or BU_UNITS_MASS.
 */
#define units_tab  bu_units_tab


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
    const cvt_tab *units[3];
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
print_list(struct ged *gedp, struct regions_list *list, const cvt_tab *units[3], char* name);

extern void
print_results_list(struct ged *gedp, struct bu_ptbl *tbl,
		   const cvt_tab *const units[3], const char *label);

extern void
clear_list(struct regions_list *list);

extern void
print_verbose_debug(struct ged *gedp, struct check_parameters *options);

typedef int check_functions_t(struct ged *gedp,
	struct analyze_results *res,
	struct check_parameters *options);

extern check_functions_t check_format_adj_air;

extern check_functions_t check_format_bbox;

extern check_functions_t check_format_centroid;

extern check_functions_t check_format_exp_air;

extern check_functions_t check_format_gap;

extern check_functions_t check_format_mass;

extern check_functions_t check_format_moments;

extern check_functions_t check_format_overlaps;

extern check_functions_t check_format_surf_area;

extern check_functions_t check_format_unconf_air;

extern check_functions_t check_format_volume;
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
