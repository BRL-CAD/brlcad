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

/* This routine reads the next field in "card" buffer
	It expects the field to contain a string representing a "double"
	The string is read and converted to type
	"double" and returned in "inum".
	If "id" is not the null string, then
	"id" is printed followed by the number.

	"eof" is the "end-of-field" delimiter
	"eor" is the "end-of-record" delimiter	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readdbl( inum , id )
char *id;
double *inum;
{
	int i=(-1),done=0,lencard;
	char num[80];
	double atof();

	if( card[counter] == eof ) /* This is an empty field */
	{
		counter++;
		return;
	}
	else if( card[counter] == eor ) /* Up against the end of record */
		return;

	if( card[72] == 'P' )
		lencard = PARAMLEN;
	else
		lencard = CARDLEN;

	if( counter >= lencard )
		Readrec( ++currec );

	while( !done )
	{
		while( (num[++i] = card[counter++]) != eof && num[i] != eor
			&& counter <= lencard )
				if( num[i] == 'D' )
					num[i] = 'e';

		if( counter > lencard && num[i] != eor && num[i] != eof )
			Readrec( ++currec );
		else
			done = 1;
	}

	if( num[i] == eor )
		counter--;

	num[++i] = '\0';
	*inum = atof( num );
	if( *id != '\0' )
		bu_log( "%s%g\n" , id , *inum );
}

