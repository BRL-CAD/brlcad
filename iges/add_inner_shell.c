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
#include "./iges_struct.h"
#include "./iges_extern.h"

struct shell *
Add_inner_shell( r , entityno , shell_orient )
struct nmgregion *r;
int entityno;
int shell_orient;
{ 

	int		sol_num;		/* IGES solid type number */
	int		no_of_faces;		/* Number of faces in shell */
	int		face_count=0;		/* Actual number of faces made */
	int		*face_de;		/* Directory seqence numbers for faces */
	int		*face_orient;		/* Orientation of faces */
	int		face;
	struct shell	*s;			/* NMG shell */
	struct faceuse	**fu;			/* list of faceuses */

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &no_of_faces , "" );

	face_de = (int *)rt_calloc( no_of_faces , sizeof( int ) , "Add_inner_shell face DE's" );
	face_orient = (int *)rt_calloc( no_of_faces , sizeof( int ) , "Add_inner_shell orients" );
	fu = (struct faceuse **)rt_calloc( no_of_faces , sizeof( struct faceuse *) , "Get_outer_shell faceuses " );

	for( face=0 ; face<no_of_faces ; face++ )
	{
		Readint( &face_de[face] , "\t\tFACE:" );
		Readint( &face_orient[face] , "\t\t\tORIENTATION:" );
	}

	s = nmg_msv( r );
	for( face=0 ; face<no_of_faces ; face++ )
	{
		fu[face_count] = Add_face_to_shell( s , (face_de[face]-1)/2 , 1 );
		if( fu[face_count] != (struct faceuse *)NULL )
			face_count++;
	}

	nmg_gluefaces( fu , face_count, &tol );

	rt_free( (char *)fu , "Add_inner_shell: faceuse list" );
	rt_free( (char *)face_de , "Add_inner_shell: face DE's" );
	rt_free( (char *)face_orient , "Add_inner_shell: face orients" );
	return( s );
}
