
/*
 *			T A B D A T A . C
 *
 *  Routines for processing tables (curves) of data with one independent
 *  parameter which is common to many sets of dependent data values.
 *
 *  Operates on bn_table (independent var) and
 *  bn_tabdata (dependent variable) structures.
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
#include "bn.h"

/*
 *			B N _ T A B L E _ F R E E
 */
void
bn_table_free( tabp )
struct bn_table	*tabp;
{
	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_free(x%x)\n", tabp);
	BN_CK_TABLE(tabp);

	tabp->nx = 0;			/* sanity */
	bu_free( tabp, "struct bn_table");
}

/*
 *			B N _ T A B D A T A _ F R E E
 */
void
bn_tabdata_free( data )
struct bn_tabdata *data;
{
	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_free(x%x)\n", data);

	BN_CK_TABDATA(data);
	BN_CK_TABLE(data->table);

	data->ny = 0;			/* sanity */
	data->table = NULL;		/* sanity */
	bu_free( data, "struct bn_tabdata" );
}

/*
 *			B N _ C K _ T A B L E
 */
void
bn_ck_table( tabp )
CONST struct bn_table	*tabp;
{
	register int	i;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_ck_table(x%x)\n", tabp);

	BN_CK_TABLE(tabp);

	if( tabp->nx < 2 ) bu_bomb("bn_ck_table() less than 2 wavelengths\n");

	for( i=0; i < tabp->nx; i++ )  {
		if( tabp->x[i] >= tabp->x[i+1] )
			bu_bomb("bn_ck_table() wavelengths not in strictly ascending order\n");
	}
}

/*
 *			B N _ T A B L E _ M A K E _ U N I F O R M
 *
 *  Set up an independent "table margin" from 'first' to 'last',
 *  inclusive, using 'num' uniformly spaced samples.  Num >= 1.
 */
struct bn_table *
bn_table_make_uniform( num, first, last )
int	num;
double	first;
double	last;
{
	struct bn_table	*tabp;
	fastf_t			*fp;
	fastf_t			delta;
	int			j;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_make_uniform( num=%d, %g, %g )\n", num, first, last );

	if( first >= last )  bu_bomb("bn_table_make_uniform() first >= last\n");

	BN_GET_TABLE( tabp, num );

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
 *			B N _ T A B D A T A _ A D D
 *
 *  Sum the values from two data tables.
 */
void
bn_tabdata_add( out, in1, in2 )
struct bn_tabdata		*out;
CONST struct bn_tabdata	*in1;
CONST struct bn_tabdata	*in2;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_add(x%x, x%x, x%x)\n", out, in1, in2);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );

	if( in1->table != in2->table || in1->table != out->table )
		bu_bomb("bn_tabdata_add(): samples drawn from different tables\n");
	if( in1->ny != in2->ny || in1->ny != out->ny )
		bu_bomb("bn_tabdata_add(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ + *i2++;
	/* VADD2N( out->y, i1->y, i2->y, in1->ny ); */
}

/*
 *			B N _ T A B D A T A _ M U L
 *
 *  Element-by-element multiply the values from two data tables.
 */
void
bn_tabdata_mul( out, in1, in2 )
struct bn_tabdata		*out;
CONST struct bn_tabdata	*in1;
CONST struct bn_tabdata	*in2;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul(x%x, x%x, x%x)\n", out, in1, in2);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );

	if( in1->table != in2->table || in1->table != out->table )
		bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
	if( in1->ny != in2->ny || in1->ny != out->ny )
		bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ * *i2++;
	/* VELMUL2N( out->y, i1->y, i2->y, in1->ny ); */
}

/*
 *			B N _ T A B D A T A _ M U L 3
 *
 *  Element-by-element multiply the values from three data tables.
 */
void
bn_tabdata_mul3( out, in1, in2, in3 )
struct bn_tabdata		*out;
CONST struct bn_tabdata	*in1;
CONST struct bn_tabdata	*in2;
CONST struct bn_tabdata	*in3;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2, *i3;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul3(x%x, x%x, x%x, x%x)\n", out, in1, in2, in3);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );
	BN_CK_TABDATA( in3 );

	if( in1->table != in2->table || in1->table != out->table || in1->table != in2->table )
		bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
	if( in1->ny != in2->ny || in1->ny != out->ny )
		bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	i3 = in3->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ * *i2++ * *i3++;
	/* VELMUL3N( out->y, i1->y, i2->y, i3->y, in1->ny ); */
}

/*
 *			B N _ T A B D A T A _ I N C R _ M U L 3 _ S C A L E
 *
 *  Element-by-element multiply the values from three data tables and a scalor.
 *
 *	out += in1 * in2 * in3 * scale
 */
void
bn_tabdata_incr_mul3_scale( out, in1, in2, in3, scale )
struct bn_tabdata	*out;
CONST struct bn_tabdata	*in1;
CONST struct bn_tabdata	*in2;
CONST struct bn_tabdata	*in3;
register double		scale;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2, *i3;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_incr_mul3_scale(x%x, x%x, x%x, x%x, %g)\n", out, in1, in2, in3, scale);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );
	BN_CK_TABDATA( in3 );

	if( in1->table != in2->table || in1->table != out->table || in1->table != in3->table )
		bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
	if( in1->ny != in2->ny || in1->ny != out->ny )
		bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	i3 = in3->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ += *i1++ * *i2++ * *i3++ * scale;
}

/*
 *			B N _ T A B D A T A _ I N C R _ M U L 2 _ S C A L E
 *
 *  Element-by-element multiply the values from two data tables and a scalor.
 *
 *	out += in1 * in2 * scale
 */
void
bn_tabdata_incr_mul2_scale( out, in1, in2, scale )
struct bn_tabdata	*out;
CONST struct bn_tabdata	*in1;
CONST struct bn_tabdata	*in2;
register double		scale;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_incr_mul2_scale(x%x, x%x, x%x, %g)\n", out, in1, in2, scale);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );

	if( in1->table != in2->table || in1->table != out->table )
		bu_bomb("bn_tabdata_mul(): samples drawn from different tables\n");
	if( in1->ny != in2->ny || in1->ny != out->ny )
		bu_bomb("bn_tabdata_mul(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ += *i1++ * *i2++ * scale;
}

/*
 *			B N _ T A B D A T A _ S C A L E
 *
 *  Multiply every element in a data table by a scalar value 'scale'.
 */
void
bn_tabdata_scale( out, in1, scale )
struct bn_tabdata		*out;
CONST struct bn_tabdata	*in1;
register double			scale;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_scale(x%x, x%x, %g)\n", out, in1, scale);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );

	if( in1->table != out->table )
		bu_bomb("bn_tabdata_scale(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		bu_bomb("bn_tabdata_scale(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ * scale;
	/* VSCALEN( out->y, in->y, scale ); */
}

/*
 *			B N _ T A B L E _ S C A L E
 *
 *  Scale the indepentent axis of a table by 'scale'.
 */
void
bn_table_scale( tabp, scale )
struct bn_table	*tabp;
register double		scale;
{
	register int		j;
	register fastf_t	*op;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_scale(x%x, %g)\n", tabp, scale );

	BN_CK_TABLE( tabp );

	op = tabp->x;
	for( j = tabp->nx+1; j > 0; j-- )
		*op++ *= scale;
	/* VSCALEN( tabp->x, tabp->x, scale, tabp->nx+1 ); */
}

/*
 *			B N _ T A B D A T A _ J O I N 1
 *
 *  Multiply every element in data table in2 by a scalar value 'scale',
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
void
bn_tabdata_join1( out, in1, scale, in2 )
struct bn_tabdata		*out;
CONST struct bn_tabdata		*in1;
register double			scale;
CONST struct bn_tabdata		*in2;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_join1(x%x, x%x, %g, x%x)\n", out, in1, scale, in2 );

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );

	if( in1->table != out->table )
		bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
	if( in1->table != in2->table )
		bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		bu_bomb("bn_tabdata_join1(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ + scale * *i2++;
	/* VJOIN1N( out->y, in1->y, scale, in2->y ); */
}

/*
 *			B N _ T A B D A T A _ J O I N 2
 *
 *  Multiply every element in data table in2 by a scalar value 'scale2',
 *  plus in3 * scale3, and
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
void
bn_tabdata_join2( out, in1, scale2, in2, scale3, in3 )
struct bn_tabdata		*out;
CONST struct bn_tabdata		*in1;
register double			scale2;
CONST struct bn_tabdata		*in2;
register double			scale3;
CONST struct bn_tabdata		*in3;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2, *i3;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_join2(x%x, x%x, %g, x%x, %g, x%x)\n", out, in1, scale2, in2, scale3, in3 );

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );

	if( in1->table != out->table )
		bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
	if( in1->table != in2->table )
		bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
	if( in1->table != in3->table )
		bu_bomb("bn_tabdata_join1(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		bu_bomb("bn_tabdata_join1(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	i3 = in3->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = *i1++ + scale2 * *i2++ + scale3 * *i3++;
	/* VJOIN2N( out->y, in1->y, scale2, in2->y, scale3, in3->y ); */
}

/*
 *			B N _ T A B D A T A _ B L E N D 2
 */
void
bn_tabdata_blend2( out, scale1, in1, scale2, in2 )
struct bn_tabdata		*out;
register double			scale1;
CONST struct bn_tabdata		*in1;
register double			scale2;
CONST struct bn_tabdata		*in2;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_blend2(x%x, %g, x%x, %g, x%x)\n", out, scale1, in1, scale2, in2 );

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );

	if( in1->table != out->table )
		bu_bomb("bn_tabdata_blend2(): samples drawn from different tables\n");
	if( in1->table != in2->table )
		bu_bomb("bn_tabdata_blend2(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		bu_bomb("bn_tabdata_blend2(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = scale1 * *i1++ + scale2 * *i2++;
	/* VBLEND2N( out->y, scale1, in1->y, scale2, in2->y ); */
}

/*
 *			B N _ T A B D A T A _ B L E N D 3
 */
void
bn_tabdata_blend3( out, scale1, in1, scale2, in2, scale3, in3 )
struct bn_tabdata		*out;
register double			scale1;
CONST struct bn_tabdata		*in1;
register double			scale2;
CONST struct bn_tabdata		*in2;
register double			scale3;
CONST struct bn_tabdata		*in3;
{
	register int		j;
	register fastf_t	*op;
	register CONST fastf_t	*i1, *i2, *i3;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_blend3(x%x, %g, x%x, %g, x%x, %g, x%x)\n", out, scale1, in1, scale2, in2, scale3, in3 );

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in1 );
	BN_CK_TABDATA( in2 );
	BN_CK_TABDATA( in3 );

	if( in1->table != out->table )
		bu_bomb("bn_tabdata_blend3(): samples drawn from different tables\n");
	if( in1->table != in2->table )
		bu_bomb("bn_tabdata_blend3(): samples drawn from different tables\n");
	if( in1->table != in3->table )
		bu_bomb("bn_tabdata_blend3(): samples drawn from different tables\n");
	if( in1->ny != out->ny )
		bu_bomb("bn_tabdata_blend3(): different tabdata lengths?\n");

	op = out->y;
	i1 = in1->y;
	i2 = in2->y;
	i3 = in3->y;
	for( j = in1->ny; j > 0; j-- )
		*op++ = scale1 * *i1++ + scale2 * *i2++ + scale3 * *i3++;
	/* VBLEND3N( out->y, scale1, in1->y, scale2, in2->y, scale3, in3->y ); */
}

/*
 *			B N _ T A B D A T A _ A R E A 1
 *
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 */
double
bn_tabdata_area1( in )
CONST struct bn_tabdata	*in;
{
	FAST fastf_t		area;
	register CONST fastf_t	*ip;
	register int		j;

	BN_CK_TABDATA(in);

	area = 0;
	ip = in->y;
	for( j = in->ny; j > 0; j-- )
		area += *ip++;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_area(x%x) = %g\n", in, area);

	return area;
}

/*
 *			B N _ T A B D A T A _ A R E A 2
 *
 *  Following interpretation #2, where y[j] stores the average
 *  value for the interval, return the area under
 *  the whole curve.  Since the iterval spacing need not be uniform,
 *  sum the areas of the rectangles.
 */
double
bn_tabdata_area2( in )
CONST struct bn_tabdata	*in;
{
	CONST struct bn_table	*tabp;
	FAST fastf_t		area;
	fastf_t			width;
	register int		j;

	BN_CK_TABDATA(in);
	tabp = in->table;
	BN_CK_TABLE(tabp);

	area = 0;
	for( j = in->ny-1; j >= 0; j-- )  {
		width = tabp->x[j+1] - tabp->x[j];
		area += in->y[j] * width;
	}

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_area2(x%x) = %g\n", in, area);
	return area;
}

/*
 *			B N _ T A B D A T A _ M U L _ A R E A 1
 *
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
double
bn_tabdata_mul_area1( in1, in2 )
CONST struct bn_tabdata	*in1;
CONST struct bn_tabdata	*in2;
{
	FAST fastf_t		area;
	register CONST fastf_t	*i1, *i2;
	register int		j;

	BN_CK_TABDATA(in1);
	BN_CK_TABDATA(in2);

	area = 0;
	i1 = in1->y;
	i2 = in2->y;
	for( j = in1->ny; j > 0; j-- )
		area += *i1++ * *i2++;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul_area1(x%x, x%x) = %g\n", in1, in2, area);
	return area;
}

/*
 *			B N _ T A B D A T A _ M U L _ A R E A 2
 *
 *  Following interpretation #2,
 *  return the area under the whole curve.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
double
bn_tabdata_mul_area2( in1, in2 )
CONST struct bn_tabdata	*in1;
CONST struct bn_tabdata	*in2;
{
	CONST struct bn_table	*tabp;
	FAST fastf_t		area;
	fastf_t			width;
	register int		j;

	BN_CK_TABDATA(in1);
	BN_CK_TABDATA(in2);
	tabp = in1->table;
	BN_CK_TABLE(tabp);

	area = 0;
	for( j = in1->ny-1; j >= 0; j-- )  {
		width = tabp->x[j+1] - tabp->x[j];
		area += in1->y[j] * in2->y[j] * width;
	}

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mul_area2(x%x, x%x) = %g\n", in1, in2, area);
	return area;
}

/*
 *			B N _ T A B L E _ F I N D _ X
 *
 *  Return the index in the table's x[] array of the interval which
 *  contains 'xval'.
 *
 *  Returns -
 *	-1	if less than start value
 *	-2	if greater than end value.
 *	0..nx-1	otherwise.
 *		Never returns a value of nx.
 *
 *  A binary search would be more efficient, as the wavelengths (x values)
 *  are known to be sorted in ascending order.
 */
int
bn_table_find_x( tabp, xval )
CONST struct bn_table	*tabp;
double			xval;
{
	register int	i;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_find_x(x%x, %g)\n", tabp, xval );
	BN_CK_TABLE(tabp);

	if( xval > tabp->x[tabp->nx] )  return -2;
	if( xval >= tabp->x[tabp->nx-1] )  return tabp->nx-1;

	/* Search for proper interval in input spectrum */
	for( i = tabp->nx-2; i >=0; i-- )  {
		if( xval >= tabp->x[i] )  return i;
	}
	/* if( xval < tabp->x[0] )  return -1; */
	return -1;
}

/*
 *			B N _ T A B L E _ L I N _ I N T E R P
 *
 *  Return the value of the curve at independent parameter value 'wl'.
 *  Linearly interpolate between values in the input table.
 *  Zero is returned for values outside the sampled range.
 */
fastf_t
bn_table_lin_interp( samp, wl )
CONST struct bn_tabdata	*samp;
register double			wl;
{
	CONST struct bn_table	*tabp;
	register int		i;
	register fastf_t	fract;
	register fastf_t	ret;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_lin_interp(x%x, %g)\n", samp, wl);

	BN_CK_TABDATA(samp);
	tabp = samp->table;
	BN_CK_TABLE(tabp);

	if( (i = bn_table_find_x( tabp, wl )) < 0 )  {
		if(bu_debug&BU_DEBUG_TABDATA)bu_log("bn_table_lin_interp(%g) out of range %g to %g\n", wl, tabp->x[0], tabp->x[tabp->nx] );
		return 0;
	}

	if( wl < tabp->x[i] || wl >= tabp->x[i+1] )  {
		bu_log("bn_table_lin_interp(%g) assertion1 failed at %g\n", wl, tabp->x[i] );
		bu_bomb("bn_table_lin_interp() assertion1 failed\n");
	}

	if( i >= tabp->nx-2 )  {
		/* Assume value is constant in final interval. */
		if(bu_debug&BU_DEBUG_TABDATA)bu_log("bn_table_lin_interp(%g)=%g off end of range %g to %g\n", wl, samp->y[tabp->nx-1], tabp->x[0], tabp->x[tabp->nx] );
		return samp->y[tabp->nx-1];
	}

	/* The interval has been found */
	fract = (wl - tabp->x[i]) / (tabp->x[i+1] - tabp->x[i]);
	if( fract < 0 || fract > 1 )  bu_bomb("bn_table_lin_interp() assertion2 failed\n");
	ret = (1-fract) * samp->y[i] + fract * samp->y[i+1];
	if(bu_debug&BU_DEBUG_TABDATA)bu_log("bn_table_lin_interp(%g)=%g in range %g to %g\n",
		wl, ret, tabp->x[i], tabp->x[i+1] );
	return ret;
}

/*
 *			B N _ T A B D A T A _ R E S A M P L E _ M A X
 *
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  maximum when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
struct bn_tabdata *
bn_tabdata_resample_max( newtable, olddata )
CONST struct bn_table	*newtable;
CONST struct bn_tabdata	*olddata;
{
	CONST struct bn_table	*oldtable;
	struct bn_tabdata	*newsamp;
	int			i;
	int			j, k;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_resample_max(x%x, x%x)\n", newtable, olddata);

	BN_CK_TABLE(newtable);
	BN_CK_TABDATA(olddata);
	oldtable = olddata->table;
	BN_CK_TABLE(oldtable);

	if( oldtable == newtable )  bu_log("bn_tabdata_resample_max() NOTICE old and new bn_table structs are the same\n");

	BN_GET_TABDATA( newsamp, newtable );

	for( i = 0; i < newtable->nx; i++ )  {
		/*
		 *  Find good value(s) in olddata to represent the span from
		 *  newtable->x[i] to newtable->x[i+1].
		 */
		j = bn_table_find_x( oldtable, newtable->x[i] );
		k = bn_table_find_x( oldtable, newtable->x[i+1] );
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
			newsamp->y[i] = bn_table_lin_interp( olddata, newtable->x[i] );
			tmp = bn_table_lin_interp( olddata, newtable->x[i+1] );
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
			n = bn_table_lin_interp( olddata, newtable->x[i] );
			tmp = bn_table_lin_interp( olddata, newtable->x[i+1] );
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
 *			B N _ T A B D A T A _ R E S A M P L E _ A V G
 *
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  average when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
struct bn_tabdata *
bn_tabdata_resample_avg( newtable, olddata )
CONST struct bn_table	*newtable;
CONST struct bn_tabdata	*olddata;
{
	CONST struct bn_table	*oldtable;
	struct bn_tabdata	*newsamp;
	int			i;
	int			j, k;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_resample_avg(x%x, x%x)\n", newtable, olddata);

	BN_CK_TABLE(newtable);
	BN_CK_TABDATA(olddata);
	oldtable = olddata->table;
	BN_CK_TABLE(oldtable);

	if( oldtable == newtable )  bu_log("bn_tabdata_resample_avg() NOTICE old and new bn_table structs are the same\n");

	BN_GET_TABDATA( newsamp, newtable );

	for( i = 0; i < newtable->nx; i++ )  {
		/*
		 *  Find good value(s) in olddata to represent the span from
		 *  newtable->x[i] to newtable->x[i+1].
		 */
		j = bn_table_find_x( oldtable, newtable->x[i] );
		k = bn_table_find_x( oldtable, newtable->x[i+1] );

		if( j < 0 || k < 0 || j == k )  {
			/*
			 *  Simple case, ends of output span are completely
			 *  contained within one input span.
			 *  Interpolate for both ends, take average.
			 *  XXX this could be more efficiently written inline here.
			 */
			newsamp->y[i] = 0.5 * (
			    bn_table_lin_interp( olddata, newtable->x[i] ) +
			    bn_table_lin_interp( olddata, newtable->x[i+1] ) );
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
			fastf_t wsum;		/* weighted sum */
			fastf_t	a,b;		/* values being averaged */
			int	s;

			/* Partial interval from newx[i] to j+1 */
			a = bn_table_lin_interp( olddata, newtable->x[i] );	/* in "j" bin */
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
			b = bn_table_lin_interp( olddata, newtable->x[i+1] );	/* in "k" bin */
			wsum += 0.5 * (a+b) * (newtable->x[i+1] - oldtable->x[k] );

			/* Adjust the weighted sum by the total width */
			newsamp->y[i] =
				wsum / (newtable->x[i+1] - newtable->x[i]);
		}
	}
	return newsamp;
}

/*
 *			B N _ T A B L E _ W R I T E 
 *
 *  Write out the table structure in an ASCII file,
 *  giving the number of values (minus 1), and the
 *  actual values.
 */
int
bn_table_write( filename, tabp )
CONST char	*filename;
CONST struct bn_table	*tabp;
{
	FILE	*fp;
	int	j;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_write(%s, x%x)\n", filename, tabp);

	BN_CK_TABLE(tabp);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "w" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		bu_log("bn_table_write(%s, x%x) FAILED\n", filename, tabp);
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
 *			B N _ T A B L E _ R E A D
 *
 *  Allocate and read in the independent variable values from an ASCII file,
 *  giving the number of samples (minus 1), and the
 *  actual values.
 */
struct bn_table *
bn_table_read( filename )
CONST char	*filename;
{
	struct bn_table	*tabp;
	struct bu_vls		line;
	FILE	*fp;
	int	nw;
	int	j;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_read(%s)\n", filename);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "r" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		bu_log("bn_table_read(%s) FAILED\n", filename);
		return NULL;
	}

	bu_vls_init(&line);
	bu_vls_gets( &line, fp );
	nw = 0;
	sscanf( bu_vls_addr(&line), "%d", &nw );
	bu_vls_free(&line);

	if( nw <= 0 ) bu_bomb("bn_table_read() bad nw value\n");

	BN_GET_TABLE( tabp, nw );

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	for( j=0; j <= tabp->nx; j++ )  {
		/* XXX assumes fastf_t == double */
		fscanf( fp, "%lf", &tabp->x[j] );
	}
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );

	bn_ck_table( tabp );

	return tabp;
}

/*
 *			B N _ P R _ T A B L E
 */
void
bn_pr_table( title, tabp )
CONST char		*title;
CONST struct bn_table	*tabp;
{
	int	j;

	bu_log("%s\n", title);
	BN_CK_TABLE(tabp);

	for( j=0; j <= tabp->nx; j++ )  {
		bu_log("%3d: %g\n", j, tabp->x[j] );
	}
}

/*
 *			B N _ P R _ T A B D A T A
 */
void
bn_pr_tabdata( title, data )
CONST char		*title;
CONST struct bn_tabdata	*data;
{
	int	j;

	bu_log("%s: ", title);
	BN_CK_TABDATA(data);

	for( j=0; j < data->ny; j++ )  {
		bu_log("%g, ", data->y[j] );
	}
	bu_log("\n");
}

/*
 *			B N _ P R I N T _ T A B L E _ A N D _ T A B D A T A
 *
 *  Write out a given data table into an ASCII file,
 *  suitable for input to GNUPLOT.
 *	(set term postscript)
 *	(set output "|print-postscript")
 *	(plot "filename" with lines)
 */
int
bn_print_table_and_tabdata( filename, data )
CONST char			*filename;
CONST struct bn_tabdata	*data;
{
	FILE	*fp;
	CONST struct bn_table	*tabp;
	int	j;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_print_table_and_tabdata(%s, x%x)\n", filename, data);

	BN_CK_TABDATA(data);
	tabp = data->table;
	BN_CK_TABLE(tabp);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "w" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		bu_log("bn_print_table_and_tabdata(%s, x%x) FAILED\n", filename, data );
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
 *			B N _ R E A D _ T A B L E _ A N D _ T A B D A T A
 *
 *  Read in a file which contains two columns of numbers, the first
 *  column being the waveength, the second column being the sample value
 *  at that wavelength.
 *  A new bn_table structure and one bn_tabdata structure
 *  are created, a pointer to the bn_tabdata structure is returned.
 *  The final wavelength is guessed at.
 */
struct bn_tabdata *
bn_read_table_and_tabdata( filename )
CONST char	*filename;
{
	struct bn_table	*tabp;
	struct bn_tabdata	*data;
	FILE	*fp;
	char	buf[128];
	int	count = 0;
	int	i;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_read_table_and_tabdata(%s)\n", filename);

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "r" );
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( fp == NULL )  {
		perror(filename);
		bu_log("bn_read_table_and_tabdata(%s) FAILED\n", filename);
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
	BN_GET_TABLE( tabp, count );
	BN_GET_TABDATA( data, tabp );

	/* Second pass:  Read only as much data as storage was allocated for */
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fp = fopen( filename, "r" );
	for( i=0; i < count; i++ )  {
		buf[0] = '\0';
		if( fgets( buf, sizeof(buf), fp ) == NULL )  {
			bu_log("bn_read_table_and_tabdata(%s) unexpected EOF on line %d\n", filename, i);
			break;
		}
		sscanf( buf, "%lf %lf", &tabp->x[i], &data->y[i] );
	}
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );

	/* Complete final interval */
	tabp->x[count] = 2 * tabp->x[count-1] - tabp->x[count-2];

	bn_ck_table( tabp );

	return data;
}

/*
 *			B N _ T A B D A T A _ B I N A R Y _ R E A D
 */
struct bn_tabdata *
bn_tabdata_binary_read( filename, num, tabp )
CONST char			*filename;
int				num;
CONST struct bn_table	*tabp;
{
	struct bn_tabdata	*data;
	char	*cp;
	int	nbytes;
	int	len;
	int	got;
	int	fd;
	int	i;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_binary_read(%s, num=%d, x%x)\n", filename, num, tabp);

	BN_CK_TABLE(tabp);

	nbytes = BN_SIZEOF_TABDATA(tabp);
	len = num * nbytes;

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fd = open(filename, 0);
	bu_semaphore_release( BU_SEM_SYSCALL );
	if( fd <= 0 )  {
		perror(filename);
		bu_log("bn_tabdata_binary_read(%s): %m\n", filename);
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		close(fd);
		bu_semaphore_release( BU_SEM_SYSCALL );
		return (struct bn_tabdata *)NULL;
	}

	/* Get a big array of structures for reading all at once */
	data = (struct bn_tabdata *)bu_malloc( len+8, "bn_tabdata[]" );

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	got = read( fd, (char *)data, len );
	bu_semaphore_release( BU_SEM_SYSCALL );
	if( got != len )  {
		bu_log("bn_tabdata_binary_read(%s) expected %d got %d\n",
			filename, len, got);
		bu_free( data, "bn_tabdata[]" );
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		close(fd);
		bu_semaphore_release( BU_SEM_SYSCALL );
		return (struct bn_tabdata *)NULL;
	}
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	close(fd);
	bu_semaphore_release( BU_SEM_SYSCALL );

	/* Connect data[i].table pointer to tabp */
	cp = (char *)data;
	for( i = num-1; i >= 0; i--, cp += nbytes )  {
		register struct bn_tabdata	*sp;

		sp = (struct bn_tabdata *)cp;
		BN_CK_TABDATA(sp);
		sp->table = tabp;
	}

	return data;
}

/*
 *			B N _ T A B D A T A _ M A L L O C _ A R R A Y
 *
 *  Allocate storage for, and initialize, an array of 'num' data table
 *  structures.
 *  This subroutine is provided because the bn_tabdata structures
 *  are variable length.
 */
struct bn_tabdata *
bn_tabdata_malloc_array( tabp, num )
CONST struct bn_table	*tabp;
int	num;
{
	struct bn_tabdata	*data;
	char	*cp;
	int	i;
	int	nw;
	int	nbytes;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_malloc_array(x%x, num=%d)\n", tabp, num);

	BN_CK_TABLE(tabp);
	nw = tabp->nx;
	nbytes = BN_SIZEOF_TABDATA(tabp);

	data = (struct bn_tabdata *)bu_calloc( num,
		nbytes, "struct bn_tabdata[]" );

	cp = (char *)data;
	for( i = 0; i < num; i++ ) {
		register struct bn_tabdata	*sp;

		sp = (struct bn_tabdata *)cp;
		sp->magic = BN_TABDATA_MAGIC;
		sp->ny = nw;
		sp->table = tabp;
		cp += nbytes;
	}
	return data;
}

/*
 *			B N _ T A B D A T A _ C O P Y
 */
void
bn_tabdata_copy( out, in )
struct bn_tabdata	*out;
CONST struct bn_tabdata	*in;
{
	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_copy(x%x, x%x)\n", out, in);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in );

	if( in->table != out->table )
		bu_bomb("bn_tabdata_copy(): samples drawn from different tables\n");
	if( in->ny != out->ny )
		bu_bomb("bn_tabdata_copy(): different tabdata lengths?\n");

	bcopy( (CONST char *)in->y, (char *)out->y, BN_SIZEOF_TABDATA_Y(in) );
}

/*
 *			B N _ T A B D A T A _ D U P
 */
struct bn_tabdata *
bn_tabdata_dup( in )
CONST struct bn_tabdata	*in;
{
	struct bn_tabdata *data;

	BN_CK_TABDATA( in );
	BN_GET_TABDATA( data, in->table );

	bcopy( (CONST char *)in->y, (char *)data->y, BN_SIZEOF_TABDATA_Y(in) );

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_dup(x%x) = x%x\n", in, data);
	return data;
}

/*
 *			B N _ T A B D A T A _ G E T _ C O N S T V A L
 *
 *  For a given table, allocate and return a tabdata structure
 *  with all elements initialized to 'val'.
 */
struct bn_tabdata *
bn_tabdata_get_constval( val, tabp )
double			val;
CONST struct bn_table	*tabp;
{
	struct bn_tabdata	*data;
	int			todo;
	register fastf_t	*op;

	BN_CK_TABLE(tabp);
	BN_GET_TABDATA( data, tabp );

	op = data->y;
	for( todo = data->ny-1; todo >= 0; todo-- )
		*op++ = val;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_get_constval(val=%g, x%x)=x%x\n", val, tabp, data);

	return data;
}

/*
 *			B N _ T A B D A T A _ C O N S T V A L
 *
 *  Set all the tabdata elements to 'val'
 */
void
bn_tabdata_constval( data, val )
struct bn_tabdata	*data;
double			val;
{
	int			todo;
	register fastf_t	*op;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_constval(x%x, val=%g)\n", data, val);

	BN_CK_TABDATA(data);

	op = data->y;
	for( todo = data->ny-1; todo >= 0; todo-- )
		*op++ = val;
}

/*
 *			B N _ T A B D A T A _ T O _ T C L
 *
 *  Convert an bn_tabdata/bn_table pair into a Tcl compatible string
 *  appended to a VLS.  It will have form:
 *	x {...} y {...} nx # ymin # ymax #
 */
void
bn_tabdata_to_tcl( vp, data )
struct bu_vls		*vp;
CONST struct bn_tabdata	*data;
{
	CONST struct bn_table	*tabp;
	register int i;
	FAST fastf_t	minval = MAX_FASTF, maxval = -MAX_FASTF;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_to_tcl(x%x, x%x)\n", vp, data);

	BU_CK_VLS(vp);
	BN_CK_TABDATA(data);
	tabp = data->table;
	BN_CK_TABLE(tabp);

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
 *			B N _ T A B D A T A _ F R O M _ A R R A Y
 *
 *  Given an array of (x,y) pairs, build the relevant bn_table and
 *  bn_tabdata structures.
 *  The table is terminated by an x value <= 0.
 *  Consistent with the interpretation of the spans,
 *  invent a final span ending x value.
 */
struct bn_tabdata *
bn_tabdata_from_array( array )
CONST double *array;
{
	register CONST double	*dp;
	int			len = 0;
	struct bn_table		*tabp;
	struct bn_tabdata	*data;
	register int		i;

	/* First, find len */
	for( dp = array; *dp > 0; dp += 2 )	/* NIL */ ;
	len = (dp - array) >> 1;

	/* Second, build bn_table */
	BN_GET_TABLE( tabp, len );
	for( i = 0; i < len; i++ )  {
		tabp->x[i] = array[i<<1];
	}
	tabp->x[len] = tabp->x[len-1] + 1;	/* invent span end */

	/* Third, build bn_tabdata (last input "y" is ignored) */
	BN_GET_TABDATA( data, tabp );
	for( i = 0; i < len-1; i++ )  {
		data->y[i] = array[(i<<1)+1];
	}

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_from_array(x%x) = x%x\n", array, data);
	return data;
}

/*
 *			B N _ T A B D A T A _ F R E Q _ S H I F T
 *
 *  Shift the data by a constant offset in the independent variable
 *  (often frequency), interpolating new sample values.
 */
void
bn_tabdata_freq_shift( out, in, offset )
struct bn_tabdata		*out;
CONST struct bn_tabdata		*in;
double				offset;
{
	CONST struct bn_table	*tabp;
	register int 		i;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_freq_shift(x%x, x%x, offset=%g)\n", out, in, offset);

	BN_CK_TABDATA( out );
	BN_CK_TABDATA( in );
	tabp = in->table;

	if( tabp != out->table )
		bu_bomb("bn_tabdata_freq_shift(): samples drawn from different tables\n");
	if( in->ny != out->ny )
		bu_bomb("bn_tabdata_freq_shift(): different tabdata lengths?\n");

	for( i=0; i < out->ny; i++ ) {
		out->y[i] = bn_table_lin_interp( in, tabp->x[i]+offset );
	}
}

/*
 *			B N _ T A B L E _ I N T E R V A L _ N U M _ S A M P L E S
 *
 *  Returns number of sample points between 'low' and 'hi', inclusive.
 */
int
bn_table_interval_num_samples( tabp, low, hi )
CONST struct bn_table *tabp;
double	low;
double	hi;
{
	register int	i;
	register int	count = 0;

	BN_CK_TABLE(tabp);

	for( i=0; i < tabp->nx-1; i++ )  {
		if( tabp->x[i] >= low && tabp->x[i] <= hi )  count++;
	}

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_interval_num_samples(x%x, low=%g, hi=%g) = %d\n", tabp, low, hi, count);
	return count;
}

/*
 *			B N _ T A B L E _ D E L E T E _ S A M P L E _ P T S
 *
 *  Remove all sampling points between subscripts i and j, inclusive.
 *  Don't bother freeing the tiny bit of storage at the end of the array.
 *  Returns number of points removed.
 */
int
bn_table_delete_sample_pts( tabp, i, j )
struct bn_table *tabp;
int	i;
int	j;
{
	int	tokill;
	int	k;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_delete_samples(x%x, %d, %d)\n", tabp, i, j);

	BN_CK_TABLE(tabp);

	if( i < 0 || j < 0 )  bu_bomb("bn_table_delete_sample_pts() negative indices\n");
	if( i >= tabp->nx || j >= tabp->nx )  bu_bomb("bn_table_delete_sample_pts() index out of range\n");

	tokill = j - i + 1;
	if( tokill < 1 )  bu_bomb("bn_table_delete_sample_pts(): nothing to delete\n");
	if( tokill >= tabp->nx ) bu_bomb("bn_table_delete_sample_pts(): you can't kill 'em all!\n");

	tabp->nx -= tokill;

	for( k = i; k < tabp->nx; k++ )  {
		tabp->x[k] = tabp->x[k+tokill];
	}
	return tokill;
}

/*
 *			B N _ T A B L E _ M E R G E 2
 *
 *  A new table is returned which has sample points at places from
 *  each of the input tables.
 */
struct bn_table *
bn_table_merge2( a, b )
CONST struct bn_table	*a;
CONST struct bn_table	*b;
{
	struct bn_table *new;
	register int i, j, k;

	BN_CK_TABLE(a);
	BN_CK_TABLE(b);

	BN_GET_TABLE(new, a->nx + b->nx + 2 );

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
	if( k > new->nx )  bu_bomb("bn_table_merge2() assertion failed, k>nx?\n");
	new->nx = k-1;

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_table_merge2(x%x, x%x) = x%x\n", a, b, new);
	return new;
}

/*
 *		B N _ T A B D A T A _ M K _ L I N E A R _ F I L T E R
 *
 *  Create a filter to accept power in a given band.
 *  The first and last filter values will be in the range 0..1,
 *  while all the internal filter values will be 1.0,
 *  and all samples outside the given band will be 0.0.
 *
 *  Returns -
 *	NULL	if given band does not overlap input spectrum
 *	tabdata*
 */
struct bn_tabdata *
bn_tabdata_mk_linear_filter( spectrum, lower_wavelen, upper_wavelen )
CONST struct bn_table *spectrum;
double		lower_wavelen;
double		upper_wavelen;
{
	struct bn_tabdata *filt;
	int	first;
	int	last;
	fastf_t filt_range;
	fastf_t	cell_range;
	fastf_t	frac;
	int	i;

	BN_CK_TABLE(spectrum);

	if(bu_debug&BU_DEBUG_TABDATA) bu_log("bn_tabdata_mk_linear_filter(x%x, low=%g, up=%g)\n", spectrum, lower_wavelen, upper_wavelen);

	if( lower_wavelen < spectrum->x[0] )
	if( upper_wavelen > spectrum->x[spectrum->nx] )
		bu_log("bn_tabdata_mk_linear_filter() warning, upper_wavelen %g > hightest sampled wavelen %g\n",
			upper_wavelen, spectrum->x[spectrum->nx] );

	/* First, find first (possibly partial) sample */
	first = bn_table_find_x( spectrum, lower_wavelen );
	if( first == -1 )  {
		first = 0;
		bu_log("bn_tabdata_mk_linear_filter() warning, lower_wavelen %g < lowest sampled wavelen %g\n",
			lower_wavelen, spectrum->x[0] );
	} else if( first <= -2 )  {
		bu_log("bn_tabdata_mk_linear_filter() ERROR, lower_wavelen %g > highest sampled wavelen %g\n",
			lower_wavelen, spectrum->x[spectrum->nx] );
		return NULL;
	}

	/* Second, find last (possibly partial) sample */
	last = bn_table_find_x( spectrum, upper_wavelen );
	if( last == -1 )  {
		bu_log("bn_tabdata_mk_linear_filter() ERROR, upper_wavelen %g < lowest sampled wavelen %g\n",
			upper_wavelen, spectrum->x[0] );
		return NULL;
	} else if( last <= -2 )  {
		last = spectrum->nx-1;
		bu_log("bn_tabdata_mk_linear_filter() warning, upper_wavelen %g > highest sampled wavelen %g\n",
			upper_wavelen, spectrum->x[spectrum->nx] );
	}

	/* 'filt' is filled with zeros by default */
	BN_GET_TABDATA( filt, spectrum );

	/* Special case:  first and last are in same sample cell */
	if( first == last )  {
		filt_range = upper_wavelen - lower_wavelen;
		cell_range = spectrum->x[first+1] - spectrum->x[first];
		frac = filt_range / cell_range;

		/* Could use a BU_ASSERT_RANGE_FLOAT */
		BU_ASSERT( (frac >= 0.0) && (frac <= 1.0) );

		filt->y[first] = frac;
		return filt;
	}

	/* Calculate fraction 0..1.0 for first and last samples */
	filt_range = spectrum->x[first+1] - lower_wavelen;
	cell_range = spectrum->x[first+1] - spectrum->x[first];
	frac = filt_range / cell_range;
	if( frac > 1 )  frac = 1;
	BU_ASSERT( (frac >= 0.0) && (frac <= 1.0) );
	filt->y[first] = frac;

	filt_range = upper_wavelen - spectrum->x[last];
	cell_range = spectrum->x[last+1] - spectrum->x[last];
	frac = filt_range / cell_range;
	if( frac > 1 )  frac = 1;
	BU_ASSERT( (frac >= 0.0) && (frac <= 1.0) );
	filt->y[last] = frac;

	/* Fill in range between with 1.0 values */
	for( i = first+1; i < last; i++ )
		filt->y[i] = 1.0;

	return filt;
}
