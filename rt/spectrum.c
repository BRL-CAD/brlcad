/*
 *			S P E C T R U M . C
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

/*
 *			R T _ S P E C T _ U N I F O R M
 *
 *  Set up a sampling of the spectrum from 'first' to 'last',
 *  inclusive, which are specified as wavelength in nm,
 *  using 'num' uniformly spaced samples.  Num >= 1.
 */
struct rt_spectrum *
rt_spect_uniform( num, first, last )
int	num;
double	first;
double	last;
{
	struct rt_spectrum	*spect;
	fastf_t			*fp;
	fastf_t			delta;
	int			j;

	if( first >= last )  rt_bomb("rt_spect_uniform() first >= last\n");

	RT_GET_SPECTRUM( spect, num );

	delta = (last - first) / (double)num;

	fp = &spect->wavel[0];
	for( j = num; j > 0; j-- )  {
		*fp++ = first;
		first += delta;
	}
	spect->wavel[num] = last;
}

/*
 *			R T _ S P E C T _ A D D
 *
 *  Sum the values from two spectral samples.
 */
void
rt_spect_add( out, in1, in2 )
struct rt_spect_sample		*out;
CONST struct rt_spect_sample	*in1;
CONST struct rt_spect_sample	*in2;
{
	register int		j;
	register fastf_t	*op, *i1, *i2;

	RT_CK_SPECT_SAMPLE( out );
	RT_CK_SPECT_SAMPLE( in1 );
	RT_CK_SPECT_SAMPLE( in2 );

	if( in1->spectrum != in2->spectrum || in1->spectrum != out->spectrum )
		rt_bomb("rt_spect_add(): samples drawn from different spectra\n");
	if( in1->nwave != in2->nwave || in1->nwave != out->nwave )
		rt_bomb("rt_spect_add(): different number of wavelengths\n");

	op = out->val;
	i1 = in1->val;
	i2 = in2->val;
	for( j = in1->nwave; j > 0; j-- )
		*op++ = *i1++ + *i2++;
	/* VADD2N( out->val, i1->val, i2->val, in1->nwave ); */
}

/*
 *			R T _ S P E C T _ M U L
 *
 *  Element-by-element multiply the values from two spectral samples.
 */
void
rt_spect_mul( out, in1, in2 )
struct rt_spect_sample		*out;
CONST struct rt_spect_sample	*in1;
CONST struct rt_spect_sample	*in2;
{
	register int		j;
	register fastf_t	*op, *i1, *i2;

	RT_CK_SPECT_SAMPLE( out );
	RT_CK_SPECT_SAMPLE( in1 );
	RT_CK_SPECT_SAMPLE( in2 );

	if( in1->spectrum != in2->spectrum || in1->spectrum != out->spectrum )
		rt_bomb("rt_spect_mul(): samples drawn from different spectra\n");
	if( in1->nwave != in2->nwave || in1->nwave != out->nwave )
		rt_bomb("rt_spect_mul(): different number of wavelengths\n");

	op = out->val;
	i1 = in1->val;
	i2 = in2->val;
	for( j = in1->nwave; j > 0; j-- )
		*op++ = *i1++ * *i2++;
	/* VELMUL2N( out->val, i1->val, i2->val, in1->nwave ); */
}

/*
 *			R T _ S P E C T _ S C A L E
 *
 *  Multiply every element in a spectral sample by a scalar value 'scale'.
 */
void
rt_spect_scale( out, in1, scale )
struct rt_spect_sample		*out;
CONST struct rt_spect_sample	*in1;
register double			scale;
{
	register int		j;
	register fastf_t	*op, *i1;

	RT_CK_SPECT_SAMPLE( out );
	RT_CK_SPECT_SAMPLE( in1 );

	if( in1->spectrum != out->spectrum )
		rt_bomb("rt_spect_scale(): samples drawn from different spectra\n");
	if( in1->nwave != out->nwave )
		rt_bomb("rt_spect_scale(): different number of wavelengths\n");

	op = out->val;
	i1 = in1->val;
	for( j = in1->nwave; j > 0; j-- )
		*op++ = *i1++ * scale;
	/* VSCALEN( out->val, in->val, scale ); */
}

/*
 *			R T _ S P E C T _ A R E A 1
 *
 *  Following interpretation #1, where val[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 */
double
rt_spect_area1( in )
CONST struct rt_spect_sample	*in;
{
	FAST fastf_t		area;
	register fastf_t	*ip;
	register int		j;

	RT_CK_SPECT_SAMPLE(in);

	area = 0;
	ip = in->val;
	for( j = in->nwave; j > 0; j-- )
		area += *ip++;

	return area;
}

/*
 *			R T _ S P E C T _ A R E A 2
 *
 *  Following interpretation #2, where val[j] stores the average
 *  value value for the interval, return the area under
 *  the whole curve.  Since the iterval spacing need not be uniform,
 *  sum the areas of the rectangles.
 */
double
rt_spect_area2( in )
CONST struct rt_spect_sample	*in;
{
	CONST struct rt_spectrum	*spect;
	FAST fastf_t		area;
	fastf_t			width;
	register int		j;

	RT_CK_SPECT_SAMPLE(in);
	spect = in->spectrum;
	RT_CK_SPECTRUM(spect);

	area = 0;
	for( j = in->nwave-1; j >= 0; j-- )  {
		width = spect->wavel[j+1] - spect->wavel[j];
		area += in->val[j] * width;
	}

	return area;
}

/* This is the data for the CIE_XYZ curves take from Judd and
 *  Wyszecki (1975), table 2.6, these are for the 1931 standard
 *  observer with a 2-degree visual field.
 *  From Roy Hall.
 */
static double	rt_CIE_XYZ[81][4] = {
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
rt_spect_make_CIE_XYZ( x, y, z, spect )
struct rt_spect_sample		**x;
struct rt_spect_sample		**y;
struct rt_spect_sample		**z;
CONST struct rt_spectrum	*spect;
{
	struct rt_spect_sample	*a, *b, *c;
	int	i;
	int	j;

	RT_CK_SPECTRUM(spect);

	RT_GET_SPECT_SAMPLE( a, spect );
	RT_GET_SPECT_SAMPLE( b, spect );
	RT_GET_SPECT_SAMPLE( c, spect );
	*x = a;
	*y = b;
	*z = c;

	/* No CIE data below 380 nm */
	for( j=0; spect->wavel[j] < 380 && j < spect->nwave; j++ )  {
		a->val[j] = b->val[j] = c->val[j] = 0;
	}

	/* Traverse the CIE table.  Produce as many output values as possible
	 * before advancing to next CIE table entry.
	 */
	for( i = 0; i < 81; i++ )  {
		FAST fastf_t	fract;		/* fraction from [i] to [i+1] */

again:
		if( j >= spect->nwave )  return;
		if( spect->wavel[j] < rt_CIE_XYZ[i][0] ) rt_bomb("rt_spect_make_CIE_XYZ assertion1 failed\n");
		if( spect->wavel[j] >= rt_CIE_XYZ[i+1][0] )  continue;
		/* The CIE table has 5nm spacing */
		fract = (spect->wavel[j] - rt_CIE_XYZ[i][0] ) / 5;
		if( fract < 0 || fract > 1 )  rt_bomb("rt_spect_make_CIE_XYZ assertion2 failed\n");
		a->val[j] = (1-fract) * rt_CIE_XYZ[i][1] + fract * rt_CIE_XYZ[i+1][1];
		b->val[j] = (1-fract) * rt_CIE_XYZ[i][2] + fract * rt_CIE_XYZ[i+1][2];
		c->val[j] = (1-fract) * rt_CIE_XYZ[i][3] + fract * rt_CIE_XYZ[i+1][3];
		j++;
		goto again;
	}

	/* No CIE data above 780 nm */
	for( ; j < spect->nwave; j++ )  {
		a->val[j] = b->val[j] = c->val[j] = 0;
	}

}

/*
 *
 */
void
rt_write_spect_sample( filename, ss )
CONST char			*filename;
CONST struct rt_spect_sample	*ss;
{
	FILE	*fp;
	CONST struct rt_spectrum	*spect;
	int	j;

	RT_CK_SPECT_SAMPLE(ss);
	spect = ss->spectrum;
	RT_CK_SPECTRUM(spect);

	RES_ACQUIRE( &rt_g.res_syscall );
	fp = fopen( filename, "w" );
	RES_RELEASE( &rt_g.res_syscall );

	if( fp == NULL )  {
		perror(filename);
		rt_log("rt_write_spect_sample(%s, %s) FAILED\n");
		return;
	}

	RES_ACQUIRE( &rt_g.res_syscall );
	for( j=0; j < spect->nwave; j++ )  {
		fprintf( fp, "%g %g\n", spect->wavel[j], ss->val[j] );
	}
	fclose(fp);
	RES_RELEASE( &rt_g.res_syscall );
}


#define C1	3.7415E4    /* watts um^4 cm^-2 */
#define C2    	1.4388E4    /* um K */ 	
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
 *  Compute at 'n-1' wavelengths evenly spaces between ax and bx,
 *  taking half of the left and right-most rectangles.
 */
void
rt_spect_black_body( ss, temp )
struct rt_spect_sample	*ss;
double			temp;		/* Degrees Kelvin */
{
	CONST struct rt_spectrum	*spect;
	int				j;

	RT_CK_SPECT_SAMPLE(ss);
	spect = ss->spectrum;
	RT_CK_SPECTRUM(spect);
rt_log("rt_spect_black_body( x%x, %g degK ) %g um to %g um\n", ss, temp,
spect->wavel[0] * 0.001,	/* nm to um */
spect->wavel[spect->nwave] * 0.001	/* nm to um */
);

	for( j = 0; j < spect->nwave; j++ )  {
		double	ax;		/* starting wavelength, um */
		double	bx;		/* ending wavelength, um */
		double	w_sum;
		double	wavlen;
		double	dx;
		unsigned long n,i;

		ax = spect->wavel[j] * 0.001;	/* nm to um */
		bx = spect->wavel[j+1] * 0.001;	/* nm to um */

		n = 3;
		dx = (bx - ax) / (double)n;

		w_sum = 0.50 * PLANCK(ax, temp);
		wavlen = ax + dx;
		for (i=1; i<n; i++)  {
			w_sum += PLANCK(wavlen, temp);
			wavlen += dx;
		}
		w_sum += 0.50 * PLANCK(bx, temp);
		w_sum *= dx;

		ss->val[j] = w_sum;
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
rt_spect_black_body_fast( ss, temp )
struct rt_spect_sample	*ss;
double			temp;		/* Degrees Kelvin */
{
	CONST struct rt_spectrum	*spect;
	int				j;

	RT_CK_SPECT_SAMPLE(ss);
	spect = ss->spectrum;
	RT_CK_SPECTRUM(spect);
rt_log("rt_spect_black_body_fast( x%x, %g degK )\n", ss, temp );

	for( j = 0; j < spect->nwave; j++ )  {
		ss->val[j] = PLANCK( (spect->wavel[j]*0.001), temp ) *
			(spect->wavel[j+1] - spect->wavel[j]) * 0.001;
	}
}

/*
 *  Returns point-samples values of spectral radiant emittance,
 *  in units of watts/cm**2/um,
 *  straight from Planck's formula.
 */
void
rt_spect_black_body_points( ss, temp )
struct rt_spect_sample	*ss;
double			temp;		/* Degrees Kelvin */
{
	CONST struct rt_spectrum	*spect;
	int				j;

	RT_CK_SPECT_SAMPLE(ss);
	spect = ss->spectrum;
	RT_CK_SPECTRUM(spect);
rt_log("rt_spect_black_body_points( x%x, %g degK )\n", ss, temp );

	for( j = 0; j < spect->nwave; j++ )  {
		ss->val[j] = PLANCK( (spect->wavel[j]*0.001), temp );
	}
}

main()
{
	struct rt_spect_sample	*x, *y, *z;
	struct rt_spectrum	*spect;

#if 0
	spect = rt_spect_uniform( 200, 360.0, 800.0 );

	rt_spect_make_CIE_XYZ( &x, &y, &z, spect );
	
	rt_write_spect_sample( "/tmp/x", x );
	rt_write_spect_sample( "/tmp/y", y );
	rt_write_spect_sample( "/tmp/z", z );
#endif

	spect = rt_spect_uniform( 100, 3.0, 3000.0 );

	RT_GET_SPECT_SAMPLE( x, spect );
	rt_spect_black_body_points( x, 10000.0 );
	rt_write_spect_sample( "/tmp/x", x );

	RT_GET_SPECT_SAMPLE( y, spect );
	rt_spect_black_body( y, 10000.0 );
	rt_write_spect_sample( "/tmp/y", y );

	RT_GET_SPECT_SAMPLE( z, spect );
	rt_spect_black_body_fast( z, 10000.0 );
	rt_write_spect_sample( "/tmp/z", z );
}

