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
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

RT_EXTERN( struct faceuse *Make_face , ( struct shell *s , int entityno , int loop_orient) );

struct faceuse *
Add_face_to_shell( s , entityno , shell_orient )
struct shell *s;
int entityno;
int shell_orient;
{ 

	int		sol_num;		/* IGES solid type number */
	int		surf_de;		/* Directory sequence number for underlying surface */
	int		no_of_loops;		/* Number of loops in face */
	int		outer_loop_flag;	/* Indicates if first loop is an the outer loop */
	int		*loop_de;		/* Directory seqence numbers for loops */
	int		*loop_orient;		/* Orientation of loops */
	int		loop;
	struct faceuse	*fu;			/* NMG face use */

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &surf_de , "" );

	/* Check that this is a planar surface */
	if( dir[(surf_de-1)/2]->type  != 190 )
	{
		rt_log( "Face entity at DE=%d is not planar, object not converted\n" , (entityno*2+1) );
		return( 0 );
	}

	Readint( &no_of_loops , "" );
	Readint( &outer_loop_flag , "" );
	loop_de = (int *)rt_calloc( no_of_loops , sizeof( int ) , "Get_outer_face loop DE's" );
	loop_orient = (int *)rt_calloc( no_of_loops , sizeof( int ) , "Get_outer_face orients" );
	for( loop=0 ; loop<no_of_loops ; loop++ )
	{
		Readint( &loop_de[loop] , "" );
		Readint( &loop_orient[loop] , "" );
	}

	fu = Make_face( s , (loop_de[0]-1)/2 , loop_orient[0] );
	for( loop=1 ; loop<no_of_loops ; loop++ )
	{
		if( !Add_loop_to_face( fu , (loop_de[loop]-1)/2 , loop_orient[loop] ) )
			goto err;
	}

	rt_free( (char *)loop_de , "Get_outer_face: loop DE's" );
	rt_free( (char *)loop_orient , "Get_outer_face: loop orients" );
	return( fu );

  err :
	rt_free( (char *)loop_de , "Get_outer_face: loop DE's" );
	rt_free( (char *)loop_orient , "Get_outer_face: loop orients" );
	return( fu );
}
