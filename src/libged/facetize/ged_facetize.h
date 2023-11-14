/*                  G E D _ F A C E T I Z E . H
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
/** @file ged_brep.h
 *
 * Private header for libged facetize cmd.
 *
 */

#ifndef LIBGED_FACETIZE_GED_PRIVATE_H
#define LIBGED_FACETIZE_GED_PRIVATE_H

#include "common.h"

#ifdef __cplusplus
#include "manifold/manifold.h"
#endif

#include "vmath.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "raytrace.h"

#define SOLID_OBJ_NAME 1
#define COMB_OBJ_NAME 2

#define FACETIZE_NULL  0x0
#define FACETIZE_NMG  0x1
#define FACETIZE_SPSR  0x2
#define FACETIZE_CONTINUATION  0x4

#define FACETIZE_SUCCESS 0
#define FACETIZE_FAILURE 1
#define FACETIZE_FAILURE_PNTGEN 2
#define FACETIZE_FAILURE_PNTBBOX 3
#define FACETIZE_FAILURE_BOTBBOX 4
#define FACETIZE_FAILURE_BOTINVALID 5
#define FACETIZE_FAILURE_DECIMATION 6
#define FACETIZE_FAILURE_NMG 7
#define FACETIZE_FAILURE_CONTINUATION_SURFACE 10
#define FACETIZE_FAILURE_SPSR_SURFACE 11
#define FACETIZE_FAILURE_SPSR_NONMATCHING 12

/* size of available memory (in bytes) below which we can't continue */
#define FACETIZE_MEMORY_THRESHOLD 150000000

__BEGIN_DECLS

/* Ideally all of this could be in facetize.cpp, but the open() calls
 * used by the logging routines are problematic in C++ with Visual C++. */

// NOTE - probably don't need the T/triangles option - it produces a
// triangles-only NMG, but to do that you could just facetize a BoT and then
// run facetize on that bot with the NMG output flag.  Normal usage is to
// produce a BoT anyway, so this almost certainly shouldn't be a toplevel
// option.

// NMG tnurbs aren't actively developed at this time - shouldn't expose as an
// option.  Anyone both interested in and able to work on that code will be
// able to set up what they need to trigger it...

struct _ged_facetize_logging_state {
    struct bu_hook_list *saved_bomb_hooks;
    struct bu_hook_list *saved_log_hooks;
    struct bu_vls *nmg_log;
    struct bu_vls *nmg_log_header;
    int nmg_log_print_header;
    int stderr_stashed;
    int serr;
    int fnull;
};

struct _ged_facetize_state {

    // Output
    int quiet;
    int verbosity;
    int make_nmg;
    int nonovlp_brep;

    // Processing
    int regions;
    int resume;
    int retry;
    int in_place;

    // Methodologies
    int no_nmg;           // method is on by default
    int no_continuation;  // method is on by default
    int screened_poisson; // off by default

    // Output Naming
    struct bu_vls *faceted_suffix;

    // Settings
    int max_time;
    int method_flags;
    fastf_t feature_size;
    fastf_t feature_scale;
    fastf_t d_feature_size;
    struct bg_tess_tol *tol;
    int max_pnts;
    struct bg_3d_spsr_opts s_opts;  // Screened Poisson specific settings

    /* Brep specific options */
    double nonovlp_threshold;

    /* Implementation */
    struct ged *gedp;

    union tree *facetize_tree;

    struct bu_attribute_value_set *c_map; // TODO - this should probably be a std::map
    struct bu_attribute_value_set *s_map; // TODO - this should probably be a std::map
    struct _ged_facetize_logging_state *log_s;
    struct bu_vls *froot;
    struct bu_vls *manifold_comb;
    struct bu_vls *nmg_comb;
    struct bu_vls *continuation_comb;
    struct bu_vls *spsr_comb;

    /* Internal - any C++ containers should be hidden here so
     * the C logging file doesn't choke on them */
    void *iptr;
};
__END_DECLS

#ifdef __cplusplus
#include <unordered_set>
struct facetize_maps {
    // Half spaces are handled differently depending on the boolean
    // op, so we need to ID them specifically.
    std::unordered_set<manifold::Manifold *> half_spaces;
};

#endif

extern int
bot_repair(void **out, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

extern int
plate_eval(void **out, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

extern int
manifold_tessellate(void **out, struct db_tree_state *tsp, const struct db_full_path *UNUSED(pathp), struct rt_db_internal *ip, void *data);

__BEGIN_DECLS
extern int _db_uniq_test(struct bu_vls *n, void *data);

extern int
_ged_manifold_do_bool(
        union tree *tp, union tree *tl, union tree *tr,
        int op, struct bu_list *vlfree, const struct bn_tol *tol, void *data);

extern void _ged_facetize_mkname(struct _ged_facetize_state *s, const char *n, int type);
extern int _ged_validate_objs_list(struct _ged_facetize_state *s, int argc, const char *argv[], int newobj_cnt);
extern int _ged_facetize_verify_solid(struct _ged_facetize_state *s, int argc, struct directory **dpa);

extern int _ged_facetize_cpcomb(struct _ged_facetize_state *s, const char *o);
extern int _ged_facetize_add_children(struct _ged_facetize_state *s, struct directory *cdp);
extern int _ged_facetize_regions(struct _ged_facetize_state *s, int argc, const char **argv);


extern int _ged_check_plate_mode(struct ged *gedp, struct directory *dp);

extern double _ged_facetize_bbox_vol(point_t b_min, point_t b_max);
extern void _ged_facetize_pnts_bbox(point_t rpp_min, point_t rpp_max, int pnt_cnt, point_t *pnts);
extern void _ged_facetize_rt_pnts_bbox(point_t rpp_min, point_t rpp_max, struct rt_pnts_internal *pnts);

extern struct rt_bot_internal *
_ged_facetize_decimate(struct _ged_facetize_state *s, struct rt_bot_internal *bot, fastf_t feature_size);

extern int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, const char **argv, const char *newname);

extern int
_ged_continuation_obj(struct _ged_facetize_state *s, const char *objname, const char *newname);

extern int
_ged_spsr_obj(struct _ged_facetize_state *s, const char *objname, const char *newname);

extern int
_ged_facetize_write_bot(struct _ged_facetize_state *s, struct rt_bot_internal *bot, const char *name);
extern int
_ged_facetize_write_nmg(struct _ged_facetize_state *s, struct model *nmg_model, const char *name);

extern int
_ged_facetize_obj_swap(struct ged *gedp, const char *o, const char *n);

extern int _nonovlp_brep_facetize(struct _ged_facetize_state *s, int argc, const char **argv);

extern void _ged_facetize_log_nmg(struct _ged_facetize_state *s);
extern void _ged_facetize_log_default(struct _ged_facetize_state *s);

__END_DECLS

#endif /* LIBGED_FACETIZE_GED_PRIVATE_H */

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
