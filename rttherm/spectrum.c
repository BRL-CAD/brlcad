/*
 *			S P E C T R U M . C
 *
 *  An application of the 'tabdata' package to spectral data.
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
#include "raytrace.h"
#include "spectrum.h"

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
struct bn_tabdata		**x;
struct bn_tabdata		**y;
struct bn_tabdata		**z;
CONST struct bn_table	*tabp;
{
	struct bn_tabdata	*a, *b, *c;
	fastf_t	xyz_scale;
	int	i;
	int	j;

	BN_CK_TABLE(tabp);

	i = bn_table_interval_num_samples( tabp, 430., 650. );
	if( i <= 4 )  bu_log("rt_spect_make_CIE_XYZ: insufficient samples (%d) in visible band\n", i);

	BN_GET_TABDATA( a, tabp );
	BN_GET_TABDATA( b, tabp );
	BN_GET_TABDATA( c, tabp );
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
		if( j >= tabp->nx )  break;
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
	xyz_scale = bn_tabdata_area2( b );
	if( fabs(xyz_scale) < VDIVIDE_TOL )  {
		rt_log("rt_spect_make_CIE_XYZ(): Area = 0 (no luminance) in this part of the spectrum, skipping normalization step\n");
		return;
	}
	xyz_scale = 1 / xyz_scale;
	bn_tabdata_scale( a, a, xyz_scale );
	bn_tabdata_scale( b, b, xyz_scale );
	bn_tabdata_scale( c, c, xyz_scale );
}

static CONST double rt_NTSC_R[][2] = {
	{543, 0.0001},
	{552, 0.05},
	{562, 0.2},
	{572, 0.4},
	{577, 0.6},
	{580, 0.8},
	{583, 0.9},
	{587, 0.95},
	{590, 1.0},
	{598, 1.0},
	{601, 0.95},
	{605, 0.9},
	{610, 0.8},
	{620, 0.6},
	{630, 0.4},
	{640, 0.26},
	{650, 0.2},
	{670, 0.1},
	{690, 0.05},
	{730, 0.0001},
	{-1, -1}
};

static CONST double rt_NTSC_G[][2] = {
	{456, 0.0001},
	{475, 0.05},
	{480, 0.1},
	{492, 0.2},
	{507, 0.4},
	{515, 0.6},
	{522, 0.8},
	{528, 0.9},
	{531, 0.95},
	{537, 1.0},
	{545, 1.0},
	{548, 0.95},
	{551, 0.9},
	{555, 0.8},
	{562, 0.6},
	{572, 0.4},
	{582, 0.2},
	{591, 0.1},
	{603, 0.05},
	{630, 0.0001},
	{-1, -1}
};

static CONST double rt_NTSC_B[][2] = {
	{347, 0.0001},
	{373, 0.05},
	{385, 0.1},
	{396, 0.2},
	{409, 0.4},
	{415, 0.6},
	{423, 0.8},
	{430, 0.9},
	{433, 0.95},
	{440, 1.0},
	{450, 1.0},
	{457, 0.95},
	{460, 0.9},
	{466, 0.8},
	{470, 0.6},
	{479, 0.4},
	{492, 0.2},
	{503, 0.1},
	{515, 0.05},
	{543, 0.0001},
	{-1, -1}
};

struct bn_tabdata *rt_NTSC_r_tabdata;
struct bn_tabdata *rt_NTSC_g_tabdata;
struct bn_tabdata *rt_NTSC_b_tabdata;

/*
 *			R T _ S P E C T _ M A K E _ N T S C _ R G B
 *
 *  Using the "Representative set of camera taking sensitivities"
 *  for a NTSC television camera, from Benson "Television Engineering
 *  Handbook" page 4.58, convert an RGB value in range 0..1 to
 *  a spectral curve also in range 0..1.
 *
 *  These curves should be used in converting spectral samples
 *  to NTSC RGB values.
 */
void
rt_spect_make_NTSC_RGB( rp, gp, bp, tabp )
struct bn_tabdata		**rp;
struct bn_tabdata		**gp;
struct bn_tabdata		**bp;
CONST struct bn_table		*tabp;
{
	BN_CK_TABLE(tabp);

	/* Convert array of number pairs into bn_tabdata & bn_table */
	rt_NTSC_r_tabdata = bn_tabdata_from_array( &rt_NTSC_R[0][0] );
	rt_NTSC_g_tabdata = bn_tabdata_from_array( &rt_NTSC_G[0][0] );
	rt_NTSC_b_tabdata = bn_tabdata_from_array( &rt_NTSC_B[0][0] );

bu_log("ntsc_R: area=%g\n", bn_tabdata_area2(rt_NTSC_r_tabdata) );
	bn_print_table_and_tabdata( "/dev/tty", rt_NTSC_r_tabdata );
bu_log("ntsc_G: area=%g\n", bn_tabdata_area2(rt_NTSC_g_tabdata) );
	bn_print_table_and_tabdata( "/dev/tty", rt_NTSC_g_tabdata );
bu_log("ntsc_B: area=%g\n", bn_tabdata_area2(rt_NTSC_b_tabdata) );
	bn_print_table_and_tabdata( "/dev/tty", rt_NTSC_b_tabdata );

	/* Resample original NTSC curves to match given bn_table sampling */
#if 0
	/* just to test the routine */
	*rp = bn_tabdata_resample_avg( tabp, rt_NTSC_r_tabdata );
	*gp = bn_tabdata_resample_avg( tabp, rt_NTSC_g_tabdata );
	*bp = bn_tabdata_resample_avg( tabp, rt_NTSC_b_tabdata );
#else
	/* use this one for real */
	*rp = bn_tabdata_resample_max( tabp, rt_NTSC_r_tabdata );
	*gp = bn_tabdata_resample_max( tabp, rt_NTSC_g_tabdata );
	*bp = bn_tabdata_resample_max( tabp, rt_NTSC_b_tabdata );
#endif
}

/*
 *			R T _ S P E C T _ R E F L E C T A N C E _ R G B
 *
 *  Given reflectance data (in range 0..1) in terms of RGB color,
 *  convert that to a spectral reflectance curve.
 *
 *  The assumption here is that the spectrum is made up of exactly three
 *  non-overlapping bands, and the reflectance is constant over each:
 *
 *	red	572nm to 1,000,000nm	(includes the full IR band)
 *	green	492nm to 572nm		(just green)
 *	blue	1nm to 492nm		(includes Ultraviolet)
 *
 *  As the caller may be doing a lot of this, the caller is expected
 *  to provide a pointer to a valid bn_tabdata structure which is
 *  to be filled in.  Allowing caller to re-cycle them rather than
 *  doing constant malloc/free cycle.
 */
void
rt_spect_reflectance_rgb( curve, rgb )
struct bn_tabdata	*curve;
CONST point_t		rgb;
{
	register int	i;
	register CONST struct bn_table	*tabp;

	BN_CK_TABDATA(curve);
	tabp = curve->table;
	BN_CK_TABLE(tabp);

	/* Fill in blue values, everything up to but not including 492nm */
	for( i=0; i < tabp->nx; i++ )  {
		if( tabp->x[i] >= 492 )  break;
		curve->y[i] = rgb[2];
	}

	/* Fill in green values, everything up to but not including 572nm */
	for( ; i < tabp->nx; i++ )  {
		if( tabp->x[i] >= 572 )  break;
		curve->y[i] = rgb[1];
	}

	/* Fill in red values, everything from here up to end of table */
	for( ; i < tabp->nx; i++ )  {
		curve->y[i] = rgb[0];
	}
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
 *			R T _ M A K E _ N T S C _ X Y Z 2 R G B
 *
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

#if 1
	/* Verify that it really works, I'm a skeptic */
	VSET( tst, 1, 1, 1 );
	MAT3X3VEC( new, rgb2xyz, tst );
	VPRINT( "white_rgb (i)", tst );
	VPRINT( "white_xyz (o)", new );

	VSET( tst, 0.313,     0.329,      0.358);
	MAT3X3VEC( new, xyz2rgb, tst );
	VPRINT( "white_xyz (i)", tst );
	VPRINT( "white_rgb (o)", new );

	VSET( tst, 1, 0, 0 );
	MAT3X3VEC( new, rgb2xyz, tst );
	VPRINT( "red_rgb (i)", tst );
	VPRINT( "red_xyz (o)", new );

	VSET( tst, 0.670,     0.330,      0.000);
	MAT3X3VEC( new, xyz2rgb, tst );
	VPRINT( "red_xyz (i)", tst );
	VPRINT( "red_rgb (o)", new );

	VSET( tst, 0, 1, 0 );
	MAT3X3VEC( new, rgb2xyz, tst );
	VPRINT( "grn_rgb (i)", tst );
	VPRINT( "grn_xyz (o)", new );

	VSET( tst, 0.210,     0.710,      0.080);
	MAT3X3VEC( new, xyz2rgb, tst );
	VPRINT( "grn_xyz (i)", tst );
	VPRINT( "grn_rgb (o)", new );

	VSET( tst, 0, 0, 1 );
	MAT3X3VEC( new, rgb2xyz, tst );
	VPRINT( "blu_rgb (i)", tst );
	VPRINT( "blu_xyz (o)", new );

	VSET( tst, 0.140,     0.080,      0.780);
	MAT3X3VEC( new, xyz2rgb, tst );
	VPRINT( "blu_xyz (i)", tst );
	VPRINT( "blu_rgb (o)", new );
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
struct bn_tabdata	*data;
double			temp;		/* Degrees Kelvin */
unsigned int		n;		/* # wavelengths to eval at */
{
	CONST struct bn_table	*tabp;
	int				j;

	BN_CK_TABDATA(data);
	tabp = data->table;
	BN_CK_TABLE(tabp);
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
struct bn_tabdata	*data;
double			temp;		/* Degrees Kelvin */
{
	CONST struct bn_table	*tabp;
	int				j;

	BN_CK_TABDATA(data);
	tabp = data->table;
	BN_CK_TABLE(tabp);
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
struct bn_tabdata	*data;
double			temp;		/* Degrees Kelvin */
{
	CONST struct bn_table	*tabp;
	int				j;

	BN_CK_TABDATA(data);
	tabp = data->table;
	BN_CK_TABLE(tabp);
if(rt_g.debug) rt_log("rt_spect_black_body_points( x%x, %g degK )\n", data, temp );

	for( j = 0; j < tabp->nx; j++ )  {
		data->y[j] = PLANCK( (tabp->x[j]*0.001), temp );
	}
}

/*
 *			R T _ S P E C T _ C U R V E _ T O _ X Y Z
 *
 *  Convenience routine.
 *  Serves same function as Roy Hall's CLR_spect_to_xyz(), pg 233.
 *  The normalization xyz_scale = 1.0 / bn_tabdata_area2( cie_y );
 *  has been folded into rt_spect_make_CIE_XYZ();
 */
void
rt_spect_curve_to_xyz( xyz, tabp, cie_x, cie_y, cie_z )
point_t			xyz;
CONST struct bn_tabdata	*tabp;
CONST struct bn_tabdata	*cie_x;
CONST struct bn_tabdata	*cie_y;
CONST struct bn_tabdata	*cie_z;
{
	FAST fastf_t	tab_area;

	BN_CK_TABDATA(tabp);

#if 0
	tab_area = bn_tabdata_area2( tabp );
rt_log(" tab_area = %g\n", tab_area);
	if( fabs(tab_area) < VDIVIDE_TOL )  {
		rt_log("rt_spect_curve_to_xyz(): Area = 0 (no luminance) in this part of the spectrum\n");
		VSETALL( xyz, 0 );
		return;
	}
	tab_area = 1 / tab_area;
#else
	/* This is what Roy says to do, but I'm not certain */
	tab_area = 1;
#endif

	xyz[X] = bn_tabdata_mul_area2( tabp, cie_x ) * tab_area;
	xyz[Y] = bn_tabdata_mul_area2( tabp, cie_y ) * tab_area;
	xyz[Z] = bn_tabdata_mul_area2( tabp, cie_z ) * tab_area;
}

/*
 *			R T _ S P E C T _ R G B _ T O _ C U R V E
 *
 *  Using the "Representative set of camera taking sensitivities"
 *  for a NTSC television camera, from Benson "Television Engineering
 *  Handbook" page 4.58, convert an RGB value in range 0..1 to
 *  a spectral curve also in range 0..1.
 *
 *  XXX This is completely wrong, don't do this.
 */
void
rt_spect_rgb_to_curve( tabp, rgb, ntsc_r, ntsc_g, ntsc_b )
struct bn_tabdata	*tabp;
CONST point_t		rgb;
CONST struct bn_tabdata	*ntsc_r;
CONST struct bn_tabdata	*ntsc_g;
CONST struct bn_tabdata	*ntsc_b;
{
	bn_tabdata_blend3( tabp,
		rgb[0], ntsc_r,
		rgb[1], ntsc_g,
		rgb[2], ntsc_b );
}

/*
 *			R T _ S P E C T _ X Y Z _ T O _ C U R V E
 *
 *  Values of the curve will be normalized to 0..1 range;
 *  caller must scale into meaningful units.
 *
 *  Convenience routine.
XXX This routine is probably wrong.  Or at least, it needs different curves.
XXX Converting rgb to a curve, directly, should be easy.
 */
void
rt_spect_xyz_to_curve( tabp, xyz, cie_x, cie_y, cie_z )
struct bn_tabdata	*tabp;
CONST point_t		xyz;
CONST struct bn_tabdata	*cie_x;
CONST struct bn_tabdata	*cie_y;
CONST struct bn_tabdata	*cie_z;
{
	bn_tabdata_blend3( tabp,
		xyz[X], cie_x,
		xyz[Y], cie_y,
		xyz[Z], cie_z );
}

/*
 *			R T _ T A B L E _ M A K E _ V I S I B L E _ A N D _ U N I F O R M
 *
 *  A quick hack to make sure there are enough samples in the visible band.
 */
struct bn_table *
bn_table_make_visible_and_uniform( num, first, last, vis_nsamp )
int	num;
double	first;
double	last;
int	vis_nsamp;
{
	struct bn_table	*new;
	struct bn_table *uniform;
	struct bn_table	*vis;

	if( vis_nsamp < 10 )  vis_nsamp = 10;
	uniform = bn_table_make_uniform( num, first, last );
	vis = bn_table_make_uniform( vis_nsamp, 340.0, 700.0 );

	new = bn_table_merge2( uniform, vis );
	bn_ck_table(new);

	bn_table_free(uniform);
	bn_table_free(vis);

	return new;
}

#if 0
main()
{
	struct bn_tabdata	*x, *y, *z;
	struct bn_table	*tabp;

#if 0
	tabp = bn_table_make_uniform( 200, 360.0, 800.0 );

	rt_spect_make_CIE_XYZ( &x, &y, &z, tabp );
	
	bn_print_table_and_tabdata( "/tmp/x", x );
	bn_print_table_and_tabdata( "/tmp/y", y );
	bn_print_table_and_tabdata( "/tmp/z", z );
#endif

	tabp = bn_table_make_uniform( 100, 3.0, 3000.0 );

	BN_GET_TABDATA( x, tabp );
	rt_spect_black_body_points( x, 10000.0 );
	bn_print_table_and_tabdata( "/tmp/x", x );

	BN_GET_TABDATA( y, tabp );
	rt_spect_black_body( y, 10000.0, 3 );
	bn_print_table_and_tabdata( "/tmp/y", y );

	BN_GET_TABDATA( z, tabp );
	rt_spect_black_body_fast( z, 10000.0 );
	bn_print_table_and_tabdata( "/tmp/z", z );
}
#endif
