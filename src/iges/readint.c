/*                       R E A D I N T . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file readint.c
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
 */

/* This routine reads the next field in "card" buffer
	It expects the field to contain a string of integers.
	The string of integers is read and converted to type
	"int" and returned in "inum".
	If "id" is not the null string, then
	"id" is printed followed by the number.

	"eof" is the "end-of-field" delimiter
	"eor" is the "end-of-record" delimiter	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readint( inum , id )
char *id;
int *inum;
{
	int i=(-1),done=0,lencard;
	char num[80];

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
		while( (num[++i] = card[counter++]) != eof && num[i] != eor &&
			counter <= lencard );
		if( counter > lencard && num[i] != eor && num[i] != eof )
			Readrec( ++currec );
		else
			done = 1;
	}

	if( num[i] == eor )
		counter--;

	num[++i] = '\0';
	*inum = atoi( num );
	if( *id != '\0' )
		bu_log( "%s%d\n" , id , *inum );
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
