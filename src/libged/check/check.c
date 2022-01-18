/*                         C H E C K . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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

#include "bu/log.h"
#include "bu/getopt.h"

#include "../ged_private.h"
#include "./check_private.h"

#include "analyze.h"

#define MAX_WIDTH (32*1024)


HIDDEN void
check_show_help(struct ged *gedp)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&str, "Usage: check {subcommand} [options] [object(s)]\n\n");

    bu_vls_printf(&str, "Subcommands:\n\n");
    bu_vls_printf(&str, "  adj_air - Detects air volumes which are next to each other but have different air_code values applied to the region.\n");
    bu_vls_printf(&str, "  centroid - Computes the centroid of the objects specified.\n");
    bu_vls_printf(&str, "  exp_air - Check if the ray encounters air regions before (or after all) solid objects.\n");
    bu_vls_printf(&str, "  gap - This reports when there is more than overlap tolerance distance between objects on the ray path.\n");
    bu_vls_printf(&str, "  mass - Computes the mass of the objects specified.\n");
    bu_vls_printf(&str, "  moments - Computes the moments and products of inertia of the objects specified.\n");
    bu_vls_printf(&str, "  overlaps - This reports overlaps, when two regions occupy the same space.\n");
    bu_vls_printf(&str, "  surf_area - Computes the surface area of the objects specified.\n");
    bu_vls_printf(&str, "  unconf_air - This reports when there are unconfined air regions.\n");
    bu_vls_printf(&str, "  volume - Computes the volume of the objects specified.\n");

    bu_vls_printf(&str, "\nOptions:\n\n");
    bu_vls_printf(&str, "  -a #[deg|rad] - Azimuth angle.\n");
    bu_vls_printf(&str, "  -d - Set debug flag.\n");
    bu_vls_printf(&str, "  -e #[deg|rad] - Elevation angle.\n");
    bu_vls_printf(&str, "  -f filename - Specifies that density values should be taken from an external file instead of from the _DENSITIES object in the database.\n");
    bu_vls_printf(&str, "  -g [initial_grid_spacing-]grid_spacing_limit or [initial_grid_spacing,]grid_spacing_limit - Specifies a limit on how far the grid can be refined and optionally the initial spacing between rays in the grids.\n");
    bu_vls_printf(&str, "  -G [grid_width,]grid_height - sets the grid size, if only grid width is mentioned then a square grid size is set\n");
    bu_vls_printf(&str, "  -i - gets 'view information' from the view to setup eye position.\n");
    bu_vls_printf(&str, "  -M # - Specifies a mass tolerance value.\n");
    bu_vls_printf(&str, "  -n # - Specifies that the grid be refined until each region has at least num_hits ray intersections.\n");
    bu_vls_printf(&str, "  -N # - Specifies that only the first num_views should be computed.\n");
    bu_vls_printf(&str, "  -o - Specifies to display the overlaps as overlays.\n");
    bu_vls_printf(&str, "  -p - Specifies to produce plot files for each of the analyses it performs.\n");
    bu_vls_printf(&str, "  -P # - Specifies that ncpu CPUs should be used for performing the calculation. By default, all local CPUs are utilized.\n");
    bu_vls_printf(&str, "  -q - Quiets (suppresses) the 'was not hit' reporting.\n");
    bu_vls_printf(&str, "  -r - Indicates to print per-region statistics for mass and volume as well as the values for the objects specified.\n");
    bu_vls_printf(&str, "  -R - Disable reporting of overlaps.\n");
    bu_vls_printf(&str, "  -s # - Specifies surface area tolerance value.\n");
    bu_vls_printf(&str, "  -S # - Specifies that the grid spacing will be initially refined so that at least samples_per_axis_min will be shot along each axis of the bounding box of the model.\n");
    bu_vls_printf(&str, "  -t - Sets the tolerance for computing overlaps.\n");
    bu_vls_printf(&str, "  -u distance_units,volume_units,mass_units - Specify the units used when reporting values.\n");
    bu_vls_printf(&str, "  -U # - Specifies the Boolean value (0 or 1) for use_air which indicates whether regions which are marked as 'air' should be retained and included in the raytrace.\n");
    bu_vls_printf(&str, "  -v - Set verbose flag.\n");
    bu_vls_printf(&str, "  -V # - Specifies a volumetric tolerance value.\n");

    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
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
HIDDEN int
read_units_double(double *val, char *buf, const struct cvt_tab *cvt)
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
	bu_vls_printf(_ged_current_gedp->ged_result_str, "Bad units specifier \"%s\" on value \"%s\"\n", units_string, buf);
	return 1;

    found_units:
	*val = a * cvt->val;
	return 0;
    }
    bu_vls_printf(_ged_current_gedp->ged_result_str, "%s sscanf problem on \"%s\" got %d\n", CPP_FILELINE, buf, i);
    return 1;
}


HIDDEN int
parse_check_args(int ac, char *av[], struct check_parameters* options, struct current_state *state)
{
    int c;
    double a;
    char *p;

    char *options_str = "a:de:f:g:G:iM:n:N:opP:qrRs:S:t:U:u:vV:h?";

    /* Turn off getopt's error messages */
    bu_opterr = 0;
    bu_optind = 1;

    /* get all the options from the command line */
    while ((c=bu_getopt(ac, av, options_str)) != -1) {
	switch (c) {
	    case 'a':
		if (bn_decode_angle(&(options->azimuth_deg), bu_optarg) == 0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing azimuth \"%s\"\n", bu_optarg);
		    return -1;
		}
		analyze_set_azimuth(state, options->azimuth_deg);
		options->getfromview = 0;
		break;
	    case 'd':
		options->debug = 1;
		options->debug_str = bu_vls_vlsinit();
		analyze_enable_debug(state, options->debug_str);
		break;
	    case 'e':
		if (bn_decode_angle(&(options->elevation_deg), bu_optarg) == 0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing elevation \"%s\"\n", bu_optarg);
		    return -1;
		}
		analyze_set_elevation(state, options->elevation_deg);
		options->getfromview = 0;
		break;

	    case 'f':
		options->densityFileName = bu_optarg;
		analyze_set_densityfile(state, options->densityFileName);
		break;

	    case 'g':
		{
		    double value1, value2;

		    /* find out if we have two or one args; user can
		     * separate them with , or - delimiter
		     */
		    p = strchr(bu_optarg, COMMA);
		    if (p)
			*p++ = '\0';
		    else {
			p = strchr(bu_optarg, '-');
			if (p)
			    *p++ = '\0';
		    }


		    if (read_units_double(&value1, bu_optarg, units_tab[0])) {
			bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing grid spacing value \"%s\"\n", bu_optarg);
			return -1;
		    }

		    if (p) {
			/* we've got 2 values, they are upper limit
			 * and lower limit.
			 */
			if (read_units_double(&value2, p, units_tab[0])) {
			    bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing grid spacing limit value \"%s\"\n", p);
			    return -1;
			}

			options->gridSpacing = value1;
			options->gridSpacingLimit = value2;
		    } else {
			options->gridSpacingLimit = value1;

			options->gridSpacing = 0.0; /* flag it */
		    }
		    analyze_set_grid_spacing(state, options->gridSpacing, options->gridSpacingLimit);
		    break;
		}
	    case 'G':
		{
		    double width, height;
		    /* find out if we have two or one args; user can
		     * separate them with , delimiter
		     */
		    p = strchr(bu_optarg, COMMA);
		    if (p)
			*p++ = '\0';
		    width = atoi(bu_optarg);

		    if (width < 1 || width > MAX_WIDTH) {
			bu_vls_printf(_ged_current_gedp->ged_result_str,"mentioned grid size is out of range\n");
			return -1;
		    }

		    if (p) {
			/* widht and height mentioned */
			height = atoi(p);
			if (height < 1 || height > MAX_WIDTH) {
			    bu_vls_printf(_ged_current_gedp->ged_result_str,"mentioned grid size is out of range\n");
			    return -1;
			}
			analyze_set_grid_size(state, width, height);
		    } else {
			/* square grid */
			analyze_set_grid_size(state, width, width);
		    }
		    break;
		}
	    case 'i':
		options->getfromview = 1;
		break;
	    case 'M':
		if (read_units_double(&(options->mass_tolerance), bu_optarg, units_tab[2])) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in mass tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		analyze_set_mass_tolerance(state, options->mass_tolerance);
		break;
	    case 'n':
		if (sscanf(bu_optarg, "%d", &c) != 1 || c < 0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "num_hits must be integer value >= 0, not \"%s\"\n", bu_optarg);
		    return -1;
		}
		options->require_num_hits = (size_t) c;
		analyze_set_required_number_hits(state, options->require_num_hits);
		break;
	    case 'N':
		options->num_views = atoi(bu_optarg);
		analyze_set_num_views(state, options->num_views);
		break;
	    case 'o':
		options->overlaps_overlay_flag = 1;
		break;
	    case 'p':
		options->plot_files = 1;
		break;
	    case 'P':
		/* cannot ask for more cpu's than the machine has */
		c = atoi(bu_optarg);
		if (c > 0 && c <= (int) bu_avail_cpus())
		    options->ncpu = c;
		analyze_set_ncpu(state, options->ncpu);
		break;
	    case 'q':
		analyze_set_quiet_missed_report(state);
		break;
	    case 'r':
		options->print_per_region_stats = 1;
		break;
	    case 'R':
		options->rpt_overlap_flag = 0;
		break;
	    case 's':
		options->surf_area_tolerance = atof(bu_optarg);
		analyze_set_surf_area_tolerance(state, options->surf_area_tolerance);
		break;
	    case 'S':
		if (sscanf(bu_optarg, "%lg", &a) != 1 || a <= 1.0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in specifying minimum samples per model axis: \"%s\"\n", bu_optarg);
		    break;
		}
		options->samples_per_model_axis = a + 1;
		analyze_set_samples_per_model_axis(state, options->samples_per_model_axis);
		break;
	    case 't':
		if (read_units_double(&(options->overlap_tolerance), bu_optarg, units_tab[0])) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in overlap tolerance distance \"%s\"\n", bu_optarg);
		    return -1;
		}
		analyze_set_overlap_tolerance(state, options->overlap_tolerance);
		break;
	    case 'v':
		options->verbose = 1;
		options->verbose_str = bu_vls_vlsinit();
		analyze_enable_verbose(state, options->verbose_str);
		break;
	    case 'V':
		if (read_units_double(&(options->volume_tolerance), bu_optarg, units_tab[1])) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in volume tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		analyze_set_volume_tolerance(state, options->volume_tolerance);
		break;
	    case 'U':
		errno = 0;
		options->use_air = strtol(bu_optarg, (char **)NULL, 10);
		if (errno == ERANGE || errno == EINVAL) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in air argument %s\n", bu_optarg);
		    return -1;
		}
		analyze_set_use_air(state, options->use_air);
		break;
	    case 'u':
		{
		    int i;
		    char *ptr = bu_optarg;
		    const struct cvt_tab *cv;
		    static const char *dim[3] = {"length", "volume", "mass"};
		    char *units_name[3] = {NULL, NULL, NULL};
		    char **units_ap;

		    /* fill in units_name with the names we parse out */
		    units_ap = units_name;

		    /* acquire unit names */
		    for (i = 0; i < 3 && ptr; i++) {
			int found_unit;

			if (i == 0) {
			    *units_ap = strtok(ptr, CPP_XSTR(COMMA));
			} else {
			    *units_ap = strtok(NULL, CPP_XSTR(COMMA));
			}

			/* got something? */
			if (*units_ap == NULL)
			    break;

			/* got something valid? */
			found_unit = 0;
			for (cv = &units_tab[i][0]; cv->name[0] != '\0'; cv++) {
			    if (units_name[i] && BU_STR_EQUAL(cv->name, units_name[i])) {
				options->units[i] = cv;
				found_unit = 1;
				break;
			    }
			}

			if (!found_unit) {
			    bu_vls_printf(_ged_current_gedp->ged_result_str, "Units \"%s\" not found in conversion table\n", units_name[i]);
			    return -1;
			}

			++units_ap;
		    }

		    bu_vls_printf(_ged_current_gedp->ged_result_str, "Units: ");
		    for (i = 0; i < 3; i++) {
			bu_vls_printf(_ged_current_gedp->ged_result_str, " %s: %s", dim[i], options->units[i]->name);
		    }
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "\n");
		}
		break;

	    default: /* '?' 'h' */
		return -1;
	}
    }
    return bu_optind;
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
print_list(struct regions_list *list, const struct cvt_tab *units[3], char* name)
{
    struct regions_list *rp;

    if (BU_LIST_IS_EMPTY(&list->l)) {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "No %s\n", name);
	return;
    }

    bu_vls_printf(_ged_current_gedp->ged_result_str, "list %s:\n", name);

    for (BU_LIST_FOR (rp, regions_list, &(list->l))) {
	if (rp->region2) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s %s count: %lu dist: %g%s @ (%g %g %g)\n",
			  rp->region1, rp->region2 ,rp->count,
			  rp->max_dist / units[LINE]->val, units[LINE]->name, V3ARGS(rp->coord));
	} else {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s count: %lu dist: %g%s @ (%g %g %g)\n",
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
print_verbose_debug(struct check_parameters *options)
{
    if (options->verbose) bu_vls_vlscat(_ged_current_gedp->ged_result_str, options->verbose_str);
    if (options->debug) bu_vls_vlscat(_ged_current_gedp->ged_result_str, options->debug_str);
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

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    _ged_current_gedp = gedp;

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

    arg_count = parse_check_args(argc, (char **)argv, &options, state);

    if (arg_count < 0 ) {
	check_show_help(gedp);
	return GED_HELP;
    }

    nobjs = argc - arg_count;
    objtab = argv + (arg_count);

    if (nobjs <= 0){
	nvobjs = (int)ged_who_argc(gedp);
    }

    tnobjs = nvobjs + nobjs;

    if (tnobjs <= 0) {
	bu_vls_printf(gedp->ged_result_str,"no objects specified or in view -- raytrace aborted\n");
	analyze_free_current_state(state);
	state = NULL;
	return GED_ERROR;
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
	if (check_adj_air(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "centroid", len) == 0) {
	if (check_centroid(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "exp_air", len) == 0) {
	if (check_exp_air(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "gap", len) == 0) {
	if (check_gap(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "mass", len) == 0) {
	if (check_mass(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "moments", len) == 0) {
	if (check_moments(state, gedp->dbip, tobjtab, tnobjs, &options)) {
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
	if (check_overlaps(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "surf_area", len) == 0) {
	if (check_surf_area(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "unconf_air", len) == 0) {
	if (check_unconf_air(state, gedp->dbip, tobjtab, tnobjs, &options)) {
	    error = 1;
	    goto freemem;
	}
    } else if (bu_strncmp(sub, "volume", len) == 0) {
	if (check_volume(state, gedp->dbip, tobjtab, tnobjs, &options)) {
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
    return (error) ? GED_ERROR : GED_OK;
}
#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl check_cmd_impl = {
    "check",
    ged_check_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd check_cmd = { &check_cmd_impl };
const struct ged_cmd *check_cmds[] = { &check_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  check_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
