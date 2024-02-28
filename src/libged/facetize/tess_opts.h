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

#include <map>
#include <set>
#include <vector>

#include "vmath.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/defines.h"
#include "bg/spsr.h"
#include "raytrace.h"

class method_options_t {
    public:

	method_options_t();
	~method_options_t();

	std::vector<std::string> methods;
	std::map<std::string, std::map<std::string,std::string>> options_map;
	// Most of the method options need to be passed through to the subprocess,
	// but the time each method is allowed to process is ultimately managed by
	// the parent command.  Some methods may be able to respect a time limit,
	// but for those that cannot the subprocess may need to be killed.
	std::map<std::string, int> max_time;

	// Routine to assemble an argument from an option_map entry.
	// May supplement information found there with info from a
	// .g database instance, if supplied.  Info in options_map
	// will take precedence
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
	virtual int set_var(const std::string &key, const std::string &val) = 0;
	virtual void sync(method_options_t &mopts) = 0;
};

class sample_opts : public method_opts {
    public:
	std::string about_method() { return std::string(""); }
	std::string print_options_help();
	int set_var(const std::string &key, const std::string &val);
	void sync(method_options_t &) {};
	void sync(sample_opts &o);
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
	int set_var(const std::string &key, const std::string &val);
	void sync(method_options_t &mopts);

	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	long nmg_debug = 0;
	int max_time = 30;
};

class cm_opts : public sample_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(const std::string &key, const std::string &val);
	void sync(method_options_t &mopts);
	void sync(sample_opts &sopts);

	int max_cycle_time = 30;  // Maximum length of a processing cycle to allow before finalizing
	int max_time = 600;  // Maximum overall time
};

class spsr_opts : public sample_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(const std::string &key, const std::string &val);
	void sync(method_options_t &mopts);
	void sync(sample_opts &sopts);

	struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
	int depth = 8; // Maximum reconstruction depth s_opts.depth
	fastf_t interpolate = 2.0; // Lower values (down to 0.0) bias towards a smoother mesh, higher values bias towards interpolation accuracy. s_opts.point_weight
	fastf_t samples_per_node = 1.5; // How many samples should go into a cell before it is refined. s_opts.samples_per_node
	int max_time = 600;  // Maximum overall time
};


#if defined(TESS_OPTS_IMPLEMENTATION)

#include "bu/opt.h"

method_options_t::method_options_t()
{
    nmg_opts n;
    cm_opts c;
    spsr_opts s;

    max_time[std::string("NMG")] = n.max_time;
    max_time[std::string("CM")] = c.max_time;
    max_time[std::string("SPSR")] = s.max_time;
}

method_options_t::~method_options_t()
{
}

std::string
method_options_t::method_optstr(std::string &method, struct db_i *dbip)
{
    if (!method.length())
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
    while (std::getline(astream, s, ',')) {
	m->methods.push_back(s);
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
	if (key_val[0] == std::string("max_time")) {
	    int max_time_val = 0;
	    const char *cstr[2];
	    cstr[0] = key_val[1].c_str();
	    cstr[1] = NULL;
	    if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_time_val) < 0)
		continue;
	    m->max_time[opts[0]] = max_time_val;
	}
    }
    return 1;
}

std::string
sample_opts::print_options_help()
{
    std::string h;
    h.append("feature_scale    -  Percentage of the average thickness observed by\n");
    h.append("                    the raytracer to use for a targeted feature size\n");
    h.append("                    with sampling based methods.\n");
    h.append("feature_size     -  Explicit feature length to try for sampling\n");
    h.append("                    based methods - overrides feature_scale.\n");
    h.append("d_feature_size   -  Initial feature length to try for decimation\n");
    h.append("                    in sampling based methods.  By default, this\n");
    h.append("                    value is set to 1.5x the feature size.\n");
    h.append("max_sample_time  -  Maximum time to allow point sampling to continue\n");
    h.append("max_pnts         -  Maximum number of points to sample\n");
    return h;
}

int
sample_opts::set_var(const std::string &key, const std::string &val)
{
    if (key.length() == 0)
	return BRLCAD_ERROR;

    const char *cstr[2];
    cstr[0] = val.c_str();
    cstr[1] = NULL;

    if (key == std::string("feature_scale")) {
	if (!val.length()) {
	    feature_scale = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&feature_scale) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("feature_size")) {
	if (!val.length()) {
	    feature_size = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&feature_size) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("d_feature_size")) {
	if (!val.length()) {
	    d_feature_size = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&d_feature_size) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("max_sample_time")) {
	if (!val.length()) {
	    max_sample_time = 0;
	    return BRLCAD_OK;
	}
	if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_sample_time) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("max_pnts")) {
	if (!val.length()) {
	    max_pnts = 0;
	    return BRLCAD_OK;
	}
	if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_pnts) < 0)
	    return BRLCAD_ERROR;
    }

    return BRLCAD_ERROR;
}

void
sample_opts::sync(sample_opts &o)
{
    feature_scale = o.feature_scale;
    feature_size = o.feature_size;
    d_feature_size = o.d_feature_size;
    max_sample_time = o.max_sample_time;
    max_pnts = o.max_pnts;
    obj_bbox_vol = o.obj_bbox_vol;
    pnts_bbox_vol = o.pnts_bbox_vol;
    target_feature_size = o.target_feature_size;
    avg_thickness = o.avg_thickness;
}

bool
sample_opts::equals(sample_opts &o)
{
    if (!NEAR_EQUAL(feature_scale, o.feature_scale, VUNITIZE_TOL))
	return false;
    if (!NEAR_EQUAL(feature_size, o.feature_size, VUNITIZE_TOL))
	return false;
    if (!NEAR_EQUAL(d_feature_size, o.d_feature_size, VUNITIZE_TOL))
	return false;
    if (max_sample_time != o.max_sample_time)
	return false;
    if (max_pnts != o.max_pnts)
	return false;

    return true;
}

std::string
cm_opts::about_method()
{
    std::string msg = "Continuation Method (Bloomenthal polygonizer)\n";
    return msg;
}

std::string
cm_opts::print_options_help()
{
    std::string h = std::string("Continuation (CM) Method Options:\n") + sample_opts::print_options_help();
    h.append("max_cycle_time   -  Maximum time to take for one processing cycle\n");
    h.append("                    Default is 30.  Zero means run until the target\n");
    h.append("                    size is met or other termination criteria kick\n");
    h.append("                    in.  Be careful when specifying zero - it can\n");
    h.append("                    produce very long runs!\n");
    h.append("max_time         -  Maximum overall run time for object conversion\n");
    return h;
}

int
cm_opts::set_var(const std::string &key, const std::string &val)
{
      if (key.length() == 0)
          return BRLCAD_ERROR;

      const char *cstr[2];
      cstr[0] = val.c_str();
      cstr[1] = NULL;

      if (key == std::string("max_cycle_time")) {
	  if (!val.length()) {
	      max_cycle_time = 0;
	      return BRLCAD_OK;
	  }
	  if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_cycle_time) < 0)
	      return BRLCAD_ERROR;
      }

      if (key == std::string("max_time")) {
	  if (!val.length()) {
	      max_time = 0;
	      return BRLCAD_OK;
	  }
	  if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_time) < 0)
	      return BRLCAD_ERROR;
      }

      // If it's not a CM setting directly, it may be for sampling
      return sample_opts::set_var(key, val);
}

void
cm_opts::sync(method_options_t &o)
{
    std::map<std::string, std::map<std::string,std::string>>::iterator o_it;
    o_it = o.options_map.find(std::string("CM"));
    if (o_it == o.options_map.end())
	return;

    std::map<std::string,std::string>::iterator m_it;
    for (m_it = o_it->second.begin(); m_it != o_it->second.end(); m_it++) {
	set_var(m_it->first, m_it->second);
    }
}

void
cm_opts::sync(sample_opts &opts)
{
    sample_opts::sync(opts);
}

std::string
nmg_opts::about_method()
{
    std::string msg = "N-Manifold Geometry (NMG) Method\n";
    return msg;
}

std::string
nmg_opts::print_options_help()
{
    std::string h;
    h.append("NMG method settings:\n");
    h.append("max_time         -  Maximum overall run time for object conversion\n");
    h.append("nmg_debug        -  NMG debugging flag.  Default is 0x00000000.\n");
    h.append("                    See 'debug -l NMG' in MGED for available values.\n");
    h.append("tol_abs          -  absolute distance tolerance.  Default is 0\n");
    h.append("tol_norm         -  normal tolerance.  Default is 0\n");
    h.append("tol_rel          -  relative distance tolerance.  Default is 0.01\n");
    return h;
}

int
nmg_opts::set_var(const std::string &key, const std::string &val)
{
    if (key.length() == 0)
	return BRLCAD_ERROR;

    const char *cstr[2];
    cstr[0] = val.c_str();
    cstr[1] = NULL;

    if (key == std::string("tol_rel")) {
	if (!val.length()) {
	    ttol.rel = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&ttol.rel) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("tol_abs")) {
	if (!val.length()) {
	    ttol.abs = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&ttol.abs) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("tol_norm")) {
	if (!val.length()) {
	    ttol.norm = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&ttol.norm) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("nmg_debug")) {
	if (!val.length()) {
	    nmg_debug = 0;
	    return BRLCAD_OK;
	}
	long ndebug = 0;
	if (bu_opt_long(NULL, 1, (const char **)cstr, (void *)&ndebug) < 0)
	    return BRLCAD_ERROR;
	nmg_debug = ndebug;
    }

    if (key == std::string("max_time")) {
	if (!val.length()) {
	    max_time = 0;
	    return BRLCAD_OK;
	}
	if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_time) < 0)
	    return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

void
nmg_opts::sync(method_options_t &o)
{
    std::map<std::string, std::map<std::string,std::string>>::iterator o_it;
    o_it = o.options_map.find(std::string("NMG"));
    if (o_it == o.options_map.end())
	return;

    std::map<std::string,std::string>::iterator m_it;
    for (m_it = o_it->second.begin(); m_it != o_it->second.end(); m_it++) {
	set_var(m_it->first, m_it->second);
    }
}

std::string
spsr_opts::about_method()
{
    std::string msg = "Screened Poisson Surface Reconstruction\n";
    return msg;
}

std::string
spsr_opts::print_options_help()
{
    std::string h = std::string("Screened Poisson Surface Reconstruction (SPSR) Options:\n") + sample_opts::print_options_help();
    h.append("depth            -  Maximum reconstruction depth. (Default is 8)\n");
    h.append("interpolate      -  Lower values (down to 0.0) bias towards a smoother\n");
    h.append("                    mesh, higher values bias towards interpolation\n");
    h.append("                    accuracy.  (Default is 2.0)\n");
    h.append("max_time         -  Maximum overall run time for object conversion\n");
    h.append("samples_per_node -  How many samples should go into a cell before it is\n");
    h.append("                    refined. (Default is 1.5)\n");
    return h;
}

int
spsr_opts::set_var(const std::string &key, const std::string &val)
{
    if (key.length() == 0)
	return BRLCAD_ERROR;

    const char *cstr[2];
    cstr[0] = val.c_str();
    cstr[1] = NULL;

    if (key == std::string("depth")) {
	if (!val.length()) {
	    s_opts.depth = 0;
	    return BRLCAD_OK;
	}
	if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&s_opts.depth) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("interpolate")) {
	if (!val.length()) {
	    s_opts.point_weight = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&s_opts.point_weight) < 0)
	    return BRLCAD_ERROR;
    }
    if (key == std::string("samples_per_node")) {
	if (!val.length()) {
	    s_opts.samples_per_node = 0.0;
	    return BRLCAD_OK;
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)cstr, (void *)&s_opts.samples_per_node) < 0)
	    return BRLCAD_ERROR;
    }

    if (key == std::string("max_time")) {
	if (!val.length()) {
	    max_time = 0;
	    return BRLCAD_OK;
	}
	if (bu_opt_int(NULL, 1, (const char **)cstr, (void *)&max_time) < 0)
	    return BRLCAD_ERROR;
    }

    // If it's not a SPSR setting directly, it may be for sampling
    return sample_opts::set_var(key, val);
}

void
spsr_opts::sync(method_options_t &o)
{
    std::map<std::string, std::map<std::string,std::string>>::iterator o_it;
    o_it = o.options_map.find(std::string("SPSR"));
    if (o_it == o.options_map.end())
	return;

    std::map<std::string,std::string>::iterator m_it;
    for (m_it = o_it->second.begin(); m_it != o_it->second.end(); m_it++) {
	set_var(m_it->first, m_it->second);
    }
}

void
spsr_opts::sync(sample_opts &opts)
{
    sample_opts::sync(opts);
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
