/*                         M A T C H . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file match.c
 *
 *  Author:		Gary S. Moss
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <setjmp.h>


#define NUL	'\0'

/*	m a t c h ( )
	if string matches pattern, return 1, else return 0
	special characters:
	*	Matches any string including the null string.
	?	Matches any single character.
	[...]	Matches any one of the characters enclosed.
	[!..]	Matches any character NOT enclosed.
	-	May be used inside brackets to specify range
	(i.e. str[1-58] matches str1, str2, ... str5, str8)
	\	Escapes special characters.
*/
int
match(char *pattern, char *string)
{
    do
    {
	switch ( pattern[0] )
	{
	    case '*': /* Match any string including null string.	*/
		if ( pattern[1] == NUL || string[0] == NUL )
		    return	1;
		while ( string[0] != NUL )
		{
		    if ( match( &pattern[1], string ) )
			return	1;
		    ++string;
		}
		return	0;
	    case '?': /* Match any character.			*/
		break;
	    case '[': /* Match one of the characters in brackets
			 unless first is a '!', then match
			 any character not inside brackets.
		      */
	    {
		char	*rgtBracket;
		static int	negation;

		++pattern; /* Skip over left bracket.		*/
		/* Find matching right bracket.			*/
		if ( (rgtBracket = strchr( pattern, ']' )) == NULL )
		{
		    (void) fprintf( stderr, "Unmatched '['." );
		    return	0;
		}
		/* Check for negation operator.			*/
		if ( pattern[0] == '!' )
		{
		    ++pattern;
		    negation = 1;
		}
		else	{
		    negation = 0;
		}
		/* Traverse pattern inside brackets.		*/
		for (	;
			pattern < rgtBracket
			    &&	pattern[0] != string[0];
			++pattern
		    )
		{
		    if (	pattern[ 0] == '-'
				&&	pattern[-1] != '\\'
			)
		    {
			if (	pattern[-1] <= string[0]
				&&	pattern[-1] != '['
				&&	pattern[ 1] >= string[0]
				&&	pattern[ 1] != ']'
			    )
			    break;
		    }
		}
		if ( pattern == rgtBracket )
		{
		    if ( ! negation )
		    {
			return	0;
		    }
		}
		else
		{
		    if ( negation )
		    {
			return	0;
		    }
		}
		pattern = rgtBracket; /* Skip to right bracket.	*/
		break;
	    }
	    case '\\': /* Escape special character.			*/
		++pattern;
		/* WARNING: falls through to default case.	*/
	    default:  /* Compare characters.			*/
		if ( pattern[0] != string[0] )
		    return	0;
	}
	++pattern;
	++string;
    }
    while ( pattern[0] != NUL && string[0]  != NUL );
    if ( (pattern[0] == NUL || pattern[0] == '*' ) && string[0]  == NUL )
    {
	return	1;
    }
    else
    {
	return	0;
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
