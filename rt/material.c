/*
 *			M A T E R I A L . C
 *
 *  Routines to coordinate the implementation of material properties
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSmaterial[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "./rdebug.h"

static CONST char *mdefault = "default"; /* Name of default material */

/*
 *			M L I B _ A D D _ S H A D E R
 *
 *  Routine to add an array of mfuncs structures to the linked
 *  list of material (shader) routines.
 */
void
mlib_add_shader( headp, mfp1 )
struct mfuncs **headp;
struct mfuncs *mfp1;
{
	register struct mfuncs *mfp;

	RT_CK_MF(mfp1);
	for( mfp = mfp1; mfp->mf_name != (char *)0; mfp++ )  {
		RT_CK_MF(mfp);
		mfp->mf_forw = *headp;
		*headp = mfp;
	}
}

/*
 *			M L I B _ S E T U P
 *
 *  Returns -
 *	-1	failed
 *	 0	indicates that this region should be dropped
 *	 1	success
 */
int
mlib_setup( headp, rp, rtip )
struct mfuncs		**headp;
register struct region	*rp;
struct rt_i		*rtip;
{
	register CONST struct mfuncs *mfp;
	int		ret;
	struct bu_vls	param;
	CONST char	*material;
	int		mlen;

	RT_CK_REGION(rp);
	RT_CK_RTI(rtip);

	if( rp->reg_mfuncs != (char *)0 )  {
		bu_log("mlib_setup:  region %s already setup\n", rp->reg_name );
		return(-1);
	}
	bu_vls_init( &param );
	material = rp->reg_mater.ma_shader;
	if( material == NULL || material[0] == '\0' )  {
		material = mdefault;
		mlen = strlen(mdefault);
	} else {
		char	*endp;
		endp = strchr( material, ' ' );
		if( endp )  {
			mlen = endp - material;
			bu_vls_strcpy( &param, rp->reg_mater.ma_shader+mlen+1 );
		} else {
			mlen = strlen(material);
		}
	}
retry:
	for( mfp = *headp; mfp != MF_NULL; mfp = mfp->mf_forw )  {
		if( material[0] != mfp->mf_name[0]  ||
		    strncmp( material, mfp->mf_name, mlen ) != 0 )
			continue;
		goto found;
	}
	bu_log("\n*ERROR mlib_setup('%s'):  material not known, default assumed %s\n",
		material, rp->reg_name );
	if( material != mdefault )  {
		material = mdefault;
		mlen = strlen(mdefault);
		bu_vls_trunc( &param, 0 );
		goto retry;
	}
	bu_vls_free( &param );
	return(-1);
found:
	rp->reg_mfuncs = (char *)mfp;
	rp->reg_udata = (char *)0;

	if(rdebug&RDEBUG_MATERIAL)
		bu_log("mlib_setup(%s) shader=%s\n", rp->reg_name, mfp->mf_name);
	if( (ret = mfp->mf_setup( rp, &param, &rp->reg_udata, mfp, rtip, headp )) < 0 )  {
		bu_log("ERROR mlib_setup(%s) failed. Material='%s', param='%s'.\n",
			rp->reg_name, material, bu_vls_addr(&param) );
		if( material != mdefault )  {
			/* If not default material, change to default & retry */
			bu_log("\tChanging %s material to default and retrying.\n", rp->reg_name);
			material = mdefault;
			bu_vls_trunc( &param, 0 );
			goto retry;
		}
		/* What to do if default setup fails? */
		bu_log("mlib_setup(%s) error recovery failed.\n", rp->reg_name);
	}
	bu_vls_free( &param );
	return(ret);		/* Good or bad, as mf_setup says */
}

/*
 *			M L I B _ F R E E
 *
 *  Routine to free material-property specific data
 */
void
mlib_free( rp )
register struct region *rp;
{
	register CONST struct mfuncs *mfp = (struct mfuncs *)rp->reg_mfuncs;

	if( mfp == MF_NULL )  {
		bu_log("mlib_free(%s):  reg_mfuncs NULL\n", rp->reg_name);
		return;
	}
	if( mfp->mf_magic != MF_MAGIC )  {
		bu_log("mlib_free(%s):  reg_mfuncs bad magic, %x != %x\n",
			rp->reg_name,
			mfp->mf_magic, MF_MAGIC );
		return;
	}
	if( mfp->mf_free ) mfp->mf_free( rp->reg_udata );
	rp->reg_mfuncs = (char *)0;
	rp->reg_udata = (char *)0;
}

/*
 *			M L I B _ Z E R O
 *
 *  Regardless of arguments, always return zero.
 *  Useful mostly as a stub print, and/or free routine.
 */
/* VARARGS */
int
mlib_zero()
{
	return(0);
}

/*
 *			M L I B _ O N E
 *
 *  Regardless of arguments, always return one.
 *  Useful mostly as a stub setup routine.
 */
/* VARARGS */
int
mlib_one()
{
	return(1);
}

/*
 *			M L I B _ V O I D
 */
/* VARARGS */
void
mlib_void()
{
}
