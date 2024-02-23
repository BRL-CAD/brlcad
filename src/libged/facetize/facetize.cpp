/*                     F A C E T I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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

#include <charconv>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "bu/opt.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

struct _ged_facetize_state *
_ged_facetize_state_create()
{
    //struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
    struct _ged_facetize_state *s = NULL;
    BU_GET(s, struct _ged_facetize_state);
    s->quiet= 0;
    s->verbosity = 0;
    s->make_nmg = 0;
    s->nonovlp_brep = 0;

    s->regions = 0;
    s->resume = 0;
    s->in_place = 0;

    s->method_flags = 0;

    BU_GET(s->faceted_suffix, struct bu_vls);
    bu_vls_init(s->faceted_suffix);
    bu_vls_sprintf(s->faceted_suffix, ".bot");

    s->max_time = 30;
    s->max_pnts = 0;
    s->tol = NULL;
    //s->s_opts = s_opts;

    s->nonovlp_threshold = 0;

    s->gedp = NULL;

    BU_ALLOC(s->c_map, struct bu_attribute_value_set);
    BU_ALLOC(s->s_map, struct bu_attribute_value_set);
    bu_avs_init_empty(s->c_map);
    bu_avs_init_empty(s->s_map);

    BU_GET(s->froot, struct bu_vls);
    bu_vls_init(s->froot);

#if 0
    BU_GET(s->nmg_comb, struct bu_vls);
    bu_vls_init(s->nmg_comb);

    BU_GET(s->manifold_comb, struct bu_vls);
    bu_vls_init(s->manifold_comb);

    BU_GET(s->continuation_comb, struct bu_vls);
    bu_vls_init(s->continuation_comb);

    BU_GET(s->spsr_comb, struct bu_vls);
    bu_vls_init(s->spsr_comb);
#endif

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

    bu_vls_free(s->froot);
    BU_PUT(s->froot, struct bu_vls);

#if 0
    bu_vls_free(s->nmg_comb);
    BU_PUT(s->nmg_comb, struct bu_vls);

    bu_vls_free(s->manifold_comb);
    BU_PUT(s->manifold_comb, struct bu_vls);

    bu_vls_free(s->continuation_comb);
    BU_PUT(s->continuation_comb, struct bu_vls);

    bu_vls_free(s->spsr_comb);
    BU_PUT(s->spsr_comb, struct bu_vls);
#endif
    BU_PUT(s, struct _ged_facetize_state);
}

int
_ged_facetize_objs(struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int ret = BRLCAD_ERROR;
    int newobj_cnt;
    char *newname;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;
    struct bu_vls oname = BU_VLS_INIT_ZERO;

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

    /* If we're doing an NMG output, use the old-school libnmg booleval */
    if (s->make_nmg) {

	/* Done with dpa */
	bu_free(dpa, "dp array");

	ret = _ged_facetize_nmgeval(s, argc, argv, newname);
	bu_vls_free(&oname);

	// Check for the in-place flag
	if (ret == BRLCAD_OK && s->in_place)
	    ret = _ged_facetize_obj_swap(gedp, argv[0], newname);

	return ret;
    }

    // If we're not doing NMG, use the Manifold booleval
    ret = _ged_facetize_booleval(s, newobj_cnt, dpa, newname, NULL, NULL);

    // Check for the in-place flag
    if (ret == BRLCAD_OK && s->in_place)
	ret = _ged_facetize_obj_swap(gedp, argv[0], newname);

    bu_vls_free(&oname);
    return ret;
}

struct method_options_t {
    std::set<std::string> methods;
    std::map<std::string, std::vector<std::string>> options_map;
    // Most of the method options need to be passed through to the subprocess,
    // but the time each method is allowed to process is ultimately managed by
    // the parent command.  Some methods may be able to respect a time limit,
    // but for those that cannot the subprocess may need to be killed.
    std::map<std::string, int> max_time;
};

int
_facetize_mopts(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    method_options_t *m = (method_options_t *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "_facetize_mopts");

    std::string av0 = std::string(argv[0]);
    std::stringstream astream(av0);
    std::string s;
    std::vector<std::string> opts;
    while (std::getline(astream, s, ' ')) {
	opts.push_back(s);
    }

    for (size_t i = 1; i < opts.size(); i++) {
	m->options_map[opts[0]].push_back(opts[i]);
	if (opts[i].length() < 8)
	    continue;
	if (!std::strncmp(opts[i].c_str(), "max_time", 8)) {
	    std::string val = opts[i].substr(8);
	    int ival;
	    auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), ival);
	    if (ec != std::errc{}) {
		bu_log("Error parsing max_time value\n");
		continue;
	    }
	    m->max_time[std::string("max_time")] = ival;
	}
    }
    return 1;
}


extern "C" int
ged_facetize_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: facetize [options] [old_obj1 ...] new_obj\n";
    int print_help = 0;
    int need_help = 0;
    int no_nmg = 0;
    int no_continuation = 0;
    int screened_poisson = 0;
    method_options_t method_options;
    // TODO - populated methods set from subprocess

    struct _ged_facetize_state *s = _ged_facetize_state_create();
    s->gedp = gedp;
    struct bu_vls methods_str = BU_VLS_INIT_ZERO;

    /* General options */
    struct bu_opt_desc d[14];
    BU_OPT(d[ 0], "h", "help",                                      "",             NULL,           &print_help, "Print help and exit");
    BU_OPT(d[ 1], "v", "verbose",                                   "",       &_ged_vopt,       &(s->verbosity), "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[ 2], "q", "quiet",                                     "",             NULL,           &(s->quiet), "Suppress all output (overrides verbose flag)");
    BU_OPT(d[ 3], "n", "nmg-output",                                "",             NULL,        &(s->make_nmg), "Create an N-Manifold Geometry (NMG) object (default is to create a triangular BoT mesh).  Note that this will disable most other processing options and may reduce the conversion success rate.");
    BU_OPT(d[ 4], "r", "regions",                                   "",             NULL,         &(s->regions), "For combs, walk the trees and create new copies of the hierarchies with each region's CSG tree replaced by a facetized evaluation of that region. (Default is to create one facetized object for all specified inputs.)");
    BU_OPT(d[ 5],  "", "in-place",                                  "",             NULL,        &(s->in_place), "Alter the existing tree/object to reference the facetized object.  May only specify one input object with this mode, and no output name.  (Warning: this option changes pre-existing geometry!)");
    BU_OPT(d[ 6],  "", "max-time",                                 "#",      &bu_opt_int,        &(s->max_time), "Maximum time to spend per processing step (in seconds).  Default is 30.  Zero means either the default (for routines which could run indefinitely) or run to completion (if there is a theoretical termination point for the algorithm).  Be careful when specifying zero - it can produce very long runs!");
    BU_OPT(d[ 7],  "", "max-pnts",                                 "#",      &bu_opt_int,        &(s->max_pnts), "Maximum number of pnts to use when applying ray sampling methods.");
    BU_OPT(d[ 8],  "", "resume",                                    "",             NULL,          &(s->resume), "Resume an interrupted conversion (region mode only)");
    BU_OPT(d[ 9],  "", "methods",                          "m1,m2,...",      &bu_opt_vls,        &(methods_str), "Specify methods to use when tessellating primitives.  If given no options and used with -h, print available methods");
    BU_OPT(d[10],  "", "method-opts",    "METHOD opt1=val opt2=val...", &_facetize_mopts,     &(method_options), "For the specified method, set the specified options.  If no options are specified and -h is used, print available method options.");
    BU_OPT(d[11], "B", "",                                          "",             NULL,      &s->nonovlp_brep, "EXPERIMENTAL: non-overlapping facetization to BoT objects of union-only brep comb tree.");
    BU_OPT(d[12], "t",  "threshold",                               "#",  &bu_opt_fastf_t, &s->nonovlp_threshold, "EXPERIMENTAL: max ovlp threshold length for -B mode.");
    BU_OPT_NULL(d[13]);

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
    if (!no_nmg)
	s->method_flags |= FACETIZE_METHOD_NMG;
    if (!no_continuation)
	s->method_flags |= FACETIZE_METHOD_CONTINUATION;
    if (screened_poisson)
	s->method_flags |= FACETIZE_METHOD_SPSR;

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

