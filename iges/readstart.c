/*
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */

/*		Read and print Start Section		*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readstart()
{

	int i=0,done=0;

	while( !done )
	{
		if( Readrec( ++i ) )
		{
			bu_log( "End of file encountered\n" );
			exit( 1 );
		}

		if( card[72] != 'S' )
		{
			done = 1;
			break;
		}
		card[72] = '\0';
		bu_log( "%s\n" , card );
	}
	bu_log( "%c", '\n' );
}

