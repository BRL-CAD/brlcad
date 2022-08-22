/*                    T E S T _ D R A W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file test_draw.cpp
 *
 * Experiment with approaches for managing drawing and selecting
 *
 */

#include "common.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <vector>

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"
}

#include <stdio.h>
#include <bu.h>
#include <bg/lod.h>
#include <ged.h>

struct draw_ctx {
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




    // The above containers are universal to the .g - the following will need
    // both shared and view specific instances

    // Sets defining all drawn solid paths (including invalid paths).  The
    // s_keys holds the ordered individual keys of each drawn solid path - it
    // is the latter that allows for the collapse operation to populate
    // drawn_paths.  s_map uses the same key as s_keys to map instances to
    // actual scene objects.
    std::unordered_map<unsigned long long, struct bv_scene_obj *> s_map;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>> s_keys;

    // Set of hashes of all drawn paths and subpaths, constructed during the collapse
    // operation from the set of drawn solid paths.  This allows calling codes to
    // spot check any path to see if it is active, without having to interrogate
    // other data structures or walk down the tree.
    std::unordered_set<unsigned long long> drawn_paths;
};

struct g_ctx {
    struct ged *gedp = NULL;
    std::unordered_set<unsigned long long> added;
    std::unordered_set<unsigned long long> removed;
    std::unordered_set<unsigned long long> changed;
    std::unordered_map<unsigned long long, std::string> old_names;
    bool need_update_nref = false;
    bool changed_flag = false;
};


extern "C" void
dp_changed_clbk(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    struct g_ctx *gd = (struct g_ctx *)u_data;

    // When this callback is made, dp is still valid, but in subsequent processing
    // it will not be.  We need to capture everything we will need from this dp
    // now, for later use when updating state
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
    unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);

    gd->need_update_nref = true;
    gd->changed_flag = true;

    // Need to invalidate any LoD caches associated with this dp
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && gd->gedp) {
	unsigned long long key = bg_mesh_lod_key_get(gd->gedp->ged_lod, dp->d_namep);
	if (key) {
	    bg_mesh_lod_clear_cache(gd->gedp->ged_lod, key);
	    bg_mesh_lod_key_put(gd->gedp->ged_lod, dp->d_namep, 0);
	}
    }

    switch(mode) {
	case 0:
	    gd->changed.insert(hash);
	    break;
	case 1:
	    gd->added.insert(hash);
	    break;
	case 2:
	    gd->removed.insert(hash);
	    gd->old_names[hash] = std::string(dp->d_namep);
	    break;
	default:
	    bu_log("changed callback mode error: %d\n", mode);
    }
}



unsigned int
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

bool
get_path_color(struct bu_color *c, struct draw_ctx *ctx, std::vector<unsigned long long> &elements)
{
    // This may not be how we'll always want to do this, but at least for the
    // moment (to duplicate observed MGED behavior) the first region_id seen
    // along the path with an active color in rt_material_head trumps all other
    // color values set by any other means.
    if (rt_material_head() != MATER_NULL) {
	std::unordered_map<unsigned long long, int>::iterator r_it;
	int region_id;
	for (size_t i = 0; i < elements.size(); i++) {
	    r_it = ctx->region_id.find(elements[i]);
	    if (r_it == ctx->region_id.end())
		continue;
	    region_id = r_it->second;
	    const struct mater *mp;
	    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
		if (region_id > mp->mt_high || region_id < mp->mt_low)
		    continue;
		unsigned char mt[3];
		mt[0] = mp->mt_r;
		mt[1] = mp->mt_g;
		mt[2] = mp->mt_b;
		bu_color_from_rgb_chars(c, mt);
		return true;
	    }
	}
    }

    // Next, check for an inherited color.  If we have one (the behavior seen in MGED
    // appears to require a comb with both inherit and a color value set to override
    // lower colors) then we are done.
    std::unordered_map<unsigned long long, int>::iterator ci_it;
    std::unordered_map<unsigned long long, unsigned int>::iterator rgb_it;
    for (size_t i = 0; i < elements.size(); i++) {
	ci_it = ctx->c_inherit.find(elements[i]);
	if (ci_it == ctx->c_inherit.end())
	    continue;
	rgb_it = ctx->rgb.find(elements[i]);
	if (rgb_it == ctx->rgb.end())
	    continue;
	int_color(c, rgb_it->second);
	return true;
    }

    // If we don't have an inherited color, it works the other way around - the
    // lowest set color wins.  Note that a region flag doesn't automatically
    // override a lower color level - i.e. there is no implicit inherit flag
    // in a region being set on a comb.
    std::vector<unsigned long long>::reverse_iterator v_it;
    for (v_it = elements.rbegin(); v_it != elements.rend(); v_it++) {
	rgb_it = ctx->rgb.find(*v_it);
	if (rgb_it == ctx->rgb.end())
	    continue;
	int_color(c, rgb_it->second);
	return true;
    }

    // If we don't have anything else, default to red
    unsigned char mt[3];
    mt[0] = 255;
    mt[1] = 0;
    mt[2] = 0;
    bu_color_from_rgb_chars(c, mt);
    return false;
}

static bool
get_matrix(matp_t m, struct draw_ctx *ctx, unsigned long long p_key, unsigned long long i_key)
{
    if (UNLIKELY(!m || !ctx || p_key == 0 || i_key == 0))
	return false;

    std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, std::vector<fastf_t>>>::iterator m_it;
    std::unordered_map<unsigned long long, std::vector<fastf_t>>::iterator mv_it;
    m_it = ctx->matrices.find(p_key);
    if (m_it == ctx->matrices.end())
	return false;
    mv_it = m_it->second.find(i_key);
    if (mv_it == m_it->second.end())
	return false;

    // If we got this far, we have an index into the matrices vector.  Assign
    // the result to m
    std::vector<fastf_t> &mv = mv_it->second;
    for (size_t i = 0; i < 16; i++)
	m[i] = mv[i];

    return true;
}

static bool
get_bbox(point_t *bbmin, point_t *bbmax, struct draw_ctx *ctx, matp_t curr_mat, unsigned long long hash)
{

    if (UNLIKELY(!bbmin || !bbmax || !ctx || hash == 0))
	return false;

    bool ret = false;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_set<unsigned long long>::iterator s_it;
    unsigned long long key = hash;
    // First, see if this is an instance we need to translate to its canonical
    // .g database name
    if (ctx->i_map.find(hash) != ctx->i_map.end())
	key = ctx->i_map[hash];

    // See if we have a direct bbox lookup available
    std::unordered_map<unsigned long long, std::vector<fastf_t>>::iterator b_it;
    b_it = ctx->bboxes.find(key);
    if (b_it != ctx->bboxes.end()) {
	point_t lbmin, lbmax;
	lbmin[X] = b_it->second[0];
	lbmin[Y] = b_it->second[1];
	lbmin[Z] = b_it->second[2];
	lbmax[X] = b_it->second[3];
	lbmax[Y] = b_it->second[4];
	lbmax[Z] = b_it->second[5];

	if (curr_mat) {
	    point_t tbmin, tbmax;
	    MAT4X3PNT(tbmin, curr_mat, lbmin);
	    VMOVE(lbmin, tbmin);
	    MAT4X3PNT(tbmax, curr_mat, lbmax);
	    VMOVE(lbmax, tbmax);
	}

	VMINMAX(*bbmin, *bbmax, lbmin);
	VMINMAX(*bbmin, *bbmax, lbmax);
	return true;
    }

    // We might have a comb.  If that's the case, we need to work
    // through the hierarchy to get the bboxes of the children.  When
    // we have an object that is not a comb, look up its pre-calculated
    // box and incorporate it into bmin/bmax.
    pc_it = ctx->p_c.find(key);
    if (pc_it != ctx->p_c.end()) {
	// Have comb children - incorporate each one
	for (s_it = pc_it->second.begin(); s_it != pc_it->second.end(); s_it++) {
	    unsigned long long child_hash = *s_it;
	    // See if we have a matrix for this case - if so, we need to
	    // incorporate it
	    mat_t nm;
	    bool have_mat = get_matrix(nm, ctx, key, child_hash);
	    if (have_mat) {
		// Construct new "current" matrix
		if (curr_mat) {
		    // If we already have a non-IDN matrix from parent
		    // path elements, we need to multiply the matrices
		    // to accumulate the position changes
		    mat_t om;
		    MAT_COPY(om, curr_mat);
		    bn_mat_mul(curr_mat, om, nm);
		    if (get_bbox(bbmin, bbmax, ctx, curr_mat, child_hash))
			ret = true;
		    MAT_COPY(curr_mat, om);
		} else {
		    // If this is the first non-IDN matrix, we don't
		    // need to combine it with parent matrices
		    if (get_bbox(bbmin, bbmax, ctx, nm, child_hash))
			ret = true;
		}
	    } else {
		if (get_bbox(bbmin, bbmax, ctx, curr_mat, child_hash))
		    ret = true;
	    }
	}
    }

    return ret;
}

static bool
get_path_bbox(point_t *bbmin, point_t *bbmax, struct draw_ctx *ctx, std::vector<unsigned long long> &elements)
{
    if (UNLIKELY(!bbmin || !bbmax || !ctx || !elements.size()))
	return false;

    // Everything but the last element should be a comb - we only need to
    // assemble a matrix from the path (if there are any non-identity matrices)
    // and call get_bbox on the last element.
    bool have_mat = false;
    mat_t start_mat;
    MAT_IDN(start_mat);
    for (size_t i = 0; i < elements.size() - 1; i++) {
	mat_t nm;
	bool got_mat = get_matrix(nm, ctx, elements[i], elements[i+1]);
	if (got_mat) {
	    mat_t cmat;
	    bn_mat_mul(cmat, start_mat, nm);
	    MAT_COPY(start_mat, cmat);
	    have_mat = true;
	}
    }
    if (have_mat) {
	return get_bbox(bbmin, bbmax, ctx, start_mat, elements[elements.size() - 1]);
    }

    return get_bbox(bbmin, bbmax, ctx, NULL, elements[elements.size() - 1]);
}

void
print_path(struct bu_vls *opath, std::vector<unsigned long long> &path, struct draw_ctx *ctx, std::unordered_map<unsigned long long, std::string> *old_names)
{
    if (!opath || !path.size() || !ctx)
	return;
    bu_vls_trunc(opath, 0);
    for (size_t i = 0; i < path.size(); i++) {
	// First, see if the hash is an instance string
	if (ctx->i_str.find(path[i]) != ctx->i_str.end()) {
	    bu_vls_printf(opath, "%s", ctx->i_str[path[i]].c_str());
	    if (i < path.size() - 1)
		bu_vls_printf(opath, "/");
	    continue;
	}
	// If we have potentially obsolete names, check those
	// before trying the dp (which may no longer be invalid)
	if (old_names && old_names->find(path[i]) != old_names->end()) {
	    bu_vls_printf(opath, "%s", (*old_names)[path[i]].c_str());
	    if (i < path.size() - 1)
		bu_vls_printf(opath, "/");
	    continue;
	}
	// If not, try the directory pointer
	if (ctx->d_map.find(path[i]) != ctx->d_map.end()) {
	    bu_vls_printf(opath, "%s", ctx->d_map[path[i]]->d_namep);
	    if (i < path.size() - 1)
		bu_vls_printf(opath, "/");
	    continue;
	}
	// Last option - invalid string
	if (ctx->invalid_entry_map.find(path[i]) != ctx->invalid_entry_map.end()) {
	    bu_vls_printf(opath, "%s", ctx->invalid_entry_map[path[i]].c_str());
	    if (i < path.size() - 1)
		bu_vls_printf(opath, "/");
	    continue;
	}
	bu_vls_printf(opath, "ERROR!!!");
    }
}


void
print_ctx_unordered(struct draw_ctx *ctx)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_set<unsigned long long>::iterator cs_it;
    for (pc_it = ctx->p_c.begin(); pc_it != ctx->p_c.end(); pc_it++) {
	bool found_entry = false;
	std::unordered_map<unsigned long long, struct directory *>::iterator dpn = ctx->d_map.find(pc_it->first);
	if (dpn != ctx->d_map.end()) {
	    bu_log("%s	(%llu):\n", dpn->second->d_namep, pc_it->first);
	    found_entry = true;
	}
	if (!found_entry) {
	    unsigned long long chash = ctx->i_map[pc_it->first];
	    dpn = ctx->d_map.find(chash);
	    if (dpn != ctx->d_map.end()) {
		bu_log("%s	(%llu->%llu):\n", dpn->second->d_namep, pc_it->first, chash);
		found_entry = true;
	    }
	}
	if (!found_entry) {
	    std::unordered_map<unsigned long long, std::string>::iterator en = ctx->invalid_entry_map.find(pc_it->first);
	    if (en != ctx->invalid_entry_map.end()) {
		bu_log("%s[I]	(%llu)\n", en->second.c_str(), pc_it->first);
		found_entry = true;
	    } else {
		bu_log("P ERROR: %llu\n", pc_it->first);
	    }
	}
	if (!found_entry)
	    continue;

	for (cs_it = pc_it->second.begin(); cs_it != pc_it->second.end(); cs_it++) {
	    found_entry = false;
	    dpn = ctx->d_map.find(*cs_it);
	    if (dpn != ctx->d_map.end()) {
		bu_log("	%s	(%llu)\n", dpn->second->d_namep, *cs_it);
		found_entry = true;
	    }
	    if (!found_entry) {
		unsigned long long chash = ctx->i_map[*cs_it];
		dpn = ctx->d_map.find(chash);
		if (dpn != ctx->d_map.end()) {
		    bu_log("	%s	(%llu->%llu)\n", dpn->second->d_namep, *cs_it, chash);
		    found_entry = true;
		}
	    }
	    if (!found_entry) {
		std::unordered_map<unsigned long long, std::string>::iterator en = ctx->invalid_entry_map.find(*cs_it);
		if (en != ctx->invalid_entry_map.end()) {
		    bu_log("	%s[I] (%llu)\n", en->second.c_str(), *cs_it);
		    found_entry = true;
		} else {
		    bu_log("P ERROR: %llu:\n", pc_it->first);
		}
	    }
	}
    }
    bu_log("\n");
}

void
print_ctx_ordered(struct draw_ctx *ctx)
{
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
    std::vector<unsigned long long>::reverse_iterator cs_it;
    for (pv_it = ctx->p_v.begin(); pv_it != ctx->p_v.end(); pv_it++) {
	bool found_entry = false;
	std::unordered_map<unsigned long long, struct directory *>::iterator dpn = ctx->d_map.find(pv_it->first);
	if (dpn != ctx->d_map.end()) {
	    bu_log("%s	(%llu):\n", dpn->second->d_namep, pv_it->first);
	    found_entry = true;
	}
	if (!found_entry) {
	    unsigned long long chash = ctx->i_map[pv_it->first];
	    dpn = ctx->d_map.find(chash);
	    if (dpn != ctx->d_map.end()) {
		bu_log("%s	(%llu->%llu):\n", dpn->second->d_namep, pv_it->first, chash);
		found_entry = true;
	    }
	}
	if (!found_entry) {
	    std::unordered_map<unsigned long long, std::string>::iterator en = ctx->invalid_entry_map.find(pv_it->first);
	    if (en != ctx->invalid_entry_map.end()) {
		bu_log("%s[I]	(%llu)\n", en->second.c_str(), pv_it->first);
		found_entry = true;
	    } else {
		bu_log("P ERROR: %llu\n", pv_it->first);
	    }
	}
	if (!found_entry)
	    continue;

	for (cs_it = pv_it->second.rbegin(); cs_it != pv_it->second.rend(); cs_it++) {
	    found_entry = false;
	    dpn = ctx->d_map.find(*cs_it);
	    if (dpn != ctx->d_map.end()) {
		bu_log("	%s	(%llu)\n", dpn->second->d_namep, *cs_it);
		found_entry = true;
	    }
	    if (!found_entry) {
		unsigned long long chash = ctx->i_map[*cs_it];
		dpn = ctx->d_map.find(chash);
		if (dpn != ctx->d_map.end()) {
		    bu_log("	%s	(%llu->%llu)\n", dpn->second->d_namep, *cs_it, chash);
		    found_entry = true;
		}
	    }
	    if (!found_entry) {
		std::unordered_map<unsigned long long, std::string>::iterator en = ctx->invalid_entry_map.find(*cs_it);
		if (en != ctx->invalid_entry_map.end()) {
		    bu_log("	%s[I] (%llu)\n", en->second.c_str(), *cs_it);
		    found_entry = true;
		} else {
		    bu_log("P ERROR: %llu:\n", pv_it->first);
		}
	    }
	}
    }
    bu_log("\n");
}

unsigned long long
path_hash(std::vector<unsigned long long> &path, size_t max_len)
{
    size_t mlen = (max_len) ? max_len : path.size();
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, path.data(), mlen * sizeof(unsigned long long));
    return (unsigned long long)XXH64_digest(&h_state);
}



static void
draw_walk_tree(union tree *tp, void *d, int subtract_skip,
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
	    draw_walk_tree(tp->tr_b.tb_right, d, subtract_skip, leaf_func);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    draw_walk_tree(tp->tr_b.tb_left, d, subtract_skip, leaf_func);
	    break;
	case OP_DB_LEAF:
	    (*leaf_func)(d, tp->tr_l.tl_name, tp->tr_l.tl_mat, tp->tr_op);
	    break;
	default:
	    bu_log("unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("draw walk\n");
    }
}

struct lc_data {
    std::unordered_set<unsigned long long> chashes;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
};


static void
get_child_hashes_leaf(void *client_data, const char *name, matp_t UNUSED(c_m), int UNUSED(op))
{
    struct lc_data *d = (struct lc_data *)client_data;
    std::unordered_map<unsigned long long, unsigned long long> &i_count = d->i_count;
    XXH64_state_t h_state;
    unsigned long long chash;
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
	bu_vls_free(&iname);
	unsigned long long ihash = (unsigned long long)XXH64_digest(&h_state);
	d->chashes.insert(ihash);
    } else {
	d->chashes.insert(chash);
    }
}

struct walk_data {
    struct draw_ctx *ctx;
    struct ged *gedp;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    mat_t *curr_mat = NULL;
    unsigned long long phash = 0;
};

static void
populate_leaf(void *client_data, const char *name, matp_t c_m, int op)
{
    struct walk_data *d = (struct walk_data *)client_data;
    struct db_i *dbip = d->gedp->dbip;
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
	d->ctx->i_map[ihash] = chash;
	d->ctx->i_str[ihash] = std::string(bu_vls_cstr(&iname));
	d->ctx->p_c[d->phash].insert(ihash);
	d->ctx->p_v[d->phash].push_back(ihash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->ctx->invalid_entry_map[ihash] = std::string(bu_vls_cstr(&iname));
	} else {
	    // In case this was previously invalid, remove
	    d->ctx->invalid_entry_map.erase(ihash);
	}
	bu_vls_free(&iname);

	// For the next stages, if we have an ihash use it
	chash = ihash;

    } else {

	d->ctx->p_v[d->phash].push_back(chash);
	d->ctx->p_c[d->phash].insert(chash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->ctx->invalid_entry_map[chash] = std::string(name);
	} else {
	    // In case this was previously invalid, remove
	    d->ctx->invalid_entry_map.erase(chash);
	}

    }

    // If we have a non-IDN matrix, store it
    if (c_m) {
	for (int i = 0; i < 16; i++)
	    d->ctx->matrices[d->phash][chash].push_back(c_m[i]);
    }

    // If we have a non-UNION op, store it
    d->ctx->i_bool[d->phash][chash] = op;
}

static void
populate_maps(struct ged *gedp, struct draw_ctx *ctx, struct directory *dp, unsigned long long phash, int reset)
{
    if (!(dp->d_flags & RT_DIR_COMB))
	return;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
    pc_it = ctx->p_c.find(phash);
    pv_it = ctx->p_v.find(phash);
    if (pc_it == ctx->p_c.end() || pv_it != ctx->p_v.end() || reset) {
	if (reset && pc_it != ctx->p_c.end()) {
	    pc_it->second.clear();
	}
	if (reset && pv_it != ctx->p_v.end()) {
	    pv_it->second.clear();
	}
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree)
	    return;

	std::unordered_map<unsigned long long, unsigned long long> i_count;
	struct walk_data d;
	d.ctx = ctx;
	d.gedp = gedp;
	d.phash = phash;
	draw_walk_tree(comb->tree, (void *)&d, 0, populate_leaf);
	rt_db_free_internal(&in);
    }
}

static bool
validate_entry(struct ged *gedp, struct draw_ctx *ctx, const char *name)
{
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, name, strlen(name)*sizeof(char));
    unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);

    // A supplied name can be a number of things.  First, check if it is an
    // instance.  If it is, the thing to check is whether the comb map
    // has a key that matches the i_map value.  (From a higher level .g
    // perspective, we must first validate parent combs before validating
    // children, to make sure p_c is current before we check instances.)
    if (ctx->i_map.find(hash) != ctx->i_map.end()) {
	unsigned long long mhash = ctx->i_map[hash];
	if (ctx->p_c.find(mhash) == ctx->p_c.end())
	    return false;
	return true;
    }

    // For the rest, we need to know the status in the .g file
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);

    // Name isn't an i_map entry.  See if it refers to an invalid map entry.
    // If so, we need to make sure it is still invalid.  NOTE:  We don't verify
    // here that this entry is used in at least one comb...  that operation -
    // clearing stale invalid entries - needs to be part of a higher level,
    // whole .g check.
    if (ctx->invalid_entry_map.find(hash) != ctx->invalid_entry_map.end()) {
	if (dp != RT_DIR_NULL)
	    return false;
	return true;
    }

    // If it's not an invalid entry, make sure d_map is current
    std::unordered_map<unsigned long long, struct directory *>::iterator d_it = ctx->d_map.find(hash);
    if (d_it == ctx->d_map.end() && !dp)
	return false;
    if (d_it != ctx->d_map.end() && dp != d_it->second)
	return false;

    // If we got this far and we don't have a dp, whatever we have isn't valid
    if (!dp)
	return false;

    // If we have a comb, we need to check that the children are current
    if (dp->d_flags & RT_DIR_COMB) {

	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
	pc_it = ctx->p_c.find(hash);

	// Have a child hash set - need to validate it against the on-disk version
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0)
	    return false;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree) {
	    if (pc_it == ctx->p_c.end() || !pc_it->second.size())
		return true;
	    return false;
	}

	// Have a tree - get the hashes
	struct lc_data ldata;
	draw_walk_tree(comb->tree, (void *)&ldata, 0, get_child_hashes_leaf);

	// Check that every entry in the current pc_it is also on disk
	std::unordered_set<unsigned long long>::iterator c_it;
	for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++) {
	    if (ldata.chashes.find(*c_it) == ldata.chashes.end()) {
		return false;
	    }
	    ldata.chashes.erase(*c_it);
	}
	if (ldata.chashes.size() > 0) {
	    // If we have entries in the on-disk set that aren't in the working
	    // set, we're also invalid
	    return false;
	}
    }

    // All checks passed
    return true;
}

static void
fp_path_split(std::vector<std::string> &objs, const char *str)
{
    std::string s(str);
    while (s.length() && s.c_str()[0] == '/')
	s.erase(0, 1);  //Remove leading slashes

    std::string nstr;
    bool escaped = false;
    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if (escaped) {
		nstr.push_back(s[i]);
		escaped = false;
		continue;
	    }
	    escaped = true;
	    continue;
	}
	if (s[i] == '/' && !escaped) {
	    if (nstr.length())
		objs.push_back(nstr);
	    nstr.clear();
	    continue;
	}
	nstr.push_back(s[i]);
	escaped = false;
    }
    if (nstr.length())
	objs.push_back(nstr);
}

static std::string
name_deescape(std::string &name)
{
    std::string s(name);
    std::string nstr;

    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if ((i+1) < s.length())
		nstr.push_back(s[i+1]);
	    i++;
	} else {
	    nstr.push_back(s[i]);
	}
    }

    return nstr;
}

#if 0
static void
name_deinstance(struct bu_vls *nstr, const char *i_name)
{
    if (!nstr || !i_name)
	return;

    for (size_t i = 0; i < strlen(i_name); i++) {
	if (i_name[i] == '@')
	    return;
	bu_vls_putc(nstr, i_name[i]);
    }
}
#endif


// This is a full (and more expensive) check to ensure
// a path has no cycles anywhere in it.
static bool
path_cyclic(std::vector<unsigned long long> &path)
{
    if (path.size() == 1)
	return false;
    int i = path.size() - 1;
    while (i > 0) {
	int j = i - 1;
	while (j >= 0) {
	    if (path[i] == path[j])
		return true;
	    j--;
	}
	i--;
    }
    return false;
}

// This version assumes the path entries other than the last
// one are OK, and checks only against that last entry.
static bool
path_addition_cyclic(std::vector<unsigned long long> &path)
{
    if (path.size() == 1)
	return false;
    int new_entry = path.size() - 1;
    int i = new_entry - 1;
    while (i >= 0) {
	if (path[new_entry] == path[i])
	    return true;
	i--;
    }
    return false;
}

static size_t
path_elements(std::vector<std::string> &elements, const char *path)
{
    std::vector<std::string> substrs;
    fp_path_split(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	std::string cleared = name_deescape(substrs[i]);
	elements.push_back(cleared);
    }
    return elements.size();
}

struct dd_t {
    struct ged *gedp;
    struct draw_ctx *ctx;
    matp_t m;
    int subtract_skip;
    std::vector<unsigned long long> path_hashes;
    std::unordered_map<unsigned long long, unsigned long long> *i_count;
};

void
draw_gather_paths(void *d, const char *name, matp_t m, int UNUSED(op))
{
    struct dd_t *dd = (struct dd_t *)d;
    struct directory *dp = db_lookup(dd->gedp->dbip, name, LOOKUP_QUIET);
    if (!dp)
	return;

    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, name, strlen(name)*sizeof(char));
    unsigned long long chash = (unsigned long long)XXH64_digest(&h_state);
    (*dd->i_count)[chash] += 1;
    if ((*dd->i_count)[chash] > 1) {
	// If we've got multiple instances of the same object in the tree,
	// hash the string labeling the instance and map it to the correct
	// parent comb so we can associate it with the tree contents
	struct bu_vls iname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&iname, "%s@%llu", name, (*dd->i_count)[chash]);
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, bu_vls_cstr(&iname), bu_vls_strlen(&iname)*sizeof(char));
	unsigned long long ihash = (unsigned long long)XXH64_digest(&h_state);
	dd->path_hashes.push_back(ihash);
    } else {
	dd->path_hashes.push_back(chash);
    }
#if 0
    struct bu_vls path_str = BU_VLS_INIT_ZERO;
    print_path(&path_str, dd->path_hashes, dd->ctx);
    bu_log("%s\n", bu_vls_cstr(&path_str));
    bu_vls_free(&path_str);
#endif
    mat_t om, nm;
    /* Update current matrix state to reflect the new branch of
     * the tree. Either we have a local matrix, or we have an
     * implicit IDN matrix. */
    MAT_COPY(om, dd->m);
    if (m) {
	MAT_COPY(nm, m);
    } else {
	MAT_IDN(nm);
    }
    bn_mat_mul(dd->m, om, nm);

    if (dp->d_flags & RT_DIR_COMB) {
	// Two things may prevent further processing of a comb - a hidden dp, or
	// a cyclic path.
	if (!(dp->d_flags & RT_DIR_HIDDEN) && !path_addition_cyclic(dd->path_hashes)) {
	    /* Keep going */
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;

	    if (rt_db_get_internal(&in, dp, dd->gedp->dbip, NULL, &rt_uniresource) < 0)
		return;

	    comb = (struct rt_comb_internal *)in.idb_ptr;
	    std::unordered_map<unsigned long long, unsigned long long> i_count;
	    std::unordered_map<unsigned long long, unsigned long long> *tmp = dd->i_count;
	    dd->i_count = &i_count;
	    draw_walk_tree(comb->tree, d, dd->subtract_skip, draw_gather_paths);
	    dd->i_count = tmp;
	    rt_db_free_internal(&in);
	}
    } else {
	// Solid - scene object time
	//bu_log("make solid\n");
	unsigned long long phash = path_hash(dd->path_hashes, 0);
	dd->ctx->s_keys[phash] = dd->path_hashes;
    }

    /* Done with branch - restore path, put back the old matrix state,
     * and restore previous color settings */
    dd->path_hashes.pop_back();
    MAT_COPY(dd->m, om);
}

static void
draw_path(struct ged *gedp, struct draw_ctx *ctx, const char *path)
{
    mat_t m;
    MAT_IDN(m);
    int op = OP_UNION;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    std::vector<std::string> pe;
    // TODO: path color
    // TODO: path matrix
    // TODO: op check (specific instances specified may contain subtractions...)
    struct dd_t d;
    d.gedp = gedp;
    d.ctx = ctx;
    d.m = m;
    d.subtract_skip = 0;
    d.i_count = &i_count;
    path_elements(pe, path);

    std::vector<unsigned long long> elements;
    XXH64_state_t h_state;
    for (size_t i = 0; i < pe.size(); i++) {
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, pe[i].c_str(), pe[i].length()*sizeof(char));
	elements.push_back((unsigned long long)XXH64_digest(&h_state));
    }

    if (path_cyclic(elements)) {
	bu_log("Error - %s is a cyclic path\n", path);
	return;
    }

    // If we have a non-toplevel to draw, we need to "seed" path hashes
    // with all but the last hash from elements so the correct paths
    // are recorded
    d.path_hashes = elements;
    d.path_hashes.pop_back();

    point_t bbmin, bbmax;
    VSETALL(bbmin, INFINITY);
    VSETALL(bbmax, -INFINITY);
    get_path_bbox(&bbmin, &bbmax, ctx, elements);
    bu_log("bbmin: %f %f %f  bbmax: %f %f %f\n", V3ARGS(bbmin), V3ARGS(bbmax));

    draw_gather_paths((void *)&d, pe[pe.size()-1].c_str(), m, op);
}


void
erase_path(struct draw_ctx *ctx, const char *path)
{
    std::vector<std::string> pe;
    path_elements(pe, path);
    std::vector<unsigned long long> root;
    for (size_t i = 0; i < pe.size(); i++) {
	XXH64_state_t h_state;
	unsigned long long chash;
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, pe[i].c_str(), pe[i].length()*sizeof(char));
	chash = (unsigned long long)XXH64_digest(&h_state);
	root.push_back(chash);
    }

    std::vector<unsigned long long> bad_paths;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    for (k_it = ctx->s_keys.begin(); k_it != ctx->s_keys.end(); k_it++) {
	if (k_it->second.size() < root.size())
	    continue;
	bool match = std::equal(root.begin(), root.end(), k_it->second.begin());
	if (match) {
	    bad_paths.push_back(k_it->first);
	}
    }
    bu_log("Starting size: %zd\n", ctx->s_keys.size());
    bu_log("To erase: %zd\n", bad_paths.size());
    for (size_t i = 0; i < bad_paths.size(); i++) {
	ctx->s_keys.erase(bad_paths[i]);
    }
    bu_log("Ending size: %zd\n", ctx->s_keys.size());
}

static void
collapse(
	std::vector<std::vector<unsigned long long>> &collapsed,
	std::unordered_set<unsigned long long> *all_fully_drawn,
	std::vector<unsigned long long> &active_paths,
	std::unordered_map<unsigned long long, std::vector<unsigned long long>> input_map,
	struct draw_ctx *ctx)
{
    std::unordered_set<unsigned long long>::iterator s_it;
    std::map<size_t, std::unordered_set<unsigned long long>> depth_groups;

    // Reset containers
    collapsed.clear();
    if (all_fully_drawn)
	all_fully_drawn->clear();

    // Group paths of the same depth.  Depth == 1 paths are already
    // top level objects and need no further processing.  If active_paths
    // is empty, the active set is all paths in input_map
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    if (active_paths.size()) {
	for (size_t i = 0; i != active_paths.size(); i++) {
	    k_it = input_map.find(active_paths[i]);
	    if (k_it == input_map.end())
		continue;
	    if (k_it->second.size() == 1) {
		collapsed.push_back(k_it->second);
		if (all_fully_drawn)
		    all_fully_drawn->insert(k_it->first);
	    } else {
		depth_groups[k_it->second.size()].insert(k_it->first);
	    }
	}
    } else {
	for (k_it = input_map.begin(); k_it != input_map.end(); k_it++) {
	    if (k_it->second.size() == 1) {
		collapsed.push_back(k_it->second);
		if (all_fully_drawn)
		    all_fully_drawn->insert(k_it->first);
	    } else {
		depth_groups[k_it->second.size()].insert(k_it->first);
	    }
	}
    }
    bu_log("depth groups: %zd\n", depth_groups.size());


    // Whittle down the depth groups until we find not-fully-drawn parents - when
    // we find that, the children constitute non-collapsible paths
    while (depth_groups.size()) {
	size_t plen = depth_groups.rbegin()->first;
	if (plen == 1)
	    break;
	std::unordered_set<unsigned long long> &pckeys = depth_groups.rbegin()->second;
	bu_log("depth_groups(%zd) key count: %zd\n", plen, pckeys.size());

	// For a given depth, group the paths by parent comb.  This results
	// in path sub-groups which will define for us how "fully drawn"
	// that particular parent comb is.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> grouped_pckeys;
	for (s_it = pckeys.begin(); s_it != pckeys.end(); s_it++) {
	    std::vector<unsigned long long> &pc_path = input_map[*s_it];
	    grouped_pckeys[pc_path[plen-2]].insert(*s_it);
	}

	// For each parent/child grouping, compare it against the .g ground
	// truth set.  If they match, fully drawn and we promote the path to
	// the parent depth.  If not, the paths do not collapse further and are
	// added to drawn paths.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pg_it;
	for (pg_it = grouped_pckeys.begin(); pg_it != grouped_pckeys.end(); pg_it++) {

	    // As above, use the full path from the input_map, but this time
	    // we're collecting the children.  This is the set we need to compare
	    // against the .g ground truth to determine fully or partially drawn.
	    std::unordered_set<unsigned long long> g_children;
	    std::unordered_set<unsigned long long> &g_pckeys = pg_it->second;
	    for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		std::vector<unsigned long long> &pc_path = input_map[*s_it];
		g_children.insert(pc_path[plen-1]);
	    }

	    // Do the check against the .g comb children info - the "ground truth"
	    // that defines what must be present for a fully drawn comb
	    bool is_fully_drawn = true;
	    std::unordered_set<unsigned long long> &ground_truth = ctx->p_c[pg_it->first];
	    for (s_it = ground_truth.begin(); s_it != ground_truth.end(); s_it++) {
		if (g_children.find(*s_it) == g_children.end()) {
		    is_fully_drawn = false;
		    break;
		}
	    }

	    if (is_fully_drawn) {
		// If fully drawn, depth_groups[plen-1] gets the first path in
		// g_pckeys.  The path is longer than that depth, but contains
		// all the necessary information and using that approach avoids
		// the need to duplicate paths.
		depth_groups[plen - 1].insert(*g_pckeys.begin());
		if (all_fully_drawn) {
		    for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
			std::vector<unsigned long long> &pc_path = input_map[*s_it];
			// Hash only to the current path length (we may have deeper
			// promoted paths being used at this level)
			all_fully_drawn->insert(path_hash(pc_path, plen));
		    }
		}
	    } else {
		// No further collapsing - add to final.  We must make trimmed
		// versions of the paths in case this depth holds promoted
		// paths from deeper levels, since we are duplicating the full
		// path contents.
		for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		    std::vector<unsigned long long> trimmed = input_map[*s_it];
		    trimmed.resize(plen);
		    collapsed.push_back(trimmed);
		    if (all_fully_drawn) {
			// Hash is calculated on that trimmed path, so this time we
			// don't need to specify a depth limit.
			all_fully_drawn->insert(path_hash(trimmed, 0));
		    }
		}
	    }
	}

	// Done with this depth
	depth_groups.erase(plen);
    }

    // If we collapsed all the way to top level objects, make sure to add them
    // if they are still valid entries.  If a toplevel entry is invalid, there
    // is no parent comb to refer to it as an "invalid" object and it can no
    // longer be drawn.
    if (depth_groups.find(1) != depth_groups.end()) {
	std::unordered_set<unsigned long long> &pckeys = depth_groups.rbegin()->second;
	for (s_it = pckeys.begin(); s_it != pckeys.end(); s_it++) {
	    std::vector<unsigned long long> trimmed = input_map[*s_it];
	    trimmed.resize(1);
	    if (ctx->d_map.find(trimmed[0]) == ctx->d_map.end())
		continue;
	    collapsed.push_back(trimmed);
	}
    }
}

void
split_test(const char *path)
{
    std::vector<std::string> substrs;
    path_elements(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	bu_log("%s\n", substrs[i].c_str());
    }
    bu_log("\n");
}


bool
check_elements(struct ged *gedp, struct draw_ctx *ctx, std::vector<std::string> &elements)
{
    if (!gedp || !elements.size())
	return false;

    // If we have only a single entry, make sure it's a valid db entry and return
    if (elements.size() == 1) {
	struct directory *dp = db_lookup(gedp->dbip, elements[0].c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    return false;
	XXH64_state_t h_state;
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);
	populate_maps(gedp, ctx, dp, hash, 1);
	return true;
    }

    // Deeper paths take more work.  NOTE - if the last element isn't a valid
    // database entry, we have an invalid comb entry specified.  We are more
    // tolerant of such paths and let them be "drawn" to allow views to add
    // wireframes to scenes when objects are renamed to make an existing
    // invalid instance "good".  However, any comb entries above the final
    // entry *must* be valid objects in the database.
    for (size_t i = 0; i < elements.size(); i++) {
	struct directory *dp = db_lookup(gedp->dbip, elements[i].c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL && i < elements.size() - 1) {
	    bu_log("invalid path: %s\n", elements[i].c_str());
	    return false;
	}
	XXH64_state_t h_state;
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);
	populate_maps(gedp, ctx, dp, hash, 1);
    }


    // Check that elements were written correctly
    for (size_t i = 0; i < elements.size(); i++) {
	bool status = validate_entry(gedp, ctx, elements[i].c_str());
	bu_log("%s: %s\n", elements[i].c_str(), (status) ? "true" : "false");
    }

    // parent/child relationship validate
    struct bu_vls hname = BU_VLS_INIT_ZERO;
    XXH64_state_t h_state;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    unsigned long long phash = 0;
    unsigned long long chash = 0;
    XXH64_reset(&h_state, 0);
    bu_vls_sprintf(&hname, "%s", elements[0].c_str());
    bu_log("parent: %s\n", elements[0].c_str());
    XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
    phash = (unsigned long long)XXH64_digest(&h_state);
    for (size_t i = 1; i < elements.size(); i++) {
	pc_it = ctx->p_c.find(phash);
	// The parent comb structure is stored only under its original name's hash - if
	// we have a numbered instance from a comb tree as a parent, we may be able to
	// map it to the correct entry with i_map.  If not, we have an invalid path.
	if (pc_it == ctx->p_c.end()) {
	    std::unordered_map<unsigned long long, unsigned long long>::iterator i_it = ctx->i_map.find(phash);
	    if (i_it == ctx->i_map.end()) {
		return false;
	    }
	    phash = i_it->second;
	}
	XXH64_reset(&h_state, 0);
	bu_vls_sprintf(&hname, "%s", elements[i].c_str());
	XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
	chash = (unsigned long long)XXH64_digest(&h_state);
	bu_log("child: %s\n\n", elements[i].c_str());

	if (pc_it->second.find(chash) == pc_it->second.end()) {
	    bu_log("Invalid element path: %s\n", elements[i].c_str());
	    return false;
	}
	phash = chash;
	bu_log("parent: %s\n", elements[i].c_str());
    }

    return true;
}

// The most involved task we must perform with these containers, in some ways,
// is to update them in response to .g database changes...  need to figure out
// if and how that will be accomplished.
//
// NOTE:  If this scheme ultimately proves viable, draw_ctx also has the
// potential to replace the gInstance container, since all the information
// stored in each instance is implicit in the containers
//
// Order of operations needs some careful consideration here, since the
// validity or invalidity of some stages may depend on others.  Worst case
// we need to fully process for each removed and changed pointer...
void
ctx_update(struct draw_ctx *ctx, struct g_ctx *g)
{
    std::unordered_set<unsigned long long>::iterator s_it;
    if (!ctx)
	return;

    // Collect all the paths with either removed or changed dps
    std::vector<unsigned long long> active_paths;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    for (k_it = ctx->s_keys.begin(); k_it != ctx->s_keys.end(); k_it++) {
	for (size_t i = 0; i < k_it->second.size(); i++) {
	    unsigned long long hash = k_it->second[i];
	    // If this is an instance, we need to map it to its canonical
	    // comb hash for the check
	    if (ctx->i_map.find(hash) != ctx->i_map.end()) { 
		hash = ctx->i_map[hash];
	    }
	    if (g->removed.find(hash) != g->removed.end()) {
		active_paths.push_back(k_it->first);
		break;
	    }
	    if (g->changed.find(hash) != g->changed.end()) {
		active_paths.push_back(k_it->first);
		break;
	    }
	}
    }
    bu_log("active_paths: %zd\n", active_paths.size());

    // Collapse until the leaf is a changed dp or not fully drawn.  We must do
    // this before the comb p_c relationships are updated in the context, since
    // we want the answers for this collapse to be from the prior state, not
    // the current state.
    std::vector<std::vector<unsigned long long>> collapsed;
    collapse(collapsed, NULL, active_paths, ctx->s_keys, ctx);
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed altered paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], ctx, &g->old_names);
	    bu_log("\t%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }

    // Update the primary data structures 
    for(s_it = g->removed.begin(); s_it != g->removed.end(); s_it++) {
	bu_log("removed: %llu\n", *s_it);

	// Combs with this key in their child set need to be updated to refer
	// to it as an invalid entry.
	std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
	for (pv_it = ctx->p_v.begin(); pv_it != ctx->p_v.end(); pv_it++) {
	    for (size_t i = 0; i < pv_it->second.size(); i++) {
		if (ctx->i_map.find(pv_it->second[i]) != ctx->i_map.end()) {
		    ctx->invalid_entry_map[ctx->i_map[pv_it->second[i]]] = ctx->i_str[ctx->i_map[pv_it->second[i]]];
		} else {
		    ctx->invalid_entry_map[pv_it->second[i]] = g->old_names[*s_it];
		}
	    }
	}

	ctx->d_map.erase(*s_it);
	ctx->bboxes.erase(*s_it);
	ctx->c_inherit.erase(*s_it);
	ctx->rgb.erase(*s_it);
	ctx->region_id.erase(*s_it);
	ctx->matrices.erase(*s_it);
	ctx->i_bool.erase(*s_it);

	// We do not clear the instance maps (i_map and i_str) since those containers do not
	// guarantee uniqueness to one child object.  If we do want to remove entries no longer
	// used anywhere in the database, we'll have to confirm their invalidity on a global
	// basis in a garbage-collect operation.  (May or may not be necessary...)

	// Entries with this hash as their key are erased.  
	ctx->p_c.erase(*s_it);
	ctx->p_v.erase(*s_it);
    }

    for(s_it = g->added.begin(); s_it != g->added.end(); s_it++) {
	bu_log("added: %llu\n", *s_it);
	// package up dbi_Head procedure used in main to initialize, apply to
	// these dps
	//
	// need to check if invalid instances are made valid by the addition of
	// these dps
	//
	// NOTE:  if paths ending in invalid dps are present in the drawn set
	// that are now valid, we need to expand them after the initial dp
	// processing is complete and replace them with the expansions
    }
 
    for(s_it = g->changed.begin(); s_it != g->changed.end(); s_it++) {
	bu_log("changed: %llu\n", *s_it);
	// Properties need to be updated - comb children, colors, matrices,
	// etc.  Paths with changed items in their array need to be collected,
	// collapsed to the minimum fully drawn paths, and considered.  If
	// the changed pointer is the leaf then it can simply be re-expanded
	// as it was fully drawn before, but if it is a parent then we need
	// to check and see if the children below the parent are still valid.
	// If not, those paths are removed from the drawn set
    }

#if 0
    // The principle for redrawing will be that anything that was previously
    // fully drawn should stay fully drawn, even if its tree structure has
    // changed.  We need to remove no-longer-valid paths, but will keep valid
    // paths to avoid the work of re-generating the scene objects when we
    // re-expand the collapsed paths using the new tree structure.
    //

    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator sk_it;
    std::unordered_set<unsigned long long> invalid_objects;
    for (sk_it = ctx->s_keys.begin(); sk_it != ctx->s_keys.end(); sk_it++) {
	// Work down from the root of each path looking for the first changed or
	// removed entry.
	std::vector<unsigned long long> &cpath = sk_it->second;
	bool parent_changed = false;
	for (size_t j = 0; j < cpath.size(); j++) {
	    if (parent_changed) {
		// TODO - need to see if this is still a parent of the new
		// comb.  If not, we're done, whether or not the parent dp was
		// removed from the database.  If it's still in the comb tree,
		// proceed with the evaluation.  This step is why the draw
		// update has to come AFTER the above primitive update
		// passes
	    }

	    bool is_removed = (g->removed.find(hash) != g->removed.end());
	    bool is_changed = (g->changed.find(hash) != g->changed.end());
	    if (is_removed) {
		if (is_removed && !j) {
		    // Top level removed - everything else is gone
		    break;
		}

		if (is_removed && j != cpath.size()-1) {
		    // If removed is first and not a leaf, erase - if we got here
		    // the parent comb either wasn't changed at all or this
		    // particular instance is still there; either way the state
		    // here is not preservable, since the path is trying to refer
		    // to a tree path which no longer exists in the hierarchy.
		    break;
		}
		if (is_removed && j == cpath.size()-1) {
		    // If removed is a leaf and the comb instance is intact,
		    // "draw" as invalid path.  In this case there is no subtree
		    // state we are trying to preserve, and the referenced instance
		    // path DOES still exist in the .g file, even though it is
		    // (at the moment) invalid.
		    active_collapsed.insert(i);
		    break;
		}
	    }
	    if (is_changed) {
		if (j == cpath.size()-1) {
		    // Changed, but a leaf - stays drawn
		    active_collapsed.insert(i);
		    break;
		}
		// Not a leaf - check child
		parent_changed = true;
		continue;
	    }

	    // If we got here, reset the parent changed flag
	    parent_changed = false;

	    // If we got all the way through the path, it's a keeper
	    if (j == cpath.size()-1)
		active_collapsed.insert(i);
	}
    }

    std::unordered_set<unsigned long long>::iterator iv_it;
    for (iv_it = invalid_objects.begin(); iv_it != invalid_objects.end(); iv_it++) {
	ctx->s_keys.erase(*iv_it);
	if (ctx->s_map.find(*iv_it) != ctx->s_map.end()) {
	    // an invalid child entry may be present in s_keys but not have an
	    // associated scene object.
	// free scene obj ctx->s_map[*iv_it];
	}
    }

    // Expand collapsed paths, creating any missing path objects

#endif

    // Added dps present their own challenge, in terms of whether or not to
    // automatically draw them.  (I think this decision comes after the existing
    // draw paths' removed/changed processing and the main .g reflecting maps are
    // updated.)  The cases:
    //
    // 1.  Already part of a drawn invalid path - draw and expand, as path was
    // drawn but is no longer invalid
    //
    // 2.  Already part of a non-drawn invalid path - do not draw (? - could see
    // a case for either behavior here, if the user wants to see the instances
    // of the newly enabled part... this may have to be a user option)
    //
    // 2.  Not part of any path, pre or post removed/changed draw states (i.e.
    // a tops object) - draw
  
    // May want to "garbage collect" invalid entires and instances sets...
}



int
main(int argc, char *argv[]) {
    struct ged *gedp;

    bu_setprogname(argv[0]);

    if (argc != 2) {
	printf("Usage: %s file.g\n", argv[0]);
	return 1;
    }
    if (!bu_file_exists(argv[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", argv[1]);
	return 2;
    }

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    gedp = ged_open("db", argv[1], 1);

    struct g_ctx g;

    db_add_changed_clbk(gedp->dbip, &dp_changed_clbk, (void *)&g);

    struct draw_ctx ctx;
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & DB_LS_HIDDEN)
		continue;

	    // Set up to go from hash back to name
	    XXH64_state_t h_state;
	    XXH64_reset(&h_state, 0);
	    XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	    unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);
	    ctx.d_map[hash] = dp;

	    // If this isn't a comb, find and record the untransformed
	    // bounding box
	    if (!(dp->d_flags & RT_DIR_COMB)) {
		point_t bmin, bmax;
		mat_t m;
		MAT_IDN(m);
		int bret = rt_bound_instance(&bmin, &bmax, dp, gedp->dbip,
			&gedp->ged_wdbp->wdb_ttol, &gedp->ged_wdbp->wdb_tol,
			&m, &rt_uniresource, gedp->ged_gvp);
		if (bret != -1) {
		    for (size_t j = 0; j < 3; j++)
			ctx.bboxes[hash].push_back(bmin[j]);
		    for (size_t j = 0; j < 3; j++)
			ctx.bboxes[hash].push_back(bmax[j]);
		}
	    }

	    // Encode hierarchy info if this is a comb
	    if (dp->d_flags & RT_DIR_COMB)
		populate_maps(gedp, &ctx, dp, hash, 0);

	    // Check for various drawing related attributes
	    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
	    db5_get_attributes(gedp->dbip, &c_avs, dp);

	    // Check for region id.  For drawing purposes this needs to be a number.
	    const char *region_id_val = bu_avs_get(&c_avs, "region_id");
	    int region_id = -1;
	    if (region_id_val) {
		if (bu_opt_int(NULL, 1, &region_id_val, (void *)&region_id) != -1) {
		    ctx.region_id[hash] = region_id;
		} else {
		    bu_log("%s is not a valid region_id\n", region_id_val);
		    region_id = -1;
		}
	    }

	    // Check for an inherit flag
	    int color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
	    if (color_inherit)
		ctx.c_inherit[hash] = color_inherit;

	    // Color (note that the rt_material_head colors and a region_id may
	    // override this, as might a parent comb with color and the inherit
	    // flag both set.
	    struct bu_color c = BU_COLOR_INIT_ZERO;
	    const char *color_val = bu_avs_get(&c_avs, "color");
	    if (!color_val)
		color_val = bu_avs_get(&c_avs, "rgb");
	    if (color_val){
		bu_opt_color(NULL, 1, &color_val, (void *)&c);
		unsigned int cval = color_int(&c);
		ctx.rgb[hash] = cval;
	    }

	    // Done with attributes
	    bu_avs_free(&c_avs);
	}
    }

    //print_ctx_unordered(&ctx);
    //print_ctx_ordered(&ctx);
#if 0
    split_test("all.g/cone.r/cone.s");
    split_test("all.g/cone.r/cone.s/");
    split_test("all.g/cone.r/cone.s//");
    split_test("all.g/cone.r/cone.s\\//");
    split_test("all.g\\/cone.r\\//cone.s");
    split_test("all.g\\\\/cone.r\\//cone.s");
    split_test("all.g\\\\\\/cone.r\\//cone.s");
    split_test("all.g\\\\\\\\/cone.r\\//cone.s");
    split_test("all.g\\cone.r\\//cone.s");
    split_test("all.g\\\\cone.r\\//cone.s");
    split_test("all.g\\\\\\cone.r\\//cone.s");
    split_test("all.g\\\\\\\\cone.r\\//cone.s");

    {
	std::vector<std::string> pe;
	path_elements(pe, "all.g/cone.r/cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "all.g/cone2.r\\//cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "cone2.r/cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "cone2.r\\//cone.s");
	check_elements(gedp, &ctx, pe);
    }
#endif

    //draw_path(gedp, &ctx, "all.g");
    draw_path(gedp, &ctx, "havoc");
#if 0
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    for (k_it = ctx.s_keys.begin(); k_it != ctx.s_keys.end(); k_it++) {
	struct bu_color c = BU_COLOR_INIT_ZERO;
	if (get_path_color(&c, &ctx, k_it->second)) {
	    int r, g, b;
	     bu_color_to_rgb_ints(&c, &r, &g, &b);
	     bu_log("%llu: %d/%d/%d\n", k_it->first, r, g, b);
	} else {
	     bu_log("%llu: no color\n", k_it->first);
	}
    }
#endif

    if (g.changed_flag) {
	bu_log("changed\n");
	ctx_update(&ctx, &g);
	g.added.clear();
	g.removed.clear();
	g.changed.clear();
	g.changed_flag = false;
    }

    std::vector<unsigned long long> active_paths;
    std::vector<std::vector<unsigned long long>> collapsed;
    collapse(collapsed, &ctx.drawn_paths, active_paths, ctx.s_keys, &ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], &ctx, NULL);
	    bu_log("%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }


#if 1

    erase_path(&ctx, "havoc/havoc_middle");
    //erase_path(&ctx, "all.g/box.r");

    collapse(collapsed, &ctx.drawn_paths, active_paths, ctx.s_keys, &ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], &ctx, NULL);
	    bu_log("%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }

    //erase_path(&ctx, "all.g");
    erase_path(&ctx, "havoc");

    collapse(collapsed, &ctx.drawn_paths, active_paths, ctx.s_keys, &ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], &ctx, NULL);
	    bu_log("%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }

    //draw_path(gedp, &ctx, "all.g/box2.r");
    draw_path(gedp, &ctx, "havoc/havoc_middle");
    draw_path(gedp, &ctx, "rt_r.pyl1_atch/rt_s.ecov1");

    collapse(collapsed, &ctx.drawn_paths, active_paths, ctx.s_keys, &ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], &ctx, NULL);
	    bu_log("%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }


    //draw_path(gedp, &ctx, "box2.r");
    draw_path(gedp, &ctx, "havoc_tail");

    collapse(collapsed, &ctx.drawn_paths, active_paths, ctx.s_keys, &ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], &ctx, NULL);
	    bu_log("%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }
#endif

    const char *av[5] = {NULL};
    av[0] = "kill";
    av[1] = "rt_s.ecov1";
    ged_exec(gedp, 2, av);

    av[0] = "rm";
    av[1] = "havoc_middle";
    av[2] = "mid_body";
    ged_exec(gedp, 3, av);

    av[0] = "rm";
    av[1] = "havoc_middle";
    av[2] = "apu_cover";
    ged_exec(gedp, 3, av);

    if (g.changed_flag) {
	bu_log("changed\n");
	ctx_update(&ctx, &g);
	g.added.clear();
	g.removed.clear();
	g.changed.clear();
	g.changed_flag = false;
    }

    collapse(collapsed, &ctx.drawn_paths, active_paths, ctx.s_keys, &ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], &ctx, NULL);
	    bu_log("%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }

    av[0] = "kill";
    av[1] = "rt_r.pyl1_atch";
    ged_exec(gedp, 2, av);

    if (g.changed_flag) {
	bu_log("changed\n");
	ctx_update(&ctx, &g);
	g.added.clear();
	g.removed.clear();
	g.changed.clear();
	g.changed_flag = false;
    }

    collapse(collapsed, &ctx.drawn_paths, active_paths, ctx.s_keys, &ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < collapsed.size(); i++) {
	    print_path(&path_str, collapsed[i], &ctx, NULL);
	    bu_log("%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }



    ged_close(gedp);

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
