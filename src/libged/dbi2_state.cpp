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


CombInst::CombInst(const char *p_name, const char *o_name, unsigned long long icnt, int i_op, matp_t i_mat)
{
    cname = std::string(p_name);
    oname = std::string(o_name);
    iname = std::string("");
    id = icnt;
    bool_op = i_op;
    if (i_mat) {
	MAT_COPY(m, i_mat);
    } else {
	MAT_IDN(m);
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

    // Calculate ohash
    ohash = bu_data_hash(oname.c_str(), strlen(oname.c_str())*sizeof(char));

    // Have the necessary info - calculate Comb Instance hash
    if (iname.length()) {
	ihash = bu_data_hash(iname.c_str(), strlen(iname.c_str())*sizeof(char));
    } else {
	ihash = ohash;
    }
}


void
CombInst::bbox(vect_t *min, vect_t *max)
{
    if (!min || !max)
	return;
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

GObj::GObj(struct db_i *dbip_i, struct directory *dp_i, struct ged_draw_cache *dcache)
{
    if (!dbip_i || !dp_i || !dcache)
	return;

    // Store the database pointers locally with the GObj
    dbip = dbip_i;
    dp = dp_i;

    // Calculate the hash of the d_namep to use as a key
    hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));

    // Encode hierarchy info if this is a comb
    if (dp->d_flags & RT_DIR_COMB)
	GenCombInstances();

    // Check the dcache for needed values

    int attr_region_id = -1;
    bool need_region_id = cache_get_int(dcache, &attr_region_id, hash, CACHE_REGION_ID);

    int rflag = 0;
    bool need_region_flag = cache_get_int(dcache, &rflag, hash, CACHE_REGION_FLAG);

    int color_inherit = 0;
    bool need_color_inherit = cache_get_int(dcache, &color_inherit, hash, CACHE_INHERIT_FLAG);

    unsigned int cval = INT_MAX;
    bool need_cval = cache_get_uint(dcache, &cval, hash, CACHE_COLOR) ;

    // If we have at least one case where we're going to need to crack the
    // attributes, do it now.
    if (need_region_id || need_region_flag || need_color_inherit || need_cval) {

	// Read the attributes from the database object
	struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
	db5_get_attributes(dbip, &c_avs, dp);

	// Region flag.
	if (need_region_flag) {
	    const char *region_flag_str = bu_avs_get(&c_avs, "region");
	    if (region_flag_str && (BU_STR_EQUAL(region_flag_str, "R") || BU_STR_EQUAL(region_flag_str, "1")))
		rflag = 1;
	    cache_write_int(dcache, hash, &rflag, CACHE_REGION_FLAG);
	}

	// Region id.  For drawing purposes this needs to be a number.
	if (need_region_id) {
	    const char *region_id_val = bu_avs_get(&c_avs, "region_id");
	    if (region_id_val)
		bu_opt_int(NULL, 1, &region_id_val, (void *)&attr_region_id);
	    cache_write_int(dcache, hash, &attr_region_id, CACHE_REGION_ID);
	}

	// Inherit flag
	if (need_color_inherit) {
	    color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;
	    cache_write_int(dcache, hash, &color_inherit, CACHE_INHERIT_FLAG);
	}

	// Color
	//
	// (Note that the rt_material_head colors and a region_id may override
	// this, as might a parent comb with color and the inherit flag both set.)
	if (need_cval) {
	    struct bu_color mc = BU_COLOR_INIT_ZERO;
	    const char *color_val = bu_avs_get(&c_avs, "color");
	    if (!color_val)
		color_val = bu_avs_get(&c_avs, "rgb");
	    if (color_val){
		bu_opt_color(NULL, 1, &color_val, (void *)&mc);
		int cr, cg, cb;
		bu_color_to_rgb_ints(&mc, &cr, &cg, &cb);
		cval = cr + (cg << 8) + (cb << 16);
		bu_log("have color: %u\n", cval);
	    }

	    // cval is an unsigned int, not an int
	    std::stringstream s;
	    s.write(reinterpret_cast<const char *>(&cval), sizeof(cval));
	    cache_write(dcache, hash, CACHE_COLOR, s);
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

    // Reading done - assign values to class members
    if (attr_region_id != -1)
	region_id = attr_region_id;
    if (color_inherit)
	c_inherit = color_inherit;
    if (cval != INT_MAX)
	rgb = cval;
    if (rflag != -2)
	region_flag = rflag;

}

GObj::~GObj()
{
    // Delete comb instances (if any) associated with this object
    for (size_t i = 0; i < cv.size(); i++)
	delete cv[i];
}

struct gobj_walk_data {
    GObj *gobj = NULL;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
};

static void
populate_leaf(void *client_data, const char *name, matp_t c_m, int op)
{
    struct gobj_walk_data *d = (struct gobj_walk_data *)client_data;
    struct db_i *dbip = d->gobj->dbip;
    RT_CHECK_DBI(dbip);

    std::unordered_map<unsigned long long, unsigned long long> &i_count = d->i_count;
    unsigned long long chash = bu_data_hash(name, strlen(name)*sizeof(char));
    i_count[chash] += 1;

    // Make the CombInst
    CombInst *c = new CombInst(d->gobj->dp->d_namep, name, i_count[chash], op, c_m);

    // Add CombInst to the parent GObj containers
    d->gobj->c[c->ihash] = c;
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
    if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0)
	return;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
    if (!comb->tree)
	return;

    struct gobj_walk_data d;
    d.gobj = this;
    populate_walk_tree(comb->tree, (void *)&d, OP_UNION, populate_leaf);

    rt_db_free_internal(&in);
} 

void
GObj::bbox(vect_t *min, vect_t *max)
{
    if (!min || !max)
	return;
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
