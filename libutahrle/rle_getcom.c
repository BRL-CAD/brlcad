/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * rle_getcom.c - Get specific comments from the_hdr structure.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sun Jan 25 1987
 * Copyright (c) 1987, University of Utah
 */

#include <stdio.h>
#include "rle.h"

/*****************************************************************
 * TAG( match )
 * 
 * Match a name against a test string for "name=value" or "name".
 * If it matches name=value, return pointer to value part, if just
 * name, return pointer to NUL at end of string.  If no match, return NULL.
 *
 * Inputs:
 * 	n:	Name to match.  May also be "name=value" to make it easier
 *		to replace comments.
 *	v:	Test string.
 * Outputs:
 * 	Returns pointer as above.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
static char *
match( n, v )
register char *n;
register char *v;
{
    for ( ; *n != '\0' && *n != '=' && *n == *v; n++, v++ )
	;
    if (*n == '\0' || *n == '=')
	if ( *v == '\0' )
	    return v;
	else if ( *v == '=' )
	    return ++v;

    return NULL;
}

/*****************************************************************
 * TAG( rle_getcom )
 * 
 * Return a pointer to the value part of a name=value pair in the comments.
 * Inputs:
 * 	name:		Name part of the comment to search for.
 *	the_hdr:	rle_dflt_hdr structure.
 * Outputs:
 * 	Returns pointer to value part of comment or NULL if no match.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
char *
rle_getcom( name, the_hdr )
CONST_DECL char *name;
rle_hdr *the_hdr;
{
    CONST_DECL char ** cp;
    char * v;

    if ( the_hdr->comments == NULL )
	return NULL;

    for ( cp = the_hdr->comments; *cp; cp++ )
	if ( (v = match( name, *cp )) != NULL )
	    return v;

    return NULL;
}

