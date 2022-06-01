/*                    G I N S T A N C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file gInstance.cpp
 *
 * Lowest level expression of BRL-CAD .g instance data.
 *
 * The gInstance container translates implicit .g object instances into
 * explicit data that can be referenced by higher level model constructs.
 */

#include "common.h"

#include <set>
#include <unordered_set>

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include "bu/env.h"
#include "bu/sort.h"
#include "raytrace.h"
#include "qtcad/gInstance.h"

unsigned long long
ginstance_hash(XXH64_state_t *h_state, struct directory *parent, std::string &dp_name, struct db_i *dbip, db_op_t op, mat_t c_m, int cnt)
{
    if (!h_state)
	return 0;

    XXH64_hash_t hash_val;

    // Make sure the hash is tied to a particular database
    XXH64_update(h_state, &dbip, sizeof(struct db_i *));

    // For uniqueness
    XXH64_update(h_state, &cnt, sizeof(int));

    if (parent)
	XXH64_update(h_state, parent->d_namep, strlen(parent->d_namep));
    // We use dp_name instead of the dp because we want the same hash
    // whether we have a dp or an invalid comb entry - the hash lookup
    // should return both with the same key.
    if (dp_name.length())
	XXH64_update(h_state, dp_name.c_str(), dp_name.length());

    int int_op = 0;
    switch (op) {
	case DB_OP_UNION:
	    int_op = 1;
	    break;
	case DB_OP_SUBTRACT:
	    int_op = 2;
	    break;
	case DB_OP_INTERSECT:
	    int_op = 3;
	    break;
	default:
	    int_op = 0;
	    break;
    }
    XXH64_update(h_state, &int_op, sizeof(int));
    XXH64_update(h_state, c_m, sizeof(mat_t));

    hash_val = XXH64_digest(h_state);
    XXH64_reset(h_state, 0);

    return (unsigned long long)hash_val;
}


gInstance::gInstance(struct directory *idp, struct db_i *idbip)
{
    dp = idp;
    dbip = idbip;
}

gInstance::~gInstance()
{
}

std::string
gInstance::print()
{
    std::string s;
    if (parent) {
	s.append(std::string(parent->d_namep));
	s.append(std::string(" ->"));
    }
    switch (op) {
	case DB_OP_UNION:
	    s.append(std::string(" u "));
	    break;
	case DB_OP_SUBTRACT:
	    s.append(std::string(" - "));
	    break;
	case DB_OP_INTERSECT:
	    s.append(std::string(" + "));
	    break;
	default:
	    bu_log("Warning - unknown op");
	    break;
    }
    if (!bn_mat_is_identity(c_m)) {
	s.append(std::string("[M]"));
    }
    s.append(dp_name);
    if (!dp) {
	s.append(std::string("[missing]"));
    }

    return s;
}

static void
add_g_instance(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, int tree_op, void *pdp, void *inst_map, void *vchash, void *val_inst, void *c_set)
{
    std::unordered_map<unsigned long long, gInstance *>::iterator i_it;

    // Validate
    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE(comb_leaf);

    // Unpack
    std::unordered_map<unsigned long long, gInstance *> *instances = (std::unordered_map<unsigned long long, gInstance *> *)inst_map;
    std::unordered_map<unsigned long long, gInstance *> *valid_instances = (std::unordered_map<unsigned long long, gInstance *> *)val_inst;
    std::unordered_set<unsigned long long> *cnt_set = (std::unordered_set<unsigned long long> *)c_set;
    struct directory *parent_dp = (struct directory *)pdp;
    std::vector<unsigned long long> *chash = (std::vector<unsigned long long> *)vchash;

    // Translate the op into the db_op_t type
    struct directory *dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET);
    db_op_t op = DB_OP_UNION;
    if (tree_op == OP_SUBTRACT) {
	op = DB_OP_SUBTRACT;
    }
    if (tree_op == OP_INTERSECT) {
	op = DB_OP_INTERSECT;
    }

    // We need to do the lookup to see if we already have this instance stored
    // - to do the lookup we need the hash.
    mat_t c_m;
    MAT_IDN(c_m);
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    std::string dp_name(comb_leaf->tr_l.tl_name);

    // The cnt_set exists only for this tree walk, and its purpose is to ensure that all tree
    // instances end up with their own unique hash, even if they are duplicates from a data
    // standpoint within the tree
    int icnt = 0;
    unsigned long long nhash = ginstance_hash(&h_state, parent_dp, dp_name, dbip, DB_OP_UNION, c_m, icnt);
    std::unordered_set<unsigned long long>::iterator c_it;
    c_it = cnt_set->find(nhash);
    while (c_it != cnt_set->end()) {
	icnt++;
	nhash = ginstance_hash(&h_state, parent_dp, dp_name, dbip, DB_OP_UNION, c_m, icnt);
	c_it = cnt_set->find(nhash);
    }
    cnt_set->insert(nhash);

    // Having done the "unique lookup" hash to get icnt, we now redo it with
    // the op and matrix if they are different from the union/IDN case so the
    // hash is also tied to those properties of the tree entry.  A tree rewrite
    // may produce a new instance of the same name in the same count position,
    // but with a different op and/or matrix - we want the hash to change in
    // that situation to reflect the tree change.
    if (comb_leaf->tr_l.tl_mat || op != DB_OP_UNION) {
	if (comb_leaf->tr_l.tl_mat) {
	    MAT_COPY(c_m, comb_leaf->tr_l.tl_mat);
	}
	nhash = ginstance_hash(&h_state, parent_dp, dp_name, dbip, op, c_m, icnt);
    }

    // See if we already have this gInstance hash or not.  If not,
    // create and add a new gInstance.
    i_it = instances->find(nhash);
    if (i_it == instances->end()) {
	gInstance *ninst = new gInstance(dp, dbip);
	ninst->parent = parent_dp;
	ninst->hash = nhash;
	ninst->dp_name = std::string(comb_leaf->tr_l.tl_name);
	ninst->op = op;
	ninst->icnt = icnt;
	MAT_COPY(ninst->c_m, c_m);
	(*instances)[nhash] = ninst;
	if (valid_instances)
	    (*valid_instances)[nhash] = ninst;
    } else {
	if (valid_instances)
	    (*valid_instances)[i_it->first] = i_it->second;
    }

    // If we're collecting child hashes of the dp, push the hash onto the
    // vector to denote it as a child of the parent.
    if (chash)
	(*chash).push_back(nhash);
}

/* db_tree_funcleaf is almost what we need here, but not quite - it doesn't
 * pass quite enough information.  We need the parent op as well. */
static void
db_tree_opleaf(
	struct db_i *dbip,
	struct rt_comb_internal *comb,
	union tree *comb_tree,
	int op,
	void (*leaf_func)(struct db_i *, struct rt_comb_internal *, union tree *, int,
	    void *, void *, void *, void *, void *),
	void *user_ptr1,
	void *user_ptr2,
	void *user_ptr3,
	void *user_ptr4,
	void *user_ptr5
	)
{
    RT_CK_DBI(dbip);

    if (!comb_tree)
	return;

    RT_CK_TREE(comb_tree);

    switch (comb_tree->tr_op) {
	case OP_DB_LEAF:
	    leaf_func(dbip, comb, comb_tree, op, user_ptr1, user_ptr2, user_ptr3, user_ptr4, user_ptr5);
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_tree_opleaf(dbip, comb, comb_tree->tr_b.tb_left, OP_UNION, leaf_func, user_ptr1, user_ptr2, user_ptr3, user_ptr4, user_ptr5);
	    db_tree_opleaf(dbip, comb, comb_tree->tr_b.tb_right, comb_tree->tr_op, leaf_func, user_ptr1, user_ptr2, user_ptr3, user_ptr4, user_ptr5);
	    break;
	default:
	    bu_log("db_tree_opleaf: bad op %d\n", comb_tree->tr_op);
	    bu_bomb("db_tree_opleaf: bad op\n");
	    break;
    }
}

bool
gInstance::has_children()
{
    if (!dp)
	return false;

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return false;
	if (intern.idb_type != ID_COMBINATION)
	    return false;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	if (!comb->tree)
	    return false;
	if (!db_tree_nleaves(comb->tree))
	    return false;
	return true;
    }

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE)
	return true;
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE)
	return true;
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP)
	return true;

    return false;
}

std::vector<unsigned long long>
gInstance::children(std::unordered_map<unsigned long long, gInstance *> *instances)
{
    std::vector<unsigned long long> chash;
    if (!dp)
	return chash;

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return chash;
	if (intern.idb_type != ID_COMBINATION) {
	    bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n", dp->d_namep);
	    dp->d_flags &= ~RT_DIR_COMB;
	    rt_db_free_internal(&intern);
	    return chash;
	}

	// We do NOT want to entangle the details of the .g comb tree storage
	// into the model logic; instead we use the librt tree walkers to
	// populate data containers more friendly to the Qt data model.  This
	// does introduce resource overhead, and someday we may be able figure
	// out something more clever to be leaner, but at least in the early
	// stages Qt models offer enough complexity without trying to do
	// anything cute with the librt comb data containers.  Because we are
	// only concerned with the immediate comb's children and not the full
	// tree, all objects are leaves and we can use db_tree_opleaf
	std::unordered_set<unsigned long long> cnt_set;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	db_tree_opleaf(dbip, comb, comb->tree, OP_UNION, add_g_instance, (void *)dp, (void *)instances, (void *)&chash, NULL, &cnt_set);
	rt_db_free_internal(&intern);
	return chash;
    }

    std::unordered_map<unsigned long long, gInstance *>::iterator i_it;
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return chash;
	struct rt_extrude_internal *extr = (struct rt_extrude_internal *)intern.idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extr);
	if (extr->sketch_name) {
	    std::string sk_name(extr->sketch_name);
	    MAT_IDN(c_m);
	    unsigned long long nhash = ginstance_hash(&h_state, dp, sk_name, dbip, DB_OP_UNION, c_m, 0);
	    i_it = instances->find(nhash);
	    if (i_it != instances->end()) {
		chash.push_back(nhash);
	    }
	}
	return chash;
    }
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return chash;
	struct rt_revolve_internal *revolve = (struct rt_revolve_internal *)intern.idb_ptr;
	RT_REVOLVE_CK_MAGIC(revolve);
	if (bu_vls_strlen(&revolve->sketch_name) > 0) {
	    std::string sk_name(bu_vls_cstr(&revolve->sketch_name));
	    MAT_IDN(c_m);
	    unsigned long long nhash = ginstance_hash(&h_state, dp, sk_name, dbip, DB_OP_UNION, c_m, 0);
	    i_it = instances->find(nhash);
	    if (i_it != instances->end()) {
		chash.push_back(nhash);
	    }
	}
	return chash;
    }
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return chash;
	struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)intern.idb_ptr;
	RT_DSP_CK_MAGIC(dsp);
	if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen(&dsp->dsp_name) > 0) {
	    std::string dsp_name(bu_vls_cstr(&dsp->dsp_name));
	    MAT_IDN(c_m);
	    unsigned long long nhash = ginstance_hash(&h_state, dp, dsp_name, dbip, DB_OP_UNION, c_m, 0);
	    i_it = instances->find(nhash);
	    if (i_it != instances->end()) {
		chash.push_back(nhash);
	    }
	}
	return chash;
    }

    return chash;
}

static void
dp_instances(std::unordered_map<unsigned long long, gInstance *> *valid_instances, std::unordered_map<unsigned long long, gInstance *> *instances, struct directory *dp, struct db_i *dbip)
{
    mat_t c_m;
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    std::unordered_map<unsigned long long, gInstance *>::iterator i_it;

    if (dp->d_flags & RT_DIR_HIDDEN)
	return;

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_extrude_internal *extr = (struct rt_extrude_internal *)intern.idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extr);
	if (extr->sketch_name) {
	    std::string sk_name(extr->sketch_name);
	    MAT_IDN(c_m);
	    unsigned long long nhash = ginstance_hash(&h_state, dp, sk_name, dbip, DB_OP_UNION, c_m, 0);
	    i_it = instances->find(nhash);
	    if (i_it != instances->end()) {
		if (valid_instances)
		    (*valid_instances)[i_it->first] = i_it->second;
		rt_db_free_internal(&intern);
		return;
	    }
	    gInstance *ninst = new gInstance(db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET), dbip);
	    ninst->parent = dp;
	    ninst->hash = nhash;
	    ninst->dp_name = std::string(extr->sketch_name);
	    MAT_IDN(ninst->c_m);
	    ninst->op = DB_OP_UNION;
	    (*instances)[nhash] = ninst;
	    if (valid_instances)
		(*valid_instances)[nhash] = ninst;
	}
	rt_db_free_internal(&intern);
	return;
    }

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_revolve_internal *revolve = (struct rt_revolve_internal *)intern.idb_ptr;
	RT_REVOLVE_CK_MAGIC(revolve);
	if (bu_vls_strlen(&revolve->sketch_name) > 0) {
	    std::string sk_name(bu_vls_cstr(&revolve->sketch_name));
	    MAT_IDN(c_m);
	    unsigned long long nhash = ginstance_hash(&h_state, dp, sk_name, dbip, DB_OP_UNION, c_m, 0);
	    i_it = instances->find(nhash);
	    if (i_it != instances->end()) {
		if (valid_instances)
		    (*valid_instances)[i_it->first] = i_it->second;
		rt_db_free_internal(&intern);
		return;
	    }
	    gInstance *ninst = new gInstance(db_lookup(dbip, bu_vls_cstr(&revolve->sketch_name), LOOKUP_QUIET), dbip);
	    ninst->parent = dp;
	    ninst->hash = nhash;
	    ninst->dp_name = std::string(bu_vls_cstr(&revolve->sketch_name));
	    MAT_IDN(ninst->c_m);
	    ninst->op = DB_OP_UNION;
	    (*instances)[nhash] = ninst;
	    if (valid_instances)
		(*valid_instances)[nhash] = ninst;
	}
	rt_db_free_internal(&intern);
	return;
    }

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)intern.idb_ptr;
	RT_DSP_CK_MAGIC(dsp);
	if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen(&dsp->dsp_name) > 0) {
	    std::string dsp_name(bu_vls_cstr(&dsp->dsp_name));
	    MAT_IDN(c_m);
	    unsigned long long nhash = ginstance_hash(&h_state, dp, dsp_name, dbip, DB_OP_UNION, c_m, 0);
	    i_it = instances->find(nhash);
	    if (i_it != instances->end()) {
		if (valid_instances)
		    (*valid_instances)[i_it->first] = i_it->second;
		rt_db_free_internal(&intern);
		return;
	    }
	    gInstance *ninst = new gInstance(db_lookup(dbip, bu_vls_cstr(&dsp->dsp_name), LOOKUP_QUIET), dbip);
	    ninst->parent = dp;
	    ninst->hash = nhash;
	    ninst->dp_name = std::string(bu_vls_cstr(&dsp->dsp_name));
	    MAT_IDN(ninst->c_m);
	    ninst->op = DB_OP_UNION;
	    (*instances)[nhash] = ninst;
	    if (valid_instances)
		(*valid_instances)[nhash] = ninst;
	}
	rt_db_free_internal(&intern);
	return;
    }

    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;
	if (intern.idb_type != ID_COMBINATION) {
	    bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n", dp->d_namep);
	    dp->d_flags &= ~RT_DIR_COMB;
	    rt_db_free_internal(&intern);
	    return;
	}

	std::unordered_set<unsigned long long> cnt_set;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	db_tree_opleaf(dbip, comb, comb->tree, OP_UNION, add_g_instance, (void *)dp, (void *)instances, NULL, (void *)valid_instances, (void *)&cnt_set);
	rt_db_free_internal(&intern);
	return;
    }
}

void
sync_instances(
	std::unordered_map<unsigned long long, gInstance *> *tops_instances,
	std::unordered_map<unsigned long long, gInstance *> *instances,
	struct db_i *dbip)
{

    if (!dbip) {
	// If we don't have a dbip, our job is just to clear out everything
	for (size_t i = 0; i < instances->size(); i++) {
	    delete (*instances)[i];
	}
	instances->clear();
	tops_instances->clear();
	return;
    }

    std::unordered_map<unsigned long long, gInstance *>::iterator i_it;
    std::unordered_map<unsigned long long, gInstance *> valid_instances;

    // Run through the objects and crack the non-leaf objects to define
    // non-tops instances (i.e. all comb instances and some primitive
    // types that reference other objects).  By definition the instances
    // either present or created in this pass are the valid instances.
    //
    // This routine will collect all instances that are NOT tops instances.
    // Because tops instances are not below any other object, they must be
    // handled as implicit instances under the "root" .g, which requires a
    // slightly different setup as well as identifying the objects in question.
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & (RT_DIR_SOLID|RT_DIR_COMB))
		dp_instances(&valid_instances, instances, dp, dbip);
	}
    }

    // We have to go through the work of figuring this out anyway - just rebuild
    // the tops set cleanly each time
    tops_instances->clear();

    struct directory **db_objects = NULL;
    int path_cnt = db_ls(dbip, DB_LS_TOPS | DB_LS_CYCLIC , NULL, &db_objects);

    // TODO - to do a flat, "ls" style tree, all we need to do is switch the
    // above db_ls filters to the unfiltered version.  Need to make this a
    // user option.
    //int path_cnt = db_ls(dbip, 0, NULL, &db_objects);

    if (path_cnt) {
	XXH64_state_t h_state;
	XXH64_reset(&h_state, 0);
	mat_t c_m;
	MAT_IDN(c_m);
	for (int i = 0; i < path_cnt; i++) {
	    gInstance *tinst = NULL;
	    struct directory *curr_dp = db_objects[i];
	    std::string dname = std::string(curr_dp->d_namep);
	    unsigned long long nhash = ginstance_hash(&h_state, NULL, dname, dbip, DB_OP_UNION, c_m, 0);
	    i_it = instances->find(nhash);
	    if (i_it == instances->end()) {
		tinst = new gInstance(curr_dp, dbip);
		tinst->parent = NULL;
		tinst->hash = nhash;
		tinst->dp_name = std::string(curr_dp->d_namep);
		tinst->op = DB_OP_UNION;
		MAT_IDN(tinst->c_m);
		(*instances)[nhash] = tinst;
	    } else {
		tinst = i_it->second;
	    }
	    (*tops_instances)[nhash] = tinst;

	    // tops instances are valid
	    valid_instances[nhash] = tinst;
	}
    }
    bu_free(db_objects, "tops obj list");

    // instances now holds both the invalid and the valid instances.  To identify
    // and remove the invalid ones, we do a set difference.
    std::vector<gInstance *> removed;
    std::vector<unsigned long long> removed_hashes;
    for (i_it = instances->begin(); i_it != instances->end(); i_it++) {
	if (valid_instances.find(i_it->first) == valid_instances.end()) {
	    removed_hashes.push_back(i_it->first);
	    removed.push_back(i_it->second);
	}
    }
    for (size_t i = 0; i < removed_hashes.size(); i++) {
	instances->erase(removed_hashes[i]);
    }
    for (size_t i = 0; i < removed.size(); i++) {
	delete removed[i];
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

