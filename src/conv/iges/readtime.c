/*                      R E A D T I M E . C
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
/** @file readtime.c
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
	It expects the field to contain a string of the form:
		13HYYMMDD.HHNNSS or 15YYYYMMDD.HHNNSS
		where:
			YY is the year (last 2 digits) or YYYY is the year (all 4 digits)
			MM is the month (01 - 12)
			DD is the day (01 - 31)
			HH is the hour (00 -23 )
			NN is the minute (00 - 59)
			SS is the second (00 - 59)
	The string is read and printed out in the form:
		/MM/DD/YYYY at HH:NN:SS

	"eof" is the "end-of-field" delimiter
	"eor" is the "end-of-record" delimiter	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readtime( id )
char *id;
{
	int i=(-1),length=0,lencard,done=0,year;
	char num[80];
	char year_str[5];

	if( card[counter] == eof ) /* This is an empty field */
	{
		counter++;
		return;
	}
	else if( card[counter] == eor ) /* Up against the end of record */
		return;

	if( *id != '\0' )
		bu_log( "%s" , id );

	if( card[72] == 'P' )
		lencard = PARAMLEN;
	else
		lencard = CARDLEN;

	if( counter > lencard )
		Readrec( ++currec );

	while( !done )
	{
		while( (counter <= lencard) &&
			((num[++i] = card[counter++]) != 'H'));
		if( counter > lencard )
			Readrec( ++currec );
		else
			done = 1;
	}

	num[++i] = '\0';
	length = atoi( num );
	if( length != 13 && length != 15 )
	{
		bu_log( "\tError in time stamp\n" );
		bu_log( "\tlength of string=%s (should be 13 or 15)\n" , num );
	}

	for( i=0 ; i<length ; i++ )
	{
		if( counter > lencard )
			Readrec( ++currec );
		num[i] = card[counter++];
	}

	year_str[0] = num[0];
	year_str[1] = num[1];
	if( length == 13 )
	{
		year_str[2] = '\0';
		year = atoi( year_str );
		year += 1900;
		bu_log( "%c%c/%c%c/%d" , num[2],num[3],num[4],num[5],
			year );
		if( length > 12 && length < 16 )
		bu_log( " at %c%c:%c%c:%c%c\n" , num[7],num[8],num[9],
			num[10],num[11],num[12] );

	}
	else
	{
		year_str[2] = num[2];
		year_str[3] = num[3];
		year_str[4] = '\0';
		year = atoi( year_str );
		bu_log( "%c%c/%c%c/%d" , num[4],num[5],num[6],num[7],
			year );
		bu_log( " at %c%c:%c%c:%c%c\n" , num[9],num[10],num[11],
			num[12],num[13],num[14] );
	}

	while( card[counter] != eof && card[counter] != eor )
	{
		if( counter < lencard )
			counter++;
		else
			Readrec( ++currec );
	}

	if( card[counter] == eof )
	{
		counter++;
		if( counter > lencard )
			Readrec( ++ currec );
	}
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
