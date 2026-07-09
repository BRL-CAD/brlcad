/*                         G Q A . C
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
/** @file libged/gqa.c
 *
 * performs a set of quantitative analyses on geometry.
 *
 * XXX need to look at gap computation
 *
 * plot the points where overlaps start/stop
 *
 * Designed to be a framework for 3d sampling of the geometry volume.
 * TODO: Need to move the sample pattern logic into LIBRT.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <limits.h>			/* home of INT_MAX aka MAXINT */


#include "bu/parallel.h"
#include "bu/getopt.h"
#include "bu/units.h"
#include "vmath.h"
#include "raytrace.h"
#include "bv/plot3.h"
#include "analyze.h"

#include "../ged_private.h"

/* bu_getopt() options */
const char *options = "A:a:de:f:g:Gm:n:N:p:P:qrS:s:t:U:u:vV:W:h?";
const char *options_str = "[-A A|a|b|c|e|g|m|o|p|s|v|w] [-a az] [-d] [-e el] [-f densityFile] [-g spacing|upper,lower|upper-lower] [-G] [-m legacy|rotated|crofton] [-n nhits] [-N nviews] [-p plotPrefix] [-P ncpus] [-q] [-r] [-S nsamples] [-t overlap_tol] [-U useair] [-u len_units vol_units wt_units] [-v] [-V volume_tol] [-W weight_tol]";

#define ANALYSIS_VOLUMES          1
#define ANALYSIS_WEIGHTS          2
#define ANALYSIS_OVERLAPS         4
#define ANALYSIS_ADJ_AIR          8 /* adjacent air */
#define ANALYSIS_GAPS            16 /* space between regions */
#define ANALYSIS_EXP_AIR         32 /* exposed air */
#define ANALYSIS_BBOX            64 /* overall bounding box */
#define ANALYSIS_INTERFACES     128
#define ANALYSIS_CENTROIDS      256
#define ANALYSIS_MOMENTS        512
#define ANALYSIS_PLOT_OVERLAPS 1024
/* Surface area: separate libanalyze pass (distinct bit from the above) */
#define GQA_ANALYSIS_SURF_AREA 2048

/* Sampling method codes */
#define GQA_METHOD_LEGACY  0  /**< original axis-aligned triple grid */
#define GQA_METHOD_ROTATED 1  /**< rotated triple grid using -a/-e orientation */
#define GQA_METHOD_CROFTON 2  /**< Crofton isotropic random sampling */

/* Note: struct parsing requires no space after the commas.  take care
 * when formatting this file.  if the compile breaks here, it means
 * that spaces got inserted incorrectly.
 */
#define COMMA ','

#define DLOG if (state->debug) bu_vls_printf

/**
 * Alias the libbu unit-conversion table type and table so that the
 * struct cstate field 'units' and parse_args() code (which refer to cvt_tab
 * and units_tab) continue to compile without further change.
 */
typedef struct bu_cvt_tab cvt_tab;
#define units_tab bu_units_tab

/* this table keeps track of the "current" or "user selected units and
 * the associated conversion values
 */
#define LINE 0
#define VOL 1
#define WGT 2

struct cstate {
    struct ged *gedp;

    /* --- Per-invocation options --- */
    int analysis_flags;
    int multiple_analyses;
    double azimuth_deg;
    double elevation_deg;
    char *densityFileName;
    double gridSpacing;
    double gridSpacingLimit;
    char makeOverlapAssemblies;
    size_t require_num_hits;
    int ncpu;
    int max_cpus;
    double Samples_per_model_axis;
    double overlap_tolerance;
    double volume_tolerance;
    double weight_tolerance;
    int print_per_region_stats;
    int max_region_name_len;
    int use_air;
    int num_objects;
    int num_views;
    int verbose;
    int quiet_missed_report;
    int debug;

    const char *plot_prefix;
    FILE *plot_volume;
    FILE *plot_overlaps;
    FILE *plot_adjair;
    FILE *plot_gaps;
    FILE *plot_expair;

    /* Plot VLIST for in-GED overlap display */
    struct bv_vlblock *plot_vbp;
    struct bu_list *plot_vhead;

    /* Units conversion pointers */
    const cvt_tab *units[3];

    /* --- Sampling method --- */
    int analysis_method; /* GQA_METHOD_LEGACY / GQA_METHOD_ROTATED / GQA_METHOD_CROFTON */
};


/* Access to these lists should be in sections
 * of code protected by state->sem_lists
 * (lists now per-invocation inside struct cstate)
 */


/* Default units (also initialised per-invocation in ged_gqa_core) */
static const cvt_tab * const units_tab_defaults[3] = {
    bu_units_tab[BU_UNITS_LENGTH],	/* linear */
    bu_units_tab[BU_UNITS_VOLUME],	/* volume */
    bu_units_tab[BU_UNITS_MASS]		/* weight */
};

/**
 * Translate a bare length-unit name to its cubic equivalent for the volume
 * table.  This preserves the historical gqa "-u ,m," convention (meaning m^3)
 * without polluting the public libbu volume table with length-unit aliases.
 */
static const char *
gqa_vol_unit_name(const char *name)
{
    if (BU_STR_EQUAL(name, "mm"))    return "mm^3";
    if (BU_STR_EQUAL(name, "cm"))    return "cm^3";
    if (BU_STR_EQUAL(name, "m"))     return "m^3";
    if (BU_STR_EQUAL(name, "in"))    return "in^3";
    if (BU_STR_EQUAL(name, "ft"))    return "ft^3";
    if (BU_STR_EQUAL(name, "yd") || BU_STR_EQUAL(name, "yds") || BU_STR_EQUAL(name, "yards"))
	return "yds^3";
    return name;
}

/**
 * Thin wrapper around bu_units_parse_double() that forwards error
 * messages to gedp->ged_result_str.  The parse_args() body uses
 * _gqa_read_units_double() by name; this bridges the old call sites to the
 * shared libbu implementation.
 */
static int
_gqa_read_units_double(struct ged *gedp, double *val, char *buf, const cvt_tab *cvt)
{
    return bu_units_parse_double(gedp->ged_result_str, val, buf, cvt);
}


/**
 * Parse through command line flags
 */
static int
parse_args(struct ged *gedp, struct cstate *state, int ac, char *av[])
{
    int c;
    int i;
    double a;
    char *p;

    /* Turn off getopt's error messages */
    bu_opterr = 0;
    bu_optind = 1;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	switch (c) {
	    case 'A':
		{
		    state->analysis_flags = 0;
		    state->multiple_analyses = 0;
		    for (p = bu_optarg; *p; p++) {
			switch (*p) {
			    case 'A' :
				state->multiple_analyses = 1;
				state->analysis_flags = state->analysis_flags \
				| ANALYSIS_ADJ_AIR \
				| ANALYSIS_BBOX \
				| ANALYSIS_CENTROIDS \
				| ANALYSIS_EXP_AIR \
				| ANALYSIS_GAPS \
				| ANALYSIS_MOMENTS \
				| ANALYSIS_OVERLAPS \
				| ANALYSIS_VOLUMES \
				| ANALYSIS_WEIGHTS \
				| GQA_ANALYSIS_SURF_AREA;
				break;
			    case 'a' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_ADJ_AIR;

				break;
			    case 'b' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_BBOX;

				break;
			    case 'c' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_WEIGHTS;
				state->analysis_flags |= ANALYSIS_CENTROIDS;

				break;
			    case 'e' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_EXP_AIR;
				break;
			    case 'g' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_GAPS;
				break;
			    case 'm' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_WEIGHTS;
				state->analysis_flags |= ANALYSIS_CENTROIDS;
				state->analysis_flags |= ANALYSIS_MOMENTS;

				break;
			    case 'o' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_OVERLAPS;
				break;
			    case 'p' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_OVERLAPS;
				state->analysis_flags |= ANALYSIS_PLOT_OVERLAPS;
				break;
			    case 'v' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_VOLUMES;
				break;
			    case 'w' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= ANALYSIS_WEIGHTS;
				break;
			    case 's' :
				if (state->analysis_flags)
				    state->multiple_analyses = 1;

				state->analysis_flags |= GQA_ANALYSIS_SURF_AREA;
				break;
			    default:
				bu_vls_printf(gedp->ged_result_str, "Unknown analysis type \"%c\" requested.\n", *p);
				return -1;
			}
		    }
		    break;
		}
	    case 'a':
		if (bn_decode_angle(&state->azimuth_deg, bu_optarg) == 0) {
		    bu_vls_printf(gedp->ged_result_str, "error parsing azimuth \"%s\"\n", bu_optarg);
		    return -1;
		}
		/* Switch to rotated-grid mode when az/el are specified */
		if (state->analysis_method == GQA_METHOD_LEGACY)
		    state->analysis_method = GQA_METHOD_ROTATED;
		break;
	    case 'e':
		if (bn_decode_angle(&state->elevation_deg, bu_optarg) == 0) {
		    bu_vls_printf(gedp->ged_result_str, "error parsing elevation \"%s\"\n", bu_optarg);
		    return -1;
		}
		/* Switch to rotated-grid mode when az/el are specified */
		if (state->analysis_method == GQA_METHOD_LEGACY)
		    state->analysis_method = GQA_METHOD_ROTATED;
		break;
	    case 'd': state->debug = 1; break;

	    case 'f': state->densityFileName = bu_optarg; break;

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


		    if (_gqa_read_units_double(gedp, &value1, bu_optarg, units_tab[0])) {
			bu_vls_printf(gedp->ged_result_str, "error parsing grid spacing value \"%s\"\n", bu_optarg);
			return -1;
		    }

		    if (p) {
			/* we've got 2 values, they are upper limit
			 * and lower limit.
			 */
			if (_gqa_read_units_double(gedp, &value2, p, units_tab[0])) {
			    bu_vls_printf(gedp->ged_result_str, "error parsing grid spacing limit value \"%s\"\n", p);
			    return -1;
			}

			state->gridSpacing = value1;
			state->gridSpacingLimit = value2;
		    } else {
			state->gridSpacingLimit = value1;

			state->gridSpacing = 0.0; /* flag it */
		    }
		    break;
		}
	    case 'G':
		state->makeOverlapAssemblies = 1;
		bu_vls_printf(gedp->ged_result_str, "-G option unimplemented\n");
		return -1;
	    case 'm':
		/* method selection: legacy, rotated, crofton */
		if (BU_STR_EQUAL(bu_optarg, "legacy")) {
		    state->analysis_method = GQA_METHOD_LEGACY;
		} else if (BU_STR_EQUAL(bu_optarg, "rotated")) {
		    state->analysis_method = GQA_METHOD_ROTATED;
		} else if (BU_STR_EQUAL(bu_optarg, "crofton")) {
		    state->analysis_method = GQA_METHOD_CROFTON;
		} else {
		    bu_vls_printf(gedp->ged_result_str, "unknown method \"%s\"; valid: legacy, rotated, crofton\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'n':
		if (sscanf(bu_optarg, "%d", &c) != 1 || c < 0) {
		    bu_vls_printf(gedp->ged_result_str, "num_hits must be integer value >= 0, not \"%s\"\n", bu_optarg);
		    return -1;
		}

		state->require_num_hits = (size_t)c;
		break;

	    case 'N':
		state->num_views = atoi(bu_optarg);
		break;
	    case 'p':
		state->plot_prefix = bu_optarg;
		break;
	    case 'P':
		/* cannot ask for more cpu's than the machine has */
		c = atoi(bu_optarg);
		if (c > 0 && c <= state->max_cpus)
		    state->ncpu = c;
		break;
	    case 'q':
		state->quiet_missed_report = 1;
		break;
	    case 'r':
		state->print_per_region_stats = 1;
		break;
	    case 'S':
		if (sscanf(bu_optarg, "%lg", &a) != 1 || a <= 1.0) {
		    bu_vls_printf(gedp->ged_result_str, "error in specifying minimum samples per model axis: \"%s\"\n", bu_optarg);
		    break;
		}
		state->Samples_per_model_axis = a + 1;
		break;
	    case 't':
		if (_gqa_read_units_double(gedp, &state->overlap_tolerance, bu_optarg, units_tab[0])) {
		    bu_vls_printf(gedp->ged_result_str, "error in overlap tolerance distance \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'v':
		state->verbose = 1;
		break;
	    case 'V':
		if (_gqa_read_units_double(gedp, &state->volume_tolerance, bu_optarg, units_tab[1])) {
		    bu_vls_printf(gedp->ged_result_str, "error in volume tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'W':
		if (_gqa_read_units_double(gedp, &state->weight_tolerance, bu_optarg, units_tab[2])) {
		    bu_vls_printf(gedp->ged_result_str, "error in weight tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;

	    case 'U':
		errno = 0;
		state->use_air = strtol(bu_optarg, (char **)NULL, 10);
		if (errno == ERANGE || errno == EINVAL) {
		    bu_vls_printf(gedp->ged_result_str, "error in air argument %s\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'u':
		{
		    char *ptr = bu_optarg;
		    const cvt_tab *cv;
		    static const char *dim[3] = {"length", "volume", "weight"};
		    char *units_name[3] = {NULL, NULL, NULL};
		    char **units_ap;

		    /* fill in units_name with the names we parse out */
		    units_ap = units_name;

		    /* acquire unit names */
		    for (i = 0; i < 3 && ptr; i++) {
			int found_unit;
			const char *lookup_name;

			if (i == 0) {
			    *units_ap = strtok(ptr, CPP_XSTR(COMMA));
			} else {
			    *units_ap = strtok(NULL, CPP_XSTR(COMMA));
			}

			/* got something? */
			if (*units_ap == NULL)
			    break;

			/* translate bare length names to cubic equivalents for volume */
			lookup_name = (i == VOL) ? gqa_vol_unit_name(units_name[i]) : units_name[i];

			/* got something valid? */
			found_unit = 0;
			for (cv = &units_tab[i][0]; cv->name[0] != '\0'; cv++) {
			    if (lookup_name && BU_STR_EQUAL(cv->name, lookup_name)) {
				state->units[i] = cv;
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
			bu_vls_printf(gedp->ged_result_str, " %s: %s", dim[i], state->units[i]->name);
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

/* ======================================================================
 * Per-event render hook callbacks for plot3 files and vlblock overlay.
 * ====================================================================== */

/**
 * Combined context for all per-event render hooks.
 *
 * The sem field serialises concurrent plot3 writes and vlist edits.
 * Either or both of plot_fp / vhead may be NULL, in which case that
 * output channel is skipped.
 */
struct gqa_render_ctx {
    int sem;
    FILE *plot_fp;
    int   plot_color[3];
    struct bu_list *vhead;
    struct bu_list *vlfree;
};


static void
gqa_overlap_render(const char *UNUSED(r1), const char *UNUSED(r2),
		   double UNUSED(depth), point_t ihit, point_t ohit, void *data)
{
    struct gqa_render_ctx *ctx = (struct gqa_render_ctx *)data;
    bu_semaphore_acquire(ctx->sem);
    if (ctx->plot_fp) {
	pl_color(ctx->plot_fp, ctx->plot_color[0], ctx->plot_color[1], ctx->plot_color[2]);
	pdv_3line(ctx->plot_fp, ihit, ohit);
    }
    if (ctx->vhead) {
	BV_ADD_VLIST(ctx->vlfree, ctx->vhead, ihit, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(ctx->vlfree, ctx->vhead, ohit, BV_VLIST_LINE_DRAW);
    }
    bu_semaphore_release(ctx->sem);
}


static void
gqa_gap_render(const char *UNUSED(r1), const char *UNUSED(r2),
	       double UNUSED(dist), point_t gap_start, point_t gap_end, void *data)
{
    struct gqa_render_ctx *ctx = (struct gqa_render_ctx *)data;
    bu_semaphore_acquire(ctx->sem);
    if (ctx->plot_fp) {
	pl_color(ctx->plot_fp, ctx->plot_color[0], ctx->plot_color[1], ctx->plot_color[2]);
	pdv_3line(ctx->plot_fp, gap_start, gap_end);
    }
    bu_semaphore_release(ctx->sem);
}


static void
gqa_adj_air_render(const char *UNUSED(solid), const char *UNUSED(air),
		   point_t in_pt, point_t out_pt, void *data)
{
    struct gqa_render_ctx *ctx = (struct gqa_render_ctx *)data;
    bu_semaphore_acquire(ctx->sem);
    if (ctx->plot_fp) {
	pl_color(ctx->plot_fp, ctx->plot_color[0], ctx->plot_color[1], ctx->plot_color[2]);
	pdv_3line(ctx->plot_fp, in_pt, out_pt);
    }
    bu_semaphore_release(ctx->sem);
}


static void
gqa_exp_air_render(const char *UNUSED(name),
		   point_t in_pt, point_t out_pt, void *data)
{
    struct gqa_render_ctx *ctx = (struct gqa_render_ctx *)data;
    bu_semaphore_acquire(ctx->sem);
    if (ctx->plot_fp) {
	pl_color(ctx->plot_fp, ctx->plot_color[0], ctx->plot_color[1], ctx->plot_color[2]);
	pdv_3line(ctx->plot_fp, in_pt, out_pt);
    }
    bu_semaphore_release(ctx->sem);
}


/* ======================================================================
 * summary_reports - print analysis results from analyze_results.
 * ====================================================================== */

static void
summary_reports(struct ged *gedp, struct cstate *state,
		struct analyze_results *res)
{
    int i;
    size_t k;
    double units2 = state->units[LINE]->val * state->units[LINE]->val;
    double units3 = units2 * state->units[LINE]->val;
    double gs = res->final_grid_spacing;

    if (gs <= 0.0) gs = 0.0; /* Crofton: no grid spacing */

    if (state->multiple_analyses)
	bu_vls_printf(gedp->ged_result_str, "Summaries (%gmm grid spacing):\n", gs);
    else
	bu_vls_printf(gedp->ged_result_str, "Summary (%gmm grid spacing):\n", gs);

    /* -- Weights -- */
    if (state->analysis_flags & ANALYSIS_WEIGHTS) {
	bu_vls_printf(gedp->ged_result_str, "Weight:\n");
	for (i = 0; i < (int)res->n_objects; i++) {
	    bu_vls_printf(gedp->ged_result_str,
			  "\t%*s %g %s\n",
			  -state->max_region_name_len,
			  res->objects[i].name,
			  res->objects[i].mass / state->units[WGT]->val,
			  state->units[WGT]->name);

	    if ((state->analysis_flags & ANALYSIS_CENTROIDS) &&
		!ZERO(res->objects[i].mass)) {
		bu_vls_printf(gedp->ged_result_str,
			      "\t\tcentroid: (%g %g %g) mm\n",
			      V3ARGS(res->objects[i].centroid));
		if (state->analysis_flags & ANALYSIS_MOMENTS) {
		    struct bu_vls title = BU_VLS_INIT_ZERO;
		    bu_vls_printf(&title,
				  "For the Moments and Products of Inertia For %s",
				  res->objects[i].name);
		    bn_mat_print_vls(bu_vls_addr(&title),
				     res->objects[i].moments_of_inertia,
				     gedp->ged_result_str);
		    bu_vls_free(&title);
		}
	    }
	}

	if (state->print_per_region_stats) {
	    bu_vls_printf(gedp->ged_result_str, "\tregions:\n");
	    for (i = 0; i < (int)res->n_regions; i++) {
		bu_vls_printf(gedp->ged_result_str,
			      "\t%s %g %s\n",
			      res->regions[i].name,
			      res->regions[i].mass / state->units[WGT]->val,
			      state->units[WGT]->name);
	    }
	}

	bu_vls_printf(gedp->ged_result_str,
		      "  Average total weight: %g %s\n",
		      res->total_mass / state->units[WGT]->val,
		      state->units[WGT]->name);

	if ((state->analysis_flags & ANALYSIS_CENTROIDS) &&
	    !ZERO(res->total_mass)) {
	    bu_vls_printf(gedp->ged_result_str,
			  "  Average centroid: (%g %g %g) mm\n",
			  V3ARGS(res->centroid));
	    if (state->analysis_flags & ANALYSIS_MOMENTS)
		bn_mat_print_vls(
		    "For the Moments and Products of Inertia For\n\tAll Specified Objects",
		    res->moments_of_inertia, gedp->ged_result_str);
	}
    }

    /* -- Volumes -- */
    if (state->analysis_flags & ANALYSIS_VOLUMES) {
	bu_vls_printf(gedp->ged_result_str, "Volume:\n");
	for (i = 0; i < (int)res->n_objects; i++) {
	    bu_vls_printf(gedp->ged_result_str,
			  "\t%*s %g %s\n",
			  -state->max_region_name_len,
			  res->objects[i].name,
			  res->objects[i].volume / units3,
			  state->units[LINE]->name);
	}

	if (state->print_per_region_stats) {
	    bu_vls_printf(gedp->ged_result_str, "\tregions:\n");
	    for (i = 0; i < (int)res->n_regions; i++) {
		bu_vls_printf(gedp->ged_result_str,
			      "\t%s volume:%g %s\n",
			      res->regions[i].name,
			      res->regions[i].volume / units3,
			      state->units[LINE]->name);
	    }
	}

	bu_vls_printf(gedp->ged_result_str,
		      "  Average total volume: %g %s\n",
		      res->total_volume / units3,
		      state->units[LINE]->name);
    }

    /* -- Surface area -- */
    if (state->analysis_flags & GQA_ANALYSIS_SURF_AREA) {
	bu_vls_printf(gedp->ged_result_str, "Surface Area:\n");
	for (i = 0; i < (int)res->n_objects; i++) {
	    bu_vls_printf(gedp->ged_result_str,
			  "\t%*s %g %s^2\n",
			  -state->max_region_name_len,
			  res->objects[i].name,
			  res->objects[i].surf_area / units2,
			  state->units[LINE]->name);
	}

	if (state->print_per_region_stats) {
	    bu_vls_printf(gedp->ged_result_str, "\tregions:\n");
	    for (i = 0; i < (int)res->n_regions; i++) {
		bu_vls_printf(gedp->ged_result_str,
			      "\t%s surf_area:%g %s^2\n",
			      res->regions[i].name,
			      res->regions[i].surf_area / units2,
			      state->units[LINE]->name);
	    }
	}

	bu_vls_printf(gedp->ged_result_str,
		      "  Average total surface area: %g %s^2\n",
		      res->total_surf_area / units2,
		      state->units[LINE]->name);
    }

    /* -- Issue lists -- */
    if (state->analysis_flags & ANALYSIS_OVERLAPS) {
	size_t nov = BU_PTBL_LEN(&res->overlaps);
	if (nov == 0) {
	    bu_vls_printf(gedp->ged_result_str, "No Overlaps\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "list Overlaps:\n");
	    for (k = 0; k < nov; k++) {
		struct analyze_overlap_record *ov =
		    (struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, k);
		bu_vls_printf(gedp->ged_result_str,
			      "%s %s count:%lu dist:%g%s vol~:%g%s^3 @ (%g %g %g)\n",
			      ov->region1,
			      ov->region2 ? ov->region2 : "?",
			      ov->count,
			      ov->max_dist / state->units[LINE]->val,
			      state->units[LINE]->name,
			      ov->estimated_volume / units3,
			      state->units[LINE]->name,
			      V3ARGS(ov->coord));
	    }
	}
    }

    if (state->analysis_flags & ANALYSIS_ADJ_AIR) {
	size_t n = BU_PTBL_LEN(&res->adj_air);
	if (n == 0) {
	    bu_vls_printf(gedp->ged_result_str, "No Adjacent Air\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "list Adjacent Air:\n");
	    for (k = 0; k < n; k++) {
		struct analyze_overlap_record *rec =
		    (struct analyze_overlap_record *)BU_PTBL_GET(&res->adj_air, k);
		bu_vls_printf(gedp->ged_result_str,
			      "%s %s count:%lu dist:%g%s @ (%g %g %g)\n",
			      rec->region1,
			      rec->region2 ? rec->region2 : "?",
			      rec->count,
			      rec->max_dist / state->units[LINE]->val,
			      state->units[LINE]->name,
			      V3ARGS(rec->coord));
	    }
	}
    }

    if (state->analysis_flags & ANALYSIS_GAPS) {
	size_t n = BU_PTBL_LEN(&res->gaps);
	if (n == 0) {
	    bu_vls_printf(gedp->ged_result_str, "No Gaps\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "list Gaps:\n");
	    for (k = 0; k < n; k++) {
		struct analyze_overlap_record *rec =
		    (struct analyze_overlap_record *)BU_PTBL_GET(&res->gaps, k);
		bu_vls_printf(gedp->ged_result_str,
			      "%s %s count:%lu dist:%g%s @ (%g %g %g)\n",
			      rec->region1,
			      rec->region2 ? rec->region2 : "?",
			      rec->count,
			      rec->max_dist / state->units[LINE]->val,
			      state->units[LINE]->name,
			      V3ARGS(rec->coord));
	    }
	}
    }

    if (state->analysis_flags & ANALYSIS_EXP_AIR) {
	size_t n = BU_PTBL_LEN(&res->exp_air);
	if (n == 0) {
	    bu_vls_printf(gedp->ged_result_str, "No Exposed Air\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "list Exposed Air:\n");
	    for (k = 0; k < n; k++) {
		struct analyze_overlap_record *rec =
		    (struct analyze_overlap_record *)BU_PTBL_GET(&res->exp_air, k);
		bu_vls_printf(gedp->ged_result_str,
			      "%s count:%lu dist:%g%s @ (%g %g %g)\n",
			      rec->region1,
			      rec->count,
			      rec->max_dist / state->units[LINE]->val,
			      state->units[LINE]->name,
			      V3ARGS(rec->coord));
	    }
	}
    }

    /* -- Bounding box -- */
    if (state->analysis_flags & ANALYSIS_BBOX) {
	bu_vls_printf(gedp->ged_result_str,
		      "bounding box: %g %g %g  %g %g %g\n",
		      V3ARGS(res->bbox_min), V3ARGS(res->bbox_max));
    }

    /* -- Region hit-count report -- */
    for (i = 0; i < (int)res->n_regions; i++) {
	unsigned long hits = res->regions[i].hits;
	if (hits < state->require_num_hits) {
	    if (hits == 0 && !state->quiet_missed_report) {
		/* Suppress if the region appears in the overlap list
		 * (it was hit during overlap detection, just not a
		 * normal through-shot). */
		int is_overlap_only = 0;
		if (state->analysis_flags & ANALYSIS_OVERLAPS) {
		    for (k = 0; k < BU_PTBL_LEN(&res->overlaps); k++) {
			struct analyze_overlap_record *ov =
			    (struct analyze_overlap_record *)
			    BU_PTBL_GET(&res->overlaps, k);
			if (BU_STR_EQUAL(ov->region1, res->regions[i].name) ||
			    (ov->region2 &&
			     BU_STR_EQUAL(ov->region2, res->regions[i].name))) {
			    is_overlap_only = 1;
			    break;
			}
		    }
		}
		if (!is_overlap_only)
		    bu_vls_printf(gedp->ged_result_str,
				  "%s was not hit\n", res->regions[i].name);
	    } else if (hits > 0) {
		bu_vls_printf(gedp->ged_result_str,
			      "%s hit only %lu times (< %zu)\n",
			      res->regions[i].name, hits,
			      state->require_num_hits);
	    }
	}
    }
}


extern "C" int
ged_gqa_core(struct ged *gedp, int argc, const char *argv[])
{
    int arg_count;
    int i;
    int n_objs;
    int ar_flags;
    struct cstate state_val;
    struct cstate *state = &state_val;
    struct analyze_config cfg;
    struct analyze_results *res;
    char **names;
    struct bu_list *vlfree = &rt_vlfree;
    static const char *usage = "object [object ...]";

    /* Render hook context objects (one per issue type). */
    struct gqa_render_ctx ov_ctx;
    struct gqa_render_ctx gap_ctx;
    struct gqa_render_ctx adj_ctx;
    struct gqa_render_ctx exp_ctx;
    int render_sem;

    memset(state, 0, sizeof(*state));
    state->gedp = gedp;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s",
		      argv[0], options_str, usage);
	return GED_HELP;
    }

    /* --- Default options --- */
    state->analysis_flags =
	ANALYSIS_VOLUMES | ANALYSIS_OVERLAPS | ANALYSIS_WEIGHTS |
	ANALYSIS_EXP_AIR | ANALYSIS_ADJ_AIR  | ANALYSIS_GAPS    |
	ANALYSIS_CENTROIDS | ANALYSIS_MOMENTS | GQA_ANALYSIS_SURF_AREA;
    state->multiple_analyses = 1;
    state->azimuth_deg   = 0.0;
    state->elevation_deg = 0.0;
    state->densityFileName   = (char *)0;
    state->gridSpacing       = 50.0;
    state->gridSpacingLimit  = 10.0 * wdbp->wdb_tol.dist;
    state->makeOverlapAssemblies = 0;
    state->require_num_hits  = 1;
    state->max_cpus = state->ncpu = (int)bu_avail_cpus();
    state->Samples_per_model_axis = 2.0;
    state->overlap_tolerance = 0.0;
    state->volume_tolerance  = -1.0;
    state->weight_tolerance  = -1.0;
    state->print_per_region_stats = 0;
    state->max_region_name_len    = 0;
    state->use_air      = 1;
    state->num_objects  = 0;
    state->num_views    = 3;
    state->verbose      = 0;
    state->quiet_missed_report = 0;
    state->plot_prefix  = NULL;
    state->plot_volume   = (FILE *)0;
    state->plot_overlaps = (FILE *)0;
    state->plot_adjair   = (FILE *)0;
    state->plot_gaps     = (FILE *)0;
    state->plot_expair   = (FILE *)0;
    state->debug         = 0;
    state->analysis_method = GQA_METHOD_LEGACY;

    state->units[LINE] = units_tab_defaults[LINE];
    state->units[VOL]  = units_tab_defaults[VOL];
    state->units[WGT]  = units_tab_defaults[WGT];

    /* --- Parse args --- */
    arg_count = parse_args(gedp, state, argc, (char **)argv);
    if (arg_count < 0 || (argc - arg_count) < 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s",
		      argv[0], options_str, usage);
	return BRLCAD_ERROR;
    }

    n_objs = argc - arg_count;
    state->num_objects = n_objs;

    /* --- Build names array for analyze_run() --- */
    names = (char **)bu_calloc(n_objs, sizeof(char *), "gqa names");
    for (i = 0; i < n_objs; i++)
	names[i] = bu_strdup(argv[arg_count + i]);

    /* --- Open plot files if a prefix was given --- */
    if (state->plot_prefix) {
	struct bu_vls vp = BU_VLS_INIT_ZERO;

	if (state->analysis_flags & ANALYSIS_OVERLAPS) {
	    bu_vls_printf(&vp, "%soverlaps.plot3", state->plot_prefix);
	    bu_log("Plotting overlaps to %s\n", bu_vls_cstr(&vp));
	    state->plot_overlaps = fopen(bu_vls_cstr(&vp), "wb");
	    if (!state->plot_overlaps)
		bu_vls_printf(gedp->ged_result_str,
			      "cannot open plot file %s\n", bu_vls_cstr(&vp));
	    bu_vls_trunc(&vp, 0);
	}
	if (state->analysis_flags & ANALYSIS_GAPS) {
	    bu_vls_printf(&vp, "%sgaps.plot3", state->plot_prefix);
	    bu_log("Plotting gaps to %s\n", bu_vls_cstr(&vp));
	    state->plot_gaps = fopen(bu_vls_cstr(&vp), "wb");
	    if (!state->plot_gaps)
		bu_vls_printf(gedp->ged_result_str,
			      "cannot open plot file %s\n", bu_vls_cstr(&vp));
	    bu_vls_trunc(&vp, 0);
	}
	if (state->analysis_flags & ANALYSIS_ADJ_AIR) {
	    bu_vls_printf(&vp, "%sadj_air.plot3", state->plot_prefix);
	    bu_log("Plotting adjacent air to %s\n", bu_vls_cstr(&vp));
	    state->plot_adjair = fopen(bu_vls_cstr(&vp), "wb");
	    if (!state->plot_adjair)
		bu_vls_printf(gedp->ged_result_str,
			      "cannot open plot file %s\n", bu_vls_cstr(&vp));
	    bu_vls_trunc(&vp, 0);
	}
	if (state->analysis_flags & ANALYSIS_EXP_AIR) {
	    bu_vls_printf(&vp, "%sexp_air.plot3", state->plot_prefix);
	    bu_log("Plotting exposed air to %s\n", bu_vls_cstr(&vp));
	    state->plot_expair = fopen(bu_vls_cstr(&vp), "wb");
	    if (!state->plot_expair)
		bu_vls_printf(gedp->ged_result_str,
			      "cannot open plot file %s\n", bu_vls_cstr(&vp));
	    bu_vls_trunc(&vp, 0);
	}

	bu_vls_free(&vp);
    }

    /* --- Validate air-analysis vs. use_air --- */
    if ((state->analysis_flags & (ANALYSIS_ADJ_AIR | ANALYSIS_EXP_AIR)) &&
	!state->use_air) {
	bu_vls_printf(gedp->ged_result_str,
		      "Error: Air regions discarded but air analysis requested!\n"
		      "Set use_air non-zero or eliminate air analysis\n");
	goto gqa_cleanup;
    }

    if (state->print_per_region_stats &&
	!(state->analysis_flags & (ANALYSIS_VOLUMES | ANALYSIS_WEIGHTS)))
	bu_vls_printf(gedp->ged_result_str,
		      "Note: -r option ignored: neither volume nor weight "
		      "options requested\n");

    /* --- Set up vlblock for in-GED overlap overlay --- */
    if (state->analysis_flags & ANALYSIS_PLOT_OVERLAPS) {
	state->plot_vbp   = bv_vlblock_init(vlfree, 32);
	state->plot_vhead = bv_vlblock_find(state->plot_vbp, 0xFF, 0xFF, 0x00);
    }

    /* --- Wire up render hook contexts --- */
    render_sem = bu_semaphore_register("gqa_render_sem");

    memset(&ov_ctx,  0, sizeof(ov_ctx));
    ov_ctx.sem      = render_sem;
    ov_ctx.plot_fp  = state->plot_overlaps;
    ov_ctx.plot_color[0] = 255; ov_ctx.plot_color[1] = 255; ov_ctx.plot_color[2] = 0;
    ov_ctx.vhead    = state->plot_vhead;
    ov_ctx.vlfree   = vlfree;

    memset(&gap_ctx, 0, sizeof(gap_ctx));
    gap_ctx.sem     = render_sem;
    gap_ctx.plot_fp = state->plot_gaps;
    gap_ctx.plot_color[0] = 0; gap_ctx.plot_color[1] = 255; gap_ctx.plot_color[2] = 0;

    memset(&adj_ctx, 0, sizeof(adj_ctx));
    adj_ctx.sem     = render_sem;
    adj_ctx.plot_fp = state->plot_adjair;
    adj_ctx.plot_color[0] = 255; adj_ctx.plot_color[1] = 128; adj_ctx.plot_color[2] = 0;

    memset(&exp_ctx, 0, sizeof(exp_ctx));
    exp_ctx.sem     = render_sem;
    exp_ctx.plot_fp = state->plot_expair;
    exp_ctx.plot_color[0] = 255; exp_ctx.plot_color[1] = 0; exp_ctx.plot_color[2] = 0;

    /* --- Map gqa analysis_flags to libanalyze ANALYZE_* flags --- */
    ar_flags = 0;
    if (state->analysis_flags & ANALYSIS_VOLUMES)        ar_flags |= ANALYZE_VOLUME;
    if (state->analysis_flags & ANALYSIS_WEIGHTS)        ar_flags |= ANALYZE_MASS;
    if (state->analysis_flags & ANALYSIS_OVERLAPS)       ar_flags |= ANALYZE_OVERLAPS;
    if (state->analysis_flags & ANALYSIS_GAPS)           ar_flags |= ANALYZE_GAP;
    if (state->analysis_flags & ANALYSIS_EXP_AIR)        ar_flags |= ANALYZE_EXP_AIR;
    if (state->analysis_flags & ANALYSIS_ADJ_AIR)        ar_flags |= ANALYZE_ADJ_AIR;
    if (state->analysis_flags & ANALYSIS_CENTROIDS)      ar_flags |= ANALYZE_CENTROIDS;
    if (state->analysis_flags & ANALYSIS_MOMENTS)        ar_flags |= ANALYZE_MOMENTS;
    if (state->analysis_flags & GQA_ANALYSIS_SURF_AREA)  ar_flags |= ANALYZE_SURF_AREA;
    if (state->analysis_flags & ANALYSIS_BBOX)           ar_flags |= ANALYZE_BOX;

    /* --- Build analyze_config from parsed options --- */
    cfg = analyze_config_defaults();

    /* Sampler selection */
    switch (state->analysis_method) {
	case GQA_METHOD_CROFTON:
	    cfg.sampler = ANALYZE_SAMPLER_CROFTON;
	    break;
	case GQA_METHOD_ROTATED:
	    cfg.sampler       = ANALYZE_SAMPLER_ROTATED;
	    cfg.azimuth_deg   = state->azimuth_deg;
	    cfg.elevation_deg = state->elevation_deg;
	    break;
	default: /* GQA_METHOD_LEGACY */
	    cfg.sampler = ANALYZE_SAMPLER_TRIPLE_GRID;
	    break;
    }

    cfg.grid_spacing     = state->gridSpacing;
    cfg.grid_spacing_min = state->gridSpacingLimit;
    cfg.samples_per_model_axis = state->Samples_per_model_axis;
    cfg.num_views        = state->num_views;
    cfg.ncpu             = state->ncpu;
    cfg.use_air          = state->use_air;
    cfg.verbose          = state->verbose;
    cfg.overlap_tol      = state->overlap_tolerance;
    cfg.volume_tol       = state->volume_tolerance;
    cfg.mass_tol         = state->weight_tolerance;
    cfg.required_hits    = (int)state->require_num_hits;
    cfg.quiet_missed     = state->quiet_missed_report;
    if (state->densityFileName)
	cfg.density_file = state->densityFileName;

    /* Render hooks */
    if (state->analysis_flags & ANALYSIS_OVERLAPS) {
	cfg.overlap_render      = gqa_overlap_render;
	cfg.overlap_render_data = &ov_ctx;
    }
    if (state->analysis_flags & ANALYSIS_GAPS) {
	cfg.gap_render      = gqa_gap_render;
	cfg.gap_render_data = &gap_ctx;
    }
    if (state->analysis_flags & ANALYSIS_ADJ_AIR) {
	cfg.adj_air_render      = gqa_adj_air_render;
	cfg.adj_air_render_data = &adj_ctx;
    }
    if (state->analysis_flags & ANALYSIS_EXP_AIR) {
	cfg.exp_air_render      = gqa_exp_air_render;
	cfg.exp_air_render_data = &exp_ctx;
    }

    /* --- Run the analysis --- */
    res = analyze_run(&cfg, gedp->dbip, names, n_objs, ar_flags);

    if (!res) {
	bu_vls_printf(gedp->ged_result_str,
		      "gqa: analysis failed (analyze_run returned NULL).\n"
		      "Check that the objects exist in the database and that "
		      "a density file is available when mass was requested.\n");
	goto gqa_cleanup;
    }

    /* Compute max_region_name_len for aligned output. */
    for (i = 0; i < (int)res->n_objects; i++) {
	int l = (int)strlen(res->objects[i].name);
	if (l > state->max_region_name_len)
	    state->max_region_name_len = l;
    }

    if (state->verbose)
	bu_vls_printf(gedp->ged_result_str, "Computation Done\n");

    summary_reports(gedp, state, res);

    /* Commit vlblock overlay for in-GED overlap display. */
    if (state->analysis_flags & ANALYSIS_PLOT_OVERLAPS) {
	if (gedp->new_cmd_forms) {
	    struct bview *gview = gedp->ged_gvp;
	    bv_vlblock_obj(state->plot_vbp, gview, "gqa::overlaps");
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, state->plot_vbp, "OVERLAPS", 0);
	}
    }

    analyze_results_free(res);

gqa_cleanup:
    /* Close any open plot files. */
    if (state->plot_overlaps) fclose(state->plot_overlaps);
    if (state->plot_volume)   fclose(state->plot_volume);
    if (state->plot_adjair)   fclose(state->plot_adjair);
    if (state->plot_gaps)     fclose(state->plot_gaps);
    if (state->plot_expair)   fclose(state->plot_expair);

    if (state->analysis_flags & ANALYSIS_PLOT_OVERLAPS)
	bv_vlblock_free(state->plot_vbp);

    for (i = 0; i < n_objs; i++)
	bu_free(names[i], "gqa name");
    bu_free(names, "gqa names");

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_GQA_COMMANDS(X, XID) \
    X(gqa, ged_gqa_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_GQA_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_gqa", 1, GED_GQA_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
