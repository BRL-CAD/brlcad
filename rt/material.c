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

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
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
mlib_setup( rp )
register struct region *rp;
{
	register struct mfuncs *mfp;
	int	ret;
	char	param[256];
	char	*material;

	if( rp->reg_mfuncs != (char *)0 )  {
		rt_log("mlib_setup:  region %s already setup\n", rp->reg_name );
		return(-1);
	}
	material = rp->reg_mater.ma_matname;
	if( material[0] == '\0' )
		material = mdefault;
retry:
	for( mfp=mfHead; mfp != MF_NULL; mfp = mfp->mf_forw )  {
		if( material[0] != mfp->mf_name[0]  ||
		    strcmp( material, mfp->mf_name ) != 0 )
			continue;
		goto found;
	}
	rt_log("mlib_setup(%s):  material not known, default assumed\n",
		material );
	if( material != mdefault )  {
		material = mdefault;
		goto retry;
	}
	return(-1);
found:
	rp->reg_mfuncs = (char *)mfp;
	rp->reg_udata = (char *)0;
	strncpy( param, rp->reg_mater.ma_matparm, sizeof(rp->reg_mater.ma_matparm) );
	param[sizeof(rp->reg_mater.ma_matparm)+1] = '\0';

	if( (ret = mfp->mf_setup( rp, param, &rp->reg_udata )) < 0 )  {
		/* What to do if setup fails? */
		if( material != mdefault )  {
			material = mdefault;
			goto retry;
		}
	}
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
		rt_log("mlib_free:  reg_mfuncs NULL\n");
		return;
	}
	if( mfp->mf_magic != MF_MAGIC )  {
		rt_log("mlib_free:  reg_mfuncs bad magic, %x != %x\n",
			mfp->mf_magic, MF_MAGIC );
		return;
	}
	mfp->mf_free( rp->reg_udata );
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
	rt_log( "Shadework %s: 0x%x\n", str, swp );
	rt_log( " sw_transmit %f\n", swp->sw_transmit );
	rt_log( " sw_reflect %f\n", swp->sw_reflect );
	rt_log( " sw_refract_index %f\n", swp->sw_refrac_index );
	rt_log( " sw_extinction %f\n", swp->sw_extinction );
	VPRINT( " sw_color", swp->sw_color );
	VPRINT( " sw_basecolor", swp->sw_basecolor );
	rt_log( " sw_uv  %f %f\n", swp->sw_uv.uv_u, swp->sw_uv.uv_v );
	rt_log( " sw_xmitonly %d\n", swp->sw_xmitonly );
	rt_log( " sw_inputs 0x%x\n", swp->sw_inputs );
}
