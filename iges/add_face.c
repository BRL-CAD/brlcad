/*
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"
#include "./iges_struct.h"
#include "./iges_extern.h"

RT_EXTERN( struct faceuse *Make_face , ( struct shell *s , int entityno , int face_orient ) );
RT_EXTERN( int Add_loop_to_face , (struct shell *s , struct faceuse *fu , int entityno , int face_orient ));
RT_EXTERN( int planar_nurb , ( int entityno ) );

struct faceuse *
Add_face_to_shell( s , entityno , face_orient )
struct shell *s;
int entityno;
int face_orient;
{ 

	int		sol_num;		/* IGES solid type number */
	int		surf_de;		/* Directory sequence number for underlying surface */
	int		no_of_loops;		/* Number of loops in face */
	int		outer_loop_flag;	/* Indicates if first loop is an the outer loop */
	int		*loop_de;		/* Directory seqence numbers for loops */
	int		loop;
	int		planar=0;
	struct faceuse	*fu;			/* NMG face use */

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return( (struct faceuse *)NULL );
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &surf_de , "" );
	Readint( &no_of_loops , "" );
	Readint( &outer_loop_flag , "" );
	loop_de = (int *)rt_calloc( no_of_loops , sizeof( int ) , "Get_outer_face loop DE's" );
	for( loop=0 ; loop<no_of_loops ; loop++ )
		Readint( &loop_de[loop] , "" );

	/* Check that this is a planar surface */
	if( dir[(surf_de-1)/2]->type == 190 ) /* plane entity */
		planar = 1;
	else if( dir[(surf_de-1)/2]->type == 128 )
	{
		/* This is a NURB, but it might still be planar */
		if( dir[(surf_de-1)/2]->form == 1 ) /* planar form */
			planar = 1;
		else if( dir[(surf_de-1)/2]->form == 0 )
		{
			/* They don't want to tell us */
			if( planar_nurb( (surf_de-1)/2 ) )
				planar = 1;
		}
	}

	if( !planar )
	{
		rt_log( "Face entity at DE=%d is not planar, object not converted\n" , (entityno*2+1) );
		return( (struct faceuse *)NULL );
	}

	fu = Make_face( s , (loop_de[0]-1)/2 , face_orient );
	for( loop=1 ; loop<no_of_loops ; loop++ )
	{
		if( !Add_loop_to_face( s , fu , ((loop_de[loop]-1)/2) , face_orient ))
			goto err;
	}


  err :
	rt_free( (char *)loop_de , "Add_face_to_shell: loop DE's" );
	return( fu );
}
