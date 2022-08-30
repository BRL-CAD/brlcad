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
/** @addtogroup ged_db*/
/** @{ */
/** @file libged/dbi_state.cpp
 *
 * Maintain and manage an in-memory representation of the database hierarchy.
 * Main utility of this is to make commonly needed hierarchy/attribute
 * information available to applications without having to crack combs from
 * disk during a standard tree walk.
 */

#include "common.h"

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"
}

#include "vmath.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "raytrace.h"
#include "ged/defines.h"

struct walk_data {
    DbiState *dbis;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    mat_t *curr_mat = NULL;
    unsigned long long phash = 0;
};

static void
populate_leaf(void *client_data, const char *name, matp_t c_m, int op)
{
    struct walk_data *d = (struct walk_data *)client_data;
    struct db_i *dbip = d->dbis->dbip;
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
	d->dbis->i_map[ihash] = chash;
	d->dbis->i_str[ihash] = std::string(bu_vls_cstr(&iname));
	d->dbis->p_c[d->phash].insert(ihash);
	d->dbis->p_v[d->phash].push_back(ihash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->dbis->invalid_entry_map[ihash] = std::string(bu_vls_cstr(&iname));
	} else {
	    // In case this was previously invalid, remove
	    d->dbis->invalid_entry_map.erase(ihash);
	}
	bu_vls_free(&iname);

	// For the next stages, if we have an ihash use it
	chash = ihash;

    } else {

	d->dbis->p_v[d->phash].push_back(chash);
	d->dbis->p_c[d->phash].insert(chash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->dbis->invalid_entry_map[chash] = std::string(name);
	} else {
	    // In case this was previously invalid, remove
	    d->dbis->invalid_entry_map.erase(chash);
	}

    }

    // If we have a non-IDN matrix, store it
    if (c_m) {
	for (int i = 0; i < 16; i++)
	    d->dbis->matrices[d->phash][chash].push_back(c_m[i]);
    }

    // If we have a non-UNION op, store it
    d->dbis->i_bool[d->phash][chash] = op;
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


void
DbiState::populate_maps(struct directory *dp, unsigned long long phash, int reset)
{
    if (!(dp->d_flags & RT_DIR_COMB))
	return;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
    pc_it = p_c.find(phash);
    pv_it = p_v.find(phash);
    if (pc_it == p_c.end() || pv_it != p_v.end() || reset) {
	if (reset && pc_it != p_c.end()) {
	    pc_it->second.clear();
	}
	if (reset && pv_it != p_v.end()) {
	    pv_it->second.clear();
	}
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, dbip, NULL, res) < 0)
	    return;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree)
	    return;

	std::unordered_map<unsigned long long, unsigned long long> i_count;
	struct walk_data d;
	d.dbis = this;
	d.phash = phash;
	populate_walk_tree(comb->tree, (void *)&d, 0, populate_leaf);
	rt_db_free_internal(&in);
    }
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

unsigned long long
DbiState::path_hash(std::vector<unsigned long long> &path, size_t max_len)
{
    return path_hash(path, max_len);
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

std::vector<unsigned long long>
DbiState::digest_path(const char *path)
{
    // If no path, nothing to process
    if (path)
	return std::vector<unsigned long long>();

    // Digest the string into individual path elements
    std::vector<std::string> elements;
    path_elements(elements, path);

    // Convert the string elements into hash elements
    std::vector<unsigned long long> phe;
    struct bu_vls hname = BU_VLS_INIT_ZERO;
    XXH64_state_t h_state;
    for (size_t i = 0; i < elements.size(); i++) {
	XXH64_reset(&h_state, 0);
	bu_vls_sprintf(&hname, "%s", elements[0].c_str());
	XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
	phe.push_back((unsigned long long)XXH64_digest(&h_state));
    }
    bu_vls_free(&hname); 

    // If we're cyclic, path is invalid
    if (path_cyclic(phe))
	return std::vector<unsigned long long>();

    // parent/child relationship validate
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_map<unsigned long long, unsigned long long>::iterator i_it;
    unsigned long long phash = phe[0];
    for (size_t i = 1; i < phe.size(); i++) {
	pc_it = p_c.find(phash);
	// The parent comb structure is stored only under its original name's hash - if
	// we have a numbered instance from a comb tree as a parent, we may be able to
	// map it to the correct entry with i_map.  If not, we have an invalid path.
	if (pc_it == p_c.end()) {
	    i_it = i_map.find(phash);
	    if (i_it == i_map.end())
		return std::vector<unsigned long long>();
	    phash = i_it->second;
	    pc_it = p_c.find(phash);
	    if (pc_it == p_c.end())
		return std::vector<unsigned long long>();
	}
	unsigned long long chash = phe[i];
	if (pc_it->second.find(chash) == pc_it->second.end()) {
	    bu_log("Invalid element path: %s\n", elements[i].c_str());
	    return std::vector<unsigned long long>();
	}
	phash = chash;
    }

    return phe;
}

#if 0
bool
DbiState::is_solid(unsigned long long key)
{
    // Fastest way should be to do a dp check
    std::unordered_map<unsigned long long, struct directory *>::iterator d_it;
    d_it = d_map.find(key);

    // If we didn't find it, check if this is an instance key
    if (d_it == d_map.end()) {
	std::unordered_map<unsigned long long, unsigned long long>::iterator i_it;
	i_it = i_map.find(key);
	if (i_it != i_map.end()) {
	    d_it = d_map.find(i_it->second);
	    // An invalid entry can't be walked, so treat it as a solid
	    if (d_it == d_map.end()) {
		return true;
	    }
	} else {
	    // An invalid entry can't be walked, so treat it as a solid
	    return true;
	}
    }

    if (!(d_it->second->d_flags & RT_DIR_COMB))
	return true;

    return false;
}

std::vector<unsigned long long> *
DbiState::comb_children(unsigned long long key)
{
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
    pv_it = p_v.find(key);

    // If we didn't find it, check if this is an instance key
    if (pv_it == p_v.end()) {
	std::unordered_map<unsigned long long, unsigned long long>::iterator i_it;
	i_it = i_map.find(key);
	if (i_it != i_map.end()) {
	    pv_it = p_v.find(i_it->second);
	    if (pv_it == p_v.end()) {
		return NULL;
	    }
	} else {
	    return NULL;
	}
    }

    return &pv_it->second;
}
#endif

unsigned int
DbiState::color_int(struct bu_color *c)
{
    if (!c)
	return 0;
    int r, g, b;
    bu_color_to_rgb_ints(c, &r, &g, &b);
    unsigned int colors = r + (g << 8) + (b << 16);
    return colors;
}

int
DbiState::int_color(struct bu_color *c, unsigned int cval)
{
    if (!c)
	return 0;

    int r, g, b;
    r = cval & 0xFF;
    g = (cval >> 8) & 0xFF;
    b = (cval >> 16) & 0xFF;

    unsigned char lrgb[3];
    lrgb[0] = (unsigned char)r;
    lrgb[1] = (unsigned char)g;
    lrgb[2] = (unsigned char)b;

    return bu_color_from_rgb_chars(c, lrgb);
}

unsigned long long
DbiState::update_dp(struct directory *dp, int reset)
{
    if (dp->d_flags & DB_LS_HIDDEN)
	return 0;

    // Set up to go from hash back to name
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
    unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);
    d_map[hash] = dp;

    // If this isn't a comb, find and record the untransformed
    // bounding box
    bboxes.erase(hash);
    if (!(dp->d_flags & RT_DIR_COMB)) {
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
	struct bn_tol tol = BG_TOL_INIT;
	point_t bmin, bmax;
	mat_t m;
	MAT_IDN(m);
	int bret = rt_bound_instance(&bmin, &bmax, dp, dbip,
		&ttol, &tol, &m, res, NULL);
	if (bret != -1) {
	    for (size_t j = 0; j < 3; j++)
		bboxes[hash].push_back(bmin[j]);
	    for (size_t j = 0; j < 3; j++)
		bboxes[hash].push_back(bmax[j]);
	}
    }

    // Encode hierarchy info if this is a comb
    if (dp->d_flags & RT_DIR_COMB)
	populate_maps(dp, hash, reset);

    // Check for various drawing related attributes
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
    db5_get_attributes(dbip, &c_avs, dp);

    // Check for region id.  For drawing purposes this needs to be a number.
    const char *region_id_val = bu_avs_get(&c_avs, "region_id");
    int attr_region_id = -1;
    if (region_id_val) {
	if (bu_opt_int(NULL, 1, &region_id_val, (void *)&attr_region_id) != -1) {
	    region_id[hash] = attr_region_id;
	} else {
	    bu_log("%s is not a valid region_id\n", region_id_val);
	    attr_region_id = -1;
	}
    }

    // Check for an inherit flag
    int color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
    c_inherit.erase(hash);
    if (color_inherit)
	c_inherit[hash] = color_inherit;

    // Color (note that the rt_material_head colors and a region_id may
    // override this, as might a parent comb with color and the inherit
    // flag both set.
    rgb.erase(hash);
    struct bu_color c = BU_COLOR_INIT_ZERO;
    const char *color_val = bu_avs_get(&c_avs, "color");
    if (!color_val)
	color_val = bu_avs_get(&c_avs, "rgb");
    if (color_val){
	bu_opt_color(NULL, 1, &color_val, (void *)&c);
	unsigned int cval = color_int(&c);
	rgb[hash] = cval;
    }

    // Done with attributes
    bu_avs_free(&c_avs);

    return hash;
}

bool
DbiState::path_color(struct bu_color *c, std::vector<unsigned long long> &elements)
{
    // This may not be how we'll always want to do this, but at least for the
    // moment (to duplicate observed MGED behavior) the first region_id seen
    // along the path with an active color in rt_material_head trumps all other
    // color values set by any other means.
    if (rt_material_head() != MATER_NULL) {
	std::unordered_map<unsigned long long, int>::iterator r_it;
	int path_region_id;
	for (size_t i = 0; i < elements.size(); i++) {
	    r_it = region_id.find(elements[i]);
	    if (r_it == region_id.end())
		continue;
	    path_region_id = r_it->second;
	    const struct mater *mp;
	    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
		if (path_region_id > mp->mt_high || path_region_id < mp->mt_low)
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
	ci_it = c_inherit.find(elements[i]);
	if (ci_it == c_inherit.end())
	    continue;
	rgb_it = rgb.find(elements[i]);
	if (rgb_it == rgb.end())
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
	rgb_it = rgb.find(*v_it);
	if (rgb_it == rgb.end())
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

bool
DbiState::get_matrix(matp_t m, unsigned long long p_key, unsigned long long i_key)
{
    if (UNLIKELY(!m || p_key == 0 || i_key == 0))
	return false;

    std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, std::vector<fastf_t>>>::iterator m_it;
    std::unordered_map<unsigned long long, std::vector<fastf_t>>::iterator mv_it;
    m_it = matrices.find(p_key);
    if (m_it == matrices.end())
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

bool
DbiState::get_path_matrix(matp_t m, std::vector<unsigned long long> &elements)
{
    bool have_mat = false;
    if (UNLIKELY(!m))
	return have_mat;

    MAT_IDN(m);
    if (elements.size() < 2)
	return have_mat;

    unsigned long long phash = elements[0];
    for (size_t i = 1; i < elements.size(); i++) {
	unsigned long long chash = elements[i];
	mat_t nm; 
	bool got_mat = get_matrix(nm, phash, chash);
	if (got_mat) {
	    mat_t cmat;
	    bn_mat_mul(cmat, m, nm);
	    MAT_COPY(m, cmat);
	    have_mat = true;
	}   
	phash = chash;
    }

    return have_mat;
}

bool
DbiState::get_bbox(point_t *bbmin, point_t *bbmax, matp_t curr_mat, unsigned long long hash)
{

    if (UNLIKELY(!bbmin || !bbmax || hash == 0))
	return false;

    bool ret = false;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_set<unsigned long long>::iterator s_it;
    unsigned long long key = hash;
    // First, see if this is an instance we need to translate to its canonical
    // .g database name
    if (i_map.find(hash) != i_map.end())
	key = i_map[hash];

    // See if we have a direct bbox lookup available
    std::unordered_map<unsigned long long, std::vector<fastf_t>>::iterator b_it;
    b_it = bboxes.find(key);
    if (b_it != bboxes.end()) {
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
    pc_it = p_c.find(key);
    if (pc_it != p_c.end()) {
	// Have comb children - incorporate each one
	for (s_it = pc_it->second.begin(); s_it != pc_it->second.end(); s_it++) {
	    unsigned long long child_hash = *s_it;
	    // See if we have a matrix for this case - if so, we need to
	    // incorporate it
	    mat_t nm;
	    bool have_mat = get_matrix(nm, key, child_hash);
	    if (have_mat) {
		// Construct new "current" matrix
		if (curr_mat) {
		    // If we already have a non-IDN matrix from parent
		    // path elements, we need to multiply the matrices
		    // to accumulate the position changes
		    mat_t om;
		    MAT_COPY(om, curr_mat);
		    bn_mat_mul(curr_mat, om, nm);
		    if (get_bbox(bbmin, bbmax, curr_mat, child_hash))
			ret = true;
		    MAT_COPY(curr_mat, om);
		} else {
		    // If this is the first non-IDN matrix, we don't
		    // need to combine it with parent matrices
		    if (get_bbox(bbmin, bbmax, nm, child_hash))
			ret = true;
		}
	    } else {
		if (get_bbox(bbmin, bbmax, curr_mat, child_hash))
		    ret = true;
	    }
	}
    }

    return ret;
}

bool
DbiState::get_path_bbox(point_t *bbmin, point_t *bbmax, std::vector<unsigned long long> &elements)
{
    if (UNLIKELY(!bbmin || !bbmax || !elements.size()))
	return false;

    // Everything but the last element should be a comb - we only need to
    // assemble a matrix from the path (if there are any non-identity matrices)
    // and call get_bbox on the last element.
    bool have_mat = false;
    mat_t start_mat;
    MAT_IDN(start_mat);
    for (size_t i = 0; i < elements.size() - 1; i++) {
	mat_t nm;
	bool got_mat = get_matrix(nm, elements[i], elements[i+1]);
	if (got_mat) {
	    mat_t cmat;
	    bn_mat_mul(cmat, start_mat, nm);
	    MAT_COPY(start_mat, cmat);
	    have_mat = true;
	}
    }
    if (have_mat) {
	return get_bbox(bbmin, bbmax, start_mat, elements[elements.size() - 1]);
    }

    return get_bbox(bbmin, bbmax, NULL, elements[elements.size() - 1]);
}


void
DbiState::update()
{
    if (!added.size() && !changed.size() && !removed.size()) {
	changed_hashes.clear();
	old_names.clear();
	return;
    }

    std::unordered_set<unsigned long long>::iterator s_it;
    std::unordered_set<struct directory *>::iterator g_it;

    if (need_update_nref) {
	db_update_nref(dbip, res);
	need_update_nref = false;
    }

    // dps -> hashes
    XXH64_state_t h_state;
    changed_hashes.clear();
    for(g_it = changed.begin(); g_it != changed.end(); g_it++) {
	struct directory *dp = *g_it;
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);
	changed_hashes.insert(hash);
    }

    // For all associated view states, cache their collapsed drawn
    // paths
    shared_vs->cache_collapsed();
    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    for (v_it = view_states.begin(); v_it != view_states.end(); v_it++) {
	if (v_it->second == shared_vs)
	    continue;
	v_it->second->cache_collapsed();
    }

    // Update the primary data structures 
    for(s_it = removed.begin(); s_it != removed.end(); s_it++) {
	bu_log("removed: %llu\n", *s_it);

	// Combs with this key in their child set need to be updated to refer
	// to it as an invalid entry.
	std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
	for (pv_it = p_v.begin(); pv_it != p_v.end(); pv_it++) {
	    for (size_t i = 0; i < pv_it->second.size(); i++) {
		if (i_map.find(pv_it->second[i]) != i_map.end()) {
		    invalid_entry_map[i_map[pv_it->second[i]]] = i_str[i_map[pv_it->second[i]]];
		} else {
		    invalid_entry_map[pv_it->second[i]] = old_names[*s_it];
		}
	    }
	}

	d_map.erase(*s_it);
	bboxes.erase(*s_it);
	c_inherit.erase(*s_it);
	rgb.erase(*s_it);
	region_id.erase(*s_it);
	matrices.erase(*s_it);
	i_bool.erase(*s_it);

	// We do not clear the instance maps (i_map and i_str) since those containers do not
	// guarantee uniqueness to one child object.  To remove entries no longer
	// used anywhere in the database, we have to confirm they are no longer needed on a global
	// basis in a subsequent garbage-collect operation.

	// Entries with this hash as their key are erased.  
	p_c.erase(*s_it);
	p_v.erase(*s_it);
    }

    for(g_it = added.begin(); g_it != added.end(); g_it++) {
	struct directory *dp = *g_it;
	bu_log("added: %s\n", dp->d_namep);
	unsigned long long hash = update_dp(dp, 0);

	// If this name was previously the source of an invalid reference,
	// it is no longer.
	invalid_entry_map.erase(hash);
    }

    for(g_it = changed.begin(); g_it != changed.end(); g_it++) {
	struct directory *dp = *g_it;
	bu_log("changed: %s\n", dp->d_namep);
	// Properties need to be updated - comb children, colors, matrices,
	// bounding box for solids, etc.
	update_dp(dp, 1);
    }

    // Garbage collect i_map and i_str
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator sk_it;
    std::unordered_set<unsigned long long> used;
    for (sk_it = p_v.begin(); sk_it != p_v.end(); sk_it++) {
	used.insert(sk_it->second.begin(), sk_it->second.end());
    }
    std::vector<unsigned long long> unused;
    std::unordered_map<unsigned long long, unsigned long long>::iterator im_it;
    for (im_it = i_map.begin(); im_it != i_map.end(); im_it++) {
	if (used.find(im_it->first) != used.end())
	    unused.push_back(im_it->first);
    }
    for (size_t i = 0; i < unused.size(); i++) {
	i_map.erase(unused[i]);
	i_str.erase(unused[i]);
    }

    // For all associated view states, execute any necessary changes to
    // view objects and lists
    shared_vs->redraw();
    for (v_it = view_states.begin(); v_it != view_states.end(); v_it++) {
	if (v_it->second == shared_vs)
	    continue;
	v_it->second->redraw();
    }

    // Updates done, clear items stored by callbacks
    added.clear();
    changed.clear();
    changed_hashes.clear();
    removed.clear();
    old_names.clear();
}


DbiState::DbiState(struct db_i *dbi_p)
{
    BU_GET(res, struct resource);
    rt_init_resource(res, 0, NULL);
    shared_vs = new BViewState(this);
    dbip = dbi_p;
    if (!dbip)
	return;

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    update_dp(dp, 0);
	}
    }
}

DbiState::~DbiState()
{
    delete shared_vs;
    rt_clean_resource_basic(NULL, res);
    BU_PUT(res, struct resource);
}

BViewState::BViewState(DbiState *s)
{
    dbis = s;
}

// 0 = valid, 1 = invalid, 2 = invalid, remain "drawn"
int
BViewState::check_status(
	std::unordered_set<unsigned long long> *invalid_objects,
       	std::vector<unsigned long long> &cpath
	)
{
    bool parent_changed = false;
    for (size_t j = 0; j < cpath.size(); j++) {
	unsigned long long phash = (j > 0) ? cpath[j-1] : 0;
	unsigned long long hash = cpath[j];
	if (phash && parent_changed) {
	    // Need to see if this is still a parent of the new comb. This step
	    // is why the draw update has to come AFTER the above primitive
	    // update passes, so the comb can give us the correct, current
	    // answer.
	    bool is_parent = false;
	    if (dbis->p_c.find(phash) != dbis->p_c.end() && dbis->p_c[phash].find(hash) != dbis->p_c[phash].end())
		is_parent = true;
	    // If not we're done, whether or not the parent dp was
	    // removed from the database.
	    if (!is_parent) {
		if (invalid_objects)
		    (*invalid_objects).insert(hash);
		return 1;
	    }
	    // If it's still in the comb tree, proceed with the evaluation.
	}

	bool is_removed = (dbis->removed.find(hash) != dbis->removed.end());
	bool is_changed = (dbis->changed_hashes.find(hash) != dbis->changed_hashes.end());
	if (is_removed) {
	    if (is_removed && !j) {
		// Top level removed - everything else is gone
		if (invalid_objects)
		    (*invalid_objects).insert(hash);
		return 1;
	    }

	    if (is_removed && j != cpath.size()-1) {
		// If removed is first and not a leaf, erase - if we got here
		// the parent comb either wasn't changed at all or this
		// particular instance is still there; either way the state
		// here is not preservable, since the path is trying to refer
		// to a tree path which no longer exists in the hierarchy.
		if (invalid_objects)
		    (*invalid_objects).insert(hash);
		return 1;
	    }
	    if (is_removed && j == cpath.size()-1) {
		// If removed is a leaf and the comb instance is intact,
		// leave "drawn" as invalid path.
		return 2;
	    }
	}
	if (is_changed) {
	    if (j == cpath.size()-1) {
		// Changed, but a leaf - stays drawn
		return 0;
	    }
	    // Not a leaf - check child
	    parent_changed = true;
	    continue;
	}

	// If we got here, reset the parent changed flag
	parent_changed = false;
    }

    return 0;
}

unsigned long long
BViewState::path_hash(std::vector<unsigned long long> &path, size_t max_len)
{
    return path_hash(path, max_len);
}

void
BViewState::collapse(std::vector<std::vector<unsigned long long>> &collapsed)
{
    std::unordered_set<unsigned long long>::iterator s_it;
    std::map<size_t, std::unordered_set<unsigned long long>> depth_groups;

    // Reset containers
    collapsed.clear();
    all_fully_drawn.clear();

    // Group paths of the same depth.  Depth == 1 paths are already
    // top level objects and need no further processing.  If active_paths
    // is empty, the active set is all paths in input_map
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    if (active_paths.size()) {
	for (size_t i = 0; i != active_paths.size(); i++) {
	    k_it = s_keys.find(active_paths[i]);
	    if (k_it == s_keys.end())
		continue;
	    if (k_it->second.size() == 1) {
		collapsed.push_back(k_it->second);
		all_fully_drawn.insert(k_it->first);
	    } else {
		depth_groups[k_it->second.size()].insert(k_it->first);
	    }
	}
    } else {
	for (k_it = s_keys.begin(); k_it != s_keys.end(); k_it++) {
	    if (k_it->second.size() == 1) {
		collapsed.push_back(k_it->second);
		all_fully_drawn.insert(k_it->first);
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
	    std::vector<unsigned long long> &pc_path = s_keys[*s_it];
	    grouped_pckeys[pc_path[plen-2]].insert(*s_it);
	}

	// For each parent/child grouping, compare it against the .g ground
	// truth set.  If they match, fully drawn and we promote the path to
	// the parent depth.  If not, the paths do not collapse further and are
	// added to drawn paths.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pg_it;
	for (pg_it = grouped_pckeys.begin(); pg_it != grouped_pckeys.end(); pg_it++) {

	    // As above, use the full path from the s_keys, but this time
	    // we're collecting the children.  This is the set we need to compare
	    // against the .g ground truth to determine fully or partially drawn.
	    std::unordered_set<unsigned long long> g_children;
	    std::unordered_set<unsigned long long> &g_pckeys = pg_it->second;
	    for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		std::vector<unsigned long long> &pc_path = s_keys[*s_it];
		g_children.insert(pc_path[plen-1]);
	    }

	    // Do the check against the .g comb children info - the "ground truth"
	    // that defines what must be present for a fully drawn comb
	    bool is_fully_drawn = true;
	    std::unordered_set<unsigned long long> &ground_truth = dbis->p_c[pg_it->first];
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
		for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		    std::vector<unsigned long long> &pc_path = s_keys[*s_it];
		    // Hash only to the current path length (we may have deeper
		    // promoted paths being used at this level)
		    all_fully_drawn.insert(path_hash(pc_path, plen));
		}
	    } else {
		// No further collapsing - add to final.  We must make trimmed
		// versions of the paths in case this depth holds promoted
		// paths from deeper levels, since we are duplicating the full
		// path contents.
		for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		    std::vector<unsigned long long> trimmed = s_keys[*s_it];
		    trimmed.resize(plen);
		    collapsed.push_back(trimmed);
		    // Hash is calculated on that trimmed path, so this time we
		    // don't need to specify a depth limit.
		    all_fully_drawn.insert(path_hash(trimmed, 0));
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
	    std::vector<unsigned long long> trimmed = s_keys[*s_it];
	    trimmed.resize(1);
	    //if (dbis->d_map.find(trimmed[0]) == dbis->d_map.end())
	//	continue;
	    collapsed.push_back(trimmed);
	}
    }
}

void
BViewState::cache_collapsed()
{
    // Collect all the paths with either removed or changed dps
    active_paths.clear();
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    for (k_it = s_keys.begin(); k_it != s_keys.end(); k_it++) {
	for (size_t i = 0; i < k_it->second.size(); i++) {
	    unsigned long long hash = k_it->second[i];
	    // If this is an instance, we need to map it to its canonical
	    // comb hash for the check
	    if (dbis->i_map.find(hash) != dbis->i_map.end()) {
		hash = dbis->i_map[hash];
	    }
	    if (dbis->removed.find(hash) != dbis->removed.end()) {
		active_paths.push_back(k_it->first);
		break;
	    }
	    if (dbis->changed_hashes.find(hash) != dbis->changed_hashes.end()) {
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
    prev_collapsed.clear();
    collapse(prev_collapsed);
#if 0
    {
	// DEBUG - print results
	bu_log("\n\nCollapsed altered paths:\n");
	struct bu_vls path_str = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < prev_collapsed.size(); i++) {
	    print_path(&path_str, prev_collapsed[i], ctx, &old_names);
	    bu_log("\t%s\n", bu_vls_cstr(&path_str));
	}
	bu_vls_free(&path_str);
	bu_log("\n");
    }
#endif
}

void
BViewState::redraw()
{
    // The principle for redrawing will be that anything that was previously
    // fully drawn should stay fully drawn, even if its tree structure has
    // changed.  We need to remove no-longer-valid paths, but will keep valid
    // paths to avoid the work of re-generating the scene objects when we
    // re-expand the collapsed paths using the new tree structure.

    std::unordered_set<unsigned long long> invalid_objects;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator sk_it;
    for (sk_it = s_keys.begin(); sk_it != s_keys.end(); sk_it++) {
	// Work down from the root of each path looking for the first changed or
	// removed entry.
	std::vector<unsigned long long> &cpath = sk_it->second;
	check_status(&invalid_objects, cpath);
    }

    std::unordered_set<unsigned long long>::iterator iv_it;
    for (iv_it = invalid_objects.begin(); iv_it != invalid_objects.end(); iv_it++) {
	s_keys.erase(*iv_it);
	bu_log("erase: %llu\n", *iv_it);
	if (s_map.find(*iv_it) != s_map.end()) {
	    // an invalid child entry may be present in s_keys but not have an
	    // associated scene object.
	// free scene obj s_map[*iv_it];
	}
    }

    // Also need to evaluate collapsed paths according to the same criteria, before
    // we re-expand them
    std::unordered_set<size_t> active_collapsed;
    std::unordered_set<size_t> draw_invalid_collapsed;
    for (size_t i = 0; i < prev_collapsed.size(); i++) {
	std::vector<unsigned long long> &cpath = prev_collapsed[i];
	int sret = check_status(NULL, cpath);
	if (sret == 2)
	    draw_invalid_collapsed.insert(i);
	if (sret == 0)
	    active_collapsed.insert(i);
    }

    // Expand active collapsed paths to solids, creating any missing path objects


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

    // Final step - rebuild drawn_paths to reflect current drawn state

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
