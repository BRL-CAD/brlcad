/*
 *			S T R C H R
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

char *
strchr(register char *sp, register char c)
{
	do {
		if( *sp == c )
			return( sp );
	}  while( *sp++ );
	return( (char *)0 );
}
