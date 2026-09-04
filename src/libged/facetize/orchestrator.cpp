/*              O R C H E S T R A T O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/facetize/orchestrator.cpp
 *
 * Command planning, session lifetime, and workspace ownership for facetize.
 */

#include "common.h"

#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/path.h"
#include "bu/vls.h"

#include "../ged_private.h"
#include "./ged_facetize.h"
#include "./tess_opts.h"

static const unsigned int FACETIZE_WORKSPACE_SCHEMA = 1;
static const char FACETIZE_MANIFEST_NAME[] = "request.manifest";

std::vector<const char *>
FacetizePlan::input_argv() const
{
    std::vector<const char *> argv;
    argv.reserve(inputs.size());
    for (const std::string &input : inputs)
	argv.push_back(input.c_str());
    return argv;
}

static struct _ged_facetize_state *
facetize_state_create()
{
    struct _ged_facetize_state *s = NULL;
    BU_GET(s, struct _ged_facetize_state);
    s->verbosity = 0;
    s->no_empty = 0;
    s->no_fixup = 0;
    s->use_variant_plan = 1;
    s->tolerate_failures = 0;
    s->tolerated_failures = 0;
    s->tolerated_failure_details = 0;
    s->tolerated_failure_omitted = 0;
    s->inspection_regions = 0;
    s->perturb_sa_tol = 10.0;
    s->perturb_vol_tol = 10.0;
    s->wdir = NULL;

    BU_GET(s->log_file, struct bu_vls);
    bu_vls_init(s->log_file);
    s->log_file_is_temporary = 0;
    s->lfile = NULL;

    BU_GET(s->failure_msg, struct bu_vls);
    bu_vls_init(s->failure_msg);
    BU_GET(s->tolerated_failure_log, struct bu_vls);
    bu_vls_init(s->tolerated_failure_log);
    BU_GET(s->region_summary, struct bu_vls);
    bu_vls_init(s->region_summary);
    BU_GET(s->primitive_summary, struct bu_vls);
    bu_vls_init(s->primitive_summary);
    BU_GET(s->inspection_log, struct bu_vls);
    bu_vls_init(s->inspection_log);
    BU_GET(s->wfile, struct bu_vls);
    bu_vls_init(s->wfile);
    BU_GET(s->bname, struct bu_vls);
    bu_vls_init(s->bname);
    BU_GET(s->suffix, struct bu_vls);
    bu_vls_init(s->suffix);
    BU_GET(s->prefix, struct bu_vls);
    bu_vls_init(s->prefix);
    BU_GET(s->solid_suffix, struct bu_vls);
    bu_vls_init(s->solid_suffix);
    bu_vls_sprintf(s->solid_suffix, ".bot");

    s->resume = 0;
    s->max_time = 0;
    s->max_pnts = 0;
    s->max_workers = 0;
    s->tol = NULL;
    s->nonovlp_threshold = 0;
    s->error_flag = 0;
    s->gedp = NULL;
    s->dbip = NULL;
    s->facetize_tree = NULL;
    s->method_opts = NULL;
    s->log_s = NULL;
    s->write_profiled = 0;
    s->write_profile_bytes = 0.0;
    s->write_profile_usec = 0.0;
    s->variant_plan = NULL;

    return s;
}

static void
facetize_state_destroy(struct _ged_facetize_state *s)
{
    if (!s)
	return;

    if (s->wdir)
	bu_free(s->wdir, "wdir");
    if (s->lfile)
	fclose(s->lfile);

    struct bu_vls *strings[] = {
	s->bname, s->log_file, s->failure_msg, s->tolerated_failure_log,
	s->region_summary, s->primitive_summary, s->inspection_log, s->wfile,
	s->prefix, s->suffix, s->solid_suffix
    };
    for (struct bu_vls *str : strings) {
	if (!str)
	    continue;
	bu_vls_free(str);
	BU_PUT(str, struct bu_vls);
    }

    delete (FacetizeVariantPlan *)s->variant_plan;
    BU_PUT(s, struct _ged_facetize_state);
}

FacetizeSession::FacetizeSession(struct ged *gedp)
{
    methods = new method_options_t;
    session_state = facetize_state_create();
    session_state->gedp = gedp;
    session_state->dbip = gedp->dbip;
    session_state->method_opts = methods;
}

FacetizeSession::~FacetizeSession()
{
    facetize_summary(session_state);
    facetize_state_destroy(session_state);
    delete methods;
}

int
facetize_build_plan(struct _ged_facetize_state *s, int argc,
	const char **argv, FacetizePlan &plan)
{
    if (!s || !s->dbip || argc <= 0 || !argv)
	return BRLCAD_ERROR;

    plan.execution = s->execution;
    plan.inputs.clear();
    plan.output.clear();

    std::vector<std::string> missing;
    for (int i = 0; i < argc; i++) {
	if (db_lookup(s->dbip, argv[i], LOOKUP_QUIET) == RT_DIR_NULL)
	    missing.emplace_back(argv[i]);
	else
	    plan.inputs.emplace_back(argv[i]);
    }

    if (plan.execution.writes_in_place()) {
	if (!missing.empty()) {
	    bu_vls_printf(s->gedp->ged_result_str,
		    "In-place conversion requires every input object to exist:\n");
	    for (const std::string &name : missing)
		bu_vls_printf(s->gedp->ged_result_str, "    %s\n", name.c_str());
	    return BRLCAD_ERROR;
	}
    } else {
	if (missing.size() != 1) {
	    if (missing.empty())
		bu_vls_printf(s->gedp->ged_result_str,
			"A new output object name is required.\n");
	    else
		bu_vls_printf(s->gedp->ged_result_str,
			"Exactly one new output object name is required; found %zu.\n",
			missing.size());
	    return BRLCAD_ERROR;
	}
	plan.output = missing.front();
    }

    if (plan.inputs.empty()) {
	bu_vls_printf(s->gedp->ged_result_str,
		"No existing input objects were specified.\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static std::string
facetize_request_signature(struct _ged_facetize_state *s,
	const FacetizePlan &plan, const char *source_path,
	const struct stat &source_stat)
{
    std::ostringstream signature;
    signature << "schema=" << FACETIZE_WORKSPACE_SCHEMA << '\n';
    signature << "source=" << source_path << '\n';
    signature << "source_size=" << source_stat.st_size << '\n';
    signature << "source_mtime=" << source_stat.st_mtime << '\n';
    signature << "scope=" << static_cast<int>(plan.execution.scope) << '\n';
    signature << "engine=" << static_cast<int>(plan.execution.boolean_engine) << '\n';
    signature << "output_format=" << static_cast<int>(plan.execution.output_format) << '\n';
    signature << "commit=" << static_cast<int>(plan.execution.commit_mode) << '\n';
    signature << "perturb=" << static_cast<int>(plan.execution.perturb_mode) << '\n';
    signature << "no_empty=" << s->no_empty << '\n';
    signature << "no_fixup=" << s->no_fixup << '\n';
    signature << "tolerate_failures=" << s->tolerate_failures << '\n';
    signature << "max_time=" << s->max_time << '\n';
    signature << "max_pnts=" << s->max_pnts << '\n';
    signature << "max_workers=" << s->max_workers << '\n';
    signature << "perturb_sa_tol=" << s->perturb_sa_tol << '\n';
    signature << "perturb_vol_tol=" << s->perturb_vol_tol << '\n';
    signature << "prefix=" << bu_vls_cstr(s->prefix) << '\n';
    signature << "suffix=" << bu_vls_cstr(s->suffix) << '\n';
    signature << "output=" << plan.output << '\n';
    for (const std::string &input : plan.inputs)
	signature << "input=" << input << '\n';

    method_options_t *methods = (method_options_t *)s->method_opts;
    if (methods) {
	for (const std::string &method : methods->methods)
	    signature << "method=" << method << '\n';
	for (const auto &method_entry : methods->options_map)
	    for (const auto &option_entry : method_entry.second)
		signature << "method_option=" << method_entry.first << ':'
		    << option_entry.first << '=' << option_entry.second << '\n';
    }
    return signature.str();
}

static bool
read_file(const char *path, std::string &contents)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
	return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    contents = buffer.str();
    return input.good() || input.eof();
}

static bool
write_file(const char *path, const std::string &contents)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
	return false;
    output.write(contents.data(), (std::streamsize)contents.size());
    return output.good();
}

int
facetize_prepare_workspace(struct _ged_facetize_state *s,
	const FacetizePlan &plan)
{
    char source_path[MAXPATHLEN];
    if (!bu_file_realpath(s->dbip->dbi_filename, source_path)) {
	bu_vls_printf(s->gedp->ged_result_str,
		"Unable to resolve database path '%s'.\n", s->dbip->dbi_filename);
	return BRLCAD_ERROR;
    }

    struct stat source_stat;
    if (stat(source_path, &source_stat) != 0) {
	bu_vls_printf(s->gedp->ged_result_str,
		"Unable to inspect database '%s'.\n", source_path);
	return BRLCAD_ERROR;
    }

    bu_path_component(s->bname, source_path, BU_PATH_BASENAME);
    unsigned long long path_hash = bu_data_hash(source_path, strlen(source_path));
    struct bu_vls workspace_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&workspace_name, "facetize_%llu", path_hash);
    s->wdir = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "facetize workspace");
    bu_dir(s->wdir, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&workspace_name), NULL);

    bool workspace_exists = bu_file_directory(s->wdir);
    if (!s->resume && workspace_exists)
	bu_dirclear(s->wdir);
    if (!bu_file_directory(s->wdir))
	bu_mkdir(s->wdir);
    if (!bu_file_directory(s->wdir)) {
	bu_vls_printf(s->gedp->ged_result_str,
		"Unable to create facetize workspace '%s'.\n", s->wdir);
	bu_vls_free(&workspace_name);
	return BRLCAD_ERROR;
    }

    char manifest_path[MAXPATHLEN];
    bu_dir(manifest_path, MAXPATHLEN, s->wdir, FACETIZE_MANIFEST_NAME, NULL);
    std::string expected_manifest = facetize_request_signature(s, plan,
	    source_path, source_stat);
    if (s->resume) {
	std::string actual_manifest;
	if (!read_file(manifest_path, actual_manifest) || actual_manifest != expected_manifest) {
	    bu_vls_printf(s->gedp->ged_result_str,
		    "Cannot resume: workspace does not match the database and facetize options.\n");
	    bu_vls_free(&workspace_name);
	    return BRLCAD_ERROR;
	}
    } else if (!write_file(manifest_path, expected_manifest)) {
	bu_vls_printf(s->gedp->ged_result_str,
		"Unable to write facetize workspace manifest '%s'.\n", manifest_path);
	bu_vls_free(&workspace_name);
	return BRLCAD_ERROR;
    }

    if (!bu_vls_strlen(s->log_file)) {
	char log_path[MAXPATHLEN];
	s->log_file_is_temporary = 1;
	bu_vls_sprintf(&workspace_name, "facetize_%s.log", bu_vls_cstr(s->bname));
	bu_dir(log_path, MAXPATHLEN, s->wdir, bu_vls_cstr(&workspace_name), NULL);
	bu_vls_sprintf(s->log_file, "%s", log_path);
    }
    bu_vls_free(&workspace_name);

    s->lfile = fopen(bu_vls_cstr(s->log_file), "a");
    if (!s->lfile) {
	bu_vls_printf(s->gedp->ged_result_str,
		"Unable to open log file %s for writing\n", bu_vls_cstr(s->log_file));
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

int
facetize_execute_plan(struct _ged_facetize_state *s,
	const FacetizePlan &plan)
{
    if (plan.execution.processes_breps())
	return _nonovlp_brep_facetize(s, plan);
    if (plan.execution.processes_regions())
	return _ged_facetize_regions(s, plan);
    return _ged_facetize_objs(s, plan);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
