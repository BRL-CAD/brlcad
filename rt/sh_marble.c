/*
 *			M A R B L E . C
 *
 *	A marble texture shader.  Note that this version has been
 *	altered from the original BRL-CAD release, and may produce
 *	results differently than may be expected.
 *
 *  Author -
 *	Tom DiGiacinto
 *
 *  Modified by -
 *	Bill Laut, Gull Island Consultants, Inc.
 *
 *  Modification Notes -
 *	This version of Tom's shader has been slightly enhanced to improve
 *	its utility.  The most notable difference is that the noise table is
 *	now approximately double the size on all three axis.  This is to allow
 *	each marble region's starting point to be "dithered" within the table,
 *	so that no two pieces of marble look exactly alike.
 *
 *	Another enhancement (primarily for "sculptured" effects) is the "id"
 *	qualifier.  This binds all of bounding RPPs for a given combination(s)
 *	during setup, to compute the overall bounding RPP for distributing the
 *	noise table.
 *
 *	Finally, "scale" and "range" are used to control the amplitude of
 *	the noise, as well as to extract a subset range which is then normalized.
 *
 *	N.B.:  The "Marble_Chain" list is purged by marble_free().
 *
 *	N.B.2: This version of sh_marble uses the extended noise table built
 *	       in module "turb.c" to insure that turbulence textures are
 *	       consistent from frame to frame, regardless of RT hacking.
 *
 *
 *  MATPARM Qualifiers -
 *	id=n			Bundles regions together to blend turbulence
 *				over region boundaries.  Negative id number
 *				causes the region's instance number to be used,
 *				which is helpful for instanced copies of a
 *				given combination.
 *
 *	m{atte}={0,1,2}		Specifying a matte operation.  The lt_rgb or
 *				dk_rgb will be taken from swp->sw_color.
 *
 *	ns=n			Number of noise samples to sum per pixel.
 *				Increasing this tends to "smooth out" the texture.
 *
 *	j{itter}=n		Adds "graininess" to the marble texture.
 *
 *	s{cale}=n		Exponent applied to returned noise value before
 *				range limiting is done. 
 *
 *	t{ensor}=n		Controls how the noise value is interpreted:
 *					0 - linear
 *					1 - new algorithm
 *					2 - Tom's original algorithm
 *
 *	a{ngle}=n		"Angle" of noise.  Useful for slightly shifting
 *				the texture's color.
 *
 *	r{ange}=lo/hi		Specifies a low- and high-limit range of
 *				acceptable noise values, the result of which is
 *				normalized.  Noise values outside of range are
 *				clamped at zero or one.
 *
 *	e{xponent}=n		Exponent applied to individual noise frequencies
 *				before they are summed together.
 *
 *	c{ompression}=n		Coefficient applied to normalized value to do
 *				post-normalized compression/expansion.
 *
 *	d{ither}=x/y/z		Specifies starting point of overall RPP in the
 *				noise table; range of [0..1].
 *
 *	lt{_rgb}=a/b/c		Color for light portions of texture
 *
 *	dk{_rgb}=a/b/c		Color for dark portions of texture
 *
 *  Status:  experimental
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"
#include "./light.h"

/* from turb.c */
extern CONST fastf_t	turb_table[20][20][20];

#define IPOINTS 10		/* Undithered number of points */
#define TPOINTS 20		/* Total number of points */

/*
 *	User parameters block for a given marble region
 */

struct	marble_specific  {
	struct marble_specific	*forw;
	struct region		*rp;
	int			flags;
	int			ident;
	int			tensor;
	int			ns;
	int			matte;
	fastf_t			e;
	fastf_t			scale;
	fastf_t			range[2];
	fastf_t			clamp[2];
	fastf_t			angle;
	fastf_t			compression;
	fastf_t			jitter;
	fastf_t			bias;
	vect_t			mar_min;
	vect_t			mar_max;
	vect_t			dither;
	vect_t			lt_rgb;
	vect_t			dk_rgb;
};

/*
 *	Flag bits which may appear in the "flags" integer
 */

#define	MP_IDENT	0x00000001
#define MP_MATTE	0x00000002
#define MP_STACK	0x00000004
#define MP_RANGE	0x00000008
#define MP_T2		0x00000010
#define	MP_CLAMP	0x00000020
#define MP_DITHER	0x00000040

/*
 *	Offset declarations
 */

#define MARB_NULL	((struct marble_specific *)0)
#define MARB_O(m)	offsetof(struct marble_specific, m)
#define MARB_OA(m)	offsetofarray(struct marble_specific, m)

/*
 *	Listheads and sundry statics
 */

static struct marble_specific	*Marble_Chain;
static int			Marble_Prepped = 0;

/*
 *      Internal procedural prototypes
 */

HIDDEN int	marble_setup(), marble_render(), marble_prep();
extern int	mlib_zero(), mlib_one();
extern void	mlib_void();

HIDDEN void	marble_check_flags RT_ARGS((CONST struct bu_structparse *sdp,
			CONST char *name, CONST char *base, char *value));

HIDDEN void	marble_free RT_ARGS(( char * ));

/*
 *	Marble-specific user data
 */

struct	bu_structparse marble_parse[] = {
	{"%d",	1,	"id",		MARB_O(ident),		marble_check_flags },
	{"%d",	1,	"ns",		MARB_O(ns),		FUNC_NULL },
	{"%d",	1,	"matte",	MARB_O(matte),		marble_check_flags },
	{"%d",	1,	"m",		MARB_O(matte),		marble_check_flags },
	{"%f",	1,	"exponent",	MARB_O(e),		FUNC_NULL },
	{"%f",	1,	"e",		MARB_O(e),		FUNC_NULL },
	{"%f",	1,	"scale",	MARB_O(scale),		FUNC_NULL },
	{"%f",	1,	"s",		MARB_O(scale),		FUNC_NULL },
	{"%f",	2,	"range",	MARB_OA(range),		marble_check_flags },
	{"%f",	2,	"r",		MARB_OA(range),		marble_check_flags },
	{"%f",	2,	"clamp",	MARB_OA(clamp),		marble_check_flags },
	{"%f",	2,	"cl",		MARB_OA(clamp),		marble_check_flags },
	{"%f",	3,	"dither",	MARB_OA(dither),	marble_check_flags },
	{"%f",	3,	"d",		MARB_OA(dither),	marble_check_flags },
	{"%f",	1,	"jitter",	MARB_O(jitter),		FUNC_NULL },
	{"%f",	1,	"j",		MARB_O(jitter),		FUNC_NULL },
	{"%d",	1,	"tensor",	MARB_O(tensor),		marble_check_flags },
	{"%d",	1,	"t",		MARB_O(tensor),		marble_check_flags },
	{"%f",	1,	"angle",	MARB_O(angle),		FUNC_NULL },
	{"%f",	1,	"a",		MARB_O(angle),		FUNC_NULL },
	{"%f",	1,	"compression",	MARB_O(compression),	FUNC_NULL },
	{"%f",	1,	"c",		MARB_O(compression),	FUNC_NULL },
	{"%f",	3,	"lt_rgb",	MARB_OA(lt_rgb),	FUNC_NULL },
	{"%f",	3,	"lt",		MARB_OA(lt_rgb),	FUNC_NULL },
	{"%f",	3,	"dk_rgb",	MARB_OA(dk_rgb),	FUNC_NULL },
	{"%f",	3,	"dk",		MARB_OA(dk_rgb),	FUNC_NULL },
	{"",	0,	(char *)0,	0,			FUNC_NULL }
};

/*
 *	mfuncs parsing structure
 */

#ifdef eRT
struct	mfuncs marble_mfuncs[] = {
	{"marble",	0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,	0,
	marble_prep,	marble_setup,	marble_render,	mlib_void,	marble_free },

	{"m",		0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,	0,
	marble_prep,    marble_setup,   marble_render,  mlib_void,      marble_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0,		0 }
};
#else
struct mfuncs marble_mfuncs[] = {
	{"marble",	0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,	0,
	marble_setup,	marble_render,	mlib_void,	marble_free},

	{"m",		0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,	0,
	marble_setup,   marble_render,  mlib_void,      marble_free},

	{(char *)0,	0,		0,		0,	0,
	0,		0,		0,		0}
};
#endif

/*
 *			M A R B L E _ P R E P
 *
 *	This routine was originally used to initialize the noise table,
 *	until we moved to using the static turb_table.  This hook has
 *	been left in to accomodate future expansion of mf_init().
 */
HIDDEN int marble_prep ()
{
	/*
	 *	Initialize the marble region chain
	 */

	Marble_Chain = MARB_NULL;

	/*
	 *	return to caller
	 */

	return (1);
}

/*
 *		S e t u p   H o o k   R o u t i n e s
 *
 *	The following are routines which are called by bu_struct_parse()
 *	while parsing the MATPARM field.  These are currently limited to
 *	setting flag bits, indicating the presence of certain options.
 */

HIDDEN void marble_check_flags (sdp, name, base, value)
CONST struct bu_structparse *sdp;
CONST char *name;
CONST char *base;
char *value;
{
	register struct marble_specific *mp =
		(struct marble_specific *)base;

	if (!strcmp (name, "id")) {
		mp->flags |= MP_IDENT;
		}

	if (!strcmp(name, "m") || !strcmp(name, "matte")) {
		mp->flags |= MP_MATTE;
		}

	if (!strcmp(name, "r") || !strcmp(name, "range")) {
		mp->flags |= MP_RANGE;
		}

	if (!strcmp(name, "cl") || !strcmp(name, "clamp")) {
		mp->flags |= MP_CLAMP;
		}

	if (!strcmp(name, "d") || !strcmp(name, "dither")) {
		mp->flags |= MP_DITHER;
		}

	if (!strcmp(name, "t") || !strcmp(name, "tensor")) {
		mp->flags |= MP_T2;
		}
}

/*
 *			M A R B L E _ S E T U P
 *
 * Initialize the table of noise points
 */
HIDDEN int
marble_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
struct mfuncs           *mfp;
struct rt_i             *rtip;  /* New since 4.4 release */
{
	register struct marble_specific *mp, *mc;
	vect_t c_min, c_max;
	extern struct resource		rt_uniresource;
	register struct resource	*resp = &rt_uniresource;
	int i;

#ifndef eRT
	/*
	 *	If this is the release-issue RT, then call the
	 *	prep routine.
	 */

	if (!(Marble_Prepped)) 
		Marble_Prepped = marble_prep();
#endif

	/*
	 *	Check the parameters, and allocate space for impure
	 */

	RT_VLS_CHECK( matparm );
	GETSTRUCT( mp, marble_specific );
	*dpp = (char *)mp;

	/*
	 *	Set the default values for this region
	 */

	mp->forw         = MARB_NULL;
	mp->rp		 = rp;
	mp->flags	 = 0;
	mp->ident        = 0;
	mp->matte	 = 0;
	mp->ns		 = 10;
	mp->tensor	 = 1;
	mp->e		 = 1.0;
	mp->scale        = 1.0;
	mp->angle	 = 180.0;
	mp->compression	 = 0.0;
	mp->range[0]     = 0.0;
	mp->range[1]     = 1.0;
	mp->clamp[0]	 = -1.0;
	mp->clamp[1]     = 1.0;
	mp->jitter	 = 0.0;
	mp->bias	 = 0.0;

	VSETALL (mp->lt_rgb, 255);
	VSETALL (mp->dk_rgb, 0);

	/*
	 *	See if this region has been processed before, and if
	 *	so flag this iteration as stacked (and use the previous
	 *	iteration's dither values).
	 */

	for (mc = Marble_Chain; mc != MARB_NULL; mc = mc->forw) {
		if (mc->rp == rp) {
			mc->flags |= MP_STACK;
			mp->flags |= MP_STACK;

			VMOVE (mp->dither, mc->dither);
			mp->flags |= MP_DITHER;

			if (mc->flags & MP_IDENT) {
				mp->flags |= MP_IDENT;
				mp->ident  = mc->ident;
				}
			break;
			}
		}

	/*
	 *	Parse the parameter block
	 */

	if (bu_struct_parse(matparm, marble_parse, (char *)mp) < 0)
		return(-1);

	/*
	 *	If the user didn't specify a dither, add one in.
	 *	If specificed, then range-check it
	 */

	if (!(mp->flags & MP_DITHER)) {
		mp->dither[X] = rand0to1 (resp->re_randptr);
		mp->dither[Y] = rand0to1 (resp->re_randptr);
		mp->dither[Z] = rand0to1 (resp->re_randptr);
		}

	   else {
		for (i=0; i<3; i++) {
			if (mp->dither[i] < 0.0 || mp->dither[i] > 1.0) {
				rt_log ("marble_setup(%s):  dither is out of range.\n",
					rp->reg_name);
				return (-1);
				}
			}
		}

	/*
	 *	Add the block to the marble chain
	 */

	mp->forw     = Marble_Chain;
	Marble_Chain = mp;

	/*
	 *	Do sundry tasks
	 */

	mp->angle       *= rt_degtorad;
	mp->compression *= rt_pi;

	for (i=0; i<3; i++) {
		mp->lt_rgb[i] *= rt_inv255;
		mp->dk_rgb[i] *= rt_inv255;
		}

	if (mp->ns < 1) {
		rt_log ("marble_setup(%s):  number-of-samples must be greater than zero.\n",
				rp->reg_name);
		return (-1);
		}

	/*
	 *	Compensate for a negative low range
	 */

	if (mp->range[0] < 0.0) {
		mp->bias      = fabs (mp->range[0]);
		mp->range[0]  = 0.0;
		mp->range[1] += mp->bias;
		}

	/*
	 *	See if we're already inited the marble texture and return
	 *	if so.  Else, begin the process by getting the bounding size
	 *	of the model.
	 */

	if( rt_bound_tree(rp->reg_treetop,mp->mar_min,mp->mar_max) < 0 )
		return(-1);	/* FAIL */

	/*
	 *	Exit if this is not part of a multi-region combination
	 */

	if (!(mp->flags & MP_IDENT)) return (1);

	/*
	 *	If the ident is negative, use the region's instance #
	 */

	if (mp->ident < 0) mp->ident = (int)rp->reg_instnum;

	/*
	 *	Add the setup to the marble chain.  Note that this should
	 *	properly be resource-synced to work fully with parallelized
	 *	tree walkers and preppers.
	 */

	VMOVE (c_min, mp->mar_min);
	VMOVE (c_max, mp->mar_max);

	for (mc = Marble_Chain; mc != MARB_NULL; mc = mc->forw) {
		if ((mc->flags & MP_IDENT) && mc->ident == mp->ident) {
			VMIN (c_min, mc->mar_min);
			VMAX (c_max, mc->mar_max);
			}
		}

	/*
	 *	Update the matching regions' RPP
	 */

	for (mc = Marble_Chain; mc != MARB_NULL; mc = mc->forw) {
		if ((mc->ident & MP_IDENT) && mc->ident == mp->ident) {
			VMOVE (mc->mar_min, c_min);
			VMOVE (mc->mar_max, c_max);
			VMOVE (mc->dither, mp->dither);
			}
		}

	/*
	 * Duplicate the three initial faces
	 *  beyond the last three faces so we can interpolate
	 *  back to zero.  NOT DONE YET.
	 */

	return(1);
}

/*
 *		M A R B L E _ F R E E
 *
 *	This routine is called to free up the user data blocks at
 *	the end of a frame, as well as clean up any references to
 *	objects on the Marble_Chain list.
 */

HIDDEN void marble_free (cp)
char *cp;
{
	register struct marble_specific *mp =
		(struct marble_specific *)cp;

	register struct marble_specific *mc;

/*	rt_log ("marble_free(%s):  Invoked.\n", mp->rp->reg_name); */

	if (Marble_Chain == mp) {
/*		rt_log ("Releasing marble (at head) for region %s.\n", mp->rp->reg_name); */
		Marble_Chain = mp->forw;
		rt_free ((char *)mp, "marble_specific");
		return;
		}

	for (mc = Marble_Chain; mc != MARB_NULL; mc = mc->forw) {
		if (mc->forw == mp) {
/*			rt_log ("Releasing marble for region %s.\n", mp->rp->reg_name); */
			mc->forw = mp->forw;
			rt_free ((char *)mp, "marble_specific");
			return;
			}
		}

}

/*
 *		M a r b l e _ N o i s e
 *
 * Return a linearly interpolated noise point for
 *  0 <= x < 1 and 0 <= y < 1 and 0 <= z < 1.
 *  Noise returned is also between 0 and 1.
 */
HIDDEN double marble_noise(x, y, z, mp)
double x, y, z;
struct marble_specific *mp;
{
	int	xi, yi, zi;		/* Integer portions of x and y */
	double	xr, yr, zr;		/* Remainders */
	double	n1, n2, noise1, noise2, noise3, noise;	/* temps */

	xi = (x * IPOINTS) + (mp->dither[X] * IPOINTS);
	xr = ((x * IPOINTS) + (mp->dither[X] * IPOINTS)) - xi;

	yi = (y * IPOINTS) + (mp->dither[Y] * IPOINTS);
	yr = ((y * IPOINTS) + (mp->dither[Y] * IPOINTS)) - yi;

	zi = (z * IPOINTS) + (mp->dither[Z] * IPOINTS);
	zr = ((z * IPOINTS) + (mp->dither[Z] * IPOINTS)) - zi;

	n1     = (1 - xr) * pow (turb_table[xi][yi][zi], mp->e) +
		       xr * pow (turb_table[xi + 1][yi][zi], mp->e);
	n2     = (1 - xr) * pow (turb_table[xi][yi + 1][zi], mp->e) +
		       xr * pow (turb_table[xi + 1][yi + 1][zi], mp->e);
	noise1 = (1 - yr) * n1 + yr * n2;

	n1     = (1 - xr) * pow (turb_table[xi][yi][zi + 1], mp->e) +
		       xr * pow (turb_table[xi + 1][yi][zi + 1], mp->e);
	n2     = (1 - xr) * pow (turb_table[xi][yi + 1][zi + 1], mp->e) +
		       xr * pow (turb_table[xi + 1][yi + 1][zi + 1], mp->e);
	noise2 = (1 - yr) * n1 + yr * n2;

	noise3 = (1 - zr) * noise1 + zr * noise2;
	noise  = pow (noise3, mp->scale);

/*rt_log("noise3(%g,%g,%g) = %g\n",x,y,z,noise);*/
	return( noise );
}

/*
 *		T u r b u l e n c e   R o u t i n e
 *
 *	This routine is called to sum a collection of noise
 *	frequencies, and does so by going through the noise array
 *	at regular, though dithered, intervals.
 */

HIDDEN double marble_turb(x, y, z, mp)
double x, y, z;
struct marble_specific *mp;
{
	extern struct resource		rt_uniresource;
	register struct resource	*resp = &rt_uniresource;

	int	i;
	double	a, b, c, turb = 0.0, scale = 1.0;

	for (i=0; i<mp->ns; i++) {
		scale = (double)(mp->ns - i) / (double)mp->ns;

		a = (x * scale) + 
		    (rand_half (resp->re_randptr) * mp->jitter);

		b = (y * scale) +
		    (rand_half (resp->re_randptr) * mp->jitter);

		c = (z * scale) +
		    (rand_half (resp->re_randptr) * mp->jitter);

		turb  += (marble_noise (a, b, c, mp) * scale);
		}


/* rt_log("turb(%g,%g,%g) = %g (%g, %g)\n",x,y,z,turb,bottom,ms); */
	return( turb );
}

/*
 *			M A R B L E _ R E N D E R
 */
HIDDEN
marble_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
register struct shadework	*swp;
char	*dp;
{
	register struct marble_specific *mp =
		(struct marble_specific *)dp;
	point_t	mat_lt, mat_dk;
	double	value;
	fastf_t	x,y,z;
	fastf_t xd,yd,zd;
	int	i;

	/*
	 *	Prep the light and dark colors
	 */

	switch (mp->matte) {

		case 0:
		default:
			VMOVE (mat_lt, mp->lt_rgb);
			VMOVE (mat_dk, mp->dk_rgb);
			break;

		case 1:
			VMOVE (mat_lt, swp->sw_color);
			VMOVE (mat_dk, mp->dk_rgb);
			break;

		case 2:
			VMOVE (mat_lt, mp->lt_rgb);
			VMOVE (mat_dk, swp->sw_color);
			break;

		}

	/*
	 *	Normalize the hit point to [0..1]
	 */

	xd = mp->mar_max[0] - mp->mar_min[0] + 1.0;
	yd = mp->mar_max[1] - mp->mar_min[1] + 1.0;
	zd = mp->mar_max[2] - mp->mar_min[2] + 1.0;

	x = (swp->sw_hit.hit_point[0] - mp->mar_min[0]) / xd;
	y = (swp->sw_hit.hit_point[1] - mp->mar_min[1]) / yd;
	z = (swp->sw_hit.hit_point[2] - mp->mar_min[2]) / zd;

	/*
	 *	Obtain the raw value
	 */

	value = marble_turb (x,y,z,mp) + mp->bias;

	/*
	 *	Apply the clamp
	 */

	if (mp->flags & MP_CLAMP) {
		if (value < mp->clamp[0]) value = mp->clamp[0];
		if (value > mp->clamp[1]) value = mp->clamp[1];
		}

	/*
	 *	Normalize the raw value
	 */

	if (mp->flags & MP_RANGE) {
		if (value < mp->range[0]) value  = 0.0;
				    else  value -= mp->range[0];

		if (value > mp->range[1]) value  = 1.0;
				    else  value /= mp->range[1];
		}

	/*
	 *	Remove the bias
	 */

	if (mp->bias != 0.0) value -= mp->bias;

	/*
	 *	If we are emulating Tom's original algorithm, do his
	 *	code here and return the value.
	 */

	if (mp->tensor == 2) {
		value = sin (value + x);
		if (value <= 0.25)
			value *= 4.0;
		else
			if (value > 0.25 && value <= 0.5)
				value = -((value-0.5) * 4.0);
			else
				if (value > 0.5 && value <= 0.75)
					value = (value-0.5) * 4.0;
				else
					value = -((value-1.0) * 4.0);

		VCOMB2 (swp->sw_color,
				value,         mat_lt,
				(1.0 - value), mat_dk);
		return (1);
		}

	/*
	 *	Apply any scaling effects
	 */

	if (mp->angle != 0.0) value *= mp->angle;
	if (mp->compression != 0.0) value *= mp->compression;

	/*
	 *	Convert the normalized value into a curve
	 */

	if (mp->tensor) value = sin (value);

	/*
	 *	Compute the color
	 */

	VCOMB2 (swp->sw_color,
			value,         mat_dk,
			(1.0 - value), mat_lt);

	/*
	 *	Apply final range checking
	 */

	for (i=0; i<3; i++) {
		if (swp->sw_color[i] < 0.0) swp->sw_color[i] = 0.0;
		if (swp->sw_color[i] > 1.0) swp->sw_color[i] = 1.0;
		}

	/*
	 *	exit
	 */

	return (1);
}
