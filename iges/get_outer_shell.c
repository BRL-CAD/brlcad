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
#include "raytrace.h"
#include "wdb.h"
#include "./iges_struct.h"
#include "./iges_extern.h"

RT_EXTERN( struct faceuse *Add_face_to_shell , ( struct shell *s , int entityno , int face_orient) );
RT_EXTERN( struct shell *nmg_msv , (struct nmgregion *r) );

int
Get_outer_shell( r , entityno , shell_orient )
struct nmgregion *r;
int entityno;
int shell_orient;
{ 

	int		sol_num;		/* IGES solid type number */
	int		no_of_faces;		/* Number of faces in shell */
	int		*face_de;		/* Directory seqence numbers for faces */
	int		*face_orient;		/* Orientation of faces */
	int		face_use_orient;	/* Face orientation for shell */
	int		face;
	struct shell	*s;			/* NMG shell */
	struct faceuse	**fu;			/* list of faceuses */

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "SHELL: " );
	Readint( &no_of_faces , "\tNo of faces: " );

	face_de = (int *)rt_calloc( no_of_faces , sizeof( int ) , "Get_outer_shell face DE's" );
	face_orient = (int *)rt_calloc( no_of_faces , sizeof( int ) , "Get_outer_shell orients" );
	fu = (struct faceuse **)rt_calloc( no_of_faces , sizeof( struct faceuse *) , "Get_outer_shell faceuses " );

	for( face=0 ; face<no_of_faces ; face++ )
	{
		Readint( &face_de[face] , "\t\tFace de: " );
		Readint( &face_orient[face] , "\t\tFace orient: " );
	}

	s = nmg_msv( r );

	for( face=0 ; face<no_of_faces ; face++ )
	{
		if( face_orient[face] == shell_orient )
			face_use_orient = 1;
		else
			face_use_orient = 0;
		fu[face] = Add_face_to_shell( s , (face_de[face]-1)/2 , face_use_orient  );
	}

	nmg_gluefaces( fu , no_of_faces );

	rt_free( (char *)fu , "Get_outer_shell: faceuse list" );
	rt_free( (char *)face_de , "Get_outer_shell: face DE's" );
	rt_free( (char *)face_orient , "Get_outer_shell: face orients" );
	return( 1 );
}
