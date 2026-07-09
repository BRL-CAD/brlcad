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

#include "bu/log.h"
#include "bu/getopt.h"
#include "bv/vlist.h"
#include "bv/plot3.h"

#include "../ged_private.h"
#include "./check_private.h"

#include "analyze.h"

#define MAX_WIDTH (32*1024)

static void
check_show_help(struct ged *gedp)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&str, "Usage: check {subcommand} [options] [object(s)]\n\n");

    bu_vls_printf(&str, "Subcommands:\n\n");
    bu_vls_printf(&str, "  adj_air - Detects air volumes which are next to each other but have different air_code values applied to the region.\n");
    bu_vls_printf(&str, "  bbox - Reports the axis-aligned bounding box of the specified objects (no rays needed).\n");
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
static int
read_units_double(struct ged *gedp, double *val, char *buf, const cvt_tab *cvt)
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


static int
parse_check_args(struct ged *gedp, int ac, char *av[], struct check_parameters* options, struct analyze_config *cfg)
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
		    bu_vls_printf(gedp->ged_result_str, "error parsing azimuth \"%s\"\n", bu_optarg);
		    return -1;
		}
		cfg->azimuth_deg = options->azimuth_deg;
		options->getfromview = 0;
		break;
	    case 'd':
		options->debug = 1;
		options->debug_str = bu_vls_vlsinit();
		cfg->log_str = options->debug_str;
		break;
	    case 'e':
		if (bn_decode_angle(&(options->elevation_deg), bu_optarg) == 0) {
		    bu_vls_printf(gedp->ged_result_str, "error parsing elevation \"%s\"\n", bu_optarg);
		    return -1;
		}
		cfg->elevation_deg = options->elevation_deg;
		options->getfromview = 0;
		break;

	    case 'f':
		options->densityFileName = bu_optarg;
		cfg->density_file = options->densityFileName;
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


		    if (read_units_double(gedp, &value1, bu_optarg, units_tab[0])) {
			bu_vls_printf(gedp->ged_result_str, "error parsing grid spacing value \"%s\"\n", bu_optarg);
			return -1;
		    }

		    if (p) {
			/* we've got 2 values, they are upper limit
			 * and lower limit.
			 */
			if (read_units_double(gedp, &value2, p, units_tab[0])) {
			    bu_vls_printf(gedp->ged_result_str, "error parsing grid spacing limit value \"%s\"\n", p);
			    return -1;
			}

			options->gridSpacing = value1;
			options->gridSpacingLimit = value2;
		    } else {
			options->gridSpacingLimit = value1;

			options->gridSpacing = 0.0; /* flag it */
		    }
		    cfg->grid_spacing     = options->gridSpacing;
		    cfg->grid_spacing_min = options->gridSpacingLimit;
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
			bu_vls_printf(gedp->ged_result_str,"mentioned grid size is out of range\n");
			return -1;
		    }

		    if (p) {
			/* width and height mentioned */
			height = atoi(p);
			if (height < 1 || height > MAX_WIDTH) {
			    bu_vls_printf(gedp->ged_result_str,"mentioned grid size is out of range\n");
			    return -1;
			}
			cfg->grid_width  = (int)width;
			cfg->grid_height = (int)height;
		    } else {
			/* square grid */
			cfg->grid_width  = (int)width;
			cfg->grid_height = (int)width;
		    }
		    break;
		}
	    case 'i':
		options->getfromview = 1;
		break;
	    case 'M':
		if (read_units_double(gedp, &(options->mass_tolerance), bu_optarg, units_tab[2])) {
		    bu_vls_printf(gedp->ged_result_str, "error in mass tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		cfg->mass_tol = options->mass_tolerance;
		break;
	    case 'n':
		if (sscanf(bu_optarg, "%d", &c) != 1 || c < 0) {
		    bu_vls_printf(gedp->ged_result_str, "num_hits must be integer value >= 0, not \"%s\"\n", bu_optarg);
		    return -1;
		}
		options->require_num_hits = (size_t) c;
		cfg->required_hits = options->require_num_hits;
		break;
	    case 'N':
		options->num_views = atoi(bu_optarg);
		cfg->num_views = options->num_views;
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
		cfg->ncpu = options->ncpu;
		break;
	    case 'q':
		cfg->quiet_missed = 1;
		break;
	    case 'r':
		options->print_per_region_stats = 1;
		break;
	    case 'R':
		options->rpt_overlap_flag = 0;
		break;
	    case 's':
		options->surf_area_tolerance = atof(bu_optarg);
		cfg->surf_area_tol = options->surf_area_tolerance;
		break;
	    case 'S':
		if (sscanf(bu_optarg, "%lg", &a) != 1 || a <= 1.0) {
		    bu_vls_printf(gedp->ged_result_str, "error in specifying minimum samples per model axis: \"%s\"\n", bu_optarg);
		    break;
		}
		options->samples_per_model_axis = a + 1;
		cfg->samples_per_model_axis = options->samples_per_model_axis;
		break;
	    case 't':
		if (read_units_double(gedp, &(options->overlap_tolerance), bu_optarg, units_tab[0])) {
		    bu_vls_printf(gedp->ged_result_str, "error in overlap tolerance distance \"%s\"\n", bu_optarg);
		    return -1;
		}
		cfg->overlap_tol = options->overlap_tolerance;
		break;
	    case 'v':
		options->verbose = 1;
		options->verbose_str = bu_vls_vlsinit();
		cfg->verbose  = 1;
		cfg->log_str  = options->verbose_str;
		break;
	    case 'V':
		if (read_units_double(gedp, &(options->volume_tolerance), bu_optarg, units_tab[1])) {
		    bu_vls_printf(gedp->ged_result_str, "error in volume tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		cfg->volume_tol = options->volume_tolerance;
		break;
	    case 'U':
		errno = 0;
		options->use_air = strtol(bu_optarg, (char **)NULL, 10);
		if (errno == ERANGE || errno == EINVAL) {
		    bu_vls_printf(gedp->ged_result_str, "error in air argument %s\n", bu_optarg);
		    return -1;
		}
		cfg->use_air = options->use_air;
		break;
	    case 'u':
		{
		    int i;
		    char *ptr = bu_optarg;
		    const cvt_tab *cv;
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
			    bu_vls_printf(gedp->ged_result_str, "Units \"%s\" not found in conversion table\n", units_name[i]);
			    return -1;
			}

			++units_ap;
		    }

		    bu_vls_printf(gedp->ged_result_str, "Units: ");
		    for (i = 0; i < 3; i++) {
			bu_vls_printf(gedp->ged_result_str, " %s: %s", dim[i], options->units[i]->name);
		    }
		    bu_vls_printf(gedp->ged_result_str, "\n");
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
print_list(struct ged *gedp, struct regions_list *list, const cvt_tab *units[3], char* name)
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


/**
 * Format a bu_ptbl of analyze_overlap_record entries (gaps, air, etc.)
 * in the same style as print_list().
 */
void
print_results_list(struct ged *gedp, struct bu_ptbl *tbl,
		   const cvt_tab *const units[3], const char *label)
{
    size_t i;

    if (BU_PTBL_LEN(tbl) == 0) {
	bu_vls_printf(gedp->ged_result_str, "No %s\n", label);
	return;
    }

    bu_vls_printf(gedp->ged_result_str, "list %s:\n", label);

    for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	struct analyze_overlap_record *r =
	    (struct analyze_overlap_record *)BU_PTBL_GET(tbl, i);
	if (r->region2) {
	    bu_vls_printf(gedp->ged_result_str,
			  "\t%s %s count: %lu dist: %g%s @ (%g %g %g)\n",
			  r->region1, r->region2, r->count,
			  r->max_dist / units[LINE]->val, units[LINE]->name,
			  V3ARGS(r->coord));
	} else {
	    bu_vls_printf(gedp->ged_result_str,
			  "\t%s count: %lu dist: %g%s @ (%g %g %g)\n",
			  r->region1, r->count,
			  r->max_dist / units[LINE]->val, units[LINE]->name,
			  V3ARGS(r->coord));
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


/* ======================================================================
 * Per-event render hook helpers.
 *
 * These static functions are installed as analyze_config render hooks
 * before calling analyze_run().  Each writes to plot files, adds overlay
 * geometry, or produces real-time console output per detected event.
 * The data pointers point into local structs on ged_check_core's stack.
 * ====================================================================== */

struct check_overlap_rd {
    FILE              *plot_file;
    int                overlap_color[3];
    int                overlay_flag;
    struct bv_vlblock *vbp;
    struct bu_list    *vhead;
    struct ged        *gedp;
    size_t             noverlaps; /* total overlap instances seen */
    int                rpt_overlap_flag;
    int                sem; /* serialises output, counter, vlist, plot */
};

static void
check_overlap_render_fn_impl(const char *r1, const char *r2,
			     double depth, point_t ihit, point_t ohit,
			     void *data)
{
    struct check_overlap_rd *rd = (struct check_overlap_rd *)data;
    size_t noverlaps;

    bu_semaphore_acquire(rd->sem);
    noverlaps = ++rd->noverlaps;
    bu_semaphore_release(rd->sem);

    if (!rd->rpt_overlap_flag) {
	/* Immediate output: print each occurrence as it occurs. */
	bu_semaphore_acquire(rd->sem);
	bu_vls_printf(rd->gedp->ged_result_str,
		      "OVERLAP %zu: %s\nOVERLAP %zu: %s\nOVERLAP %zu: depth %gmm\n"
		      "OVERLAP %zu: in_hit_point (%g, %g, %g) mm\n"
		      "OVERLAP %zu: out_hit_point (%g, %g, %g) mm\n"
		      "------------------------------------------------------------\n",
		      noverlaps, r1, noverlaps, r2, noverlaps, depth,
		      noverlaps, V3ARGS(ihit), noverlaps, V3ARGS(ohit));
	bu_semaphore_release(rd->sem);
    }

    if (rd->overlay_flag && rd->vbp && rd->vhead) {
	bu_semaphore_acquire(rd->sem);
	BV_ADD_VLIST(rd->vbp->free_vlist_hd, rd->vhead, ihit, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(rd->vbp->free_vlist_hd, rd->vhead, ohit, BV_VLIST_LINE_DRAW);
	bu_semaphore_release(rd->sem);
    }

    if (rd->plot_file) {
	bu_semaphore_acquire(rd->sem);
	pl_color(rd->plot_file, V3ARGS(rd->overlap_color));
	pdv_3line(rd->plot_file, ihit, ohit);
	bu_semaphore_release(rd->sem);
    }
}


struct check_gap_rd {
    FILE *plot_file;
    int   gap_color[3];
    int   sem;
};

static void
check_gap_render_fn_impl(const char *UNUSED(r1), const char *UNUSED(r2),
			 double UNUSED(dist),
			 point_t gap_start, point_t gap_end, void *data)
{
    struct check_gap_rd *rd = (struct check_gap_rd *)data;
    if (rd->plot_file) {
	bu_semaphore_acquire(rd->sem);
	pl_color(rd->plot_file, V3ARGS(rd->gap_color));
	pdv_3line(rd->plot_file, gap_start, gap_end);
	bu_semaphore_release(rd->sem);
    }
}


struct check_adj_air_rd {
    FILE *plot_file;
    int   adjAir_color[3];
    int   sem;
};

static void
check_adj_air_render_fn_impl(const char *UNUSED(r1), const char *UNUSED(r2),
			     point_t in_pt, point_t out_pt, void *data)
{
    struct check_adj_air_rd *rd = (struct check_adj_air_rd *)data;
    if (rd->plot_file) {
	bu_semaphore_acquire(rd->sem);
	pl_color(rd->plot_file, V3ARGS(rd->adjAir_color));
	pdv_3line(rd->plot_file, in_pt, out_pt);
	bu_semaphore_release(rd->sem);
    }
}


struct check_exp_air_rd {
    FILE *plot_file;
    int   expAir_color[3];
    int   sem;
};

static void
check_exp_air_render_fn_impl(const char *UNUSED(region_name),
			     point_t in_pt, point_t out_pt, void *data)
{
    struct check_exp_air_rd *rd = (struct check_exp_air_rd *)data;
    if (rd->plot_file) {
	bu_semaphore_acquire(rd->sem);
	pl_color(rd->plot_file, V3ARGS(rd->expAir_color));
	pdv_3line(rd->plot_file, in_pt, out_pt);
	bu_semaphore_release(rd->sem);
    }
}


struct check_unconf_air_rd {
    FILE *plot_file;
    int   unconfAir_color[3];
    int   sem;
};

static void
check_unconf_air_render_fn_impl(const char *UNUSED(r1), const char *UNUSED(r2),
				point_t in_pt, point_t out_pt, void *data)
{
    struct check_unconf_air_rd *rd = (struct check_unconf_air_rd *)data;
    if (rd->plot_file) {
	bu_semaphore_acquire(rd->sem);
	pl_color(rd->plot_file, V3ARGS(rd->unconfAir_color));
	pdv_3line(rd->plot_file, in_pt, out_pt);
	bu_semaphore_release(rd->sem);
    }
}


int ged_check_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int opt_argc, arg_count;
    const char *cmd = argv[0];
    const char *sub = NULL;
    size_t len;
    int flags = 0;

    struct analyze_config cfg = ANALYZE_CONFIG_INIT_ZERO;
    struct analyze_results *res = NULL;

    /* Per-event render hook storage (stack-allocated; pointers into cfg). */
    struct check_overlap_rd   overlap_rd;
    struct check_gap_rd       gap_rd;
    struct check_adj_air_rd   adj_air_rd;
    struct check_exp_air_rd   exp_air_rd;
    struct check_unconf_air_rd unconf_air_rd;

    /* Plot file handles (opened on demand when options.plot_files is set). */
    FILE *plot_overlaps   = NULL;
    FILE *plot_gaps       = NULL;
    FILE *plot_adj_air    = NULL;
    FILE *plot_exp_air    = NULL;
    FILE *plot_unconf_air = NULL;
    FILE *plot_volume     = NULL;

    struct check_parameters options;
    const char *check_subcommands[] = {"adj_air", "bbox", "centroid", "exp_air", "gap",
				       "mass", "moments", "overlaps", "surf_area",
				       "unconf_air", "volume", NULL};
    const cvt_tab *units[3] = {
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
    struct bu_list *vlfree = &rt_vlfree;
    VMOVE(options.units, units);

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

    /* Propagate defaults that libanalyze needs to know about. */
    cfg.use_air = options.use_air = 1;
    cfg.ncpu    = options.ncpu;

    /* shift to subcommand args */
    argc -= opt_argc;
    argv = &argv[opt_argc];

    arg_count = parse_check_args(gedp, argc, (char **)argv, &options, &cfg);

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

    /* ------------------------------------------------------------------
     * Determine analysis flags from the subcommand name.
     * ------------------------------------------------------------------ */
    sub = argv[0];
    len = strlen(sub);
    if (bu_strncmp(sub, "adj_air", len) == 0) {
	flags = ANALYZE_ADJ_AIR;
    } else if (bu_strncmp(sub, "bbox", len) == 0) {
	flags = ANALYZE_BOX;
    } else if (bu_strncmp(sub, "centroid", len) == 0) {
	flags = ANALYZE_MASS | ANALYZE_CENTROIDS;
    } else if (bu_strncmp(sub, "exp_air", len) == 0) {
	flags = ANALYZE_EXP_AIR;
    } else if (bu_strncmp(sub, "gap", len) == 0) {
	flags = ANALYZE_GAP;
    } else if (bu_strncmp(sub, "mass", len) == 0) {
	flags = ANALYZE_MASS;
    } else if (bu_strncmp(sub, "moments", len) == 0) {
	flags = ANALYZE_MASS | ANALYZE_CENTROIDS | ANALYZE_MOMENTS;
    } else if (bu_strncmp(sub, "overlaps", len) == 0) {
	flags = ANALYZE_OVERLAPS;
    } else if (bu_strncmp(sub, "surf_area", len) == 0) {
	flags = ANALYZE_SURF_AREA;
    } else if (bu_strncmp(sub, "unconf_air", len) == 0) {
	flags = ANALYZE_UNCONF_AIR;
    } else if (bu_strncmp(sub, "volume", len) == 0) {
	flags = ANALYZE_VOLUME;
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!", cmd, sub);
	error = 1;
	goto freemem;
    }

    /* ------------------------------------------------------------------
     * VIEW_PLANE sampler: populate cfg from the current GED view for the
     * overlaps subcommand when -i was given.
     * ------------------------------------------------------------------ */
    if (options.getfromview && bu_strncmp(sub, "overlaps", len) == 0) {
	if (!gedp->ged_gvp) {
	    bu_vls_printf(gedp->ged_result_str, "no view active; -i requires a view\n");
	    error = 1;
	    goto freemem;
	}
	{
	    point_t eye_model;
	    quat_t quat;
	    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);
	    _ged_rt_set_eye_model(gedp, eye_model);
	    cfg.sampler   = ANALYZE_SAMPLER_VIEW_PLANE;
	    cfg.view_size = gedp->ged_gvp->gv_size;
	    VMOVE(cfg.view_eye, eye_model);
	    HMOVE(cfg.view_quat, quat);
	}
    }

    /* ------------------------------------------------------------------
     * Open plot files and install render hooks for subcommands that need
     * per-segment drawing.
     * ------------------------------------------------------------------ */
    if (bu_strncmp(sub, "overlaps", len) == 0) {
	memset(&overlap_rd, 0, sizeof(overlap_rd));
	overlap_rd.overlap_color[0] = 255;
	overlap_rd.overlap_color[1] = 255;
	overlap_rd.overlap_color[2] = 0; /* yellow */
	overlap_rd.overlay_flag     = options.overlaps_overlay_flag;
	overlap_rd.rpt_overlap_flag = options.rpt_overlap_flag;
	overlap_rd.gedp             = gedp;
	overlap_rd.sem = bu_semaphore_register("check_overlap_rd");

	if (options.overlaps_overlay_flag) {
	    overlap_rd.vbp   = bv_vlblock_init(vlfree, 32);
	    overlap_rd.vhead = bv_vlblock_find(overlap_rd.vbp, 0xFF, 0xFF, 0x00);
	}
	if (options.plot_files) {
	    plot_overlaps = fopen("overlaps.plot3", "wb");
	    if (!plot_overlaps)
		bu_vls_printf(gedp->ged_result_str, "cannot open overlaps.plot3\n");
	    overlap_rd.plot_file = plot_overlaps;
	}

	cfg.overlap_render      = check_overlap_render_fn_impl;
	cfg.overlap_render_data = &overlap_rd;

    } else if (bu_strncmp(sub, "gap", len) == 0) {
	memset(&gap_rd, 0, sizeof(gap_rd));
	gap_rd.gap_color[0] = 128;
	gap_rd.gap_color[1] = 192;
	gap_rd.gap_color[2] = 255; /* cyan */
	gap_rd.sem = bu_semaphore_register("check_gap_rd");
	if (options.plot_files) {
	    plot_gaps = fopen("gaps.plot3", "wb");
	    if (!plot_gaps)
		bu_vls_printf(gedp->ged_result_str, "cannot open gaps.plot3\n");
	    gap_rd.plot_file = plot_gaps;
	}
	cfg.gap_render      = check_gap_render_fn_impl;
	cfg.gap_render_data = &gap_rd;

    } else if (bu_strncmp(sub, "adj_air", len) == 0) {
	memset(&adj_air_rd, 0, sizeof(adj_air_rd));
	adj_air_rd.adjAir_color[0] = 128;
	adj_air_rd.adjAir_color[1] = 255;
	adj_air_rd.adjAir_color[2] = 192; /* pale green */
	adj_air_rd.sem = bu_semaphore_register("check_adj_air_rd");
	if (options.plot_files) {
	    plot_adj_air = fopen("adj_air.plot3", "wb");
	    if (!plot_adj_air)
		bu_vls_printf(gedp->ged_result_str, "cannot open adj_air.plot3\n");
	    adj_air_rd.plot_file = plot_adj_air;
	}
	cfg.adj_air_render      = check_adj_air_render_fn_impl;
	cfg.adj_air_render_data = &adj_air_rd;

    } else if (bu_strncmp(sub, "exp_air", len) == 0) {
	memset(&exp_air_rd, 0, sizeof(exp_air_rd));
	exp_air_rd.expAir_color[0] = 255;
	exp_air_rd.expAir_color[1] = 128;
	exp_air_rd.expAir_color[2] = 255; /* magenta */
	exp_air_rd.sem = bu_semaphore_register("check_exp_air_rd");
	if (options.plot_files) {
	    plot_exp_air = fopen("exp_air.plot3", "wb");
	    if (!plot_exp_air)
		bu_vls_printf(gedp->ged_result_str, "cannot open exp_air.plot3\n");
	    exp_air_rd.plot_file = plot_exp_air;
	}
	cfg.exp_air_render      = check_exp_air_render_fn_impl;
	cfg.exp_air_render_data = &exp_air_rd;

    } else if (bu_strncmp(sub, "unconf_air", len) == 0) {
	memset(&unconf_air_rd, 0, sizeof(unconf_air_rd));
	unconf_air_rd.unconfAir_color[0] = 0;
	unconf_air_rd.unconfAir_color[1] = 255;
	unconf_air_rd.unconfAir_color[2] = 0; /* green */
	unconf_air_rd.sem = bu_semaphore_register("check_unconf_air_rd");
	if (options.plot_files) {
	    plot_unconf_air = fopen("unconf_air.plot3", "wb");
	    if (!plot_unconf_air)
		bu_vls_printf(gedp->ged_result_str, "cannot open unconf_air.plot3\n");
	    unconf_air_rd.plot_file = plot_unconf_air;
	}
	cfg.unconf_air_render      = check_unconf_air_render_fn_impl;
	cfg.unconf_air_render_data = &unconf_air_rd;

    } else if (bu_strncmp(sub, "volume", len) == 0) {
	if (options.plot_files) {
	    plot_volume = fopen("volume.plot3", "wb");
	    if (!plot_volume)
		bu_vls_printf(gedp->ged_result_str, "cannot open volume.plot3\n");
	    cfg.volume_plot_file = plot_volume;
	}
    }

    /* ------------------------------------------------------------------
     * Run the analysis via libanalyze.
     * ------------------------------------------------------------------ */
    res = analyze_run(&cfg, gedp->dbip, tobjtab, tnobjs, flags);
    if (!res) {
	bu_vls_printf(gedp->ged_result_str, "analysis failed\n");
	error = 1;
	goto cleanup_plots;
    }

    /* ------------------------------------------------------------------
     * Emit verbose / debug log to result string.
     * ------------------------------------------------------------------ */
    print_verbose_debug(gedp, &options);

    /* ------------------------------------------------------------------
     * Dispatch to the appropriate formatter.
     * ------------------------------------------------------------------ */
    if (bu_strncmp(sub, "adj_air", len) == 0) {
	if (check_format_adj_air(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "bbox", len) == 0) {
	if (check_format_bbox(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "centroid", len) == 0) {
	if (check_format_centroid(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "exp_air", len) == 0) {
	if (check_format_exp_air(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "gap", len) == 0) {
	if (check_format_gap(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "mass", len) == 0) {
	if (check_format_mass(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "moments", len) == 0) {
	if (check_format_moments(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "overlaps", len) == 0) {
	if (check_format_overlaps(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "surf_area", len) == 0) {
	if (check_format_surf_area(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "unconf_air", len) == 0) {
	if (check_format_unconf_air(gedp, res, &options)) error = 1;
    } else if (bu_strncmp(sub, "volume", len) == 0) {
	if (check_format_volume(gedp, res, &options)) error = 1;
    }

    analyze_results_free(res);
    res = NULL;

    /* ------------------------------------------------------------------
     * Commit overlay geometry (overlaps -o).
     * ------------------------------------------------------------------ */
    if (bu_strncmp(sub, "overlaps", len) == 0 && options.overlaps_overlay_flag
	&& overlap_rd.vbp) {
	if (gedp->new_cmd_forms) {
	    struct bview *view = gedp->ged_gvp;
	    bv_vlblock_obj(overlap_rd.vbp, view, "check::overlaps");
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, overlap_rd.vbp, "OVERLAPS", 0);
	}
	bv_vlblock_free(overlap_rd.vbp);
	overlap_rd.vbp = NULL;
    }

cleanup_plots:
    /* Close all plot files and emit names. */
    if (plot_overlaps) {
	fclose(plot_overlaps);
	bu_vls_printf(gedp->ged_result_str, "\nplot file saved as overlaps.plot3");
    }
    if (plot_gaps) {
	fclose(plot_gaps);
	bu_vls_printf(gedp->ged_result_str, "\nplot file saved as gaps.plot3");
    }
    if (plot_adj_air) {
	fclose(plot_adj_air);
	bu_vls_printf(gedp->ged_result_str, "\nplot file saved as adj_air.plot3");
    }
    if (plot_exp_air) {
	fclose(plot_exp_air);
	bu_vls_printf(gedp->ged_result_str, "\nplot file saved as exp_air.plot3");
    }
    if (plot_unconf_air) {
	fclose(plot_unconf_air);
	bu_vls_printf(gedp->ged_result_str, "\nplot file saved as unconf_air.plot3");
    }
    if (plot_volume) {
	fclose(plot_volume);
	bu_vls_printf(gedp->ged_result_str, "\nplot file saved as volume.plot3");
    }

freemem:
    if (res) {
	analyze_results_free(res);
	res = NULL;
    }
    bu_free(tobjtab, "free tobjtab");
    tobjtab = NULL;
    if (options.verbose) bu_vls_free(options.verbose_str);
    if (options.debug) bu_vls_free(options.debug_str);
    return (error) ? BRLCAD_ERROR : BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_CHECK_COMMANDS(X, XID) \
    X(check, ged_check_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_CHECK_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_check", 1, GED_CHECK_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
