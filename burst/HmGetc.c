/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$ (BRL)
*/
#if ! defined( lint )
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <signal.h>
#include "./burst.h"
#include "./Hm.h"
/*LINTLIBRARY*/
int
#if __STDC__
HmGetchar( void )
#else
HmGetchar()
#endif
	{	int	c;
	while( (c = getc( HmTtyFp )) == EOF )
		;
	return	c;
	}

int
#if __STDC__
HmUngetchar( int c )
#else
HmUngetchar( c )
#endif
	{
	return	ungetc( c, HmTtyFp );
	}
