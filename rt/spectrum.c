/*
 *			T A B L E . C
 *			S P E C T R U M . C
    ...about to be split....
 *
 *  This should perhaps be called the "table" package, as it's use is
 *  really much more general than just storing spectral curves.
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
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "spectrum.h"

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
	register fastf_t	*op, *i1, *i2;

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
	register fastf_t	*op, *i1, *i2;

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
	register fastf_t	*op, *i1;

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
	register fastf_t	*ip;
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

/* This is the data for the CIE_XYZ curves take from Judd and
 *  Wyszecki (1975), table 2.6, these are for the 1931 standard
 *  observer with a 2-degree visual field.
 *  From Roy Hall, pg 228.
 */
static CONST double	rt_CIE_XYZ[81][4] = {
    {380, 0.0014, 0.0000, 0.0065}, {385, 0.0022, 0.0001, 0.0105},
    {390, 0.0042, 0.0001, 0.0201}, {395, 0.0076, 0.0002, 0.0362},
    {400, 0.0143, 0.0004, 0.0679}, {405, 0.0232, 0.0006, 0.1102},
    {410, 0.0435, 0.0012, 0.2074}, {415, 0.0776, 0.0022, 0.3713},
    {420, 0.1344, 0.0040, 0.6456}, {425, 0.2148, 0.0073, 1.0391},
    {430, 0.2839, 0.0116, 1.3856}, {435, 0.3285, 0.0168, 1.6230},
    {440, 0.3483, 0.0230, 1.7471}, {445, 0.3481, 0.0298, 1.7826},
    {450, 0.3362, 0.0380, 1.7721}, {455, 0.3187, 0.0480, 1.7441},
    {460, 0.2908, 0.0600, 1.6692}, {465, 0.2511, 0.0739, 1.5281},
    {470, 0.1954, 0.0910, 1.2876}, {475, 0.1421, 0.1126, 1.0419},
    {480, 0.0956, 0.1390, 0.8130}, {485, 0.0580, 0.1693, 0.6162},
    {490, 0.0320, 0.2080, 0.4652}, {495, 0.0147, 0.2586, 0.3533},
    {500, 0.0049, 0.3230, 0.2720}, {505, 0.0024, 0.4073, 0.2123},
    {510, 0.0093, 0.5030, 0.1582}, {515, 0.0291, 0.6082, 0.1117},
    {520, 0.0633, 0.7100, 0.0782}, {525, 0.1096, 0.7932, 0.0573},
    {530, 0.1655, 0.8620, 0.0422}, {535, 0.2257, 0.9149, 0.0298},
    {540, 0.2904, 0.9540, 0.0203}, {545, 0.3597, 0.9803, 0.0134},
    {550, 0.4334, 0.9950, 0.0087}, {555, 0.5121, 1.0000, 0.0057},
    {560, 0.5945, 0.9950, 0.0039}, {565, 0.6784, 0.9786, 0.0027},
    {570, 0.7621, 0.9520, 0.0021}, {575, 0.8425, 0.9154, 0.0018},
    {580, 0.9163, 0.8700, 0.0017}, {585, 0.9786, 0.8163, 0.0014},
    {590, 1.0263, 0.7570, 0.0011}, {595, 1.0567, 0.6949, 0.0010},
    {600, 1.0622, 0.6310, 0.0008}, {605, 1.0456, 0.5668, 0.0006},
    {610, 1.0026, 0.5030, 0.0003}, {615, 0.9384, 0.4412, 0.0002},
    {620, 0.8544, 0.3810, 0.0002}, {625, 0.7514, 0.3210, 0.0001},
    {630, 0.6424, 0.2650, 0.0000}, {635, 0.5419, 0.2170, 0.0000},
    {640, 0.4479, 0.1750, 0.0000}, {645, 0.3608, 0.1382, 0.0000},
    {650, 0.2835, 0.1070, 0.0000}, {655, 0.2187, 0.0816, 0.0000},
    {660, 0.1649, 0.0610, 0.0000}, {665, 0.1212, 0.0446, 0.0000},
    {670, 0.0874, 0.0320, 0.0000}, {675, 0.0636, 0.0232, 0.0000},
    {680, 0.0468, 0.0170, 0.0000}, {685, 0.0329, 0.0119, 0.0000},
    {690, 0.0227, 0.0082, 0.0000}, {695, 0.0158, 0.0057, 0.0000},
    {700, 0.0114, 0.0041, 0.0000}, {705, 0.0081, 0.0029, 0.0000},
    {710, 0.0058, 0.0021, 0.0000}, {715, 0.0041, 0.0015, 0.0000},
    {720, 0.0029, 0.0010, 0.0000}, {725, 0.0020, 0.0007, 0.0000},
    {730, 0.0014, 0.0005, 0.0000}, {735, 0.0010, 0.0004, 0.0000},
    {740, 0.0007, 0.0002, 0.0000}, {745, 0.0005, 0.0002, 0.0000},
    {750, 0.0003, 0.0001, 0.0000}, {755, 0.0002, 0.0001, 0.0000},
    {760, 0.0002, 0.0001, 0.0000}, {765, 0.0001, 0.0000, 0.0000},
    {770, 0.0001, 0.0000, 0.0000}, {775, 0.0001, 0.0000, 0.0000},
    {780, 0.0000, 0.0000, 0.0000}
};

/*
 *			R T _ S P E C T _ M A K E _ C I E _ X Y Z
 *
 *  Given as input a spectral sampling distribution,
 *  generate the 3 curves to match the human eye's response
 *  in CIE color parameters X, Y, and Z.
 *  XYZ space can be readily converted to RGB with a 3x3 matrix.
 *
 *  The tabulated data is linearly interpolated.
 *
 *  Pointers to the three spectral weighting functions are "returned",
 *  storage for the X, Y, and Z curves is allocated by this routine
 *  and must be freed by the caller.
 */
void
rt_spect_make_CIE_XYZ( x, y, z, tabp )
struct rt_tabdata		**x;
struct rt_tabdata		**y;
struct rt_tabdata		**z;
CONST struct rt_table	*tabp;
{
	struct rt_tabdata	*a, *b, *c;
	fastf_t	xyz_scale;
	int	i;
	int	j;

	RT_CK_TABLE(tabp);

	RT_GET_TABDATA( a, tabp );
	RT_GET_TABDATA( b, tabp );
	RT_GET_TABDATA( c, tabp );
	*x = a;
	*y = b;
	*z = c;

	/* No CIE data below 380 nm */
	for( j=0; tabp->x[j] < 380 && j < tabp->nx; j++ )  {
		a->y[j] = b->y[j] = c->y[j] = 0;
	}

	/* Traverse the CIE table.  Produce as many output values as possible
	 * before advancing to next CIE table entry.
	 */
	for( i = 0; i < 81-1; i++ )  {
		FAST fastf_t	fract;		/* fraction from [i] to [i+1] */

again:
		if( j >= tabp->nx )  return;
		if( tabp->x[j] < rt_CIE_XYZ[i][0] ) rt_bomb("rt_spect_make_CIE_XYZ assertion1 failed\n");
		if( tabp->x[j] >= rt_CIE_XYZ[i+1][0] )  continue;
		/* The CIE table has 5nm spacing */
		fract = (tabp->x[j] - rt_CIE_XYZ[i][0] ) / 5;
		if( fract < 0 || fract > 1 )  rt_bomb("rt_spect_make_CIE_XYZ assertion2 failed\n");
		a->y[j] = (1-fract) * rt_CIE_XYZ[i][1] + fract * rt_CIE_XYZ[i+1][1];
		b->y[j] = (1-fract) * rt_CIE_XYZ[i][2] + fract * rt_CIE_XYZ[i+1][2];
		c->y[j] = (1-fract) * rt_CIE_XYZ[i][3] + fract * rt_CIE_XYZ[i+1][3];
		j++;
		goto again;
	}

	/* No CIE data above 780 nm */
	for( ; j < tabp->nx; j++ )  {
		a->y[j] = b->y[j] = c->y[j] = 0;
	}

	/* Normalize the curves so that area under Y curve is 1.0 */
	xyz_scale = rt_tabdata_area2( b );
	if( fabs(xyz_scale) < VDIVIDE_TOL )  {
		rt_log("rt_spect_make_CIE_XYZ(): Area = 0 (no luminance) in this part of the spectrum, skipping normalization step\n");
		return;
	}
	xyz_scale = 1 / xyz_scale;
	rt_tabdata_scale( a, a, xyz_scale );
	rt_tabdata_scale( b, b, xyz_scale );
	rt_tabdata_scale( c, c, xyz_scale );
}

/*
 *  These are the NTSC primaries with D6500 white point for use as
 *  the default initialization as given in sect 5.1.1 Color
 *  Correction for Display.
 *  From Roy Hall, page 228.
 *  Gives the XYZ coordinates of the NTSC primaries and D6500 white.
 *  Note:  X+Y+Z=1 for primaries (cf. equations of pg.54)
 */
CONST static point_t      rgb_NTSC[4] = {
    {0.670,     0.330,      0.000},     /* red */
    {0.210,     0.710,      0.080},     /* green */
    {0.140,     0.080,      0.780},     /* blue */
    {0.313,     0.329,      0.358}};    /* white */

/*
 *  Create the map from 
 *  CIE XYZ perceptual space into
 *  an idealized RGB space assuming NTSC primaries with D6500 white.
 *  Only high-quality television-studio monitors are like this, but...
 */
void
rt_make_ntsc_xyz2rgb( xyz2rgb )
mat_t	xyz2rgb;
{
	mat_t	rgb2xyz;
	point_t	tst, new;

	if( rt_clr__cspace_to_xyz( rgb_NTSC, rgb2xyz ) == 0 )
		rt_bomb("rt_make_ntsc_xyz2rgb() can't initialize color space\n");
	mat_inv( xyz2rgb, rgb2xyz );

#if 0
	/* Verify that it really works, I'm a skeptic */
	VSET( tst, 1, 1, 1 );
	MAT3X3VEC( new, rgb2xyz, tst );
	VPRINT( "white_xyz", new );
	VSET( tst, 0.951368, 1, 1.08815);
	MAT3X3VEC( new, xyz2rgb, tst );
	VPRINT( "white_rgb", new );
#endif
}

/* ****************************************************************
 * clr__cspace_to_xyz (cspace, t_mat)
 *  CLR_XYZ       cspace[4]   (in)  - the color space definition,
 *                                      3 primaries and white
 *  double        t_mat[3][3] (mod) - the color transformation
 *
 * Builds the transformation from a set of primaries to the CIEXYZ
 *  color space.  This is the basis for the generation of the color
 *  transformations in the CLR_ routine set.  The method used is
 *  that detailed in Sect 3.2 Colorimetry and the RGB monitor.
 *  Returns RGB to XYZ matrix.
 *  From Roy Hall, pg 239-240.
 *
 *  The RGB white point of (1,1,1) times this matrix gives the
 *  (Y=1 normalized) XYZ white point of (0.951368, 1, 1.08815)
 *  From Roy Hall, pg 54.
 *	MAT3X3VEC( xyz, rgb2xyz, rgb );
 *
 *  Returns -
 *	0 if there is a singularity.
 *	!0 if OK
 */
int
rt_clr__cspace_to_xyz (cspace, rgb2xyz)
CONST point_t	cspace[4];
mat_t		rgb2xyz;
{
	int     ii, jj, kk, tmp_i, ind[3];
	fastf_t  mult, white[3], scale[3];
	mat_t	t_mat;

	/* Might want to enforce X+Y+Z=1 for 4 inputs.  Roy does, on pg 229. */

	/* normalize the white point to Y=1 */
#define WHITE	3
	if (cspace[WHITE][Y] <= 0.0) return 0;
	white[0] = cspace[WHITE][X] / cspace[WHITE][Y];
	white[1] = 1.0;
	white[2] = cspace[WHITE][Z] / cspace[WHITE][Y];

#define tmat(a,b)	t_mat[(a)*4+(b)]
	mat_idn(t_mat);
	for (ii=0; ii<=2; ii++) {
		tmat(0,ii) = cspace[ii][X];
		tmat(1,ii) = cspace[ii][Y];
		tmat(2,ii) = cspace[ii][Z];
		ind[ii] = ii;
	}

	/* gaussian elimination  with partial pivoting */
	for (ii=0; ii<2; ii++) {
		for (jj=ii+1; jj<=2; jj++)  {
			if (fabs(tmat(ind[jj],ii)) > fabs(tmat(ind[ii],ii))) {
				tmp_i=ind[jj];
				ind[jj]=ind[ii];
				ind[ii]=tmp_i;
			}
		}
		if (tmat(ind[ii],ii) == 0.0) return 0;

		for (jj=ii+1; jj<=2; jj++) {
			mult = tmat(ind[jj],ii) / tmat(ind[ii],ii);
			for (kk=ii+1; kk<=2; kk++)
			tmat(ind[jj],kk) -= tmat(ind[ii],kk) * mult;
			white[ind[jj]] -= white[ind[ii]] * mult;
		}
	}
	if (tmat(ind[2],2) == 0.0) return 0;

	/* back substitution to solve for scale */
	scale[ind[2]] = white[ind[2]] / tmat(ind[2],2);
	scale[ind[1]] = (white[ind[1]] - (tmat(ind[1],2) *
			scale[ind[2]])) / tmat(ind[1],1);
	scale[ind[0]] = (white[ind[0]] - (tmat(ind[0],1) *
			scale[ind[1]]) - (tmat(ind[0],2) *
			scale[ind[2]])) / tmat(ind[0],0);

	/* build matrix.  Embed 3x3 in BRL-CAD 4x4 */
	for (ii=0; ii<=2; ii++) {
		rgb2xyz[0*4+ii] = cspace[ii][X] * scale[ii];
		rgb2xyz[1*4+ii] = cspace[ii][Y] * scale[ii];
		rgb2xyz[2*4+ii] = cspace[ii][Z] * scale[ii];
			rgb2xyz[3*4+ii] = 0;
	}
	rgb2xyz[12] = rgb2xyz[13] = rgb2xyz[14];
	rgb2xyz[15] = 1;

	return 1;
}


/*
 *			R T _ T A B L E _ L I N _ I N T E R P
 *
 *  Return the value of the spectral curve at wavelength 'wl'.
 *  Linearly interpolate between values in the input table.
 *  Zero is returned for values outside the sampled range.
 *
 *  A binary search would be more efficient, as the wavelengths
 *  are known to be sorted in ascending order.
 */
fastf_t
rt_table_lin_interp( samp, wl )
CONST struct rt_tabdata	*samp;
register double			wl;
{
	CONST struct rt_table	*tabp;
	register int			i;

	RT_CK_TABDATA(samp);
	tabp = samp->table;
	RT_CK_TABLE(tabp);

	if( wl < tabp->x[0] || wl > tabp->x[tabp->nx] )
		return 0;

	/* Search for proper interval in input spectrum */
	for( i = 0; i < tabp->nx-1; i++ )  {
		FAST fastf_t	fract;		/* fraction from [i] to [i+1] */

		if( wl < tabp->x[i] )  rt_bomb("rt_table_lin_interp() assertion1 failed\n");
		if( wl >= tabp->x[i+1] )  continue;

		/* The interval has been found */
		fract = (wl - tabp->x[i]) /
			(tabp->x[i+1] - tabp->x[i]);
		if( fract < 0 || fract > 1 )  rt_bomb("rt_spect_spect_evaluate() assertion2 failed\n");
		return (1-fract) * samp->y[i] + fract * samp->y[i+1];
	}

	/* Assume value is constant in final interval. */
	if( !( wl >= tabp->x[tabp->nx-1] ) )
		rt_bomb("rt_table_lin_interp() assertion3 failed\n");
	return samp->y[tabp->nx-1];
}

/*
 *			R T _ T A B D A T A _ R E S A M P L E
 *
 *  Given a set of sampled data 'olddata', resample it for different
 *  spectral spacing, by linearly interpolating the values.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
struct rt_tabdata *
rt_tabdata_resample( newtable, olddata )
CONST struct rt_table	*newtable;
CONST struct rt_tabdata	*olddata;
{
	CONST struct rt_table	*oldspect;
	struct rt_tabdata		*newsamp;
	int				i;

	RT_CK_TABLE(newtable);
	RT_CK_TABDATA(olddata);
	oldspect = olddata->table;
	RT_CK_TABLE(oldspect);

	if( oldspect == newtable )  rt_log("rt_tabdata_resample() old and new spectrum structs are the same\n");

	RT_GET_TABDATA( newsamp, newtable );

	for( i = 0; i < newtable->nx; i++ )  {
		newsamp->y[i] = rt_table_lin_interp( olddata, newtable->x[i] );
	}
	return newsamp;
}

/*
 *			R T _ T A B L E _ W R I T E 
 *
 *  Write out the spectrum structure in an ASCII file,
 *  giving the number of wavelengths (minus 1), and the
 *  actual wavelengths.
 */
int
rt_table_write( filename, tabp )
CONST char	*filename;
CONST struct rt_table	*tabp;
{
	FILE	*fp;
	int	j;

	RT_CK_TABLE(tabp);

	RES_ACQUIRE( &rt_g.res_syscall );
	fp = fopen( filename, "w" );
	RES_RELEASE( &rt_g.res_syscall );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_table_write(%s, x%x) FAILED\n", filename, tabp);
		return -1;
	}

	RES_ACQUIRE( &rt_g.res_syscall );
	fprintf(fp, "  %d sample starts, and one end.\n", tabp->nx );
	for( j=0; j <= tabp->nx; j++ )  {
		fprintf( fp, "%g\n", tabp->x[j] );
	}
	fclose(fp);
	RES_RELEASE( &rt_g.res_syscall );
	return 0;
}

/*
 *			R T _ T A B L E _ R E A D
 *
 *  Allocate and read in the spectrum structure from an ASCII file,
 *  giving the number of wavelengths (minus 1), and the
 *  actual wavelengths.
 */
struct rt_table *
rt_table_read( filename )
CONST char	*filename;
{
	struct rt_table	*tabp;
	struct rt_vls		line;
	FILE	*fp;
	int	nw;
	int	j;

	RES_ACQUIRE( &rt_g.res_syscall );
	fp = fopen( filename, "r" );
	RES_RELEASE( &rt_g.res_syscall );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_table_read(%s) FAILED\n", filename);
		return NULL;
	}

	rt_vls_init(&line);
	rt_vls_gets( &line, fp );
	nw = 0;
	sscanf( rt_vls_addr(&line), "%d", &nw );
	rt_vls_free(&line);

	if( nw <= 0 ) rt_bomb("rt_table_read() bad nw value\n");

	RT_GET_TABLE( tabp, nw );

	RES_ACQUIRE( &rt_g.res_syscall );
	for( j=0; j <= tabp->nx; j++ )  {
		/* XXX assumes fastf_t == double */
		fscanf( fp, "%lf", &tabp->x[j] );
	}
	fclose(fp);
	RES_RELEASE( &rt_g.res_syscall );

	rt_ck_table( tabp );

	return tabp;
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

	RES_ACQUIRE( &rt_g.res_syscall );
	fp = fopen( filename, "w" );
	RES_RELEASE( &rt_g.res_syscall );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_pr_table_and_tabdata(%s, x%x) FAILED\n", filename, data );
		return -1;
	}

	RES_ACQUIRE( &rt_g.res_syscall );
	for( j=0; j < tabp->nx; j++ )  {
		fprintf( fp, "%g %g\n", tabp->x[j], data->y[j] );
	}
	fclose(fp);
	RES_RELEASE( &rt_g.res_syscall );
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
	struct rt_vls		line;
	FILE	*fp;
	char	buf[128];
	int	count = 0;
	int	i;

	RES_ACQUIRE( &rt_g.res_syscall );
	fp = fopen( filename, "r" );
	RES_RELEASE( &rt_g.res_syscall );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_read_table_and_tabdata(%s) FAILED\n", filename);
		return NULL;
	}

	/* First pass:  Count number of lines */
	RES_ACQUIRE( &rt_g.res_syscall );
	for(;;)  {
		if( fgets( buf, sizeof(buf), fp ) == NULL )  break;
		count++;
	}
	fclose(fp);
	RES_RELEASE( &rt_g.res_syscall );

	/* Allocate storage */
	RT_GET_TABLE( tabp, count );
	RT_GET_TABDATA( data, tabp );

	/* Second pass:  Read only as much data as storage was allocated for */
	RES_ACQUIRE( &rt_g.res_syscall );
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
	RES_RELEASE( &rt_g.res_syscall );

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
	int	fd;
	int	i;

	RT_CK_TABLE(tabp);

	nbytes = RT_SIZEOF_TABDATA(tabp);
	len = num * nbytes;
	data = (struct rt_tabdata *)rt_malloc( len+8, "rt_tabdata[]" );

	if( (fd = open(filename, 0)) <= 0 )  {
		perror(filename);
		rt_bomb("Unable to open rt_tabdata file\n");
	}
	if( read( fd, (char *)data, len ) != len )  {
		rt_bomb("Read of rt_tabdata failed\n");
	}
	close(fd);

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

#define C1	3.7415E4    /* watts um^4 cm^-2 */
#define C2	14387.86    /* um K */ 	
/* Russ gives these values at 37,415 and 14,388 */
/* Handbook of Physics and Chem gives these values as 37,403 and 14,384 */
/* Aircraft Combat Surv gives these values as 37,483.2 and 14,387.86 */

/* Requires wavelength _w in um, not nm, returns units: W / cm**2 / um */
#define	PLANCK(_w,_tempK)	\
	(C1/(_w*_w*_w*_w*_w*(exp(C2/(_w*_tempK))-1)))

/*
 *			R T _ S P E C T _ B L A C K _ B O D Y
 *
 *  Integrate Planck's Radiation Formula for a black body radiator
 *  across the given spectrum.
 *  Returns radiant emittance in W/cm**2 for each wavelength interval.
 *
 *  Based upon code kindly provided by Russ Moulton, Jr., EOSoft Inc.
 *  Compute at 'n-1' wavelengths evenly spaced between ax and bx.
 */
void
rt_spect_black_body( data, temp, n )
struct rt_tabdata	*data;
double			temp;		/* Degrees Kelvin */
unsigned int		n;		/* # wavelengths to eval at */
{
	CONST struct rt_table	*tabp;
	int				j;

	RT_CK_TABDATA(data);
	tabp = data->table;
	RT_CK_TABLE(tabp);
if(rt_g.debug) rt_log("rt_spect_black_body( x%x, %g degK ) %g um to %g um\n", data, temp,
tabp->x[0] * 0.001,	/* nm to um */
tabp->x[tabp->nx] * 0.001	/* nm to um */
);

	if( n < 3 )  n = 3;

	for( j = 0; j < tabp->nx; j++ )  {
		double	ax;		/* starting wavelength, um */
		double	bx;		/* ending wavelength, um */
		double	dx;		/* wavelength interval, um */
		double	w_sum;		/* sum over wavelengths */
		double	wavlen;		/* current wavelength */
		unsigned long i;

		ax = tabp->x[j] * 0.001;	/* nm to um */
		bx = tabp->x[j+1] * 0.001;	/* nm to um */
		dx = (bx - ax) / (double)n;

		w_sum = 0;
		wavlen = ax;
		for (i=0; i<n; i++)  {
			w_sum += PLANCK(wavlen, temp);
			wavlen += dx;
		}
		w_sum *= dx;

		data->y[j] = w_sum;
	}
}

/*
 *			R T _ S P E C T _ B L A C K _ B O D Y _ F A S T
 *
 *  Returns radiant emittance for each spectral interval in the given
 *  spectrum in units of watts/cm**2.
 *  Integrate each wavelength interval of spectral radiant emittance,
 *  by fitting with a rectangle (approximating curve with a horizontal line).
 *  For narrow spacing in wavelength this is OK, but with large spacing
 *  this tends to over-predict the power by 20%, due to the sharp
 *  (exponential) slope of the curve.
 *  With coarse spacing, or when unsure, use rt_spect_black_body().
 */
void
rt_spect_black_body_fast( data, temp )
struct rt_tabdata	*data;
double			temp;		/* Degrees Kelvin */
{
	CONST struct rt_table	*tabp;
	int				j;

	RT_CK_TABDATA(data);
	tabp = data->table;
	RT_CK_TABLE(tabp);
if(rt_g.debug) rt_log("rt_spect_black_body_fast( x%x, %g degK )\n", data, temp );

	for( j = 0; j < tabp->nx; j++ )  {
		data->y[j] = PLANCK( (tabp->x[j]*0.001), temp ) *
			(tabp->x[j+1] - tabp->x[j]) * 0.001;
	}
}

/*
 *			R T _ S P E C T _ B L A C K _ B O D Y _ P O I N T S
 *
 *  Returns point-sampled values of spectral radiant emittance,
 *  in units of watts/cm**2/um,
 *  straight from Planck's black-body radiation formula.
 */
void
rt_spect_black_body_points( data, temp )
struct rt_tabdata	*data;
double			temp;		/* Degrees Kelvin */
{
	CONST struct rt_table	*tabp;
	int				j;

	RT_CK_TABDATA(data);
	tabp = data->table;
	RT_CK_TABLE(tabp);
if(rt_g.debug) rt_log("rt_spect_black_body_points( x%x, %g degK )\n", data, temp );

	for( j = 0; j < tabp->nx; j++ )  {
		data->y[j] = PLANCK( (tabp->x[j]*0.001), temp );
	}
}

#if 0
main()
{
	struct rt_tabdata	*x, *y, *z;
	struct rt_table	*tabp;

#if 0
	tabp = rt_table_make_uniform( 200, 360.0, 800.0 );

	rt_spect_make_CIE_XYZ( &x, &y, &z, tabp );
	
	rt_pr_table_and_tabdata( "/tmp/x", x );
	rt_pr_table_and_tabdata( "/tmp/y", y );
	rt_pr_table_and_tabdata( "/tmp/z", z );
#endif

	tabp = rt_table_make_uniform( 100, 3.0, 3000.0 );

	RT_GET_TABDATA( x, tabp );
	rt_spect_black_body_points( x, 10000.0 );
	rt_pr_table_and_tabdata( "/tmp/x", x );

	RT_GET_TABDATA( y, tabp );
	rt_spect_black_body( y, 10000.0, 3 );
	rt_pr_table_and_tabdata( "/tmp/y", y );

	RT_GET_TABDATA( z, tabp );
	rt_spect_black_body_fast( z, 10000.0 );
	rt_pr_table_and_tabdata( "/tmp/z", z );
}
#endif

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

	bcopy( (char *)in->y, (char *)out->y, in->ny * sizeof(fastf_t) );
}
