/*                D B I _ S T A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 1990-2022 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/dbi_state.cpp
 *
 * Maintain and manage an in-memory representation of the database hierarchy.
 * Main utility of this is to make commonly needed hierarchy/attribute
 * information available to applications without having to crack combs from
 * disk during a standard tree walk.
 */

#include "common.h"

#include <unordered_map>
#include <unordered_set>

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"
}

#include "vmath.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "raytrace.h"
#include "./librt_private.h"

struct dbi_state_impl {

    // Db instance associated with this state
    struct db_i *dbip;

    // These maps are the ".g ground truth" of the comb structures - the set
    // associated with each has contains all the child hashes from the comb
    // definition in the database for quick lookup, and the vector preserves
    // the comb ordering for listing.
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> p_c;
    // Note: to match MGED's 'l' printing you need to use a reverse_iterator
    std::unordered_map<unsigned long long, std::vector<unsigned long long>> p_v;

    // Translate individual object hashes to their directory names.  This map must
    // be updated any time a database object changes to remain valid.
    std::unordered_map<unsigned long long, struct directory *> d_map;

    // For invalid comb entry strings, we can't point to a directory pointer.  This
    // map must also be updated after every db change - if a directory pointer hash
    // maps to an entry in this map it needs to be removed, and newly invalid entries
    // need to be added.
    std::unordered_map<unsigned long long, std::string> invalid_entry_map;

    // This is a map of non-uniquely named child instances (i.e. instances that must be
    // numbered) to the .g database name associated with those instances.  Allows for
    // one unique entry in p_c rather than requiring per-instance duplication
    std::unordered_map<unsigned long long, unsigned long long> i_map;
    std::unordered_map<unsigned long long, std::string> i_str;

    // Matrices above comb instances are critical to geometry placement.  For non-identity
    // matrices, we store them locally so they may be accessed without having to unpack
    // the comb from disk.
    std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, std::vector<fastf_t>>> matrices;

    // Similar to matrices, store non-union bool ops for instances
    std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, size_t>> i_bool;


    // Bounding boxes for each solid.  To calculate the bbox for a comb, the
    // children are walked combining the bboxes.  The idea is to be able to
    // retrieve solid bboxes and calculate comb bboxes without having to touch
    // the disk beyond the initial per-solid calculations, which may be done
    // once per load and/or dimensional change.
    std::unordered_map<unsigned long long, std::vector<fastf_t>> bboxes;


    // We also have a number of standard attributes that can impact drawing,
    // which are normally only accessible by loading in the attributes of
    // the object.  We stash them in maps to have the information available
    // without having to interrogate the disk
    std::unordered_map<unsigned long long, int> c_inherit; // color inheritance flag
    std::unordered_map<unsigned long long, unsigned int> rgb; // color RGB value  (r + (g << 8) + (b << 16))
    std::unordered_map<unsigned long long, int> region_id; // region_id


    // Data to be used by callbacks
    std::unordered_set<struct directory *> added;
    std::unordered_set<struct directory *> changed;
    std::unordered_set<unsigned long long> removed;
    std::unordered_map<unsigned long long, std::string> old_names;
};

struct walk_data {
    struct dbi_state *dbis;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    mat_t *curr_mat = NULL;
    unsigned long long phash = 0;
};

static void
populate_leaf(void *client_data, const char *name, matp_t c_m, int op)
{
    struct walk_data *d = (struct walk_data *)client_data;
    struct db_i *dbip = d->dbis->i->dbip;
    RT_CHECK_DBI(dbip);

    std::unordered_map<unsigned long long, unsigned long long> &i_count = d->i_count;
    XXH64_state_t h_state;
    unsigned long long chash;
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, name, strlen(name)*sizeof(char));
    chash = (unsigned long long)XXH64_digest(&h_state);
    i_count[chash] += 1;
    if (i_count[chash] > 1) {
	// If we've got multiple instances of the same object in the tree,
	// hash the string labeling the instance and map it to the correct
	// parent comb so we can associate it with the tree contents
	struct bu_vls iname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&iname, "%s@%llu", name, i_count[chash]);
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, bu_vls_cstr(&iname), bu_vls_strlen(&iname)*sizeof(char));
	unsigned long long ihash = (unsigned long long)XXH64_digest(&h_state);
	d->dbis->i->i_map[ihash] = chash;
	d->dbis->i->i_str[ihash] = std::string(bu_vls_cstr(&iname));
	d->dbis->i->p_c[d->phash].insert(ihash);
	d->dbis->i->p_v[d->phash].push_back(ihash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->dbis->i->invalid_entry_map[ihash] = std::string(bu_vls_cstr(&iname));
	} else {
	    // In case this was previously invalid, remove
	    d->dbis->i->invalid_entry_map.erase(ihash);
	}
	bu_vls_free(&iname);

	// For the next stages, if we have an ihash use it
	chash = ihash;

    } else {

	d->dbis->i->p_v[d->phash].push_back(chash);
	d->dbis->i->p_c[d->phash].insert(chash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->dbis->i->invalid_entry_map[chash] = std::string(name);
	} else {
	    // In case this was previously invalid, remove
	    d->dbis->i->invalid_entry_map.erase(chash);
	}

    }

    // If we have a non-IDN matrix, store it
    if (c_m) {
	for (int i = 0; i < 16; i++)
	    d->dbis->i->matrices[d->phash][chash].push_back(c_m[i]);
    }

    // If we have a non-UNION op, store it
    d->dbis->i->i_bool[d->phash][chash] = op;
}

static void
populate_walk_tree(union tree *tp, void *d, int subtract_skip,
	void (*leaf_func)(void *, const char *, matp_t, int)
	)
{
    if (!tp)
	return;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_SUBTRACT:
	    if (subtract_skip)
		return;
	    /* fall through */
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
	    populate_walk_tree(tp->tr_b.tb_right, d, subtract_skip, leaf_func);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    populate_walk_tree(tp->tr_b.tb_left, d, subtract_skip, leaf_func);
	    break;
	case OP_DB_LEAF:
	    (*leaf_func)(d, tp->tr_l.tl_name, tp->tr_l.tl_mat, tp->tr_op);
	    break;
	default:
	    bu_log("unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("draw walk\n");
    }
}


static void
populate_maps(struct dbi_state *dbis, struct directory *dp, unsigned long long phash, int reset)
{
    if (!(dp->d_flags & RT_DIR_COMB))
	return;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
    pc_it = dbis->i->p_c.find(phash);
    pv_it = dbis->i->p_v.find(phash);
    if (pc_it == dbis->i->p_c.end() || pv_it != dbis->i->p_v.end() || reset) {
	if (reset && pc_it != dbis->i->p_c.end()) {
	    pc_it->second.clear();
	}
	if (reset && pv_it != dbis->i->p_v.end()) {
	    pv_it->second.clear();
	}
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, dbis->i->dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree)
	    return;

	std::unordered_map<unsigned long long, unsigned long long> i_count;
	struct walk_data d;
	d.dbis = dbis;
	d.phash = phash;
	populate_walk_tree(comb->tree, (void *)&d, 0, populate_leaf);
	rt_db_free_internal(&in);
    }
}

static unsigned int
color_int(struct bu_color *c)
{
    int r, g, b;
    bu_color_to_rgb_ints(c, &r, &g, &b);
    unsigned int colors = r + (g << 8) + (b << 16);
    return colors;
}

int
int_color(struct bu_color *c, unsigned int cval)
{
    if (!c)
	return 0;
    int r, g, b;
    r = cval & 0xFF;
    g = (cval >> 8) & 0xFF;
    b = (cval >> 16) & 0xFF;

    unsigned char rgb[3];
    rgb[0] = (unsigned char)r;
    rgb[1] = (unsigned char)g;
    rgb[2] = (unsigned char)b;

    return bu_color_from_rgb_chars(c, rgb);
}

static unsigned long long
update_dp(struct dbi_state *dbis, struct directory *dp, int reset)
{
    if (dp->d_flags & DB_LS_HIDDEN)
	return 0;

    // Set up to go from hash back to name
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
    unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);
    dbis->i->d_map[hash] = dp;

    // If this isn't a comb, find and record the untransformed
    // bounding box
    dbis->i->bboxes.erase(hash);
    if (!(dp->d_flags & RT_DIR_COMB)) {
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
	struct bn_tol tol = BG_TOL_INIT;
	point_t bmin, bmax;
	mat_t m;
	MAT_IDN(m);
	int bret = rt_bound_instance(&bmin, &bmax, dp, dbis->i->dbip,
		&ttol, &tol, &m, &rt_uniresource, NULL);
	if (bret != -1) {
	    for (size_t j = 0; j < 3; j++)
		dbis->i->bboxes[hash].push_back(bmin[j]);
	    for (size_t j = 0; j < 3; j++)
		dbis->i->bboxes[hash].push_back(bmax[j]);
	}
    }

    // Encode hierarchy info if this is a comb
    if (dp->d_flags & RT_DIR_COMB)
	populate_maps(dbis, dp, hash, reset);

    // Check for various drawing related attributes
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
    db5_get_attributes(dbis->i->dbip, &c_avs, dp);

    // Check for region id.  For drawing purposes this needs to be a number.
    const char *region_id_val = bu_avs_get(&c_avs, "region_id");
    int region_id = -1;
    if (region_id_val) {
	if (bu_opt_int(NULL, 1, &region_id_val, (void *)&region_id) != -1) {
	    dbis->i->region_id[hash] = region_id;
	} else {
	    bu_log("%s is not a valid region_id\n", region_id_val);
	    region_id = -1;
	}
    }

    // Check for an inherit flag
    int color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
    dbis->i->c_inherit.erase(hash);
    if (color_inherit)
	dbis->i->c_inherit[hash] = color_inherit;

    // Color (note that the rt_material_head colors and a region_id may
    // override this, as might a parent comb with color and the inherit
    // flag both set.
    dbis->i->rgb.erase(hash);
    struct bu_color c = BU_COLOR_INIT_ZERO;
    const char *color_val = bu_avs_get(&c_avs, "color");
    if (!color_val)
	color_val = bu_avs_get(&c_avs, "rgb");
    if (color_val){
	bu_opt_color(NULL, 1, &color_val, (void *)&c);
	unsigned int cval = color_int(&c);
	dbis->i->rgb[hash] = cval;
    }

    // Done with attributes
    bu_avs_free(&c_avs);

    return hash;
}


struct dbi_state *
dbi_state_create(struct db_i *dbip)
{
    if (!dbip)
	return NULL;

    struct dbi_state *dbis;
    BU_GET(dbis, struct dbi_state);

    dbis->i = new struct dbi_state_impl;

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    update_dp(dbis, dp, 0);
	}
    }

    return dbis;
}

void
dbi_state_destroy(struct dbi_state *dbis)
{
    if (!dbis)
	return;

    delete dbis->i;
    dbis->i = NULL;
    BU_PUT(dbis, struct dbi_state);
}



/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
