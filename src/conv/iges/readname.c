/*                      R E A D N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
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
/** @file readname.c
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
	It expects the field to contain a character string
	of the form "nHstring" where n is the length
	of "string". If "id" is not the null string, then
	"id" is printed followed by "string".  A pointer
	to the string is returned in "ptr"

	"eof" is the "end-of-field" delimiter
	"eor" is the "end-of-record" delimiter	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readname( ptr , id )
char *id,**ptr;
{
	int i=(-1),length=0,done=0,lencard;
	char num[80],*ch;


	if( card[counter] == eof ) /* This is an empty field */
	{
		*ptr = (char *)NULL;
		counter++;
		return;
	}
	else if( card[counter] == eor ) /* Up against the end of record */
	{
		*ptr = (char *)NULL;
		return;
	}

	if( card[72] == 'P' )
		lencard = PARAMLEN;
	else
		lencard = CARDLEN;

	if( counter > lencard )
		Readrec( ++currec );

	if( *id != '\0' )
		bu_log( "%s" , id );

	while( !done )
	{
		while( (num[++i] = card[counter++]) != 'H' &&
				counter <= lencard);
		if( counter > lencard )
			Readrec( ++currec );
		else
			done = 1;
	}
	num[++i] = '\0';
	length = atoi( num );
	*ptr = (char *)bu_malloc( (length + 1)*sizeof( char ) , "Readname: name" );
	ch = *ptr;
	for( i=0 ; i<length ; i++ )
	{
		if( counter > lencard )
			Readrec( ++currec );
		ch[i] = card[counter++];
		if( *id != '\0' )
			bu_log( "%c", ch[i] );
	}
	ch[length] = '\0';
	if( *id != '\0' )
		bu_log( "%c", '\n' );

	done = 0;
	while( !done )
	{
		while( card[counter++] != eof && card[counter] != eor &&
			counter <= lencard );
		if( counter > lencard && card[counter] != eor && card[counter] != eof )
			Readrec( ++currec );
		else
			done = 1;
	}

	if( card[counter-1] == eor )
		counter--;
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
