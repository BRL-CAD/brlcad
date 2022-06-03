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


//////////////////////////////////////////////////////////////////////////

void
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

bool
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

#if 0
/* Add and remove operations on sets of instance paths.  These are primarily for drawing
 * code but may also be useful when manipulating selection sets. */
int
ged_fp_str_set_add(struct bu_ptbl *fps, const char *npath, struct db_i *dbip, int solids_flag)
{
}

int
ged_fp_str_set_rm(struct bu_ptbl *fps, const char *npath, struct db_i *dbip, int consolidate_flag)
{
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

