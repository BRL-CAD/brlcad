/*                         P U S H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/push.c
 *
 * The push command.
 *
 */

#include "common.h"

#include <set>
#include <map>

#include "bu/cmd.h"
#include "bu/opt.h"

#include "../ged_private.h"

/* When it comes to push operations, the notion of what constitutes a unique
 * instance is defined as the combination of the object and its associated
 * parent matrix. Because the dp may be used under multiple matrices, for some
 * push operations it will be necessary to generate a new unique name to refer
 * to instances - we store those names with the instances for convenience. */
class dp_i {
    public:
	struct directory *dp;
	mat_t mat;
	std::string iname;
	const struct bn_tol *ts_tol;
	bool push_obj = true;

	bool operator<(const dp_i &o) const {
	    if (dp < o.dp) return true;
	    if (o.dp < dp) return false;
	    /* If the dp doesn't tell us, check the matrix. */
	    if (!bn_mat_is_equal(mat, o.mat, ts_tol)) {
		for (int i = 0; i < 16; i++) {
		    if (mat[i] < o.mat[i]) {
			return true;
		    }
		}
	    }
	    return false;
	}
};

/* Container to hold information during tree walk.  To generate names (or
 * recognize when a push would create a conflict) we keep track of how many
 * times we have encountered each dp during processing. */
struct push_state {
    bool survey = false;
    std::set<std::string> target_objs;
    std::set<dp_i> s_i;
    std::map<struct directory *, int> s_c;
    int verbosity = 0;
    int max_depth = 0;
    bool stop_at_regions = false;
    const struct bn_tol *tol;
};

// TODO - terminate non-survey walk according to leaf criteria for
// push operations - depth, region, etc.
static bool
is_push_leaf(struct directory *dp, int depth, struct push_state *s)
{
    if (!dp || !s || depth < 0) return true;

    if (s->survey) {
	// In a survey, if a leaf is one of the originally specified objects,
	// we're done - below such an object push logic is active.  A survey
	// needs to find objects not otherwise considered by the push.
	if (s->target_objs.find(std::string(dp->d_namep)) != s->target_objs.end()) {
	    return true;
	}
	return false;
    }

    /* Solids are always leaves, since there's nothing to walk */
    if (!(dp->d_flags & RT_DIR_COMB)) {
	return true;
    }

    /* Regions may be leaves, depending on user options */
    if ((dp->d_flags & RT_DIR_REGION) && s->stop_at_regions) {
	return true;
    }

    /* Depth halting may be active */
    if (s->max_depth && (depth >= s->max_depth))
	return true;

    return false;
}

static void
push_walk(struct db_i *dbip,
	    struct directory *dp,
	    void (*comb_func) (struct db_i *, struct directory *, int depth, mat_t *curr_mat, void *),
	    void (*leaf_func) (struct db_i *, struct directory *, int depth, mat_t *curr_mat, void *),
	    struct resource *resp,
	    int depth,
	    mat_t *curr_mat,
	    void *client_data);

static void
push_walk_subtree(struct db_i *dbip,
		    union tree *tp,
		    void (*comb_func) (struct db_i *, struct directory *, int depth, mat_t *curr_mat, void *),
		    void (*leaf_func) (struct db_i *, struct directory *, int depth, mat_t *curr_mat, void *),
		    struct resource *resp,
		    int depth,
		    mat_t *curr_mat,
		    void *client_data)
{
    struct directory *dp;
    struct push_state *s = (struct push_state *)client_data;
    mat_t om, nm;

    if (!tp)
	return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    if (resp) {
	RT_CK_RESOURCE(resp);
    }

    switch (tp->tr_op) {

	case OP_DB_LEAF:

	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
		return;

	    if (is_push_leaf(dp, depth, s)) {
		return;
	    }

	    /* Update current matrix state to reflect the new branch of
	     * the tree we are about to go down. */
	    MAT_COPY(om, *curr_mat);
	    if (tp->tr_l.tl_mat) {
		MAT_COPY(nm, tp->tr_l.tl_mat);
	    } else {
		MAT_IDN(nm);
	    }
	    bn_mat_mul(*curr_mat, om, nm);

	    /* Process branch. */
	    push_walk(dbip, dp, comb_func, leaf_func, resp, depth, curr_mat, client_data);

	    /* Done with branch - put back the old matrix state */
	    MAT_COPY(*curr_mat, om);
	    break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    push_walk_subtree(dbip, tp->tr_b.tb_left, comb_func, leaf_func, resp, depth, curr_mat, client_data);
	    push_walk_subtree(dbip, tp->tr_b.tb_right, comb_func, leaf_func, resp, depth, curr_mat, client_data);
	    break;
	default:
	    bu_log("push_walk_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("push_walk_subtree: unrecognized operator\n");
    }
}

static void
push_walk(struct db_i *dbip,
	    struct directory *dp,
	    void (*comb_func) (struct db_i *, struct directory *, int depth, mat_t *curr_mat, void *),
	    void (*leaf_func) (struct db_i *, struct directory *, int depth, mat_t *curr_mat, void *),
	    struct resource *resp,
	    int depth,
	    mat_t *curr_mat,
	    void *client_data)
{
    RT_CK_DBI(dbip);
    if (resp) {
	RT_CK_RESOURCE(resp);
    }

    if ((!dp) || (!comb_func && !leaf_func)) {
	return; /* nothing to do */
    }

    if (dp->d_flags & RT_DIR_COMB) {

	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal5(&in, dp, dbip, NULL, resp) < 0)
	    return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	push_walk_subtree(dbip, comb->tree, comb_func, leaf_func, resp, depth+1, curr_mat, client_data);
	rt_db_free_internal(&in);

	/* Finally, the combination itself */
	if (comb_func)
	    comb_func(dbip, dp, depth, curr_mat, client_data);

    } else if (dp->d_flags & RT_DIR_SOLID || dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK) {

	if (leaf_func)
	    leaf_func(dbip, dp, depth, curr_mat, client_data);

    } else {

	bu_log("push_walk:  %s is neither COMB nor SOLID?\n", dp->d_namep);

    }
}



static void
visit_comb_memb(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, void *data, void *cm, void *tdepth, void *UNUSED(u4))
{
    struct push_state *s = (struct push_state *)data;
    struct directory *dp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL)
	return;

    if (s->verbosity) {
	if (comb_leaf->tr_l.tl_mat) {
	    bu_log("Found comb entry: %s[M]\n", dp->d_namep);
	} else {
	    bu_log("Found comb entry: %s\n", dp->d_namep);
	}
	if (s->verbosity > 1) {
	    struct bu_vls title = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&title, "%s matrix", comb_leaf->tr_l.tl_name);
	    mat_t *curr_mat = (mat_t *)cm;
	    if (!bn_mat_is_equal(*curr_mat, bn_mat_identity, s->tol))
		bn_mat_print(bu_vls_cstr(&title), *curr_mat);
	    bu_vls_free(&title);
	}
    }

    dp_i idp;
    idp.dp = dp;
    idp.ts_tol = s->tol;
    idp.push_obj = !(s->survey);

    if (!s->survey) {
	/* If this is a "leaf" as far as the proposed push operation is
	 * concerned, either because of depth or because it satisfies
	 * other criteria, record the final matrix for application.  Otherwise,
	 * the matrix is an identity matrix since the push operation will
	 * keep going. */
	int tree_depth = *(int *)tdepth;
	if (is_push_leaf(dp, tree_depth, s)) {
	    if (comb_leaf->tr_l.tl_mat) {
		bn_mat_mul(idp.mat, *((mat_t *)cm), comb_leaf->tr_l.tl_mat);
	    } else {
		MAT_COPY(idp.mat, *((mat_t *)cm));
	    }
	} else {
	    MAT_IDN(idp.mat);
	}
    } else {
	/* In a survey of objects we are not pushing, we need to evaluate
	 * the object in isolation (i.e. using its global position, not the
	 * instance position.)  If we're evaluating a push operation,
	 * we're considering the instance with its evaluated matrix. */
	MAT_IDN(idp.mat);
    }

    if (s->s_i.find(idp) == s->s_i.end())
	s->s_i.insert(idp);
}

static void
process_comb(struct db_i *dbip, struct directory *dp, int depth, mat_t *curr_mat, void *data)
{
    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	return;
    }
    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    if (comb->tree)
	db_tree_funcleaf(dbip, comb, comb->tree, visit_comb_memb, data, (void *)curr_mat, (void *)&depth, NULL);
    rt_db_free_internal(&intern);
    struct push_state *s = (struct push_state *)data;
    if (s->verbosity) {
	bu_log("Processed comb (depth: %d): %s\n", depth, dp->d_namep);
    }
}

static void
process_solid(struct db_i *dbip, struct directory *dp, int depth, mat_t *UNUSED(curr_mat), void *data)
{
    if (!depth)
	return;

    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	return;
    }
    rt_db_free_internal(&intern);
    struct push_state *s = (struct push_state *)data;
    if (s->verbosity) {
	bu_log("Processed solid (depth: %d): %s\n", depth, dp->d_namep);
    }
}


static void
npush_usage(struct bu_vls *str, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: npush [options] obj\n");
    bu_vls_printf(str, "\nPushes position/rotation matrices 'down' the tree hierarchy, altering existing geometry as needed.  Default behavior clears all matrices from tree, unless push requires creation of new geometry objects.\n\n");
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}


extern "C" int
ged_npush_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int xpush = 0;
    int to_regions = 0;
    int to_solids = 0;
    int max_depth = 0;
    int verbosity = 0;
    struct bu_opt_desc d[9];
    BU_OPT(d[0], "h", "help",      "",   NULL,         &print_help,  "Print help and exit");
    BU_OPT(d[1], "?", "",          "",   NULL,         &print_help,  "");
    BU_OPT(d[2], "v", "verbosity",  "",  &bu_opt_incr_long, &verbosity,     "Increase output verbosity (multiple specifications of -v increase verbosity)");
    BU_OPT(d[3], "f", "force",     "",   NULL,         &xpush,       "Create new objects if needed to push matrices (xpush)");
    BU_OPT(d[4], "x", "xpush",     "",   NULL,         &xpush,       "");
    BU_OPT(d[5], "r", "regions",   "",   NULL,         &to_regions,  "Halt push at regions (matrix will be above region reference)");
    BU_OPT(d[6], "s", "solids",    "",   NULL,         &to_solids,   "Halt push at solids (matrix will be above solid reference)");
    BU_OPT(d[7], "d", "max-depth", "",   &bu_opt_int,  &max_depth,   "Halt at depth # from tree root (matrix will be above item # layers deep)");

    BU_OPT_NULL(d[8]);

    /* Skip command name */
    argc--; argv++;

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (!argc || print_help) {
	struct bu_vls npush_help = BU_VLS_INIT_ZERO;
	npush_usage(&npush_help, d);
	bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&npush_help));
	bu_vls_free(&npush_help);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    /* Need nref current for db_ls to work */
    db_update_nref(dbip, &rt_uniresource);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);


    /* Based on the push arg list, walk the specified trees to identify
     * instances involved with matrices */
    struct push_state s;
    s.verbosity = verbosity;
    s.tol = &gedp->ged_wdbp->wdb_tol;
    mat_t m;
    MAT_IDN(m);
    for (int i = 0; i < argc; i++) {
	s.target_objs.insert(std::string(argv[i]));
    }
    std::set<std::string>::iterator s_it;
    for (s_it = s.target_objs.begin(); s_it != s.target_objs.end(); s_it++) {
	struct directory *dp = db_lookup(dbip, s_it->c_str(), LOOKUP_NOISY);
	if (dp != RT_DIR_NULL)
	    push_walk(dbip, dp, process_comb, process_solid, &rt_uniresource, 0, &m, &s);
    }

    /* Sanity - if we didn't end up with m back at the identity matrix,
     * something went wrong with the walk */
    if (!bn_mat_is_equal(m, bn_mat_identity, &gedp->ged_wdbp->wdb_tol)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error - initial tree walk finished with non-IDN matrix.\n");
	return GED_ERROR;
    }


    /* Because pushes have potentially global consequences, we must
     * also characterize all unique object instances in the database.  That
     * information will be needed to know if any given matrix push is self
     * contained as-is or would require an object copy+rename to be isolated.
     */
    s.survey = true;
    struct directory **all_paths;
    int tops_cnt = db_ls(gedp->ged_wdbp->dbip, DB_LS_TOPS, NULL, &all_paths);
    for (int i = 0; i < tops_cnt; i++) {
	struct directory *dp = all_paths[i];
	if (s.target_objs.find(std::string(dp->d_namep)) == s.target_objs.end())
	    push_walk(dbip, dp, process_comb, process_solid, &rt_uniresource, 0, &m, &s);
    }
    bu_free(all_paths, "free db_ls output");

    // Once the survey walk is complete, iterate over s_i and count how many
    // instances of each dp are present.  Any dp with multiple instances can't
    // be pushed without a copy being made, as the dp instances represent
    // multiple volumes in space.
    std::set<dp_i>::iterator si_it;
    for (si_it = s.s_i.begin(); si_it != s.s_i.end(); si_it++) {
	const dp_i &dpi = *si_it;
	s.s_c[dpi.dp]++;
    }
    std::map<struct directory *, std::vector<dp_i>> dpref;
    std::set<dp_i> bpush;
    for (si_it = s.s_i.begin(); si_it != s.s_i.end(); si_it++) {
    	const dp_i *dpi = &(*si_it);
	if (dpi->push_obj) {
	    if (s.s_c[dpi->dp] > 1) {
		dpref[dpi->dp].push_back(*dpi);
	    } else {
		if (!bn_mat_is_equal(dpi->mat, bn_mat_identity, s.tol))
		    bpush.insert(*dpi);
	    }
	}
    }
    std::map<struct directory *, std::vector<dp_i>>::iterator d_it;
    for (d_it = dpref.begin(); d_it != dpref.end(); d_it++) {
	for (size_t i = 0; i < d_it->second.size(); i++) {
	    dp_i &sd = d_it->second[i];
	    std::string nname = std::string(d_it->first->d_namep) + std::string("_") + std::to_string(i);
	    sd.iname = nname;
	}
    }

    // See what we've got...
    if (dpref.size()) {
	std::cout << "Need renaming:\n"; 
	for (d_it = dpref.begin(); d_it != dpref.end(); d_it++) {
	    std::cout << d_it->first->d_namep << ":\n";
	    for (size_t i = 0; i < d_it->second.size(); i++) {
		dp_i &dpi = d_it->second[i];
		std::cout << dpi.iname << "\n";
		if (!bn_mat_is_equal(dpi.mat, bn_mat_identity, s.tol)) {
		    bn_mat_print(dpi.dp->d_namep, dpi.mat);
		}
	    }
	}
    }

    if (bpush.size()) {
	std::cout << "Push:\n"; 
	std::set<dp_i>::iterator b_it;
	for (b_it = bpush.begin(); b_it != bpush.end(); b_it++) {
	    const dp_i &dpi = *b_it;
	    if (!bn_mat_is_equal(dpi.mat, bn_mat_identity, s.tol)) {
		std::cout << dpi.dp->d_namep << "\n";
		bn_mat_print(dpi.dp->d_namep, dpi.mat);
	    }
	}
    }
 

    return GED_OK;
}

extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl npush_cmd_impl = {
    "npush",
    ged_npush_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd npush_cmd = { &npush_cmd_impl };
const struct ged_cmd *npush_cmds[] = { &npush_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  npush_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
