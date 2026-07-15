/*                     F A C E T I Z E . C P P
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

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/path.h"
#include "wdb.h"

#include "../ged_private.h"

#define TESS_OPTS_IMPLEMENTATION
#include "./tess_opts.h"
#include "./ged_facetize.h"


struct facetize_parse_args {
    int print_help = 0;
    long verbosity = 0;
    int quiet = 0;
    int make_nmg = 0;
    int regions = 0;
    const char *suffix = NULL;
    const char *prefix = NULL;
    int in_place = 0;
    int max_time = 0;
    int max_pnts = 0;
    int resume = 0;
    method_options_t *method_options = NULL;
    int no_empty = 0;
    const char *log_file = NULL;
    int nmg_booleval = 0;
    int no_fixup = 0;
    int force_perturb = 0;
    int disable_perturb = 0;
    int nonovlp_brep = 0;
    fastf_t nonovlp_threshold = 0.0;
    fastf_t perturb_sa_tol = 10.0;
    fastf_t perturb_vol_tol = 10.0;
    int tolerate_failures = 0;
};

static const struct bu_cmd_schema *facetize_schema(void);


static void
facetize_show_schema_help(struct ged *gedp, const char *usage)
{
    char *help = bu_cmd_schema_describe(facetize_schema());

    if (usage)
	bu_vls_strcat(gedp->ged_result_str, usage);
    if (help) {
	bu_vls_putc(gedp->ged_result_str, '\n');
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "facetize native schema help");
    }
}

void
_facetize_methods_help(struct ged *gedp)
{
        // Build up the path to the ged_exec executable
      char tess_exec[MAXPATHLEN];
      bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);

      const char *tess_cmd[MAXPATHLEN] = {NULL};
      tess_cmd[ 0] = tess_exec;
      tess_cmd[ 1] = "facetize_process";
      tess_cmd[ 2] = "--list-methods";
      tess_cmd[ 3] = NULL;

      struct bu_process* mp;
      bu_process_create(&mp, tess_cmd, BU_PROCESS_HIDE_WINDOW);
      char mraw[MAXPATHLEN] = {'\0'};
      int read_res = bu_process_read_n(mp, BU_PROCESS_STDOUT, MAXPATHLEN, mraw);
      if (bu_process_wait_n(&mp, 0) || (read_res <= 0))
	  return;   // wait error or read error
      std::string method_list(mraw);
      bu_vls_printf(gedp->ged_result_str, "Available BoT tessellation methods: %s\n", method_list.c_str());

      tess_cmd[ 3] = "-h";
      tess_cmd[ 4] = NULL;

      struct bu_process* mop;
      bu_process_create(&mop, tess_cmd, BU_PROCESS_HIDE_WINDOW);
      char moraw[MAXPATHLEN*10] = {'\0'};
      read_res = bu_process_read_n(mop, BU_PROCESS_STDOUT, MAXPATHLEN*10, moraw);
      if (bu_process_wait_n(&mop, 0) || (read_res <= 0))
	  return;   // wait error
      std::string method_options(moraw);

      bu_vls_printf(gedp->ged_result_str, "\nMethod specific options:\n\n%s\n", method_options.c_str());
}

struct _ged_facetize_state *
_ged_facetize_state_create()
{
    struct _ged_facetize_state *s = NULL;
    BU_GET(s, struct _ged_facetize_state);
    s->verbosity = 0;
    s->no_empty = 0;
    s->make_nmg = 0;
    s->nonovlp_brep = 0;
    s->no_fixup = 0;
    s->nmg_booleval = 0;
    s->use_variant_plan = 1;
    s->tolerate_failures = 0;
    s->tolerated_failures = 0;
    s->tolerated_failure_details = 0;
    s->tolerated_failure_omitted = 0;
    s->perturb_sa_tol  = 10.0;
    s->perturb_vol_tol = 10.0;

    s->wdir = NULL;

    BU_GET(s->log_file, struct bu_vls);
    bu_vls_init(s->log_file);

    s->lfile = NULL;

    BU_GET(s->failure_msg, struct bu_vls);
    bu_vls_init(s->failure_msg);

    BU_GET(s->tolerated_failure_log, struct bu_vls);
    bu_vls_init(s->tolerated_failure_log);

    BU_GET(s->wfile, struct bu_vls);
    bu_vls_init(s->wfile);

    BU_GET(s->bname, struct bu_vls);
    bu_vls_init(s->bname);

    s->regions = 0;
    s->resume = 0;
    s->in_place = 0;

    BU_GET(s->suffix, struct bu_vls);
    bu_vls_init(s->suffix);

    BU_GET(s->prefix, struct bu_vls);
    bu_vls_init(s->prefix);

    BU_GET(s->solid_suffix, struct bu_vls);
    bu_vls_init(s->solid_suffix);
    bu_vls_sprintf(s->solid_suffix, ".bot");

    s->max_time = 0;
    s->max_pnts = 0;

    s->tol = NULL;
    s->nonovlp_threshold = 0;

    s->gedp = NULL;

    s->variant_plan = NULL;

    return s;
}
void _ged_facetize_state_destroy(struct _ged_facetize_state *s)
{
    if (!s)
       	return;

    if (s->wdir)
	bu_free(s->wdir, "wdir");

    if (s->bname) {
	bu_vls_free(s->bname);
	BU_PUT(s->bname, struct bu_vls);
    }

    if (s->lfile) {
	fclose(s->lfile);
	s->lfile = NULL;
    }

    if (s->log_file) {
	bu_vls_free(s->log_file);
	BU_PUT(s->log_file, struct bu_vls);
    }

    if (s->failure_msg) {
	bu_vls_free(s->failure_msg);
	BU_PUT(s->failure_msg, struct bu_vls);
    }

    if (s->tolerated_failure_log) {
	bu_vls_free(s->tolerated_failure_log);
	BU_PUT(s->tolerated_failure_log, struct bu_vls);
    }

    if (s->wfile) {
	bu_vls_free(s->wfile);
	BU_PUT(s->wfile, struct bu_vls);
    }

    if (s->prefix) {
	bu_vls_free(s->prefix);
	BU_PUT(s->prefix, struct bu_vls);
    }

    if (s->suffix) {
	bu_vls_free(s->suffix);
	BU_PUT(s->suffix, struct bu_vls);
    }

    if (s->solid_suffix) {
	bu_vls_free(s->solid_suffix);
	BU_PUT(s->solid_suffix, struct bu_vls);
    }

    if (s->variant_plan) {
	delete (FacetizeVariantPlan *)s->variant_plan;
	s->variant_plan = NULL;
    }

    BU_PUT(s, struct _ged_facetize_state);
}

int
_ged_facetize_objs(struct _ged_facetize_state *s, int argc, const char **argv)
{
    int ret = BRLCAD_ERROR;
    int newobj_cnt, i;
    int ok_cnt = 0;
    const char *oname = NULL;
    const char *av[2];
    struct directory **dpa = NULL;
    struct directory *idpa[2];
    struct db_i *dbip = s->dbip;

    RT_CHECK_DBI(dbip);

    if (argc < 0) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(dbip, argc, argv, dpa);
    if (_ged_validate_objs_list(s, argc, argv, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "dp array");
	return BRLCAD_ERROR;
    }

    if (!s->in_place) {
	oname = argv[argc-1];
	argc--;
    }

    /* If we're doing an NMG output, or we have been instructed to, use the
     * old-school libnmg booleval */
    if (s->make_nmg || s->nmg_booleval) {
	if (!s->in_place) {
	    ret = _ged_facetize_nmgeval(s, argc, argv, oname);
	    goto booleval_cleanup;
	} else {
	    for (i = 0; i < argc; i++) {
		av[0] = argv[i];
		av[1] = NULL;
		ret = _ged_facetize_nmgeval(s, 1, av, av[0]);
		if (ret == BRLCAD_ERROR && s->tolerate_failures) {
		    facetize_tolerated_failure(s, "object '%s' failed during NMG boolean evaluation and was skipped", argv[i]);
		    continue;
		}
		if (ret == BRLCAD_ERROR)
		    goto booleval_cleanup;
		ok_cnt++;
	    }
	    if (s->tolerate_failures && ok_cnt > 0)
		ret = BRLCAD_OK;
	    goto booleval_cleanup;
	}
    }

    // If we're not doing NMG, use the Manifold booleval
    if (!s->in_place) {
	ret = _ged_facetize_booleval(s, argc, dpa, oname, false, false);
    } else {
	for (i = 0; i < argc; i++) {
	    idpa[0] = dpa[i];
	    idpa[1] = NULL;
	    ret = _ged_facetize_booleval(s, 1, (struct directory **)idpa, argv[i], false, false);
	    if (ret == BRLCAD_ERROR && s->tolerate_failures) {
		facetize_tolerated_failure(s, "object '%s' failed during BoT boolean evaluation and was skipped", argv[i]);
		continue;
	    }
	    if (ret == BRLCAD_ERROR)
		goto booleval_cleanup;
	    ok_cnt++;
	}
	if (s->tolerate_failures && ok_cnt > 0)
	    ret = BRLCAD_OK;
    }

    // Report on the primitive processing
    facetize_primitives_summary(s);

    // After collecting info for summary, we can now clean up working files
    bu_dirclear(s->wdir);

booleval_cleanup:
    bu_free(dpa, "dp array");

    return ret;
}

extern "C" int
ged_facetize_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: facetize [options] [old_obj1 ...] [new_obj]\n";
    int need_help = 0;
    method_options_t *method_options = new method_options_t;
    std::map<std::string, std::map<std::string,std::string>>::iterator o_it;
    struct _ged_facetize_state *s = _ged_facetize_state_create();
    struct facetize_parse_args args;
    s->gedp = gedp;
    s->dbip = gedp->dbip;
    s->method_opts = method_options;

    args.method_options = method_options;
    args.perturb_sa_tol = s->perturb_sa_tol;
    args.perturb_vol_tol = s->perturb_vol_tol;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Parse the canonical native option schema. */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    int operand_start = bu_cmd_schema_parse(facetize_schema(), &args, &omsg, argc, argv);
    if (operand_start < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing failed: %s\n", bu_vls_cstr(&omsg));
	ret = BRLCAD_ERROR;
	bu_vls_free(&omsg);
	goto ged_facetize_memfree;
    }
    bu_vls_free(&omsg);
    argc -= operand_start;
    argv += operand_start;

    s->make_nmg = args.make_nmg;
    s->regions = args.regions;
    s->in_place = args.in_place;
    s->max_time = args.max_time;
    s->max_pnts = args.max_pnts;
    s->resume = args.resume;
    s->no_empty = args.no_empty;
    s->nmg_booleval = args.nmg_booleval;
    s->no_fixup = args.no_fixup;
    s->nonovlp_brep = args.nonovlp_brep;
    s->nonovlp_threshold = args.nonovlp_threshold;
    s->perturb_sa_tol = args.perturb_sa_tol;
    s->perturb_vol_tol = args.perturb_vol_tol;
    s->tolerate_failures = args.tolerate_failures;
    if (args.suffix)
	bu_vls_strcpy(s->suffix, args.suffix);
    if (args.prefix)
	bu_vls_strcpy(s->prefix, args.prefix);
    if (args.log_file)
	bu_vls_strcpy(s->log_file, args.log_file);

    // Sanity
    if (args.force_perturb && args.disable_perturb) {
    	bu_vls_printf(gedp->ged_result_str, "Can only specify one of --perturb or --no-perturb\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_memfree;
    }

    // Set no_perturb according to options.  Enable by default for -r, disable
    // if not doing -r (one large BoT, generally a less likely candidate for
    // worrying about volume/surf_area matching.)
    s->no_perturb = (s->regions) ? 0 : 1;
    if (args.disable_perturb)
	s->no_perturb = 1;
    if (args.force_perturb)
	s->no_perturb = 0;

    s->verbosity = (int)args.verbosity;

    // If we got a max-time top level arg, override any times that aren't specifically set
    // by method options
    if (s->max_time) {
	for (o_it = method_options->options_map.begin(); o_it != method_options->options_map.end(); o_it++) {
	    std::map<std::string,std::string>::iterator m_it = o_it->second.find(std::string("max_time"));
	    if (m_it == o_it->second.end()) {
		// max-time wasn't explicitly set by a method, and we have an option - override
		method_options->max_time[o_it->first] = s->max_time;
		o_it->second[std::string("max_time")] = std::to_string(s->max_time);
	    }
	}
    }

    /* Sync -q and -v options */
    if (args.quiet)
	s->verbosity = -1;

    /* Don't allow incorrect type suffixes */
    if (s->make_nmg && BU_STR_EQUAL(bu_vls_cstr(s->solid_suffix), ".bot")) {
	bu_vls_sprintf(s->solid_suffix, ".nmg");
    }
    if (!s->make_nmg && BU_STR_EQUAL(bu_vls_cstr(s->solid_suffix), ".nmg")) {
	bu_vls_sprintf(s->solid_suffix, ".bot");
    }

    /* Check if we want/need help */
    need_help += (argc < 1);
    need_help += (argc < 2 && !s->in_place && !s->regions && !s->resume && !s->nonovlp_brep);
    if (args.print_help || need_help || argc < 1) {
	facetize_show_schema_help(gedp, usage);
	_facetize_methods_help(gedp);
	ret = args.print_help ? BRLCAD_OK : (need_help ? BRLCAD_ERROR : BRLCAD_OK);
	goto ged_facetize_memfree;
    }

    /* Beyond this point, we're likely to need info on the cache directory. Generate some
     * paths and strings we will need. */
    {
	// Get the root filename
	char rfname[MAXPATHLEN];
	bu_file_realpath(gedp->dbip->dbi_filename, rfname);
	bu_path_component(s->bname, rfname, BU_PATH_BASENAME);

	// Hash the path string and construct a location in the cache directory
	unsigned long long hash_num = bu_data_hash((void *)bu_vls_cstr(s->bname), bu_vls_strlen(s->bname));
	struct bu_vls dname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&dname, "facetize_%llu", hash_num);
	s->wdir = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "wdir");
	bu_dir(s->wdir, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), NULL);

	// If we're starting over, clear the old working directory
	if (!s->resume && bu_file_directory(s->wdir)) {
	    bu_dirclear(s->wdir);
	}

	if (!bu_file_directory(s->wdir)) {
	    // Set up the directory
	    bu_mkdir(s->wdir);
	}

	if (!bu_vls_strlen(s->log_file)) {
	    char tmplfile[MAXPATHLEN];
	    bu_vls_sprintf(&dname, "facetize_%s.log", bu_vls_cstr(s->bname));
	    bu_dir(tmplfile, MAXPATHLEN, s->wdir, bu_vls_cstr(&dname), NULL);
	    bu_vls_sprintf(s->log_file, "%s", tmplfile);
	}
	bu_vls_free(&dname);

	s->lfile = fopen(bu_vls_cstr(s->log_file), "a");
	if (!s->lfile) {
	    bu_vls_printf(gedp->ged_result_str, "Unable to open log file %s for writing\n", bu_vls_cstr(s->log_file));
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_memfree;
	}
    }

    /* If we're doing the experimental brep-only logic, it's a separate process */
    if (s->nonovlp_brep) {
	if (NEAR_ZERO(s->nonovlp_threshold, SMALL_FASTF)) {
	    bu_vls_printf(gedp->ged_result_str, "-B option requires a specified length threshold\n");
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_memfree;
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
    facetize_tolerated_summary(s);
    _ged_facetize_state_destroy(s);
    delete method_options;

    return ret;
}

#include "../include/plugin.h"

static const char * const facetize_perturb_options[] = {
    "perturb", "no-perturb", NULL
};
static const char * const facetize_method_names[] = {
    "NMG", "CM", "SPSR", NULL
};
static const struct bu_cmd_arg_shape facetize_methods_shape = {
    BU_CMD_ARG_SHAPE_COMMA_LIST, 1, 1, "comma-separated tessellation methods", NULL
};

static const struct bu_cmd_constraint facetize_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(facetize_perturb_options, 0, 1,
	"--perturb and --no-perturb cannot be used together"),
    BU_CMD_CONSTRAINT_NULL
};

static const struct bu_cmd_option facetize_options[] = {
    BU_CMD_FLAG("h", "help", facetize_parse_args, print_help, "Print help and exit"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", facetize_parse_args, verbosity, "Verbose output; repeat to increase detail"),
    BU_CMD_FLAG("q", "quiet", facetize_parse_args, quiet, "Suppress all output"),
    BU_CMD_FLAG("n", "nmg-output", facetize_parse_args, make_nmg, "Create N-Manifold Geometry output"),
    BU_CMD_FLAG("r", "regions", facetize_parse_args, regions, "Facetize region trees"),
    BU_CMD_STRING("s", "suffix", facetize_parse_args, suffix, "suffix", "Output suffix"),
    BU_CMD_STRING("p", "prefix", facetize_parse_args, prefix, "prefix", "Output prefix"),
    BU_CMD_FLAG(NULL, "in-place", facetize_parse_args, in_place, "Replace input objects"),
    BU_CMD_INTEGER(NULL, "max-time", facetize_parse_args, max_time, "seconds", "Maximum seconds per object"),
    BU_CMD_INTEGER(NULL, "max-pnts", facetize_parse_args, max_pnts, "count", "Maximum sample points"),
    BU_CMD_FLAG(NULL, "resume", facetize_parse_args, resume, "Resume conversion"),
    {NULL, "methods", "methods", "method[,method...]", "Comma-separated tessellation methods",
	BU_CMD_VALUE_CUSTOM, offsetof(facetize_parse_args, method_options), tess_active_methods_from_str,
	NULL, NULL, NULL, 0, 0, facetize_method_names, BU_CMD_ARG_REQUIRED,
	&facetize_methods_shape, NULL, NULL},
    BU_CMD_CUSTOM(NULL, "method-opts", facetize_parse_args, method_options, tess_method_opts_from_str,
	"\"METHOD option=value ...\"", "Method-specific options"),
    BU_CMD_FLAG(NULL, "no-empty", facetize_parse_args, no_empty, "Suppress empty outputs"),
    BU_CMD_FILE(NULL, "log-file", facetize_parse_args, log_file, "file", "Log file"),
    BU_CMD_FLAG(NULL, "nmg-booleval", facetize_parse_args, nmg_booleval, "Use NMG Boolean evaluation"),
    BU_CMD_FLAG(NULL, "disable-fixup", facetize_parse_args, no_fixup, "Disable mesh fixups"),
    BU_CMD_FLAG(NULL, "perturb", facetize_parse_args, force_perturb, "Enable perturbation"),
    BU_CMD_FLAG(NULL, "no-perturb", facetize_parse_args, disable_perturb, "Disable perturbation"),
    BU_CMD_FLAG("B", NULL, facetize_parse_args, nonovlp_brep, "Use experimental non-overlapping BREP mode"),
    BU_CMD_NUMBER("t", "threshold", facetize_parse_args, nonovlp_threshold, "length", "Non-overlap threshold"),
    BU_CMD_NUMBER(NULL, "perturb-sa-tol", facetize_parse_args, perturb_sa_tol, "percent", "Perturbation surface-area tolerance"),
    BU_CMD_NUMBER(NULL, "perturb-vol-tol", facetize_parse_args, perturb_vol_tol, "percent", "Perturbation volume tolerance"),
    BU_CMD_FLAG(NULL, "tolerate-failures", facetize_parse_args, tolerate_failures, "Generate partial output after failures"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand facetize_operands[] = {
    BU_CMD_OPERAND("input_or_output_object", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Input database objects followed by an optional output name", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema facetize_cmd_schema = {
    "facetize", "Convert geometry to BOT or NMG form", facetize_options, facetize_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, facetize_constraints)
};

static const struct bu_cmd_schema *
facetize_schema(void)
{
    return &facetize_cmd_schema;
}

#define GED_FACETIZE_COMMANDS(X, XID) \
    X(facetize, ged_facetize_core, GED_CMD_DEFAULT, &facetize_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_FACETIZE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_facetize", 1, GED_FACETIZE_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
