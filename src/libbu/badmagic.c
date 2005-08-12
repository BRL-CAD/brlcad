/*                      B A D M A G I C . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbu */
/*@{*/
/** @file badmagic.c
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
 */
/*@}*/

#ifndef lint
static const char RCSbadmagic[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
