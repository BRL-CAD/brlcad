/*                       T A B D A T A . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/** @addtogroup anim */
/** @{ */
/** @file libbn/tabdata.c
 *
 * @brief
 *  Routines for processing tables (curves) of data with one independent
 *  parameter which is common to many sets of dependent data values.
 *
 *  Operates on bn_table (independent var) and
 *  bn_tabdata (dependent variable) structures.
 *
 *  One application is for storing spectral curves, see spectrum.c
 *
 *  @par Inspired by -
 *	Roy Hall and his book "Illumination and Color in Computer
 *@n	Generated Imagery", Springer Verlag, New York, 1989.
 *@n	ISBN 0-387-96774-5
 *
 *  With thanks to Russ Moulton Jr, EOSoft Inc. for his "rad.c" module.
 */
/** @} */

#include "common.h"

#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bu.h"
#include "bn.h"

void
bn_table_free(struct bn_table *tabp)
{
    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_free(%p)\n", (void *)tabp);
    BN_CK_TABLE(tabp);

    tabp->nx = 0;			/* sanity */
    bu_free(tabp, "struct bn_table");
}

void
bn_tabdata_free(struct bn_tabdata *data)
{
    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_free(%p)\n", (void *)data);

    BN_CK_TABDATA(data);
    BN_CK_TABLE(data->table);

    data->ny = 0;			/* sanity */
    data->table = NULL;		/* sanity */
    bu_free(data, "struct bn_tabdata");
}

void
bn_ck_table(const struct bn_table *tabp)
{
    register size_t i;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_ck_table(%p)\n", (void *)tabp);

    BN_CK_TABLE(tabp);

    if (tabp->nx < 2)
	bu_bomb("bn_ck_table() less than 2 wavelengths\n");

    for (i=0; i < tabp->nx; i++)  {
	if (tabp->x[i] >= tabp->x[i+1])
	    bu_bomb("bn_ck_table() wavelengths not in strictly ascending order\n");
    }
}

struct bn_table *
bn_table_make_uniform(size_t num, double first, double last) {
    struct bn_table	*tabp;
    fastf_t			*fp;
    fastf_t			delta;
    int			j;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_make_uniform( num=%zu, %g, %g )\n", num, first, last);

    if (first >= last)  bu_bomb("bn_table_make_uniform() first >= last\n");

    BN_GET_TABLE(tabp, num);

    delta = (last - first) / (double)num;

    fp = &tabp->x[0];
    for (j = num; j > 0; j--)  {
	*fp++ = first;
	first += delta;
    }
    tabp->x[num] = last;

    return tabp;
}

void
bn_tabdata_add(struct bn_tabdata *out, const struct bn_tabdata *in1, const struct bn_tabdata *in2)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_add(%p, %p, %p)\n", (void *)out, (void *)in1, (void *)in2);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);

    if (in1->table != in2->table || in1->table != out->table)
	bu_bomb("bn_tabdata_add(): samples drawn from different tables\n");
    if (in1->ny != in2->ny || in1->ny != out->ny)
	bu_bomb("bn_tabdata_add(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = *i1++ + *i2++;
    /* VADD2N( out->y, i1->y, i2->y, in1->ny ); */
}

void
bn_tabdata_mul(struct bn_tabdata *out, const struct bn_tabdata *in1, const struct bn_tabdata *in2)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul(%p, %p, %p)\n", (void *)out, (void *)in1, (void *)in2);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);

    if (in1->table != in2->table || in1->table != out->table)
	bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
    if (in1->ny != in2->ny || in1->ny != out->ny)
	bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = *i1++ * *i2++;
    /* VELMUL2N( out->y, i1->y, i2->y, in1->ny ); */
}

void
bn_tabdata_mul3(struct bn_tabdata *out, const struct bn_tabdata *in1, const struct bn_tabdata *in2, const struct bn_tabdata *in3)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2, *i3;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul3(%p, %p, %p, %p)\n", (void *)out, (void *)in1, (void *)in2, (void *)in3);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);
    BN_CK_TABDATA(in3);

    if (in1->table != in2->table || in1->table != out->table || in1->table != in2->table)
	bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
    if (in1->ny != in2->ny || in1->ny != out->ny)
	bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    i3 = in3->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = *i1++ * *i2++ * *i3++;
    /* VELMUL3N( out->y, i1->y, i2->y, i3->y, in1->ny ); */
}

void
bn_tabdata_incr_mul3_scale(struct bn_tabdata *out, const struct bn_tabdata *in1, const struct bn_tabdata *in2, const struct bn_tabdata *in3, register double scale)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2, *i3;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_incr_mul3_scale(%p, %p, %p, %p, %g)\n", (void *)out, (void *)in1, (void *)in2, (void *)in3, scale);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);
    BN_CK_TABDATA(in3);

    if (in1->table != in2->table || in1->table != out->table || in1->table != in3->table)
	bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
    if (in1->ny != in2->ny || in1->ny != out->ny)
	bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    i3 = in3->y;
    for (j = in1->ny; j > 0; j--)
	*op++ += *i1++ * *i2++ * *i3++ * scale;
}

void
bn_tabdata_incr_mul2_scale(struct bn_tabdata *out, const struct bn_tabdata *in1, const struct bn_tabdata *in2, register double scale)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_incr_mul2_scale(%p, %p, %p, %g)\n", (void *)out, (void *)in1, (void *)in2, scale);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);

    if (in1->table != in2->table || in1->table != out->table)
	bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
    if (in1->ny != in2->ny || in1->ny != out->ny)
	bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    for (j = in1->ny; j > 0; j--)
	*op++ += *i1++ * *i2++ * scale;
}

void
bn_tabdata_scale(struct bn_tabdata *out, const struct bn_tabdata *in1, register double scale)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_scale(%p, %p, %g)\n", (void *)out, (void *)in1, scale);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);

    if (in1->table != out->table)
	bu_bomb("bn_tabdata_scale(): samples drawn from different tables\n");
    if (in1->ny != out->ny)
	bu_bomb("bn_tabdata_scale(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = *i1++ * scale;
    /* VSCALEN( out->y, in->y, scale ); */
}

void
bn_table_scale(struct bn_table *tabp, register double scale)
{
    register int		j;
    register fastf_t	*op;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_scale(%p, %g)\n", (void *)tabp, scale);

    BN_CK_TABLE(tabp);

    op = tabp->x;
    for (j = tabp->nx+1; j > 0; j--)
	*op++ *= scale;
    /* VSCALEN( tabp->x, tabp->x, scale, tabp->nx+1 ); */
}

void
bn_tabdata_join1(struct bn_tabdata *out, const struct bn_tabdata *in1, register double scale, const struct bn_tabdata *in2)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_join1(%p, %p, %g, %p)\n", (void *)out, (void *)in1, scale, (void *)in2);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);

    if (in1->table != out->table)
	bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
    if (in1->table != in2->table)
	bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
    if (in1->ny != out->ny)
	bu_bomb("bn_tabdata_join1(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = *i1++ + scale * *i2++;
    /* VJOIN1N( out->y, in1->y, scale, in2->y ); */
}

void
bn_tabdata_join2(struct bn_tabdata *out, const struct bn_tabdata *in1, register double scale2, const struct bn_tabdata *in2, register double scale3, const struct bn_tabdata *in3)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2, *i3;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_join2(%p, %p, %g, %p, %g, %p)\n", (void *)out, (void *)in1, scale2, (void *)in2, scale3, (void *)in3);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);

    if (in1->table != out->table)
	bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
    if (in1->table != in2->table)
	bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
    if (in1->table != in3->table)
	bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
    if (in1->ny != out->ny)
	bu_bomb("bn_tabdata_join1(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    i3 = in3->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = *i1++ + scale2 * *i2++ + scale3 * *i3++;
    /* VJOIN2N( out->y, in1->y, scale2, in2->y, scale3, in3->y ); */
}

void
bn_tabdata_blend2(struct bn_tabdata *out, register double scale1, const struct bn_tabdata *in1, register double scale2, const struct bn_tabdata *in2)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_blend2(%p, %g, %p, %g, %p)\n", (void *)out, scale1, (void *)in1, scale2, (void *)in2);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);

    if (in1->table != out->table)
	bu_bomb("bn_tabdata_blend2(): samples drawn from different tables\n");
    if (in1->table != in2->table)
	bu_bomb("bn_tabdata_blend2(): samples drawn from different tables\n");
    if (in1->ny != out->ny)
	bu_bomb("bn_tabdata_blend2(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = scale1 * *i1++ + scale2 * *i2++;
    /* VBLEND2N( out->y, scale1, in1->y, scale2, in2->y ); */
}

void
bn_tabdata_blend3(struct bn_tabdata *out, register double scale1, const struct bn_tabdata *in1, register double scale2, const struct bn_tabdata *in2, register double scale3, const struct bn_tabdata *in3)
{
    register int		j;
    register fastf_t	*op;
    register const fastf_t	*i1, *i2, *i3;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_blend3(%p, %g, %p, %g, %p, %g, %p)\n", (void *)out, scale1, (void *)in1, scale2, (void *)in2, scale3, (void *)in3);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);
    BN_CK_TABDATA(in3);

    if (in1->table != out->table)
	bu_bomb("bn_tabdata_blend3(): samples drawn from different tables\n");
    if (in1->table != in2->table)
	bu_bomb("bn_tabdata_blend3(): samples drawn from different tables\n");
    if (in1->table != in3->table)
	bu_bomb("bn_tabdata_blend3(): samples drawn from different tables\n");
    if (in1->ny != out->ny)
	bu_bomb("bn_tabdata_blend3(): different tabdata lengths?\n");

    op = out->y;
    i1 = in1->y;
    i2 = in2->y;
    i3 = in3->y;
    for (j = in1->ny; j > 0; j--)
	*op++ = scale1 * *i1++ + scale2 * *i2++ + scale3 * *i3++;
    /* VBLEND3N( out->y, scale1, in1->y, scale2, in2->y, scale3, in3->y ); */
}

double
bn_tabdata_area1(const struct bn_tabdata *in)
{
    register fastf_t		area;
    register const fastf_t	*ip;
    register int		j;

    BN_CK_TABDATA(in);

    area = 0;
    ip = in->y;
    for (j = in->ny; j > 0; j--)
	area += *ip++;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_area(%p) = %g\n", (void *)in, area);

    return area;
}

double
bn_tabdata_area2(const struct bn_tabdata *in)
{
    const struct bn_table	*tabp;
    register fastf_t		area;
    fastf_t			width;
    register int		j;

    BN_CK_TABDATA(in);
    tabp = in->table;
    BN_CK_TABLE(tabp);

    area = 0;
    for (j = in->ny-1; j >= 0; j--)  {
	width = tabp->x[j+1] - tabp->x[j];
	area += in->y[j] * width;
    }

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_area2(%p) = %g\n", (void *)in, area);
    return area;
}

double
bn_tabdata_mul_area1(const struct bn_tabdata *in1, const struct bn_tabdata *in2)
{
    register fastf_t		area;
    register const fastf_t	*i1, *i2;
    register int		j;

    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);

    area = 0;
    i1 = in1->y;
    i2 = in2->y;
    for (j = in1->ny; j > 0; j--)
	area += *i1++ * *i2++;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul_area1(%p, %p) = %g\n", (void *)in1, (void *)in2, area);
    return area;
}

double
bn_tabdata_mul_area2(const struct bn_tabdata *in1, const struct bn_tabdata *in2)
{
    const struct bn_table	*tabp;
    register fastf_t		area;
    fastf_t			width;
    register int		j;

    BN_CK_TABDATA(in1);
    BN_CK_TABDATA(in2);
    tabp = in1->table;
    BN_CK_TABLE(tabp);

    area = 0;
    for (j = in1->ny-1; j >= 0; j--)  {
	width = tabp->x[j+1] - tabp->x[j];
	area += in1->y[j] * in2->y[j] * width;
    }

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul_area2(%p, %p) = %g\n", (void *)in1, (void *)in2, area);
    return area;
}

/*
 *@brief
 *  Return the index in the table's x[] array of the interval which
 *  contains 'xval'.
 *
 *
 * @return	-1	if less than start value
 * @return	-2	if greater than end value.
 * @return	0..nx-1	otherwise.
 *		Never returns a value of nx.
 *
 *  A binary search would be more efficient, as the wavelengths (x values)
 *  are known to be sorted in ascending order.
 */
long
bn_table_find_x(const struct bn_table *tabp, double xval)
{
    register int	i;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_find_x(%p, %g)\n", (void *)tabp, xval);
    BN_CK_TABLE(tabp);

    if (xval > tabp->x[tabp->nx])  return -2;
    if (xval >= tabp->x[tabp->nx-1])  return tabp->nx-1;

    /* Search for proper interval in input spectrum */
    for (i = tabp->nx-2; i >=0; i--)  {
	if (xval >= tabp->x[i])  return i;
    }
    /* if ( xval < tabp->x[0] )  return -1; */
    return -1;
}

fastf_t
bn_table_lin_interp(const struct bn_tabdata *samp, register double wl)
{
    const struct bn_table *tabp;
    size_t i;
    int idx;
    fastf_t fract;
    fastf_t ret;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_lin_interp(%p, %g)\n", (void *)samp, wl);

    BN_CK_TABDATA(samp);
    tabp = samp->table;
    BN_CK_TABLE(tabp);

    idx = bn_table_find_x(tabp, wl);
    if (idx < 0)  {
	if (bu_debug&BU_DEBUG_TABDATA)
	    bu_log("bn_table_lin_interp(%g) out of range %g to %g\n", wl, tabp->x[0], tabp->x[tabp->nx]);
	return 0;
    }
    i = (size_t)idx;

    if (wl < tabp->x[i] || wl >= tabp->x[i+1])  {
	bu_log("bn_table_lin_interp(%g) assertion1 failed at %g\n", wl, tabp->x[i]);
	bu_bomb("bn_table_lin_interp() assertion1 failed\n");
    }

    if (i >= tabp->nx-2)  {
	/* Assume value is constant in final interval. */
	if (bu_debug&BU_DEBUG_TABDATA)bu_log("bn_table_lin_interp(%g)=%g off end of range %g to %g\n", wl, samp->y[tabp->nx-1], tabp->x[0], tabp->x[tabp->nx]);
	return samp->y[tabp->nx-1];
    }

    /* The interval has been found */
    fract = (wl - tabp->x[i]) / (tabp->x[i+1] - tabp->x[i]);
    if (fract < 0 || fract > 1)  bu_bomb("bn_table_lin_interp() assertion2 failed\n");
    ret = (1-fract) * samp->y[i] + fract * samp->y[i+1];
    if (bu_debug&BU_DEBUG_TABDATA)bu_log("bn_table_lin_interp(%g)=%g in range %g to %g\n",
					     wl, ret, tabp->x[i], tabp->x[i+1]);
    return ret;
}

struct bn_tabdata *
bn_tabdata_resample_max(const struct bn_table *newtable, const struct bn_tabdata *olddata) {
    const struct bn_table	*oldtable;
    struct bn_tabdata	*newsamp;
    size_t i;
    int			j, k;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_resample_max(%p, %p)\n", (void *)newtable, (void *)olddata);

    BN_CK_TABLE(newtable);
    BN_CK_TABDATA(olddata);
    oldtable = olddata->table;
    BN_CK_TABLE(oldtable);

    if (oldtable == newtable)  bu_log("bn_tabdata_resample_max() NOTICE old and new bn_table structs are the same\n");

    BN_GET_TABDATA(newsamp, newtable);

    for (i = 0; i < newtable->nx; i++)  {
	/*
	 *  Find good value(s) in olddata to represent the span from
	 *  newtable->x[i] to newtable->x[i+1].
	 */
	j = bn_table_find_x(oldtable, newtable->x[i]);
	k = bn_table_find_x(oldtable, newtable->x[i+1]);
	if (k == -1)  {
	    /* whole new span is off left side of old table */
	    newsamp->y[i] = 0;
	    continue;
	}
	if (j == -2)  {
	    /* whole new span is off right side of old table */
	    newsamp->y[i] = 0;
	    continue;
	}

	if (j == k && j > 0)  {
	    register fastf_t tmp;
	    /*
	     *  Simple case, ends of output span are completely
	     *  contained within one input span.
	     *  Interpolate for both ends, take max.
	     *  XXX this could be more efficiently written inline here.
	     */
	    newsamp->y[i] = bn_table_lin_interp(olddata, newtable->x[i]);
	    tmp = bn_table_lin_interp(olddata, newtable->x[i+1]);
	    if (tmp > newsamp->y[i])  newsamp->y[i] = tmp;
	} else {
	    register fastf_t tmp, n;
	    register int	s;
	    /*
	     *  Complex case: find good representative value.
	     *  Interpolate both ends, and consider all
	     *  intermediate old samples in span.  Take max.
	     *  One (but not both) new ends may be off old table.
	     */
	    n = bn_table_lin_interp(olddata, newtable->x[i]);
	    tmp = bn_table_lin_interp(olddata, newtable->x[i+1]);
	    if (tmp > n)  n = tmp;
	    for (s = j+1; s <= k; s++)  {
		if ((tmp = olddata->y[s]) > n)
		    n = tmp;
	    }
	    newsamp->y[i] = n;
	}
    }
    return newsamp;
}

struct bn_tabdata *
bn_tabdata_resample_avg(const struct bn_table *newtable, const struct bn_tabdata *olddata) {
    const struct bn_table	*oldtable;
    struct bn_tabdata	*newsamp;
    size_t i;
    int			j, k;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_resample_avg(%p, %p)\n", (void *)newtable, (void *)olddata);

    BN_CK_TABLE(newtable);
    BN_CK_TABDATA(olddata);
    oldtable = olddata->table;
    BN_CK_TABLE(oldtable);

    if (oldtable == newtable)  bu_log("bn_tabdata_resample_avg() NOTICE old and new bn_table structs are the same\n");

    BN_GET_TABDATA(newsamp, newtable);

    for (i = 0; i < newtable->nx; i++)  {
	/*
	 *  Find good value(s) in olddata to represent the span from
	 *  newtable->x[i] to newtable->x[i+1].
	 */
	j = bn_table_find_x(oldtable, newtable->x[i]);
	k = bn_table_find_x(oldtable, newtable->x[i+1]);

	if (j < 0 || k < 0 || j == k)  {
	    /*
	     *  Simple case, ends of output span are completely
	     *  contained within one input span.
	     *  Interpolate for both ends, take average.
	     *  XXX this could be more efficiently written inline here.
	     */
	    newsamp->y[i] = 0.5 * (
				bn_table_lin_interp(olddata, newtable->x[i]) +
				bn_table_lin_interp(olddata, newtable->x[i+1]));
	} else {
	    /*
	     *  Complex case: find average value.
	     *  Interpolate both ends, and consider all
	     *  intermediate old spans.
	     *  There are three parts to sum:
	     *	Partial interval from newx[i] to j+1
	     *	Full intervals from j+1 to k
	     *	Partial interval from k to newx[i+1]
	     */
	    fastf_t wsum;		/* weighted sum */
	    fastf_t	a, b;		/* values being averaged */
	    int	s;

	    /* Partial interval from newx[i] to j+1 */
	    a = bn_table_lin_interp(olddata, newtable->x[i]);	/* in "j" bin */
	    b = olddata->y[j+1];
	    wsum = 0.5 * (a+b) * (oldtable->x[j+1] - newtable->x[i]);

	    /* Full intervals from j+1 to k */
	    for (s = j+1; s < k; s++)  {
		a = olddata->y[s];
		b = olddata->y[s+1];
		wsum += 0.5 * (a+b) * (oldtable->x[s+1] - oldtable->x[s]);
	    }

	    /* Partial interval from k to newx[i+1] */
	    a = olddata->y[k];
	    b = bn_table_lin_interp(olddata, newtable->x[i+1]);	/* in "k" bin */
	    wsum += 0.5 * (a+b) * (newtable->x[i+1] - oldtable->x[k]);

	    /* Adjust the weighted sum by the total width */
	    newsamp->y[i] =
		wsum / (newtable->x[i+1] - newtable->x[i]);
	}
    }
    return newsamp;
}

int
bn_table_write(const char *filename, const struct bn_table *tabp)
{
    FILE *fp;
    size_t j;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_write(%s, %p)\n", filename, (void *)tabp);

    BN_CK_TABLE(tabp);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fp = fopen(filename, "wb");
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (fp == NULL)  {
	perror(filename);
	bu_log("bn_table_write(%s, %p) FAILED\n", filename, (void *)tabp);
	return -1;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fprintf(fp, "  %lu sample starts, and one end.\n", (long unsigned int)tabp->nx);
    for (j=0; j <= tabp->nx; j++)  {
	fprintf(fp, "%g\n", tabp->x[j]);
    }
    fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;
}

struct bn_table *
bn_table_read(const char *filename) {
    int ret;
    struct bn_table *tabp;
    struct bu_vls line = BU_VLS_INIT_ZERO;
    FILE *fp;
    size_t nw;
    size_t j;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_read(%s)\n", filename);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fp = fopen(filename, "rb");
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (fp == NULL)  {
	perror(filename);
	bu_log("bn_table_read(%s) FAILED\n", filename);
	return NULL;
    }

    if (bu_vls_gets(&line, fp) < 0) {
	perror(filename);
	bu_log("Failed to read line\n");
	fclose(fp);
	return NULL;
    }
    nw = 0;
    /* TODO: %lu to size_t isn't right. We may need a bu_sscanf() that can cope
     * with native pointer width correctly, as %p is not ubiquitous and %z is a
     * C99-ism */
    sscanf(bu_vls_addr(&line), "%lu", (long unsigned int *)(&nw));
    bu_vls_free(&line);

    if (nw <= 0) bu_bomb("bn_table_read() bad nw value\n");

    BN_GET_TABLE(tabp, nw);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    for (j=0; j <= tabp->nx; j++)  {
	double val;
	ret = fscanf(fp, "%lf", &val);
	if (ret != 1) {
	    bu_log("bn_table_read(%s) READ FAILURE. Abort\n", filename);
	    break;
	}
	tabp->x[j] = val;
    }
    fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);

    bn_ck_table(tabp);

    return tabp;
}

void
bn_pr_table(const char *title, const struct bn_table *tabp)
{
    size_t j;

    bu_log("%s\n", title);
    BN_CK_TABLE(tabp);

    for (j=0; j <= tabp->nx; j++)  {
	bu_log("%3zu: %g\n", j, tabp->x[j]);
    }
}

void
bn_pr_tabdata(const char *title, const struct bn_tabdata *data)
{
    size_t j;

    bu_log("%s: ", title);
    BN_CK_TABDATA(data);

    for (j=0; j < data->ny; j++)  {
	bu_log("%g, ", data->y[j]);
    }
    bu_log("\n");
}

int
bn_print_table_and_tabdata(const char *filename, const struct bn_tabdata *data)
{
    FILE	*fp;
    const struct bn_table	*tabp;
    size_t j;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_print_table_and_tabdata(%s, %p)\n", filename, (void *)data);

    BN_CK_TABDATA(data);
    tabp = data->table;
    BN_CK_TABLE(tabp);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fp = fopen(filename, "wb");
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (fp == NULL)  {
	perror(filename);
	bu_log("bn_print_table_and_tabdata(%s, %p) FAILED\n", filename, (void *)data);
	return -1;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    for (j=0; j < tabp->nx; j++)  {
	fprintf(fp, "%g %g\n", tabp->x[j], data->y[j]);
    }
    fprintf(fp, "%g (novalue)\n", tabp->x[tabp->nx]);
    fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;
}

struct bn_tabdata *
bn_read_table_and_tabdata(const char *filename) {
    struct bn_table	*tabp;
    struct bn_tabdata	*data;
    FILE	*fp;
    char	buf[128];
    size_t count = 0;
    size_t i;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_read_table_and_tabdata(%s)\n", filename);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fp = fopen(filename, "rb");
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (fp == NULL)  {
	perror(filename);
	bu_log("bn_read_table_and_tabdata(%s) FAILED. Couldn't open file.\n", filename);
	return NULL;
    }

    /* First pass:  Count number of lines */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    for (;;)  {
	if (bu_fgets(buf, sizeof(buf), fp) == NULL)  break;
	count++;
    }
    fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (count < 2)  {
	perror(filename);
	bu_log("bn_read_table_and_tabdata(%s) FAILED. Expected at least 2 lines in file.\n", filename);
	return NULL;
    }

    /* Allocate storage */
    BN_GET_TABLE(tabp, count);
    BN_GET_TABDATA(data, tabp);

    /* Second pass:  Read only as much data as storage was allocated for */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fp = fopen(filename, "rb");
    for (i=0; i < count; i++) {
	double xval, yval;
	int ret;

	buf[0] = '\0';
	if (bu_fgets(buf, sizeof(buf), fp) == NULL)  {
	    bu_log("bn_read_table_and_tabdata(%s) unexpected EOF on line %zu\n", filename, i);
	    break;
	}
	ret = sscanf(buf, "%lf %lf", &xval, &yval);
	if (ret != 2) {
	    bu_log("Malformatted table data encountered in %s\n", filename);
	    break;
	}
	xval = tabp->x[i];
	yval = data->y[i];
    }
    fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);

    /* Complete final interval */
    tabp->x[count] = 2 * tabp->x[count-1] - tabp->x[count-2];

    bn_ck_table(tabp);

    return data;
}

struct bn_tabdata *
bn_tabdata_binary_read(const char *filename, size_t num, const struct bn_table *tabp) {
    struct bn_tabdata	*data;
    char	*cp;
    int	nbytes;
    int	len;
    int	got;
    int	fd;
    long i;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_binary_read(%s, num=%zu, %p)\n", filename, num, (void *)tabp);

    BN_CK_TABLE(tabp);

    nbytes = BN_SIZEOF_TABDATA(tabp);
    len = num * nbytes;

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fd = open(filename, 0);
    bu_semaphore_release(BU_SEM_SYSCALL);
    if (fd <= 0)  {
	perror(filename);
	bu_log("bn_tabdata_binary_read open failed on \"%s\"\n", filename);
	return (struct bn_tabdata *)NULL;
    }

    /* Get a big array of structures for reading all at once */
    data = (struct bn_tabdata *)bu_malloc(len+8, "bn_tabdata[]");

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    got = read(fd, (char *)data, len);
    bu_semaphore_release(BU_SEM_SYSCALL);
    if (got != len)  {
	if (got < 0) {
	    perror(filename);
	    bu_log("bn_tabdata_binary_read read error on \"%s\"\n", filename);
	} else {
	    bu_log("bn_tabdata_binary_read(%s) expected %d got %d\n", filename, len, got);
	}
	bu_free(data, "bn_tabdata[]");
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	close(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);
	return (struct bn_tabdata *)NULL;
    }
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    close(fd);
    bu_semaphore_release(BU_SEM_SYSCALL);

    /* Connect data[i].table pointer to tabp */
    cp = (char *)data;
    for (i = num-1; i >= 0; i--, cp += nbytes)  {
	register struct bn_tabdata *sp;
	sp = (struct bn_tabdata *)cp;
	BN_CK_TABDATA(sp);
	sp->table = tabp;
    }

    return data;
}

struct bn_tabdata *
bn_tabdata_malloc_array(const struct bn_table *tabp, size_t num) {
    struct bn_tabdata *data;
    char *cp;
    size_t i;
    size_t nw;
    size_t nbytes;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_malloc_array(%p, num=%zu)\n", (void *)tabp, num);

    BN_CK_TABLE(tabp);
    nw = tabp->nx;
    nbytes = BN_SIZEOF_TABDATA(tabp);

    data = (struct bn_tabdata *)bu_calloc(num,
					  nbytes, "struct bn_tabdata[]");

    cp = (char *)data;
    for (i = 0; i < num; i++) {
	register struct bn_tabdata	*sp;

	sp = (struct bn_tabdata *)cp;
	sp->magic = BN_TABDATA_MAGIC;
	sp->ny = nw;
	sp->table = tabp;
	cp += nbytes;
    }
    return data;
}

void
bn_tabdata_copy(struct bn_tabdata *out, const struct bn_tabdata *in)
{
    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_copy(%p, %p)\n", (void *)out, (void *)in);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in);

    if (in->table != out->table)
	bu_bomb("bn_tabdata_copy(): samples drawn from different tables\n");
    if (in->ny != out->ny)
	bu_bomb("bn_tabdata_copy(): different tabdata lengths?\n");

    memcpy((char *)out->y, (const char *)in->y, BN_SIZEOF_TABDATA_Y(in));
}

struct bn_tabdata *
bn_tabdata_dup(const struct bn_tabdata *in) {
    struct bn_tabdata *data;

    BN_CK_TABDATA(in);
    BN_GET_TABDATA(data, in->table);

    memcpy((char *)data->y, (const char *)in->y, BN_SIZEOF_TABDATA_Y(in));

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_dup(%p) = %p\n", (void *)in, (void *)data);
    return data;
}

struct bn_tabdata *
bn_tabdata_get_constval(double val, const struct bn_table *tabp) {
    struct bn_tabdata	*data;
    int			todo;
    register fastf_t	*op;

    BN_CK_TABLE(tabp);
    BN_GET_TABDATA(data, tabp);

    op = data->y;
    for (todo = data->ny-1; todo >= 0; todo--)
	*op++ = val;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_get_constval(val=%g, %p)=%p\n", val, (void *)tabp, (void *)data);

    return data;
}

void
bn_tabdata_constval(struct bn_tabdata *data, double val)
{
    int			todo;
    register fastf_t	*op;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_constval(%p, val=%g)\n", (void *)data, val);

    BN_CK_TABDATA(data);

    op = data->y;
    for (todo = data->ny-1; todo >= 0; todo--)
	*op++ = val;
}

void
bn_tabdata_to_tcl(struct bu_vls *vp, const struct bn_tabdata *data)
{
    const struct bn_table	*tabp;
    register size_t i;
    register fastf_t	minval = MAX_FASTF, maxval = -MAX_FASTF;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_to_tcl(%p, %p)\n", (void *)vp, (void *)data);

    BU_CK_VLS(vp);
    BN_CK_TABDATA(data);
    tabp = data->table;
    BN_CK_TABLE(tabp);

    bu_vls_strcat(vp, "x {");
    for (i=0; i < tabp->nx; i++)  {
	bu_vls_printf(vp, "%g ", tabp->x[i]);
    }
    bu_vls_strcat(vp, "} y {");
    for (i=0; i < data->ny; i++)  {
	register fastf_t val = data->y[i];
	bu_vls_printf(vp, "%g ", val);
	if (val < minval)  minval = val;
	if (val > maxval)  maxval = val;
    }
    bu_vls_printf(vp, "} nx %zu ymin %g ymax %g",
		  tabp->nx, minval, maxval);
}

struct bn_tabdata *
bn_tabdata_from_array(const double *array) {
    register const double *dp;
    size_t len = 0;
    struct bn_table *tabp;
    struct bn_tabdata *data;
    register size_t i;

    /* First, find len */
    for (dp = array; *dp > 0; dp += 2)
	; /* NIL */
    len = (dp - array) >> 1;

    /* Second, build bn_table */
    BN_GET_TABLE(tabp, len);
    for (i = 0; i < len; i++)  {
	tabp->x[i] = array[i<<1];
    }
    tabp->x[len] = tabp->x[len-1] + 1;	/* invent span end */

    /* Third, build bn_tabdata (last input "y" is ignored) */
    BN_GET_TABDATA(data, tabp);
    for (i = 0; i < len-1; i++)  {
	data->y[i] = array[(i<<1)+1];
    }

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_from_array(%p) = %p\n", (void *)array, (void *)data);
    return data;
}

void
bn_tabdata_freq_shift(struct bn_tabdata *out, const struct bn_tabdata *in, double offset)
{
    const struct bn_table	*tabp;
    register size_t i;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_freq_shift(%p, %p, offset=%g)\n", (void *)out, (void *)in, offset);

    BN_CK_TABDATA(out);
    BN_CK_TABDATA(in);
    tabp = in->table;

    if (tabp != out->table)
	bu_bomb("bn_tabdata_freq_shift(): samples drawn from different tables\n");
    if (in->ny != out->ny)
	bu_bomb("bn_tabdata_freq_shift(): different tabdata lengths?\n");

    for (i=0; i < out->ny; i++) {
	out->y[i] = bn_table_lin_interp(in, tabp->x[i]+offset);
    }
}

int
bn_table_interval_num_samples(const struct bn_table *tabp, double low, double hi)
{
    register size_t i;
    register size_t count = 0;

    BN_CK_TABLE(tabp);

    for (i=0; i < tabp->nx-1; i++)  {
	if (tabp->x[i] >= low && tabp->x[i] <= hi)  count++;
    }

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_interval_num_samples(%p, low=%g, hi=%g) = %zu\n", (void *)tabp, low, hi, count);
    return count;
}

int
bn_table_delete_sample_pts(struct bn_table *tabp, unsigned int i, unsigned int j)
{
    size_t tokill;
    unsigned int k;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_delete_samples(%p, %d, %d)\n", (void *)tabp, i, j);

    BN_CK_TABLE(tabp);

    if (i >= tabp->nx || j >= tabp->nx)
	bu_bomb("bn_table_delete_sample_pts() index out of range\n");

    tokill = j - i + 1;
    if (tokill < 1)  bu_bomb("bn_table_delete_sample_pts(): nothing to delete\n");
    if (tokill >= tabp->nx) bu_bomb("bn_table_delete_sample_pts(): you can't kill 'em all!\n");

    tabp->nx -= tokill;

    for (k = i; k < tabp->nx; k++)  {
	tabp->x[k] = tabp->x[k+tokill];
    }
    return tokill;
}

struct bn_table *
bn_table_merge2(const struct bn_table *a, const struct bn_table *b) {
    struct bn_table *newtab;
    register size_t i, j, k;

    BN_CK_TABLE(a);
    BN_CK_TABLE(b);

    BN_GET_TABLE(newtab, a->nx + b->nx + 2);

    i = j = 0;		/* input subscripts */
    k = 0;			/* output subscript */
    while (i <= a->nx || j <= b->nx)  {
	if (i > a->nx)  {
	    while (j <= b->nx)
		newtab->x[k++] = b->x[j++];
	    break;
	}
	if (j > b->nx)  {
	    while (i <= a->nx)
		newtab->x[k++] = a->x[i++];
	    break;
	}
	/* Both have remaining elements, take lower one */
	if (ZERO(a->x[i] - b->x[j]))  {
	    newtab->x[k++] = a->x[i++];
	    j++;		/* compress out duplicate */
	    continue;
	}
	if (a->x[i] <= b->x[j])  {
	    newtab->x[k++] = a->x[i++];
	} else {
	    newtab->x[k++] = b->x[j++];
	}
    }
    if (k > newtab->nx)  bu_bomb("bn_table_merge2() assertion failed, k>nx?\n");
    newtab->nx = k-1;

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_merge2(%p, %p) = %p\n", (void *)a, (void *)b, (void *)newtab);
    return newtab;
}

struct bn_tabdata *
bn_tabdata_mk_linear_filter(const struct bn_table *spectrum, double lower_wavelen, double upper_wavelen) {
    struct bn_tabdata *filt;
    int	first;
    int	last;
    fastf_t filt_range;
    fastf_t	cell_range;
    fastf_t	frac;
    size_t i;

    BN_CK_TABLE(spectrum);

    if (bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mk_linear_filter(%p, low=%g, up=%g)\n", (void *)spectrum, lower_wavelen, upper_wavelen);

    if (lower_wavelen < spectrum->x[0])
	if (upper_wavelen > spectrum->x[spectrum->nx])
	    bu_log("bn_tabdata_mk_linear_filter() warning, upper_wavelen %g > highest sampled wavelen %g\n",
		   upper_wavelen, spectrum->x[spectrum->nx]);

    /* First, find first (possibly partial) sample */
    first = bn_table_find_x(spectrum, lower_wavelen);
    if (first == -1)  {
	first = 0;
	bu_log("bn_tabdata_mk_linear_filter() warning, lower_wavelen %g < lowest sampled wavelen %g\n",
	       lower_wavelen, spectrum->x[0]);
    } else if (first <= -2)  {
	bu_log("bn_tabdata_mk_linear_filter() ERROR, lower_wavelen %g > highest sampled wavelen %g\n",
	       lower_wavelen, spectrum->x[spectrum->nx]);
	return NULL;
    }

    /* Second, find last (possibly partial) sample */
    last = bn_table_find_x(spectrum, upper_wavelen);
    if (last == -1)  {
	bu_log("bn_tabdata_mk_linear_filter() ERROR, upper_wavelen %g < lowest sampled wavelen %g\n",
	       upper_wavelen, spectrum->x[0]);
	return NULL;
    } else if (last <= -2)  {
	last = spectrum->nx-1;
	bu_log("bn_tabdata_mk_linear_filter() warning, upper_wavelen %g > highest sampled wavelen %g\n",
	       upper_wavelen, spectrum->x[spectrum->nx]);
    }

    /* 'filt' is filled with zeros by default */
    BN_GET_TABDATA(filt, spectrum);

    /* Special case:  first and last are in same sample cell */
    if (first == last)  {
	filt_range = upper_wavelen - lower_wavelen;
	cell_range = spectrum->x[first+1] - spectrum->x[first];
	frac = filt_range / cell_range;

	/* Could use a BU_ASSERT_RANGE_FLOAT */
	BU_ASSERT((frac >= 0.0) && (frac <= 1.0));

	filt->y[first] = frac;
	return filt;
    }

    /* Calculate fraction 0..1.0 for first and last samples */
    filt_range = spectrum->x[first+1] - lower_wavelen;
    cell_range = spectrum->x[first+1] - spectrum->x[first];
    frac = filt_range / cell_range;
    if (frac > 1)  frac = 1;
    BU_ASSERT((frac >= 0.0) && (frac <= 1.0));
    filt->y[first] = frac;

    filt_range = upper_wavelen - spectrum->x[last];
    cell_range = spectrum->x[last+1] - spectrum->x[last];
    frac = filt_range / cell_range;
    if (frac > 1)  frac = 1;
    BU_ASSERT((frac >= 0.0) && (frac <= 1.0));
    filt->y[last] = frac;

    /* Fill in range between with 1.0 values */
    for (i = first+1; i < (size_t)last; i++)
	filt->y[i] = 1.0;

    return filt;
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
