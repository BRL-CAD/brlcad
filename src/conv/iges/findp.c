/*                         F I N D P . C
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
/** @file findp.c
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

/* This routine reads the last record in the IGES file.
	That record contains the nunber of records in each section.
	These numbers are used to calculate the starting record number
	for the parameter section and the directory section.
	space is then reserved for the directory.  This routine depends on
	"fseek" and "ftell" operating with offsets given in bytes.	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

int
Findp()
{
	int saverec,rec2,i;
	long offset;
	char str[8];

	str[7] = '\0';


	saverec = currec;	/* save current record number */

	if( fseek( fd , 0L , 2 ) )	/* go to end of file */
	{
		bu_log( "Cannot seek to end of file\n" );
		perror( "Findp" );
		exit( 1 );
	}
	offset = ftell( fd );	/* get file length */
	rec2 = offset/reclen;	/* calculate record number for last record */
	Readrec( rec2 );	/* read last record into "card" buffer */
	dstart = 0;
	pstart = 0;
	for( i=0 ; i<3 ; i++ )
	{
		counter++;	/* skip the single letter section ID */
		Readcols( str , 7 );	/* read the number of records in the section */
		pstart += atoi( str );	/* increment pstart */
		if( i == 1 )	/* Global section */
		{
			/* set record number for start of directory section */
			dstart = pstart;
		}
	}

	/* restore current record */
	currec = saverec;
	Readrec( currec );

	/* make space for directory entries */
	totentities = (pstart - dstart)/2;
	if( totentities > 0 )
	{
		dir = (struct iges_directory **)bu_calloc( totentities ,
			sizeof( struct iges_directory *),
			"IGES directory*" );

		for( i=0 ; i<totentities ; i++ )
		{
			dir[i] = (struct iges_directory *)bu_malloc( sizeof( struct iges_directory ) , "IGES directory" );
			dir[i]->name = (char *)NULL;
			dir[i]->trans = (-1);
		}
	}
	else
		totentities = 0;

	dirarraylen = totentities;

	return( pstart );
}

void
Free_dir()
{
	int i;

	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 124 || dir[i]->type == 700 )
			bu_free( (char *)dir[i]->rot, "Free_dir: dir[i]->rot" );
		bu_free( (char *)dir[i], "Free_dir: dir[i]" );
	}

	if( totentities > 0 )
		bu_free( (char *)dir, "Free_dir: dir" );
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
