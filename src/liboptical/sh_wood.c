/*
 *  			W O O D . C
 * 
 *	Simple wood-grain texture 
 *
 *  Author -
 *	Bill Laut, Gull Island Consultants, Inc.
 *
 *  "Where Credit is Due" Authors -
 *	Tom DiGiacinto - For his linearly-interpolated noise function as
 *			 found in the "sh_marble.c" routine.
 *
 *  Description -
 *	This module implements a simple concentric ring abstraction that
 *	fairly simulates the ring pattern of wood.  The placement of the
 *	rings within the combination is controlled through two MATPARM
 *	entries which centers the rings and specifies their direction.
 *	The actual rings themselves are formed on a plane perpendicular to
 *	the direction cosines, from the sine of the product of the outward
 *	distance and a MATPARM coefficient.
 *
 *	The dithering mechanism is a slight enhancement upon the 3-D noise
 *	table which Tom DiGiacinto used in "sh_marble."  In my case, the
 *	access is still limited to 10x10x10, but the table itself is expanded
 *	to 20x20x20.  There is a MATPARM "dither" field which is used to
 *	"dither the dither."  IE, the "dither" parameter is a coefficient which
 *	is summed into Tom's interpolation routine, thereby allowing each
 *	wood-shaded combination to have a different noise pattern, and prevents
 *	all similar combinations from looking alike.  The "dither" field is
 *	initialized with the "bn_rand0to1" routine before calling the parser, so
 *	default values can be used.  However, (obviously) the user can override
 *	the defaults if desired.
 *
 *	The MATPARM fields for this shader are:
 *
 *	    id = n		Multi-region identification number
 *	    o{verlay} = n	# of dithered overlay rings to circumscribe
 *	    lt{_rgb} = R/G/B	The RGB color for the wood between the rings.
 *	    dk{_rgb} = R/G/B	The RGB color of the rings.
 *	    s{pacing} = n	Space in mm between rings
 *	    p{hase} = n		Controls thickness of the rings
 *	    qd = n		Degree of dithered "bleed" on edges of rings
 *	    qp = n		Degree of undithered "bleed" on ring edges
 *	    dd = n		Amount of 3-D dither applied to vertex
 *	    dz = n		Amount of Z-axis vertex dither
 *	    di{ther} = a/b/c	Starting point of dither within noise table
 *	    de{pth} = f		Amount of dither to add to sine
 *	    r{otation} = a/b/c	3-D rotation of rings' vertex
 *	    V = X/Y/Z		Vertex of rings' center
 *	    D = X/Y/Z		XYZ of where rings's center is aimed at
 *
 *  Source -
 *	Gull Island Consultants, Inc.
 *	P.O. Box 627
 *	Muskegon, MI  49440
 *
 *  Copyright Notice -
 *	This Software is Copyright (c) 1994, Gull Island Consultants, Inc.
 *	All rights reserved.
 *
 *  Distribution Notice -
 *	Permission is granted to freely distribute this software as part of
 *	the BRL-CAD package.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
/*#include "../rt/mathtab.h"*/

/*
 *	Sundry external references
 */
extern	fastf_t	turb_table[20][20][20];

/*
 *	Sundry routine declarations
 */

HIDDEN int	wood_init(void), wood_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), wood_render(struct application *ap, struct partition *partp, struct shadework *swp, char *dp);
HIDDEN void	wood_print(register struct region *rp);
HIDDEN void	wood_free RT_ARGS(( char * ));

HIDDEN void	wood_V_set RT_ARGS((const struct bu_structparse *, const char *, const char *, char *));
HIDDEN void	wood_D_set RT_ARGS((const struct bu_structparse *, const char *, const char *, char *));

/*
 *	functions block for the shader
 */

#ifdef eRT
struct mfuncs wood_mfuncs[] = {
	{MF_MAGIC,	"wood",	0,		MFI_HIT|MFI_UV|MFI_NORMAL,	0,
	wood_init,	wood_setup,	wood_render,	wood_print,	wood_free},

	{MF_MAGIC,	"w",		0,		MFI_HIT|MFI_UV|MFI_NORMAL,	0,
	wood_init,	wood_setup,	wood_render,	wood_print,	wood_free},

	{0,		(char *)0,	0,		0,	0,
	0,		0,		0,		0,		0}
};
#else
struct mfuncs wood_mfuncs[] = {
	{MF_MAGIC,	"wood",	0, 		MFI_HIT|MFI_UV|MFI_NORMAL,	0,
	wood_setup,	wood_render,	wood_print,	wood_free},

	{MF_MAGIC,	"w",		0,		MFI_HIT|MFI_UV|MFI_NORMAL,	0,
	wood_setup,	wood_render,	wood_print,	wood_free},

	{0,		(char *)0,	0,		0,	0,
	0,		0,		0,		0}
};
#endif

/*
 *	Impure storage area for shader
 */

struct wood_specific {
	struct	wood_specific	*forw;
	struct	region		*rp;
	int			ident;
	int			flags;
	int			overlay;
	int			ns;
	fastf_t			jitter;
	fastf_t			lt_rgb[3];
	fastf_t			dk_rgb[3];
	fastf_t			depth;
	fastf_t			spacing;
	fastf_t			phase;
	fastf_t			dd;
	fastf_t			qd;
	fastf_t			dz;
	fastf_t			qp;
	fastf_t			scale;
	fastf_t			dither[3];
	vect_t			vertex;
	vect_t			dir;
	vect_t			rot;
	vect_t			b_min;
	vect_t			b_max;
	vect_t			c_min;
	vect_t			c_max;
	vect_t			D;
	vect_t			V;
};

HIDDEN void	wood_setup_2 RT_ARGS((struct wood_specific *));

/*
 *	Flags and useful offset declarations
 */

#define WOOD_NULL	((struct wood_specific *)0)
#define WOOD_O(m)	offsetof(struct wood_specific, m)
#define WOOD_OA(m)	bu_offsetofarray(struct wood_specific, m)

#define	EXPLICIT_VERTEX		1
#define EXPLICIT_DIRECTION	2

/*
 *	Listhead for multi-region wood combinations
 */

static struct wood_specific	*Wood_Chain;

/*
 *	MATPARM parsing structure
 */

struct bu_structparse wood_parse[] = {
	{"%d",	1, "ident",		WOOD_O(ident),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "id",		WOOD_O(ident),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "overlay",		WOOD_O(overlay),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "o",			WOOD_O(overlay),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "ns",		WOOD_O(ns),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "jitter",		WOOD_O(jitter),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "j",			WOOD_O(jitter),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "lt_rgb",		WOOD_OA(lt_rgb),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "lt",		WOOD_OA(lt_rgb),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "dk_rgb",		WOOD_OA(dk_rgb),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "dk",		WOOD_OA(dk_rgb),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "spacing",		WOOD_O(spacing),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "s",			WOOD_O(spacing),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "scale",		WOOD_O(scale),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "sc",		WOOD_O(scale),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "phase",		WOOD_O(phase),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "p",			WOOD_O(phase),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "qd",		WOOD_O(qd),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "qp",		WOOD_O(qp),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "dither",		WOOD_OA(dither),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "di",		WOOD_OA(dither),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "depth",		WOOD_O(depth),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "de",		WOOD_O(depth),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "dd",		WOOD_O(dd),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "dz",		WOOD_O(dz),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "rotation",		WOOD_OA(rot),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "r",			WOOD_OA(rot),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	3, "D",			WOOD_OA(D),		wood_D_set },
	{"%f",	3, "V",			WOOD_OA(V),		wood_V_set },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

/*
 *	Noise Table.  Based upon work by Tom DiGiacinto.  This table gives us
 *	a linearly-interpolated noise function for dithering location of rings
 *	within the wood surface.  It is approximately four times bigger than is
 *	actually needed, so that we can dither the starting position on a 
 *	per-region basis, and thus make each region look unique.
 */

#define IPOINTS 10			/* undithered number of points */
#define TPOINTS 20			/* Dithering space */

static	int	wood_done = 0;

/*
 *			W O O D _ I N I T
 *
 *	This routine is called at the beginning of RT's cycle to initialize
 *	the noise table. 
 */

HIDDEN int wood_init (void)
{

	/*
	 *	Initialize the wood chain
	 */

	Wood_Chain = WOOD_NULL;

	/*
	 *	Return to caller
	 */

	return (1);
}

/*
 *		M I S C _ S E T U P _ F U N C T I O N S
 *
 *	The following are miscellaneous routines which are invoked by the parser
 *	to set flag bits, indicating the presence of actual parsed values.
 */

HIDDEN void wood_V_set (const struct bu_structparse *sdp, const char *name, const char *base, char *value)
{
	register struct wood_specific *wd =
		(struct wood_specific *)base;

	wd->flags |= EXPLICIT_VERTEX;
}

HIDDEN void wood_D_set (const struct bu_structparse *sdp, const char *name, const char *base, char *value)
{
	register struct wood_specific *wd =
		(struct wood_specific *)base;

	wd->flags |= EXPLICIT_DIRECTION;
}

/*
 *			W O O D _ S E T U P
 */
HIDDEN int wood_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)
                      	    
             		         
    			      
                             
                                /* New since 4.4 release */
{
	register int i;
	register struct wood_specific *wd;

	extern struct resource		rt_uniresource;
	register struct resource	*resp = &rt_uniresource;

#ifndef eRT
	/*
	 *	If this isn't the customized RT, then call "wood_init"
	 *	here to prep the noise tables.
	 */

	if (!wood_done) {
		wood_init();
		wood_done = 1;
		}
#endif

	/*
	 *	Get the impure storage for the control block
	 */

	BU_CK_VLS( matparm );
	BU_GETSTRUCT( wd, wood_specific );
	*dpp = (char *)wd;

	/*
	 *	Load the default values
	 */

	if (rp->reg_mater.ma_color_valid) {
		VSCALE (wd->lt_rgb, rp->reg_mater.ma_color, 255);
		}
	   else {
		wd->lt_rgb[0] = 255;	/* Light yellow */
		wd->lt_rgb[1] = 255;
		wd->lt_rgb[2] = 224;
		}

	wd->dk_rgb[0] = 191;		/* Brownish-red */
	wd->dk_rgb[1] =  97;
	wd->dk_rgb[2] =   0;

	wd->ident     = 0;
	wd->forw      = WOOD_NULL;
	wd->rp	      = rp;
	wd->flags     = 0;
	wd->overlay   = 0;		/* Draw only one ring */
	wd->ns	      = 10;
	wd->jitter    = 0.0;
	wd->scale     = 1.0;
	wd->spacing   = 5;		/* 5mm space between rings */
	wd->dd        = 0.0;		/* no dither of vertex */
	wd->dz        = 0.0;		/* nor of Z-axis */
	wd->qd        = 0;
	wd->qp        = 0;
	wd->phase     = 5;
	wd->depth     = 0;

	wd->dither[0] = bn_rand0to1 (resp->re_randptr);
	wd->dither[1] = bn_rand0to1 (resp->re_randptr);
	wd->dither[2] = bn_rand0to1 (resp->re_randptr);

	VSETALL (wd->rot, 0);
	VSETALL (wd->vertex, 0);
	VSETALL (wd->D, 0);
	VSETALL (wd->V, 0);

	/*
	 *	Parse the MATPARM field
	 */

	if (bu_struct_parse( matparm, wood_parse, (char *)wd ) < 0 )  {
		bu_free( (char *)wd, "wood_specific" );
		return(-1);
		}

	/*
	 *	Do some sundry range and misc. checking
	 */

	for (i=0; i<3; i++) {
		if (wd->dither[i] < 0 || wd->dither[i] > 1.0) {
			bu_log ("wood_setup(%s):  dither is out of range.\n",
				rp->reg_name);
			return (-1);
			}
		}

	if (wd->flags == EXPLICIT_VERTEX) {
		bu_log ("wood_setup(%s):  Explicit vertex specfied without direction\n", rp->reg_name);
		return (-1);
		}

	if (wd->flags == EXPLICIT_DIRECTION) {
		bu_log ("wood_setup(%s):  Explicit direction specfied without vertex\n", rp->reg_name);
		return (-1);
		}

	/*
	 *	Get the bounding RPP
	 */

	if (rt_bound_tree (rp->reg_treetop, wd->b_min, wd->b_max) < 0) return (-1);

	/*
	 *	Add it to the wood chain
	 */

	wd->forw   = Wood_Chain;
	Wood_Chain = wd;

	/*
	 *	See if the user has flagged this region as a member of a larger
	 *	combination.  If so, go ahead and process it
	 */

	if (wd->ident == 0)
		wood_setup_2 (wd);

	   else {
		register struct wood_specific	*wc;
		vect_t			c_min, c_max;

		/*
		 *	First, process the accumulated chain of wood regions and
		 *	process all regions which have the specified ident field.
		 */

		VSETALL (c_min, 0);
		VSETALL (c_max, 0);

		for (wc = Wood_Chain; wc != WOOD_NULL; wc = wc->forw) {
			if (wc->ident == wd->ident) {
				VMIN (c_min, wc->b_min);
				VMAX (c_max, wc->b_max);
				}
			}

		/*
		 *	Now, loop through the chain again this time updating the
		 *	regions' min/max fields with the new values
		 */

		for (wc = Wood_Chain; wc != WOOD_NULL; wc = wc->forw) {
			if (wc->ident == wd->ident) {
				VMOVE (wc->b_min, c_min);
				VMOVE (wc->b_max, c_max);
				wood_setup_2 (wc);
				}
			}

		/*
		 *	End of multi-region processing loop
		 */

		}

	/*
	 *	Normalize the RGB colors
	 */

	for (i = 0; i < 3; i++) {
		wd->lt_rgb[i] *= bn_inv255;
		wd->dk_rgb[i] *= bn_inv255;
		}

	/*
	 *	Return to the caller
	 */

	return (1);
}

/*
 *	Phase 2 setup routine
 */

HIDDEN void wood_setup_2 (struct wood_specific *wd)
{
	mat_t	xlate;
	int	i;
	vect_t	a_vertex, a_dir;

	extern struct resource		rt_uniresource;
	register struct resource	*resp = &rt_uniresource;

	/*
	 *	See if the user specified absolute coordinates for the vertex and 
	 *	direction.  If so, use those instead of the RPP.
	 */

	bn_mat_angles (xlate, V3ARGS (wd->rot));

	if (wd->flags & EXPLICIT_VERTEX) {
		MAT4X3PNT (wd->vertex, xlate, wd->V);
		MAT4X3PNT (wd->dir, xlate, wd->D);
		}
	   else {
		if (wd->dz > 0.0) {
			for (i=0; i<2; i++) {
				a_vertex[i] = wd->b_min[i];
				a_dir[i] = wd->b_max[i];
				}
			/* Z component is [2] */
			a_vertex[2] = ((wd->b_max[2] - wd->b_min[2]) * 
					(bn_rand0to1(resp->re_randptr) * wd->dz)) + wd->b_min[2];
			a_dir[2]    = ((wd->b_max[2] - wd->b_min[2]) * 
					(bn_rand0to1(resp->re_randptr) * wd->dz)) + wd->b_min[2];
			}
		   else {
			for (i=0; i<3; i++) {
				a_vertex[i] = ((wd->b_max[i] - wd->b_min[i]) * 
						(bn_rand0to1(resp->re_randptr) * wd->dd)) + wd->b_min[i];
				a_dir[i]    = ((wd->b_max[i] - wd->b_min[i]) * 
						(bn_rand0to1(resp->re_randptr) * wd->dd)) + wd->b_max[i];
				}
			}
		MAT4X3PNT (wd->vertex, xlate, a_vertex);
		MAT4X3PNT (wd->dir, xlate, a_dir);
		}

	VSUB2 (wd->dir, wd->dir, wd->vertex);
	VUNITIZE (wd->dir);
}

/*
 *			W O O D _ P R I N T
 */
HIDDEN void wood_print(register struct region *rp)
{
	bu_struct_print(rp->reg_name, wood_parse, (char *)rp->reg_udata);
}

/*
 *			W O O D _ F R E E
 *
 *	This routine is called to free up the user block at the end
 *	of a frame, as well as clean up any references to objects on
 *	the Wood_Chain list.
 */
HIDDEN void wood_free (char *cp)
{
	register struct wood_specific *wd =
		(struct wood_specific *)cp;

	register struct wood_specific *wc;

	if (Wood_Chain == wd) {
/*		bu_log ("wood_free(%s):  Releasing region (at head).\n", wd->rp->reg_name); */
		Wood_Chain = wd->forw;
		bu_free ((char *)wd, "wood_specific");
		return;
		}

	for (wc = Wood_Chain; wc != WOOD_NULL; wc = wc->forw) {
		if (wc->forw == wd) {
/*			bu_log ("wood_free(%s):  Releasing region.\n", wd->rp->reg_name); */
			wc->forw = wd->forw;
			bu_free ((char *)wd, "wood_specific");
			return;
			}
		}

	bu_free ((char *)wd, "wood_specific");
}

/*
 *		N O I S E  &  T U R B U L E N C E
 *
 *  These are the noise and turbulence routines which the rendering routine
 *  uses to perturb the rings.  They are lifted directly from the "sh_marble"
 *  routine.  Eventually, they will be moved into a separate library for
 *  dealing with noise and turbulence.
 */

HIDDEN double wood_noise (double x, double y, double z, struct wood_specific *wd)
{
	int	xi, yi, zi;
	double	xr, yr, zr;
	double	n1, n2, noise1, noise2, noise3, noise;

	xi = x * IPOINTS;
	xr = (x * IPOINTS) - xi;
	yi = y * IPOINTS;
	yr = (y * IPOINTS) - yi;
	zi = z * IPOINTS;
	zr = (z * IPOINTS) - zi;

	n1     = (1 - xr) * turb_table[xi][yi][zi] + 
		       xr * turb_table[xi + 1][yi][zi];
	n2     = (1 - xr) * turb_table[xi][yi + 1][zi] +
		       xr * turb_table[xi + 1][yi + 1][zi];
	noise1 = (1 - yr) * n1 + yr * n2;

	n1     = (1 - xr) * turb_table[xi][yi][zi + 1] +
		       xr * turb_table[xi + 1][yi][zi + 1];
	n2     = (1 - xr) * turb_table[xi][yi + 1][zi + 1] +
		       xr * turb_table[xi + 1][yi + 1][zi + 1];
	noise2 = (1 - yr) * n1 + yr * n2;

	noise3 = (1 - zr) * noise1 + zr * noise2;
	noise  = pow (noise3, wd->scale);

	return (noise);
}

HIDDEN double wood_turb (double x, double y, double z, struct wood_specific *wd)
{
	extern struct resource		rt_uniresource;
	register struct resource	*resp = &rt_uniresource;

	int	i;
	fastf_t	a, b, c, turb = 0.0, scale;

	for (i=0; i<wd->ns; i++) {
		scale = (double)i / (double)wd->ns;

		a = (x * scale) +
		    (bn_rand_half (resp->re_randptr) * wd->jitter) +
		    wd->dither[X];

		b = (y * scale) +
		    (bn_rand_half (resp->re_randptr) * wd->jitter) +
		    wd->dither[Y];

		c = (z * scale) +
		    (bn_rand_half (resp->re_randptr) * wd->jitter) +
		    wd->dither[Z];

		turb += wood_noise (a, b, c, wd);
		}

	return (turb);
}

/*
 *  			W O O D _ R E N D E R
 *  
 *  Given an XYZ hit point, compute the concentric ring structure.  We do
 *  this by computing the dot-product of the hit point vs the ring vertex,
 *  which is then used to compute the distance from the ring center.  This
 *  distance is then multiplied by a velocity coefficient that is sined.
 */
HIDDEN int wood_render(struct application *ap, struct partition *partp, struct shadework *swp, char *dp)
{
	register struct wood_specific *wd =
		(struct wood_specific *)dp;

	vect_t	g, h;
	point_t	dprod, lprod;
	double	c, A, B, C;
	double	x, y, z, xd, yd, zd;
	double	mixture, pp, pq, wt;

	/*
	 *	Compute the normalized hit point
	 */

	xd = wd->b_max[0] - wd->b_min[0] + 1.0;
	yd = wd->b_max[1] - wd->b_min[1] + 1.0;
	zd = wd->b_max[2] - wd->b_min[2] + 1.0;

	x = wd->dither[X] + ((swp->sw_hit.hit_point[0] - wd->b_min[0]) / xd);
	y = wd->dither[Y] + ((swp->sw_hit.hit_point[1] - wd->b_min[1]) / yd);
	z = wd->dither[Z] + ((swp->sw_hit.hit_point[2] - wd->b_min[2]) / zd);

	/*
	 *	Compute the distance from the ring center to the hit
	 *	point by formulating a triangle between the hit point,
	 *	the ring vertex, and ring's local X-axis.
	 */

	VSUB2 (h, swp->sw_hit.hit_point, wd->vertex);
	VMOVE (g, h);
	VUNITIZE (g);				/* xlate to ray */

	wt = wood_turb (x,y,z,wd) * wd->depth;	/* used in two places */

	c = fabs (VDOT (g, wd->dir));
	A = MAGNITUDE (h) + wt;
	B = c * A;				/* abscissa */
	C = sqrt (pow (A, 2.0) - pow (B, 2.0));	/* ordinate */ 

	/*
	 *	Divide the ordinate by the spacing coefficient, and
	 *	compute the sine from that product.
	 */

	c = fabs (sin ((C / wd->spacing) * bn_pi));

	/*
	 *	Dither the "q" control
	 */

	pq = cos (((wd->qd * wt) + wd->qp + wd->phase) * bn_degtorad);
	pp = cos (wd->phase * bn_degtorad);

	/*
	 *	Color the hit point based on the phase of the ring
	 */

	if (c < pq) {
		VMOVE (swp->sw_color, wd->lt_rgb);
		}
	   else if (c >= pp) {
			VMOVE (swp->sw_color, wd->dk_rgb);
			}
		   else {
			mixture = (c - pq) / (pp - pq);
			VSCALE (lprod, wd->lt_rgb, (1.0 - mixture));
			VSCALE (dprod, wd->dk_rgb, mixture);
			VADD2 (swp->sw_color, lprod, dprod);
			}

	/*
	 *	All done.  Return to the caller
	 */

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
