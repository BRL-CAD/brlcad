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
 *
 */

#include "common.h"

#include <map>
#include <string>

#include "ged/view/select.h"
#include "./ged_private.h"

struct ged_selection_set_impl {
    std::map<std::string, struct ged_selection *> m;
};

struct ged_selection_sets_impl {
    std::map<std::string, struct ged_selection_set *> *s;
};


struct ged_selection_sets *
ged_selection_sets_create()
{
    struct ged_selection_sets *s;
    BU_GET(s, struct ged_selection_sets);
    s->i = new ged_selection_sets_impl;
    s->i->s = new std::map<std::string, struct ged_selection_set *>;
    struct ged_selection_set *ds;
    BU_GET(ds, struct ged_selection_set);
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
    if (!s)
	return NULL;
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    std::string sname = (s_name || !strlen(s_name)) ? std::string("default") : std::string(s_name);
    s_it = s->i->s->find(sname);
    if (s_it == s->i->s->end()) {
	struct ged_selection_set *ds;
	BU_GET(ds, struct ged_selection_set);
	ds->i = new ged_selection_set_impl;
	(*s->i->s)[sname] = ds;
	s_it = s->i->s->find(sname);
    }
    return s_it->second;
}

struct ged_selection_set *
ged_selection_sets_lookup(struct ged_selection_sets *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return NULL;
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    s_it = s->i->s->find(std::string(s_name));
    if (s_it == s->i->s->end())
	return NULL;
    return s_it->second;
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

    delete s_it->second->i;
    BU_GET(s_it->second, struct ged_selection_set);
    s->i->s->erase(s_it);
}

int
ged_selection_sets_list(char ***set_names, struct ged_selection_sets *s)
{
    if (!set_names || !s)
	return 0;

    (*set_names) = (char **)bu_calloc(s->i->s->size()+1, sizeof(char *), "name array");
    std::map<std::string, struct ged_selection_set *>::iterator s_it;
    int i = 0;
    for (s_it = s->i->s->begin(); s_it != s->i->s->end(); s_it++) {
	(*set_names)[i] = bu_strdup(s_it->first.c_str());
	i++;
    }
    return (int)s->i->s->size();
}


struct ged_selection *
ged_selection_set_get(struct ged_selection_set *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return NULL;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    s_it = s->i->m.find(std::string(s_name));
    if (s_it == s->i->m.end()) {
	struct ged_selection *s_o;
	BU_GET(s_o, struct ged_selection);
	s_o->r_os = NULL;
	s_o->so = NULL;
	s->i->m[std::string(s_name)] = s_o;
	s_it = s->i->m.find(std::string(s_name));
    }
    return s_it->second;
}

struct ged_selection *
ged_selection_set_lookup(struct ged_selection_set *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return NULL;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    s_it = s->i->m.find(std::string(s_name));
    if (s_it == s->i->m.end())
	return NULL;
    return s_it->second;
}

void
ged_selection_set_put(struct ged_selection_set *s, const char *s_name)
{
    if (!s || !s_name || !strlen(s_name))
	return;
    std::map<std::string, struct ged_selection *>::iterator s_it;
    s_it = s->i->m.find(std::string(s_name));
    if (s_it == s->i->m.end())
	return;
    BU_PUT(s_it->second, struct ged_selection);
    s->i->m.erase(std::string(s_name));
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
    struct ged_selection *s = ged_selection_set_get(ss, object_name);
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

