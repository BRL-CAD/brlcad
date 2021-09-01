/*                       D B _ F P . C P P
 * BRL-CAD
 *
 * Copyright (c) 1990-2021 United States Government as represented by
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
/** @file librt/db_fp.cpp
 *
 * Routines to manipulate geometry database hierarchical path description
 * structures.
 */

#include "common.h"

#include <vector>
#include <set>
#include <stack>
#include <map>
#include <utility>

#include <limits.h>
#include <math.h>
#include <string.h>
#include "bio.h"

extern "C" {
#include "vmath.h"
#include "bn/mat.h"
#include "rt/db_fp.h"
#include "raytrace.h"
}

/* Working node definition */
struct db_fp_node {
    struct directory *dp;
    struct directory *parent;
    db_op_t op;
    matp_t m;
    int uses;
    /* Using a stack here so calling code can locally take advantage of this
     * feature without out having to worry about manually caching and restoring
     * any pre-existing pointers to avoid disrupting higher level code. */
    std::stack<void *> data_stack;
};

// Use the name strings of the parent object (which may be null) and the
// current object as keys for a multimap to manage overall node tracking.  This
// paring is not necessarily unique - matrix and/or boolean operation can cause
// non-uniqueness, and the code working with db_fp paths and nodes will need to
// be aware of that - but the multimap allows us to handle the (hopefully
// relatively infrequent) collisions of that nature while limiting the
// complexity of our look-up key.  Paths that are unique only by virtue of
// operation or matrix tend to cause problems for other code routines, so we're
// structuring this container with that in mind - using the PImpl information
// hiding code pattern lets us revisit this design later if it does not prove
// to be a practical decision since it is strictly an internal implementation
// detail.
typedef std::pair<const char *, const char *> CInst;    // Comb object instance
typedef std::multimap<CInst, struct db_fp_node *> IMap; // Comb instance to db_fp_node map

struct db_fp_pool_impl {
    std::set<struct db_fp_node *> nodes;
    IMap imap;
    int uses;
};

struct db_fp_impl {
    std::vector<struct db_fp_node *> nodes;
    struct db_fp_pool *p;
    int shared_pool;
    std::stack<void *> data_stack;
};

int
db_fp_pool_init(struct db_fp_pool *p)
{
    if (!p) return -1;
    p->i = new struct db_fp_pool_impl;
    return 0;
}

int
db_fp_pool_create(struct db_fp_pool **p)
{
    if (!p) return -1;
    BU_GET(*p, struct db_fp_pool);
    return db_fp_pool_init(*p);
}

void
db_fp_pool_clear(struct db_fp_pool *p)
{
    if (!p || !p->i) return;
    std::set<struct db_fp_node *>::iterator n_it;
    for (n_it = p->i->nodes.begin(); n_it != p->i->nodes.end(); n_it++) {
	struct db_fp_node *n = *n_it;
	if (n->m) {
	    bu_free(n->m, "node matrix");
	}
	delete n;
    }
    delete p->i;
}

void
db_fp_pool_destroy(struct db_fp_pool *p)
{
    if (!p) return;
    db_fp_pool_clear(p);
    BU_PUT(p, struct db_fp_pool);
}

void
db_fp_node_map_remove(struct db_fp_pool *p, struct db_fp_node *n)
{
    IMap::iterator to_remove = p->i->imap.end();
    IMap::iterator p_it;
    std::pair<IMap::iterator, IMap::iterator> range;
    CInst mkey(n->parent->d_namep, n->dp->d_namep);
    range = p->i->imap.equal_range(mkey);
    for (p_it = range.first; p_it != range.second; p_it++) {
	struct db_fp_node *m = (*p_it).second;
	if (m == n) {
	    to_remove = p_it;
	}
    }
    if (to_remove != p->i->imap.end()) {
	p->i->imap.erase(to_remove);
    }
}

void
db_fp_pool_gc(struct db_fp_pool *p)
{
    if (!p || !p->i) return;
    std::set<struct db_fp_node *> to_erase;
    std::set<struct db_fp_node *>::iterator n_it;
    for (n_it = p->i->nodes.begin(); n_it != p->i->nodes.end(); n_it++) {
	struct db_fp_node *n = *n_it;
	if (!n->uses) {
	    to_erase.insert(n);
	    db_fp_node_map_remove(p, n);
	    if (n->m) {
		bu_free(n->m, "node matrix");
	    }
	    BU_PUT(n, struct db_fp_node);
	}
    }
    for (n_it = to_erase.begin(); n_it != to_erase.end(); n_it++) {
	p->i->nodes.erase(*n_it);
    }
}

struct db_fp *
db_fp_create(struct db_fp_pool *p)
{
    struct db_fp *n;
    BU_GET(n, struct db_fp);
    n->i = new struct db_fp_impl;
    if (p) {
	n->i->shared_pool = 1;
	n->i->p = p;
    } else {
	n->i->shared_pool = 0;
	db_fp_pool_create(&n->i->p);
    }
    n->i->p->i->uses++;
    return n;
}

void
db_fp_destroy(struct db_fp *fp)
{
    if (!fp) return;
    if (!fp->i->shared_pool) {
	db_fp_pool_destroy(fp->i->p);
    } else {
	fp->i->p->i->uses--;
    }
    delete fp->i;
    BU_PUT(fp, struct db_fp);
}

size_t
db_fp_len(struct db_fp *fp)
{
    if (!fp) return 0;
    return fp->i->nodes.size();
}

struct db_fp_node *
_db_fp_new_node(struct directory *parent, struct directory *dp, db_op_t op, mat_t mat)
{
    struct db_fp_node *nn = new struct db_fp_node;
    nn->dp = dp;
    nn->op = op;
    nn->parent = parent;
    nn->m = (mat) ? bn_mat_dup(mat) : NULL;
    nn->uses = 1;
    return nn;
}

struct db_fp_node *
_db_fp_find_node(struct db_fp_pool_impl *p, const char *pname, const char *oname, db_op_t op, mat_t mat)
{
    IMap::iterator p_it;
    std::pair<IMap::iterator, IMap::iterator> range;
    CInst mkey(pname, oname);
    range = p->imap.equal_range(mkey);

    for (p_it = range.first; p_it != range.second; p_it++) {
	/* If we found something, check against the bool (and mat if we have
	 * it - otherwise identity matrix is assumed. */
	struct db_fp_node *n = (*p_it).second;
	if (n->op != op) continue;
	if ((mat && !n->m) || (!mat && n->m)) continue;
	if (mat && n->m) {
	    /* TODO - what's an appropriate tolerance here? */
	    struct bn_tol btol = BG_TOL_INIT;
	    if (!bn_mat_is_equal(mat, n->m, &btol)) continue;
	}
	/* Made it - we have a match! */
	return n;
    }

    return NULL;
}


int
db_fp_cyclic(const struct db_fp *fp, const char *name)
{
    long int depth;
    const char *test_name;
    if (!fp || !name || !fp->i->nodes.size()) return 0;

    depth = (long int)fp->i->nodes.size() - 1;

    if (name[0] != '\0') {
	test_name = name;
    } else {
	test_name = fp->i->nodes.back()->dp->d_namep;
    }

    while (--depth >= 0) {
	if (BU_STR_EQUAL(test_name, fp->i->nodes.at(depth)->dp->d_namep)) {
	    return 1;
	}
    }

    return 0;
}


int
db_fp_append(struct db_fp *fp, struct directory *dp, db_op_t op, mat_t mat)
{
    struct directory *parent = RT_DIR_NULL;
    const char *pname = NULL;
    const char *oname = dp->d_namep;
    if (!fp || !dp) return -1;
    if (fp->i->nodes.size()) {
	parent = fp->i->nodes.back()->dp;
	pname = parent->d_namep;
    }
    struct db_fp_node *existing_node = _db_fp_find_node(fp->i->p->i, pname, oname, op, mat);

    if (existing_node) {
	if (!db_fp_cyclic(fp, existing_node->dp->d_namep)) {
	    existing_node->uses++;
	    fp->i->nodes.push_back(existing_node);
	    return 0;
	} else { 
	    return 1;
	}
    } else {
	/* New node */
	if (!db_fp_cyclic(fp, oname)) {
	    CInst ni(pname, oname);
	    struct db_fp_node *nn = _db_fp_new_node(parent, dp, op, mat);
	    fp->i->nodes.push_back(nn);
	    fp->i->p->i->nodes.insert(nn);
	    fp->i->p->i->imap.insert(std::pair<CInst, struct db_fp_node *>(ni, nn));
	    return 0;
	} else {
	    return 1;
	}
    }
}

int
db_fp_set(struct db_fp *fp, int i, struct directory *dp, db_op_t op, mat_t mat)
{
    const char *pname = NULL;
    const char *oname = dp->d_namep;
    if (!fp || !dp) return -1;
    if ((size_t)i > fp->i->nodes.size() - 1) return 1;
    if (!fp->i->nodes.size()) return db_fp_append(fp, dp, op, mat);

    pname = fp->i->nodes.at(i)->parent->d_namep;

    struct db_fp_node *existing_node = _db_fp_find_node(fp->i->p->i, pname, oname, op, mat);

    if (existing_node) {
	if (!db_fp_cyclic(fp, existing_node->dp->d_namep)) {
	    existing_node->uses++;
	    fp->i->nodes[i] = existing_node;
	    return 0;
	} else {
	    return 2;
	}
    } else {
	/* New node */
	if (!db_fp_cyclic(fp, dp->d_namep)) {
	    CInst ni(pname, oname);
	    struct db_fp_node *nn = _db_fp_new_node(fp->i->nodes.at(i)->parent, dp, op, mat);
	    fp->i->nodes[i] = nn;
	    fp->i->p->i->nodes.insert(nn);
	    fp->i->p->i->imap.insert(std::pair<CInst, struct db_fp_node *>(ni, nn));
	    return 0;
	} else {
	    return 2;
	}
    }
}


void
db_fp_cpy(struct db_fp *dest, struct db_fp *src)
{
    std::vector<struct db_fp_node *>::iterator v_it;
    if (!dest || !src) return;

    /* First, let any nodes in the old dest path know their use count has decreased */
    for (v_it = dest->i->nodes.begin(); v_it != dest->i->nodes.end(); v_it++) {
	(*v_it)->uses--;
    }

    /* Clear the old nodes */
    dest->i->nodes.clear();

    if (dest->i->p != src->i->p) {
	// Pools don't match, need to do a deep copy
	for (v_it = src->i->nodes.begin(); v_it != src->i->nodes.end(); v_it++) {
	    struct db_fp_node *n = *v_it;
	    db_fp_append(dest, n->dp, n->op, n->m);
	    dest->i->nodes.back()->data_stack = n->data_stack;
	}
    } else {
	/* Copy the src nodes to dest */
	dest->i->nodes = src->i->nodes; 
    }
}

void
db_fp_pop(struct db_fp *fp)
{
    if (!fp) return;
    fp->i->nodes.back()->uses--;
    fp->i->nodes.pop_back();
}

void
db_fp_push(struct db_fp *dest, struct db_fp *src, size_t start_offset)
{
    size_t pos = 0;
    std::vector<struct db_fp_node *>::iterator v_it;
    if (!dest || !src) return;

    for (v_it = src->i->nodes.begin(); v_it != src->i->nodes.end(); v_it++) {
	struct db_fp_node *n = *v_it;
	if (pos >= start_offset) {
	    if (pos == start_offset) {
		/* joining node - need to find or create node with correct
		 * parent, not use this node (which will be a root version) */
		db_fp_append(dest, n->dp, n->op, n->m);
	    } else {
		n->uses++;
		dest->i->nodes.push_back(n);
	    }
	}
	pos++;
    }
}

struct directory *
db_fp_get_dir(const struct db_fp *fp, int i)
{
    if (!fp || i < -1 || (i > 0 && (size_t)i >= fp->i->nodes.size())) return NULL;
    if (i == -1) return fp->i->nodes.back()->dp;
    return fp->i->nodes[i]->dp;
}

db_op_t
db_fp_get_bool(const struct db_fp *fp, int i)
{
    if (!fp || i < -1 || (i > 0 && (size_t)i >= fp->i->nodes.size())) return DB_OP_NULL;
    if (i == -1) return fp->i->nodes.back()->op;
    return fp->i->nodes[i]->op;
}

matp_t
db_fp_get_mat(const struct db_fp *fp, int i)
{
    if (!fp || i < -1 || (i > 0 && (size_t)i >= fp->i->nodes.size())) return NULL;
    if (i == -1) return fp->i->nodes.back()->m;
    return fp->i->nodes[i]->m;
}

void
db_fp_push_data(struct db_fp *fp, void *data)
{
    if (!fp || !data) return;
    fp->i->data_stack.push(data);
}

void *
db_fp_get_data(struct db_fp *fp)
{
    if (!fp) return NULL;
    return fp->i->data_stack.top();
}

void
db_fp_pop_data(struct db_fp *fp)
{
    if (!fp) return;
    fp->i->data_stack.pop();
}


void
db_fp_push_node_data(struct db_fp *fp, int index, void *data)
{
    if (!fp || !data) return;
    if (index < -1 || (index > 0 && (size_t)index >= fp->i->nodes.size())) return;
    if (index == -1) {
	fp->i->nodes.back()->data_stack.push(data);
    } else {
	fp->i->nodes[index]->data_stack.push(data);
    }
}

void *
db_fp_get_node_data(struct db_fp *fp, int index)
{
    if (!fp) return NULL;
    if (index < -1 || (index > 0 && (size_t)index >= fp->i->nodes.size())) return NULL;
    if (index == -1) {
	return fp->i->nodes.back()->data_stack.top();
    } else {
	return fp->i->nodes[index]->data_stack.top();
    }
}

void
db_fp_pop_node_data(struct db_fp *fp, int index)
{
    if (!fp) return;
    if (index < -1 || (index > 0 && (size_t)index >= fp->i->nodes.size())) return;
    if (index == -1) {
	fp->i->nodes.back()->data_stack.pop();
    } else {
	fp->i->nodes[index]->data_stack.pop();
    }
}

void
db_fp_to_vls(struct bu_vls *v, const struct db_fp *fp, int flags)
{
    if (!v || !fp) return;
    if (flags) return;
    for(size_t i = 0; i < fp->i->nodes.size() - 1; i++) {
	bu_vls_printf(v, "%s/", fp->i->nodes.at(i)->dp->d_namep);
    }
    bu_vls_printf(v, "%s", fp->i->nodes.back()->dp->d_namep);
}


char *
db_fp_to_str(const struct db_fp *fp, int flags)
{
    char *outstr = NULL;
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    if (!fp) return NULL;
    db_fp_to_vls(&tmpstr, fp, flags);
    if (bu_vls_strlen(&tmpstr)) {
	outstr = bu_strdup(bu_vls_addr(&tmpstr));
    }
    bu_vls_free(&tmpstr);
    return outstr;
}

int
db_argv_to_fp(struct db_fp **fp, const struct db_i *dbip, int argc, const char *const*argv, struct db_fp_pool *p)
{
    if (!fp || !dbip || !argv || !argc) return -1;

    struct db_fp *nfp = db_fp_create(p);

    for (int i = 0; i < argc; i++) {
	// Decode argv entry
	struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_NOISY);
	if (dp == RT_DIR_NULL) {
	    db_fp_destroy(nfp);
	    return -1;
	}
	// append to fp
	db_fp_append(nfp, dp, DB_OP_UNION, NULL);
    }

    (*fp) = nfp;
    return 0;
}

int
db_str_to_fp(struct db_fp **fp, const struct db_i *dbip, const char *str, struct db_fp_pool *p)
{
    int ret = 0;
    if (!fp || !dbip || !str) return -1;

    /* Perform initial split */
    std::vector<std::string> substrs;
    std::string lstr = std::string(str);
    size_t pos = 0;
    while ((pos = lstr.find("/", 0)) != std::string::npos) {
	std::string ss = lstr.substr(0, pos);
	substrs.push_back(ss);
	lstr.erase(0, pos + 1);
    }
    substrs.push_back(lstr);

    /* Step this down into a C argc/argv array so we don't get any surprises */
    int ac = (int)substrs.size();
    const char **av = (const char **)bu_calloc(ac+1, sizeof(const char *), "av array");
    for (int i = 0; i < ac; i++) {
	av[i] = bu_strdup(substrs.at(i).c_str());
    }

    ret = db_argv_to_fp(fp, dbip, ac, (const char *const*)av, p);

    /* clean up */
    for (int i = 0; i < ac; i++) {
	bu_free((char *)av[i], "free str");
    }
    bu_free(av, "free av array");

    return ret;
}

int
db_fp_identical(const struct db_fp *a, const struct db_fp *b)
{
    if (!a && !b) return 1;
    if ((a && !b) || (b && !a)) return 0;
    if (a->i->nodes.size() != b->i->nodes.size()) return 0;
    for (size_t i = 0; i < a->i->nodes.size(); i++) {
	struct db_fp_node *d1 = a->i->nodes.at(i);
	struct db_fp_node *d2 = b->i->nodes.at(i);
	if (!BU_STR_EQUAL(d1->dp->d_namep, d2->dp->d_namep)) return 0;
	if (d1->op != d2->op) return 0;
	if ((d1->m && !d2->m) || (!d1->m && d2->m)) return 0;
	if (d1->m && d2->m) {
	    /* TODO - what's an appropriate tolerance here? */
	    struct bn_tol btol = BG_TOL_INIT;
	    if (!bn_mat_is_equal(d1->m, d2->m, &btol)) return 0;
	}
    }
    return 1;
}

/* Mimic the logic from db_apply_state_from_memb */
void
db_fp_eval_mat(mat_t mat, const struct db_fp *fp, unsigned int depth)
{
    mat_t xmat, old_xlate, ts_mat;
    MAT_IDN(xmat);
    MAT_IDN(old_xlate);
    MAT_IDN(ts_mat);
    for (size_t i = 0; i < fp->i->nodes.size(); i++) {
	struct db_fp_node *n = fp->i->nodes.at(i);
	if (i > depth) break;
	MAT_COPY(old_xlate, ts_mat);
	if (n->m) {
	    MAT_COPY(xmat, n->m);
	} else {
	    MAT_IDN(xmat);
	}
	bn_mat_mul(ts_mat, old_xlate, xmat);
    }
    MAT_COPY(mat, ts_mat);
}


int
db_fp_identical_solid(const struct db_fp *a, const struct db_fp *b)
{
    if (!a && !b) return 0;
    if ((a && !b) || (b && !a)) return 0;
    if (a->i->nodes.size() != b->i->nodes.size()) return 0;
    if (db_fp_identical(a,b)) return 1;

    /* Are the leaf solids the same? If not, we're done */
    struct directory *aleaf = db_fp_get_dir(a, -1);
    struct directory *bleaf = db_fp_get_dir(b, -1);
    if (!BU_STR_EQUAL(aleaf->d_namep, bleaf->d_namep)) return 0;

    /* If we have anything but unions in the path at this stage, we're done */
    for (size_t i = 0; i < a->i->nodes.size(); i++) {
	if (a->i->nodes.at(i)->op != DB_OP_UNION) return 0;
    }
    for (size_t i = 0; i < b->i->nodes.size(); i++) {
	if (b->i->nodes.at(i)->op != DB_OP_UNION) return 0;
    }

    /* Now, see if the matrices along the path evaluate to the same
     * final matrix */
    mat_t amat, bmat;
    db_fp_eval_mat(amat, a, 0);
    db_fp_eval_mat(bmat, a, 0);
    /* TODO - what's an appropriate tolerance here? */
    struct bn_tol btol = BG_TOL_INIT;
    if (!bn_mat_is_equal(amat, bmat, &btol)) return 0;

    return 1;
}

int
_db_fp_node_equal(struct db_fp_node *an, struct db_fp_node *bn)
{
    if (!BU_STR_EQUAL(an->dp->d_namep, bn->dp->d_namep)) return 0;
    if (!BU_STR_EQUAL(an->parent->d_namep, bn->parent->d_namep)) return 0;
    if (an->op != bn->op) return 0;
    if ((an->m && !bn->m) || (!an->m && bn->m)) return 0;
    if (an->m && bn->m) {
	/* TODO - what's an appropriate tolerance here? */
	struct bn_tol btol = BG_TOL_INIT;
	if (!bn_mat_is_equal(an->m, bn->m, &btol)) return 0;
    }
    return 1;
}

int
db_fp_match(const struct db_fp *a, const struct db_fp *b, unsigned int skip_first, int leaf_to_root)
{
    if (!a && !b) return -1;
    if ((a && !b) || (b && !a)) return -1;
    size_t alen = a->i->nodes.size();
    size_t blen = a->i->nodes.size();
    if (alen <= (blen + skip_first)) return -1;

    if (leaf_to_root) {
	struct db_fp_node *bleaf = b->i->nodes.back();
	for (size_t i = alen - 1 - skip_first; i > (blen + skip_first); i--) {
	    struct db_fp_node *tnode = a->i->nodes.at(i);
	    if (_db_fp_node_equal(tnode, bleaf)) {
		int have_match = 1;
		int k = i - 1;
		for (long j = blen - 2; j >= 0; j--) {
		    struct db_fp_node *anode = a->i->nodes.at(k);
		    struct db_fp_node *bnode = b->i->nodes.at(j);
		    if (!_db_fp_node_equal(anode, bnode)) {
			have_match = 0;
			break;
		    }
		    k--;
		}
		if (have_match) return i;
	    }
	}
    } else {
	struct db_fp_node *broot = b->i->nodes.at(0);
	for (size_t i = skip_first; i < alen - (blen + skip_first); i++) {
	    struct db_fp_node *tnode = a->i->nodes.at(i);
	    if (_db_fp_node_equal(tnode, broot)) {
		int have_match = 1;
		int k = i + 1;
		for (size_t j = 1; j < blen; j++) {
		    struct db_fp_node *anode = a->i->nodes.at(k);
		    struct db_fp_node *bnode = b->i->nodes.at(j);
		    if (!_db_fp_node_equal(anode, bnode)) {
			have_match = 0;
			break;
		    }
		    k++;
		}
		if (have_match) return i;
	    }

	}
    }

    return -1;
}

int
db_fp_search(const struct db_fp *p, const char *n, unsigned int skip_first, int leaf_to_root)
{
    if (!p || !n) return -1;
    if (skip_first >= p->i->nodes.size()) return -1;

    if (leaf_to_root) {
	for (long i = p->i->nodes.size() - 1; i >= 0; i--) {
	    struct db_fp_node *cn = p->i->nodes.at(i);
	    if (BU_STR_EQUAL(cn->dp->d_namep, n)) return i;
	}

    } else {
	for (size_t i = 0; i < p->i->nodes.size(); i++) {
	    struct db_fp_node *cn = p->i->nodes.at(i);
	    if (BU_STR_EQUAL(cn->dp->d_namep, n)) return i;
	}
    }

    return -1;
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

