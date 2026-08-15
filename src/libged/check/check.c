/*                         C H E C K . C
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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "bu/log.h"
#include "bu/cmdschema.h"
#include "bu/opt.h"

#include "../ged_private.h"
#include "./check_private.h"

#include "analyze.h"

#define MAX_WIDTH (32*1024)


static void
check_show_help(struct ged *gedp)
{
    ged_cmd_help_append(gedp->ged_result_str, "check", "check");
}


/**
 * read_units_double
 *
 * Read a non-negative floating point value with optional units
 *
 * Return
 * 1 Failure
 * 0 Success
 */
static int
read_units_double(struct ged *gedp, double *val, const char *buf, const struct cvt_tab *cvt)
{
    double a;
#define UNITS_STRING_SZ 256
    char units_string[UNITS_STRING_SZ+1] = {0};
    int i;


    i = sscanf(buf, "%lg" CPP_SCAN(UNITS_STRING_SZ), &a, units_string);

    if (i < 0) return 1;

    if (i == 1) {
	*val = a;

	return 0;
    }
    if (i == 2) {
	*val = a;
	for (; cvt->name[0] != '\0';) {
	    if (!bu_strncmp(cvt->name, units_string, sizeof(units_string))) {
		goto found_units;
	    } else {
		cvt++;
	    }
	}
	bu_vls_printf(gedp->ged_result_str, "Bad units specifier \"%s\" on value \"%s\"\n", units_string, buf);
	return 1;

    found_units:
	*val = a * cvt->val;
	return 0;
    }
    bu_vls_printf(gedp->ged_result_str, "%s sscanf problem on \"%s\" got %d\n", CPP_FILELINE, buf, i);
    return 1;
}


struct check_args {
    struct check_parameters *options;
    struct current_state *state;
    int debug;
    const char *density_file;
    const char *grid_spacing;
    const char *grid_size;
    const char *mass_tolerance;
    int required_hits;
    int views;
    int overlaps_overlay;
    int plot_files;
    int cpus;
    int quiet;
    int region_stats;
    int report_overlaps;
    fastf_t surf_area_tolerance;
    fastf_t samples;
    const char *overlap_tolerance;
    int use_air;
    const char *report_units;
    int verbose;
    const char *volume_tolerance;
    int help;
};

static int
check_opt_azimuth(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct check_args *args = (struct check_args *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "azimuth");
    if (bn_decode_angle(&args->options->azimuth_deg, argv[0]) == 0) {
	if (msg)
	    bu_vls_printf(msg, "error parsing azimuth \"%s\"\n", argv[0]);
	return -1;
    }
    analyze_set_azimuth(args->state, args->options->azimuth_deg);
    args->options->getfromview = 0;
    return 1;
}

static int
check_opt_elevation(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct check_args *args = (struct check_args *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "elevation");
    if (bn_decode_angle(&args->options->elevation_deg, argv[0]) == 0) {
	if (msg)
	    bu_vls_printf(msg, "error parsing elevation \"%s\"\n", argv[0]);
	return -1;
    }
    analyze_set_elevation(args->state, args->options->elevation_deg);
    args->options->getfromview = 0;
    return 1;
}

static int
check_opt_view(struct bu_vls *UNUSED(msg), size_t UNUSED(argc),
	const char **UNUSED(argv), void *set_var)
{
    struct check_args *args = (struct check_args *)set_var;
    args->options->getfromview = 1;
    return 0;
}

static int
check_opt_nonnegative_int(struct bu_vls *msg, size_t argc,
	const char **argv, void *set_var)
{
    int ret = bu_opt_int(msg, argc, argv, set_var);

    if (ret < 0)
	return ret;
    if (*(int *)set_var < 0) {
	if (msg)
	    bu_vls_printf(msg, "num_hits must be an integer value >= 0, not \"%s\"\n",
		argv[0]);
	return -1;
    }
    return ret;
}

#define CHECK_OPTIONS(a) \
    {"a", NULL, "angle", check_opt_azimuth, a, "Set azimuth angle"}, \
    BU_OPT_FLAG(a, "d", NULL, debug, "Enable debug output"), \
    {"e", NULL, "angle", check_opt_elevation, a, "Set elevation angle"}, \
    BU_OPT_STR(a, "f", NULL, density_file, "file", "Read densities from file"), \
    BU_OPT_STR(a, "g", NULL, grid_spacing, "spacing", "Set grid refinement spacing"), \
    BU_OPT_STR(a, "G", NULL, grid_size, "size", "Set grid width and height"), \
    {"i", NULL, NULL, check_opt_view, a, "Use the current view"}, \
    BU_OPT_STR(a, "M", NULL, mass_tolerance, "tolerance", "Set mass tolerance"), \
    BU_OPT_CUSTOM(a, "n", NULL, required_hits, "hits", check_opt_nonnegative_int, "Set required hits per region"), \
    BU_OPT_INT(a, "N", NULL, views, "views", "Set number of views"), \
    BU_OPT_FLAG(a, "o", NULL, overlaps_overlay, "Display overlaps as overlays"), \
    BU_OPT_FLAG(a, "p", NULL, plot_files, "Produce plot files"), \
    BU_OPT_INT(a, "P", NULL, cpus, "cpus", "Set processor count"), \
    BU_OPT_FLAG(a, "q", NULL, quiet, "Suppress not-hit reporting"), \
    BU_OPT_FLAG(a, "r", NULL, region_stats, "Print per-region statistics"), \
    BU_OPT_FLAG(a, "R", NULL, report_overlaps, "Disable overlap reporting"), \
    BU_OPT_NUM(a, "s", NULL, surf_area_tolerance, "tolerance", "Set surface-area tolerance"), \
    BU_OPT_NUM(a, "S", NULL, samples, "samples", "Set minimum samples per model axis"), \
    BU_OPT_STR(a, "t", NULL, overlap_tolerance, "tolerance", "Set overlap tolerance"), \
    BU_OPT_BOOL(a, "U", NULL, use_air, "use_air", "Include air regions"), \
    BU_OPT_STR(a, "u", NULL, report_units, "units", "Set reporting units"), \
    BU_OPT_FLAG(a, "v", NULL, verbose, "Enable verbose output"), \
    BU_OPT_STR(a, "V", NULL, volume_tolerance, "tolerance", "Set volume tolerance"), \
    BU_OPT_FLAG(a, "h", NULL, help, "Print command help"), \
    BU_OPT_FLAG(a, "?", NULL, help, "Print command help"),
BU_OPT_DESC_BUILDER(check_options, struct check_args, CHECK_OPTIONS);

static int
parse_check_args(struct ged *gedp, int ac, const char **av,
	struct check_parameters *options, struct current_state *state)
{
    struct check_args args = {0};
    int object_count;

    args.options = options;
    args.state = state;
    args.required_hits = INT_MIN;
    args.views = INT_MIN;
    args.cpus = INT_MIN;
    args.surf_area_tolerance = NAN;
    args.samples = NAN;
    args.use_air = INT_MIN;

    if (ac < 1)
	return -1;
    ac--; av++;
    object_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	(size_t)ac, av, check_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (object_count < 0 || args.help)
	return -1;

    if (args.debug) {
	options->debug = 1;
	options->debug_str = bu_vls_vlsinit();
	analyze_enable_debug(state, options->debug_str);
    }
    if (args.density_file) {
	options->densityFileName = (char *)args.density_file;
	analyze_set_densityfile(state, options->densityFileName);
    }
    if (args.grid_spacing) {
	char *grid_arg = bu_strdup(args.grid_spacing);
	char *limit_arg = strchr(grid_arg, COMMA);
	double value1, value2;
	if (limit_arg)
	    *limit_arg++ = '\0';
	else {
	    limit_arg = strchr(grid_arg, '-');
	    if (limit_arg)
		*limit_arg++ = '\0';
	}
	if (read_units_double(gedp, &value1, grid_arg, units_tab[0])) {
	    bu_vls_printf(gedp->ged_result_str,
		"error parsing grid spacing value \"%s\"\n", grid_arg);
	    bu_free(grid_arg, "check grid argument");
	    return -1;
	}
	if (limit_arg) {
	    if (read_units_double(gedp, &value2, limit_arg, units_tab[0])) {
		bu_vls_printf(gedp->ged_result_str,
		    "error parsing grid spacing limit value \"%s\"\n", limit_arg);
		bu_free(grid_arg, "check grid argument");
		return -1;
	    }
	    options->gridSpacing = value1;
	    options->gridSpacingLimit = value2;
	} else {
	    options->gridSpacing = 0.0;
	    options->gridSpacingLimit = value1;
	}
	analyze_set_grid_spacing(state, options->gridSpacing,
	    options->gridSpacingLimit);
	bu_free(grid_arg, "check grid argument");
    }
    if (args.grid_size) {
	char *grid_arg = bu_strdup(args.grid_size);
	char *height_arg = strchr(grid_arg, COMMA);
	char *end = NULL;
	long width, height;
	if (height_arg)
	    *height_arg++ = '\0';
	errno = 0;
	width = strtol(grid_arg, &end, 10);
	if (errno || !end || *end != '\0' || width < 1 || width > MAX_WIDTH) {
	    bu_vls_printf(gedp->ged_result_str, "mentioned grid size is out of range\n");
	    bu_free(grid_arg, "check grid size argument");
	    return -1;
	}
	height = width;
	if (height_arg) {
	    errno = 0;
	    height = strtol(height_arg, &end, 10);
	    if (errno || !end || *end != '\0' || height < 1 || height > MAX_WIDTH) {
		bu_vls_printf(gedp->ged_result_str, "mentioned grid size is out of range\n");
		bu_free(grid_arg, "check grid size argument");
		return -1;
	    }
	}
	analyze_set_grid_size(state, (double)width, (double)height);
	bu_free(grid_arg, "check grid size argument");
    }
    if (args.mass_tolerance && read_units_double(gedp,
	&options->mass_tolerance, args.mass_tolerance, units_tab[2])) {
	bu_vls_printf(gedp->ged_result_str,
	    "error in mass tolerance \"%s\"\n", args.mass_tolerance);
	return -1;
    }
    if (args.mass_tolerance)
	analyze_set_mass_tolerance(state, options->mass_tolerance);
    if (args.required_hits != INT_MIN) {
	options->require_num_hits = (size_t)args.required_hits;
	analyze_set_required_number_hits(state, options->require_num_hits);
    }
    if (args.views != INT_MIN) {
	options->num_views = args.views;
	analyze_set_num_views(state, options->num_views);
    }
    options->overlaps_overlay_flag = args.overlaps_overlay;
    options->plot_files = args.plot_files;
    if (args.cpus > 0 && args.cpus <= (int)bu_avail_cpus())
	options->ncpu = args.cpus;
    analyze_set_ncpu(state, options->ncpu);
    if (args.quiet)
	analyze_set_quiet_missed_report(state);
    options->print_per_region_stats = args.region_stats;
    if (args.report_overlaps)
	options->rpt_overlap_flag = 0;
    if (!isnan(args.surf_area_tolerance)) {
	options->surf_area_tolerance = args.surf_area_tolerance;
	analyze_set_surf_area_tolerance(state, options->surf_area_tolerance);
    }
    if (!isnan(args.samples)) {
	if (args.samples <= 1.0) {
	    bu_vls_printf(gedp->ged_result_str,
		"error in specifying minimum samples per model axis: \"%g\"\n",
		args.samples);
	} else {
	    options->samples_per_model_axis = args.samples + 1.0;
	    analyze_set_samples_per_model_axis(state,
		options->samples_per_model_axis);
	}
    }
    if (args.overlap_tolerance && read_units_double(gedp,
	&options->overlap_tolerance, args.overlap_tolerance, units_tab[0])) {
	bu_vls_printf(gedp->ged_result_str,
	    "error in overlap tolerance distance \"%s\"\n",
	    args.overlap_tolerance);
	return -1;
    }
    if (args.overlap_tolerance)
	analyze_set_overlap_tolerance(state, options->overlap_tolerance);
    if (args.use_air != INT_MIN) {
	options->use_air = args.use_air;
	analyze_set_use_air(state, options->use_air);
    }
    if (args.report_units) {
	char *units_arg = bu_strdup(args.report_units);
	char *unit = strtok(units_arg, CPP_XSTR(COMMA));
	static const char *dim[3] = {"length", "volume", "mass"};
	int i;
	for (i = 0; i < 3 && unit; i++, unit = strtok(NULL, CPP_XSTR(COMMA))) {
	    const struct cvt_tab *cv;
	    int found_unit = 0;
	    for (cv = &units_tab[i][0]; cv->name[0] != '\0'; cv++) {
		if (BU_STR_EQUAL(cv->name, unit)) {
		    options->units[i] = cv;
		    found_unit = 1;
		    break;
		}
	    }
	    if (!found_unit) {
		bu_vls_printf(gedp->ged_result_str,
		    "Units \"%s\" not found in conversion table\n", unit);
		bu_free(units_arg, "check units argument");
		return -1;
	    }
	}
	bu_free(units_arg, "check units argument");
	bu_vls_printf(gedp->ged_result_str, "Units: ");
	for (i = 0; i < 3; i++)
	    bu_vls_printf(gedp->ged_result_str, " %s: %s", dim[i], options->units[i]->name);
	bu_vls_printf(gedp->ged_result_str, "\n");
    }
    if (args.verbose) {
	options->verbose = 1;
	options->verbose_str = bu_vls_vlsinit();
	analyze_enable_verbose(state, options->verbose_str);
    }
    if (args.volume_tolerance && read_units_double(gedp,
	&options->volume_tolerance, args.volume_tolerance, units_tab[1])) {
	bu_vls_printf(gedp->ged_result_str,
	    "error in volume tolerance \"%s\"\n", args.volume_tolerance);
	return -1;
    }
    if (args.volume_tolerance)
	analyze_set_volume_tolerance(state, options->volume_tolerance);

    return object_count;
}

/*
 * add unique pairs of regions to list
 */
void
add_to_list(struct regions_list *list,
	    const char *r1,
	    const char *r2,
	    double dist,
	    point_t pt)
{
    struct regions_list *rp, *rpair;

    /* look for it in our list */
    for (BU_LIST_FOR (rp, regions_list, &list->l)) {

	if ((BU_STR_EQUAL(r1, rp->region1) && BU_STR_EQUAL(r2, rp->region2)) || (BU_STR_EQUAL(r1, rp->region2) && BU_STR_EQUAL(r2, rp->region1))) {
	    /* we already have an entry for this region pair, we
	     * increase the counter, check the depth and update
	     * thickness maximum and entry point if need be and
	     * return.
	     */
	    rp->count++;

	    if (dist > rp->max_dist) {
		rp->max_dist = dist;
		VMOVE(rp->coord, pt);
	    }
	    return;
	}
    }
    /* didn't find it in the list.  Add it */
    BU_ALLOC(rpair, struct regions_list);
    rpair->region1 = (char *)bu_malloc(strlen(r1)+1, "region1");
    bu_strlcpy(rpair->region1, r1, strlen(r1)+1);
    if (r2) {
    rpair->region2 = (char *)bu_malloc(strlen(r2)+1, "region2");
    bu_strlcpy(rpair->region2, r2, strlen(r2)+1);
    } else {
	rpair->region2 = (char *) NULL;
    }
    rpair->count = 1;
    rpair->max_dist = dist;
    VMOVE(rpair->coord, pt);
    list->max_dist ++; /* really a count */

    /* insert in the list at the "nice" place */
    for (BU_LIST_FOR (rp, regions_list, &list->l)) {
	if (bu_strcmp(rp->region1, r1) <= 0)
	    break;
    }
    BU_LIST_INSERT(&rp->l, &rpair->l);
}


void
print_list(struct ged *gedp, struct regions_list *list, const struct cvt_tab *units[3], char* name)
{
    struct regions_list *rp;

    if (BU_LIST_IS_EMPTY(&list->l)) {
	bu_vls_printf(gedp->ged_result_str, "No %s\n", name);
	return;
    }

    bu_vls_printf(gedp->ged_result_str, "list %s:\n", name);

    for (BU_LIST_FOR (rp, regions_list, &(list->l))) {
	if (rp->region2) {
	    bu_vls_printf(gedp->ged_result_str, "\t%s %s count: %lu dist: %g%s @ (%g %g %g)\n",
			  rp->region1, rp->region2 ,rp->count,
			  rp->max_dist / units[LINE]->val, units[LINE]->name, V3ARGS(rp->coord));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\t%s count: %lu dist: %g%s @ (%g %g %g)\n",
			  rp->region1, rp->count,
			  rp->max_dist / units[LINE]->val, units[LINE]->name, V3ARGS(rp->coord));
	}
    }
}


void
clear_list(struct regions_list *list)
{
    struct regions_list *rp;
    for (BU_LIST_FOR (rp, regions_list, &(list->l))) {
	bu_free(rp->region1, "reg1 name");
	if (rp->region2 != (char*)NULL)
	    bu_free(rp->region2, "reg1 name");
    }
    bu_list_free(&list->l);
}


void
print_verbose_debug(struct ged *gedp, struct check_parameters *options)
{
    if (options->verbose) bu_vls_vlscat(gedp->ged_result_str, options->verbose_str);
    if (options->debug) bu_vls_vlscat(gedp->ged_result_str, options->debug_str);
}


int ged_check_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int opt_argc, arg_count;
    const char *cmd = argv[0];
    const char *sub = NULL;
    size_t len;

    struct current_state *state = NULL;

    struct check_parameters options;
    const char *check_subcommands[] = {"adj_air", "centroid", "exp_air", "gap",
				       "mass", "moments", "overlaps", "surf_area",
				       "unconf_air", "volume", NULL};
    const struct cvt_tab *units[3] = {
	&units_tab[0][0],	/* linear */
	&units_tab[1][0],	/* volume */
	&units_tab[2][0]	/* mass */
    };

    int tnobjs = 0;
    int nvobjs = 0;
    char **tobjtab;
    char **objp;
    int nobjs = 0;			/* Number of cmd-line treetops */
    const char **objtab;		/* array of treetop strings */
    int error = 0;
    VMOVE(options.units, units);

    state = analyze_current_state_init();

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

   if (argc < 2) {
	check_show_help(gedp);
	return GED_HELP;
    }

    /* See if we have any options to deal with.  Once we hit a subcommand, we're done */
    opt_argc = argc;
    for (i = 1; i < argc; ++i) {
	const char * const *subcmd = check_subcommands;

	for (; *subcmd != NULL; ++subcmd) {
	    if (BU_STR_EQUAL(argv[i], *subcmd)) {
		opt_argc = i;
		i = argc;
		break;
	    }
	}
    }

    if (opt_argc >= argc) {
	check_show_help(gedp);
	return GED_HELP;
    }

    options.getfromview = 0;
    options.print_per_region_stats = 0;
    options.overlaps_overlay_flag = 0;
    options.plot_files = 0;
    options.debug = 0;
    options.ncpu = bu_avail_cpus();
    options.verbose = 0;
    options.rpt_overlap_flag = 1;

    /* shift to subcommand args */
    argc -= opt_argc;
    argv = &argv[opt_argc];

    arg_count = parse_check_args(gedp, argc, argv, &options, state);

    if (arg_count < 0 ) {
	check_show_help(gedp);
	return GED_HELP;
    }

    nobjs = arg_count;
    objtab = argv + 1;

    if (nobjs <= 0){
	nvobjs = (int)ged_who_argc(gedp);
    }

    tnobjs = nvobjs + nobjs;

    if (tnobjs <= 0) {
	bu_vls_printf(gedp->ged_result_str,"no objects specified or in view -- raytrace aborted\n");
	analyze_free_current_state(state);
	state = NULL;
	return BRLCAD_ERROR;
    }

    tobjtab = (char **)bu_calloc(tnobjs, sizeof(char *), "alloc tobjtab");
    objp = &tobjtab[0];

    /* copy all specified objects if any */
    for(i = 0; i < nobjs; i++)
	*objp++ = bu_strdup(objtab[i]);

    /* else copy all the objects in view if any */
    if (nobjs <= 0) {
	nvobjs = ged_who_argv(gedp, objp, (const char **)&tobjtab[tnobjs]);
	/* now, as we know the exact number of objects in the view, check again for > 0 */
	if (nvobjs <= 0) {
	    bu_vls_printf(gedp->ged_result_str,"no objects specified or in view, aborting\n");
	    error = 1;
	    goto freemem;
	}
    }

    tnobjs = nvobjs + nobjs;

    /* determine subcommand */
    sub = argv[0];
    len = strlen(sub);
    if (bu_strncmp(sub, "adj_air", len) == 0) {
	if (check_adj_air(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "centroid", len) == 0) {
	if (check_centroid(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "exp_air", len) == 0) {
	if (check_exp_air(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "gap", len) == 0) {
	if (check_gap(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "mass", len) == 0) {
	if (check_mass(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "moments", len) == 0) {
	if (check_moments(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "overlaps", len) == 0) {
	if (options.getfromview) {
	    point_t eye_model;
	    quat_t quat;
	    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);
	    _ged_rt_set_eye_model(gedp, eye_model);
	    analyze_set_view_information(state, gedp->ged_gvp->gv_size, &eye_model, &quat);
	}
	if (check_overlaps(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "surf_area", len) == 0) {
	if (check_surf_area(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "unconf_air", len) == 0) {
	if (check_unconf_air(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "volume", len) == 0) {
	if (check_volume(gedp, state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!", cmd, sub);
	error = 1;
    }

freemem:
    bu_free(tobjtab, "free tobjtab");
    tobjtab = NULL;
    analyze_free_current_state(state);
    state = NULL;
    if (options.verbose) bu_vls_free(options.verbose_str);
    if (options.debug) bu_vls_free(options.debug_str);
    return (error) ? BRLCAD_ERROR : BRLCAD_OK;
}

#include "../include/plugin.h"

static const struct bu_cmd_option check_schema_options[] = {
    BU_CMD_VALUE_UNBOUND("a", NULL, "a", BU_CMD_VALUE_STRING, "angle", "Azimuth angle"),
    BU_CMD_FLAG_UNBOUND("d", NULL, "d", "Enable debug output"),
    BU_CMD_VALUE_UNBOUND("e", NULL, "e", BU_CMD_VALUE_STRING, "angle", "Elevation angle"),
    BU_CMD_VALUE_UNBOUND("f", NULL, "f", BU_CMD_VALUE_FILE, "file", "External density file"),
    BU_CMD_VALUE_UNBOUND("g", NULL, "g", BU_CMD_VALUE_STRING, "spacing", "Grid refinement spacing"),
    BU_CMD_VALUE_UNBOUND("G", NULL, "G", BU_CMD_VALUE_STRING, "size", "Grid width and height"),
    BU_CMD_FLAG_UNBOUND("i", NULL, "i", "Use the current view"),
    BU_CMD_VALUE_UNBOUND("M", NULL, "M", BU_CMD_VALUE_NUMBER, "number", "Mass tolerance"),
    BU_CMD_VALUE_UNBOUND("n", NULL, "n", BU_CMD_VALUE_INTEGER, "count", "Minimum hits per region"),
    BU_CMD_VALUE_UNBOUND("N", NULL, "N", BU_CMD_VALUE_INTEGER, "count", "Maximum views"),
    BU_CMD_FLAG_UNBOUND("o", NULL, "o", "Display overlaps as overlays"),
    BU_CMD_FLAG_UNBOUND("p", NULL, "p", "Produce plot files"),
    BU_CMD_VALUE_UNBOUND("P", NULL, "P", BU_CMD_VALUE_INTEGER, "count", "CPU count"),
    BU_CMD_FLAG_UNBOUND("q", NULL, "q", "Suppress not-hit reporting"),
    BU_CMD_FLAG_UNBOUND("r", NULL, "r", "Print per-region statistics"),
    BU_CMD_FLAG_UNBOUND("R", NULL, "R", "Disable overlap reporting"),
    BU_CMD_VALUE_UNBOUND("s", NULL, "s", BU_CMD_VALUE_NUMBER, "number", "Surface-area tolerance"),
    BU_CMD_VALUE_UNBOUND("S", NULL, "S", BU_CMD_VALUE_NUMBER, "count", "Minimum samples per axis"),
    BU_CMD_VALUE_UNBOUND("t", NULL, "t", BU_CMD_VALUE_STRING, "number", "Overlap tolerance"),
    BU_CMD_VALUE_UNBOUND("u", NULL, "u", BU_CMD_VALUE_STRING, "units", "Distance, volume, and mass units"),
    BU_CMD_VALUE_UNBOUND("U", NULL, "U", BU_CMD_VALUE_BOOL, "0|1", "Include air regions"),
    BU_CMD_FLAG_UNBOUND("v", NULL, "v", "Enable verbose output"),
    BU_CMD_VALUE_UNBOUND("V", NULL, "V", BU_CMD_VALUE_NUMBER, "number", "Volume tolerance"),
    BU_CMD_FLAG_UNBOUND("h", NULL, "h", "Print help"),
    BU_CMD_ALIAS_SHORT("?", "h", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand check_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 0, BU_CMD_COUNT_UNLIMITED,
	"Objects to analyze (defaults to displayed objects)", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
#define CHECK_SCHEMA(_id, _name, _help) \
    static const struct bu_cmd_schema _id##_schema = \
	BU_CMD_SCHEMA_EXTERNAL(_name, _help, check_schema_options, check_schema_operands, \
	    BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL)
CHECK_SCHEMA(check_adj_air, "adj_air", "Find adjacent air regions with differing codes");
CHECK_SCHEMA(check_centroid, "centroid", "Compute centroids");
CHECK_SCHEMA(check_exp_air, "exp_air", "Find exposed air regions");
CHECK_SCHEMA(check_gap, "gap", "Report gaps along ray paths");
CHECK_SCHEMA(check_mass, "mass", "Compute mass");
CHECK_SCHEMA(check_moments, "moments", "Compute moments and products of inertia");
CHECK_SCHEMA(check_overlaps, "overlaps", "Report overlapping regions");
CHECK_SCHEMA(check_surf_area, "surf_area", "Compute surface area");
CHECK_SCHEMA(check_unconf_air, "unconf_air", "Report unconfined air regions");
CHECK_SCHEMA(check_volume, "volume", "Compute volume");
#undef CHECK_SCHEMA
GED_DEFINE_NO_ARG_SCHEMA_NAMED(check_root_schema, "check",
    "Analyze geometric and physical properties", BU_CMD_PARSE_OPTIONS_FIRST);
static const struct bu_cmd_tree_node check_subcommands[] = {
    BU_CMD_TREE_NODE(&check_adj_air_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_centroid_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_exp_air_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_gap_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_mass_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_moments_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_overlaps_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_surf_area_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_unconf_air_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&check_volume_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree check_tree = {
    &check_root_schema, check_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static int
check_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &check_tree, input, cursor_pos, result);
}

static int
check_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &check_tree, input, analysis);
}

static char *
check_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&check_tree);
}

static char *
check_grammar_help(const char *invocation)
{
    return bu_cmd_tree_help(&check_tree, invocation);
}

static int
check_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&check_tree, msgs);
}

static const struct ged_cmd_grammar check_grammar = {
    "check", "Analyze geometric and physical properties", check_grammar_validate,
    check_grammar_analyze, check_grammar_json, check_grammar_lint, NULL,
    check_grammar_help
};

#define GED_CHECK_COMMANDS(X, XID, NX, NXID, GX, GXID) \
    GX(check, ged_check_core, GED_CMD_DEFAULT, &check_grammar) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_CHECK_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_check", 1, GED_CHECK_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
