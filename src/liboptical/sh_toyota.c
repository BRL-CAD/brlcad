/*                     S H _ T O Y O T A . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file sh_toyota.c
 *			T O Y O T A
 *
 *  Notes -
 *	Implementation of model developed by Atsushi Takagi, Hitoshi
 *	Takaoka, Tetsuya Oshima, and Yoshinori Ogata described in
 *	the paper "Accurate Rendering Technique Based on Colorimetric
 *	Conception" in "Computer Graphics, Volume 24, Number 4, August
 *	1990", an ACM SIGGRAPH Publication, pp. 263-72.  References
 *	to Toyota in this module refer to that paper.
 *
 *	The normals on all surfaces point OUT of the solid.
 *	The incoming light rays point IN.  Thus the sign change.
 *
 *  Author -
 *	Michael Markowski
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "plot3.h"
#include "light.h"

/* Sky onditions for luminance calculations. */
#define CLEAR_SKY	0
#define MEDIUM_SKY	1
#define OVERCAST_SKY	2

/* Sometimes found in <math.h> */
#if !defined(PI)
#define PI    3.14159265358979323846264338327950288419716939937511
#endif
#define MIKE_TOL	.000001

/* Local information */
struct toyota_specific {
	fastf_t	lambda;		/* Wavelength of light (nm).		*/
	fastf_t	alpha, beta;	/* Coefficients of turbidity.		*/
	int	weather;	/* CLEAR_SKY, MEDIUM_SKY, OVERCAST_SKY	*/
	fastf_t	sun_sang;	/* Solid angle of sun from ground.	*/
	fastf_t	index_refrac;	/* Index of refraction of material.	*/
	fastf_t	atmos_trans;	/* Atmospheric transmittance.		*/
	vect_t	Zenith;		/* Sky zenith.				*/
	char	material[128];	/* File name of reflectance data.	*/
	fastf_t	*refl;		/* Data read from 'material'.		*/
	int	refl_lines;	/* Lines read from 'material' file.	*/
	int	glass;		/* Boolean, is it glass?		*/
};
#define CK_NULL	((struct toyota_specific *)0)
#define CL_O(m)	bu_offsetof(struct toyota_specific, m)

struct bu_structparse toyota_parse[] = {
	{"%f", 1, "alpha",	CL_O(alpha),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 1, "beta",	CL_O(beta),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d", 1, "weather",	CL_O(weather),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 1, "sun_sang",	CL_O(sun_sang),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 1, "index_refrac",CL_O(index_refrac),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 1, "atmos_trans",CL_O(atmos_trans),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 3, "Zenith",	bu_offsetofarray(struct toyota_specific, Zenith),		BU_STRUCTPARSE_FUNC_NULL },
	{"%s", 1, "material",	bu_offsetofarray(struct toyota_specific, material),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d", 1, "glass",	CL_O(glass),		BU_STRUCTPARSE_FUNC_NULL },
	{"",   0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	toyota_setup(register struct region *rp, struct bu_vls *matparm, char **dtp, struct mfuncs *mfp, struct rt_i *rtip), tmirror_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), tglass_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int	toyota_render(register struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	toyota_print(register struct region *rp, char *dp);
HIDDEN void	toyota_free(char *cp);
void		lambda_to_rgb(fastf_t lambda, fastf_t irrad, fastf_t *rgb),
		spectral_dist_table(fastf_t lambda, fastf_t *e_mean, fastf_t *v1, fastf_t *v2);
fastf_t
	atmos_irradiance(fastf_t lambda),
	air_mass(fastf_t gamma),
	background_light(fastf_t lambda, struct toyota_specific *ts, fastf_t *Refl, fastf_t *Sun, fastf_t t_vl, struct shadework *swp),
	clear_sky_lum(fastf_t lz, fastf_t *Sky_elmt, fastf_t *Sun, fastf_t *Zenith),
	fresnel_refl(fastf_t cos_eps, fastf_t n1, fastf_t n2),
	homogenous_sky_lum(fastf_t *Sky_elmt, fastf_t *Sun, fastf_t t_vl),
	sun_radiance(fastf_t lambda, fastf_t alpha, fastf_t beta, fastf_t sun_alt, fastf_t sun_sang),
	absorp_coeff(fastf_t lambda, char *material),
	overcast_sky_lum(fastf_t lz, fastf_t *Zenith, fastf_t *Sky_elmt),
	ozone_absorption(fastf_t lambda),
	skylight_spectral_dist(fastf_t lambda, fastf_t *Zenith, fastf_t *Sky_elmt, fastf_t *Sun, int weather, fastf_t t_vl),
	zenith_luminance(fastf_t sun_alt, fastf_t t_vl);

struct mfuncs toyota_mfuncs[] = {
	{MF_MAGIC,	"toyota",	0,		MFI_NORMAL|MFI_LIGHT,	0,
	toyota_setup,	toyota_render,	toyota_print,	toyota_free },

	{MF_MAGIC,	"tmirror",	0,		MFI_NORMAL|MFI_LIGHT,	0,
	tmirror_setup,	toyota_render,	toyota_print,	toyota_free },

	{MF_MAGIC,	"tglass",	0,		MFI_NORMAL|MFI_LIGHT,	0,
	tglass_setup,	toyota_render,	toyota_print,	toyota_free },

	{0,		(char *)0,	0,		0,	0,
	0,		0,		0,		0 }
};

#define RI_AIR		1.0    /* Refractive index of air.		*/

/*
 *	T O Y O T A _ S E T U P
 *
 *	These should be measured values, but since we're lazy
 *	let's use values Toyota carefully measured in Japan one day.
 *
 *	Data measured:	11:28am, October 20, 1989 (Friday)
 *
 *	Location:	Toyota-city, Aichi-prefecture, Japan
 *			at 35d 2'55.07" North, 137d 9'45.26" East
 *
 *	Measuring Instruments: Precision pyrheliometer (Model MS-52 made
 *			by Eko Co., Ltd.), Sun-photo meter (Model MS-110
 *			made by Eko Co., Ltd.), Luminance meter
 *			(MINOLTA CS-100)
 */
HIDDEN int
toyota_setup(register struct region *rp, struct bu_vls *matparm, char **dtp, struct mfuncs *mfp, struct rt_i *rtip)




                                /* New since 4.4 release */
{
	char	mfile[200];
	fastf_t	l, a, b;
	FILE	*fp;
	int	i, lines;
	register struct toyota_specific *tp;

	BU_CK_VLS(matparm);
	BU_GETSTRUCT(tp, toyota_specific);
	*dtp = (char *)tp;

	/* Sun was at 44.35 degrees above horizon. */
	tp->alpha = 0.957;	/* coefficients of turbidity (aerosol */
	tp->beta = 0.113;	/* optical depth) */
	tp->atmos_trans = 0.772;/* atmospheric transmittance */
	tp->weather = CLEAR_SKY;/* no clouds */

	/*            / 2*PI  / arctan(rs/d)
	 * sun_sang = |       |
	 *	      |       |     sin(theta) d-theta d-phi
	 *	     /       /
	 *
	 *	    = 2*PI*(1 - cos(arctan(rs/d)))
	 *
	 *          = 2*PI*(1 - cos(arctan(695300/149000000)))
	 *
	 *	    = 2*PI*(1 - cos(.00466640908179121739))
	 *
	 *	    = 2*PI*(1 - .99998911233289762807)
	 *
	 *	    = .00006840922996708585320208283043854326275346156491
	 *
	 *   where rs = radius of the sun (km),
	 *         d  = distance from earth to sun (km).
	 *
	 * (This term is cancelled out in calculations it turns out.)
	 */
	tp->sun_sang = 6.840922996708585e-5;	/* in steradians */
	tp->index_refrac = 1.2;
	(void)strcpy( tp->material, "junk" );
	VSET(tp->Zenith, 0., 0., 1.);

	if (bu_struct_parse(matparm, toyota_parse, (char *)tp) < 0)  {
		bu_free((char *)tp, "toyota_specific");
		return(-1);
	}

	/* Read in reflectance data. */
	if (tp->material[0] == '/') {
		/* Do nothing, user has his own reflectance data. */
		strcpy(mfile, tp->material);
	} else {
		/* Look for reflectance data in usual place. */
		strcpy(mfile, "/m/cad/material/");
		strcat(mfile, tp->material);
		strcat(mfile, "/reflectance");
	}
	if ((fp = fopen(mfile, "r")) == NULL) {
		perror(mfile);
		bu_log("reflectance: cannot open %s for reading.", mfile);
		rt_bomb("");
	}
	if (fscanf(fp, "%d", &lines) != 1) {
		bu_log("toyota_setup: no data in %s\n", mfile);
		rt_bomb("");
	}
	tp->refl_lines = lines;
	tp->refl = (fastf_t *)bu_malloc(sizeof(fastf_t)*lines*3, "refl[]");
	i = 0;
	while (fscanf(fp, "%lf %lf %lf", &l, &a, &b) == 3 && i < lines*3) {
		tp->refl[i] = l;
		tp->refl[i+1] = a;
		tp->refl[i+2] = b;
		i += 3;
	}
	fclose(fp);

	if (strncmp("glass", tp->material, 5) == 0)  {
		tp->glass = 1;
	} else if (tp->material[0] == '/' )  {
		char *cp = strrchr(tp->material, '/')+1;
		if (strncmp("glass", cp, 5) == 0)
			tp->glass = 1;
	} else {
		tp->glass = 0;
	}
	return(1);
}

/*
 *	M I R R O R _ S E T U P
 */
HIDDEN int
tmirror_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)




                                /* New since 4.4 release */
{
	register struct toyota_specific *pp;

	BU_CK_VLS(matparm);
	BU_GETSTRUCT(pp, toyota_specific);
	*dpp = (char *)pp;

	/* use function, fn(lambda), describing spectral */
	/* reflectance of mirror */

	return(1);
}

/*
 *	G L A S S _ S E T U P
 */
HIDDEN int
tglass_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)




                                /* New since 4.4 release */
{
	register struct toyota_specific *pp;

	BU_CK_VLS(matparm);
	BU_GETSTRUCT(pp, toyota_specific);
	*dpp = (char *)pp;

	/* use function, fn(lambda), describing spectral */
	/* reflectance of glass */

	return(1);
}

/*
 *	T O Y O T A _ P R I N T
 */
HIDDEN void
toyota_print(register struct region *rp, char *dp)
{
	bu_struct_print(rp->reg_name, toyota_parse, (char *)dp);
}

/*
 *	T O Y O T A _ F R E E
 */
HIDDEN void
toyota_free(char *cp)
{
	/* need to free cp->refl */
	bu_free(cp, "toyota_specific");
}

/*
 *	A I R _ M A S S
 *
 *	Return the relative optical air mass as a function of solar
 *	altitude.  See Kasten, Fritz "Archiv fuer Meterologie Geophysik
 *	und Bioklimateorie, Ser. B", Vol. 14, p. 14, 1966, A New Table and
 *	Approximation Formula for the Relative Optical Air Mass.
 *
 *	Unitless (ratio).
 */
fastf_t
air_mass(fastf_t gamma)
       	      	/* Solar altitude off horizon (degrees). */
{
	fastf_t	m;

	if (gamma <= 0.) {
		bu_log("air_mass: sun altitude of %g degrees ignored.\n");
		m = 0.;
	} else
		m = 1./(sin(gamma*PI/180.)
			+ 0.1500*pow((gamma + 3.885), -1.253));
	return(m);
}

/*
 *	Z E N I T H _ L U M I N A N C E
 *
 *	Return sky luminance at its zenith.
 *
 *	DON'T KNOW HOW TO DO THIS YET!!!
 *	SENT LETTER TO CIE & TOYOTA JULY 7, 1992
 *
 *	Luminance units: cd/m^2
 */
fastf_t
zenith_luminance(fastf_t sun_alt, fastf_t t_vl)
       	        	/* Solar altitude off horizon (degrees). */
       	     		/* atmospheric turbidity (aerosol optical depth) */
{
	return(2000.);	/* swag */
}

/*
 *	O V E R C A S T _ S K Y _ L U M
 *
 *	CIE Standard sky luminance function.  Sky covered with clouds
 *	so thick that the sun cannot be seen.
 *	Luminance units: cd/m^2
 *
 *	Taken from "Continuous Tone Representation of Three-Dimensional
 *	Objects Illuminated by Sky Light" in "Computer Graphics, Volume 20,
 *	Number 4, August 1986, pp. 127-8."
 */
fastf_t
overcast_sky_lum(fastf_t lz, fastf_t *Zenith, fastf_t *Sky_elmt)
       	   			/* luminance of the zenith */
      	                 	/* vectors to zenith and a sky element */
{
	return(lz * (1. + 2.*VDOT(Zenith, Sky_elmt)/3.));
}

/*	H O M O G E N O U S _ S K Y _ L U M
 *
 *	Intermediate homogenous sky light luminance function.  Sky in
 *	which weather homogenously changes between clear and overcast skies
 *	without clouds scattered in the sky.
 *	Luminance units: cd/m^2
 *
 *	DON'T KNOW HOW TO DO THIS YET!!!
 *	SENT LETTER TO CIE & TOYOTA JULY 7, 1992
 */
fastf_t
homogenous_sky_lum(fastf_t *Sky_elmt, fastf_t *Sun, fastf_t t_vl)
      	              	/* vectors to a sky element and to sun */
       	     		/* Turbidity factor. */
{
	return(0.);
}

/*
 *	C L E A R _ S K Y _ L U M
 *
 *	CIE Standard sky luminance function.  Sky free from clouds.
 *	Luminance units: cd/m^2
 *
 *	Taken from "Continuous Tone Representation of Three-Dimensional
 *	Objects Illuminated by Sky Light" in "Computer Graphics, Volume 20,
 *	Number 4, August 1986, pp. 127-8."
 */
fastf_t
clear_sky_lum(fastf_t lz, fastf_t *Sky_elmt, fastf_t *Sun, fastf_t *Zenith)
       	   		/* luminance of the zenith */
      	              	/* vectors to a sky element and to sun */
      	       		/* vector to zenith */
{
	fastf_t	cos_gamma;	/* cos(gamma) */
	fastf_t	cos_z0;		/* cos(z0) */
	fastf_t	cos_theta;	/* cos of angle between zenith & sky element */
	fastf_t	gamma;		/* angle from sun to a sky element */
	fastf_t	lum;		/* luminance */
	fastf_t	z0;		/* angle from zenith to the sun */

	cos_gamma = VDOT(Sun, Sky_elmt);
	cos_theta = VDOT(Sky_elmt, Zenith);
	cos_z0 = VDOT(Zenith, Sun);
	z0 = acos(cos_z0);
	gamma = acos(cos_gamma);

	lum =
		lz
		* (0.91 + 10*exp(-3.*gamma) + 0.45*cos_gamma*cos_gamma)
		* (1. - exp(-0.32/cos_theta))
		/ 0.27385*(0.91 + 10.*exp(-3.*z0) + 0.45*cos_z0*cos_z0);
#if 0
bu_log("clear_sky_lum(lz=%g, ...) = %g\n", lz, lum );
#endif

	return(lum);
}

/*
 *	A T M O S _ I R R A D I A N C E
 *
 *	Table of solar irradiance values taken from WMO - No. 590,
 *	"Commission for Instruments and Methods of Observation", Abridged
 *	Final Report of the Eigth Session, Mexico City, 19-30 October 1981,
 *	pp. 71-5.
 *
 *	Return the solar spectral irradiance through the atmosphere
 *	for a given wavelength of light.
 *	Units: W/m^2
 */
fastf_t
atmos_irradiance(fastf_t lambda)
{
#define NSIRRAD 992 /* Number of entries in solar irradiance table. */

	fastf_t		irrad;
	fastf_t		ratio;
	int		hi, j, lo;
	/* Table row: wavelength, solar irradiance. */
	static fastf_t	table[NSIRRAD][2] = {
		{25000.00,	.72506},
		{10000.00,	.02389},
		{9900.00,	.02479},
		{9800.00,	.02589},
		{9700.00,	.02689},
		{9600.00,	.02814},
		{9500.00,	.02904},
		{9400.00,	.02959},
		{9300.00,	.03085},
		{9200.00,	.03275},
		{9100.00,	.03475},
		{9000.00,	.03650},
		{8900.00,	.03816},
		{8800.00,	.03986},
		{8700.00,	.04111},
		{8600.00,	.04266},
		{8500.00,	.04477},
		{8400.00,	.04697},
		{8300.00,	.04917},
		{8200.00,	.05243},
		{8100.00,	.05443},
		{8000.00,	.05723},
		{7900.00,	.06014},
		{7800.00,	.06344},
		{7700.00,	.06590},
		{7600.00,	.06975},
		{7500.00,	.07411},
		{7400.00,	.07746},
		{7300.00,	.08157},
		{7200.00,	.08668},
		{7100.00,	.09138},
		{7000.00,	.09659},
		{6900.00,	.10160},
		{6800.00,	.10846},
		{6700.00,	.11432},
		{6600.00,	.12158},
		{6500.00,	.12899},
		{6400.00,	.13690},
		{6300.00,	.14596},
		{6200.00,	.15553},
		{6100.00,	.16529},
		{6000.00,	.17641},
		{5900.00,	.18843},
		{5800.00,	.20140},
		{5700.00,	.21577},
		{5600.00,	.23119},
		{5500.00,	.24821},
		{5400.00,	.26694},
		{5300.00,	.28697},
		{5200.00,	.30925},
		{5100.00,	.33374},
		{5000.00,	.06965},
		{4980.00,	.07055},
		{4960.00,	.07186},
		{4940.00,	.07296},
		{4920.00,	.07431},
		{4900.00,	.07556},
		{4800.00,	.07671},
		{4860.00,	.07801},
		{4840.00,	.07947},
		{4820.00,	.08077},
		{4800.00,	.08222},
		{4780.00,	.08357},
		{4760.00,	.08512},
		{4740.00,	.08653},
		{4720.00,	.08798},
		{4700.00,	.08958},
		{4680.00,	.09113},
		{4660.00,	.09279},
		{4640.00,	.09444},
		{4620.00,	.09624},
		{4600.00,	.09784},
		{4580.00,	.09965},
		{4560.00,	.10150},
		{4540.00,	.10330},
		{4520.00,	.10525},
		{4500.00,	.10721},
		{4480.00,	.10921},
		{4460.00,	.11116},
		{4440.00,	.11322},
		{4420.00,	.11547},
		{4400.00,	.11762},
		{4380.00,	.11978},
		{4360.00,	.12213},
		{4340.00,	.12448},
		{4320.00,	.12679},
		{4300.00,	.12924},
		{4280.00,	.13174},
		{4260.00,	.13430},
		{4240.00,	.13695},
		{4220.00,	.13950},
		{4200.00,	.14231},
		{4180.00,	.14526},
		{4160.00,	.14807},
		{4140.00,	.15107},
		{4120.00,	.15403},
		{4100.00,	.15718},
		{4080.00,	.16039},
		{4060.00,	.16364},
		{4040.00,	.16700},
		{4020.00,	.17060},
		{4000.00,	.17376},
		{3980.00,	.17686},
		{3960.00,	.18026},
		{3940.00,	.18357},
		{3920.00,	.18707},
		{3900.00,	.19053},
		{3880.00,	.19424},
		{3860.00,	.19804},
		{3840.00,	.20175},
		{3820.00,	.20575},
		{3800.00,	.20976},
		{3780.00,	.21391},
		{3760.00,	.21817},
		{3740.00,	.22243},
		{3720.00,	.22693},
		{3700.00,	.23139},
		{3680.00,	.23610},
		{3660.00,	.24080},
		{3640.00,	.24591},
		{3620.00,	.25067},
		{3600.00,	.25593},
		{3580.00,	.26133},
		{3560.00,	.26654},
		{3540.00,	.27220},
		{3520.00,	.27796},
		{3500.00,	.28387},
		{3480.00,	.29003},
		{3460.00,	.29623},
		{3440.00,	.30264},
		{3420.00,	.30920},
		{3400.00,	.31586},
		{3380.00,	.32282},
		{3360.00,	.33003},
		{3340.00,	.33745},
		{3320.00,	.34496},
		{3300.00,	.35277},
		{3280.00,	.36063},
		{3260.00,	.36894},
		{3240.00,	.37735},
		{3220.00,	.38612},
		{3200.00,	.39528},
		{3180.00,	.40454},
		{3160.00,	.41401},
		{3140.00,	.42377},
		{3120.00,	.43384},
		{3100.00,	.44415},
		{3080.00,	.45502},
		{3060.00,	.46598},
		{3040.00,	.47740},
		{3020.00,	.48922},
		{3000.00,	.50144},
		{2980.00,	.51398},
		{2960.00,	.52680},
		{2940.00,	.54014},
		{2920.00,	.55401},
		{2900.00,	.56821},
		{2880.00,	.58281},
		{2860.00,	.59805},
		{2840.00,	.61378},
		{2820.00,	.63002},
		{2800.00,	.64675},
		{2780.00,	.66405},
		{2760.00,	.68188},
		{2740.00,	.70035},
		{2720.00,	.71951},
		{2700.00,	.73934},
		{2680.00,	.75994},
		{2660.00,	.78120},
		{2640.00,	.80325},
		{2620.00,	.82621},
		{2600.00,	.84980},
		{2580.00,	.87431},
		{2560.00,	.89997},
		{2540.00,	.92633},
		{2520.00,	.95377},
		{2500.00,	.98254},
		{2480.00,	1.01619},
		{2460.00,	.99909},
		{2440.00,	1.09984},
		{2420.00,	1.09744},
		{2400.00,	1.15632},
		{2380.00,	1.19330},
		{2360.00,	1.22515},
		{2340.00,	1.12220},
		{2320.00,	1.21063},
		{2300.00,	1.31874},
		{2280.00,	1.35071},
		{2260.00,	1.44179},
		{2240.00,	1.42464},
		{2220.00,	1.52048},
		{2200.00,	1.46643},
		{2180.00,	1.59920},
		{2160.00,	1.65085},
		{2140.00,	1.74686},
		{2120.00,	1.80843},
		{2100.00,	1.90680},
		{2080.00,	1.97332},
		{2060.00,	2.09129},
		{2040.00,	2.20196},
		{2020.00,	2.25371},
		{2000.00,	.59725},
		{1995.00,	.60954},
		{1990.00,	.60954},
		{1985.00,	.61938},
		{1980.00,	.63661},
		{1975.00,	.63663},
		{1970.00,	.62682},
		{1965.00,	.62930},
		{1960.00,	.61948},
		{1955.00,	.62191},
		{1950.00,	.63663},
		{1945.00,	.64402},
		{1940.00,	.65388},
		{1935.00,	.66127},
		{1930.00,	.66372},
		{1925.00,	.66866},
		{1920.00,	.67852},
		{1915.00,	.68836},
		{1910.00,	.68836},
		{1905.00,	.67607},
		{1900.00,	.68097},
		{1895.00,	.69081},
		{1890.00,	.69084},
		{1885.00,	.69825},
		{1880.00,	.69089},
		{1875.00,	.67860},
		{1870.00,	.69827},
		{1865.00,	.71795},
		{1860.00,	.72289},
		{1855.00,	.73520},
		{1850.00,	.74995},
		{1845.00,	.76222},
		{1840.00,	.75977},
		{1835.00,	.76718},
		{1830.00,	.78195},
		{1825.00,	.78933},
		{1820.00,	.79920},
		{1815.00,	.80168},
		{1810.00,	.82136},
		{1805.00,	.84351},
		{1800.00,	.85581},
		{1795.00,	.85581},
		{1790.00,	.84847},
		{1785.00,	.85340},
		{1780.00,	.86076},
		{1775.00,	.87554},
		{1770.00,	.90015},
		{1765.00,	.91983},
		{1760.00,	.93708},
		{1755.00,	.94446},
		{1750.00,	.93462},
		{1745.00,	.94451},
		{1740.00,	.95438},
		{1735.00,	.95192},
		{1730.00,	.96915},
		{1725.00,	1.00603},
		{1720.00,	1.04541},
		{1715.00,	1.04050},
		{1710.00,	1.04053},
		{1705.00,	1.07498},
		{1700.00,	1.07989},
		{1695.00,	1.08482},
		{1690.00,	1.09711},
		{1685.00,	1.10207},
		{1680.00,	1.10455},
		{1675.00,	1.12177},
		{1670.00,	1.14395},
		{1665.00,	1.15627},
		{1660.00,	1.16859},
		{1655.00,	1.17107},
		{1650.00,	1.17355},
		{1645.00,	1.17357},
		{1640.00,	1.18096},
		{1635.00,	1.20064},
		{1630.00,	1.21786},
		{1625.00,	1.21543},
		{1620.00,	1.21298},
		{1615.00,	1.22282},
		{1610.00,	1.22039},
		{1605.00,	1.22780},
		{1600.00,	1.23789},
		{1595.00,	1.23369},
		{1590.00,	1.24698},
		{1585.00,	1.27011},
		{1580.00,	1.28837},
		{1575.00,	1.30169},
		{1570.00,	1.31253},
		{1565.00,	1.33581},
		{1560.00,	1.35664},
		{1555.00,	1.36510},
		{1550.00,	1.37111},
		{1545.00,	1.37710},
		{1540.00,	1.37815},
		{1535.00,	1.39415},
		{1530.00,	1.43248},
		{1525.00,	1.44347},
		{1520.00,	1.44457},
		{1515.00,	1.45564},
		{1510.00,	1.46670},
		{1505.00,	1.48030},
		{1500.00,	1.49392},
		{1495.00,	1.51255},
		{1490.00,	1.50869},
		{1485.00,	1.50731},
		{1480.00,	1.52847},
		{1475.00,	1.54965},
		{1470.00,	1.56084},
		{1465.00,	1.57459},
		{1460.00,	1.56319},
		{1455.00,	1.57949},
		{1450.00,	1.60836},
		{1445.00,	1.61710},
		{1440.00,	1.64852},
		{1435.00,	1.67501},
		{1430.00,	1.71667},
		{1425.00,	1.73057},
		{1420.00,	1.72175},
		{1415.00,	1.73062},
		{1410.00,	1.74454},
		{1405.00,	1.76101},
		{1400.00,	1.78009},
		{1395.00,	1.79413},
		{1390.00,	1.81071},
		{1385.00,	1.82473},
		{1380.00,	1.83622},
		{1375.00,	1.84776},
		{1370.00,	1.85167},
		{1365.00,	1.87598},
		{1360.00,	1.90545},
		{1355.00,	1.92728},
		{1350.00,	1.95935},
		{1345.00,	1.98637},
		{1340.00,	2.00061},
		{1335.00,	2.01744},
		{1330.00,	2.04455},
		{1325.00,	2.07422},
		{1320.00,	2.08599},
		{1315.00,	2.09267},
		{1310.00,	2.12504},
		{1305.00,	2.17289},
		{1300.00,	2.19728},
		{1295.00,	2.20817},
		{1290.00,	2.20586},
		{1285.00,	2.18811},
		{1280.00,	2.22702},
		{1275.00,	2.23501},
		{1270.00,	2.19665},
		{1265.00,	2.20982},
		{1260.00,	2.27449},
		{1255.00,	2.34947},
		{1250.00,	2.35491},
		{1245.00,	2.36347},
		{1240.00,	2.39271},
		{1235.00,	2.40869},
		{1230.00,	2.41800},
		{1225.00,	2.41267},
		{1220.00,	2.46066},
		{1215.00,	2.48077},
		{1210.00,	2.46524},
		{1205.00,	2.48264},
		{1200.00,	2.50255},
		{1195.00,	2.54010},
		{1190.00,	2.56496},
		{1185.00,	2.57288},
		{1180.00,	2.60708},
		{1175.00,	2.65420},
		{1170.00,	2.66601},
		{1165.00,	2.68286},
		{1160.00,	2.74250},
		{1155.00,	2.75429},
		{1150.00,	2.75855},
		{1145.00,	2.79045},
		{1140.00,	2.80977},
		{1135.00,	2.83161},
		{1130.00,	2.84585},
		{1125.00,	2.87770},
		{1120.00,	2.92710},
		{1115.00,	2.98648},
		{1110.00,	3.01826},
		{1105.00,	3.03248},
		{1100.00,	3.02659},
		{1095.00,	3.03573},
		{1090.00,	3.07248},
		{1085.00,	3.09417},
		{1080.00,	3.13585},
		{1075.00,	3.17982},
		{1070.00,	3.21144},
		{1065.00,	3.22118},
		{1060.00,	3.23810},
		{1055.00,	3.28527},
		{1050.00,	3.37350},
		{1045.00,	3.42433},
		{1040.00,	3.44123},
		{1035.00,	3.45900},
		{1030.00,	3.49964},
		{1025.00,	3.53842},
		{1020.00,	3.57139},
		{1015.00,	3.64688},
		{1010.00,	3.68679},
		{1005.00,	3.71518},
		{1000.00,	3.72852},
		{995.00,	3.79626},
		{990.00,	3.81926},
		{985.00,	3.84833},
		{980.00,	3.84504},
		{975.00,	3.80245},
		{970.00,	3.84045},
		{965.00,	3.83984},
		{960.00,	3.86656},
		{955.00,	3.85118},
		{950.00,	3.92912},
		{945.00,	3.97737},
		{940.00,	4.03069},
		{935.00,	4.13721},
		{930.00,	4.15959},
		{925.00,	4.13519},
		{920.00,	4.27664},
		{915.00,	4.36772},
		{910.00,	4.38679},
		{905.00,	4.51286},
		{900.00,	4.61964},
		{895.00,	4.69612},
		{890.00,	4.75038},
		{885.00,	4.79674},
		{880.00,	4.85967},
		{875.00,	4.91975},
		{870.00,	4.80462},
		{865.00,	4.95842},
		{860.00,	5.06243},
		{855.00,	4.82633},
		{850.00,	5.10683},
		{845.00,	5.08209},
		{840.00,	5.12503},
		{835.00,	5.15348},
		{830.00,	5.25912},
		{825.00,	5.30549},
		{820.00,	5.38867},
		{815.00,	5.52502},
		{810.00,	5.61305},
		{805.00,	5.72199},
		{800.00,	1.14525},
		{799.00,	1.14639},
		{798.00,	1.15026},
		{797.00,	1.15517},
		{796.00,	1.15412},
		{795.00,	1.17566},
		{794.00,	1.15308},
		{793.00,	1.11129},
		{792.00,	1.13051},
		{791.00,	1.13666},
		{790.00,	1.14973},
		{789.00,	1.15286},
		{788.00,	1.16280},
		{787.00,	1.16576},
		{786.00,	1.17273},
		{785.00,	1.17720},
		{784.00,	1.17970},
		{783.00,	1.17325},
		{782.00,	1.18221},
		{781.00,	1.19437},
		{780.00,	1.19116},
		{779.00,	1.20813},
		{778.00,	1.18794},
		{777.00,	1.14998},
		{776.00,	1.16775},
		{775.00,	1.18614},
		{774.00,	1.18553},
		{773.00,	1.17829},
		{772.00,	1.18492},
		{771.00,	1.18526},
		{770.00,	1.19155},
		{769.00,	1.18482},
		{768.00,	1.19784},
		{767.00,	1.20924},
		{766.00,	1.21086},
		{765.00,	1.19780},
		{764.00,	1.21247},
		{763.00,	1.23097},
		{762.00,	1.22714},
		{761.00,	1.20968},
		{760.00,	1.22332},
		{759.00,	1.22675},
		{758.00,	1.23696},
		{757.00,	1.23914},
		{756.00,	1.24716},
		{755.00,	1.25022},
		{754.00,	1.25518},
		{753.00,	1.24331},
		{752.00,	1.26015},
		{751.00,	1.26451},
		{750.00,	1.27698},
		{749.00,	1.28908},
		{748.00,	1.28946},
		{747.00,	1.28650},
		{746.00,	1.28984},
		{745.00,	1.29302},
		{744.00,	1.29319},
		{743.00,	1.27635},
		{742.00,	1.29336},
		{741.00,	1.29234},
		{740.00,	1.31036},
		{739.00,	1.29629},
		{738.00,	1.32839},
		{737.00,	1.36420},
		{736.00,	1.36049},
		{735.00,	1.35433},
		{734.00,	1.35677},
		{733.00,	1.35552},
		{732.00,	1.35922},
		{731.00,	1.36585},
		{730.00,	1.36291},
		{729.00,	1.33668},
		{728.00,	1.35997},
		{727.00,	1.37247},
		{726.00,	1.38325},
		{725.00,	1.40749},
		{724.00,	1.39403},
		{723.00,	1.41306},
		{722.00,	1.38057},
		{721.00,	1.32661},
		{720.00,	1.34807},
		{719.00,	1.36114},
		{718.00,	1.36953},
		{717.00,	1.36779},
		{716.00,	1.37792},
		{715.00,	1.37511},
		{714.00,	1.38806},
		{713.00,	1.39009},
		{712.00,	1.40101},
		{711.00,	1.44352},
		{710.00,	1.41192},
		{709.00,	1.36238},
		{708.00,	1.38033},
		{707.00,	1.37736},
		{706.00,	1.39827},
		{705.00,	1.42790},
		{704.00,	1.41918},
		{703.00,	1.39298},
		{702.00,	1.41241},
		{701.00,	1.42990},
		{700.00,	1.40560},
		{699.00,	1.46310},
		{698.00,	1.47849},
		{697.00,	1.43771},
		{696.00,	1.45109},
		{695.00,	1.46484},
		{694.00,	1.43486},
		{693.00,	1.41952},
		{692.00,	1.41664},
		{691.00,	1.44080},
		{690.00,	1.41801},
		{689.00,	1.38104},
		{688.00,	1.37581},
		{687.00,	1.41410},
		{686.00,	1.44913},
		{685.00,	1.44300},
		{684.00,	1.46830},
		{683.00,	1.46116},
		{682.00,	1.46852},
		{681.00,	1.48219},
		{680.00,	1.47857},
		{679.00,	1.46307},
		{678.00,	1.50134},
		{677.00,	1.47300},
		{676.00,	1.49683},
		{675.00,	1.53461},
		{674.00,	1.48491},
		{673.00,	1.44037},
		{672.00,	1.47113},
		{671.00,	1.51041},
		{670.00,	1.52216},
		{669.00,	1.51527},
		{668.00,	1.54030},
		{667.00,	1.52621},
		{666.00,	1.57673},
		{665.00,	1.55345},
		{664.00,	1.58544},
		{663.00,	1.57047},
		{662.00,	1.57889},
		{661.00,	1.57011},
		{660.00,	1.53724},
		{659.00,	1.53194},
		{658.00,	1.49228},
		{657.00,	1.32164},
		{656.00,	1.42139},
		{655.00,	1.54562},
		{654.00,	1.62637},
		{653.00,	1.57986},
		{652.00,	1.61729},
		{651.00,	1.63040},
		{650.00,	1.53595},
		{649.00,	1.60642},
		{648.00,	1.60444},
		{647.00,	1.59691},
		{646.00,	1.63982},
		{645.00,	1.62074},
		{644.00,	1.62888},
		{643.00,	1.64789},
		{642.00,	1.59696},
		{641.00,	1.58633},
		{640.00,	1.63895},
		{639.00,	1.67531},
		{638.00,	1.64694},
		{637.00,	1.67149},
		{636.00,	1.64827},
		{635.00,	1.67492},
		{634.00,	1.62610},
		{633.00,	1.70191},
		{632.00,	1.62718},
		{631.00,	1.60537},
		{630.00,	1.65151},
		{629.00,	1.67118},
		{628.00,	1.69006},
		{627.00,	1.71263},
		{626.00,	1.62812},
		{625.00,	1.61401},
		{624.00,	1.68983},
		{623.00,	1.68857},
		{622.00,	1.68940},
		{621.00,	1.67615},
		{620.00,	1.75682},
		{619.00,	1.73019},
		{618.00,	1.71626},
		{617.00,	1.64209},
		{616.00,	1.69330},
		{615.00,	1.73164},
		{614.00,	1.68130},
		{613.00,	1.73872},
		{612.00,	1.72727},
		{611.00,	1.74206},
		{610.00,	1.72421},
		{609.00,	1.66239},
		{608.00,	1.78557},
		{607.00,	1.73555},
		{606.00,	1.74974},
		{605.00,	1.71831},
		{604.00,	1.80630},
		{603.00,	1.74145},
		{602.00,	1.69186},
		{601.00,	1.75060},
		{600.00,	1.71290},
		{599.00,	1.72688},
		{598.00,	1.75866},
		{597.00,	1.83835},
		{596.00,	1.76154},
		{595.00,	1.80773},
		{594.00,	1.76196},
		{593.00,	1.77442},
		{592.00,	1.79407},
		{591.00,	1.82231},
		{590.00,	1.60829},
		{589.00,	1.73918},
		{588.00,	1.81230},
		{587.00,	1.79763},
		{586.00,	1.77402},
		{585.00,	1.84529},
		{584.00,	1.84870},
		{583.00,	1.84148},
		{582.00,	1.86959},
		{581.00,	1.84871},
		{580.00,	1.81983},
		{579.00,	1.82467},
		{578.00,	1.77838},
		{577.00,	1.83666},
		{576.00,	1.87205},
		{575.00,	1.85126},
		{574.00,	1.83439},
		{573.00,	1.91009},
		{572.00,	1.81549},
		{571.00,	1.75498},
		{570.00,	1.88340},
		{569.00,	1.81645},
		{568.00,	1.85804},
		{567.00,	1.84184},
		{566.00,	1.82646},
		{565.00,	1.84641},
		{564.00,	1.87654},
		{563.00,	1.88301},
		{562.00,	1.82558},
		{561.00,	1.86677},
		{560.00,	1.85019},
		{559.00,	1.82068},
		{558.00,	1.82223},
		{557.00,	1.84785},
		{556.00,	1.91617},
		{555.00,	1.90149},
		{554.00,	1.92922},
		{553.00,	1.84554},
		{552.00,	1.90498},
		{551.00,	1.86042},
		{550.00,	1.93179},
		{549.00,	1.90540},
		{548.00,	1.84846},
		{547.00,	1.91547},
		{546.00,	1.87116},
		{545.00,	1.93473},
		{544.00,	1.88832},
		{543.00,	1.89407},
		{542.00,	1.85358},
		{541.00,	1.81955},
		{540.00,	1.83170},
		{539.00,	1.93188},
		{538.00,	1.91722},
		{537.00,	1.88985},
		{536.00,	2.01246},
		{535.00,	1.90310},
		{534.00,	1.91707},
		{533.00,	1.83936},
		{532.00,	1.95080},
		{531.00,	2.00163},
		{530.00,	1.90818},
		{529.00,	1.93233},
		{528.00,	1.85343},
		{527.00,	1.70982},
		{526.00,	1.92169},
		{525.00,	1.95513},
		{524.00,	1.93306},
		{523.00,	1.90316},
		{522.00,	1.88478},
		{521.00,	1.87583},
		{520.00,	1.80397},
		{519.00,	1.65066},
		{518.00,	1.75840},
		{517.00,	1.63891},
		{516.00,	1.90975},
		{515.00,	1.85825},
		{514.00,	1.87383},
		{513.00,	1.86534},
		{512.00,	1.98578},
		{511.00,	1.94355},
		{510.00,	1.95217},
		{509.00,	1.92814},
		{508.00,	1.91922},
		{507.00,	1.93144},
		{506.00,	2.00576},
		{505.00,	1.92417},
		{504.00,	1.89776},
		{503.00,	1.93792},
		{502.00,	1.81978},
		{501.00,	1.81488},
		{500.00,	1.93810},
		{499.00,	1.88115},
		{498.00,	2.01008},
		{497.00,	1.96528},
		{496.00,	1.88415},
		{495.00,	2.06915},
		{494.00,	1.97487},
		{493.00,	1.86509},
		{492.00,	1.82987},
		{491.00,	1.97188},
		{490.00,	2.01005},
		{489.00,	1.86863},
		{488.00,	1.88769},
		{487.00,	1.65586},
		{486.00,	1.82825},
		{485.00,	2.00517},
		{484.00,	2.00703},
		{483.00,	2.00383},
		{482.00,	2.07663},
		{481.00,	2.02201},
		{480.00,	2.09844},
		{479.00,	2.02674},
		{478.00,	2.05672},
		{477.00,	1.97211},
		{476.00,	2.00328},
		{475.00,	2.03169},
		{474.00,	2.00207},
		{473.00,	2.03415},
		{472.00,	2.00717},
		{471.00,	1.89753},
		{470.00,	2.00753},
		{469.00,	2.01253},
		{468.00,	1.99016},
		{467.00,	1.92453},
		{466.00,	1.99046},
		{465.00,	1.96616},
		{464.00,	2.05647},
		{463.00,	2.11132},
		{462.00,	2.09523},
		{461.00,	2.05107},
		{460.00,	1.99193},
		{459.00,	1.98629},
		{458.00,	2.05302},
		{457.00,	2.07690},
		{456.00,	2.03659},
		{455.00,	2.02841},
		{454.00,	1.96426},
		{453.00,	1.92493},
		{452.00,	2.14598},
		{451.00,	2.22018},
		{450.00,	2.02552},
		{449.00,	2.00552},
		{448.00,	1.99823},
		{447.00,	1.92336},
		{446.00,	1.81465},
		{445.00,	1.97037},
		{444.00,	1.92663},
		{443.00,	1.97709},
		{442.00,	1.90977},
		{441.00,	1.72818},
		{440.00,	1.78344},
		{439.00,	1.60128},
		{438.00,	1.75646},
		{437.00,	1.94545},
		{436.00,	1.83281},
		{435.00,	1.70941},
		{434.00,	1.71971},
		{433.00,	1.59093},
		{432.00,	1.73931},
		{431.00,	1.18114},
		{430.00,	1.43179},
		{429.00,	1.61524},
		{428.00,	1.55855},
		{427.00,	1.68954},
		{426.00,	1.67122},
		{425.00,	1.83233},
		{424.00,	1.66385},
		{423.00,	1.62314},
		{422.00,	1.93380},
		{421.00,	1.73978},
		{420.00,	1.73008},
		{419.00,	1.58521},
		{418.00,	1.60288},
		{417.00,	1.95848},
		{416.00,	1.69074},
		{415.00,	1.75825},
		{414.00,	1.72855},
		{413.00,	1.78972},
		{412.00,	1.84721},
		{411.00,	1.60548},
		{410.00,	1.68896},
		{409.00,	1.70336},
		{408.00,	1.64855},
		{407.00,	1.60358},
		{406.00,	1.66262},
		{405.00,	1.61197},
		{404.00,	1.66177},
		{403.00,	1.69945},
		{402.00,	1.85464},
		{401.00,	1.74765},
		{400.00,	1.65557},
		{399.00,	1.55885},
		{398.00,	.98892},
		{397.00,	.73157},
		{396.00,	1.36058},
		{395.00,	1.16506},
		{394.00,	.51918},
		{393.00,	.96770},
		{392.00,	1.35593},
		{391.00,	1.20391},
		{390.00,	1.27662},
		{389.00,	.95946},
		{388.00,	1.03225},
		{387.00,	.89680},
		{386.00,	1.00866},
		{385.00,	1.03205},
		{384.00,	.71553},
		{383.00,	.72384},
		{382.00,	1.06938},
		{381.00,	1.26323},
		{380.00,	1.09391},
		{379.00,	1.42018},
		{378.00,	1.37788},
		{377.00,	1.24686},
		{376.00,	1.14902},
		{375.00,	.91977},
		{374.00,	.85347},
		{373.00,	1.04017},
		{372.00,	1.19835},
		{371.00,	1.10971},
		{370.00,	1.23693},
		{369.00,	1.11109},
		{368.00,	1.16056},
		{367.00,	1.29568},
		{366.00,	1.24442},
		{365.00,	.94159},
		{364.00,	1.03171},
		{363.00,	1.13020},
		{362.00,	.96457},
		{361.00,	1.07451},
		{360.00,	1.07420},
		{359.00,	.64928},
		{358.00,	.85466},
		{357.00,	.92873},
		{356.00,	1.07914},
		{355.00,	1.19326},
		{354.00,	1.10298},
		{353.00,	.93729},
		{352.00,	.94835},
		{351.00,	1.06738},
		{350.00,	.90599},
		{349.00,	.89630},
		{348.00,	.93721},
		{347.00,	.87770},
		{346.00,	.96477},
		{345.00,	.69647},
		{344.00,	.93273},
		{343.00,	.95317},
		{342.00,	.92445},
		{341.00,	1.01743},
		{340.00,	.96269},
		{339.00,	.93752},
		{338.00,	.83161},
		{337.00,	.77820},
		{336.00,	.94669},
		{335.00,	.92177},
		{334.00,	.93237},
		{333.00,	.96663},
		{332.00,	.94620},
		{331.00,	1.05837},
		{330.00,	1.06780},
		{329.00,	.92378},
		{328.00,	1.03023},
		{327.00,	1.08882},
		{326.00,	.74511},
		{325.00,	.64021},
		{324.00,	.61387},
		{323.00,	.74378},
		{322.00,	.74378},
		{321.00,	.82054},
		{320.00,	.74435},
		{319.00,	.68167},
		{318.00,	.80912},
		{317.00,	.62832},
		{316.00,	.71710},
		{315.00,	.69875},
		{314.00,	.69629},
		{313.00,	.66662},
		{312.00,	.72524},
		{311.00,	.58104},
		{310.00,	.50548},
		{309.00,	.59188},
		{308.00,	.67086},
		{307.00,	.54720},
		{306.00,	.56630},
		{305.00,	.51899},
		{304.00,	.56413},
		{303.00,	.50060},
		{302.00,	.61720},
		{301.00,	.50544},
		{300.00,	.49927},
		{299.00,	.49424},
		{298.00,	.54290},
		{297.00,	.52618},
		{296.00,	.59232},
		{295.00,	.54323},
		{294.00,	.56829},
		{293.00,	.55761},
		{292.00,	.60710},
		{291.00,	.63478},
		{290.00,	.52521},
		{289.00,	.37398},
		{288.00,	.36223},
		{287.00,	.36138},
		{286.00,	.20862},
		{285.00,	.24310},
		{284.00,	.34419},
		{283.00,	.32759},
		{282.00,	.24273},
		{281.00,	.13202},
		{280.00,	.10385},
		{279.00,	.17168},
		{278.00,	.24958},
		{277.00,	.27124},
		{276.00,	.21274},
		{275.00,	.15652},
		{274.00,	.21112},
		{273.00,	.23353},
		{272.00,	.24674},
		{271.00,	.30166},
		{270.00,	.27652},
		{269.00,	.27532},
		{268.00,	.28459},
		{267.00,	.28227},
		{266.00,	.29209},
		{265.00,	.28081},
		{264.00,	.19431},
		{263.00,	.12632},
		{262.00,	.11230},
		{261.00,	.10629},
		{260.00,	.12267},
		{259.00,	.14837},
		{258.00,	.14702},
		{257.00,	.12463},
		{256.00,	.09759},
		{255.00,	.07485},
		{254.00,	.06496},
		{253.00,	.05415},
		{252.00,	.05655},
		{251.00,	.06992},
		{250.00,	2.51369}
	};

	if (lambda < 250. || lambda > 25000.)
		rt_bomb("atmos_irradiance: bad wavelength.");
	{
		/* Find index of lower lambda in table. */
		lo = 0;
		hi = NSIRRAD - 1;
		j = (NSIRRAD - 1) >> 1;
		while (hi - lo > 1) {
			if (table[j][0] > lambda)
				lo = j;
			else if (table[j][0] <= lambda)
				hi = j;
			j = lo + ((hi - lo) >> 1);
		}
		/* Do linear interpolation to find approximate values. */
		ratio = (lambda - table[j][0]) / (table[j+1][0] - table[j][0]);
		irrad = ratio*(table[j+1][1] - table[j][1]) + table[j][1];
	}
	return(irrad);
}

/*
 *	O Z O N E _ A B S O R P T I O N
 *
 *	Return absorption coefficient due to ozone for a given wavelength
 *	of light.
 *	Units: m ^ -1 (inverse m)
 *
 *	Data from Inn, Edward; Tanaka, Yoshio; Journal of the Optical
 *	Society of America, Volume 43, Number 10, "Absorption Coefficient of
 *	Ozone in the Ultraviolet and Visible Regions," October 1953,
 *	pp. 870-3.
 */
fastf_t
ozone_absorption(fastf_t lambda)
       	       	/* Wavelength of light.  Units: nm. */
{
#define NABSORP 38 /* Number of entries in absorption coefficient table. */

	fastf_t		coeff;
	fastf_t		ratio;
	int		hi, j, lo;
	/* Data in report presented graphically only - I read a hopefully */
	/* representative number of samples off the graph as best I could. */
	/* Table row: wavelength (nm), absorption coefficient (1/cm). */
	static fastf_t	table[NABSORP][2] = {
		{200.,	5.},
		{205.,	6.},
		{210.,	7.},
		{215.,	12.},
		{220.,	20.},
		{225.,	35.},
		{230.,	50.},
		{235.,	75.},
		{240.,	93.},
		{245.,	113.},
		{250.,	127.},
		{255.,	135.},
		{260.,	127.},
		{265.,	112.},
		{270.,	88.},
		{275.,	65.},
		{280.,	45.},
		{285.,	28.},
		{290.,	18.},
		{300.,	7.},
		{325.,	1.},
		{350.,	0.1},
		{375.,	0.0},
		{400.,	0.0},
		{425.,	0.0},
		{450.,	0.002},
		{475.,	0.005},
		{500.,	0.013},
		{525.,	0.024},
		{550.,	0.037},
		{575.,	0.05},
		{600.,	0.055},
		{625.,	0.041},
		{650.,	0.028},
		{675.,	0.016},
		{700.,	0.010},
		{725.,	0.006},
		{750.,	0.004}
	};

	if (lambda < 200. || lambda > 750.) {
		rt_bomb("ozone absorption: bad wavelength.");
	}
	/* Find index of lower lambda in table. */
	lo = 0;
	hi = NABSORP - 1;
	j = (NABSORP - 1) >> 1;
	while (hi - lo > 1) {
		if (table[j][0] <= lambda)
			lo = j;
		else if (table[j][0] > lambda)
			hi = j;
		j = lo + ((hi - lo) >> 1);
	}
	/* Do linear interpolation to find approximate values. */
	ratio = (lambda - table[j][0]) / (table[j+1][0] - table[j][0]);
	coeff = ratio*(table[j+1][1] - table[j][1]) + table[j][1];

	/* Convert units from 1/cm to 1/m */
	return(/* 100. * */ coeff);
}

/*
 *	S P E C T R A L _ D I S T _ T A B L E
 *
 *	Do a table lookup to get data on spectral irradiance of daylight.
 *	Table taken from Judd, D B; MacAdam, D L; Wyszecki, G J; Journal
 *	for Optical Science of America, Vol. 54, 1964, "Spectral Distribution
 *	of Typical Daylight as a Function of Correlated Color Temperature,"
 *	pp. 1031-40.
 */
void
spectral_dist_table(fastf_t lambda, fastf_t *e_mean, fastf_t *v1, fastf_t *v2)
{
	fastf_t		ratio;
	int		j;
	/* Mean and 1st two characteristic vectors of the composite data */
	/* of the spectral absorptance of the earth's atmoshere due to */
	/* ozone and water vapor. */
	/* table row: wavelength (nm), e_mean, v1, v2 */
	/* THESE ARE DIVIDED BY 10 IN "COLOR SCIENCE..." BY WYSZECKI??? */
	static fastf_t	table[][4] = {
		{300.,	0.4,	0.2,	0.0},
		{310.,	60.,	45.,	20.},
		{320.,	296.,	224.,	40.},
		{330.,	553.,	420.,	85.},
		{340.,	573.,	406.,	78.},
		{350.,	618.,	416.,	67.},
		{360.,	615.,	380.,	53.},
		{370.,	688.,	424.,	61.},
		{380.,	634.,	385.,	30.},
		{390.,	658.,	350.,	12.},
		{400.,	949.,	434.,	-11.},
		{410.,	1048.,	463.,	-5.},
		{420.,	1059.,	439.,	-7.},
		{430.,	968.,	371.,	-12.},
		{440.,	1139.,	367.,	-26.},
		{450.,	1256.,	359.,	-29.},
		{460.,	1255.,	326.,	-28.},
		{470.,	1213.,	279.,	-26.},
		{480.,	1213.,	243.,	-26.},
		{490.,	1135.,	201.,	-18.},
		{500.,	1131.,	162.,	-15.},
		{510.,	1108.,	132.,	-13.},
		{520.,	1065.,	86.,	-12.},
		{530.,	1088.,	61.,	-10.},
		{540.,	1053.,	42.,	-5.},
		{550.,	1044.,	19.,	-3.},
		{560.,	1000.,	0.0,	0.0},
		{570.,	960.,	-16.,	2.},
		{580.,	951.,	-35.,	5.},
		{590.,	891.,	-35.,	21.},
		{600.,	905.,	-58.,	32.},
		{610.,	903.,	-72.,	41.},
		{620.,	884.,	-86.,	47.},
		{630.,	840.,	-95.,	51.},
		{640.,	851.,	-109.,	67.},
		{650.,	819.,	-107.,	73.},
		{660.,	826.,	-120.,	86.},
		{670.,	849.,	-140.,	98.},
		{680.,	813.,	-136.,	102.},
		{690.,	719.,	-120.,	83.},
		{700.,	743.,	-133.,	96.},
		{710.,	764.,	-129.,	85.},
		{720.,	633.,	-106.,	70.},
		{730.,	717.,	-116.,	76.},
		{740.,	770.,	-122.,	80.},
		{750.,	652.,	-102.,	67.},
		{760.,	477.,	-78.,	52.},
		{770.,	686.,	-112.,	74.},
		{780.,	650.,	-104.,	68.},
		{790.,	660.,	-106.,	70.},
		{800.,	610.,	-97.,	64.},
		{810.,	533.,	-83.,	55.},
		{820.,	589.,	-93.,	61.},
		{830.,	619.,	-98.,	65.}
	};

	if (lambda < 300. || lambda > 830.) {
		rt_bomb("spectral_dist_table: bad wavelength.");
	} else {
		/* Do linear interpolation to find approximate values. */
		j = ((int)lambda - 300)/10;
		ratio = (lambda - table[j][0]) / (table[j+1][0] - table[j][0]);
		*e_mean = ratio*(table[j+1][1] - table[j][1]) + table[j][1];
		*v1 = ratio*(table[j+1][2] - table[j][2]) + table[j][2];
		*v2 = ratio*(table[j+1][3] - table[j][3]) + table[j][3];
	}
}

/*
 *	S K Y _ L I G H T _ S P E C T R A L _ D I S T
 *
 *	Return sky light spectral distribution for a sky element.  Because
 *	to date there is no decisive research on the spectral distribution
 *	of sky light, the CIE synthesized daylight expression is used.
 *	See Judd, D B; MacAdam, D L; Wyszecki, G J; Journal for Optical
 *	Science of America, Vol. 54, 1964, p. 1031.
 *
 *	Units: W/m^2/nm/sr
 */
fastf_t
skylight_spectral_dist(fastf_t lambda, fastf_t *Zenith, fastf_t *Sky_elmt, fastf_t *Sun, int weather, fastf_t t_vl)
       	       		/* Wavelength of light (nm). */
      	       		/* Vector to sky zenith. */
      	         	/* Vector to sky element of interest. */
      	    		/* Vector to sun. */
   	        	/* Weather condition. */
       	     		/* Turbidity factor. */
{
	fastf_t	e_mean, v1, v2,
		lum,	/* Luminance at a given point in the sky (cd/m^2). */
		lz,	/* Luminance at the zenith.  Units: cd/m^2 */
		m1, m2,
		sd,	/* Spectral distribution at a given point in the */
			/* sky.  Units: W/m^2/nm/sr */
		sun_alt,/* Altitude of sun. */
		t_cp,	/* Correlated color temperature corresponding */
			/* to a given luminance.  Units: K */
		x, y;	/* 1931 CIE chromaticity coordinates. */

	sun_alt = 90 - acos(VDOT(Sun, Zenith))*180/PI;
	lz = zenith_luminance(sun_alt, t_vl);
	/* Get luminance distribution */
	switch (weather) {
	case CLEAR_SKY:
		lum = clear_sky_lum(lz, Sky_elmt, Sun, Zenith);
		break;
	case MEDIUM_SKY:
		lum = homogenous_sky_lum(Sky_elmt, Sun, t_vl);
		break;
	case OVERCAST_SKY: /* fallthrough */
	default:
		lum = overcast_sky_lum(lz, Zenith, Sky_elmt);
		break;
	}
/* XXX hack */
if (lum <= 0.) {/*bu_log("lum = %g\n", lum);*/ return(0.);}

	/* Convert to color temperature.  Expression based on careful */
	/* measurements by Toyota. */
	t_cp = 1.1985e8/pow(lum, 1.2) + 6500.;	/* Kelvin */

	/* Convert color temperature into spectral distribution */
	/* using CIE synthesized daylight expression. */

	/* Chromaticity coordinates, taken from Wyszecki, Guenter; Stiles, */
	/* WS; "Color Science: Concepts and Methods, Quantitative Data and */
	/* Formulae," John Wiley and Sons, 1982, pp. 145-6. */
	if (t_cp >= 4000. && t_cp < 7000.) {
		x =
			-4.6070e9/(t_cp*t_cp*t_cp)
			+ 2.9678e6/(t_cp*t_cp)
			+ 0.09911e3/t_cp
			+ 0.244063;
	} else if (t_cp >= 7000. && t_cp <= 25000.) {
		x =
			-2.0064e9/(t_cp*t_cp*t_cp)
			+ 1.9018e6/(t_cp*t_cp)
			+ 0.24748e3/t_cp
			+ 0.237040;
	} else {
		bu_log("skylight_spectral_dist: color temperature %lf out of range, lum=%lf.\n", t_cp, lum);
		t_cp = 25000;
		x =
			-2.0064e9/(t_cp*t_cp*t_cp)
			+ 1.9018e6/(t_cp*t_cp)
			+ 0.24748e3/t_cp
			+ 0.237040;
#if 0
		rt_bomb("temp");
#endif
	}
	y = 2.870*x - 3.000*x*x - 0.275;

	/* Scalar multiples os 1st two characteristic vectors needed to */
	/* reconstitue spectral distribution curves of typical daylight. */
	m1 = -1.3515 - 1.7703*x + 5.9114*y;
	m2 = 1./(0.0241 + 0.2562*x - 0.7341*y);
	m1 *= m2;
	m2 *= 0.0300 - 31.4424*x + 30.0717*y;

	/* Do table lookup for spectral distribution data. */
	spectral_dist_table(lambda, &e_mean, &v1, &v2);

	/* Get spectral distribution for sky element of interest. */
	sd = e_mean + m1*v1 + m2*v2;

	return(sd);
}

/*
 *	S U N _ R A D I A N C E
 *
 *	Calculate spectral radiance of sun on ground.
 *	Units: W/m^2/nm/sr.
 */
fastf_t
sun_radiance(fastf_t lambda, fastf_t alpha, fastf_t beta, fastf_t sun_alt, fastf_t sun_sang)
       	       		/* Wavelength of light (nm). */
	            	/* Coefficients of turbidity. */
	        	/* Altitude of sun above horizon. */
	         	/* Solid angle of sun as seen from ground. */
{
	fastf_t	cm,	/* Attenuation factor according to diffusion of */
			/* aerosol (unitless?). */
		coz,	/* Absorption coefficient due the absorption of */
			/* ozone (1/m). */
		cr,	/* Attenuation factor according to Rayleigh */
			/* diffusion of air molecules (unitless?). */
		e0,	/* Spectral irradiance out of atmosphere (W/m^2). */
		em,	/* Spectral irradiance on the ground surface */
			/* (W/m^2/nm). */
		lmicro,	/* lambda in um, (10^-6 m). */
		ls;	/* Spectral radiance of sun on ground (W/m^2/nm/sr). */

	lmicro = lambda/1000;
	e0 = atmos_irradiance(lambda);
	cr = 0.00864
	     * pow(lmicro,
		   -(3.916 + 0.074*lmicro + 0.050/lmicro));
	cm = beta * pow(lmicro, -alpha*lmicro);
	coz = ozone_absorption(lambda);
	em = e0 * exp(-(cr + cm + coz)*air_mass(sun_alt));
	ls = em/sun_sang;
#if 0
bu_log("e0 = %g\ncr = %g\ncm = %g\ncoz = %g\nem = %g\n", e0,cr,cm,coz,em);
bu_log("sun radiance = %g\n", ls);
#endif
	return(ls);
}

/*
 *	F R E S N E L _ R E F L
 *
 *	Return reflectance of a material with only regular (specular)
 *	reflectance using Fresnel's equations.  For materials other
 *	than glass, it is better to measure the specular reflectance
 *	since usually it will not follow Fresnel's formula.
 *
 *	Note: This works only for unpolarized light.  For polarized
 *	skylight, the formula must be extended.
 *
 *	SHOULD CONTACT TOYOTA TO UNDERSTAND WHAT THEY DID.
 */
fastf_t
fresnel_refl(fastf_t cos_eps, fastf_t n1, fastf_t n2)
       	        	/* Cosine of angle of incidence on the medium. */
	   		/* Index of refraction of material (usually air)
			 * in contact with the medium under test. */
	   		/* Index of refraction of the medium. */
{
	fastf_t	refl;		/* Returned reflectance. */
	fastf_t	p_parallel;	/* Reflectance for a plane-polarized beam
				 * of radiant energy so oriented that the
				 * plane of vibration of the electric vector
				 * is parallel to the plane of incidence
				 * defined by the central ray of the medium.
				 */
	fastf_t	p_perpendicular;/* Reflectance for a plane-polarized beam
				 * whose radiant energy plane of vibration
				 * is perpendicular to the plane of incidence.
				 */
	fastf_t	work, work2;	/* Intermediate results. */

	work2 = (n2/n1)*(n2/n1);
	work = sqrt(work2 - (1. - cos_eps*cos_eps));

	p_parallel = (cos_eps - work)/(cos_eps + work);
	p_parallel *= p_parallel;

	p_perpendicular = (work2*cos_eps - work)/(work2*cos_eps + work);
	p_perpendicular *= p_perpendicular;

	/* Only accurate for unpolarized radiant energy. */
	refl = 0.5 * (p_parallel + p_perpendicular);

	return(refl);
}

/*
 *	A B S O R P _ C O E F F
 *
 *	For each type of material which has had its absorption coefficient
 *	measured, linear interpolation between data points is used to
 *	return the absorption coefficient for a given wavelength.
 *
 *	Data files are 2 column ascii: wavelength (nm), absorption
 *	coefficient (1/m).
 */
fastf_t
absorp_coeff(fastf_t lambda, char *material)
       	       		/* wavelength (nm) */

{
	char	mfile[80];
	fastf_t	a, l,
		absorp, absorp_h, absorp_l, lambda_h, lambda_l;
	FILE	*fp;
	int	n;

	if (material[0] == '/') {
		/* Do nothing, user has his own absorption data. */
		strcpy(mfile, material);
	} else {
		/* Look for absorption data in usual place. */
		strcpy(mfile, "/m/cad/material/");
		strcat(mfile, material);
		strcat(mfile, "/absorption");
	}
	if ((fp = fopen(mfile, "r")) == NULL) {
		fprintf(stderr,
			"absorp_coeff: cannot open %s for reading.", mfile);
		rt_bomb("");
	}

	/* Find "nearby" values of lambda, absorption for interpolation. */
	if ((n = fscanf(fp, "%lf %lf", &l, &a)) != 2 || lambda + MIKE_TOL < l)
		return(-1.);
	lambda_l = l;
	absorp_l = a;

	/* Find low lambda. */
	while ((n = fscanf(fp, "%lf %lf", &l, &a)) == 2 && l < lambda) {
		lambda_l = l;
		absorp_l = a;
	}
	if (n != 2)
		return(-1.);
	else {
		lambda_h = l;
		absorp_h = a;
	}

	fclose(fp);

	absorp = (absorp_h - absorp_l)*(lambda - lambda_l)
		 /(lambda_h - lambda_l) + absorp_l;

	return(absorp);
}

/*
 *	R E F L E C T A N C E
 *
 *	For each type of material which has had its spectral reflectance
 *	measured, linear interpolation between data points is used to
 *	return the reflectance factor for a given wavelength and a given
 *	angle of light incidence.
 *
 *	Data files are 3 column ascii: wavelength, angle, reflectance.
 */
fastf_t
reflectance(fastf_t lambda, fastf_t alpha, fastf_t *refl, int lines)
       	       		/* Wavelength (nm). */
       	      		/* Angle of incident light, in degrees. */
       	      		/* Reflectance data. */
   	      		/* How many lines of data in refl[]. */
{
	fastf_t	alpha_hh, alpha_hl, alpha_lh, alpha_ll,
		beta_hh,  beta_hl,  beta_lh,  beta_ll,
		beta_l, beta_h,
		beta,
		lambda_h, lambda_l,
		l;
	int	i, j;

	/* Find low lambda. */
	for (l = 0, i = 0; l < lambda && i < lines; i++) {
		l = refl[i*3];
	}

	if (i == 0 || i >= lines)  {
		beta = -1;
		goto out;
	}

	j = --i;
	i--;
	lambda_l = refl[i*3];
	while (i >= 0 && refl[i*3] == lambda_l && refl[i*3+1] > alpha)
		i--;

	if (i < 0) {
		beta = -1;
		goto out;
	}

	if (refl[(i+1)*3] == lambda_l) {
		alpha_ll = refl[i*3+1];
		beta_ll  = refl[i*3+2];
		alpha_lh = refl[(i+1)*3+1];
		beta_lh  = refl[(i+1)*3+2];
	} else {
		alpha_ll = refl[(i-1)*3+1];
		beta_ll  = refl[(i-1)*3+2];
		alpha_lh = refl[i*3+1];
		beta_lh  = refl[i*3+2];
	}

	lambda_h = refl[j*3];	/* High lambda. */
	while (j < lines && refl[j*3] == lambda_h && refl[j*3+1] < alpha) {
		j++;
	}

	if (j >= lines) {
		beta = -1;
		goto out;
	}

	if (refl[(j-1)*3] == lambda_h) {
		alpha_hl = refl[(j-1)*3+1];
		beta_hl  = refl[(j-1)*3+2];
		alpha_hh = refl[j*3+1];
		beta_hh  = refl[j*3+2];
	} else {
		alpha_hl = refl[j*3+1];
		beta_hl  = refl[j*3+2];
		alpha_hh = refl[(j+1)*3+1];
		beta_hh  = refl[(j+1)*3+2];
	}

	/* Interpolate beta between alphas of lower lambda. */
	beta_l = (beta_lh - beta_ll)/(alpha_lh - alpha_ll)*(alpha - alpha_ll)
		 + beta_ll;
	/* Interpolate beta between alphas of higher lambda. */
	beta_h = (beta_hh - beta_hl)/(alpha_hh - alpha_hl)*(alpha - alpha_hl)
		 + beta_hl;
	/* Interpolate beta between low and high lambdas. */
	beta = (beta_h - beta_l)/(lambda_h - lambda_l)*(lambda - lambda_l)
	       + beta_l;

out:
#if 0
bu_log("reflectance(lambda=%g, alpha=%g)=%g\n", lambda, alpha, beta );
#endif
	return(beta);
}

/*
 *	L A M B D A _ T O _ R G B
 *
 *	Given a wavelength in nm of light, return its rgb
 *	approximation.
 *
 *	Taken from Wyszecki, Guenter; Stiles, WS; "Color Science:
 *	Concepts and Methods, Quantitative Data and Formulae," John
 *	Wiley and Sons, 1982, pp. 615, table taken from pp. 806-7.
 *
 *	They, in turn, took the data from Vos, J J, Colorimetric and
 *	photometric properties of a 2 degree fundamental observer,
 *	"Color Res. & Appl. 3", 125 (1978).
 *
 *	Table row: wavelength, x_bar(lambda), y_bar(lambda), z_bar(lambda).
 */
void
lambda_to_rgb(fastf_t lambda, fastf_t irrad, fastf_t *rgb)
       	       	/* Input, wavelength of light. */
       	      	/* Input, irradiance of light. */
       	     	/* Output, RGB approximation of input. */
{

/* Number of entries in color matching table. */
#define NCOLOR	90

	fastf_t	krx, kry, krz, kgx, kgy, kgz, kbz;
	fastf_t	ratio, r, g, b, x, y, z;
	static const fastf_t	table[NCOLOR][4] = {
		{380,	0.26899e-2,	0.20000e-3,	0.12260e-1},
		{385,	0.53105e-2,	0.39556e-3,	0.24222e-1},
		{390,	0.10781e-1,	0.80000e-3,	0.49250e-1},
		{395,	0.20792e-1,	0.15457e-2,	0.95135e-1},
		{400,	0.37981e-1,	0.28000e-2,	0.17409},
		{405,	0.63157e-1,	0.46562e-2,	0.29013},
		{410,	0.99941e-1,	0.74000e-2,	0.46053},
		{415,	0.15824,	0.11779e-1,	0.73166},
		{420,	0.22948,	0.17500e-1,	0.10658e1},
		{425,	0.28108,	0.22678e-1,	0.13146e1},
		{430,	0.31095,	0.27300e-1,	0.14672e1},
		{435,	0.33072,	0.32584e-1,	0.15796e1},
		{440,	0.33336,	0.37900e-1,	0.16166e1},
		{445,	0.31672,	0.42391e-1,	0.15682e1},
		{450,	0.28882,	0.46800e-1,	0.14717e1},
		{455,	0.25969,	0.52122e-1,	0.13740e1},
		{460,	0.23276,	0.60000e-1,	0.12917e1},
		{465,	0.20999,	0.72942e-1,	0.12356e1},
		{470,	0.17476,	0.90980e-1,	0.11138e1},
		{475,	0.13287,	0.11284,	0.94220},
		{480,	0.91944e-1,	0.13902,	0.75596},
		{485,	0.56985e-1,	0.16987,	0.58640},
		{490,	0.31731e-1,	0.20802,	0.44669},
		{495,	0.14613e-1,	0.25808,	0.34116},
		{500,	0.48491e-2,	0.32300,	0.26437},
		{505,	0.23215e-2,	0.40540,	0.20594},
		{510,	0.92899e-2,	0.50300,	0.15445},
		{515,	0.29278e-1,	0.60811,	0.10918},
		{520,	0.63791e-1,	0.71000,	0.76585e-1},
		{525,	0.11081,	0.79510,	0.56227e-1},
		{530,	0.16692,	0.86200,	0.41366e-1},
		{535,	0.22768,	0.91505,	0.29353e-1},
		{540,	0.29269,	0.95400,	0.20042e-1},
		{545,	0.36225,	0.98004,	0.13312e-1},
		{550,	0.43635,	0.99495,	0.87823e-2},
		{555,	0.51513,	0.10001e1,	0.58573e-2},
		{560,	0.59748,	0.99500,	0.40493e-2},
		{565,	0.68121,	0.97875,	0.29217e-2},
		{570,	0.76425,	0.95200,	0.22771e-2},
		{575,	0.84394,	0.91558,	0.19706e-2},
		{580,	0.91635,	0.87000,	0.18066e-2},
		{585,	0.97703,	0.81623,	0.15449e-2},
		{590,	0.10230e1,	0.75700,	0.12348e-2},
		{595,	0.10513e1,	0.69483,	0.11177e-2},
		{600,	0.10550e1,	0.63100,	0.90564e-3},
		{605,	0.10362e1,	0.56654,	0.69467e-3},
		{610,	0.99239,	0.50300,	0.42885e-3},
		{615,	0.92861,	0.44172,	0.31817e-3},
		{620,	0.84346,	0.38100,	0.25598e-3},
		{625,	0.73983,	0.32052,	0.15679e-3},
		{630,	0.63289,	0.26500,	0.97694e-4},
		{635,	0.53351,	0.21702,	0.68944e-4},
		{640,	0.44062,	0.17500,	0.51165e-4},
		{645,	0.35453,	0.13812,	0.36016e-4},
		{650,	0.27862,	0.10700,	0.24238e-4},
		{655,	0.21485,	0.81652e-1,	0.16915e-4},
		{660,	0.16161,	0.61000e-1,	0.11906e-4},
		{665,	0.11820,	0.44327e-1,	0.81489e-5},
		{670,	0.85753e-1,	0.32000e-1,	0.56006e-5},
		{675,	0.63077e-1,	0.23454e-1,	0.39544e-5},
		{680,	0.45834e-1,	0.17000e-1,	0.27912e-5},
		{685,	0.32057e-1,	0.11872e-1,	0.19176e-5},
		{690,	0.22187e-1,	0.82100e-2,	0.13135e-5},
		{695,	0.15612e-1,	0.57723e-2,	0.91519e-6},
		{700,	0.11098e-1,	0.41020e-2,	0.64767e-6},
		{705,	0.79233e-2,	0.29291e-2,	0.46352e-6},
		{710,	0.56531e-2,	0.20910e-2,	0.33304e-6},
		{715,	0.40039e-2,	0.14822e-2,	0.23823e-6},
		{720,	0.28253e-2,	0.10470e-2,	0.17026e-6},
		{725,	0.19947e-2,	0.74015e-3,	0.12207e-6},
		{730,	0.13994e-2,	0.52000e-3,	0.87107e-7},
		{735,	0.96980e-3,	0.36093e-3,	0.61455e-7},
		{740,	0.66847e-3,	0.24920e-3,	0.43162e-7},
		{745,	0.46141e-3,	0.17231e-3,	0.30379e-7},
		{750,	0.32073e-3,	0.12000e-3,	0.21554e-7},
		{755,	0.22573e-3,	0.84620e-4,	0.15493e-7},
		{760,	0.15973e-3,	0.60000e-4,	0.11204e-7},
		{765,	0.11275e-3,	0.42446e-4,	0.80873e-8},
		{770,	0.79513e-4,	0.30000e-4,	0.58340e-8},
		{775,	0.56087e-4,	0.21210e-4,	0.42110e-8},
		{780,	0.39541e-4,	0.14989e-4,	0.30383e-8},
		{785,	0.27852e-4,	0.10584e-4,	0.21907e-8},
		{790,	0.19597e-4,	0.74656e-5,	0.15778e-8},
		{795,	0.13770e-4,	0.52592e-5,	0.11348e-8},
		{800,	0.96700e-5,	0.37028e-5,	0.81565e-9},
		{805,	0.67918e-5,	0.26076e-5,	0.58626e-9},
		{810,	0.47706e-5,	0.18365e-5,	0.42138e-9},
		{815,	0.33550e-5,	0.12950e-5,	0.30319e-9},
		{820,	0.23534e-5,	0.91092e-6,	0.21753e-9},
		{825,	0.16377e-5,	0.63564e-6,	0.15476e-9}
	};
	int	hi, j, lo;

	/* Vos(1978) xyz to rgb conversion matrix values. */
	krx = 0.1551646;
	kry= 0.5430763;
	krz= -0.0370161;

	kgx = -0.1551646;
	kgy = 0.4569237;
	kgz = 0.0296946;

	kbz = 0.0073215;

	/* Interpolate values of x, y, z. */
	if (lambda < 380. || lambda > 825.) {
		bu_log("lambda_to_rgb: bad wavelength, %g nm.", lambda);
		bu_bomb("");
	}
	/* Find index of lower lambda in table. */
	lo = 0;
	hi = NCOLOR - 1;
	j = (NCOLOR - 1) >> 1;
	while (hi - lo > 1) {
		if (table[j][0] <= lambda)
			lo = j;
		else if (table[j][0] > lambda)
			hi = j;
		j = lo + ((hi - lo) >> 1);
	}
	/* Do linear interpolation to find approximate values. */
	ratio = (lambda - table[j][0]) / (table[j+1][0] - table[j][0]);
	x = ratio*(table[j+1][1] - table[j][1]) + table[j][1];
	y = ratio*(table[j+1][2] - table[j][2]) + table[j][2];
	z = ratio*(table[j+1][3] - table[j][3]) + table[j][3];


	/* Convert xyz to rgb. */
	r = krx*x + kry*y + krz*z;
	g = kgx*x + kgy*y + kgz*z;
	b =                 kbz*z;

	/* Convert relative rgb into displayable rgb. */
/* SOMEHOW NEED TO USE IRRAD, BUT DON'T KNOW ITS VALUES YET */
	rgb[0] += r/(r+g+b) * irrad/1e4;
	rgb[1] += g/(r+g+b) * irrad/1e4;
	rgb[2] += b/(r+g+b) * irrad/1e4;
bu_log("rgb = (%g %g %g), irrad = %g\n",r,g,b,irrad);
}

/*
 *	B A C K G R O U N D _ L I G H T
 *
 *	Calculate radiance of background light.  For a given wavelength,
 *	non-zero reflectance values for a range of incident light angles
 *	determine the solid angle of contributing background light.
 *
 *	Units: W/m^2/nm/sr
 *
 */
fastf_t
background_light(fastf_t lambda, struct toyota_specific *ts, fastf_t *Refl, fastf_t *Sun, fastf_t t_vl, struct shadework *swp)
       			       	/* Wavelength of light (nm). */

      			     	/* Regularly reflected ray. */
      			    	/* Vector pointing to sun. */
       			     	/* Atmospheric turbidity. */
                	     	/* Holds surface normal. */
{
	fastf_t	alpha0, alpha1, alpha_c,
		ang,
		bg_radiance,
		del_omega,
		irradiance,
		phi,
		r,
		refl,
		x, y;
	fastf_t i_dot_n  = .9999;
	vect_t	Ctr,
		Horiz,
		Sky_elmnt,
		Xaxis, Yaxis,
		work;

/* Angular spread between vectors used in solid angle integration. */
#define SPREAD		(10*PI/180)

	/* Differential solid angle. */
	del_omega = PI*sin(SPREAD/2)*sin(SPREAD/2);

	/* Find limits of solid angle. */
	alpha0 = 0.0;	/* degrees. */
	refl = reflectance(lambda, alpha0, ts->refl, ts->refl_lines);
	while (refl < MIKE_TOL) {
		if (refl == -1.)
			rt_bomb("toyota render: no reflectance data.");
		alpha0++;
		refl = reflectance(lambda, alpha0, ts->refl, ts->refl_lines);
	}
	alpha1 = alpha0;	/* degrees. */
	while (refl > MIKE_TOL && alpha1 < 90.) {
		alpha1++;
		refl = reflectance(lambda, alpha1, ts->refl, ts->refl_lines);
	}
	if (refl == -1.)
		alpha1--;
	alpha_c = (alpha0 + alpha1)/2;	/* degrees. */
	alpha_c *= PI/180;		/* radians. */

	/* Find horizontal component of reflected light. */
	if (!NEAR_ZERO(VDOT(swp->sw_hit.hit_normal, Refl)-1, MIKE_TOL)) {
		VCROSS(Yaxis, swp->sw_hit.hit_normal, Refl);
	} else {	/* R and N are the same vector. */
	 	vec_ortho(Yaxis, swp->sw_hit.hit_normal);
	}
	VCROSS(Horiz, Yaxis, swp->sw_hit.hit_normal);

	/* Get vector which cuts through center of solid angle. */
	VBLEND2(Ctr, cos(alpha_c), swp->sw_hit.hit_normal,
		sin(alpha_c), Horiz);
	VUNITIZE(Yaxis);
	VUNITIZE(Ctr);

	/* Angle between Ctr and edge of solid angle. */
	alpha_c = (alpha1 - alpha0)/2;	/* degrees. */
	alpha_c *= PI/180;		/* radians. */

	/* Set up coord axes with Ctr as Z axis. */
	VCROSS(Xaxis, Yaxis, Ctr);

	irradiance = 0.;
	/* Integrate over solid angle. */
/* JUST INTEGRATE OVER HEMISPHERE - THIS IS CURRENTLY WRONG */
	for (ang = SPREAD; ang < alpha_c; ang += SPREAD) {
		r = sin(ang);
		for (phi = 0.; phi < 2*PI; phi += SPREAD) {
			x = r*cos(phi);
			y = r*sin(phi);
			VJOIN2(Sky_elmnt, Ctr, x, Xaxis, y, Yaxis);
			VUNITIZE(Sky_elmnt);
			i_dot_n = VDOT(swp->sw_hit.hit_normal, Sky_elmnt);
if (i_dot_n >= 1.) i_dot_n = .9999;
#if 0
/*
 *			shoot_ray(Ctr);
 *			if (hit)
 *				bg_radiance = toyota_render();
 *			else
 *				bg_radiance = skylight_spectral_dist(
 *					lambda, ts->Zenith, Ctr,
 *					Sun, ts->weather, t_vl);
 *
 *			Might look something like:
 *
 *			VSET(ap.a_ray.r_pt, ?, ?, ?);
 *			VMOVE(ap.a_ray.r_dir, Sky_elmnt);
 *			ap.a_hit = toyota_render();
 *			ap.a_miss = skylight_spectral_dist(
 *					lambda, ts->Zenith, Sky_elmnt,
 *					Sun, ts->weather, t_vl);
 *			shoot_ray(Sky_elmnt);
 *			bg_radiance = return val from hit or miss
 */
#else
			if (rdebug&RDEBUG_RAYPLOT )  {
				VSCALE(work, Sky_elmnt, 200.);
				VADD2(work,swp->sw_hit.hit_point,work);
				pl_color( stdout, 0, 255, 0 );
				pdv_3line( stdout, swp->sw_hit.hit_point, work);
			}
			/* XXX hack:  just assume skylight */
			bg_radiance = skylight_spectral_dist(
				lambda, ts->Zenith, Sky_elmnt,
				Sun, ts->weather, t_vl);
#endif
/* XXX hack */		if (i_dot_n > 0.) {
			irradiance +=
				reflectance(lambda, acos(i_dot_n)*bn_radtodeg,
					ts->refl, ts->refl_lines)
				* bg_radiance
				* i_dot_n
				* del_omega;
#if 0
bu_log("bg radiance = %g\n", bg_radiance);
bu_log("I . N = %g\n", i_dot_n);
bu_log("del_omega = %g\n", del_omega);
bu_log("irradiance = %g\n", irradiance);
#endif
}
		}
	}
	/* Don't forget contribution from luminance at Ctr. */
#if 0
/*
 *		shoot_ray(Ctr);
 *		if (hit)
 *			bg_radiance = toyota_render();
 *		else
 *			bg_radiance = skylight_spectral_dist(
 *				lambda, ts->Zenith, Ctr,
 *				Sun, ts->weather, t_vl);
 */
#else
	bg_radiance = skylight_spectral_dist(lambda, ts->Zenith, Ctr,
		Sun, ts->weather, t_vl);
#endif
	if (rdebug&RDEBUG_RAYPLOT )  {
		VSCALE(work, Ctr, 200.);
		VADD2(work,swp->sw_hit.hit_point,work);
		pl_color( stdout, 255, 50, 0 );
		pdv_3line( stdout, swp->sw_hit.hit_point, work);
	}
	irradiance +=
		reflectance(lambda, acos(i_dot_n)*bn_radtodeg, ts->refl, ts->refl_lines)
		* bg_radiance
		* VDOT(Sky_elmnt, swp->sw_hit.hit_normal)
		* del_omega;
	irradiance /= PI;
#if 0
bu_log("irradiance = %g\n\n", irradiance);
#endif

	return(irradiance);
}

/*
 *	T O Y O T A _ R E N D E R
 *
 *	Lighting model developed by Toyota.  "Technique makes it
 *	possible to:
 *		(1) Generate an image as if it is an actual object.
 *		(2) Render any material and color.
 *		(3) Render the appearance at any place and time under
 *		    any weather conditions."
 */
HIDDEN int
toyota_render(register struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	fastf_t	direct_sunlight,
		dist,			/* Distance light travels (m). */
		f,			/* Fresnel factor. */
		i_dot_n,		/* Incident light DOT Normal. */
		i_refl,			/* Radiance of reflected light. */
		refl_radiance,		/* Radiance of reflected ray. */
		rx, ry,
#if 0
		solar_radiance,
#endif
		specular_light,
		sun_alt,		/* Altitude of sun over horizon. */
		sun_dot_n,		/* Sun, Normal dot product. */
		t_vl,			/* Atmospheric turbidity. */
		theta,
		transmitted_light,
		trans_radiance;		/* Radiance of transmitted ray. */
	int	i;
	register struct	light_specific *lp;
	struct toyota_specific	*ts =
		(struct toyota_specific *)dp;
	vect_t	Horiz,
		Reflected,
		Sun,			/* Vector to sun. */
		Transmitted,
		work;

	swp->sw_color[0] = swp->sw_color[1] = swp->sw_color[2] = 0;

	ts->lambda = 450;	/* XXX nm */

	i_refl = 0.;
	/* Consider effects of light source (>1 doesn't make sense really). */
	for (i=ap->a_rt_i->rti_nlights-1; i >= 0; i--)  {

/* WHY DOES THIS CAUSE THE BLACK CRESCENT ON THE SPHERE? */
		if ((lp = (struct light_specific *)swp->sw_visible[i])
			== LIGHT_NULL)
			continue;	/* shadowed */

		/* Light is not shadowed -- add this contribution */
/* UNNECESSARY	intensity = swp->sw_intensity+3*i; */
/* UNNECESSARY	VMOVE(Sun, swp->sw_tolight+3*i); */
		VMOVE(Sun, lp->lt_pos);
		VUNITIZE(Sun);
/* VPRINT("Sun", Sun); */
		/* Altitude of sun above horizon (degrees). */
		sun_alt = 90 - acos(VDOT(Sun, ts->Zenith))*180/PI;
#if 0
		/* Solar radiance. */
		solar_radiance = sun_radiance(ts->lambda, ts->alpha, ts->beta,
			sun_alt, ts->sun_sang);
#endif

		/* Create reflected ray. */
		i_dot_n = VDOT(swp->sw_hit.hit_normal, ap->a_ray.r_dir);
		if (i_dot_n > 1.) i_dot_n = .9999;
		VSCALE( work, swp->sw_hit.hit_normal, 2*i_dot_n );
		VSUB2( Reflected, work, ap->a_ray.r_dir );
		VUNITIZE(Reflected);
		VREVERSE(Reflected, Reflected);

		/* Cosine of angle between sun and surface normal. */
		sun_dot_n = VDOT(swp->sw_hit.hit_normal, Sun);

		f = fresnel_refl(i_dot_n, RI_AIR, ts->index_refrac);

		/* Direct sunlight contribution. */
		direct_sunlight =
			1./PI
			* reflectance(ts->lambda, acos(i_dot_n)*bn_radtodeg,
				ts->refl, ts->refl_lines)
			* sun_radiance(ts->lambda, ts->alpha, ts->beta,
				sun_alt, ts->sun_sang)
			* sun_dot_n
			* ts->sun_sang;
#if 0
bu_log("1/PI = %g\n", 1/PI);
bu_log("beta = %g\n", reflectance(ts->lambda, acos(i_dot_n)*bn_radtodeg,
ts->refl, ts->refl_lines));
bu_log("sun radiance = %g\n",  sun_radiance(ts->lambda, ts->alpha, ts->beta,
sun_alt, ts->sun_sang));
bu_log("direct_sunlight = %g\n", direct_sunlight);
bu_log("S . N = %g\n", sun_dot_n);
#endif

#if 0
/*
 *		shoot_ray(Reflected);
 *		if (hit)
 *			refl_radiance = toyota_render();
 *		else
 *			refl_radiance = skylight_spectral_dist(ts->lambda,
 *				ts->Zenith, Reflected, Sun, ts->weather, t_vl);
 */
		refl_ap = *ap;		/* struct copy */
		refl_ap.a_level = ap->a_level + 1;
		refl_ap.a_onehit = 1;
		VMOVE( refl_ap.a_ray.r_pt, swp->sw_hit.hit_point );
		VMOVE( refl_ap.a_ray.r_dir, Reflected );
		refl_ap.a_purpose = "Toyota reflected sun ray";
		/* a_hit should be colorview() */
#if 0
		refl_ap.a_hit = toyota_render;	/* XXX is this right */
#endif
		refl_ap.a_miss = toyota_miss;
		(void)rt_shootray( &refl_ap );
		/* a_return or a_user to hold hit/miss flag? */
#else
		/* XXX Hack:  it always misses */
		if (rdebug&RDEBUG_RAYPLOT )  {
			VSCALE(work, Reflected, 200.);
			VADD2(work,swp->sw_hit.hit_point,work);
			pl_color( stdout, 0, 150, 255 );
			pdv_3line( stdout, swp->sw_hit.hit_point, work);
		}
		t_vl = 0; /* XXX Was uninitialized variable */
		refl_radiance = skylight_spectral_dist(ts->lambda,
			ts->Zenith, Reflected, Sun, ts->weather, t_vl);

		/* Regularly reflected light contribution. */
		specular_light = f * refl_radiance;
#endif

		i_refl +=
			direct_sunlight
			+ specular_light
			+ background_light(ts->lambda, ts, Reflected, Sun,
				t_vl, swp);

		/* Regularly transmitted light contribution. */
		if (ts->glass) {

			/* Find horizontal component of reflected light. */
			ry = VDOT(swp->sw_hit.hit_normal, Reflected);
			theta = acos(ry);
			rx = sin(theta);
			VSCALE(Horiz, swp->sw_hit.hit_normal, ry);
			VSUB2(Horiz, Reflected, Horiz);
			VSCALE(Horiz, Horiz, 1/rx);
			VUNITIZE(Horiz);

			/* Snell's Law of Refraction (with air). */
			theta = asin(sin(acos(i_dot_n))/ts->index_refrac);
			/* Generate transmitted ray. */
			VBLEND2(Transmitted, sin(theta), Horiz,
				-cos(theta), swp->sw_hit.hit_normal);
			VUNITIZE(Transmitted);
#if 0
/*
 *			shoot_ray(Transmitted);
 *			if (hit)
 *				trans_radiance = toyota_render();
 *			else
 *				trans_radiance = skylight_spectral_dist(
 *					ts->lambda, ts->Zenith, Transmitted,
 *					Sun, ts->weather, t_vl);
 */
#else
			trans_radiance = skylight_spectral_dist(
				ts->lambda, ts->Zenith, Transmitted,
				Sun, ts->weather, t_vl);
#endif
			dist = 1;	/* XXX what distance to use? */
			transmitted_light =
				(1 - f)
				* trans_radiance
				* exp(-absorp_coeff(ts->lambda, ts->material)
/* NEED THIS STILL! */		      * dist);
			i_refl += transmitted_light;
		}

		i_refl *= lp->lt_fraction;
#if 0
bu_log("i_refl = %g\n", i_refl);
#endif

/* WHERE DOES THIS GO??  NOT HERE.  SO WHERE DOES i_refl GO THEN. */
		/* Convert wavelength and radiance into RGB triple. */
		lambda_to_rgb(ts->lambda, i_refl, swp->sw_color);
#if 0
bu_log("rgb = (%g  %g  %g)\n",swp->sw_color[0], swp->sw_color[1], swp->sw_color[2]);
#endif
	}

	/* Turn off colorview()'s handling of reflect/refract */
	swp->sw_reflect = 0;
	swp->sw_transmit = 0;
	return(1);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
