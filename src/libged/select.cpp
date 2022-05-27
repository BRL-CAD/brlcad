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

struct _selection {
    struct rt_object_selections *ro_s;
};

struct selection_set {
    std::map<std::string, struct _selection *> m;
};

struct ged_selection_sets_impl {
    std::map<std::string, struct selection_set *> *s;
};


struct ged_selection_sets *
ged_selection_sets_create()
{
    struct ged_selection_sets *s;
    BU_GET(s, struct ged_selection_sets);
    s->i = new ged_selection_sets_impl;
    s->i->s = new std::map<std::string, struct selection_set *>;
    (*s->i->s)[std::string("default")] = new selection_set;
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


struct rt_object_selections *
ged_get_object_selections(struct ged *gedp, const char *object_name)
{
    struct selection_set *ss = (*gedp->ged_selection_sets->i->s)[std::string("default")];
    if (ss->m.find(std::string(object_name)) == ss->m.end()) {
	ss->m[std::string(object_name)] = new _selection;
    }
    struct _selection *s = ss->m[std::string(object_name)];
    if (!s->ro_s) {
	struct rt_object_selections *ro_s;
	BU_GET(ro_s, struct rt_object_selections);
	s->ro_s = ro_s;
    }

    return s->ro_s;
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

