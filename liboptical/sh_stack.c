/*
 *			S T A C K . C
 *
 *  Stack multiple material modules together
 *
 *  Author -
 *	Phillip Dykstra
 *  	Lee A. Butler
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "rtprivate.h"

HIDDEN int	stk_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mf_p, struct rt_i *rtip, struct mfuncs **headp), stk_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	stk_print(register struct region *rp, char *dp), stk_free(char *cp);
HIDDEN int	ext_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mf_p, struct rt_i *rtip, struct mfuncs **headp);

struct mfuncs stk_mfuncs[] = {
	{MF_MAGIC,	"stack",	0,		0,	0,
	stk_setup,	stk_render,	stk_print,	stk_free},

	{MF_MAGIC,	"extern",	0,		0,	0,
	ext_setup,	stk_render,	stk_print,	stk_free},

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
	{"",	0,	(char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

/*
 *			E X T _ S E T U P
 *
 *  Returns 0 on failure, 1 on success.
 */
HIDDEN int
ext_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mf_p, struct rt_i *rtip, struct mfuncs **headp)
                           
             	         	/* parameter string */
    		      		/* pointer to user data pointer */
             	      
           	      
             	        
{
	struct bu_mapped_file	*parameter_file;
	struct bu_vls		parameter_data;
	char			*filename;
	int			status;

	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);
	
	filename = bu_vls_addr(matparm);
	parameter_file = bu_open_mapped_file( filename, (char *)NULL );
	if (!parameter_file) {
		bu_log("cannot open external shader file \"%s\"\n", filename);
		bu_bomb("ext_setup()\n");
	}

	bu_vls_init(&parameter_data);
	bu_vls_strncpy( &parameter_data, (char *)parameter_file->buf,
		parameter_file->buflen );

	if (rdebug&RDEBUG_SHADE ) {
		bu_log("ext_setup(%s): {%s}\n",
			filename, bu_vls_addr(&parameter_data));
	}

	bu_close_mapped_file( parameter_file );

	status = stk_setup(rp, &parameter_data, dpp, mf_p, rtip, headp);

	bu_vls_free( &parameter_data );

	return status;
}

/*
 *			S T K _ D O S E T U P
 */
static int stk_dosetup(char *cp, struct region *rp, char **dpp, char **mpp, struct rt_i *rtip, struct mfuncs **headp)
    	    
             	    
    	      		/* udata pointer address */
    	      		/* mfuncs pointer address */
           	      
             	        
{
	register struct mfuncs *mfp;
#ifdef HAVE_DLOPEN
	register struct mfuncs *mfp_new;
#endif
	struct bu_vls	arg;
	char	matname[32];
	int	ret;
	int	i;

	RT_CK_RTI(rtip);

	if(rdebug&RDEBUG_MATERIAL)
		bu_log( "...starting \"%s\"\n", cp );

	/* skip leading white space */
	while( *cp == ' ' || *cp == '\t' )
		cp++;

	for( i = 0; i < 31 && *cp != '\0'; i++, cp++ ) {
		if (*cp == ' ' || *cp == '\t' ) {
			matname[i++] = '\0';
			break;
		} else
			matname[i] = *cp;
	}
	matname[i] = '\0';	/* ensure null termination */

#ifdef HAVE_DLOPEN
retry:
#endif
	for( mfp = *headp; mfp != MF_NULL; mfp = mfp->mf_forw )  {
		if (matname[0] != mfp->mf_name[0]  ||
		    strcmp( matname, mfp->mf_name ) != 0 )
			continue;
		goto found;
	}
#ifdef HAVE_DLOPEN
	/* If we get here, then the shader wasn't found in the list of 
	 * compiled-in (or previously loaded) shaders.  See if we can
	 * dynamically load it.
	 */

	bu_log("Shader \"%s\"... ", matname);

	if ((mfp_new = load_dynamic_shader(matname, strlen(matname)))) {
		mlib_add_shader(headp, mfp_new);
		goto retry;
	}
#else
	bu_log("****** dynamic shader loading not available ******\n");
#endif



	bu_log("stack_setup(%s):  material not known\n",
		matname );
	ret = -1;
	goto out;

found:
	*mpp = (char *)mfp;
	*dpp = (char *)0;
	bu_vls_init( &arg );
	if (*cp != '\0' )
		bu_vls_strcat( &arg, ++cp );
	if (rdebug&RDEBUG_MATERIAL)
		bu_log("calling %s with %s\n", mfp->mf_name, bu_vls_addr(&arg));
	if (mfp->mf_setup( rp, &arg, dpp, mfp, rtip, headp ) < 0 )  {
		/* Setup has failed */
		bu_vls_free( &arg );
		ret = -1;		/* BAD */
		goto out;
	}
	bu_vls_free( &arg );
	ret = 0;			/* OK */
out:
	if (rdebug&RDEBUG_MATERIAL)
		bu_log( "...finished \"%s\", ret=%d\n", matname, ret );
	return ret;
}

/*
 *			S T K _ S E T U P
 *
 *  Returns 0 on failure, 1 on success.
 */
HIDDEN int
stk_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mf_p, struct rt_i *rtip, struct mfuncs **headp)
                           
             	         	/* parameter string */
    		      		/* pointer to user data pointer */
             	      
           	      
             	        
{
	register struct stk_specific *sp;
	char	*cp, *start;
	int	i;
	int	inputs = 0;
	struct mfuncs *mfp;

	BU_CK_VLS( matparm );
	RT_CK_RTI(rtip);

	BU_GETSTRUCT( sp, stk_specific );
	*dpp = (char *)sp;

	/*bu_struct_parse( matparm, stk_parse, (char *)sp );*/

	if(rdebug&RDEBUG_MATERIAL || rdebug&RDEBUG_SHADE)
		bu_log( "stk_setup called with \"%s\"\n", bu_vls_addr(matparm) );

	i = 0;
	start = cp = bu_vls_addr(matparm);
	while( *cp != '\0' ) {
		if (*cp == ';' ) {
			*cp = '\0';
			if (i >= 16 ) {
				bu_log( "stk_setup: max levels exceeded\n" );
				return( 0 );
			}
			/* add one */
			if (stk_dosetup(start, rp, &sp->udata[i],
				(char **)&sp->mfuncs[i], rtip, headp) == 0 )  {
					inputs |= sp->mfuncs[i]->mf_inputs;
					i++;
			} else {
				/* XXX else clear entry? */
				bu_log("Problem in stack shader setup\n");
			}
			start = ++cp;
		} else {
			cp++;
		}
	}
	if (start != cp ) {
		if (i >= 16 ) {
			bu_log( "stk_setup: max levels exceeded\n" );
			return( 0 );
		}
		/* add one */
		if (stk_dosetup(start, rp, &sp->udata[i], (char **)&sp->mfuncs[i],
		    rtip, headp ) == 0 )  {
			inputs |= sp->mfuncs[i]->mf_inputs;
			i++;
		} else {
			/* XXX else clear entry? */
		}
	}

	/* Request only those input bits needed by subordinate shaders */
	BU_GETSTRUCT( mfp, mfuncs );
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
HIDDEN int
stk_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct stk_specific *sp =
		(struct stk_specific *)dp;
	int	i;
	int	ret_status;
	char	tmp[128];

	for( i = 0; i < 16 && sp->mfuncs[i] != NULL; i++ ) {
		if (rdebug&RDEBUG_SHADE)  {
			sprintf(tmp, "before stacked \"%s\" shader", sp->mfuncs[i]->mf_name);

			pr_shadework( tmp, swp );
		}

		/*
		 *  Every shader takes the shadework structure as its
		 *  input and updates it as the "output".
		 */
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
stk_print(register struct region *rp, char *dp)
{
	register struct stk_specific *sp =
		(struct stk_specific *)dp;
	int	i;

	bu_log("~~~~starting stack print\n");

	for( i = 0; i < 16 && sp->mfuncs[i] != NULL; i++ ) {
		bu_log("~~~~stack entry %d:\n", i);
		sp->mfuncs[i]->mf_print( rp, sp->udata[i] );
	}

	bu_log("~~~~ending stack print\n");
}

/*
 *			S T K _ F R E E
 */
HIDDEN void
stk_free(char *cp)
{
	register struct stk_specific *sp =
		(struct stk_specific *)cp;
	int	i;

	for( i = 0; i < 16 && sp->mfuncs[i] != NULL; i++ ) {
		sp->mfuncs[i]->mf_free( sp->udata[i] );
	}

	bu_free( cp, "stk_specific" );
}

