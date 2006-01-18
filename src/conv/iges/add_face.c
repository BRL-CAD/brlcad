/*                      A D D _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
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
/** @file add_face.c
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
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return( (struct faceuse *)NULL );
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &surf_de , "" );
	Readint( &no_of_loops , "" );
	Readint( &outer_loop_flag , "" );
	loop_de = (int *)bu_calloc( no_of_loops , sizeof( int ) , "Get_outer_face loop DE's" );
	for( loop=0 ; loop<no_of_loops ; loop++ )
		Readint( &loop_de[loop] , "" );

	/* Check that this is a planar surface */
	if( dir[(surf_de-1)/2]->type == 190 ) /* plane entity */
		planar = 1;
#if 0
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
#endif

	if( planar )
	{
		fu = Make_planar_face( s , (loop_de[0]-1)/2 , face_orient );
		if( !fu )
			goto err;
		for( loop=1 ; loop<no_of_loops ; loop++ )
		{
			if( !Add_loop_to_face( s , fu , ((loop_de[loop]-1)/2) , face_orient ))
				goto err;
		}
	}
	else if( dir[(surf_de-1)/2]->type == 128 )
	{
		struct face *f;

		fu = Make_nurb_face( s, (surf_de-1)/2 );
		NMG_CK_FACEUSE( fu );
		if( !face_orient )
		{
			f = fu->f_p;
			NMG_CK_FACE( f );
			f->flip = 1;
		}

NMG_CK_FACE_G_SNURB( fu->f_p->g.snurb_p );

		for( loop=0 ; loop<no_of_loops ; loop++ )
		{
			if( !Add_nurb_loop_to_face( s, fu, ((loop_de[loop]-1)/2) , face_orient ))
				goto err;
		}
NMG_CK_FACE_G_SNURB( fu->f_p->g.snurb_p );
	}
	else
	{
		fu = (struct faceuse *)NULL;
		bu_log( "Add_face_to_shell: face at DE%d is neither planar nor NURB, ignoring\n", surf_de );
	}

  err :
	bu_free( (char *)loop_de , "Add_face_to_shell: loop DE's" );
	return( fu );
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
