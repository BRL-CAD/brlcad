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
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "material.h"

extern int phong_setup();
extern int txt_setup();
extern int tmap_setup();
extern int cloud_setup();
extern int mirror_setup();
extern int glass_setup();
extern int ckr_setup();

struct matlib {
	char	*ml_name;
	int	(*ml_setup)();
} matlib[] = {
	"plastic",	phong_setup,
	"texture",	txt_setup,
	"testmap",	tmap_setup,
	"cloud",	cloud_setup,
	"mirror",	mirror_setup,
	"glass",	glass_setup,
	"checker",	ckr_setup,
	(char *)0,	0			/* END */
};

/*
 *			M L I B _ S E T U P
 *
 *  Returns -
 *	0	failed
 *	!0	success
 */
int
mlib_setup( rp )
register struct region *rp;
{
	register struct matlib *mlp;

	if( rp->reg_ufunc )  {
		rt_log("mlib_setup:  region %s already setup\n", rp->reg_name );
		return(0);
	}
	if( rp->reg_mater.ma_matname[0] == '\0' )
		goto def;
	for( mlp=matlib; mlp->ml_name != (char *)0; mlp++ )  {
		if( rp->reg_mater.ma_matname[0] != mlp->ml_name[0]  ||
		    strcmp( rp->reg_mater.ma_matname, mlp->ml_name ) != 0 )
			continue;
		return( mlp->ml_setup( rp ) );
	}
	rt_log("mlib_setup(%s):  material not known, default assumed\n",
		rp->reg_mater.ma_matname );
def:
	return( phong_setup( rp ) );
}

/*
 *			M L I B _ P A R S E
 */
mlib_parse( cp, parsetab, base )
register char *cp;
struct matparse *parsetab;
char *base;		/* base address of users structure */
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
			if( strcmp( mp->mp_name, name ) == 0 )  {
				(void)sscanf( value,
					mp->mp_fmt, 
					base + mp->mp_offset );
				goto out;
			}
		}
		rt_log("mlib_parse:  %s=%s not a valid arg\n", name, value);
out:		;
	}
}

/*
 *			M L I B _ P R I N T
 */
mlib_print( title, parsetab, base )
char *title;
struct matparse *parsetab;
char *base;		/* base address of users structure */
{
	register struct matparse *mp;

	rt_log( "%s\n", title );
	for( mp = parsetab; mp->mp_name != (char *)0; mp++ )  {
		switch( mp->mp_fmt[1] )  {
		case 's':
			rt_log( " %s=%s\n", mp->mp_name,
				base + mp->mp_offset );
			break;
		case 'd':
			rt_log( " %s=%d\n", mp->mp_name,
				*((int *)(base + mp->mp_offset)) );
			break;
		case 'f':
			rt_log( " %s=%f\n", mp->mp_name,
				*((double *)(base + mp->mp_offset)) );
			break;
		default:
			rt_log( " %s=%s??\n", mp->mp_name,
				mp->mp_fmt );
			break;
		}
	}
}
