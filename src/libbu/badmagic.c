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
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSbadmagic[] = "@(#)$Header$ (ARL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include "machine.h"
#include "bu.h"

/*
 *			B U _ B A D M A G I C
 *
 *  Support routine for BU_CKMAG macro
 */
void
bu_badmagic(const long int *ptr, unsigned long int magic, const char *str, const char *file, int line)
{
	char	buf[512];

	if( !(ptr) )  { 
		sprintf(buf, "ERROR: NULL %s pointer, file %s, line %d\n", 
			str, file, line ); 
		bu_bomb(buf); 
	}
	if( ((size_t)(ptr)) & (sizeof(long)-1) )  {
		sprintf(buf, "ERROR: x%lx mis-aligned %s pointer, file %s, line %d\n", 
			(long)ptr, str, file, line ); 
		bu_bomb(buf); 
	}
	if( *(ptr) != (long int)(magic) )  { 
		sprintf(buf, "ERROR: bad pointer x%lx: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n", 
			(long)ptr,
			str, magic,
			bu_identify_magic( *(ptr) ), *(ptr),
			file, line ); 
		bu_bomb(buf); 
	}
}
