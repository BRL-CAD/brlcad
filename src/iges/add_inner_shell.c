/*               A D D _ I N N E R _ S H E L L . C
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
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
/** @file add_inner_shell.c
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
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
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &no_of_faces , "" );

	face_de = (int *)bu_calloc( no_of_faces , sizeof( int ) , "Add_inner_shell face DE's" );
	face_orient = (int *)bu_calloc( no_of_faces , sizeof( int ) , "Add_inner_shell orients" );
	fu = (struct faceuse **)bu_calloc( no_of_faces , sizeof( struct faceuse *) , "Get_outer_shell faceuses " );

	for( face=0 ; face<no_of_faces ; face++ )
	{
		Readint( &face_de[face] , "" );
		Readint( &face_orient[face] , "" );
	}

	s = nmg_msv( r );
	for( face=0 ; face<no_of_faces ; face++ )
	{
		fu[face_count] = Add_face_to_shell( s , (face_de[face]-1)/2 , 1 );
		if( fu[face_count] != (struct faceuse *)NULL )
			face_count++;
	}

	nmg_gluefaces( fu , face_count, &tol );

	bu_free( (char *)fu , "Add_inner_shell: faceuse list" );
	bu_free( (char *)face_de , "Add_inner_shell: face DE's" );
	bu_free( (char *)face_orient , "Add_inner_shell: face orients" );
	return( s );
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
