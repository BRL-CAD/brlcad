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

/* Routine to read the directory section of an IGES file.
	and store needed info in the 'directory' structures.
	dir[n] is the structure for entity #n.
	The directory entry for entity #n is located on
	line D'2n+1' of the iges file.	*/

#include <stdio.h>
#include <math.h>
#include "./iges_struct.h"
#include "./iges_extern.h"

#define	CR	'\015'

Makedir()
{
	
	int found,i,saverec,entcount=(-1),paramptr=0,paramguess=0;
	char str[9];
	
	str[8] = '\0';
	printf( "Reading Directory Section...\n" );
	printf( "Number of entities checked:\n" );
	Readrec( dstart+1 );	/* read first record in directory section */
	
	while( 1 )
	{
		if( card[72] != 'D' )	/* We are not in the directory section */
			break;
		
		entcount++;	/* increment count of entities */

		if( entcount%20 == 0 )
		{
			sprintf( str , "\t%d%c" , entcount , CR );
			write( 1 , str , strlen( str ) );
		}

		/* save the directory record number for this entity */
		dir[entcount]->direct = atoi( &card[73] );

		/* set reference count to 0 */
		dir[entcount]->referenced = 0;

		/* set record number to read for next entity */
		saverec = currec + 2;

		Readcols( str , 8 );	/* read entity type */
		dir[entcount]->type = atoi( str );
		switch( dir[entcount]->type )
		{
			case 150:
				sprintf( dir[entcount]->name , "block.%d" , entcount );
				break;
			case 152:
				sprintf( dir[entcount]->name , "wedge.%d" , entcount );
				break;
			case 154:
				sprintf( dir[entcount]->name , "cyl.%d" , entcount );
				break;
			case 156:
				sprintf( dir[entcount]->name , "cone.%d" , entcount );
				break;
			case 158:
				sprintf( dir[entcount]->name , "sphere.%d" , entcount );
				break;
			case 160:
				sprintf( dir[entcount]->name , "torus.%d" , entcount );
				break;
			case 162:
				sprintf( dir[entcount]->name , "revolution.%d" , entcount );
				break;
			case 164:
				sprintf( dir[entcount]->name , "extrusion.%d" , entcount );
				break;
			case 168:
				sprintf( dir[entcount]->name , "ell.%d" , entcount );
				break;
			case 180:
				sprintf( dir[entcount]->name , "region.%d" , entcount );
				break;
			case 184:
				sprintf( dir[entcount]->name , "group.%d" , entcount );
				break;
			case 430:
				sprintf( dir[entcount]->name , "inst.%d" , entcount );
				break;
			case 128:
				sprintf( dir[entcount]->name , "nurb.%d" , entcount );
				break;
			default:
				sprintf( dir[entcount]->name , "entity%d" , entcount );
				break;
		}

		Readcols( str , 8 );	/* read pointer to parameter entry */

		/* convert it to a file record number */
		paramptr = atoi( str );
		if( paramptr == 0 && entcount > 0 )
		{
			paramguess = 1;
			dir[entcount]->param = dir[entcount-1]->param + dir[entcount-1]->paramlines;
		}
		else if( paramptr > 0 )
			dir[entcount]->param = paramptr + pstart;
		else
			fprintf( stderr , "Entity number %d does not have a correct parameter pointer\n",
				entcount );

		counter = counter + 32;	/* skip 32 columns */
		Readcols( str , 8 );	/* read pointer to transformation entity */

		/* convert it to a "dir" index */
		dir[entcount]->trans = (atoi( str ) - 1)/2;

		Readrec( currec + 1 );	/* read next record into buffer */
		counter += 16;		/* skip first two fields */

		Readcols( str , 8 );	/* read pointer to color entity */
		/* if pointer is negative, convert to a 'dir' index */
		dir[entcount]->colorp = atoi( str );
		if( dir[entcount]->colorp < 0 )
			dir[entcount]->colorp = (dir[entcount]->colorp + 1)/2;

		Readcols( str , 8 );	/* read parameter line count */
		dir[entcount]->paramlines = atoi( str );
		if( dir[entcount]->paramlines == 0 )
			dir[entcount]->paramlines = 1;
		Readcols( str , 8 );	/* read form number */
		dir[entcount]->form = atoi( str );
		
		/* Look for entity type in list and incrememt that count */
		
		found = 0;
		for( i=0 ; i<ntypes ; i++ )
		{
			if( typecount[i][0] == dir[entcount]->type )
			{
				typecount[i][1]++;
				found = 1;
				break;
			}
		}
		if( !found )
			typecount[ntypes][1]++;

		/* Check if this is a transformation entity */
		if( dir[entcount]->type == 124 || dir[entcount]->type == 700 )
		{
			/* Read and store the matrix */
			if( dir[entcount]->param <= pstart )
			{
				printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
						dir[entcount]->direct , dir[entcount]->name );
				dir[entcount]->rot = NULL;
			}
			else
			{
				dir[entcount]->rot = (mat_t *)malloc( sizeof( mat_t ) );
				Readmatrix( dir[entcount]->param , *dir[entcount]->rot );
			}
		}
		else /* set to NULL */
			dir[entcount]->rot = NULL;

	Readrec( saverec ); /* restore previous record */
	}

printf( "\t%d\n\n" ,entcount+1 );
if( paramguess )
	printf( "Some entities did not have proper parameter pointers, so a resonable guess was made\n" );
}

