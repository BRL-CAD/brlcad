/*                    M E A S U R E . C
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
/** @file libbsg/measure.c */

#include "common.h"

#include <math.h>
#include <string.h>

#include "bn.h"
#include "bu/malloc.h"
#include "bsg/action.h"
#include "bsg/measure.h"
#include "action_private.h"

static int
_measure_candidates_impl(struct bsg_view *v, point_t a, point_t b,
			 struct bsg_measure_result *out)
{
    if (!out)
	return 0;

    out->mr_distance = 0.0;
    out->mr_projection = 0.0;
    out->mr_normal_alignment = 0.0;
    out->mr_valid = 0;

    vect_t ab = VINIT_ZERO;
    VSUB2(ab, b, a);
    fastf_t d = MAGNITUDE(ab);
    if (ZERO(d))
	return 0;

    out->mr_distance = d;

    if (v) {
	point_t av = VINIT_ZERO;
	point_t bv = VINIT_ZERO;
	vect_t abv = VINIT_ZERO;
	MAT4X3PNT(av, v->gv_model2view, a);
	MAT4X3PNT(bv, v->gv_model2view, b);
	VSUB2(abv, bv, av);
	out->mr_projection = MAGNITUDE(abv);

	vect_t n = VINIT_ZERO;
	VSET(n, 0.0, 0.0, 1.0);
	fastf_t norm_ab[3] = {ab[0], ab[1], ab[2]};
	VUNITIZE(norm_ab);
	out->mr_normal_alignment = fabs(VDOT(norm_ab, n));
    } else {
	out->mr_projection = d;
	out->mr_normal_alignment = 0.0;
    }

    out->mr_valid = 1;
    return 1;
}

struct measure_action_data {
    struct bsg_view *v;
    point_t a;
    point_t b;
    struct bsg_measure_result *out;
};

static int
_measure_action_apply(bsg_action_ref UNUSED(action),
		      bsg_node_ref UNUSED(root),
		      void *data)
{
    struct measure_action_data *st = (struct measure_action_data *)data;
    if (!st)
	return 0;
    return _measure_candidates_impl(st->v, st->a, st->b, st->out);
}

static void
_measure_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    if (data)
	bu_free(data, "measure action data");
}

bsg_action_ref
bsg_measure_action_create(struct bsg_view *view,
			  const point_t a,
			  const point_t b,
			  struct bsg_measure_result *result)
{
    struct measure_action_data *st;
    BU_ALLOC(st, struct measure_action_data);
    memset(st, 0, sizeof(*st));
    st->v = view;
    VMOVE(st->a, a);
    VMOVE(st->b, b);
    st->out = result;
    return bsg_action_ref_create_internal(bsg_measure_action_type(),
	    _measure_action_apply, _measure_action_destroy, st,
	    "measure action");
}

int
bsg_measure_candidates(struct bsg_view *v, point_t a, point_t b,
		       struct bsg_measure_result *out)
{
    bsg_action_ref action = bsg_measure_action_create(v, a, b, out);
    int ret = bsg_action_apply(action, bsg_node_ref_null());
    bsg_action_ref_destroy(action);
    return ret;
}
