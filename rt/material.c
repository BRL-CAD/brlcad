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
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./material.h"
#include "./rdebug.h"

struct mfuncs *mfHead = MF_NULL;	/* Head of list of materials */

static char *mdefault = "default";	/* Name of default material */

/*
 *			M L I B _ A D D
 *
 *  Internal routine to add an array of mfuncs structures to the linked
 *  list of material routines.
 */
void
mlib_add( mfp )
register struct mfuncs *mfp;
{
	for( ; mfp->mf_name != (char *)0; mfp++ )  {
		mfp->mf_magic = MF_MAGIC;
		mfp->mf_forw = mfHead;
		mfHead = mfp;
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
mlib_setup( rp, rtip )
register struct region	*rp;
struct rt_i		*rtip;
{
	register struct mfuncs *mfp;
	int	ret;
	struct rt_vls	param;
	char	*material;
	int	mlen;

	RT_CK_REGION(rp);
	RT_CK_RTI(rtip);

	if( rp->reg_mfuncs != (char *)0 )  {
		rt_log("mlib_setup:  region %s already setup\n", rp->reg_name );
		return(-1);
	}
	rt_vls_init( &param );
	material = rp->reg_mater.ma_shader;
	if( material == NULL || material[0] == '\0' )  {
		material = mdefault;
		mlen = strlen(mdefault);
	} else {
		char	*endp;
		endp = strchr( material, ' ' );
		if( endp )  {
			mlen = endp - material;
			rt_vls_strcpy( &param, rp->reg_mater.ma_shader+mlen+1 );
		}
	}
retry:
	for( mfp=mfHead; mfp != MF_NULL; mfp = mfp->mf_forw )  {
		if( material[0] != mfp->mf_name[0]  ||
		    strncmp( material, mfp->mf_name, mlen ) != 0 )
			continue;
		goto found;
	}
	rt_log("\n*ERROR mlib_setup('%s'):  material not known, default assumed %s\n",
		material, rp->reg_name );
	if( material != mdefault )  {
		material = mdefault;
		mlen = strlen(mdefault);
		rt_vls_trunc( &param, 0 );
		goto retry;
	}
	rt_vls_free( &param );
	return(-1);
found:
	rp->reg_mfuncs = (char *)mfp;
	rp->reg_udata = (char *)0;

	if( (ret = mfp->mf_setup( rp, &param, &rp->reg_udata, mfp, rtip )) < 0 )  {
		rt_log("ERROR mlib_setup(%s) failed. Material='%s', param='%s'.\n",
			rp->reg_name, material, RT_VLS_ADDR(&param) );
		if( material != mdefault )  {
			/* If not default material, change to default & retry */
			rt_log("\tChanging %s material to default and retrying.\n", rp->reg_name);
			material = mdefault;
			rt_vls_trunc( &param, 0 );
			goto retry;
		}
		/* What to do if default setup fails? */
		rt_log("mlib_setup(%s) error recovery failed.\n", rp->reg_name);
	}
	rt_vls_free( &param );
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
	register struct mfuncs *mfp = (struct mfuncs *)rp->reg_mfuncs;

	if( mfp == MF_NULL )  {
		rt_log("mlib_free(%s):  reg_mfuncs NULL\n", rp->reg_name);
		return;
	}
	if( mfp->mf_magic != MF_MAGIC )  {
		rt_log("mlib_free(%s):  reg_mfuncs bad magic, %x != %x\n",
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

/*
 *			P R _ S H A D E W O R K
 *
 *  Pretty print a shadework structure.
 */
void
pr_shadework( str, swp )
char *str;
register struct shadework *swp;
{
	int	i;

	rt_log( "Shadework %s: 0x%x\n", str, swp );
	rt_log( " sw_transmit %f\n", swp->sw_transmit );
	rt_log( " sw_reflect %f\n", swp->sw_reflect );
	rt_log( " sw_refract_index %f\n", swp->sw_refrac_index );
	rt_log( " sw_extinction %f\n", swp->sw_extinction );
	VPRINT( " sw_color", swp->sw_color );
	VPRINT( " sw_basecolor", swp->sw_basecolor );
	rt_log( " sw_uv  %f %f\n", swp->sw_uv.uv_u, swp->sw_uv.uv_v );
	rt_log( " sw_xmitonly %d\n", swp->sw_xmitonly );
	rt_printb( " sw_inputs", swp->sw_inputs,
		"\020\4HIT\3LIGHT\2UV\1NORMAL" );
	rt_log( "\n");
	for( i=0; i < SW_NLIGHTS; i++ )  {
		if( swp->sw_visible[i] == (char *)0 )  continue;
		rt_log("   light %d visible, intensity=%g, dir=(%g,%g,%g)\n",
			i,
			swp->sw_intensity[i],
			V3ARGS(&swp->sw_tolight[i*3]) );
	}
}
