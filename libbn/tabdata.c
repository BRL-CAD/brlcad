/*
 *			T A B D A T A . C
 *
 *  Routines for processing tables (curves) of data with one independent
 *  parameter which is common to many sets of dependent data values.
 *
 *  Operates on rt_table (independent var) and
 *  rt_tabdata (dependent variable) structures.
 *
 *  One application is for storing spectral curves, see spectrum.c
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  Inspired by -
 *	Roy Hall and his book "Illumination and Color in Computer
 *	Generated Imagery", Springer Verlag, New York, 1989.
 *	ISBN 0-387-96774-5
 *
 *  With thanks to Russ Moulton Jr, EOSoft Inc. for his "rad.c" module.
 */
#ifndef lint
static char RCStabdata[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "tabdata.h"
#include "./debug.h"

/*
 *			R T _ T A B L E _ F R E E
 */
void
rt_table_free( tabp )
struct rt_table	*tabp;
{
	RT_CK_TABLE(tabp);

	tabp->nx = 0;			/* sanity */
	bu_free( tabp, "struct rt_table");
}

/*
 *			R T _ T A B D A T A _ F R E E
 */
void
rt_tabdata_free( data )
struct rt_tabdata *data;
{
	RT_CK_TABDATA(data);
	RT_CK_TABLE(data->table);

	data->ny = 0;			/* sanity */
	data->table = NULL;		/* sanity */
	bu_free( data, "struct rt_tabdata" );
}

/*
 *			R T _ C K _ T A B L E
 */
void
rt_ck_table( tabp )
CONST struct rt_table	*tabp;
{
	register int	i;

	RT_CK_TABLE(tabp);

	if( tabp->nx < 2 ) rt_bomb("rt_ck_table() less than 2 wavelengths\n");

	for( i=0; i < tabp->nx; i++ )  {
		if( tabp->x[i] >= tabp->x[i+1] )
			rt_bomb("rt_ck_table() wavelengths not in strictly ascending order\n");
	}
}

/*
 *			R T _ T A B L E _ M A K E _ U N I F O R M
 *
 *  Set up an independent "table margin" from 'first' to 'last',
 *  inclusive, using 'num' uniformly spaced samples.  Num >= 1.
 */
struct rt_table *
rt_table_make_uniform( num, first, last )
int	num;
double	first;
double	last;
{
	struct rt_table	*tabp;
	fastf_t			*fp;
	fastf_t			delta;
	int			j;

	if( first >= last )  rt_bomb("rt_table_make_uniform() first >= last\n");

	RT_GET_TABLE( tabp, num );

	delta = (last - first) / (double)num;

	fp = &tabp->x[0];
	for( j = num; j > 0; j-- )  {
		*fp++ = first;
		first += delta;
	}
	tabp->x[num] = last;

	return( tabp );
}

/*
 *			R T _ T A B D A T A _ A D D
 *
 *  Sum the values from two data tables.
 */
void
rt_tabdata_add( out, in1, in2 )
struct rt_tabdata		*out;
CONST struct rt_tabdata	*in1;
CONST struct rt_tabdata	*in2;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in1 );
	RT_CK_TABDATA( in2 );

	if( in1->table != in2->table || in1->table != out->table )
		rt_bomb("rt_tabdata_add(): samples drawn from different tables\n");
	if( in1->ny != in2->ny || in1->ny != out->ny )
		rt_bomb("rt_tabdata_add(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ + *i2++;
	/* VADD2N( out->y, i1->y, i2->y, in1->ny ); */
}

/*
 *			R T _ T A B D A T A _ M U L
 *
 *  Element-by-element multiply the values from two data tables.
 */
void
rt_tabdata_mul( out, in1, in2 )
struct rt_tabdata		*out;
CONST struct rt_tabdata	*in1;
CONST struct rt_tabdata	*in2;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in1 );
	RT_CK_TABDATA( in2 );

	if( in1->table != in2->table || in1->table != out->table )
		rt_bomb("rt_tabdata_mul(): samples drawn from different tables\n");
	if( in1->ny != in2->ny || in1->ny != out->ny )
		rt_bomb("rt_tabdata_mul(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ * *i2++;
	/* VELMUL2N( out->y, i1->y, i2->y, in1->ny ); */
}

/*
 *			R T _ T A B D A T A _ S C A L E
 *
 *  Multiply every element in a data table by a scalar value 'scale'.
 */
void
rt_tabdata_scale( out, in1, scale )
struct rt_tabdata		*out;
CONST struct rt_tabdata	*in1;
register double			scale;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1;

	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in1 );

	if( in1->table != out->table )
		rt_bomb("rt_tabdata_scale(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		rt_bomb("rt_tabdata_scale(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ * scale;
	/* VSCALEN( out->y, in->y, scale ); */
}

/*
 *			R T _ T A B L E _ S C A L E
 *
 *  Scale the indepentent axis of a table by 'scale'.
 */
void
rt_table_scale( tabp, scale )
struct rt_table	*tabp;
register double		scale;
{
	register int		j;
	register fastf_t	*op, *i1;

	RT_CK_TABLE( tabp );

	op = tabp->x;
	for( j = tabp->nx+1; j > 0; j-- )
		*op++ *= scale;
	/* VSCALEN( tabp->x, tabp->x, scale, tabp->nx+1 ); */
}

/*
 *			R T _ T A B D A T A _ J O I N 1
 *
 *  Multiply every element in data table in2 by a scalar value 'scale',
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
void
rt_tabdata_join1( out, in1, scale, in2 )
struct rt_tabdata		*out;
CONST struct rt_tabdata		*in1;
register double			scale;
CONST struct rt_tabdata		*in2;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in1 )
	RT_CK_TABDATA( in2 );

	if( in1->table != out->table )
		rt_bomb("rt_tabdata_join1(): samples drawn from different tables\n");
	if( in1->table != in2->table )
		rt_bomb("rt_tabdata_join1(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		rt_bomb("rt_tabdata_join1(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ + scale * *i2++;
	/* VJOIN1N( out->y, in1->y, scale, in2->y ); */
}

/*
 *			R T _ T A B D A T A _ J O I N 2
 *
 *  Multiply every element in data table in2 by a scalar value 'scale2',
 *  plus in3 * scale3, and
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
void
rt_tabdata_join2( out, in1, scale2, in2, scale3, in3 )
struct rt_tabdata		*out;
CONST struct rt_tabdata		*in1;
register double			scale2;
CONST struct rt_tabdata		*in2;
register double			scale3;
CONST struct rt_tabdata		*in3;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2, *i3;

	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in1 )
	RT_CK_TABDATA( in2 );

	if( in1->table != out->table )
		rt_bomb("rt_tabdata_join1(): samples drawn from different tables\n");
	if( in1->table != in2->table )
		rt_bomb("rt_tabdata_join1(): samples drawn from different tables\n");
	if( in1->table != in3->table )
		rt_bomb("rt_tabdata_join1(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		rt_bomb("rt_tabdata_join1(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	i3 = in3->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ + scale2 * *i2++ + scale3 * *i3++;
	/* VJOIN2N( out->y, in1->y, scale2, in2->y, scale3, in3->y ); */
}

/*
 *			R T _ T A B D A T A _ B L E N D 3
 */
void
rt_tabdata_blend3( out, scale1, in1, scale2, in2, scale3, in3 )
struct rt_tabdata		*out;
register double			scale1;
CONST struct rt_tabdata		*in1;
register double			scale2;
CONST struct rt_tabdata		*in2;
register double			scale3;
CONST struct rt_tabdata		*in3;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2, *i3;

	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in1 )
	RT_CK_TABDATA( in2 );
	RT_CK_TABDATA( in3 );

	if( in1->table != out->table )
		rt_bomb("rt_tabdata_blend3(): samples drawn from different tables\n");
	if( in1->table != in2->table )
		rt_bomb("rt_tabdata_blend3(): samples drawn from different tables\n");
	if( in1->table != in3->table )
		rt_bomb("rt_tabdata_blend3(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		rt_bomb("rt_tabdata_blend3(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	i3 = in3->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = scale1 * *i1++ + scale2 * *i2++ + scale3 * *i3++;
	/* VBLEND3N( out->y, scale1, in1->y, scale2, in2->y, scale3, in3->y ); */
}

/*
 *			R T _ T A B D A T A _ A R E A 1
 *
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 */
double
rt_tabdata_area1( in )
CONST struct rt_tabdata	*in;
{
	FAST fastf_t		area;
	register CONST fastf_t	*ip;
	register int		j;

	RT_CK_TABDATA(in);

	area = 0;
	ip = in->y;
	for( j = in->ny; j > 0; j-- )
		area += *ip++;

	return area;
}

/*
 *			R T _ T A B D A T A _ A R E A 2
 *
 *  Following interpretation #2, where y[j] stores the average
 *  value for the interval, return the area under
 *  the whole curve.  Since the iterval spacing need not be uniform,
 *  sum the areas of the rectangles.
 */
double
rt_tabdata_area2( in )
CONST struct rt_tabdata	*in;
{
	CONST struct rt_table	*tabp;
	FAST fastf_t		area;
	fastf_t			width;
	register int		j;

	RT_CK_TABDATA(in);
	tabp = in->table;
	RT_CK_TABLE(tabp);

	area = 0;
	for( j = in->ny-1; j >= 0; j-- )  {
		width = tabp->x[j+1] - tabp->x[j];
		area += in->y[j] * width;
	}

	return area;
}

/*
 *			R T _ T A B D A T A _ M U L _ A R E A 1
 *
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
double
rt_tabdata_mul_area1( in1, in2 )
CONST struct rt_tabdata	*in1;
CONST struct rt_tabdata	*in2;
{
	FAST fastf_t		area;
	register CONST fastf_t	*i1, *i2;
	register int		j;

	RT_CK_TABDATA(in1);
	RT_CK_TABDATA(in2);

	area = 0;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		area += *i1++ * *i2++;

	return area;
}

/*
 *			R T _ T A B D A T A _ M U L _ A R E A 2
 *
 *  Following interpretation #2,
 *  return the area under the whole curve.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
double
rt_tabdata_mul_area2( in1, in2 )
CONST struct rt_tabdata	*in1;
CONST struct rt_tabdata	*in2;
{
	CONST struct rt_table	*tabp;
	FAST fastf_t		area;
	fastf_t			width;
	register int		j;

	RT_CK_TABDATA(in1);
	RT_CK_TABDATA(in2);
	tabp = in1->table;
	RT_CK_TABLE(tabp);

	area = 0;
	for( j = in1->ny-1; j >= 0; j-- )  {
		width = tabp->x[j+1] - tabp->x[j];
		area += in1->y[j] * in2->y[j] * width;
	}
	return area;
}

/*
 *			R T _ T A B L E _ F I N D _ X
 *
 *  Return the index in the table's x[] array of the interval which
 *  contains 'xval'.
 *
 *  Returns -1 if less than start value, -2 if greater than end value.
 *
 *  A binary search would be more efficient, as the wavelengths (x values)
 *  are known to be sorted in ascending order.
 */
int
rt_table_find_x( tabp, xval )
CONST struct rt_table	*tabp;
double			xval;
{
	register int	i;

	RT_CK_TABLE(tabp);

	if( xval >= tabp->x[tabp->nx-1] )  return -2;

	/* Search for proper interval in input spectrum */
	for( i = tabp->nx-2; i >=0; i-- )  {
		if( xval >= tabp->x[i] )  return i;
	}
	/* if( xval < tabp->x[0] )  return -1; */
	return -1;
}

/*
 *			R T _ T A B L E _ L I N _ I N T E R P
 *
 *  Return the value of the curve at independent parameter value 'wl'.
 *  Linearly interpolate between values in the input table.
 *  Zero is returned for values outside the sampled range.
 */
fastf_t
rt_table_lin_interp( samp, wl )
CONST struct rt_tabdata	*samp;
register double			wl;
{
	CONST struct rt_table	*tabp;
	register int		i;
	register fastf_t	fract;
	register fastf_t	ret;

	RT_CK_TABDATA(samp);
	tabp = samp->table;
	RT_CK_TABLE(tabp);

	if( (i = rt_table_find_x( tabp, wl )) < 0 )  {
		if(rt_g.debug&DEBUG_MATH)bu_log("rt_table_lin_interp(%g) out of range %g to %g\n", wl, tabp->x[0], tabp->x[tabp->nx] );
		return 0;
	}

	if( wl < tabp->x[i] || wl >= tabp->x[i+1] )  {
		rt_log("rt_table_lin_interp(%g) assertion1 failed at %g\n", wl, tabp->x[i] );
		rt_bomb("rt_table_lin_interp() assertion1 failed\n");
	}

	if( i >= tabp->nx-2 )  {
		/* Assume value is constant in final interval. */
		if(rt_g.debug&DEBUG_MATH)bu_log("rt_table_lin_interp(%g)=%g off end of range %g to %g\n", wl, samp->y[tabp->nx-1], tabp->x[0], tabp->x[tabp->nx] );
		return samp->y[tabp->nx-1];
	}

	/* The interval has been found */
	fract = (wl - tabp->x[i]) / (tabp->x[i+1] - tabp->x[i]);
	if( fract < 0 || fract > 1 )  rt_bomb("rt_table_lin_interp() assertion2 failed\n");
	ret = (1-fract) * samp->y[i] + fract * samp->y[i+1];
	if(rt_g.debug&DEBUG_MATH)bu_log("rt_table_lin_interp(%g)=%g in range %g to %g\n",
		wl, ret, tabp->x[i], tabp->x[i+1] );
	return ret;
}

/*
 *			R T _ T A B D A T A _ R E S A M P L E _ M A X
 *
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  maximum when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
struct rt_tabdata *
rt_tabdata_resample_max( newtable, olddata )
CONST struct rt_table	*newtable;
CONST struct rt_tabdata	*olddata;
{
	CONST struct rt_table	*oldtable;
	struct rt_tabdata	*newsamp;
	int			i;
	int			j, k;

	RT_CK_TABLE(newtable);
	RT_CK_TABDATA(olddata);
	oldtable = olddata->table;
	RT_CK_TABLE(oldtable);

	if( oldtable == newtable )  rt_log("rt_tabdata_resample_max() NOTICE old and new rt_table structs are the same\n");

	RT_GET_TABDATA( newsamp, newtable );

	for( i = 0; i < newtable->nx; i++ )  {
		/*
		 *  Find good value(s) in olddata to represent the span from
		 *  newtable->x[i] to newtable->x[i+1].
		 */
		j = rt_table_find_x( oldtable, newtable->x[i] );
		k = rt_table_find_x( oldtable, newtable->x[i+1] );
		if( k == -1 )  {
			/* whole new span is off left side of old table */
			newsamp->y[i] = 0;
			continue;
		}
		if( j == -2 )  {
			/* whole new span is off right side of old table */
			newsamp->y[i] = 0;
			continue;
		}

		if( j == k && j > 0 )  {
			register fastf_t tmp;
			/*
			 *  Simple case, ends of output span are completely
			 *  contained within one input span.
			 *  Interpolate for both ends, take max.
			 *  XXX this could be more efficiently written inline here.
			 */
			newsamp->y[i] = rt_table_lin_interp( olddata, newtable->x[i] );
			tmp = rt_table_lin_interp( olddata, newtable->x[i+1] );
			if( tmp > newsamp->y[i] )  newsamp->y[i] = tmp;
		} else {
			register fastf_t tmp, n;
			register int	s;
			/*
			 *  Complex case: find good representative value.
			 *  Interpolate both ends, and consider all
			 *  intermediate old samples in span.  Take max.
			 *  One (but not both) new ends may be off old table.
			 */
			n = rt_table_lin_interp( olddata, newtable->x[i] );
			tmp = rt_table_lin_interp( olddata, newtable->x[i+1] );
			if( tmp > n )  n = tmp;
			for( s = j+1; s <= k; s++ )  {
				if( (tmp = olddata->y[s]) > n )
					n = tmp;
			}
			newsamp->y[i] = n;
		}
	}
	return newsamp;
}

/*
 *			R T _ T A B D A T A _ R E S A M P L E _ A V G
 *
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  average when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
struct rt_tabdata *
rt_tabdata_resample_avg( newtable, olddata )
CONST struct rt_table	*newtable;
CONST struct rt_tabdata	*olddata;
{
	CONST struct rt_table	*oldtable;
	struct rt_tabdata	*newsamp;
	int			i;
	int			j, k;

	RT_CK_TABLE(newtable);
	RT_CK_TABDATA(olddata);
	oldtable = olddata->table;
	RT_CK_TABLE(oldtable);

	if( oldtable == newtable )  rt_log("rt_tabdata_resample_avg() NOTICE old and new rt_table structs are the same\n");

	RT_GET_TABDATA( newsamp, newtable );

	for( i = 0; i < newtable->nx; i++ )  {
		/*
		 *  Find good value(s) in olddata to represent the span from
		 *  newtable->x[i] to newtable->x[i+1].
		 */
		j = rt_table_find_x( oldtable, newtable->x[i] );
		k = rt_table_find_x( oldtable, newtable->x[i+1] );

		if( j < 0 || k < 0 || j == k )  {
			/*
			 *  Simple case, ends of output span are completely
			 *  contained within one input span.
			 *  Interpolate for both ends, take average.
			 *  XXX this could be more efficiently written inline here.
			 */
			newsamp->y[i] = 0.5 * (
			    rt_table_lin_interp( olddata, newtable->x[i] ) +
			    rt_table_lin_interp( olddata, newtable->x[i+1] ) );
		} else {
			/*
			 *  Complex case: find average value.
			 *  Interpolate both end, and consider all
			 *  intermediate old spans.
			 *  There are three parts to sum:
			 *	Partial interval from newx[i] to j+1
			 *	Full intervals from j+1 to k
			 *	Partial interval from k to newx[i+1]
			 */
			fastf_t	w_frac;
			fastf_t wsum;		/* weighted sum */
			fastf_t	a,b;		/* values being averaged */
			int	s;

			/* Partial interval from newx[i] to j+1 */
			a = rt_table_lin_interp( olddata, newtable->x[i] );	/* in "j" bin */
			b = olddata->y[j+1];
			wsum = 0.5 * (a+b) * (oldtable->x[j+1] - newtable->x[i] );

			/* Full intervals from j+1 to k */
			for( s = j+1; s < k; s++ )  {
				a = olddata->y[s];
				b = olddata->y[s+1];
				wsum += 0.5 * (a+b) * (oldtable->x[s+1] - oldtable->x[s] );
			}

			/* Partial interval from k to newx[i+1] */
			a = olddata->y[k];
			b = rt_table_lin_interp( olddata, newtable->x[i+1] );	/* in "k" bin */
			wsum += 0.5 * (a+b) * (newtable->x[i+1] - oldtable->x[k] );

			/* Adjust the weighted sum by the total width */
			newsamp->y[i] =
				wsum / (newtable->x[i+1] - newtable->x[i]);
		}
	}
	return newsamp;
}

/*
 *			R T _ T A B L E _ W R I T E 
 *
 *  Write out the table structure in an ASCII file,
 *  giving the number of values (minus 1), and the
 *  actual values.
 */
int
rt_table_write( filename, tabp )
CONST char	*filename;
CONST struct rt_table	*tabp;
{
	FILE	*fp;
	int	j;

	RT_CK_TABLE(tabp);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "w" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_table_write(%s, x%x) FAILED\n", filename, tabp);
		return -1;
	}

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fprintf(fp, "  %d sample starts, and one end.\n", tabp->nx );
	for( j=0; j <= tabp->nx; j++ )  {
		fprintf( fp, "%g\n", tabp->x[j] );
	}
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );
	return 0;
}

/*
 *			R T _ T A B L E _ R E A D
 *
 *  Allocate and read in the independent variable values from an ASCII file,
 *  giving the number of samples (minus 1), and the
 *  actual values.
 */
struct rt_table *
rt_table_read( filename )
CONST char	*filename;
{
	struct rt_table	*tabp;
	struct bu_vls		line;
	FILE	*fp;
	int	nw;
	int	j;

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "r" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_table_read(%s) FAILED\n", filename);
		return NULL;
	}

	bu_vls_init(&line);
	bu_vls_gets( &line, fp );
	nw = 0;
	sscanf( bu_vls_addr(&line), "%d", &nw );
	bu_vls_free(&line);

	if( nw <= 0 ) rt_bomb("rt_table_read() bad nw value\n");

	RT_GET_TABLE( tabp, nw );

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	for( j=0; j <= tabp->nx; j++ )  {
		/* XXX assumes fastf_t == double */
		fscanf( fp, "%lf", &tabp->x[j] );
	}
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );

	rt_ck_table( tabp );

	return tabp;
}

/*
 *			R T _ P R _ T A B L E
 */
void
rt_pr_table( title, tabp )
CONST char		*title;
CONST struct rt_table	*tabp;
{
	int	j;

	bu_log("rt_pr_table(%s):\n", title);
	RT_CK_TABLE(tabp);

	for( j=0; j <= tabp->nx; j++ )  {
		bu_log("%3d: %g\n", j, tabp->x[j] );
	}
}

/*
 *			R T _ P R _ T A B D A T A
 */
void
rt_pr_tabdata( title, data )
CONST char		*title;
CONST struct rt_tabdata	*data;
{
	int	j;

	bu_log("rt_pr_tabdata(%s): ", title);
	RT_CK_TABDATA(data);

	for( j=0; j < data->ny; j++ )  {
		bu_log("%g, ", data->y[j] );
	}
	bu_log("\n");
}

/*
 *			R T _ P R _ T A B L E _ A N D _ T A B D A T A
 *
 *  Write out a given data table into an ASCII file,
 *  suitable for input to GNUPLOT.
 *	(set term postscript)
 *	(set output "|print-postscript")
 *	(plot "filename" with lines)
 */
int
rt_pr_table_and_tabdata( filename, data )
CONST char			*filename;
CONST struct rt_tabdata	*data;
{
	FILE	*fp;
	CONST struct rt_table	*tabp;
	int	j;

	RT_CK_TABDATA(data);
	tabp = data->table;
	RT_CK_TABLE(tabp);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "w" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_pr_table_and_tabdata(%s, x%x) FAILED\n", filename, data );
		return -1;
	}

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	for( j=0; j < tabp->nx; j++ )  {
		fprintf( fp, "%g %g\n", tabp->x[j], data->y[j] );
	}
	fprintf( fp, "%g (novalue)\n", tabp->x[tabp->nx] );
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );
	return 0;
}

/*
 *			R T _ R E A D _ T A B L E _ A N D _ T A B D A T A
 *
 *  Read in a file which contains two columns of numbers, the first
 *  column being the waveength, the second column being the sample value
 *  at that wavelength.
 *  A new rt_table structure and one rt_tabdata structure
 *  are created, a pointer to the rt_tabdata structure is returned.
 *  The final wavelength is guessed at.
 */
struct rt_tabdata *
rt_read_table_and_tabdata( filename )
CONST char	*filename;
{
	struct rt_table	*tabp;
	struct rt_tabdata	*data;
	struct bu_vls		line;
	FILE	*fp;
	char	buf[128];
	int	count = 0;
	int	i;

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "r" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_read_table_and_tabdata(%s) FAILED\n", filename);
		return NULL;
	}

	/* First pass:  Count number of lines */
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	for(;;)  {
		if( fgets( buf, sizeof(buf), fp ) == NULL )  break;
		count++;
	}
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );

	/* Allocate storage */
	RT_GET_TABLE( tabp, count );
	RT_GET_TABDATA( data, tabp );

	/* Second pass:  Read only as much data as storage was allocated for */
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "r" );
	for( i=0; i < count; i++ )  {
		buf[0] = '\0';
		if( fgets( buf, sizeof(buf), fp ) == NULL )  {
			rt_log("rt_read_table_and_tabdata(%s) unexpected EOF on line %d\n", filename, i);
			break;
		}
		sscanf( buf, "%lf %lf", &tabp->x[i], &data->y[i] );
	}
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );

	/* Complete final interval */
	tabp->x[count] = 2 * tabp->x[count-1] - tabp->x[count-2];

	rt_ck_table( tabp );

	return data;
}

/*
 *			R T _ T A B D A T A _ B I N A R Y _ R E A D
 */
struct rt_tabdata *
rt_tabdata_binary_read( filename, num, tabp )
CONST char			*filename;
int				num;
CONST struct rt_table	*tabp;
{
	struct rt_tabdata	*data;
	char	*cp;
	int	nbytes;
	int	len;
	int	got;
	int	fd;
	int	i;

	RT_CK_TABLE(tabp);

	nbytes = RT_SIZEOF_TABDATA(tabp);
	len = num * nbytes;

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fd = open(filename, 0);
	bu_semaphore_release( BU_SEM_SYSCALL );
	if( fd <= 0 )  {
		perror(filename);
		bu_log("rt_tabdata_binary_read(%s): %m\n", filename);
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		close(fd);
		bu_semaphore_release( BU_SEM_SYSCALL );
		return (struct rt_tabdata *)NULL;
	}

	/* Get a big array of structures for reading all at once */
	data = (struct rt_tabdata *)bu_malloc( len+8, "rt_tabdata[]" );

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	got = read( fd, (char *)data, len );
	bu_semaphore_release( BU_SEM_SYSCALL );
	if( got != len )  {
		bu_log("rt_tabdata_binary_read(%s) expected %d got %d\n",
			filename, len, got);
		bu_free( data, "rt_tabdata[]" );
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		close(fd);
		bu_semaphore_release( BU_SEM_SYSCALL );
		return (struct rt_tabdata *)NULL;
	}
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	close(fd);
	bu_semaphore_release( BU_SEM_SYSCALL );

	/* Connect data[i].table pointer to tabp */
	cp = (char *)data;
	for( i = num-1; i >= 0; i--, cp += nbytes )  {
		register struct rt_tabdata	*sp;

		sp = (struct rt_tabdata *)cp;
		RT_CK_TABDATA(sp);
		sp->table = tabp;
	}

	return data;
}

/*
 *			R T _ T A B D A T A _ M A L L O C _ A R R A Y
 *
 *  Allocate storage for, and initialize, an array of 'num' data table
 *  structures.
 *  This subroutine is provided because the rt_tabdata structures
 *  are variable length.
 */
struct rt_tabdata *
rt_tabdata_malloc_array( tabp, num )
CONST struct rt_table	*tabp;
int	num;
{
	struct rt_tabdata	*data;
	char	*cp;
	int	i;
	int	nw;
	int	nbytes;

	RT_CK_TABLE(tabp);
	nw = tabp->nx;
	nbytes = RT_SIZEOF_TABDATA(tabp);

	data = (struct rt_tabdata *)rt_calloc( num,
		nbytes, "struct rt_tabdata[]" );

	cp = (char *)data;
	for( i = 0; i < num; i++ ) {
		register struct rt_tabdata	*sp;

		sp = (struct rt_tabdata *)cp;
		sp->magic = RT_TABDATA_MAGIC;
		sp->ny = nw;
		sp->table = tabp;
		cp += nbytes;
	}
	return data;
}

/*
 *			R T _ T A B D A T A _ C O P Y
 */
void
rt_tabdata_copy( out, in )
struct rt_tabdata		*out;
CONST struct rt_tabdata	*in;
{
	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in );

	if( in->table != out->table )
		rt_bomb("rt_tabdata_copy(): samples drawn from different tables\n");
	if( in->ny != out->ny )
		rt_bomb("rt_tabdata_copy(): different tabdata lengths?\n");

	bcopy( (char *)in->y, (char *)out->y, RT_SIZEOF_TABDATA(in->table) );
}

/*
 *			R T _ T A B D A T A _ D U P
 */
struct rt_tabdata *
rt_tabdata_dup( in )
CONST struct rt_tabdata	*in;
{
	struct rt_tabdata *data;

	RT_CK_TABDATA( in );
	RT_GET_TABDATA( data, in->table );

	bcopy( (char *)in->y, (char *)data->y, RT_SIZEOF_TABDATA(in->table) );
	return data;
}

/*
 *			R T _ T A B D A T A _ G E T _ C O N S T V A L
 *
 *  For a given table, allocate and return a tabdata structure
 *  with all elements initialized to 'val'.
 */
struct rt_tabdata *
rt_tabdata_get_constval( val, tabp )
double			val;
CONST struct rt_table	*tabp;
{
	struct rt_tabdata	*data;
	int			todo;
	register fastf_t	*op;

	RT_CK_TABLE(tabp);
	RT_GET_TABDATA( data, tabp );

	op = data->y;
	for( todo = data->ny-1; todo >= 0; todo-- )
		*op++ = val;

	return data;
}

/*
 *			R T _ T A B D A T A _ C O N S T V A L
 *
 *  Set all the tabdata elements to 'val'
 */
void
rt_tabdata_constval( data, val )
struct rt_tabdata	*data;
double			val;
{
	int			todo;
	register fastf_t	*op;

	RT_CK_TABDATA(data);

	op = data->y;
	for( todo = data->ny-1; todo >= 0; todo-- )
		*op++ = val;
}

/*
 *			R T _ T A B D A T A _ T O _ T C L
 *
 *  Convert an rt_tabdata/rt_table pair into a Tcl compatible string
 *  appended to a VLS.  It will have form:
 *	x {...} y {...} nx # ymin # ymax #
 */
void
rt_tabdata_to_tcl( vp, data )
struct bu_vls		*vp;
CONST struct rt_tabdata	*data;
{
	CONST struct rt_table	*tabp;
	register int i;
	FAST fastf_t	minval = INFINITY, maxval = -INFINITY;

	BU_CK_VLS(vp);
	RT_CK_TABDATA(data);
	tabp = data->table;
	RT_CK_TABLE(tabp);

	bu_vls_strcat(vp, "x {");
	for( i=0; i < tabp->nx; i++ )  {
		bu_vls_printf( vp, "%g ", tabp->x[i] );
	}
	bu_vls_strcat(vp, "} y {");
	for( i=0; i < data->ny; i++ )  {
		register fastf_t val = data->y[i];
		bu_vls_printf( vp, "%g ", val );
		if( val < minval )  minval = val;
		if( val > maxval )  maxval = val;
	}
	bu_vls_printf( vp, "} nx %d ymin %g ymax %g",
		tabp->nx, minval, maxval );
}

/*
 *			R T _ T A B D A T A _ F R O M _ A R R A Y
 *
 *  Given an array of (x,y) pairs, build the relevant rt_table and
 *  rt_tabdata structures.
 *  The table is terminated by an x value <= 0.
 *  Consistent with the interpretation of the spans,
 *  invent a final span ending x value.
 */
struct rt_tabdata *
rt_tabdata_from_array( array )
CONST double *array;
{
	register CONST double	*dp;
	int			len = 0;
	struct rt_table		*tabp;
	struct rt_tabdata	*data;
	register int		i;

	/* First, find len */
	for( dp = array; *dp > 0; dp += 2 )	/* NIL */ ;
	len = (dp - array) >> 1;

	/* Second, build rt_table */
	RT_GET_TABLE( tabp, len );
	for( i = 0; i < len; i++ )  {
		tabp->x[i] = array[i<<1];
	}
	tabp->x[len] = tabp->x[len-1] + 1;	/* invent span end */

	/* Third, build rt_tabdata (last input "y" is ignored) */
	RT_GET_TABDATA( data, tabp );
	for( i = 0; i < len-1; i++ )  {
		data->y[i] = array[(i<<1)+1];
	}
	return data;
}

/*
 *			R T _ T A B D A T A _ F R E Q _ S H I F T
 *
 *  Shift the data by a constant offset in the independent variable
 *  (often frequency), interpolating new sample values.
 */
void
rt_tabdata_freq_shift( out, in, offset )
struct rt_tabdata		*out;
CONST struct rt_tabdata		*in;
double				offset;
{
	CONST struct rt_table	*tabp;
	register int 		i;

	RT_CK_TABDATA( out );
	RT_CK_TABDATA( in );
	tabp = in->table;

	if( tabp != out->table )
		rt_bomb("rt_tabdata_freq_shift(): samples drawn from different tables\n");
	if( in->ny != out->ny )
		rt_bomb("rt_tabdata_freq_shift(): different tabdata lengths?\n");

	for( i=0; i < out->ny; i++ ) {
		out->y[i] = rt_table_lin_interp( in, tabp->x[i]+offset );
	}
}

/*
 *			R T _ T A B L E _ I N T E R V A L _ N U M _ S A M P L E S
 *
 *  Returns number of sample points between 'low' and 'hi', inclusive.
 */
int
rt_table_interval_num_samples( tabp, low, hi )
CONST struct rt_table *tabp;
double	low;
double	hi;
{
	register int	i;
	register int	count = 0;

	RT_CK_TABLE(tabp);

	for( i=0; i < tabp->nx-1; i++ )  {
		if( tabp->x[i] >= low && tabp->x[i] <= hi )  count++;
	}
	return count;
}

/*
 *			R T _ T A B L E _ D E L E T E _ S A M P L E _ P T S
 *
 *  Remove all sampling points between subscripts i and j, inclusive.
 *  Don't bother freeing the tiny bit of storage at the end of the array.
 */
int
rt_table_delete_sample_pts( tabp, i, j )
struct rt_table *tabp;
int	i;
int	j;
{
	int	tokill;
	int	k;

	RT_CK_TABLE(tabp);

	if( i < 0 || j < 0 )  bu_bomb("rt_table_delete_sample_pts() negative indices\n");
	if( i >= tabp->nx || j >= tabp->nx )  bu_bomb("rt_table_delete_sample_pts() index out of range\n");

	tokill = j - i + 1;
	if( tokill < 1 )  bu_bomb("rt_table_delete_sample_pts(): nothing to delete\n");
	if( tokill >= tabp->nx ) bu_bomb("rt_table_delete_sample_pts(): you can't kill 'em all!\n");

	tabp->nx -= tokill;

	for( k = i; k < tabp->nx; k++ )  {
		tabp->x[k] = tabp->x[k+tokill];
	}
	return tokill;
}

/*
 *			R T _ T A B L E _ M E R G E 2
 *
 *  A new table is returned which has sample points at places from
 *  each of the input tables.
 */
struct rt_table *
rt_table_merge2( a, b )
CONST struct rt_table	*a;
CONST struct rt_table	*b;
{
	struct rt_table *new;
	register int i, j, k;

	RT_CK_TABLE(a);
	RT_CK_TABLE(b);

	RT_GET_TABLE(new, a->nx + b->nx + 2 );

	i = j = 0;		/* input subscripts */
	k = 0;			/* output subscript */
	while( i <= a->nx || j <= b->nx )  {
		if( i > a->nx )  {
			while( j <= b->nx )
				new->x[k++] = b->x[j++];
			break;
		}
		if( j > b->nx )  {
			while( i <= a->nx )
				new->x[k++] = a->x[i++];
			break;
		}
		/* Both have remaining elements, take lower one */
		if( a->x[i] == b->x[j] )  {
			new->x[k++] = a->x[i++];
			j++;		/* compress out duplicate */
			continue;
		}
		if( a->x[i] <= b->x[j] )  {
			new->x[k++] = a->x[i++];
		} else {
			new->x[k++] = b->x[j++];
		}
	}
	if( k > new->nx )  bu_bomb("rt_table_merge2() assertion failed, k>nx?\n");
	new->nx = k-1;

	return new;
}
