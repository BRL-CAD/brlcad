/*
 *			D B _ M A T C H . C
 *
 * Functions -
 *	db_regexp_match		Determine if a string matches a regexp pattern
 *	db_regexp_match_all	Return a vls filled with all names matching
 *				the given pattern
 *
 *  Author -
 *	Michael John Muuss
 *	Glenn Durfee
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"

/*
 *			D B _ R E G E X P _ M A T C H
 *
 *	If string matches pattern, return 1, else return 0
 *
 *	special characters:
 *		*	Matches any string including the null string.
 *		?	Matches any single character.
 *		[...]	Matches any one of the characters enclosed.
 *		-	May be used inside brackets to specify range
 *			(i.e. str[1-58] matches str1, str2, ... str5, str8)
 *		\	Escapes special characters.
 */
int
db_regexp_match( pattern, string )
register char *pattern, *string;
{
	do {
		switch( *pattern ) {
		case '*':
			/* match any string including null string */
			++pattern;
			do {
				if( db_regexp_match( pattern, string ) )
					return( 1 );
			} while( *string++ != '\0' );
			return( 0 );
		case '?':
			/* match any character  */
			if( *string == '\0' )
				return( 0 );
			break;
		case '[':
			/* try to match one of the characters in brackets */
			++pattern;
			if( *pattern == '\0' )
				return( 0 );
			while( *pattern != *string ) {
				if( pattern[0] == '-' && pattern[-1] != '\\')
					if(	pattern[-1] <= *string &&
						pattern[-1] != '[' &&
						pattern[ 1] >= *string &&
						pattern[ 1] != ']' )
						break;
				++pattern;
				if( *pattern == '\0' || *pattern == ']' )
					return( 0 );
			}
			/* skip to next character after closing bracket */
			while( *pattern != '\0' && *pattern != ']' )
				++pattern;
			break;
		case '\\':
			/* escape special character */
			++pattern;
			/* compare characters */
			if( *pattern != *string )
				return( 0 );
			break;
		default:
			/* compare characters */
			if( *pattern != *string )
				return( 0 );
		}
		++string;
	} while( *pattern++ != '\0' );
	return( 1 );
}



/*
 *			D B _ R E G E X P _ M A T C H _ A L L
 *
 * Appends a list of all database matches to the given vls, or the pattern
 * itself if no matches are found.
 * Returns the number of matches.
 *
 * XXX need to extern this guy for mged
 */
 
int
db_regexp_match_all( dest, dbip, pattern )
struct bu_vls	*dest;
struct db_i	*dbip;
char		*pattern;
{
	register int i, num;
	register struct directory *dp;

	for( i = num = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw ){
			if( !db_regexp_match( pattern, dp->d_namep ) )
				continue;
			if( num == 0 )
				bu_vls_strcat( dest, dp->d_namep );
			else {
				bu_vls_strcat( dest, " " );
				bu_vls_strcat( dest, dp->d_namep );
			}
			++num;
		}
	}

	return num;
}
