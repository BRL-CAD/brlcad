/*                       S E L E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file select.cpp
 *
 * "Sets" in BRL-CAD have some interesting complications, particularly when it
 * comes to geometric scenes.  Graphically visible scene objects representing
 * .g geometry always correspond to one instance of one solid.  Frequently those
 * "raw geometry" scene objects will be grouped as children of a parent scene
 * object that corresponds to the requested "drawn" object. Those top level
 * groups are what is reported out by the who command, since it will be more
 * compact and informative to users than a detailed itemization of (potentially)
 * thousands of individual instance shape objects.  Those top level groups also
 * offer convenient subsets which alleviate the need for the drawing logic to
 * check all scene object paths for matches of a user specified path - if the
 * parent scene object isn't a partial match, none of the children need to be
 * checked.
 *
 * However, when defining selection sets, it gets more complex.  Building selection
 * sets graphically by interrogating the scene will build up sets of individual
 * shape objects.  If the correct sub-sets of those objects are selected, we can
 * conceptually regard a parent comb as being fully selected, if we so choose.
 * However, that parent comb may not have a directly corresponding scene object,
 * if it was not what was originally requested by the user (or generated as
 * consequence of the processing actions of the draw/erase routines.)  Nor would we
 * always want the "fully selected" parent comb to replace all of its children in
 * the set, since different types of actions may want/need the individual instance
 * solids rather than the parent combs.
 *
 * We will need "expand" and "collapse" operations for sets, the former of which
 * will take each entry in the set and replace it with all of its child instances.
 * In the event of multiple expansions producing duplicates, the duplicates will
 * be collapsed to a unique set.
 *
 * Likewise, the "collapse" operation will look at the children contained in the set,
 * and for cases where all children are present, those children will be replaced
 * with one reference to the parent.  This will be done working "up" the various
 * trees until either all potential collapses are missing one or more children, or
 * all entries collapse to top level objects.
 *
 * Nor do we want to always expand or collapse - in some cases we want exactly
 * what was selected.  For example, if we have the tree
 *
 * a
 *  b
 *   c
 *    d
 *     e
 *
 * and we want to select and operate on /a/b/c, we can't simply collapse (which
 * will result in /a) or expand (which will result in /a/b/c/d/e).  If /a/b/c/d/e
 * is part of a larger selection set and we want to replace it with /a/b/c, we will
 * need a "insert" operation that identifies and removes all subset paths of
 * /a/b/c prior to insertion of /a/b/c into the set, but does not alter any
 * other paths or process /a/b/c itself.
 *
 * As an exercise, we consider a potential use of sets - selecting an instance of
 * an object in a scene for editing operations.  There are a number of things we
 * will need to know:
 *
 * The solids associated with the selected instance itself.  If a solid was
 * selected we already know where to get the editing wireframe, but if comb was
 * selected from the tree, there's more work to do.  We have the advantage of
 * explicitly knowing the level at which the editing operations will take place
 * (a tree selection would correspond to the previously discussed "insert"
 * operation to the set), but we will need to generate a list of that comb's
 * child solids so we can create appropriate editing wireframe visual objects
 * to represent the comb in the scene.  In the event of multiple selections we
 * would need to construct the instance set from multiple sources.
 *
 * If we are performing graphical selection operations, we may be either refining
 * a set by selecting objects to be removed from it, or selecting objects not
 * already part of the set to add to it.  In either case we will be operating
 * at the individual instance level, and must provide ways to allow the user
 * to refine (perhaps via the tree view) their selection intent for editing
 * purposes.
 *
 * If we want to (say) set all non-active objects to be highly transparent when
 * an editing operation commences, we will also need to be able to construct the
 * set of all scene objects which are NOT active in the currently processed set.
 *
 * If we want to edit primitive parameters, rather than comb instances, we will
 * need to boil down a set of selected comb instances to one or a set of solid
 * names.  (Usually a set count > 1 won't work for primitive editing... in that
 * case we may want to just reject an attempt to solid edit...)  Given the solid,
 * we will then need to extract the set of all drawn instances of that solid from
 * the scene, so we can generate and update per-instance wireframes for all of
 * them to visually reflect the impact of the edit (MGED's inability to do this
 * is why solid editing is rejected if more than one instance of the object is
 * drawn in the scene - we don't want that limitation.)
 *
 */

#include "common.h"

#include <map>
#include <set>
#include <string>
#include <unordered_map>

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"
}

#include "bu/path.h"
#include "bu/str.h"
#include "ged/view/select.h"
#include "./ged_private.h"

static void
fp_path_split(std::vector<std::string> &objs, const char *str)
{
    std::string s(str);
    size_t pos = 0;
    if (s.c_str()[0] == '/')
	s.erase(0, 1);  //Remove leading slash
    while ((pos = s.find_first_of("/", 0)) != std::string::npos) {
	std::string ss = s.substr(0, pos);
	objs.push_back(ss);
	s.erase(0, pos + 1);
    }
    objs.push_back(s);
}

static bool
fp_path_top_match(std::vector<std::string> &top, std::vector<std::string> &candidate)
{
    for (size_t i = 0; i < top.size(); i++) {
	if (i == candidate.size())
	    return false;
	if (top[i] != candidate[i])
	    return false;
    }
    return true;
}


struct ged_selection_set_impl {
    std::map<std::string, struct ged_selection *> m;
};

struct ged_selection_sets_impl {
    std::map<std::string, struct ged_selection_set *> *s;
};


struct ged_selection_sets *
ged_selection_sets_create(struct ged *gedp)
{
    if (!gedp)
	return NULL;
    struct ged_selection_sets *s;
    BU_GET(s, struct ged_selection_sets);
    s->gedp = gedp;
    s->i = new ged_selection_sets_impl;
    s->i->s = new std::map<std::string, struct ged_selection_set *>;
    struct ged_selection_set *ds;
    BU_GET(ds, struct ged_selection_set);
    bu_vls_init(&ds->name);
    bu_vls_sprintf(&ds->name, "default");
    ds->gedp = gedp;
    ds->i = new ged_selection_set_impl;
    (*s->i->s)[std::string("default")] = ds;
    return s;
}

void
ged_selection_sets_destroy(struct ged_selection_sets *s)
{
    if (!s)
	return;
    // TODO - clean up set contents
    delete s->i->s;
    delete s->i;
    BU_PUT(s, struct ged_selection_sets);
}


int
ged_selection_set_cpy(struct ged_selection_set *to, struct ged_selection_set *from)
{
    if (!from || !to || from == to)
	return 0;

    ged_selection_set_clear(to);
    std::map<std::string, struct ged_selection *>::iterator it;
    int i = 0;
    for (it = from->i->m.begin(); it != from->i->m.end(); it++) {
	struct ged_selection *o;
	BU_GET(o, struct ged_selection);
	bu_vls_init(&o->path);
	bu_vls_sprintf(&o->path, "%s", bu_vls_cstr(&it->second->path));
	o->r_os = it->second->r_os;
	bu_ptbl_init(&o->sobjs, 8, "selection objs");
	for (size_t j = 0; j < BU_PTBL_LEN(&it->second->sobjs); j++) {
	    bu_ptbl_ins(&o->sobjs, BU_PTBL_GET(&it->second->sobjs, j));
	}
	to->i->m[it->first] = o;
	i++;
    }

    return i;
}

struct ged_selection_set *
ged_selection_set_create(const char *s_name, struct ged *gedp)
{
    struct ged_selection_set *ds;
    BU_GET(ds, struct ged_selection_set);
    ds->gedp = gedp;
    bu_vls_init(&ds->name);
    if (s_name)
	bu_vls_sprintf(&ds->name, "%s", s_name);
    ds->i = new ged_selection_set_impl;
    return ds;
}

void
ged_selection_set_destroy(struct ged_selection_set *s)
{
    if (!s)
	return;

    ged_selection_set_clear(s);

    // If we have a null or empty s_name, we're targeting the "default" set.  We don't
    // delete it, since there is always a default set
    if (BU_STR_EQUAL(bu_vls_cstr(&s->name), "default")) {
	return;
    }

    bu_vls_free(&s->name);
    delete s->i;
    BU_PUT(s, struct ged_selection_set);
}

struct ged_selection_set *
ged_selection_sets_get(struct ged_selection_sets *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return NULL;
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    std::string sname = (s_name || !strlen(s_name)) ? std::string("default") : std::string(s_name);
    s_it = s->i->s->find(sname);
    if (s_it == s->i->s->end()) {
	struct ged_selection_set *ds = ged_selection_set_create(s_name, s->gedp);
	(*s->i->s)[sname] = ds;
	s_it = s->i->s->find(sname);
    }
    return s_it->second;
}

void
ged_selection_sets_put(struct ged_selection_sets *s, const char *s_name)
{
    if (!s)
	return;
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    std::string sname = (!s_name || !strlen(s_name)) ? std::string("default") : std::string(s_name);
    s_it = s->i->s->find(sname);
    if (s_it == s->i->s->end())
	return;

    ged_selection_set_destroy(s_it->second);

    // If we have a null or empty s_name, we're targeting the "default" set.  We don't
    // delete it, since there is always a default set
    if (sname == std::string("default")) {
	return;
    }

    s->i->s->erase(s_it);
}

size_t
ged_selection_sets_lookup(struct bu_ptbl *sets, struct ged_selection_sets *s, const char *pattern)
{
    if (!sets|| !s || !pattern || !strlen(pattern))
	return 0;

    bu_ptbl_reset(sets);

    std::vector<struct ged_selection_set *> s_sets;
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    for (s_it = s->i->s->begin(); s_it != s->i->s->end(); s_it++) {
	if (!bu_path_match(pattern, s_it->first.c_str(), 0)) {
	    s_sets.push_back(s_it->second);
	}
    }

    for (size_t i = 0; i < s_sets.size(); i++) {
	bu_ptbl_ins(sets, (long *)s_sets[i]);
    }

    return s_sets.size();
}

int
ged_selection_find(struct ged_selection_set *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return 0;

    std::string key(s_name);
    if (s->i->m.find(key) != s->i->m.end())
	return 1;

    return 0;
}

size_t
ged_selection_lookup(struct bu_ptbl *matches, struct ged_selection_set *s, const char *s_name)
{
    if (!matches || !s || !s_name || !strlen(s_name))
	return 0;

    bu_ptbl_reset(matches);

    std::vector<std::string> s_top;
    fp_path_split(s_top, s_name);

    // We want to do a "db_fullpath" style check to see if any active entries are matches or
    // subsets of this path.  To allow for the possibility of selections that have no corresponding
    // database object (in particular, invalid comb entries we may want to select to edit) we
    // do like the erase command and operate on the strings themselves.
    std::vector<struct ged_selection *> s_matches;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	std::vector<std::string> s_c;
	fp_path_split(s_c, s_it->first.c_str());
	if (fp_path_top_match(s_top, s_c)) {
	    s_matches.push_back(s_it->second);
	}
    }

    for (size_t i = 0; i < s_matches.size(); i++) {
	bu_ptbl_ins(matches, (long *)s_matches[i]);
    }

    return s_matches.size();
}

size_t
ged_selection_lookup_fp(struct bu_ptbl *matches, struct ged_selection_set *s, struct db_full_path *fp)
{
    if (!matches || !s || !fp)
	return 0;
    char *fp_s = db_path_to_string(fp);
    size_t ret = ged_selection_lookup(matches, s, fp_s);
    bu_free(fp_s, "fp_s");
    return ret;
}


int
ged_selection_set_list(char ***keys, struct ged_selection_set *s)
{
    if (!keys || !s || !s->i->m.size())
	return 0;

    (*keys) = (char **)bu_calloc(s->i->m.size()+1, sizeof(char *), "name array");
    std::map<std::string, struct ged_selection *>::iterator s_it;
    int i = 0;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	(*keys)[i] = bu_strdup(s_it->first.c_str());
	i++;
    }
    return (int)s->i->m.size();
}

void
ged_selection_set_clear(struct ged_selection_set *s)
{
    if (!s)
	return;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	bu_vls_free(&s_it->second->path);
	BU_PUT(s_it->second, struct ged_selection);
    }
    s->i->m.clear();
}

struct ged_selection *
_selection_get(struct ged_selection_set *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return NULL;
    struct ged_selection *s_o;
    BU_GET(s_o, struct ged_selection);
    s_o->gedp = s->gedp;
    s_o->r_os = NULL;
    bu_ptbl_init(&s_o->sobjs, 8, "selection objs");
    bu_vls_init(&s_o->path);
    bu_vls_sprintf(&s_o->path, "%s", s_name);
    s->i->m[std::string(s_name)] = s_o;
    return s_o;
}

void
_selection_put(struct ged_selection_set *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    s_it = s->i->m.find(std::string(s_name));
    if (s_it == s->i->m.end())
	return;
    struct ged_selection *gs = s_it->second; 
    s->i->m.erase(s_it);
    bu_vls_free(&gs->path);
    BU_PUT(gs, struct ged_selection);
}

struct ged_selection *
ged_selection_insert(struct ged_selection_set *s, const char *s_path)
{
    if (!s || !s_path || !strlen(s_path))
	return NULL;

    // If s_path isn't a valid path, don't add it
    struct db_full_path ifp;
    db_full_path_init(&ifp);
    int spathret = db_string_to_path(&ifp, s->gedp->dbip, s_path);
    if (spathret < 0) {
	db_free_full_path(&ifp);
	return NULL;
    }
    db_free_full_path(&ifp);

    // Unlike the lookup, with the insert operation we need to check if this
    // path is equal to or below any already added paths in the set.  If so,
    // it's already active (either explicitly or implicitly) in the set and we
    // don't duplicate it.  So the incoming path is the child candidate and the
    // existing paths are the top candidates, switching the
    // ged_selection_lookup behavior.
    struct bu_vls tpath = BU_VLS_INIT_ZERO;
    std::vector<std::string> s_c;
    fp_path_split(s_c, s_path);

    struct ged_selection *match = NULL;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	std::vector<std::string> s_top;
	bu_vls_sprintf(&tpath, "%s", s_it->first.c_str());
	fp_path_split(s_top, bu_vls_cstr(&tpath));
	if (fp_path_top_match(s_top, s_c)) {
	    match = s_it->second;
	    break;
	}
    }

    if (match) {
	bu_vls_free(&tpath);
	return match;
    }

    // If no match found, we're adding the path.  We also need to remove any
    // paths that are below this path.
    std::set<std::string> to_remove;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	std::vector<std::string> s_cc;
	bu_vls_sprintf(&tpath, "%s", s_it->first.c_str());
	fp_path_split(s_cc, bu_vls_cstr(&tpath));
	if (fp_path_top_match(s_c, s_cc))
	    to_remove.insert(std::string(bu_vls_cstr(&tpath)));
    }
    std::set<std::string>::iterator r_it;
    for (r_it = to_remove.begin(); r_it != to_remove.end(); r_it++) {
	bu_vls_sprintf(&tpath, "%s", r_it->c_str());
	_selection_put(s, bu_vls_cstr(&tpath));
    }

    // Children cleared - add the new path
    match = _selection_get(s, s_path);

    bu_vls_free(&tpath);

    return match;
}

struct ged_selection *
ged_selection_insert_fp(struct ged_selection_set *s, struct db_full_path *fp)
{
    if (!s || !fp)
	return NULL;
    char *fp_s = db_path_to_string(fp);
    struct ged_selection *ss = ged_selection_insert(s, fp_s);
    bu_free(fp_s, "fp_s");
    return ss;
}

struct ged_selection *
ged_selection_insert_obj(struct ged_selection_set *s, struct bv_scene_obj *o)
{
    if (!s || !o)
	return NULL;
    struct ged_selection *ss = ged_selection_insert(s, bu_vls_cstr(&o->s_name));
    if (ss)
	bu_ptbl_ins(&ss->sobjs, (long *)o);
    return ss;
}


// The remove operation may require splitting of an existing path into its components,
// unless it happens to match exactly a selected path.  This basically looks like an
// expanding of the path which is a tops path of the erase candidate, the removal
// of every expanded path for which the erase path is a match or parent, and then the
// collapse of the remaining paths.  This new set is then the replacement for the
// original parent path.
void
ged_selection_remove(struct ged_selection_set *s, const char *s_path)
{
    if (!s || !s_path || !strlen(s_path))
	return;

    // See if the proposed erase path is a child (or equal to) any existing path
    std::vector<std::string> s_c;
    fp_path_split(s_c, s_path);

    struct ged_selection *match = NULL;
    std::string match_str;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	std::vector<std::string> s_top;
	fp_path_split(s_top, s_it->first.c_str());
	if (fp_path_top_match(s_top, s_c)) {
	    match = s_it->second;
	    match_str = s_it->first;
	    break;
	}
    }
    if (!match) {
	s->i->m.erase(std::string(s_path));
	return;
    }

    // If we have an exact match we're done
    if (BU_STR_EQUAL(s_path, bu_vls_cstr(&match->path))) {
	_selection_put(s, s_path);
	return;
    }

    // If not, s_path is a child of match.  The hard case - need to split and
    // reconstitute the remaining sub-paths of match as new selections

    // Create a temporary selection set
    struct ged_selection_set *tmp_s;
    BU_GET(tmp_s, struct ged_selection_set);
    tmp_s->gedp = s->gedp;
    tmp_s->i = new ged_selection_set_impl;

    // Move the match from the original set to the temporary set
    s->i->m.erase(match_str);
    tmp_s->i->m[(std::string(s_path))] = match;

    ged_selection_set_expand(tmp_s, tmp_s);

    // Remove everything in the expanded temp set where s_c is a
    // top path of the current path
    std::set<std::string> rm_tmp;
    for (s_it = tmp_s->i->m.begin(); s_it != tmp_s->i->m.end(); s_it++) {
	std::vector<std::string> s_cc;
	fp_path_split(s_cc, s_it->first.c_str());
	if (fp_path_top_match(s_c, s_cc)) {
	    rm_tmp.insert(s_it->first.c_str());
	}
    }
    std::set<std::string>::iterator tmp_it;
    for (tmp_it = rm_tmp.begin(); tmp_it != rm_tmp.end(); tmp_it++) {
	_selection_put(tmp_s, (*tmp_it).c_str());
    }

    // Collapse everything that's left into the new selection paths
    ged_selection_set_collapse(tmp_s, tmp_s);

    // Move the tmp_s selections into the original set
    for (s_it = tmp_s->i->m.begin(); s_it != tmp_s->i->m.end(); s_it++) {
	s->i->m[s_it->first] = s_it->second;
    }
    tmp_s->i->m.clear();

    // Delete temporary selection set
    delete tmp_s->i;
    BU_PUT(tmp_s, struct ged_selection_set);
}

void
ged_selection_remove_fp(struct ged_selection_set *s, struct db_full_path *fp)
{
    if (!s || !fp)
	return;
    char *fp_s = db_path_to_string(fp);
    ged_selection_remove(s, fp_s);
    bu_free(fp_s, "fp_s");
}

void
ged_selection_remove_obj(struct ged_selection_set *s, struct bv_scene_obj *o)
{
    if (!s || !o)
	return;
    ged_selection_remove(s, bu_vls_cstr(&o->s_name));
}



/* Search client data container */
struct expand_client_data_t {
    struct db_i *dbip;
    struct bu_ptbl *full_paths;
};

/**
 * A generic traversal function maintaining awareness of the full path
 * to a given object.
 */
static void
expand_subtree(struct db_full_path *path, int curr_bool, union tree *tp,
	void (*traverse_func) (struct db_full_path *path, void *),
	void *cmap, void *client_data)
{
    struct directory *dp;
    struct expand_client_data_t *ecd= (struct expand_client_data_t *)client_data;
    std::unordered_map<std::string, int> *c_inst_map = (std::unordered_map<std::string, int> *)cmap;

    if (!tp)
	return;

    RT_CK_FULL_PATH(path);
    RT_CHECK_DBI(ecd->dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    expand_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, cmap, client_data);
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    expand_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, cmap, client_data);
	    expand_subtree(path, tp->tr_op, tp->tr_b.tb_right, traverse_func, cmap, client_data);
	    break;
	case OP_DB_LEAF:
	    if (UNLIKELY(ecd->dbip->dbi_use_comb_instance_ids && c_inst_map))
		(*c_inst_map)[std::string(tp->tr_l.tl_name)]++;
	    if ((dp=db_lookup(ecd->dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {
		/* Create the new path */
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    db_add_node_to_full_path(path, dp);
		    DB_FULL_PATH_SET_CUR_BOOL(path, curr_bool);
		    if (UNLIKELY(ecd->dbip->dbi_use_comb_instance_ids && c_inst_map))
			DB_FULL_PATH_SET_CUR_COMB_INST(path, (*c_inst_map)[std::string(tp->tr_l.tl_name)]-1);
		    if (!db_full_path_cyclic(path, NULL, 0)) {
			/* Keep going */
			traverse_func(path, client_data);
		    } else {
			char *path_string = db_path_to_string(path);
			bu_log("WARNING: not traversing cyclic path %s\n", path_string);
			bu_free(path_string, "free path str");
		    }
		}
		DB_FULL_PATH_POP(path);
		break;
	    }
	    break;
	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}

static void
expand_list(struct db_full_path *path, void *client_data)
{
    struct directory *dp;
    struct expand_client_data_t *ecd= (struct expand_client_data_t *)client_data;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(ecd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, ecd->dbip, NULL, &rt_uniresource) < 0)
	    return;

	std::unordered_map<std::string, int> c_inst_map;
	comb = (struct rt_comb_internal *)in.idb_ptr;
	expand_subtree(path, OP_UNION, comb->tree, expand_list, &c_inst_map, client_data);
	rt_db_free_internal(&in);
    } else {
	struct db_full_path *newpath;
	BU_ALLOC(newpath, struct db_full_path);
	db_full_path_init(newpath);
	db_dup_full_path(newpath, path);
	bu_ptbl_ins(ecd->full_paths, (long *)newpath);
    }
}


int
ged_selection_set_expand(struct ged_selection_set *s_out, struct ged_selection_set *s)
{
    if (!s_out || !s)
	return BRLCAD_ERROR;

    std::queue<struct ged_selection *> initial;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	initial.push(s_it->second);
    }
    if (s_out != s) {
	ged_selection_set_clear(s_out);
    }

    while (!initial.empty()) {
	struct ged_selection *ss = initial.front();
	initial.pop();

	struct db_full_path dfp;
	db_full_path_init(&dfp);
	if (db_string_to_path(&dfp, s->gedp->dbip, bu_vls_cstr(&ss->path))) {
	    // If we can't get a valid dbip, there's nothing from the database
	    // to expand this to - just keep it and move on
	    ged_selection_insert(s_out, bu_vls_cstr(&ss->path));
	    db_free_full_path(&dfp);
	    continue;
	}

	struct expand_client_data_t ecd;
	struct bu_ptbl *solid_paths;
	BU_ALLOC(solid_paths, struct bu_ptbl);
	BU_PTBL_INIT(solid_paths);
	ecd.dbip = s->gedp->dbip;
	ecd.full_paths = solid_paths;

	// Get the full paths of all solid stances below dfp
	expand_list(&dfp, (void *)&ecd);

	db_free_full_path(&dfp);

	if (BU_PTBL_LEN(solid_paths)) {
	    bool keep_orig = false;
	    for (size_t i = 0; i < BU_PTBL_LEN(solid_paths); i++) {
		struct db_full_path *sfp = (struct db_full_path *)BU_PTBL_GET(solid_paths, i);
		char *s_path = db_path_to_string(sfp);
		_selection_get(s_out, s_path);
		if (BU_STR_EQUAL(s_path, bu_vls_cstr(&ss->path)))
		    keep_orig = true;
	    }
	    // Expanded - remove original
	    if (!keep_orig)
		_selection_put(s_out, bu_vls_cstr(&ss->path));
	} else {
	    // No expansion - keep initial
	    if (s_out != s) {
		ged_selection_insert(s_out, bu_vls_cstr(&ss->path));
	    }
	}
	bu_ptbl_free(solid_paths);
	BU_FREE(solid_paths, struct bu_ptbl);
    }

    return BRLCAD_OK;
}

void
child_name_set_tree(union tree *tp, struct db_i *dbip, void *cmap, void *client_data)
{
    std::unordered_map<std::string, int> *cinst_map = (std::unordered_map<std::string, int> *)cmap;
    std::set<std::string> *g_c = (std::set<std::string> *)client_data;
    struct bu_vls iname = BU_VLS_INIT_ZERO; 

    if (!tp)
	return;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    child_name_set_tree(tp->tr_b.tb_left, dbip, cmap, client_data);
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    child_name_set_tree(tp->tr_b.tb_left, dbip, cmap, client_data);
	    child_name_set_tree(tp->tr_b.tb_right, dbip, cmap, client_data);
	    break;
	case OP_DB_LEAF:
	    if (UNLIKELY(dbip->dbi_use_comb_instance_ids && cinst_map)) {
		(*cinst_map)[std::string(tp->tr_l.tl_name)]++;
		if ((*cinst_map)[std::string(tp->tr_l.tl_name)] > 1) {
		    bu_vls_printf(&iname, "%s@%d", tp->tr_l.tl_name, (*cinst_map)[std::string(tp->tr_l.tl_name)] - 1);
		} else {
		    bu_vls_printf(&iname, "%s", tp->tr_l.tl_name);
		}
	    } else {
		bu_vls_printf(&iname, "%s", tp->tr_l.tl_name);
	    }
	    g_c->insert(std::string(bu_vls_cstr(&iname)));
	    bu_vls_free(&iname);
	    return;
	default:
	    bu_log("child_name_set_tree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("child_name_set_tree: unrecognized operator\n");
    }
}

void
child_name_set(std::set<std::string> *g_c, struct ged *gedp, std::string &dp_name)
{
    if (!g_c || !gedp || !dp_name.size())
	return;

    struct directory *dp = db_lookup(gedp->dbip, dp_name.c_str(), LOOKUP_QUIET);
    if (!dp)
	return;

    struct rt_db_internal in;
    if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0)
	return;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
    if (UNLIKELY(gedp->dbip->dbi_use_comb_instance_ids)) {
	std::unordered_map<std::string, int> cinst_map;
	child_name_set_tree(comb->tree, gedp->dbip, (void *)&cinst_map, (void *)g_c);
    } else {
	child_name_set_tree(comb->tree, gedp->dbip, NULL, (void *)g_c);
    }
}


struct vstr_cmp {
    bool operator() (std::vector<std::string> fp, std::vector<std::string> o) const {
	// First, check length
	if (fp.size() != o.size())
	    return (fp.size() > o.size());

	// If length is the same, check the dp contents
	for (size_t i = 0; i < fp.size(); i++) {
	    if (fp[i] != o[i])
		return (bu_strcmp(fp[i].c_str(), o[i].c_str()) > 0);
	}
	return (bu_strcmp(fp[fp.size()-1].c_str(), o[fp.size()-1].c_str()) > 0);
    }
};


int
ged_selection_set_collapse(struct ged_selection_set *s_out, struct ged_selection_set *s)
{
    if (!s_out || !s)
	return BRLCAD_ERROR;

    std::set<std::vector<std::string>> final_paths;
    std::set<std::vector<std::string>, vstr_cmp> s1, s2;
    std::set<std::vector<std::string>, vstr_cmp> *ps, *pnext, *tmp;
    ps = &s1;
    pnext = &s2;

    // Seed the set with the current paths
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	std::vector<std::string> s_p;
	fp_path_split(s_p, s_it->first.c_str());
	ps->insert(s_p);
    }
    ged_selection_set_clear(s_out);

    size_t longest = (ps->size()) ? ((*ps->begin()).size()) : 0;
    std::vector<std::string> cparent;
    std::set<std::string> found_children;
    while (ps->size() || pnext->size() || found_children.size()) {
	std::vector<std::string> s_p;
	bool empty = false;
	if (ps->size()) {
	    s_p = *ps->begin();
	    ps->erase(ps->begin());
	    std::cout << "Process: ";
	    for(size_t i = 0; i < s_p.size(); i++)
		std::cout << "/" << s_p[i];
	    std::cout << "\n";
	} else {
	    empty = true;
	}
	if (empty || s_p.size() < longest) {
	    // Because the set was ordered by path length, if we've hit a
	    // shorter path that means we've processed all the paths with the
	    // correct length - now it's time to find out if we have a fully
	    // realized parent.  If we do, we insert cparent int pnext.  If
	    // not, we put all the found_children of cparent into final_paths.
	    std::cout << "End check\n";
	    std::set<std::string> g_c;
	    child_name_set(&g_c, s->gedp, cparent[cparent.size()-1]);
	    if (g_c == found_children) {
		// Have all the children - collapse.  cparent may be part of
		// a higher level collapse, so we insert into pnext for further
		// processing
		pnext->insert(cparent);
	    } else {
		// Not all children present - cparent + found_children constitute
		// final paths
		std::set<std::string>::iterator f_it;
		for (f_it = found_children.begin(); f_it != found_children.end(); f_it++) {
		    cparent.push_back(*f_it);
		    final_paths.insert(cparent);
		    cparent.pop_back();
		}
	    }

	    // Having figured out the answer for the previous case, move
	    // remaining paths to pnext, swap ps and pnext, and continue
	    if (!empty) {
		pnext->insert(s_p);
		while (ps->size()) {
		    s_p = *ps->begin();
		    ps->erase(ps->begin());
		    pnext->insert(s_p);
		}
	    }
	    tmp = ps;
	    ps = pnext;
	    pnext = tmp;
	    longest = (ps->size()) ? ((*ps->begin()).size()) : 0;
	    cparent.clear();
	    found_children.clear();
	    continue;
	}

	if (s_p.size() == 1) {
	    // Already as collapsed as it can get
	    final_paths.insert(s_p);
	    continue;
	}

	if (!cparent.size()) {
	    // Starting a new comb check - initialize parent
	    cparent = s_p;
	    std::cout << "Starting new: ";
	    for(size_t i = 0; i < cparent.size(); i++)
		std::cout << "/" << cparent[i];
	    std::cout << "\n";

	    found_children.insert(cparent[cparent.size()-1]);
	    cparent.pop_back();
	    continue;
	}

	if (s_p[s_p.size()-2] != cparent[cparent.size()-1]) {
	    // Right length, but wrong parent - defer
	    std::cout << "Defer: ";
	    for(size_t i = 0; i < s_p.size(); i++)
		std::cout << "/" << s_p[i];
	    std::cout << "\n";

	    pnext->insert(s_p);
	    continue;
	} else {
	    std::cout << "Found child\n";
	    found_children.insert(s_p[s_p.size()-1]);
	    continue;
	}
    }

    // Have final path set, add it to s_out
    std::set<std::vector<std::string>>::iterator fp_it;
    for (fp_it = final_paths.begin(); fp_it != final_paths.end(); fp_it++) {
	const std::vector<std::string> &v = *fp_it;
	std::string fpath;
	for (size_t i = 0; i < v.size();  i++) {
	    fpath.append("/");
	    fpath.append(v[i]);
	}
	ged_selection_insert(s_out, fpath.c_str());
    }

    return BRLCAD_OK;
}


void
ged_selection_assign_objs(struct ged_selection_set *s)
{
    if (!s || !s->i->m.size())
	return;

    // If we don't have view objects, there's nothing to associate
    struct bu_ptbl *sg = bv_view_objs(s->gedp->ged_gvp, BV_DB_OBJS);
    if (!sg) {
	std::map<std::string, struct ged_selection *>::iterator s_it;
	for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	    struct ged_selection *ss = s_it->second;
	    bu_ptbl_reset(&ss->sobjs);
	}
	return;
    }

    // populate the paths map with scene objects
    std::map<struct directory *, std::set<struct db_full_path *>> so_paths;
    std::map<struct db_full_path *, struct bv_scene_obj *> path_to_obj;
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(sg, i);
	struct db_full_path *dfp;
	BU_GET(dfp, struct db_full_path);
	db_full_path_init(dfp);
	if (db_string_to_path(dfp, s->gedp->dbip, bu_vls_cstr(&so->s_name))) {
	    db_free_full_path(dfp);
	    BU_PUT(dfp, struct db_full_path);
	    continue;
	}
	so_paths[DB_FULL_PATH_GET(dfp, 0)].insert(dfp);
	path_to_obj[dfp] = so;
    }

    // For the selections, add any scene objects with paths below the selection
    // path to that selections sobjs ptbl
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	struct ged_selection *ss = s_it->second;
	struct db_full_path *sel_fp;
	BU_GET(sel_fp, struct db_full_path);
	db_full_path_init(sel_fp);
	// If the path isn't valid, there's nothing to add
	if (db_string_to_path(sel_fp, s->gedp->dbip, bu_vls_cstr(&ss->path))) {
	    db_free_full_path(sel_fp);
	    BU_PUT(sel_fp, struct db_full_path);
	    continue;
	}
	// If no paths match the first pointer, there's no point checking further
	if (so_paths.find(DB_FULL_PATH_GET(sel_fp, 0)) == so_paths.end()) {
	    db_free_full_path(sel_fp);
	    BU_PUT(sel_fp, struct db_full_path);
	    continue;
	}
	// Pull the set of paths that are a first pointer match, and check them
	// to see which ones are top matches for this selection path.  If they
	// are a match, add to sobjs.  This can probably be made more efficient
	// - we filter on first pointer matching to try and help cut down the
	// unnecessary checks, but there are probably better ways.
	std::set<struct db_full_path *> &dpaths = so_paths[DB_FULL_PATH_GET(sel_fp, 0)];
	std::set<struct db_full_path *>::iterator d_it;
	for (d_it = dpaths.begin(); d_it != dpaths.end(); d_it++) {
	    struct db_full_path *so_fp = *d_it;
	    if (db_full_path_match_top(sel_fp, so_fp)) {
		struct bv_scene_obj *sso = path_to_obj[sel_fp];
		if (!sso)
		    continue;
		bu_ptbl_ins(&ss->sobjs, (long *)sso);
	    }
	}
	db_free_full_path(sel_fp);
	BU_PUT(sel_fp, struct db_full_path);
    }

    std::map<struct directory *, std::set<struct db_full_path *>>::iterator sp_it;
    for (sp_it = so_paths.begin(); sp_it != so_paths.end(); sp_it++) {
	std::set<struct db_full_path *>::iterator d_it;
	for (d_it = sp_it->second.begin(); d_it != sp_it->second.end(); d_it++) {
	    struct db_full_path *dfp = *d_it;
	    db_free_full_path(dfp);
	    BU_PUT(dfp, struct db_full_path);
	}
    }
}

static void
_ill_toggle(struct bv_scene_obj *s, char ill_state)
{
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	_ill_toggle(s_c, ill_state);
    }
    s->s_iflag = ill_state;
}


void
ged_selection_toggle_illum(struct ged_selection_set *s, char ill_state)
{
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	struct ged_selection *ss = s_it->second;
	for (size_t i = 0; i < BU_PTBL_LEN(&ss->sobjs); i++) {
	    struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(&ss->sobjs, i);
	    _ill_toggle(so, ill_state);
	}
    }
}

unsigned long long
ged_selection_hash_set(struct ged_selection_set *s)
{
    if (!s)
	return 0;

    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	XXH64_update(&h_state, bu_vls_cstr(&s_it->second->path), bu_vls_strlen(&s_it->second->path)*sizeof(char));
    }
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    return hash;
}

unsigned long long
ged_selection_hash_sets(struct ged_selection_sets *ss)
{
    if (!ss)
	return 0;


    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    struct bu_vls sname = BU_VLS_INIT_ZERO;   

    std::map<std::string, struct ged_selection_set *>::iterator ss_it;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    for (ss_it = ss->i->s->begin(); ss_it != ss->i->s->end(); ss_it++) {
	bu_vls_sprintf(&sname, "%s", ss_it->first.c_str());
	XXH64_update(&h_state, bu_vls_cstr(&sname), bu_vls_strlen(&sname)*sizeof(char));
	struct ged_selection_set *s = ss_it->second;
	for (s_it = s->i->m.begin(); s_it != s->i->m.end(); s_it++) {
	    XXH64_update(&h_state, bu_vls_cstr(&s_it->second->path), bu_vls_strlen(&s_it->second->path)*sizeof(char));
	}
    }
    bu_vls_free(&sname);

    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    return hash;
}


struct rt_object_selections *
ged_get_object_selections(struct ged *gedp, const char *object_name)
{
    struct ged_selection_set *ss = (*gedp->ged_selection_sets->i->s)[std::string("default")];
    struct ged_selection *s = _selection_get(ss, object_name);
    if (!s->r_os) {
	struct rt_object_selections *ro_s;
	BU_GET(ro_s, struct rt_object_selections);
	s->r_os = ro_s;
    }
    return s->r_os;
}


struct rt_selection_set *
ged_get_selection_set(struct ged *gedp, const char *object_name, const char *selection_name)
{
    struct rt_object_selections *o_s = ged_get_object_selections(gedp, object_name);
    struct rt_selection_set *set = (struct rt_selection_set *)bu_hash_get(o_s->sets, (uint8_t *)selection_name, strlen(selection_name));
    if (!set) {
        BU_ALLOC(set, struct rt_selection_set);
        BU_PTBL_INIT(&set->selections);
        bu_hash_set(o_s->sets, (uint8_t *)selection_name, strlen(selection_name), (void *)set);
    }

    return set;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

