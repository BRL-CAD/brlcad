/*                  G E D _ F A C E T I Z E . H
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
/** @file ged_facetize.h
 *
 * Private header for libged facetize cmd.
 *
 */

#ifndef LIBGED_FACETIZE_GED_PRIVATE_H
#define LIBGED_FACETIZE_GED_PRIVATE_H

#include "common.h"

#include <map>
#include <set>
#include <string>
#include <vector>

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
    int no_perturb;
    int use_variant_plan;

    /* Perturb validation thresholds (percentage, 0–100).
     * Trigger the perturb retry when the CSG–BoT difference exceeds these
     * values.  Defaults are 10 % for both surface area and volume. */
    fastf_t perturb_sa_tol;
    fastf_t perturb_vol_tol;

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

    /* Instance-aware variant plan (FacetizeVariantPlan *, NULL until planning) */
    void *variant_plan;
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

/**
 * Per-instance variant assignment plan for coplanar face avoidance.
 *
 * Produced by _ged_facetize_build_variant_plan() before tessellation.
 * Consumed by _booltree_leaf_tess() during Manifold boolean evaluation.
 */
struct FacetizeVariantPlan {
    /**
     * Maps (db_path_to_string() path key + "#base" or "#sub") → variant name
     * in the working .g.
     * The role suffix disambiguates the same primitive appearing in both a
     * UNION and a SUBTRACT branch at the same tree depth (e.g. "u A … - A").
     * BASE variants use a small perturbation to break coplanarity.
     * SUB variants use a larger perturbation to clear subtraction slivers.
     */
    std::map<std::string, std::string> inst_to_variant;

    /** Variant primitive names that still need NMG tessellation. */
    std::vector<std::string> variant_names;

    /**
     * Per-variant reconstruction record.
     * Used at Pass 2 validation time to recreate the perturbed CSG in an
     * in-memory database (from the original s->dbip primitives) so that
     * Crofton operates on true parametric geometry, not BoTs.
     */
    struct VariantRec {
        std::string src_name;  /**< primitive name in s->dbip to perturb from */
        fastf_t     factor;    /**< perturbation factor actually applied */
    };
    std::map<std::string, VariantRec> variant_recs;  /**< vname → rec */

    /* Reporting counters */
    int n_adjusted_instances;    /**< instances with a variant assigned */
    int n_sub_variants;          /**< subtractive instances with a variant */
    int n_perturb_fallbacks;     /**< instances where ft_perturb is unavailable */
    int n_variant_tess_failures; /**< variant tessellation failures */

    FacetizeVariantPlan()
        : n_adjusted_instances(0), n_sub_variants(0),
          n_perturb_fallbacks(0), n_variant_tess_failures(0) {}
};

/**
 * Walk the source .g tree for each directory pointer in dpa[], recording every
 * leaf instance with its full path key, boolean role (subtractive or not), and
 * primitive type.  Then create perturbed variant primitives in the working .g
 * (via ft_perturb hooks for ARB8/ARBN/TGC/ELL/SPH/TOR) and build the role-keyed
 * lookup table.
 *
 * The inst_to_variant key is path + "#base" or "#sub", allowing the same
 * primitive at the same tree depth to have distinct BASE and SUB variants.
 *
 * Returns an allocated FacetizeVariantPlan owned by the caller (or NULL on
 * allocation failure).  Primitives without ft_perturb support are counted in
 * n_perturb_fallbacks and will fall back to the original mesh at booleval time.
 */
extern FacetizeVariantPlan *
_ged_facetize_build_variant_plan(struct _ged_facetize_state *s,
                                 int argc,
                                 struct directory **dpa);

/**
 * Tessellate the variant primitives in the working .g using the NMG method.
 * Called after _ged_facetize_leaves_tri() so original leaves are already BoTs.
 * Updates plan->n_variant_tess_failures for any variants that could not be
 * tessellated (they will silently fall back to the original mesh at booleval).
 */
extern int
_ged_facetize_tessellate_variant_names(struct _ged_facetize_state *s,
                                       FacetizeVariantPlan *plan);

/** Forward declaration for use by plan.cpp */
extern int
tess_run(struct _ged_facetize_state *s,
         const char **tess_cmd,
         int tess_cmd_cnt,
         fastf_t max_time,
         int ocnt);

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
