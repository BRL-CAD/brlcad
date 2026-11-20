/* M I M E _ O U T P U T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef GCV_MIME_OUTPUT_H
#define GCV_MIME_OUTPUT_H
#include "common.h"
#include <locale.h>
#include "raytrace.h"
#include "gcv/api.h"

static inline int
gcv_output_ascii_text(const char *text)
{
    if (!text) return 0;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
	if (*at > 0x7f) return 0;
    return 1;
}

static inline union tree *
gcv_output_profile_leaf(struct db_tree_state *UNUSED(state), const struct db_full_path *UNUSED(path),
    struct rt_db_internal *UNUSED(object), void *UNUSED(data))
{
    union tree *empty;
    BU_GET(empty, union tree);
    RT_TREE_INIT(empty);
    empty->tr_op = OP_NOP;
    return empty;
}

static inline union tree *
gcv_output_ascii_region(struct db_tree_state *UNUSED(state), const struct db_full_path *path,
    union tree *current_tree, void *data)
{
    int *valid = (int *)data;
    for (size_t i = 0; *valid && i < path->fp_len; ++i)
	if (!gcv_output_ascii_text(path->fp_names[i]->d_namep)) *valid = 0;
    return current_tree;
}

/* These writers emit selected region paths and use printf's numeric
 * locale. Placeholder leaves check the same paths without tessellation,
 * before any output or companion files are opened. */
static inline int
gcv_output_ascii_paths(struct gcv_context *context, const struct gcv_opts *options)
{
    const struct lconv *numeric = localeconv();
    int valid = numeric && gcv_output_ascii_text(numeric->decimal_point);
    if (!valid) return 0;
    struct db_tree_state state;
    RT_DBTS_INIT(&state);
    state.ts_tol = &options->calculational_tolerance;
    state.ts_ttol = &options->tessellation_tolerance;
    return db_walk_tree(context->dbip, options->num_objects, (const char **)options->object_names, 1,
	&state, NULL, gcv_output_ascii_region, gcv_output_profile_leaf, &valid) >= 0 && valid;
}
#endif
