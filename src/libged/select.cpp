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
#include <string>

#include "bu/path.h"
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



struct ged_selection *
_selection_get(struct ged_selection_set *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return NULL;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    s_it = s->i->m.find(std::string(s_name));
    if (s_it == s->i->m.end()) {
	struct ged_selection *s_o;
	BU_GET(s_o, struct ged_selection);
	s_o->gedp = s->gedp;
	s_o->r_os = NULL;
	s_o->so = NULL;
	bu_vls_init(&s_o->path);
	bu_vls_sprintf(&s_o->path, "%s", s_name);
	s->i->m[std::string(s_name)] = s_o;
	s_it = s->i->m.find(std::string(s_name));
    }
    return s_it->second;
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
    bu_vls_init(&s_it->second->path);
    BU_PUT(s_it->second, struct ged_selection);
    s->i->m.erase(std::string(s_name));
}


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



struct ged_selection_set *
ged_selection_sets_get(struct ged_selection_sets *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return NULL;
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    std::string sname = (s_name || !strlen(s_name)) ? std::string("default") : std::string(s_name);
    s_it = s->i->s->find(sname);
    if (s_it == s->i->s->end()) {
	struct ged_selection_set *ds;
	BU_GET(ds, struct ged_selection_set);
	ds->gedp = s->gedp;
	bu_vls_init(&ds->name);
	bu_vls_sprintf(&ds->name, "%s", s_name);
	ds->i = new ged_selection_set_impl;
	(*s->i->s)[sname] = ds;
	s_it = s->i->s->find(sname);
    }
    return s_it->second;
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

void
ged_selection_sets_put(struct ged_selection_sets *s, const char *s_name)
{
    if (!s)
	return;
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    std::string sname = (s_name || !strlen(s_name)) ? std::string("default") : std::string(s_name);
    s_it = s->i->s->find(sname);
    if (s_it == s->i->s->end())
	return;

    ged_selection_set_clear(s_it->second);

    // If we have a null or empty s_name, we're targeting the "default" set.  We don't
    // delete it, since there is always a default set
    if (sname == std::string("default")) {
	return;
    }

    bu_vls_free(&s_it->second->name);
    delete s_it->second->i;
    BU_GET(s_it->second, struct ged_selection_set);
    s->i->s->erase(s_it);
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
	BU_PUT(s_it->second, struct ged_selection);
    }
    s->i->m.clear();
}

int
ged_selection_set_cpy(struct ged_selection_sets *s, const char *from, const char *to)
{
    if (!s || from == to)
	return 0;

    struct ged_selection_set *s_from = ged_selection_sets_get(s, from);
    struct ged_selection_set *s_to = ged_selection_sets_get(s, to);
    if (s_from == s_to) {
	return 0;
    }
    ged_selection_set_clear(s_to);
    std::map<std::string, struct ged_selection *>::iterator s_it;
    int i = 0;
    for (s_it = s_from->i->m.begin(); s_it != s_from->i->m.end(); s_it++) {
	struct ged_selection *s_o;
	BU_GET(s_o, struct ged_selection);
	bu_vls_init(&s_o->path);
	bu_vls_sprintf(&s_o->path, "%s", bu_vls_cstr(&s_it->second->path));
	s_o->r_os = s_it->second->r_os;
	s_o->so = s_it->second->so;
	s_to->i->m[s_it->first] = s_o;
	i++;
    }

    return i;
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

