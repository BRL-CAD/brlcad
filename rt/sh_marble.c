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
 *	m{atte}={0,1}		Specifying a one indicates a matte operation.  The
 *				shader will take its "lt_rgb[]" values from sw_color.
 *				This is used in conjunction with sh_stack to overlay
 *				multiple marble textures.
 *
 *	ns=n			Number of noise samples to sum per pixel.
 *				Increasing this tends to "smooth out" the texture.
 *
 *	j{itter}=n		Adds "graininess" to the marble texture.
 *
 *	s{cale}=n		Exponent applied to returned noise value before
 *				range limiting is done. 
 *
 *	t{ensor}=n		Controls how the raw value is interpreted:
 *					0 - linear, 1 - sin, 2 - cosin
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
 *	dither=x/y/z		Specifies starting point of overall RPP in the
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
	double			e;
	double			scale;
	double			range[2];
	double			angle;
	double			compression;
	double			jitter;
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
HIDDEN void	marble_free( char * );
extern int	mlib_zero(), mlib_one();
extern void	mlib_void();

HIDDEN void	marble_ident_set (CONST struct structparse *, CONST char *, CONST char *, char *);
HIDDEN void	marble_matte_set (CONST struct structparse *, CONST char *, CONST char *, char *);

/*
 *	Marble-specific user data
 */

struct	structparse marble_parse[] = {
	{"%d",	1,	"id",		MARB_O(ident),		marble_ident_set },
	{"%d",	1,	"ns",		MARB_O(ns),		FUNC_NULL },
	{"%d",	1,	"matte",	MARB_O(matte),		marble_matte_set },
	{"%d",	1,	"m",		MARB_O(matte),		marble_matte_set },
	{"%f",	1,	"exponent",	MARB_O(e),		FUNC_NULL },
	{"%f",	1,	"e",		MARB_O(e),		FUNC_NULL },
	{"%f",	1,	"scale",	MARB_O(scale),		FUNC_NULL },
	{"%f",	1,	"s",		MARB_O(scale),		FUNC_NULL },
	{"%f",	2,	"range",	MARB_OA(range),		FUNC_NULL },
	{"%f",	2,	"r",		MARB_OA(range),		FUNC_NULL },
	{"%f",	3,	"dither",	MARB_OA(dither),	FUNC_NULL },
	{"%f",	3,	"d",		MARB_OA(dither),	FUNC_NULL },
	{"%f",	1,	"jitter",	MARB_O(jitter),		FUNC_NULL },
	{"%f",	1,	"j",		MARB_O(jitter),		FUNC_NULL },
	{"%d",	1,	"tensor",	MARB_O(tensor),		FUNC_NULL },
	{"%d",	1,	"t",		MARB_O(tensor),		FUNC_NULL },
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
	{"marble",	0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,
	marble_prep,	marble_setup,	marble_render,	mlib_void,	marble_free },

	{"m",		0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,
	marble_prep,    marble_setup,   marble_render,  mlib_void,      marble_free },

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0,		0 }
};
#else
struct mfuncs marble_mfuncs[] = {
	{"marble",	0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,
	marble_setup,	marble_render,	mlib_void,	marble_free},

	{"m",		0,		0,		MFI_HIT|MFI_NORMAL|MFI_LIGHT,
	marble_setup,   marble_render,  mlib_void,      marble_free},

	{(char *)0,	0,		0,		0,
	0,		0,		0,		0}
};
#endif

/*
 *			M A R B L E _ P R E P
 *
 *	Initialize the static noise arrays.
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
 *	The following are routines which are called by rt_structparse()
 *	while parsing the MATPARM field.  These are currently limited to
 *	setting flag bits, indicating the presence of certain options.
 */

HIDDEN void marble_ident_set (sdp, name, base, value)
CONST struct structparse *sdp;
CONST char *name;
CONST char *base;
char *value;
{
	register struct marble_specific *mp =
		(struct marble_specific *)base;

	mp->flags |= MP_IDENT;
}

HIDDEN void marble_matte_set (sdp, name, base, value)
CONST struct structparse *sdp;
CONST char *name;
CONST char *base;
char *value;
{
	register struct marble_specific *mp =
		(struct marble_specific *)base;

	mp->flags |= MP_MATTE;
}

/*
 *			M A R B L E _ S E T U P
 *
 * Initialize the table of noise points
 */
HIDDEN int
marble_setup( rp, matparm, dpp )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
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
	mp->jitter	 = 0.0;

	VSETALL (mp->lt_rgb, 255);
	VSETALL (mp->dk_rgb, 0);

	/*
	 *	See if this region has been processed before, and if
	 *	so flag this iteration as stacked (and use the previous
	 *	iteration's dither values).
	 */

	for (mc = Marble_Chain; mc != MARB_NULL; mc = mc->forw) {
		if (mc->rp == rp) {
			mp->flags |= MP_STACK;
			VMOVE (mp->dither, mc->dither);
			break;
			}
		}

	/*
	 *	If this is the first iteration of a user block for
	 *	this region, go ahead and init the dither field
	 */

	if (!(mp->flags & MP_STACK)) {
		mp->dither[X] = rand0to1 (resp->re_randptr);
		mp->dither[Y] = rand0to1 (resp->re_randptr);
		mp->dither[Z] = rand0to1 (resp->re_randptr);
		}

	/*
	 *	Parse the parameter block
	 */

	if (rt_structparse (matparm, marble_parse, (char *)mp) < 0)
		return(-1);

	/*
	 *	Add the block to the marble chain
	 */

	mp->forw     = Marble_Chain;
	Marble_Chain = mp;

	/*
	 *	Do sundry limit range checking
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

	for (i=0; i<3; i++) {
		if (mp->dither[i] < 0) {
			rt_log ("marble_setup(%s):  dither is negative.\n",
				rp->reg_name);
			return (-1);
			}
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

	xi = x * IPOINTS;
	xr = (x * IPOINTS) - xi;
	yi = y * IPOINTS;
	yr = (y * IPOINTS) - yi;
	zi = z * IPOINTS;
	zr = (z * IPOINTS) - zi;

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
		scale = (double)i / (double)mp->ns;

		a = (x * scale) + 
		    (rand_half (resp->re_randptr) * mp->jitter) +
		    mp->dither[X];

		b = (y * scale) +
		    (rand_half (resp->re_randptr) * mp->jitter) +
		    mp->dither[Y];

		c = (z * scale) +
		    (rand_half (resp->re_randptr) * mp->jitter) +
		    mp->dither[Z];

		turb += marble_noise (a, b, c, mp);
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
	double	value;
	fastf_t	x,y,z;
	vect_t	color;
	fastf_t xd,yd,zd;
	int	i;

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

	value = marble_turb (x,y,z,mp);

	/*
	 *	Normalize the raw value
	 */

	if (value < mp->range[0]) value = 0;
			    else  value -= mp->range[0];

	if (value > mp->range[1]) value = 1.0;
			    else  value /= mp->range[1];

	/*
	 *	Convert the normalized value into a curve
	 */

	if (mp->compression != 0.0)
		value = value * mp->angle * mp->compression;
	   else value = value * mp->angle;

	if (mp->tensor == 1) value = sin (value);
	if (mp->tensor == 2) value = cos (value);

	if (value < 0.0) value = 0.0;

	/*
	 *	Compute the color
	 */

	for (i=0; i<3; i++) {
		if (!(mp->flags & MP_MATTE))
			color[i] = (value * mp->dk_rgb[i]) +
				   ((1.0 - value) * mp->lt_rgb[i]);
		   else color[i] = (value * mp->dk_rgb[i]) +
				   ((1.0 - value) * swp->sw_color[i]);
		}

	/*
	 *	Return the color and exit
	 */

	VMOVE (swp->sw_color, color);
	return (1);
}
