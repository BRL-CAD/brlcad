/* 
 *       N U R B _ K N O T . C
 *
 * Function -
 *     Various knot vector routines.
 * 
 * Author -
 *     Paul R. Stay
 *
 * Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1990 by the United States Army.
 *     All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

/* rt_nurb_kvknot()
 * Generate a open knot vector with n=order knots at the beginning of
 * the sequence and n knots at the end of the sequence with a lower,
 * and an upper value and num knots in between
 */

void
rt_nurb_kvknot( new_knots, order, lower, upper, num, res)
register struct knot_vector *new_knots;
int	order;
fastf_t lower;
fastf_t upper;
int	num;
struct resource *res;
{
	register int	i;
	int	total;
	fastf_t knot_step;

	total = order * 2 + num;

	knot_step = (upper - lower) / ( num + 1 );

	new_knots->k_size = total;

	if( res )
		new_knots->knots = (fastf_t * ) pmalloc ( sizeof( fastf_t) * total,
		    &res->re_pmem);
	else
		new_knots->knots = (fastf_t * ) rt_malloc ( sizeof( fastf_t) * total,
		    "rt_nurb_kvknot: new knots values");

	for ( i = 0; i < order; i++)
		new_knots->knots[i] = lower;

	for ( i = order; i <= (num + order - 1); i++)
		new_knots->knots[i] = new_knots->knots[i-1] + knot_step;

	for ( i = ( num + order ) ; i < total; i++)
		new_knots->knots[i] = upper;
}


/* rt_nurb_kvmult()
 *	Construct a new knot vector which is the same as the passed in
 * knot vector except it has multiplicity of num of val. It checks to see if 
 * val already is a multiple knot. 
 */
void
rt_nurb_kvmult( new_kv, kv, num, val, res)
struct knot_vector	*new_kv;
CONST struct knot_vector *kv;
int			num;
register fastf_t	val;
struct resource *res;
{
	int	n;
	register int	i;
	struct knot_vector check;

	n = rt_nurb_kvcheck( val, kv );

	check.k_size = num - n;
	if( check.k_size <= 0 )  {
		bu_log("rt_nurb_kvmult(new_kv=x%x, kv=x%x, num=%d, val=%g)\n",
			new_kv, kv, num, val);
		rt_nurb_pr_kv(kv);
		rt_bomb("rt_nurb_kvmult\n");
	}

	if( res )
		check.knots = (fastf_t * ) pmalloc( sizeof(fastf_t) * check.k_size,
		    &res->re_pmem);
	else
		check.knots = (fastf_t * ) rt_malloc( sizeof(fastf_t) * check.k_size,
		    "rt_nurb_kvmult: check knots");

	for ( i = 0; i < num - n; i++)
		check.knots[i] = val;

	rt_nurb_kvmerge( new_kv, &check, kv, res);

	/* free up old knot values */
	if( res )
		pfree((char *)check.knots, &res->re_pmem);
	else
		rt_free((char *)check.knots, "rt_nurb_kvmult:check knots");
}


/* rt_nurb_kvgen( )
 * 	Generate a knot vector with num knots from lower value to 
 * 	the upper  value.
 */

void
rt_nurb_kvgen( kv, lower, upper, num, res)
register struct knot_vector *kv;
int	num;
fastf_t lower, upper;
struct resource *res;
{
	register int	i;
	register fastf_t inc;

	inc = (upper - lower) / ( num + 1 );

	kv->k_size = num;

	if( res )
		kv->knots = (fastf_t * ) pmalloc ( sizeof( fastf_t) * num, 
		    &res->re_pmem);
	else
		kv->knots = (fastf_t * ) rt_malloc ( sizeof( fastf_t) * num, 
		    "rt_nurb_kvgen: kv knots");

	for ( i = 1; i <= num; i++)
		kv->knots[i-1] = lower + i * inc;
}


/* rt_nurb_kvmerge()
 *	Merge two knot vectors together and return the new resulting 
 *	knot vector.
 */

void
rt_nurb_kvmerge( new_knots, kv1, kv2, res )
struct knot_vector *new_knots;
CONST struct knot_vector *kv1;
CONST struct knot_vector *kv2;
struct resource *res;
{
	int	kv1_ptr = 0;
	int	kv2_ptr = 0;
	int	new_ptr;

	new_knots->k_size = kv1->k_size + kv2->k_size;

	if( res )
		new_knots->knots = (fastf_t * ) pmalloc( 
		    sizeof (fastf_t) * new_knots->k_size,
		    &res->re_pmem);
	else
		new_knots->knots = (fastf_t * ) rt_malloc( 
		    sizeof (fastf_t) * new_knots->k_size,
		    "rt_nurb_kvmerge: new knot values");

	for ( new_ptr = 0; new_ptr < new_knots->k_size; new_ptr++) {
		if ( kv1_ptr >= kv1->k_size )
			new_knots->knots[new_ptr] = kv2->knots[kv2_ptr++];
		else if ( kv2_ptr >= kv2->k_size )
			new_knots->knots[new_ptr] = kv1->knots[kv1_ptr++];
		else if ( kv1->knots[kv1_ptr] < kv2->knots[kv2_ptr])
			new_knots->knots[new_ptr] = kv1->knots[kv1_ptr++];
		else
			new_knots->knots[new_ptr] = kv2->knots[kv2_ptr++];
	}
}


/* rt_nurb_kvcheck()
 *	Checks to see if the knot (val) exists in the Knot Vector and
 *	returns its multiplicity.
 */

int
rt_nurb_kvcheck( val, kv)
fastf_t val;
register CONST struct knot_vector *kv;
{
	register int	kv_num = 0;
	register int	i;

	for ( i = 0; i < kv->k_size; i++) {
		if ( val == kv->knots[i] )
			kv_num++;
	}

	return kv_num;
}


/* rt_nurb_kvextract()
 *	Extract the portion of the knot vector from kv->knots[lower] to
 *	kv->knots[upper]
 */

void
rt_nurb_kvextract( new_kv, kv, lower, upper, res)
struct knot_vector	*new_kv;
register CONST struct knot_vector *kv;
int	upper, lower;
struct resource *res;
{
	register int	i;
	register fastf_t *ptr;

	if( res )
		new_kv->knots = (fastf_t * ) pmalloc ( 
		    sizeof (fastf_t) * (upper - lower),
		    &res->re_pmem );
	else
		new_kv->knots = (fastf_t * ) rt_malloc ( 
		    sizeof (fastf_t) * (upper - lower),
		    "spl_kvextract: nkw kv values" );

	new_kv->k_size = upper - lower;
	ptr = new_kv->knots;

	for ( i = lower; i < upper; i++)
		*ptr++ = kv->knots[i];
}


/* rt_nurb_kvcopy()
 *	Generic copy the knot vector and pass a new one in.
 */

void
rt_nurb_kvcopy( new_kv, old_kv, res )
struct knot_vector	*new_kv;
register CONST struct knot_vector *old_kv;
struct resource *res;
{
	register int	i;

	new_kv->k_size = old_kv->k_size;

	if( res )
		new_kv->knots = (fastf_t * ) pmalloc( sizeof( fastf_t) * 
		    new_kv->k_size, &res->re_pmem);
	else
		new_kv->knots = (fastf_t * ) rt_malloc( sizeof( fastf_t) * 
		    new_kv->k_size, "spl_kvcopy: new knot values");

	for ( i = 0; i < new_kv->k_size; i++)
		new_kv->knots[i] = old_kv->knots[i];
}


/* rt_nurb_kvnorm()
 *	Normalize the knot vector so its values are from zero to one.
 */

/* XXX Need to check to see if the lower value is zero */
void
rt_nurb_kvnorm( kv )
register struct knot_vector *kv;
{
	register fastf_t upper;
	register int	i;

	upper = kv->knots[kv->k_size - 1];
	if( NEAR_ZERO( upper, SMALL ) )
		upper = 0;
	else
		upper = 1 / upper;

	for ( i = 0; i < kv->k_size; i++)
		kv->knots[i] *= upper;
}


/* knot_index()
 *	Calculates and returns the index of the value for the knot vector
 *
 * XXX It is hard to know what tolerance to use here for the comparisons.
 */

int
rt_nurb_knot_index( kv, k_value, order)
CONST struct knot_vector *kv;
fastf_t k_value;
int	order;
{
	int	i;
	fastf_t  knt;
	int	k_index;

	if ( k_value < ( knt = *(kv->knots + order - 1))) {
		if (fabs( k_value - knt) < 0.0001) {
			k_value = knt;
		} else
			return - 1;
	}

	if ( k_value > ( knt = *(kv->knots + kv->k_size - order + 1)) ) {
		if (fabs( k_value - knt) < 0.0001) {
			k_value = knt;
		} else
			return - 1;
	}

	if ( k_value == kv->knots[ kv->k_size - order + 1] )
		k_index = kv->k_size - order - 1;
	else if ( k_value == kv->knots[ order - 1] )
		k_index = order - 1;
	else
	 {
		k_index = 0;
		for ( i = 0; i < kv->k_size - 1; i++)
			if ( kv->knots[i] < k_value && k_value <= kv->knots[i+1] )
				k_index = i;

	}

	return k_index;
}


/* rt_nurb_gen_knot_vector()
 * Generate a open knot vector with n=order knots at the beginning of
 * the sequence and n knots at the end of the sequence.
 */

void
rt_nurb_gen_knot_vector( new_knots, order, lower, upper, res)
register struct knot_vector *new_knots;
int		order;
fastf_t		lower;
fastf_t		upper;
struct resource *res;
{
    register int i;
    int total;

    total = order * 2;

    new_knots->k_size = total;

    if( res )
	    new_knots->knots = (fastf_t *) pmalloc ( sizeof( fastf_t) * total,
		&res->re_pmem);
    else
	    new_knots->knots = (fastf_t *) rt_malloc ( sizeof( fastf_t) * total,
		"rt_nurb_gen_knot_vector: new knots values");

    for ( i = 0; i < order; i++)
        new_knots->knots[i] = lower;

    for ( i = order; i < total; i++)
        new_knots->knots[i] = upper;
}
