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
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

/* This routine reads the next field in "card" buffer
	It expects the field to contain a string of the form:
		13HYYMMDD.HHNNSS
		where:
			YY is the year (last 2 digits)
			MM is the month (01 - 12)
			DD is the day (01 - 31)
			HH is the hour (00 -23 )
			NN is the minute (00 - 59)
			SS is the second (00 - 59)
	The string is read and printed out in the form:
		/MM/DD/YYYY at HH:NN:SS

	If the year is less than 50, 2000 is added to it, otherwise 1900
	is added (Y2K compliancy??)

	"eof" is the "end-of-field" delimiter
	"eor" is the "end-of-record" delimiter	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readtime( id )
char *id;
{
	int i=(-1),length=0,lencard,done=0,year;
	char num[80],year_str[3];

	if( card[counter] == eof ) /* This is an empty field */
	{
		counter++;
		return;
	}
	else if( card[counter] == eor ) /* Up against the end of record */
		return;

	if( *id != '\0' )
		rt_log( "%s" , id );

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
	if( length != 13 )
	{
		rt_log( "\tError in time stamp\n" );
		rt_log( "\tlength of string=%s (should be 13)\n" , num );
	}

	for( i=0 ; i<length ; i++ )
	{
		if( counter > lencard )
			Readrec( ++currec );
		num[i] = card[counter++];
	}

	if( length > 5 && length < 16 )
	{
		char year_str[3];
		int year;

		year_str[0] = num[0];
		year_str[1] = num[1];
		year_str[2] = '\0';
		year = atoi( year_str );
		if( year < 50 )
			year += 2000;
		else
			year += 1900;
		rt_log( "%c%c/%c%c/%d" , num[2],num[3],num[4],num[5],
			year );
	}

	if( length > 12 && length < 16 )
		rt_log( " at %c%c:%c%c:%c%c\n" , num[7],num[8],num[9],
			num[10],num[11],num[12] );

	if( length > 15 )
		rt_log( "%s\n" , num );

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
