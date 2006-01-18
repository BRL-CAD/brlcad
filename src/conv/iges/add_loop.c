/*                      A D D _ L O O P . C
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
/** @file add_loop.c
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

int
Add_loop_to_face( s , fu , entityno , face_orient )
struct shell *s;
struct faceuse *fu;
int entityno;
int face_orient;
{
	struct faceuse *fu_tmp;
	plane_t pl_fu,pl_fu_tmp;

	NMG_CK_SHELL( s );
	NMG_CK_FACEUSE( fu );
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
	{
		bu_log( "fu x%x (%s) and mate x%x (%s) have no OT_SAME use\n" ,
			fu , nmg_orientation( fu->orientation ) ,
			fu->fumate_p , nmg_orientation( fu->fumate_p->orientation ) );
		rt_bomb( "Faceuse and mate have no OT_SAME use\n" );

	}

	/* first make a new face from the loop */
	fu_tmp = Make_planar_face( s , entityno , face_orient );

	if( !fu_tmp )
		return( 0 );

	if( fu_tmp->orientation != OT_SAME )
		fu_tmp = fu_tmp->fumate_p;
	if( fu_tmp->orientation != OT_SAME )
	{
		bu_log( "fu_tmp x%x (%s) nad mate x%x (%s) have no OT_SAME use\n" ,
			fu_tmp , nmg_orientation( fu_tmp->orientation ) ,
			fu_tmp->fumate_p , nmg_orientation( fu_tmp->fumate_p->orientation ) );
		rt_bomb( "Faceuse and mate have no OT_SAME use\n" );

	}


	/* make sure OT_SAME use of this face is same direction as fu */
	NMG_GET_FU_PLANE( pl_fu , fu );
	NMG_GET_FU_PLANE( pl_fu_tmp , fu_tmp );
	if( VDOT( pl_fu , pl_fu_tmp ) < 0.0 )
		nmg_reverse_face( fu_tmp );

	/* join this temporary face to the existing face */
	nmg_jf( fu , fu_tmp );

	return( 1 );
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
