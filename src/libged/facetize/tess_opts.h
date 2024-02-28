/*                  T E S S _ O P T S . H
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
/** @file tess_opts.h
 *
 * Logic for parsing tessellation options.  Used by both libged
 * command and subcommand, so contained in this header for easy
 * reuse.
 *
 */

#ifndef TESS_OPTS_H
#define TESS_OPTS_H

#include "common.h"

#include "vmath.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "raytrace.h"

class method_options_t {
    public:
	std::set<std::string> methods;
	std::map<std::string, std::map<std::string,std::string>> options_map;
	// Most of the method options need to be passed through to the subprocess,
	// but the time each method is allowed to process is ultimately managed by
	// the parent command.  Some methods may be able to respect a time limit,
	// but for those that cannot the subprocess may need to be killed.
	std::map<std::string, int> max_time;

	std::string method_optstr(std::string &method, struct db_i *dbip);
};


int _tess_active_methods(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);
int _tess_method_opts(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);


#define FACETIZE_METHOD_NULL          0x0
#define FACETIZE_METHOD_NMG           0x1
#define FACETIZE_METHOD_CONTINUATION  0x2
#define FACETIZE_METHOD_SPSR          0x4
#define FACETIZE_METHOD_REPAIR        0x8

class method_opts {
    public:
	virtual std::string about_method() = 0;
	virtual std::string print_options_help() = 0;
	virtual int set_var(std::string &key, std::string &val) = 0;
	virtual void sync(method_options_t &mopts) = 0;
	int max_time = 0;
	//struct bg_tess_tol *ttol = NULL;
	//struct bn_tol *tol = NULL;
};

class sample_opts : public method_opts {
    public:
	std::string about_method() { return std::string(""); }
	std::string print_options_help();
	int set_var(std::string &key, std::string &val);
	void sync(method_options_t &mopts);
	void sync(sample_opts &sopts);

	std::string print_sample_options_help();
	bool equals(sample_opts &other);

	fastf_t feature_scale = 0.15; // Percentage of the average thickness observed by the raytracer to use for a targeted feature size with sampling based methods.
	fastf_t feature_size = 0.0; // Explicit feature length to try for sampling based methods - overrides feature-scale.
	fastf_t d_feature_size = 0.0; // Initial feature length to try for decimation in sampling based methods.  By default, this value is set to 1.5x the feature size.
	int max_sample_time = 30;
	int max_pnts = 200000;

	// Implementation variables
	double obj_bbox_vol;
	double pnts_bbox_vol;
	fastf_t target_feature_size;
	fastf_t avg_thickness;
};

class nmg_opts : public method_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(std::string &key, std::string &val);
	void sync(method_options_t &mopts);

	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	long nmg_debug = 0;
};

class cm_opts : public sample_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(std::string &key, std::string &val);
	void sync(method_options_t &mopts);
	void sync(sample_opts &sopts);

	int max_time = 30; // Maximum time to spend per processing step (in seconds).  Default is 30.  Zero means either the default (for routines which could run indefinitely) or run to completion (if there is a theoretical termination point for the algorithm).  Be careful when specifying zero - it can produce very long runs!
};

class spsr_opts : public sample_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(std::string &key, std::string &val);
	void sync(method_options_t &mopts);
	void sync(sample_opts &sopts);

	struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
	int depth = 8; // Maximum reconstruction depth s_opts.depth
	fastf_t interpolate = 2.0; // Lower values (down to 0.0) bias towards a smoother mesh, higher values bias towards interpolation accuracy. s_opts.point_weight
	fastf_t samples_per_node = 1.5; // How many samples should go into a cell before it is refined. s_opts.samples_per_node
};


#if defined(TESS_OPTS_IMPLEMENTATION)

std::string
method_options_t::method_optstr(std::string &method, struct db_i *dbip)
{
    if (!method.length())
	return std::string("");
    if (methods.find(method) == methods.end())
	return std::string("");
    if (options_map.find(method) == options_map.end() || options_map[method].size() == 0)
	if (method != std::string("NMG"))
	    return method;

    std::string moptstr = method;
    std::map<std::string,std::string>::iterator m_it;
    for (m_it = options_map[method].begin(); m_it != options_map[method].end(); m_it++) {
	moptstr.append(std::string(" "));
	std::string kvalstr = std::string(" ") + m_it->first + std::string("=") + m_it->second;
	moptstr.append(kvalstr);
    }

    if (method == std::string("NMG")) {
	// There are a few settings we want to make sure are passed to automatically
	// generated NMG method options if the user hasn't specifically overridden them
	if (options_map[method].find(std::string("nmg_debug")) == options_map[method].end()) {
	    struct bu_vls debug_str = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&debug_str, "0x%08lx", (unsigned long)nmg_debug);
	    std::string kvalstr = std::string(" nmg_debug=") + std::string(bu_vls_cstr(&debug_str));
	    moptstr.append(kvalstr);
	}
	if (dbip) {
	    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
	    if (options_map[method].find(std::string("tol_abs")) == options_map[method].end()) {
		struct bu_vls val_str = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&val_str, "%0.17f", wdbp->wdb_ttol.abs);
		std::string kvalstr = std::string(" tol_abs=") + std::string(bu_vls_cstr(&val_str));
		moptstr.append(kvalstr);
	    }
	    if (options_map[method].find(std::string("tol_rel")) == options_map[method].end()) {
		struct bu_vls val_str = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&val_str, "%0.17f", wdbp->wdb_ttol.rel);
		std::string kvalstr = std::string(" tol_rel=") + std::string(bu_vls_cstr(&val_str));
		moptstr.append(kvalstr);
	    }
	    if (options_map[method].find(std::string("tol_norm")) == options_map[method].end()) {
		struct bu_vls val_str = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&val_str, "%0.17f", wdbp->wdb_ttol.norm);
		std::string kvalstr = std::string(" tol_norm=") + std::string(bu_vls_cstr(&val_str));
		moptstr.append(kvalstr);
	    }
	}
    }

    return moptstr;
}

int
_tess_active_methods(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    method_options_t *m = (method_options_t *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "_tess_active_methods");

    std::string av0 = std::string(argv[0]);
    std::stringstream astream(av0);
    std::string s;
    std::vector<std::string> opts;
    while (std::getline(astream, s, ',')) {
	m->methods.insert(s);
    }
    return 1;
}

int
_tess_method_opts(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    method_options_t *m = (method_options_t *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "_tess_method_opts");

    std::string av0 = std::string(argv[0]);
    std::stringstream astream(av0);
    std::string s;
    std::vector<std::string> opts;
    while (std::getline(astream, s, ' ')) {
	opts.push_back(s);
    }

    for (size_t i = 1; i < opts.size(); i++) {
	std::string wopt = opts[i];
	std::stringstream ostream(wopt);
	std::string optstr;
	std::vector<std::string> key_val;
	while (std::getline(ostream, optstr, '=')) {
	    key_val.push_back(optstr);
	}
	if (key_val.size() != 2) {
	    bu_log("method options error!\n");
	    continue;
	}
	m->options_map[opts[0]][key_val[0]] = key_val[1];
    }
    return 1;
}


#endif // TESS_OPTS_IMPLEMENTATION

#endif // TESS_OPTS_H

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
