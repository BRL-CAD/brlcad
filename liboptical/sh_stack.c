/*
 *			S T A C K . C
 *
 *  Stack multiple material modules together
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./rdebug.h"

HIDDEN int	stk_setup(), stk_render();
HIDDEN void	stk_print(), stk_free();

CONST struct mfuncs stk_mfuncs[] = {
	{MF_MAGIC,	"stack",	0,		0,	0,
	stk_setup,	stk_render,	stk_print,	stk_free},

	{0,		(char *)0,	0,		0,	0,
	0,		0,		0,		0}
};

struct stk_specific {
	struct	mfuncs	*mfuncs[16];
	char	*udata[16];
};
#define STK_NULL	((struct stk_specific *)0)
#define STK_O(m)	offsetof(struct stk_specific, m)

struct bu_structparse stk_parse[] = {
	{"",	0,	(char *)0,	0,			FUNC_NULL }
};

/*
 *			S T K _ S E T U P
 *
 *  Returns 0 on failure, 1 on success.
 */
HIDDEN int
stk_setup( rp, matparm, dpp, mf_p, rtip )
register struct region *rp;
struct rt_vls	*matparm;	/* parameter string */
char		**dpp;		/* pointer to user data pointer */
struct mfuncs	*mf_p;
struct rt_i	*rtip;
{
	register struct stk_specific *sp;
	char	*cp, *start;
	int	i;
	int	inputs = 0;
	struct mfuncs *mfp;

	RT_VLS_CHECK( matparm );
	RT_CK_RTI(rtip);

	GETSTRUCT( sp, stk_specific );
	*dpp = (char *)sp;

	/*bu_struct_parse( matparm, stk_parse, (char *)sp );*/

	if(rdebug&RDEBUG_MATERIAL)
		rt_log( "stk_setup called with \"%s\"\n", RT_VLS_ADDR(matparm) );
	i = 0;
	start = cp = RT_VLS_ADDR(matparm);
	while( *cp != '\0' ) {
		if( *cp == ';' ) {
			*cp = '\0';
			if( i >= 16 ) {
				rt_log( "stk_setup: max levels exceeded\n" );
				return( 0 );
			}
			/* add one */
			if( dosetup(start, rp, &sp->udata[i], &sp->mfuncs[i],
				rtip) == 0 )  {
				inputs |= sp->mfuncs[i]->mf_inputs;
				i++;
			} else {
				/* XXX else clear entry? */
				rt_log("stk_setup problem\n");
			}
			start = ++cp;
		} else {
			cp++;
		}
	}
	if( start != cp ) {
		if( i >= 16 ) {
			rt_log( "stk_setup: max levels exceeded\n" );
			return( 0 );
		}
		/* add one */
		if( dosetup(start, rp, &sp->udata[i], &sp->mfuncs[i],
		    rtip ) == 0 )  {
			inputs |= sp->mfuncs[i]->mf_inputs;
			i++;
		} else {
			/* XXX else clear entry? */
		}
	}

	/* Request only those input bits needed by subordinate shaders */
	GETSTRUCT( mfp, mfuncs );
	bcopy( (char *)rp->reg_mfuncs, (char *)mfp, sizeof(*mfp) );
	mfp->mf_inputs = inputs;
	rp->reg_mfuncs = (genptr_t)mfp;
	return( 1 );
}

/*
 *  			S T K _ R E N D E R
 *  
 *  Evaluate all of the rendering functions in the stack.
 *
 *  Returns:
 *	0	stack processing aborted
 *	1	stack processed to completion
 */
HIDDEN
stk_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct stk_specific *sp =
		(struct stk_specific *)dp;
	int	i;
	int	ret_status;
	char	tmp[128];

	for( i = 0; i < 16 && sp->mfuncs[i] != NULL; i++ ) {
		if( rdebug&RDEBUG_SHADE)  {
			sprintf(tmp, "before stacked \"%s\" shader", sp->mfuncs[i]->mf_name);

			pr_shadework( tmp, swp );
		}

		ret_status = sp->mfuncs[i]->mf_render( ap, pp, swp,
			sp->udata[i] );

		if ( ! ret_status ) return ret_status;

	}
	return(1);
}

/*
 *			S T K _ P R I N T
 */
HIDDEN void
stk_print( rp, dp )
register struct region *rp;
char	*dp;
{
	register struct stk_specific *sp =
		(struct stk_specific *)dp;
	int	i;

	rt_log("~~~~starting stack print\n");

	for( i = 0; i < 16 && sp->mfuncs[i] != NULL; i++ ) {
		rt_log("~~~~stack entry %d:\n", i);
		sp->mfuncs[i]->mf_print( rp, sp->udata[i] );
	}

	rt_log("~~~~ending stack print\n");
}

/*
 *			S T K _ F R E E
 */
HIDDEN void
stk_free( cp )
char *cp;
{
	register struct stk_specific *sp =
		(struct stk_specific *)cp;
	int	i;

	for( i = 0; i < 16 && sp->mfuncs[i] != NULL; i++ ) {
		sp->mfuncs[i]->mf_free( sp->udata[i] );
	}

	rt_free( cp, "stk_specific" );
}

extern struct mfuncs *mfHead;	/* Head of list of materials */

dosetup( cp, rp, dpp, mpp, rtip )
char	*cp;
struct region	*rp;
char	**dpp;		/* udata pointer address */
char	**mpp;		/* mfuncs pointer address */
struct rt_i	*rtip;
{
	register struct mfuncs *mfp;
	struct rt_vls	arg;
	char	matname[32];
	int	i;

	RT_CK_RTI(rtip);

	if(rdebug&RDEBUG_MATERIAL)
		rt_log( "...starting \"%s\"\n", cp );

	/* skip leading white space */
	while( *cp == ' ' || *cp == '\t' )
		cp++;

	for( i = 0; i < 31 && *cp != '\0'; i++, cp++ ) {
		if( *cp == ' ' || *cp == '\t' ) {
			matname[i++] = '\0';
			break;
		} else
			matname[i] = *cp;
	}
	matname[i] = '\0';	/* ensure null termination */

	for( mfp=mfHead; mfp != MF_NULL; mfp = mfp->mf_forw )  {
		if( matname[0] != mfp->mf_name[0]  ||
		    strcmp( matname, mfp->mf_name ) != 0 )
			continue;
		goto found;
	}
	rt_log("stack_setup(%s):  material not known\n",
		matname );
	return(-1);

found:
	*mpp = (char *)mfp;
	*dpp = (char *)0;
	RT_VLS_INIT( &arg );
	rt_vls_strcat( &arg, cp );
	if( mfp->mf_setup( rp, &arg, dpp, mfp, rtip ) < 0 )  {
		/* What to do if setup fails? */
		return(-1);		/* BAD */
	}
	rt_vls_free( &arg );
	return(0);			/* OK */
}
