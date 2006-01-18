/*                      R E A D T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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
/** @file readtree.c
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

/*		Read and construct a boolean tree		*/

#include "./iges_struct.h"
#include "./iges_extern.h"

union tree *Readtree( matp )
mat_t *matp;
{
	int length,i,k,op;
	union tree *ptr,*Pop();
	matp_t new_mat;

	Readint( &i , "" );
	if( i != 180 )
	{
		bu_log( "Expecting a Boolean Tree, found type %d\n" , i );
		return( (union tree *)NULL );
	}

	Freestack();
	Readint( &length , "" );
	for( i=0 ; i<length ; i++ )
	{
		Readint( &op , "" );
		if( op < 0 )	/* This is an operand */
		{
			ptr = (union tree *)bu_malloc( sizeof( union tree ),
				"Readtree: ptr" );
			ptr->magic = RT_TREE_MAGIC;
			ptr->tr_l.tl_op = OP_DB_LEAF;
			k = ((-op)-1)/2;
			if( k < 0 || k >= totentities )
			{
				bu_log( "Readtree(): pointer in tree is out of bounds (%d)\n", -op );
				return( (union tree *)NULL );
			}
			if( dir[k]->type <= 0 )
			{
				bu_log( "Unknown entity type (%d) at D%07d\n", -dir[k]->type, dir[k]->direct );
				return( (union tree *)NULL );
			}
			ptr->tr_l.tl_name = bu_strdup( dir[k]->name );
			if( matp && dir[k]->rot )
			{
				new_mat = (matp_t)bu_malloc( sizeof( mat_t ), "new_mat" );
				bn_mat_mul( new_mat, *matp, *dir[k]->rot );
			}
			else if( dir[k]->rot )
				new_mat = *dir[k]->rot;
			else if( matp )
				new_mat = *matp;
			else
				new_mat = (matp_t)NULL;
			ptr->tr_l.tl_mat = new_mat;
			Push( ptr );
		}
		else	/* This is an operator */
		{
			ptr = (union tree *)bu_malloc( sizeof( union tree ),
				"Readtree: ptr" );
			ptr->magic = RT_TREE_MAGIC;
			switch( op )
			{
				case 1:
					ptr->tr_b.tb_op = OP_UNION;
					break;
				case 2:
					ptr->tr_b.tb_op = OP_INTERSECT;
					break;
				case 3:
					ptr->tr_b.tb_op = OP_SUBTRACT;
					break;
				default:
					bu_log( "Readtree(): illegal operator code (%d)\n", op );
					exit( 1 );
			}
			ptr->tr_b.tb_right = Pop();
			ptr->tr_b.tb_left = Pop();
			Push( ptr );
		}
	}

	return( Pop() );
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
