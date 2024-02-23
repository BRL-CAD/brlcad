/*                  T E S S E L L A T E . H
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
/** @file tessellate.h
 *
 * Private header for ged_tessellate executable.
 *
 */

#ifndef TESSELLATE_EXEC_H
#define TESSELLATE_EXEC_H

#include "common.h"

#ifdef __cplusplus
#include "manifold/manifold.h"
#endif

#include "vmath.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "raytrace.h"

__BEGIN_DECLS

#define FACETIZE_METHOD_NULL          0x0
#define FACETIZE_METHOD_NMG           0x1
#define FACETIZE_METHOD_CONTINUATION  0x2
#define FACETIZE_METHOD_SPSR          0x4
#define FACETIZE_METHOD_REPAIR        0x8

struct tess_opts {
    int nmg;
    int instant_mesh;
    int continuation;
    int ball_pivot;
    int Co3Ne;
    int screened_poisson;
    fastf_t feature_size;
    fastf_t fsize;
    fastf_t feature_scale;
    fastf_t d_feature_size;
    int overwrite_obj;
    int max_time;
    int max_pnts;
    struct bg_3d_spsr_opts s_opts;
    struct bg_tess_tol *ttol;
    struct bn_tol *tol;
    double obj_bbox_vol;
    double pnts_bbox_vol;
    fastf_t target_feature_size;
    fastf_t avg_thickness;
};

#define TESS_OPTS_DEFAULT {0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0, 0, 50000, BG_3D_SPSR_OPTS_DEFAULT, NULL, NULL, 0.0, 0.0, 0.0, 0.0}

class method_opts {
    public:
	virtual std::string about_method() = 0;
	virtual std::string print_options_help() = 0;
	virtual int set_var(std::string &ostr) = 0;
};

class sample_opts : public method_opts {
    public:
	std::string print_sample_options_help();

	fastf_t feature_scale = 0.15; // Percentage of the average thickness observed by the raytracer to use for a targeted feature size with sampling based methods.
	fastf_t feature_size = 0.0; // Explicit feature length to try for sampling based methods - overrides feature-scale.
	fastf_t d_feature_size = 0.0; // Initial feature length to try for decimation in sampling based methods.  By default, this value is set to 1.5x the feature size.
	int max_sample_time = 30;
	int max_pnts = 200000;
};

class nmg_opts : public method_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(std::string &ostr);

	fastf_t tol_abs = 0.0;
	fastf_t tol_rel = 0.0;
	fastf_t tol_norm = 0.0;
	long nmg_debug = 0;
};

class cm_opts : public sample_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(std::string &ostr);

	int max_time = 30; // Maximum time to spend per processing step (in seconds).  Default is 30.  Zero means either the default (for routines which could run indefinitely) or run to completion (if there is a theoretical termination point for the algorithm).  Be careful when specifying zero - it can produce very long runs!
};

class spsr_opts : public sample_opts {
    public:
	std::string about_method();
	std::string print_options_help();
	int set_var(std::string &ostr);

	int depth = 8; // Maximum reconstruction depth s_opts.depth
	fastf_t interpolate = 2.0; // Lower values (down to 0.0) bias towards a smoother mesh, higher values bias towards interpolation accuracy. s_opts.point_weight
	fastf_t samples_per_node = 1.5; // How many samples should go into a cell before it is refined. s_opts.samples_per_node
};


extern struct rt_bot_internal *
_tess_facetize_decimate(struct rt_bot_internal *bot, fastf_t feature_size);

extern int
_tess_facetize_write_bot(struct db_i *dbip, struct rt_bot_internal *bot, const char *name, int method_flag);

extern struct rt_pnts_internal *
_tess_pnts_sample(const char *oname, struct db_i *dbip, struct tess_opts *s);

extern int
continuation_mesh(struct rt_bot_internal **obot, struct db_i *dbip, const char *objname, struct tess_opts *s, point_t seed);

extern int
_ged_spsr_obj(struct ged *gedp, const char *objname, const char *newname);

__END_DECLS

#endif // TESSELLATE_EXEC_H

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
