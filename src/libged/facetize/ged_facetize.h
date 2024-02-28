/*                  G E D _ F A C E T I Z E . H
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
/** @file ged_facetize.h
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

__BEGIN_DECLS

struct _ged_facetize_state {

    // Output
    int quiet;
    int verbosity;
    int make_nmg;
    int nonovlp_brep;

    // Processing
    int regions;
    int resume;
    int in_place;

    // Output Naming
    struct bu_vls *faceted_suffix;

    // Settings
    int max_time;
    int max_pnts;
    struct bg_tess_tol *tol;
    struct bu_vls *froot;

    /* Brep specific options */
    double nonovlp_threshold;

    /* Implementation */
    struct ged *gedp;

    union tree *facetize_tree;

    struct bu_attribute_value_set *c_map; // TODO - this should probably be a std::map
    struct bu_attribute_value_set *s_map; // TODO - this should probably be a std::map

    void *method_opts;
    void *log_s;
};
__END_DECLS

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

extern struct rt_bot_internal *
_ged_facetize_decimate(struct _ged_facetize_state *s, struct rt_bot_internal *bot, fastf_t feature_size);

extern int
_ged_facetize_nmgeval(struct _ged_facetize_state *s, int argc, const char **argv, const char *newname);

extern int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, struct directory **dpa, const char *newname, char *pwdir, char *pwfile);

extern int
_ged_continuation_obj(struct _ged_facetize_state *s, const char *objname, const char *newname);

extern int
_ged_spsr_obj(struct _ged_facetize_state *s, const char *objname, const char *newname);

extern int
_ged_facetize_write_bot(struct _ged_facetize_state *s, struct rt_bot_internal *bot, const char *name);

extern int
_ged_facetize_working_file_setup(char **wfile, char **wdir, struct db_i *dbip, struct bu_ptbl *leaf_dps, int resume);

extern int
_ged_facetize_leaves_tri(struct _ged_facetize_state *s, char *wfile, char *wdir, struct db_i *dbip, struct bu_ptbl *leaf_dps);

extern int
_ged_facetize_booleval_tri(struct _ged_facetize_state *s, struct db_i *dbip, struct rt_wdb *wdbp, int argc, const char **argv, const char *newname);

extern int
_ged_facetize_obj_swap(struct ged *gedp, const char *o, const char *n);

extern int _nonovlp_brep_facetize(struct _ged_facetize_state *s, int argc, const char **argv);

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
