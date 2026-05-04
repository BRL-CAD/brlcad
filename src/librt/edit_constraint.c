/*              E D I T _ C O N S T R A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "rt/edit_constraint.h"


void
rt_constraint_edit_violation_init(struct rt_constraint_edit_violation *v)
{
    if (!v)
        return;

    memset(v, 0, sizeof(struct rt_constraint_edit_violation));
    v->a.index0 = -1;
    v->a.index1 = -1;
    v->b.index0 = -1;
    v->b.index1 = -1;
    bu_vls_init(&v->msg);
}


void
rt_constraint_edit_violation_free(struct rt_constraint_edit_violation *v)
{
    if (!v)
        return;

    bu_vls_free(&v->msg);
    memset(v, 0, sizeof(struct rt_constraint_edit_violation));
}


void
rt_constraint_edit_result_init(struct rt_constraint_edit_result *r)
{
    if (!r)
        return;

    memset(r, 0, sizeof(struct rt_constraint_edit_result));
    bu_ptbl_init(&r->changed_params, 8, "cedit changed_params");
    bu_ptbl_init(&r->violations, 8, "cedit violations");
    bu_vls_init(&r->summary);
}


void
rt_constraint_edit_result_clear(struct rt_constraint_edit_result *r)
{
    size_t i;

    if (!r)
        return;

    for (i = 0; i < BU_PTBL_LEN(&r->changed_params); i++) {
        char *s = (char *)BU_PTBL_GET(&r->changed_params, i);
        if (s)
            bu_free(s, "cedit changed param");
    }
    bu_ptbl_reset(&r->changed_params);

    for (i = 0; i < BU_PTBL_LEN(&r->violations); i++) {
        struct rt_constraint_edit_violation *v = (struct rt_constraint_edit_violation *)BU_PTBL_GET(&r->violations, i);
        if (v) {
            rt_constraint_edit_violation_free(v);
            bu_free(v, "cedit violation");
        }
    }
    bu_ptbl_reset(&r->violations);

    r->accepted = 0;
    r->snapped = 0;
    r->violation_count_before = 0;
    r->violation_count_after = 0;
    r->total_cost = 0.0;
    bu_vls_trunc(&r->summary, 0);
}


void
rt_constraint_edit_result_free(struct rt_constraint_edit_result *r)
{
    if (!r)
        return;

    rt_constraint_edit_result_clear(r);
    bu_ptbl_free(&r->changed_params);
    bu_ptbl_free(&r->violations);
    bu_vls_free(&r->summary);
    memset(r, 0, sizeof(struct rt_constraint_edit_result));
}
