/*                     F A C E T I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize.cpp
 *
 * The facetize command.
 *
 */

#include "common.h"

#include "bu/opt.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

static struct _ged_facetize_logging_state *
logging_state_create()
{
    struct _ged_facetize_logging_state *ls;
    BU_GET(ls, struct _ged_facetize_logging_state);

    BU_ALLOC(ls->saved_log_hooks, struct bu_hook_list);
    bu_hook_list_init(ls->saved_log_hooks);

    BU_ALLOC(ls->saved_bomb_hooks, struct bu_hook_list);
    bu_hook_list_init(ls->saved_bomb_hooks);

    BU_GET(ls->nmg_log, struct bu_vls);
    bu_vls_init(ls->nmg_log);

    BU_GET(ls->nmg_log_header, struct bu_vls);
    bu_vls_init(ls->nmg_log_header);

    return ls;
}

void
logging_state_destroy(struct _ged_facetize_logging_state *s)
{
    if (!s)
	return;

    bu_hook_delete_all(s->saved_log_hooks);
    bu_free(s->saved_log_hooks, "saved log hooks");

    bu_hook_delete_all(s->saved_bomb_hooks);
    bu_free(s->saved_bomb_hooks, "saved log hooks");

    bu_vls_free(s->nmg_log);
    bu_vls_free(s->nmg_log_header);
    BU_PUT(s->nmg_log, struct bu_vls);
    BU_PUT(s->nmg_log_header, struct bu_vls);

    BU_PUT(s, struct _ged_facetize_logging_state);
}

struct _ged_facetize_state *
_ged_facetize_state_create()
{
    struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
    struct _ged_facetize_state *s = NULL;
    BU_GET(s, struct _ged_facetize_state);
    s->quiet= 0;
    s->verbosity = 0;
    s->make_nmg = 0;
    s->nonovlp_brep = 0;

    s->regions = 0;
    s->resume = 0;
    s->retry = 0;
    s->in_place = 0;

    s->no_nmg = 0;
    s->no_continuation = 0;
    s->screened_poisson = 0;

    BU_GET(s->faceted_suffix, struct bu_vls);
    bu_vls_init(s->faceted_suffix);
    bu_vls_sprintf(s->faceted_suffix, ".bot");

    s->max_time = 30;
    s->method_flags = 0;
    s->feature_size = 0.0;
    s->feature_scale = 0.15;
    s->d_feature_size = 0.0;
    s->tol = NULL;
    s->max_pnts = 0;
    s->s_opts = s_opts;

    s->nonovlp_threshold = 0;

    s->gedp = NULL;

    BU_ALLOC(s->c_map, struct bu_attribute_value_set);
    BU_ALLOC(s->s_map, struct bu_attribute_value_set);
    bu_avs_init_empty(s->c_map);
    bu_avs_init_empty(s->s_map);

    s->log_s = logging_state_create();

    BU_GET(s->froot, struct bu_vls);
    bu_vls_init(s->froot);

    BU_GET(s->nmg_comb, struct bu_vls);
    bu_vls_init(s->nmg_comb);

    BU_GET(s->manifold_comb, struct bu_vls);
    bu_vls_init(s->manifold_comb);

    BU_GET(s->continuation_comb, struct bu_vls);
    bu_vls_init(s->continuation_comb);

    BU_GET(s->spsr_comb, struct bu_vls);
    bu_vls_init(s->spsr_comb);

    return s;
}
void _ged_facetize_state_destroy(struct _ged_facetize_state *s)
{
    if (!s)
       	return;

    if (s->faceted_suffix) {
	bu_vls_free(s->faceted_suffix);
	BU_PUT(s->faceted_suffix, struct bu_vls);
    }

    bu_avs_free(s->c_map);
    bu_avs_free(s->s_map);
    bu_free(s->c_map, "comb map");
    bu_free(s->s_map, "solid map");

    logging_state_destroy(s->log_s);

    bu_vls_free(s->froot);
    BU_PUT(s->froot, struct bu_vls);

    bu_vls_free(s->nmg_comb);
    BU_PUT(s->nmg_comb, struct bu_vls);

    bu_vls_free(s->manifold_comb);
    BU_PUT(s->manifold_comb, struct bu_vls);

    bu_vls_free(s->continuation_comb);
    BU_PUT(s->continuation_comb, struct bu_vls);

    bu_vls_free(s->spsr_comb);
    BU_PUT(s->spsr_comb, struct bu_vls);

    BU_PUT(s, struct _ged_facetize_state);
}

int
_ged_facetize_objs(struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int ret = BRLCAD_ERROR;
    int done_trying = 0;
    int newobj_cnt;
    char *newname;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;
    struct bu_vls oname = BU_VLS_INIT_ZERO;
    int flags = s->method_flags;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct bg_tess_tol *tol = &(wdbp->wdb_ttol);

    RT_CHECK_DBI(dbip);

    if (argc < 0) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (_ged_validate_objs_list(s, argc, argv, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "dp array");
	return BRLCAD_ERROR;
    }

    if (!s->in_place) {
	newname = (char *)argv[argc-1];
	argc--;
    } else {
	/* Find a new name for the original object - that's also our working
	 * "newname" for the initial processing, until we swap at the end. */
	bu_vls_sprintf(&oname, "%s_original", argv[0]);
	if (db_lookup(dbip, bu_vls_cstr(&oname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(&oname, "-0");
	    bu_vls_incr(&oname, NULL, NULL, &_db_uniq_test, (void *)gedp);
	}
	newname = (char *)bu_vls_cstr(&oname);
    }

#if 0
    /* Before we try this, check that all the objects in the specified tree(s) are valid solids */
    if (!_ged_facetize_verify_solid(s, argc, dpa)) {
	if (flags & FACETIZE_SPSR) {
	    if (flags != FACETIZE_SPSR)
		bu_log("non-solid objects in specified tree(s) - falling back on point sampling/reconstruction methodology\n");
	    flags = FACETIZE_SPSR;
	} else {
	    if (!s->quiet)
		bu_log("Facetization aborted: non-solid objects in specified tree(s).\n");
	    bu_free(dpa, "dp array");
	    return BRLCAD_ERROR;
	}
    }
#endif

    /* Done with dpa */
    bu_free(dpa, "dp array");

    /* Given there are multiple possible methods, we need to iterate until either
     * we succeed or we exhaust all enabled methods. */
    while (!done_trying) {

	if (flags & FACETIZE_NMG) {
	    s->log_s->nmg_log_print_header = 1;
	    bu_vls_sprintf(s->log_s->nmg_log_header, "NMG: tessellating %s...\n", argv[0]);
	    if (argc != 1)
		bu_vls_sprintf(s->log_s->nmg_log_header, "NMG: tessellating %d objects with tolerances a=%g, r=%g, n=%g\n", argc, tol->abs, tol->rel, tol->norm);

	    /* Let the user know what's going on, unless output is suppressed */
	    if (!s->quiet)
		bu_log("%s", bu_vls_cstr(s->log_s->nmg_log_header));

	    if (_ged_facetize_booleval(s, argc, argv, newname) == BRLCAD_OK) {
		ret = BRLCAD_OK;
		break;
	    }

	    // NMG didn't work
	    flags = flags & ~(FACETIZE_NMG);
	    continue;
	}


	if (flags & FACETIZE_CONTINUATION) {
	    if (argc != 1) {
		if (s->verbosity)
		    bu_log("Continuation mode (currently) only supports one existing object at a time as input - not attempting.\n");
		flags = flags & ~(FACETIZE_CONTINUATION);
		continue;
	    }

	    if (_ged_continuation_obj(s, argv[0], newname) == FACETIZE_SUCCESS) {
		ret = BRLCAD_OK;
		break;
	    }

	    // Continuation didn't work
	    flags = flags & ~(FACETIZE_CONTINUATION);
	    continue;
	}

	if (flags & FACETIZE_SPSR) {
	    if (argc != 1) {
		if (s->verbosity)
		    bu_log("Screened Poisson mode (currently) only supports one existing object at a time as input - not attempting.\n");
		flags = flags & ~(FACETIZE_SPSR);
		continue;
	    }

	    if (_ged_spsr_obj(s, argv[0], newname) == FACETIZE_SUCCESS) {
		ret = BRLCAD_OK;
		break;
	    }

	    flags = flags & ~(FACETIZE_SPSR);
	    continue;
	}

	/* Out of options */
	done_trying = 1;

    }

    if ((ret == BRLCAD_OK) && s->in_place) {
	if (_ged_facetize_obj_swap(gedp, argv[0], newname) != BRLCAD_OK) {
	    return BRLCAD_ERROR;
	}
    }

    if (bu_vls_strlen(s->log_s->nmg_log) && s->method_flags & FACETIZE_NMG && s->verbosity > 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(s->log_s->nmg_log));
    }

    return ret;
}

extern "C" int
ged_facetize_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: facetize [options] [old_obj1 ...] new_obj\n";
    int print_help = 0;
    int need_help = 0;
    struct _ged_facetize_state *s = _ged_facetize_state_create();
    s->gedp = gedp;

    /* General options */
    struct bu_opt_desc d[19];
    BU_OPT(d[0],  "h", "help",          "",  NULL,            &print_help,            "Print help and exit");
    BU_OPT(d[1],  "v", "verbose",       "",  &_ged_vopt,      &(s->verbosity),        "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[2],  "q", "quiet",         "",  NULL,            &(s->quiet),            "Suppress all output (overrides verbose flag)");
    BU_OPT(d[3],  "",  "disable-nmg",   "",  NULL,            &(s->no_nmg),           "Disable use of the N-Manifold Geometry (NMG) meshing method");
    BU_OPT(d[4],  "",  "disable-cm",    "",  NULL,            &(s->no_continuation),  "Disable use of the Continuation Method (CM) meshing method");
    BU_OPT(d[5],  "",  "enable-spsr",   "",  NULL,            &(s->screened_poisson), "Enable Screened Poisson Surface Reconstruction (SPSR) meshing method (run -h --SPSR to see more options for this mode)");
    BU_OPT(d[6],  "n", "nmg-output",    "",  NULL,            &(s->make_nmg),         "Create an N-Manifold Geometry (NMG) object (default is to create a triangular BoT mesh).  Note that this will limit processing options and may reduce the conversion success rate.");
    BU_OPT(d[7],  "r", "regions",       "",  NULL,            &(s->regions),          "For combs, walk the trees and create new copies of the hierarchies with each region replaced by a facetized evaluation of that region. (Default is to create one facetized object for all specified inputs.)");
    BU_OPT(d[8],  "",  "resume",        "",  NULL,            &(s->resume),           "Resume an interrupted conversion (region mode only)");
    BU_OPT(d[9],  "",  "retry",         "",  NULL,            &(s->retry),            "When resuming an interrupted conversion, re-try operations that previously failed (default is to not repeat previous attempts with already-attempted methods.)");
    BU_OPT(d[10], "",  "in-place",      "",  NULL,            &(s->in_place),         "Alter the existing tree/object to reference the facetized object.  May only specify one input object with this mode, and no output name.  (Warning: this option changes pre-existing geometry!)");
    BU_OPT(d[11], "F", "fscale",        "#", &bu_opt_fastf_t, &(s->feature_scale),    "Percentage of the average thickness observed by the raytracer to use for a targeted feature size with sampling based methods.  Defaults to 0.15, overridden by --fsize");
    BU_OPT(d[12], "",  "fsize",         "#", &bu_opt_fastf_t, &(s->feature_size),     "Explicit feature length to try for sampling based methods - overrides feature-scale.");
    BU_OPT(d[13], "",  "fsized",        "#", &bu_opt_fastf_t, &(s->d_feature_size),   "Initial feature length to try for decimation in sampling based methods.  By default, this value is set to 1.5x the feature size.");
    BU_OPT(d[14], "",  "max-time",      "#", &bu_opt_int,     &(s->max_time),         "Maximum time to spend per processing step (in seconds).  Default is 30.  Zero means either the default (for routines which could run indefinitely) or run to completion (if there is a theoretical termination point for the algorithm).  Be careful when specifying zero - it can produce very long runs!.");
    BU_OPT(d[15], "",  "max-pnts",      "#", &bu_opt_int,     &(s->max_pnts),         "Maximum number of pnts to use when applying ray sampling methods.");
    BU_OPT(d[16], "B",  "",             "",  NULL,            &s->nonovlp_brep,       "EXPERIMENTAL: non-overlapping facetization to BoT objects of union-only brep comb tree.");
    BU_OPT(d[17], "t",  "threshold",    "#", &bu_opt_fastf_t, &s->nonovlp_threshold,  "EXPERIMENTAL: max ovlp threshold length for -B mode.");
    BU_OPT_NULL(d[18]);

    /* Poisson specific options */
    struct bu_opt_desc pd[4];
    BU_OPT(pd[0], "d", "depth",            "#", &bu_opt_int,     &(s->s_opts.depth),            "Maximum reconstruction depth (default 8)");
    BU_OPT(pd[1], "w", "interpolate",      "#", &bu_opt_fastf_t, &(s->s_opts.point_weight),     "Lower values (down to 0.0) bias towards a smoother mesh, higher values bias towards interpolation accuracy. (Default 2.0)");
    BU_OPT(pd[2], "",  "samples-per-node", "#", &bu_opt_fastf_t, &(s->s_opts.samples_per_node), "How many samples should go into a cell before it is refined. (Default 1.5)");
    BU_OPT_NULL(pd[3]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    argc = bu_opt_parse(&omsg, argc, argv, d);
    if (argc < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing failed: %s\n", bu_vls_cstr(&omsg));
	ret = BRLCAD_ERROR;
	bu_vls_free(&omsg);
	goto ged_facetize_memfree;
    }
    bu_vls_free(&omsg);

    /* It is known that libnmg will (as of 2018 anyway) throw a lot of
     * bu_bomb calls during operation. Because we need facetize to run
     * to completion and potentially try multiple ways to convert before
     * giving up, we need to un-hook any pre-existing bu_bomb hooks */
    bu_bomb_save_all_hooks(s->log_s->saved_bomb_hooks);

    /* We will need to catch libnmg output and store it up for later
     * use, while still bu_logging our own status updates. Cache the
     * current bu_log hooks so they can be restored at need */
    bu_log_hook_save_all(s->log_s->saved_log_hooks);

    /* Sync -q and -v options */
    if (s->quiet && s->verbosity)
       	s->verbosity = 0;

    /* Don't allow incorrect type suffixes */
    if (s->make_nmg && BU_STR_EQUAL(bu_vls_cstr(s->faceted_suffix), ".bot")) {
	bu_vls_sprintf(s->faceted_suffix, ".nmg");
    }
    if (!s->make_nmg && BU_STR_EQUAL(bu_vls_cstr(s->faceted_suffix), ".nmg")) {
	bu_vls_sprintf(s->faceted_suffix, ".bot");
    }

    /* Sort out which methods we can try */
    s->method_flags = 0;
    if (!s->no_nmg)
	s->method_flags |= FACETIZE_NMG;
    if (!s->no_continuation)
	s->method_flags |= FACETIZE_CONTINUATION;
    if (s->screened_poisson)
	s->method_flags |= FACETIZE_SPSR;

    if (s->method_flags & FACETIZE_SPSR) {
	/* Parse Poisson specific options, if that method is enabled */
	argc = bu_opt_parse(NULL, argc, argv, pd);

	if (argc < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Screened Poisson option parsing failed\n");
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_memfree;
	}
    }

    /* We can only resume in region mode - validate options */
    if (s->resume && !s->regions) {
	bu_vls_printf(gedp->ged_result_str, "--resume is only supported with with region (-r) mode\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_memfree;
    }

    /* Check if we want/need help */
    need_help += (argc < 1);
    need_help += (argc < 2 && !s->in_place && !s->resume && !s->nonovlp_brep);
    if (print_help || need_help || argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	if (s->method_flags & FACETIZE_SPSR) {
	    _ged_cmd_help(gedp, usage, pd);
	}
	ret = (need_help) ? BRLCAD_ERROR : BRLCAD_OK;
	goto ged_facetize_memfree;
    }

    /* If we're doing the experimental brep-only logic, it's a separate process */
    if (s->nonovlp_brep) {
	if (NEAR_ZERO(s->nonovlp_threshold, SMALL_FASTF)) {
	    bu_vls_printf(gedp->ged_result_str, "-B option requires a specified length threshold\n");
	    return BRLCAD_ERROR;
	}
	ret = _nonovlp_brep_facetize(s, argc, argv);
	goto ged_facetize_memfree;
    }

    /* Multi-region mode has a different processing logic */
    if (s->regions) {
	ret = _ged_facetize_regions(s, argc, argv);
    } else {
	ret = _ged_facetize_objs(s, argc, argv);
    }

ged_facetize_memfree:
    _ged_facetize_state_destroy(s);

    return ret;
}




#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
struct ged_cmd_impl facetize_cmd_impl = { "facetize", ged_facetize_core, GED_CMD_DEFAULT };
const struct ged_cmd facetize_cmd = { &facetize_cmd_impl };
const struct ged_cmd *facetize_cmds[] = { &facetize_cmd,  NULL };

static const struct ged_plugin pinfo = { GED_API,  facetize_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

