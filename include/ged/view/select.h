/*                       S E L E C T . H
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
/** @addtogroup ged_select
 *
 * Geometry EDiting Library Object Selection Functions.
 *
 */
/** @{ */
/** @file ged/view/select.h */

#ifndef GED_VIEW_SELECT_H
#define GED_VIEW_SELECT_H

#include "common.h"
#include "bv/defines.h"
#include "ged/defines.h"

__BEGIN_DECLS

struct ged_selection {
    struct ged *gedp;
    struct bu_vls path;
    struct bu_ptbl sobjs; // struct bv_scene_obj pointers
    struct rt_object_selections *r_os;
};

struct ged_selection_set_impl;
struct ged_selection_set {
    struct ged *gedp;
    struct bu_vls name;
    struct ged_selection_set_impl *i;
};

struct ged_selection_sets_impl;
struct ged_selection_sets {
    struct ged *gedp;
    struct ged_selection_sets_impl *i;
};

// Routines to manage a set of selection sets
GED_EXPORT extern struct ged_selection_sets *ged_selection_sets_create(struct ged *gedp);
GED_EXPORT extern void ged_selection_sets_destroy(struct ged_selection_sets *s);
GED_EXPORT int ged_selection_set_cpy(struct ged_selection_sets *s, const char *from, const char *to);

// Routines to retrieve and remove individual selection sets
GED_EXPORT struct ged_selection_set *ged_selection_sets_get(struct ged_selection_sets *s, const char *s_path);
GED_EXPORT void ged_selection_sets_put(struct ged_selection_sets *s, const char *s_path);
GED_EXPORT size_t ged_selection_sets_lookup(struct bu_ptbl *sets, struct ged_selection_sets *s, const char *pattern);

// Retrieve data.
GED_EXPORT int ged_selection_find(struct ged_selection_set *s, const char *s_name);
GED_EXPORT size_t ged_selection_lookup(struct bu_ptbl *matches, struct ged_selection_set *s, const char *s_path);
GED_EXPORT size_t ged_selection_lookup_fp(struct bu_ptbl *matches, struct ged_selection_set *s, struct db_full_path *fp);
GED_EXPORT int ged_selection_set_list(char ***keys, struct ged_selection_set *s);
GED_EXPORT void ged_selection_set_clear(struct ged_selection_set *s);

// Manipulate data
GED_EXPORT struct ged_selection *ged_selection_insert(struct ged_selection_set *s, const char *s_path);
GED_EXPORT struct ged_selection *ged_selection_insert_fp(struct ged_selection_set *s, struct db_full_path *fp);
GED_EXPORT struct ged_selection *ged_selection_insert_obj(struct ged_selection_set *s, struct bv_scene_obj *o);
GED_EXPORT void ged_selection_remove(struct ged_selection_set *s, const char *s_path);
GED_EXPORT void ged_selection_remove_fp(struct ged_selection_set *s, struct db_full_path *fp);
GED_EXPORT void ged_selection_remove_obj(struct ged_selection_set *s, struct bv_scene_obj *o);
GED_EXPORT int ged_selection_set_expand(struct ged_selection_set *s_out, struct ged_selection_set *s);
GED_EXPORT int ged_selection_set_collapse(struct ged_selection_set *s_out, struct ged_selection_set *s);

// Given a set, associate the DBOBJ scene objects with any matching selection objects.
GED_EXPORT void ged_selection_assign_objs(struct ged_selection_set *s);
GED_EXPORT void ged_selection_toggle_illum(struct ged_selection_set *s, char ill_state);

/**
 * Returns a list of items within the previously defined rectangle.
 */
GED_EXPORT extern int ged_rselect(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns a list of items within the specified rectangle or circle.
 */
GED_EXPORT extern int ged_select(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return ged selections for specified object. Created if it doesn't
 * exist.
 */
GED_EXPORT struct rt_object_selections *ged_get_object_selections(struct ged *gedp,
								  const char *object_name);

/**
 * Return ged selections of specified kind for specified object.
 * Created if it doesn't exist.
 */
GED_EXPORT struct rt_selection_set *ged_get_selection_set(struct ged *gedp,
							  const char *object_name,
							  const char *selection_name);




__END_DECLS

#endif /* GED_VIEW_SELECT_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
