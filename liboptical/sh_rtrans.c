/*
 *	S H _ R T R A N S . C
 *
 *	Random transparency shader. A random number from 0 to 1 is drawn
 * for each pixel rendered. If the random number is less than the threshold
 * value, the pixel is rendered as 100% transparent
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "rtprivate.h"

extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);
#define RTRANS_MAGIC 0x4a6f686e
struct rtrans_specific {
	long	magic;
	double	threshold;
	int	next_rand;
};
#define CK_RTRANS_SP(_p) BU_CKMAG(_p, RTRANS_MAGIC, "rtrans_specific")

static struct rtrans_specific rtrans_defaults = {
	RTRANS_MAGIC,
	0.5,
	3 } ;

#define SHDR_NULL	((struct rtrans_specific *)0)
#define SHDR_O(m)	offsetof(struct rtrans_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct rtrans_specific, m)

struct bu_structparse rtrans_parse[] = {
	{"%f",  1, "threshold",		SHDR_O(threshold),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "t",			SHDR_O(threshold),		BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	rtrans_setup(), rtrans_render();
HIDDEN void	rtrans_print(), rtrans_free();

struct mfuncs rtrans_mfuncs[] = {
	{MF_MAGIC,	"rtrans",	0,		0,	0,
	rtrans_setup,	rtrans_render,	rtrans_print,	rtrans_free },

	{0,		(char *)0,	0,		0,	0,
	0,		0,		0,		0 }
};


/*	R T R A N S _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
rtrans_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct bu_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct rtrans_specific	*rtrans_sp;

	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);
	BU_GETSTRUCT( rtrans_sp, rtrans_specific );
	*dpp = (char *)rtrans_sp;

	memcpy(rtrans_sp, &rtrans_defaults, sizeof(struct rtrans_specific) );

	if (bu_struct_parse( matparm, rtrans_parse, (char *)rtrans_sp ) < 0 )
		return(-1);

	BN_RANDSEED(rtrans_sp->next_rand, 3);

	if (rdebug&RDEBUG_SHADE)
		bu_struct_print( rp->reg_name, rtrans_parse, (char *)rtrans_sp );

	return(1);
}

/*
 *	R T R A N S _ P R I N T
 */
HIDDEN void
rtrans_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, rtrans_parse, (char *)dp );
}

/*
 *	R T R A N S _ F R E E
 */
HIDDEN void
rtrans_free( cp )
char *cp;
{
	bu_free( cp, "rtrans_specific" );
}

/*
 *	R T R A N S _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c)
 *	once for each hit point to be shaded.
 */
int
rtrans_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct rtrans_specific *rtrans_sp =
		(struct rtrans_specific *)dp;

	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_RTRANS_SP(rtrans_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_struct_print( "random transparency", rtrans_parse, (char *)rtrans_sp );

	if (rtrans_sp->threshold >= 1.0 ||
	    BN_RANDOM(rtrans_sp->next_rand)  < rtrans_sp->threshold )
	{
		swp->sw_transmit = 1.0;
		swp->sw_reflect = 0.0;
		swp->sw_refrac_index = 1.0;
		VSETALL( swp->sw_basecolor, 1.0 );

		if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
	}

	return(1);
}
