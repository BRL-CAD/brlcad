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
Get_outer_shell( r , entityno , shell_orient )
struct nmgregion *r;
int entityno;
int shell_orient;
{ 

	int		sol_num;		/* IGES solid type number */
	int		no_of_faces;		/* Number of faces in shell */
	int		face_count=0;		/* Number of faces actually made */
	int		*face_de;		/* Directory seqence numbers for faces */
	int		*face_orient;		/* Orientation of faces */
	int		face;
	struct shell	*s;			/* NMG shell */
	struct faceuse	**fu;			/* list of faceuses */

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &no_of_faces , "" );

	face_de = (int *)bu_calloc( no_of_faces , sizeof( int ) , "Get_outer_shell face DE's" );
	face_orient = (int *)bu_calloc( no_of_faces , sizeof( int ) , "Get_outer_shell orients" );
	fu = (struct faceuse **)bu_calloc( no_of_faces , sizeof( struct faceuse *) , "Get_outer_shell faceuses " );

	for( face=0 ; face<no_of_faces ; face++ )
	{
		Readint( &face_de[face] , "" );
		Readint( &face_orient[face] , "" );
	}

	s = nmg_msv( r );

	for( face=0 ; face<no_of_faces ; face++ )
	{
		fu[face_count] = Add_face_to_shell( s , (face_de[face]-1)/2 , face_orient[face]  );
		if( fu[face_count] != (struct faceuse *)NULL )
			face_count++;
	}

	nmg_gluefaces( fu , face_count, &tol );

	bu_free( (char *)fu , "Get_outer_shell: faceuse list" );
	bu_free( (char *)face_de , "Get_outer_shell: face DE's" );
	bu_free( (char *)face_orient , "Get_outer_shell: face orients" );
	return( s );
}
