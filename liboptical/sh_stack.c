#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./rdebug.h"

HIDDEN int stk_setup(), stk_render(), stk_print(), stk_free();

struct mfuncs stk_mfuncs[] = {
	"stack",	0,		0,		MFI_UV|MFI_NORMAL,
	stk_setup,	stk_render,	stk_print,	stk_free,

	(char *)0,	0,		0,		0,
	0,		0,		0,		0
};

struct stk_specific {
	char	st_file[128];	/* Filename */
	struct	mfuncs	*mfuncs[16];
	char	*udata[16];
};
#define STK_NULL	((struct stk_specific *)0)

struct matparse stk_parse[] = {
#ifndef cray
	"file",		(mp_off_ty)(STK_NULL->st_file),	"%s",
#else
	"file",		(mp_off_ty)1,			"%s",
#endif
	(char *)0,	(mp_off_ty)0,			(char *)0
};

/*
 *			S T K _ S E T U P
 *
 *  Returns 0 on failure, 1 on success.
 */
HIDDEN int
stk_setup( rp, matparm, dpp )
register struct region *rp;
char	*matparm;	/* parameter string */
char	**dpp;		/* pointer to user data pointer */
{
	register struct stk_specific *sp;
	char	*cp, *start;
	int	i;

	GETSTRUCT( sp, stk_specific );
	*dpp = (char *)sp;

	sp->st_file[0] = '\0';
	/*mlib_parse( matparm, stk_parse, (mp_off_ty)sp );*/

fprintf( stderr, "stk_setup called with \"%s\"\n", matparm );
	i = 0;
	start = cp = matparm;
	while( *cp != NULL ) {
		if( *cp == ';' ) {
			*cp = NULL;
			if( i >= 16 ) {
				fprintf( stderr, "stk_setup: max levels exceeded\n" );
				break;
			}
			/* add one */
			if( dosetup(start, sp, i, rp, &sp->udata[i]) == 0 )
				i++;	/* XXX else clear entry? */
			start = ++cp;
		} else {
			cp++;
		}
	}

	return( 1 );
}

/*
 *  			S T K _ R E N D E R
 *  
 *  Evaluate all of the rendering functions in the stack.
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

	for( i = 0; i < 16 && sp->mfuncs[i] != NULL; i++ ) {
		sp->mfuncs[i]->mf_render( ap, pp, swp, sp->udata[i] );
	}
}

/*
 *			S T K _ P R I N T
 */
HIDDEN int
stk_print( rp, dp )
register struct region *rp;
char	*dp;
{
	mlib_print(rp->reg_name, stk_parse, (mp_off_ty)dp);
}

/*
 *			S T K _ F R E E
 */
HIDDEN int
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

dosetup( string, sp, n, rp, dpp )
char	*string;
register struct stk_specific *sp;
int	n;
char	*rp;
char	**dpp;
{
	register struct mfuncs *mfp;
	char	matname[32];
	int	i;

fprintf( stderr, "...starting \"%s\"\n", string );
	for( i = 0; i < 31 && string[i] != NULL; i++ ) {
		if( string[i] == ' ' ) {
			matname[i++] = '\0';
			break;
		} else
			matname[i] = string[i];
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
	sp->mfuncs[n] = mfp;
	sp->udata[n] = (char *)0;
	if( mfp->mf_setup( rp, &string[i], &sp->udata[n] ) < 0 )  {
		/* What to do if setup fails? */
		return(-1);		/* BAD */
	}
	return(0);			/* OK */
}
