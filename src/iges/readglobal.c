/*                    R E A D G L O B A L . C
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
/** @file readglobal.c
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

/*	Read Global Section 	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

/* Conversion Factors (to mm)	*/
fastf_t cnv[]={
/*	default,inch, mm, ? , feet,  miles  ,meters,kilometers,mils  ,microns,
		cm ,microinches	*/
	1.0,    25.4,1.0,1.0,304.8,1609344.0,1000.0, 1000000.0,0.0254,0.001,
		10.0,0.0000254	};

/* IGES Version */
#define NO_OF_VERSIONS	10
char *iges_version[NO_OF_VERSIONS]={
	" ",
	"1.0",
	"ANSI Y14.26M - 1981",
	"2.0",
	"3.0",
	"ANSI Y14.26M - 1987",
	"4.0",
	"ASME Y14.26M - 1989",
	"5.0",
	"5.1" };

void
Readglobal( file_count )
int file_count;
{

	int field=2,i;
	fastf_t a;
	char *name;


	/* Get End-of-field delimiter */
	if( card[counter] != ',' )
	{
		counter--;
		while( card[++counter] == ' ' );
		if( card[counter] != '1' || card[counter+1] != 'H' )
		{
			bu_log( "Error in new delimiter\n" );
			bu_log( "%s\n" , card );
			for( i=0 ; i<counter-1 ; i++ )
				bu_log( "%c", ' ' );
			bu_log( "^\n" );
			exit( 1 );
		}
		counter++;
		eof = card[++counter];
		while( card[++counter] != eof );
	}
	else
		eof = ',';


	/* Get End-of-record delimiter */
	if( card[++counter] != eof )
	{
		counter--;
		while( card[++counter] == ' ' );
		if( card[counter] != '1' || card[counter+1] != 'H' )
		{
			bu_log( "Error in new record delimiter\n" );
			exit( 1 );
		}
		counter++;
		eor = card[++counter];
		while( card[++counter] != eof );
	}
	else
		eor = ';';


	/* Read all the fields in the Global Section */
	counter++;
	while( field < 23 )
	{
	   if( card[counter-1] == eor )
	   {
		Readrec( ++currec );
		break;
	   }

	   switch( ++field )
		{
		case 3:		Readname( &name , "Product ID: ");
				if( !file_count )
				{
					if( name != NULL )
					{
						mk_id( fdout , name );
						bu_free( name, "Readglobal: name" );
					}
					else
						mk_id( fdout , "Un-named Product" );
				}
				break;
		case 4:		Readstrg( "File Name: " );
				break;
		case 5:		Readstrg( "System ID: " );
				break;
		case 6:		Readstrg( "Version: " );
				break;
		case 7:		Readint( &i , "Integer Bits: ");
				break;
		case 8:		Readint( &i , "Max Power of ten(single precision): " );
				break;
		case 9:		Readint( &i , "Max significant digits (single precision): " );
				break;
		case 10:	Readint( &i , "Max Power of ten(double precision): " );
				break;
		case 11:	Readint( &i , "Max significant digits (single precision): " );
				break;
		case 12:	Readstrg( "Product ID: ");
				break;
		case 13:	Readflt( &scale , "Scale: " );
				if( scale == 0.0 )
					scale = 1.0;
				inv_scale = 1.0/scale;
				break;
		case 14:	Readint( &units , "Units: " );
				if( units == 0 || units == 3 || units > 11 )
				{
					bu_log( "Unrecognized units, assuming 'mm'\n" );
					conv_factor = 1.0;
				}
				else
					conv_factor = cnv[units];
				/* make "conv_factor" account for both units and
					scale factor */
				conv_factor *= inv_scale;
				break;
		case 15:	Readstrg( "Units: " );
				break;
		case 16:	Readint( &i , "Line Weight Gradations: " );
				break;
		case 17:	Readflt( &a , "Line Width: " );
				break;
		case 18:	Readtime( "Exchange File Created On: " );
				break;
		case 19:	Readflt( &a , "Resolution: " );
				break;
		case 20:	Readflt( &a , "Maximum value: " );
				break;
		case 21:	Readstrg( "Author: " );
				break;
		case 22:	Readstrg( "Organization: " );
				break;
		case 23:	Readint( &i , "" );
				if( i<1 || i>=NO_OF_VERSIONS )
					bu_log( "Unrecognized IGES version\n" );
				else
					bu_log( "IGES version: %s\n" , iges_version[i] );
				break;
		case 24:	Readint( &i , "" );
				break;
		case 25:	Readtime( "Model Last Modified: " );
				break;
		}
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
