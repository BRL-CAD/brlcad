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

#include "vmath.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "ged/defines.h"

__BEGIN_DECLS

#define FACETIZE_METHOD_ATTR "facetize_method"

struct _ged_facetize_state {

    // Output
    int verbosity;
    int no_empty;
    int make_nmg;
    int nonovlp_brep;
    int no_fixup;

    char *wdir;
    struct bu_vls *wfile;
    struct bu_vls *bname;
    struct bu_vls *log_file;
    FILE *lfile;

    // Processing
    int regions;
    int resume;
    int in_place;
    int nmg_booleval;

    // Settings
    int max_time;
    int max_pnts;
    struct bu_vls *prefix;
    struct bu_vls *suffix;

    /* Brep specific */
    struct bg_tess_tol *tol;
    double nonovlp_threshold;
    struct bu_vls *solid_suffix;

    /* Implementation */
    int error_flag;
    struct ged *gedp;
    struct db_i *dbip;
    union tree *facetize_tree;
    void *method_opts;
    void *log_s;
};

extern void
facetize_log(struct _ged_facetize_state *, int msg_level, const char *, ...) _BU_ATTR_PRINTF34;

extern int
_db_uniq_test(struct bu_vls *n, void *data);

extern int
_ged_validate_objs_list(struct _ged_facetize_state *s, int argc, const char *argv[], int newobj_cnt);

extern int
_ged_facetize_regions(struct _ged_facetize_state *s, int argc, const char **argv);

extern int
_ged_facetize_nmgeval(struct _ged_facetize_state *s, int argc, const char **argv, const char *newname);

extern int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, struct directory **dpa, const char *newname, bool output_to_working, bool cleanup);

extern int
_ged_facetize_write_bot(struct db_i *dbip, struct rt_bot_internal *bot, const char *name, int verbosity);

extern int
_ged_facetize_working_file_setup(struct _ged_facetize_state *s, struct bu_ptbl *leaf_dps);

extern int
_ged_facetize_leaves_tri(struct _ged_facetize_state *s, struct db_i *dbip, struct bu_ptbl *leaf_dps);

extern int
_ged_facetize_booleval_tri(struct _ged_facetize_state *s, struct db_i *dbip, struct rt_wdb *wdbp, int argc, const char **argv, const char *newname, struct bu_list *vlfree, bool output_to_working);

extern int _nonovlp_brep_facetize(struct _ged_facetize_state *s, int argc, const char **argv);

extern struct rt_bot_internal *
bot_fixup(struct _ged_facetize_state *s, struct db_i *wdbip, struct directory *bot_dp, const char *bname);

extern void
facetize_primitives_summary(struct _ged_facetize_state *s);

__END_DECLS

extern int
method_scan(std::map<std::string, std::set<std::string>> *method_sets, struct db_i *dbip);

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
