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

/* This routine reads a specific number of characters from the "card"
	buffer.  The number is "cols".  The string of characters read
	is pointed to by "id". 	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readcols( id , cols )
char *id;
int cols;
{
	int i;
	char *tmp;

	tmp = id;

	for( i=0 ; i<cols ; i++ )
		*tmp++ = card[counter++];
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
