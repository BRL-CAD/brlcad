/*
 *			B A D M A G I C . C
 *
 *  Routines involved with handling "magic numbers" used to identify
 *  various in-memory data structures.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSbadmagic[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"

/*
 *			B U _ B A D M A G I C
 *
 *  Support routine for BU_CKMAG macro
 */
void
bu_badmagic( ptr, magic, str, file, line )
CONST long	*ptr;
long		magic;
CONST char	*str;
CONST char	*file;
int		line;
{
	char	buf[512];

	if( !(ptr) )  { 
		sprintf(buf, "ERROR: NULL %s pointer, file %s, line %d\n", 
			str, file, line ); 
		bu_bomb(buf); 
	}
	if( *((long *)(ptr)) != (magic) )  { 
		sprintf(buf, "ERROR: bad pointer x%x: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n", 
			ptr,
			str, magic,
			bu_identify_magic( *(ptr) ), *(ptr),
			file, line ); 
		bu_bomb(buf); 
	}
}
