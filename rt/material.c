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
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./rdebug.h"

struct mfuncs *mfHead = MF_NULL;	/* Head of list of materials */

/*
 *			M L I B _ A D D
 *
 *  Internal routine to add an array of mfuncs structures to the linked
 *  list of material routines.
 */
HIDDEN void
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
 *			M L I B _ I N I T
 *
 *  Enrole the various materials.  Single point of explicit interface with
 *  the materials modules.
 */
void
mlib_init()
{
	extern struct mfuncs phg_mfuncs[];
	extern struct mfuncs light_mfuncs[];
	extern struct mfuncs cloud_mfuncs[];
	extern struct mfuncs spm_mfuncs[];
	extern struct mfuncs txt_mfuncs[];
	extern struct mfuncs stk_mfuncs[];
	extern struct mfuncs cook_mfuncs[];

	mlib_add( phg_mfuncs );
	mlib_add( light_mfuncs );
	mlib_add( cloud_mfuncs );
	mlib_add( spm_mfuncs );
	mlib_add( txt_mfuncs );
	mlib_add( stk_mfuncs );
	mlib_add( cook_mfuncs );
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

	if( rp->reg_mfuncs != (char *)0 )  {
		rt_log("mlib_setup:  region %s already setup\n", rp->reg_name );
		return(-1);
	}
	if( rp->reg_mater.ma_matname[0] == '\0' )
		goto def;
	for( mfp=mfHead; mfp != MF_NULL; mfp = mfp->mf_forw )  {
		if( rp->reg_mater.ma_matname[0] != mfp->mf_name[0]  ||
		    strcmp( rp->reg_mater.ma_matname, mfp->mf_name ) != 0 )
			continue;
		goto found;
	}
	rt_log("mlib_setup(%s):  material not known, default assumed\n",
		rp->reg_mater.ma_matname );
def:
	mfp = phg_mfuncs;		/* default */
found:
	rp->reg_mfuncs = (char *)mfp;
	rp->reg_udata = (char *)0;
	strncpy( param, rp->reg_mater.ma_matparm, sizeof(rp->reg_mater.ma_matparm) );
	param[sizeof(rp->reg_mater.ma_matparm)+1] = '\0';

	if( (ret = mfp->mf_setup( rp, param, &rp->reg_udata )) < 0 )  {
		/* What to do if setup fails? */
		if( mfp != phg_mfuncs )
			goto def;
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
 *			M L I B _ R G B
 *
 *  Parse a slash (or other non-numeric, non-whitespace) separated string
 *  as 3 decimal (or octal) bytes.  Useful for entering rgb values in
 *  mlib_parse as 4/5/6.  Element [3] is made non-zero to indicate
 *  that a value has been loaded.
 */
void
mlib_rgb( rgb, str )
register unsigned char *rgb;
register char *str;
{
	if( !isdigit(*str) )  return;
	rgb[0] = atoi(str);
	rgb[1] = rgb[2] = 0;
	rgb[3] = 1;
	while( *str )
		if( !isdigit(*str++) )  break;
	if( !*str )  return;
	rgb[1] = atoi(str);
	while( *str )
		if( !isdigit(*str++) )  break;
	if( !*str )  return;
	rgb[2] = atoi(str);
}

/*
 *			M L I B _ P A R S E
 */
void
mlib_parse( cp, parsetab, base )
register char *cp;
struct matparse *parsetab;
int *base;		/* base address of users structure */
{
	register struct matparse *mp;
	char *name;
	char *value;

	while( *cp )  {
		/* NAME = VALUE separator (comma, space, tab) */

		/* skip any leading whitespace */
		while( *cp != '\0' && 
		    (*cp == ',' || *cp == ' ' || *cp == '\t' ) )
			cp++;

		/* Find equal sign */
		name = cp;
		while( *cp != '\0' && *cp != '=' )  cp++;
		if( *cp == '\0' )  {
			rt_log("name %s without value\n", name );
			break;
		}
		*cp++ = '\0';

		/* Find end of value */
		value = cp;
		while( *cp != '\0' && *cp != ',' &&
		    *cp != ' ' && *cp != '\t' )
			cp++;
		if( *cp != '\0' )
			*cp++ = '\0';

		/* Lookup name in parsetab table */
		for( mp = parsetab; mp->mp_name != (char *)0; mp++ )  {
			register char *loc;

			if( strcmp( mp->mp_name, name ) != 0 )
				continue;
			loc = (char *)(((mp_off_ty)base) +
					((int)mp->mp_offset));
			if( mp->mp_fmt[1] == 'C' )
				mlib_rgb( loc, value );
			else
				(void)sscanf( value, mp->mp_fmt, loc );
			goto out;
		}
		rt_log("mlib_parse:  %s=%s not a valid arg\n", name, value);
out:		;
	}
}

/*
 *			M L I B _ P R I N T
 */
void
mlib_print( title, parsetab, base )
char *title;
struct matparse *parsetab;
int *base;		/* base address of users structure */
{
	register struct matparse *mp;
	register char *loc;
	register mp_off_ty lastoff = (mp_off_ty)(-1);

	rt_log( "%s\n", title );
	for( mp = parsetab; mp->mp_name != (char *)0; mp++ )  {

		/* Skip alternate keywords for same value */
		if( lastoff == mp->mp_offset )
			continue;
		lastoff = mp->mp_offset;

		loc = (char *)(((mp_off_ty)base) +
				((int)mp->mp_offset));

		switch( mp->mp_fmt[1] )  {
		case 's':
			rt_log( " %s=%s\n", mp->mp_name, (char *)loc );
			break;
		case 'd':
			rt_log( " %s=%d\n", mp->mp_name,
				*((int *)loc) );
			break;
		case 'f':
			rt_log( " %s=%g\n", mp->mp_name,
				*((double *)loc) );
			break;
		case 'C':
			{
				register unsigned char *cp =
					(unsigned char *)loc;
				rt_log(" %s=%d/%d/%d(%d)\n", mp->mp_name,
					cp[0], cp[1], cp[2], cp[3] );
				break;
			}
		default:
			rt_log( " %s=%s??\n", mp->mp_name,
				mp->mp_fmt );
			break;
		}
	}
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
