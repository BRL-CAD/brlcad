/*                     N U R B _ K N O T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file primitives/bspline/nurb_knot.c
 *
 * Various knot vector routines.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"


/**
 * rt_nurb_kvknot()
 *
 * Generate a open knot vector with n=order knots at the beginning of
 * the sequence and n knots at the end of the sequence with a lower,
 * and an upper value and num knots in between
 */
void
rt_nurb_kvknot(register struct knot_vector *new_knots, int order, fastf_t lower, fastf_t upper, int num, struct resource *res)
{
    register int i;
    int total;
    fastf_t knot_step;

    if (res) RT_CK_RESOURCE(res);

    total = order * 2 + num;

    knot_step = (upper - lower) / (num + 1);

    new_knots->k_size = total;

    new_knots->knots = (fastf_t *) bu_malloc (sizeof(fastf_t) * total,
					      "rt_nurb_kvknot: new knots values");

    for (i = 0; i < order; i++)
	new_knots->knots[i] = lower;

    for (i = order; i <= (num + order - 1); i++)
	new_knots->knots[i] = new_knots->knots[i-1] + knot_step;

    for (i = (num + order); i < total; i++)
	new_knots->knots[i] = upper;
}


/**
 * rt_nurb_kvmult()
 *
 * Construct a new knot vector which is the same as the passed in knot
 * vector except it has multiplicity of num of val. It checks to see
 * if val already is a multiple knot.
 */
void
rt_nurb_kvmult(struct knot_vector *new_kv, const struct knot_vector *kv, int num, register fastf_t val, struct resource *res)
{
    int n;
    register int i;
    struct knot_vector check;

    if (res) RT_CK_RESOURCE(res);

    n = rt_nurb_kvcheck(val, kv);

    check.k_size = num - n;
    if (check.k_size <= 0) {
	bu_log("rt_nurb_kvmult(new_kv=x%x, kv=x%x, num=%d, val=%g)\n",
	       new_kv, kv, num, val);
	rt_nurb_pr_kv(kv);
	bu_bomb("rt_nurb_kvmult\n");
    }

    check.knots = (fastf_t *) bu_malloc(sizeof(fastf_t) * check.k_size,
					"rt_nurb_kvmult: check knots");

    for (i = 0; i < num - n; i++)
	check.knots[i] = val;

    rt_nurb_kvmerge(new_kv, &check, kv, res);

    /* free up old knot values */
    bu_free((char *)check.knots, "rt_nurb_kvmult:check knots");
}


/**
 * rt_nurb_kvgen()
 *
 * Generate a knot vector with num knots from lower value to the upper
 * value.
 */
void
rt_nurb_kvgen(register struct knot_vector *kv, fastf_t lower, fastf_t upper, int num, struct resource *res)
{
    register int i;
    register fastf_t inc;

    if (res) RT_CK_RESOURCE(res);

    inc = (upper - lower) / (num + 1);

    kv->k_size = num;

    kv->knots = (fastf_t *) bu_malloc (sizeof(fastf_t) * num,
				       "rt_nurb_kvgen: kv knots");

    for (i = 1; i <= num; i++)
	kv->knots[i-1] = lower + i * inc;
}


/**
 * rt_nurb_kvmerge()
 *
 * Merge two knot vectors together and return the new resulting knot
 * vector.
 */
void
rt_nurb_kvmerge(struct knot_vector *new_knots, const struct knot_vector *kv1, const struct knot_vector *kv2, struct resource *res)
{
    int kv1_ptr = 0;
    int kv2_ptr = 0;
    int new_ptr;

    if (res) RT_CK_RESOURCE(res);

    new_knots->k_size = kv1->k_size + kv2->k_size;

    new_knots->knots = (fastf_t *) bu_malloc(
	sizeof (fastf_t) * new_knots->k_size,
	"rt_nurb_kvmerge: new knot values");

    for (new_ptr = 0; new_ptr < new_knots->k_size; new_ptr++) {
	if (kv1_ptr >= kv1->k_size)
	    new_knots->knots[new_ptr] = kv2->knots[kv2_ptr++];
	else if (kv2_ptr >= kv2->k_size)
	    new_knots->knots[new_ptr] = kv1->knots[kv1_ptr++];
	else if (kv1->knots[kv1_ptr] < kv2->knots[kv2_ptr])
	    new_knots->knots[new_ptr] = kv1->knots[kv1_ptr++];
	else
	    new_knots->knots[new_ptr] = kv2->knots[kv2_ptr++];
    }
}


/**
 * rt_nurb_kvcheck()
 *
 * Checks to see if the knot (val) exists in the Knot Vector and
 * returns its multiplicity.
 */
int
rt_nurb_kvcheck(fastf_t val, register const struct knot_vector *kv)
{
    register int kv_num = 0;
    register int i;

    for (i = 0; i < kv->k_size; i++) {
	if (ZERO(val - kv->knots[i]))
	    kv_num++;
    }

    return kv_num;
}


/**
 * rt_nurb_kvextract()
 *
 * Extract the portion of the knot vector from kv->knots[lower] to
 * kv->knots[upper]
 */
void
rt_nurb_kvextract(struct knot_vector *new_kv, register const struct knot_vector *kv, int lower, int upper, struct resource *res)
{
    register int i;
    register fastf_t *ptr;

    if (res) RT_CK_RESOURCE(res);

    new_kv->knots = (fastf_t *) bu_malloc (
	sizeof (fastf_t) * (upper - lower),
	"spl_kvextract: nkw kv values");

    new_kv->k_size = upper - lower;
    ptr = new_kv->knots;

    for (i = lower; i < upper; i++)
	*ptr++ = kv->knots[i];
}


/**
 * rt_nurb_kvcopy()
 *
 * Generic copy the knot vector and pass a new one in.
 */
void
rt_nurb_kvcopy(struct knot_vector *new_kv, register const struct knot_vector *old_kv, struct resource *res)
{
    register int i;

    if (res) RT_CK_RESOURCE(res);

    new_kv->k_size = old_kv->k_size;

    new_kv->knots = (fastf_t *) bu_malloc(sizeof(fastf_t) *
					  new_kv->k_size, "spl_kvcopy: new knot values");

    for (i = 0; i < new_kv->k_size; i++)
	new_kv->knots[i] = old_kv->knots[i];
}


/**
 * rt_nurb_kvnorm()
 *
 * Normalize the knot vector so its values are from zero to one.
 *
 * XXX Need to check to see if the lower value is zero
 */
void
rt_nurb_kvnorm(register struct knot_vector *kv)
{
    register fastf_t upper;
    register int i;

    upper = kv->knots[kv->k_size - 1];
    if (ZERO(upper))
	upper = 0;
    else
	upper = 1 / upper;

    for (i = 0; i < kv->k_size; i++)
	kv->knots[i] *= upper;
}


/**
 * knot_index()
 *
 * Calculates and returns the index of the value for the knot vector
 *
 * XXX It is hard to know what tolerance to use here for the comparisons.
 */
int
rt_nurb_knot_index(const struct knot_vector *kv, fastf_t k_value, int order)
{
    int i;
    fastf_t knt;
    int k_index;

    if (k_value < (knt = *(kv->knots + order - 1))) {
	if (fabs(k_value - knt) < 0.0001) {
	    k_value = knt;
	} else
	    return - 1;
    }

    if (k_value > (knt = *(kv->knots + kv->k_size - order + 1))) {
	if (fabs(k_value - knt) < 0.0001) {
	    k_value = knt;
	} else
	    return - 1;
    }

    if (ZERO(k_value - kv->knots[ kv->k_size - order + 1]))
	k_index = kv->k_size - order - 1;
    else if (ZERO(k_value - kv->knots[ order - 1]))
	k_index = order - 1;
    else {
	k_index = 0;
	for (i = 0; i < kv->k_size - 1; i++)
	    if (kv->knots[i] < k_value && k_value <= kv->knots[i+1])
		k_index = i;

    }

    return k_index;
}


/**
 * rt_nurb_gen_knot_vector()
 *
 * Generate a open knot vector with n=order knots at the beginning of
 * the sequence and n knots at the end of the sequence.
 */
void
rt_nurb_gen_knot_vector(register struct knot_vector *new_knots, int order, fastf_t lower, fastf_t upper, struct resource *res)
{
    register int i;
    int total;

    if (res) RT_CK_RESOURCE(res);

    total = order * 2;

    new_knots->k_size = total;

    new_knots->knots = (fastf_t *) bu_malloc (sizeof(fastf_t) * total,
					      "rt_nurb_gen_knot_vector: new knots values");

    for (i = 0; i < order; i++)
	new_knots->knots[i] = lower;

    for (i = order; i < total; i++)
	new_knots->knots[i] = upper;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
