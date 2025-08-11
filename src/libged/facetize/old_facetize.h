/*                  O L D _ F A C E T I Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/old_facetize.h
 *
 * The facetize command, circa v7.38.2
 *
 */

#include "common.h"
#include "vmath.h"
#include "bu.h"
#include "bg.h"

__BEGIN_DECLS

/* Ideally all of this could be in facetize.cpp, but the open() calls
 * used by the logging routines are problematic in C++ with Visual C++. */
struct _old_ged_facetize_opts {
    int quiet;
    int verbosity;
    int regions;
    int resume;
    int retry;
    int in_place;
    fastf_t feature_size;
    fastf_t feature_scale;
    fastf_t d_feature_size;
    int max_time;
    struct bg_tess_tol *tol;
    struct bu_vls *faceted_suffix;

    /* NMG specific options */
    int triangulate;
    int make_nmg;
    int nmgbool;
    int manifold;
    int screened_poisson;
    int continuation;
    int method_flags;
    int nmg_use_tnurbs;

    /* Poisson specific options */
    int max_pnts;
    struct bg_3d_spsr_opts s_opts;

    /* Brep specific options */
    double nonovlp_threshold;

    /* internal */
    struct bu_attribute_value_set *c_map;
    struct bu_attribute_value_set *s_map;
    struct bu_hook_list *saved_bomb_hooks;
    struct bu_hook_list *saved_log_hooks;
    struct bu_vls *nmg_log;
    struct bu_vls *nmg_log_header;
    int nmg_log_print_header;
    int stderr_stashed;
    int serr;
    int fnull;

    struct bu_vls *froot;
    struct bu_vls *manifold_comb;
    struct bu_vls *nmg_comb;
    struct bu_vls *continuation_comb;
    struct bu_vls *spsr_comb;
};

extern void
_old_ged_facetize_log_nmg(struct _old_ged_facetize_opts *o);

extern void
_old_ged_facetize_log_default(struct _old_ged_facetize_opts *o);

__END_DECLS

