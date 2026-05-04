/*              E D I T _ C O N S T R A I N T . H
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

#ifndef RT_EDIT_CONSTRAINT_H
#define RT_EDIT_CONSTRAINT_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

struct rt_db_internal;

enum rt_constraint_edit_policy {
    RT_CONSTRAINT_EDIT_REJECT = 0,
    RT_CONSTRAINT_EDIT_WARN_ONLY,
    RT_CONSTRAINT_EDIT_SNAP,
    RT_CONSTRAINT_EDIT_SNAP_IF_WITHIN_TOL
};

enum rt_constraint_edit_severity {
    RT_CONSTRAINT_EDIT_INFO = 0,
    RT_CONSTRAINT_EDIT_WARN,
    RT_CONSTRAINT_EDIT_ERROR
};

enum rt_constraint_edit_op_kind {
    RT_CONSTRAINT_EDIT_OP_MOVE_POINT = 0,
    RT_CONSTRAINT_EDIT_OP_ADD_POINT,
    RT_CONSTRAINT_EDIT_OP_INSERT_POINT,
    RT_CONSTRAINT_EDIT_OP_DELETE_POINT,
    RT_CONSTRAINT_EDIT_OP_SET_OD,
    RT_CONSTRAINT_EDIT_OP_SET_ID,
    RT_CONSTRAINT_EDIT_OP_SET_BEND,
    RT_CONSTRAINT_EDIT_OP_SCALE_OD,
    RT_CONSTRAINT_EDIT_OP_SCALE_ID,
    RT_CONSTRAINT_EDIT_OP_SCALE_BEND
};

struct rt_constraint_edit_param_ref {
    const char *name;
    int index0;
    int index1;
};

struct rt_constraint_edit_violation {
    int code;
    enum rt_constraint_edit_severity severity;
    struct rt_constraint_edit_param_ref a;
    struct rt_constraint_edit_param_ref b;
    fastf_t value_a;
    fastf_t value_b;
    struct bu_vls msg;
};

struct rt_constraint_edit_metric {
    fastf_t w_coord;
    fastf_t w_od;
    fastf_t w_id;
    fastf_t w_bend;
};

struct rt_constraint_edit_tolerances {
    fastf_t max_coord_delta;
    fastf_t max_od_delta;
    fastf_t max_id_delta;
    fastf_t max_bend_delta;
    fastf_t max_total_cost;
};

struct rt_constraint_edit_op {
    enum rt_constraint_edit_op_kind kind;
    int point_index;
    vect_t proposed_coord;
    fastf_t proposed_scalar;
};

struct rt_constraint_edit_result {
    int accepted;
    int snapped;
    int violation_count_before;
    int violation_count_after;
    fastf_t total_cost;
    struct bu_ptbl changed_params;
    struct bu_ptbl violations;
    struct bu_vls summary;
};

struct rt_constraint_edit_ctx {
    enum rt_constraint_edit_policy policy;
    struct rt_constraint_edit_metric metric;
    struct rt_constraint_edit_tolerances tol;
    int diagnostics_verbosity;
};

typedef int (*rt_constraint_edit_validate_t)(
    struct bu_ptbl *violations,
    const struct rt_db_internal *ip,
    const struct rt_constraint_edit_ctx *ctx);

typedef int (*rt_constraint_edit_project_apply_t)(
    struct rt_constraint_edit_result *out,
    struct rt_db_internal *ip,
    const struct rt_constraint_edit_op *op,
    const struct rt_constraint_edit_ctx *ctx);

typedef fastf_t (*rt_constraint_edit_score_delta_t)(
    const struct rt_db_internal *before_ip,
    const struct rt_db_internal *after_ip,
    const struct rt_constraint_edit_metric *metric);

RT_EXPORT extern void rt_constraint_edit_violation_init(struct rt_constraint_edit_violation *v);
RT_EXPORT extern void rt_constraint_edit_violation_free(struct rt_constraint_edit_violation *v);

RT_EXPORT extern void rt_constraint_edit_result_init(struct rt_constraint_edit_result *r);
RT_EXPORT extern void rt_constraint_edit_result_clear(struct rt_constraint_edit_result *r);
RT_EXPORT extern void rt_constraint_edit_result_free(struct rt_constraint_edit_result *r);

__END_DECLS

#endif /* RT_EDIT_CONSTRAINT_H */
