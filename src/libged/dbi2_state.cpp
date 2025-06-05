/*                D B I 2 _ S T A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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

#include <algorithm>
#include <map>
#include <thread>
#include <fstream>
#include <sstream>

extern "C" {
#include "lmdb.h"
}

#include "./alphanum.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bu/path.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bv/lod.h"
#include "raytrace.h"
#include "ged/defines.h"
#include "ged/view.h"
#include "./ged_private.h"


DbiPath::DbiPath(DbiState *dbis, const char *path)
{
    d = dbis;

    // If no path, or no way to decode it, we're done
    if (!path || !d)
	return;

    // Digest the string into individual path elements
    std::vector<std::string> substrs;
    split_path(substrs, path);
    unsigned long long chash = 0;
    for (size_t i = 0; i < substrs.size(); i++) {
	std::string cleared = name_deescape(substrs[i]);
	unsigned long long ehash = bu_data_hash(cleared.c_str(), strlen(cleared.c_str())*sizeof(char));
	unsigned long long ohash = 0;
	// Validate against the DbiState - if we don't have a supporting
	// GObj or CombInst, we don't have a valid path element
	if (i == 0) {
	    // .g object lookups are simple - it's just the hash of the name
	    ohash = push(ehash);
	    if (!ohash)
		bu_log("Invalid first path element (must be GObj): %s\n", substrs[0].c_str());
	} else {
	    // Comb instance lookup hashes are made up of a combination of the
	    // parent comb's name hash and the instance reference hash
	    std::vector<unsigned long long> lvec;
	    lvec.push_back(chash);
	    lvec.push_back(ehash);
	    ehash = bu_data_hash(lvec.data(), lvec.size() * sizeof(unsigned long long));
	    ohash = push(ehash);
	    if (!ohash)
		bu_log("Invalid comb instance - %s/%s\n", substrs[i-1].c_str(), substrs[i].c_str());
	}
	if (!ohash) {
	    bu_log("Failed to create valid DbiPath from string: %s\n", path);
	    elements.clear();
	    return;
	}

	// If we made a cyclic path, there's no point in processing further
	if (UNLIKELY(is_cyclic)) {
	    bu_log("Note path %s is cyclic\n", path);
	    return;
	}
    }
}

DbiPath::DbiPath(const DbiPath &p)
{
    is_cyclic = p.is_cyclic;
    is_valid = p.is_valid;
    elements.clear();
    d = p.d;
    parent_path_hashes.clear();
    std::unordered_set<unsigned long long>::const_iterator p_it;
    for (p_it = p.parent_path_hashes.begin(); p_it != p.parent_path_hashes.end(); ++p_it) {
	parent_path_hashes.insert(*p_it);
    }
    path_hash = p.path_hash;
}

DbiPath::~DbiPath()
{
    is_cyclic = 0;
    is_valid = 1;
    elements.clear();
    d = NULL;
    parent_path_hashes.clear();
    path_hash = 0;
}

bool
DbiPath::parent(DbiPath &p)
{
    // If we are listed in p's parent hashes, we are its parent
    if (p.parent_path_hashes.find(path_hash) == p.parent_path_hashes.end())
	return true;

    return false;
}

bool
DbiPath::child(DbiPath &p)
{
    // If p is our parent, we are its child
    if (parent_path_hashes.find(p.path_hash) == parent_path_hashes.end())
	return true;

    return false;
}

bool
DbiPath::color(struct bu_color *c)
{
    const struct mater *mp;

    std::vector<GObj *> cgs = d->get_gobjs(elements);

    // This may not be how we'll always want to do this, but at least for the
    // moment (to duplicate observed MGED behavior) the first region_id seen
    // along the path with an active color in rt_material_head trumps all other
    // color values set by any other means.
    if (rt_material_head() != MATER_NULL) {
	for (size_t i = 0; i < cgs.size(); i++) {
	    if (cgs[i]->region_id >= 0) {
		for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
		    if (cgs[i]->region_id > mp->mt_high || cgs[i]->region_id < mp->mt_low)
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
    }

    // Next, check for an inherited color.  If we have one (the behavior seen in MGED
    // appears to require a comb with both inherit and a color value set to override
    // lower colors) then we are done.
    for (size_t i = 0; i < cgs.size(); i++) {
	if (!cgs[i]->c_inherit)
	    continue;
	if (!cgs[i]->color_set)
	    continue;
	BU_COLOR_CPY(c, &cgs[i]->color);
	return true;
    }

    // If we don't have an inherited color, it works the other way around - the
    // lowest set color wins.  Note that a region flag doesn't automatically
    // override a lower color level - i.e. there is no implicit inherit flag
    // in a region being set on a comb.
    for (long long i = cgs.size()-1; i >= 0; i--) {
	if (!cgs[i]->color_set)
	    continue;
	BU_COLOR_CPY(c, &cgs[i]->color);
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
DbiPath::matrix(matp_t m)
{
    if (UNLIKELY(!m))
	return false;

    bool have_mat = false;

    MAT_IDN(m);
    if (elements.size() < 2)
	return false;

    // The root GObj doesn't have a matrix, but any comb instances
    // in the path might - check all of them
    for (size_t i = 1; i < elements.size(); i++) {
	CombInst *c = GetCombInst(i);
	if (UNLIKELY(!c))
	    return false;

	if (c->non_default_matrix) {
	    mat_t cmat;
	    bn_mat_mul(cmat, m, c->m);
	    MAT_COPY(m, cmat);
	    have_mat = true;
	}
    }

    return have_mat;
}


bool
DbiPath::is_subtraction()
{
    // GObj only path is never a subtraction
    if (elements.size() < 2)
	return false;

    for (size_t i = 1; i < elements.size(); i++) {
	CombInst *c = GetCombInst(i);
	if (UNLIKELY(!c))
	    return false;

	if (c->boolean_op == OP_SUBTRACT)
	    return true;
    }

    return false;
}

bool
DbiPath::is_intersection()
{
    // GObj only path is never an intersection
    if (elements.size() < 2)
	return false;

    for (size_t i = 1; i < elements.size(); i++) {
	CombInst *c = GetCombInst(i);
	if (UNLIKELY(!c))
	    return false;

	if (c->boolean_op == OP_INTERSECT)
	    return true;
    }

    return false;
}

bool
DbiPath::valid()
{
    if (!d || !is_valid)
	return false;

    // Check if the path objs are valid
    if (d->gobjs.find(elements[0]) == d->gobjs.end()) {
	is_valid = false;
	return false;
    }
    for (size_t i = 1; i < elements.size(); i++) {
	if (d->combinsts.find(elements[i]) == d->combinsts.end()) {
	    is_valid = false;
	    return false;
	}
    }

    return true;
}

bool
DbiPath::cyclic(bool full_check)
{
    // We can't properly check an invalid path
    if (!is_valid)
	return false;

    // A GObj only path cannot be cyclic
    if (elements.size() < 2)
	is_cyclic = 0;

    // If we already know the answer, don't do the (expensive)
    // recheck
    if (is_cyclic < 2)
	return (is_cyclic) ? true : false;

    // Check path elements against parents to see if there are any self
    // references.  If we're doing a full check do them all, otherwise
    // just do the leaf.
    size_t leaf_ind = elements.size() - 1;
    while (leaf_ind > 0) {
	CombInst *c = d->combinsts[leaf_ind];
	unsigned long long chash = c->ohash;
	unsigned long long phash = 0;
	int j = leaf_ind - 1;
	while (j >= 0) {
	    if (j > 0) {
		CombInst *cp = GetCombInst(j);
		if (UNLIKELY(!cp))
		    return false;
		phash = cp->ohash;
	    } else {
		GObj *gp = GetGObj();
		if (UNLIKELY(!gp))
		    return false;
		phash = gp->hash;
	    }
	    if (chash == phash) {
		// Found a cycle in the path
		is_cyclic = true;
		return true;
	    }
	    // All clear - check next element
	    j--;
	}

	// If we're not doing a full check, by this point in the first pass
	// we can call it not cyclic.
	if (!full_check)
	    return false;

	// Keep going - decrement our leaf index
	leaf_ind--;
    }

    // If we got this far, it is not cyclic
    return false;
}

size_t
DbiPath::depth()
{
    if (UNLIKELY(!elements.size()))
	return 0;
    return elements.size() - 1;
}


std::string
DbiPath::str(size_t pmax, int verbose)
{
    if (!is_valid)
	return std::string("Invalid path");

    std::string pstr;
    // We're always printing the first element
    GObj *g = GetGObj();
    if (UNLIKELY(!g)) {
	pstr.append(std::string("Invalid path"));
	return pstr;
    }

    pstr.append(g->name);

    // Print any comb instances
    size_t ecnt = (pmax) ? pmax : elements.size();
    for (size_t i = 1; i < ecnt; i++) {

	pstr.append(std::string("/"));

	CombInst *cp = GetCombInst(i);
	if (UNLIKELY(!cp)) {
	    pstr.append(std::string("Invalid path"));
	    return pstr;
	}

	if (verbose > 0 && cp->non_default_matrix)
	    pstr.append(std::string("M"));

	// TODO - incorporate boolean op printing as well

	pstr.append(cp->iname.length() ? cp->iname : cp->oname);

    }

    return pstr;
}

bool
DbiPath::bbox(point_t *bmin, point_t *bmax)
{
    if (!elements.size())
	return false;

    point_t lbmin, lbmax;
    VSETALL(lbmin, INFINITY);
    VSETALL(lbmax, -INFINITY);

    // One element path, just return the GObj bbox
    std::unordered_map<unsigned long long, GObj *>::iterator p;
    if (elements.size() == 1) {
	p = d->gobjs.find(elements[0]);
	if (p == d->gobjs.end()) {
	    is_valid = false;
	    return false;
	}
	GObj *g = d->gobjs[elements[0]];
	g->bbox(&lbmin, &lbmax);
	VMINMAX(*bmin, *bmax, lbmin);
	VMINMAX(*bmin, *bmax, lbmax);
	return true;
    }

    // Get the CombInst leaf's assocated GObj bbox.  This is a geometry
    // calculation, so if there is no associated GObj we can't proceed.
    // However, it doesn't imply an invalid path, since a comb tree in a .g may
    // reference a non existent object.
    GObj *g = GetGObj(depth());
    if (!g)
	return false;
    g->bbox(&lbmin, &lbmax);

    // Build up the path matrix and apply it, if it is non IDN
    mat_t m;
    if (matrix(m)) {
	point_t tbmin, tbmax;
	MAT4X3PNT(tbmin, m, lbmin);
	MAT4X3PNT(tbmax, m, lbmax);
	VMOVE(lbmin, tbmin);
	VMOVE(lbmax, tbmax);
    }

    // Set the results
    VMINMAX(*bmin, *bmax, lbmin);
    VMINMAX(*bmin, *bmax, lbmax);

    return true;
}

unsigned long long
DbiPath::hash(size_t max_len)
{
    if (UNLIKELY(!elements.size()))
	return 0;
    size_t mlen = (max_len) ? max_len : elements.size();
    return bu_data_hash(elements.data(), mlen * sizeof(unsigned long long));
}


// Validate a proposed new path element, and add if if valid.
//
// If a path is already cyclic, or invalid, we can't add anything else to it.
// We DO need to be able to represent cyclic paths, since they can occur in a
// .g database, but we don't allow them to grow beyond the cyclic addition.
unsigned long long
DbiPath::push(unsigned long long new_element)
{
    // If we're cyclic or invalid we can't add anything else
    if (UNLIKELY(is_cyclic || !is_valid || !d))
	return 0;

    // Adding root element, which must be a GObj - make sure we have a valid
    // GObj hash
    if (!elements.size()) {
	std::unordered_map<unsigned long long, GObj *>::iterator r;
	r =  d->gobjs.find(new_element);
	if (r == d->gobjs.end()) {
	    // If we have an invalid GObj hash, we can't create a valid path
	    return 0;
	}
	elements.push_back(new_element);
	path_hash = hash();
	return new_element;
    }

    // Adding a new Comb element.  A Comb instance doesn't necessarily need to
    // have its referenced instance solid in the database, but it DOES need to
    // have a CombInst in DbiState or it is not a valid hash to use in a path.
    // If we've been fed a non-CombInst hash, the add op fails.
    std::unordered_map<unsigned long long, CombInst *>::iterator cp;
    cp = d->combinsts.find(new_element);
    if (cp == d->combinsts.end())
	return 0;

    // We have a CombInst, so we can add the element to the path
    elements.push_back(new_element);

    // Update current hash and parent hashes
    parent_path_hashes.insert(path_hash);
    path_hash = hash();

    // Need to check if path is now cyclic.  Assuming previously defined path
    // is NOT cyclic, per the is_cyclic check at the beginning of this method,
    // so we only need to look at the last element and the default cyclic()
    // test is sufficient.
    is_cyclic = 2;
    cyclic();

    // Successfully added
    return cp->second->ohash;
}

void
DbiPath::pop(bool no_check)
{
    // Don't pop_back on an empty vector - undefined behavior
    if (elements.size())
	elements.pop_back();

    // Update current hash and parent hashes
    path_hash = hash();
    parent_path_hashes.erase(path_hash);

    // If we have an empty path, or the user has instructed us to skip
    // validation, we're done
    if (no_check || !elements.size()) {
	// Reset
	is_valid = true;
	is_cyclic = 0;
	return;
    }

    // See if we removed an invalid element
    if (!is_valid) {
	// Reset validity
	is_valid = true;
	// Re-run the check
	is_valid = valid();
    }

    // If we're not empty, and we were previously cyclic,
    // check again
    if (is_cyclic) {
	is_cyclic = 2;
	cyclic();
    }
}

CombInst *
DbiPath::GetCombInst(size_t ind)
{
    if (UNLIKELY(!is_valid || elements.size() < 2))
	return NULL;

    if (UNLIKELY(ind > elements.size() - 1))
	return NULL;

    // For CombInst, 0 ind implies returning the Leaf
    int search_ind = (!ind) ? elements.size() - 1 : ind;

    std::unordered_map<unsigned long long, CombInst *>::iterator c;
    c = d->combinsts.find(elements[search_ind]);
    if (c == d->combinsts.end()) {
	// Path appears invalid
	is_valid = false;
	return NULL;
    }

    return c->second;
}

GObj *
DbiPath::GetGObj(size_t ind)
{
    if (UNLIKELY(!is_valid || !elements.size()))
	return NULL;

    if (UNLIKELY(ind > elements.size() - 1))
	return NULL;

    std::unordered_map<unsigned long long, GObj *>::iterator p;

    // If we are after the root, that's a straight lookup
    if (!ind) {
	p = d->gobjs.find(elements[0]);
	if (UNLIKELY(p == d->gobjs.end())) {
	    // Path appears invalid
	    is_valid = false;
	    return NULL;
	}
	return p->second;
    }

    // Anything else, we need the CombInst to give us
    // our Instance Object GObj hash
    CombInst *c = GetCombInst(ind);
    if (UNLIKELY(!c))
	return NULL;

    // Unlike the other cases, ohash is not necessarily
    // guaranteed to map to a GObj - so a failure here
    // does NOT imply an invalid path.
    p = d->gobjs.find(c->ohash);
    if (UNLIKELY(p == d->gobjs.end()))
	return NULL;

    return p->second;
}

// Private
void
DbiPath::split_path(std::vector<std::string> &objs, const char *str)
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

// Private
std::string
DbiPath::name_deescape(std::string &name)
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


CombInst::CombInst(DbiState *dbis, const char *p_name, const char *o_name, unsigned long long icnt, int i_op, matp_t i_mat)
{
    d = dbis;
    cname = std::string(p_name);
    oname = std::string(o_name);
    iname = std::string("");
    id = icnt;
    boolean_op = i_op;
    if (i_mat) {
	MAT_COPY(m, i_mat);
	non_default_matrix = true;
    } else {
	// Deliberately set an invalid matrix so we notice if logic doesn't
	// check non_default_matix
	MAT_ZERO(m);
    }

    // The instance name is supposed to refer to another object in the
    // database, but there isn't actually anything that guarantees a comb
    // instance reference name is actually a database object - accordingly, we
    // don't do any checking for that.  What we *do* need, however, is a name
    // for the instance that is unique even within the local comb tree - and
    // the object name itself is NOT guaranteed to be unique.  We thus use the
    // counting of the instances maintained by the tree walk to construct
    // unique instance reference strings for any name encountered multiple
    // times.
    if (icnt > 1) {
	// If we've got multiple instances of the same object in the tree,
	// hash the string labeling the instance and map it to the correct
	// parent comb so we can associate it with the tree contents
	struct bu_vls iname_c = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&iname_c, "%s@%llu", o_name, icnt - 1);
	iname = std::string(bu_vls_cstr(&iname_c));
	bu_vls_free(&iname_c);
    }


    // Calculate chash
    chash = bu_data_hash(cname.c_str(), strlen(cname.c_str())*sizeof(char));

    // Calculate ohash
    ohash = bu_data_hash(oname.c_str(), strlen(oname.c_str())*sizeof(char));

    // Have the necessary info - calculate Unique Comb Instance hash
    unsigned long long comb_uniq_hash = ohash;
    if (iname.length())
	comb_uniq_hash = bu_data_hash(iname.c_str(), strlen(iname.c_str())*sizeof(char));

    // Calculate the Instance Hash - ihash.  This incorporates awareness of
    // both the parent comb name and the comb_uniq_hash to make a hash value
    // that is unique both in the local comb context and the overall DbiState.
    // As a result ihash can be used in paths to look up CombInst instances
    // globally as well as GObj containers.
    std::vector<unsigned long long> lvec;
    lvec.push_back(chash);
    lvec.push_back(comb_uniq_hash);
    ihash = bu_data_hash(lvec.data(), lvec.size() * sizeof(unsigned long long));

    // Register with DbiState
    d->combinsts[ihash] = this;
}

CombInst::~CombInst()
{
    if (d)
	d->combinsts.erase(ihash);
}

db_op_t
CombInst::bool_op()
{
    if (boolean_op == OP_SUBTRACT)
	return DB_OP_SUBTRACT;
    if (boolean_op == OP_INTERSECT)
	return DB_OP_INTERSECT;
    return DB_OP_UNION;
}

void
CombInst::bbox(point_t *min, point_t *max)
{
    if (!min || !max)
	return;

    // If there is no GObj associated with the instance object name, this
    // particular instance can make no contribution to a bounding box.
    std::unordered_map<unsigned long long, GObj *>::iterator ocomb = d->gobjs.find(ohash);
    if (ocomb == d->gobjs.end())
	return;

    // Get the obj matrix
    point_t lbmin, lbmax;
    VSETALL(lbmin, INFINITY);
    VSETALL(lbmax, -INFINITY);
    ocomb->second->bbox(&lbmin, &lbmax);

    // If we have an instance matrix to apply, do so
    if (!non_default_matrix) {
	point_t tbmin, tbmax;
	MAT4X3PNT(tbmin, m, lbmin);
	VMOVE(lbmin, tbmin);
	MAT4X3PNT(tbmax, m, lbmax);
	VMOVE(lbmax, tbmax);
    }

    // Build the min/max bbox
    VMINMAX(*min, *max, lbmin);
    VMINMAX(*min, *max, lbmax);
}

static bool
cache_get_int(struct ged_draw_cache *dcache, int *oval, unsigned long long hash, const char *key)
{
    bool ret = true;
    const char *b = NULL;
    size_t bsize = cache_get(dcache, (void **)&b, hash, key);
    if (bsize == sizeof(*oval)) {
	memcpy(oval, b, sizeof(*oval));
	ret = false;
    }
    cache_done(dcache);
    return ret;
}

static void
cache_write_int(struct ged_draw_cache *dcache, unsigned long long hash, int *ovar, const char *key)
{
    std::stringstream s;
    s.write(reinterpret_cast<const char *>(ovar), sizeof(*ovar));
    cache_write(dcache, hash, key, s);
}


static bool
cache_get_uint(struct ged_draw_cache *dcache, unsigned int *oval, unsigned long long hash, const char *key)
{
    bool ret = true;
    const char *b = NULL;
    size_t bsize = cache_get(dcache, (void **)&b, hash, key);
    if (bsize == sizeof(*oval)) {
	memcpy(oval, b, sizeof(*oval));
	ret = false;
    }
    cache_done(dcache);
    return ret;
}

GObj::GObj(DbiState *dbis, struct directory *dp_i)
{
    if (!dbis || !dp_i)
	return;

    // Store the pointers locally with the GObj
    d = dbis;
    dp = dp_i;

    // Calculate the hash of the d_namep to use as a key
    hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));

    // Encode hierarchy info if this is a comb
    if (dp->d_flags & RT_DIR_COMB)
	GenCombInstances();

    // Check the dcache for needed values

    int attr_region_id = -1;
    bool need_region_id = cache_get_int(dbis->dcache, &attr_region_id, hash, CACHE_REGION_ID);

    int rflag = 0;
    bool need_region_flag = cache_get_int(dbis->dcache, &rflag, hash, CACHE_REGION_FLAG);

    int color_inherit = 0;
    bool need_color_inherit = cache_get_int(dbis->dcache, &color_inherit, hash, CACHE_INHERIT_FLAG);

    unsigned int cval = INT_MAX;
    bool need_cval = cache_get_uint(dbis->dcache, &cval, hash, CACHE_COLOR) ;
    if (!need_cval) {
	// Unpack the cache cval into a bu_color
	int r, g, b;
	r = cval & 0xFF;
	g = (cval >> 8) & 0xFF;
	b = (cval >> 16) & 0xFF;

	unsigned char lrgb[3];
	lrgb[0] = (unsigned char)r;
	lrgb[1] = (unsigned char)g;
	lrgb[2] = (unsigned char)b;

	bu_color_from_rgb_chars(&color, lrgb);

	color_set = true;
    }

    // If we have at least one case where we're going to need to crack the
    // attributes, do it now.
    struct db_i *dbip = d->gedp->dbip;
    if (need_region_id || need_region_flag || need_color_inherit || need_cval) {

	// Read the attributes from the database object
	struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
	db5_get_attributes(dbip, &c_avs, dp);

	// Region flag.
	if (need_region_flag) {
	    const char *region_flag_str = bu_avs_get(&c_avs, "region");
	    if (region_flag_str && (BU_STR_EQUAL(region_flag_str, "R") || BU_STR_EQUAL(region_flag_str, "1")))
		rflag = 1;
	    cache_write_int(dbis->dcache, hash, &rflag, CACHE_REGION_FLAG);
	}

	// Region id.  For drawing purposes this needs to be a number.
	if (need_region_id) {
	    const char *region_id_val = bu_avs_get(&c_avs, "region_id");
	    if (region_id_val)
		bu_opt_int(NULL, 1, &region_id_val, (void *)&attr_region_id);
	    cache_write_int(dbis->dcache, hash, &attr_region_id, CACHE_REGION_ID);
	}

	// Inherit flag
	if (need_color_inherit) {
	    color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
	    cache_write_int(dbis->dcache, hash, &color_inherit, CACHE_INHERIT_FLAG);
	}

	// Color
	//
	// (Note that the rt_material_head colors and a region_id may override
	// this, as might a parent comb with color and the inherit flag both set.)
	if (need_cval) {
	    const char *color_val = bu_avs_get(&c_avs, "color");
	    if (!color_val)
		color_val = bu_avs_get(&c_avs, "rgb");
	    if (color_val) {
		bu_opt_color(NULL, 1, &color_val, (void *)&color);
		color_set = true;

		// Serialize for cache
		int r, g, b;
		bu_color_to_rgb_ints(&color, &r, &g, &b);
		unsigned int colors = r + (g << 8) + (b << 16);

		// Writing an unsigned int, not an int, so we don't use the
		// cache_write_int func here
		std::stringstream s;
		s.write(reinterpret_cast<const char *>(&colors), sizeof(colors));
		cache_write(dbis->dcache, hash, CACHE_COLOR, s);
	    }
	}

	// Done with attributes
	bu_log("Had to load avs\n");
	bu_avs_free(&c_avs);

    }

    // If a region flag is set but a region_id is not, there is an implicit
    // assumption that the region_id is to be regarded as 0.  Not sure this
    // will always be true, but right now region table based coloring works
    // that way in existing BRL-CAD code (see the example m35.g model's
    // all.g/component/power.train/r75 for an instance of this)
    if (region_flag && attr_region_id == -1)
	attr_region_id = 0;

    // Attributes reading done - assign values to class members.  Color
    // is already handled, so we don't need to redo it here.
    if (attr_region_id != -1)
	region_id = attr_region_id;
    if (color_inherit)
	c_inherit = color_inherit;
    if (rflag != -2)
	region_flag = rflag;


    // The bounding box calculation is deferred if the info isn't present in
    // the cache, since finding it is potentially expensive.
    point_t bmin, bmax;
    bool have_bbox = false;
    const char *b = NULL;
    size_t bsize = cache_get(dbis->dcache, (void **)&b, hash, CACHE_OBJ_BOUNDS);
    if (bsize) {
	if (bsize != (sizeof(bmin) + sizeof(bmax))) {
	    bu_log("Incorrect data size found loading cached bounds data for %s!\n", dp->d_namep);
	} else {
	    memcpy(&bmin, b, sizeof(bmin));
	    b += sizeof(bmin);
	    memcpy(&bmax, b, sizeof(bmax));
	    //bu_log("cached: bmin: %f %f %f bbmax: %f %f %f\n", V3ARGS(bmin), V3ARGS(bmax));
	    have_bbox = true;
	}
    }
    cache_done(dbis->dcache);

    if (have_bbox) {
	VMOVE(bb_min, bmin);
	VMOVE(bb_max, bmax);
	bb_valid = true;
    } else {
	// If we don't have the info, just set bb defaults to invalid values.
	VSETALL(bb_min, INFINITY);
	VSETALL(bb_max, -INFINITY);
	bb_valid = false;
    }

    // Register with DbiState
    d->gobjs[hash] = this;
    d->dp2g[dp] = this;
}

GObj::~GObj()
{
    // Delete comb instances (if any) associated with this object
    for (size_t i = 0; i < cv.size(); i++)
	delete cv[i];

    // De-register with the DbiState
    if (d) {
	d->dp2g.erase(dp);
	d->gobjs.erase(hash);
    }
}

struct gobj_walk_data {
    GObj *gobj = NULL;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
};

static void
populate_leaf(void *client_data, const char *name, matp_t c_m, int op)
{
    struct gobj_walk_data *d = (struct gobj_walk_data *)client_data;
    struct db_i *dbip = d->gobj->d->gedp->dbip;
    RT_CHECK_DBI(dbip);

    std::unordered_map<unsigned long long, unsigned long long> &i_count = d->i_count;
    unsigned long long chash = bu_data_hash(name, strlen(name)*sizeof(char));
    i_count[chash] += 1;

    // Make the CombInst
    CombInst *c = new CombInst(d->gobj->d, d->gobj->dp->d_namep, name, i_count[chash], op, c_m);

    // Add CombInst to the parent GObj container
    d->gobj->cv.push_back(c);
}

static void
populate_walk_tree(union tree *tp, void *d, int p_op,
	void (*leaf_func)(void *, const char *, matp_t, int)
	)
{
    if (!tp)
	return;

    RT_CK_TREE(tp);

    int op = p_op;
    switch (tp->tr_op) {
	case OP_SUBTRACT:
	    op = OP_SUBTRACT;
	    break;
	case OP_INTERSECT:
	    op = OP_INTERSECT;
	    break;
    };


    switch (tp->tr_op) {
	case OP_SUBTRACT:
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
	    populate_walk_tree(tp->tr_b.tb_right, d, op, leaf_func);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    populate_walk_tree(tp->tr_b.tb_left, d, OP_UNION, leaf_func);
	    break;
	case OP_DB_LEAF:
	    (*leaf_func)(d, tp->tr_l.tl_name, tp->tr_l.tl_mat, op);
	    break;
	default:
	    bu_log("unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("GObj comb walk\n");
    }
}

void
GObj::GenCombInstances()
{
    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    struct rt_db_internal in;
    if (rt_db_get_internal(&in, dp, d->gedp->dbip, NULL, d->res) < 0)
	return;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
    if (!comb->tree)
	return;

    struct gobj_walk_data dw;
    dw.gobj = this;
    populate_walk_tree(comb->tree, (void *)&dw, OP_UNION, populate_leaf);

    rt_db_free_internal(&in);
}

void
GObj::bbox(point_t *min, point_t *max)
{
    if (!min || !max)
	return;

    if (cv.size()) {
	// TODO - support cached comb bounding boxes - will have to be done
	// carefully to handle invalidating properly when there are edits to
	// instances or objects in comb trees.

	// It's a comb - iterate over all the comb instances and incorporate
	// all their bounding boxes.
	for (size_t i = 0; i < cv.size(); i++) {
	    point_t lbmin, lbmax;
	    VSETALL(lbmin, INFINITY);
	    VSETALL(lbmax, -INFINITY);
	    cv[i]->bbox(&lbmin, &lbmax);
	    VMINMAX(*min, *max, lbmin);
	    VMINMAX(*min, *max, lbmax);
	}
	return;
    }

    // Not a comb - if we're cached, just report that value.
    if (bb_valid) {
	VMINMAX(*min, *max, bb_min);
	VMINMAX(*min, *max, bb_max);
	return;
    }

    // There is a second cache we can ask about this - the LoD cache may also
    // know what we need.
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && d->gedp->ged_lod) {
	unsigned long long key = bv_mesh_lod_key_get(d->gedp->ged_lod, dp->d_namep);
	if (key) {
	    struct bv_mesh_lod *lod = bv_mesh_lod_create(d->gedp->ged_lod, key);
	    if (lod) {
		vect_t bmin, bmax;
		VMOVE(bmin, lod->bmin);
		VMOVE(bmax, lod->bmax);

		// Update drawing cache with LoD cache values
		std::stringstream s;
		s.write(reinterpret_cast<const char *>(&bmin), sizeof(bmin));
		s.write(reinterpret_cast<const char *>(&bmax), sizeof(bmax));
		cache_write(d->dcache, hash, CACHE_OBJ_BOUNDS, s);

		VMOVE(bb_min, bmin);
		VMOVE(bb_max, bmax);
		bb_valid = true;

		VMINMAX(*min, *max, bb_min);
		VMINMAX(*min, *max, bb_max);
		return;
	    }
	}
    }

    // Not cached - need to ask librt.  This is the slow operation.
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    mat_t m;
    MAT_IDN(m);
    vect_t bmin, bmax;
    int bret = rt_bound_instance(&bmin, &bmax, dp, d->gedp->dbip, &ttol, &tol, &m, d->res);
    if (bret != -1) {
	// Update drawing cache (TODO - probably should tell LoD cache as well?)
	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&bmin), sizeof(bmin));
	s.write(reinterpret_cast<const char *>(&bmax), sizeof(bmax));
	cache_write(d->dcache, hash, CACHE_OBJ_BOUNDS, s);

	VMOVE(bb_min, bmin);
	VMOVE(bb_max, bmax);
	bb_valid = true;

	// Local calculation complete - process min and max if we found a box
	VMINMAX(*min, *max, bb_min);
	VMINMAX(*min, *max, bb_max);
    }
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
