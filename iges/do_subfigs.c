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
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA. All rights reserved.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "./iges_struct.h"
#include "./iges_extern.h"
#include "../librt/debug.h"

void
Do_subfigs()
{
	int i,j;
	int entity_type;
	struct wmember head1;
	struct wmember *wmem;

	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_mem_barriercheck();

	RT_LIST_INIT( &head1.l );

	for( i=0 ; i<totentities ; i++ )
	{
		int subfigdef_de;
		int subfigdef_index;
		int no_of_members;
		int *members;
		char *name;
		struct wmember head2;
		vect_t tmp_vec;
		double mat_scale[3];
		int non_unit;

		if( dir[i]->type != 408 )
			continue;

		if( rt_g.debug & DEBUG_MEM_FULL )
			rt_mem_barriercheck();

		if( dir[i]->param <= pstart )
		{
			rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
					dir[i]->direct , dir[i]->name );
			continue;
		}

		Readrec( dir[i]->param );
		Readint( &entity_type , "" );
		if( entity_type != 408 )
		{
			rt_log( "Expected Singular Subfigure Instance Entity, found %s\n",
				iges_type(entity_type) );
			continue;
		}

		Readint( &subfigdef_de, "" );
		subfigdef_index = (subfigdef_de - 1)/2;
		if( subfigdef_index >= totentities )
		{
			rt_log( "Singular Subfigure Instance Entity gives Subfigure Definition" );
			rt_log( "\tEntity DE of %d, largest DE in file is %d\n",
				subfigdef_de, (totentities * 2) - 1 );
			continue;
		}
		if( dir[subfigdef_index]->type != 308 )
		{
			rt_log( "Expected Subfigure Definition Entity, found %s\n",
				iges_type(dir[subfigdef_index]->type) );
			continue;
		}

		if( dir[subfigdef_index]->param <= pstart )
		{
			rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
					dir[subfigdef_index]->direct , dir[subfigdef_index]->name );
			continue;
		}
		Readrec( dir[subfigdef_index]->param );
		Readint( &entity_type , "" );
		if( entity_type != 308 )
		{
			rt_log( "Expected Subfigure Definition Entity, found %s\n",
				iges_type(entity_type) );
			continue;
		}

		Readint( &j, "" );	/* ignore depth */
		Readstrg( "");		/* ignore subfigure name */

		wmem = mk_addmember( dir[subfigdef_index]->name, &head1, WMOP_UNION );
		non_unit = 0;
		for( j=0 ; j<3 ; j++ )
		{
			double mag_sq;

			mat_scale[j] = 1.0;
			mag_sq = MAGSQ( &(*dir[i]->rot)[j*4] );
			if( !NEAR_ZERO( mag_sq - 1.0, 100.0*SQRT_SMALL_FASTF ) )
			{
				mat_scale[j] = 1.0/sqrt( mag_sq );
				non_unit = 1;
			}
		}

		if( non_unit )
		{
			rt_log( "Illegal transformation matrix in %s for member %s\n",
				curr_file->obj_name, wmem->wm_name );
			rt_log( " row vector magnitudes are %g, %g, and %g\n", 
				1.0/mat_scale[0], 1.0/mat_scale[1], 1.0/mat_scale[2] );
			mat_print( "", *dir[i]->rot );
			for( j=0 ; j<11 ; j++ )
			{
				if( (j+1)%4 == 0 )
					continue;
				(*dir[i]->rot)[j] *= mat_scale[0];
			}
			mat_print( "After scaling:", *dir[i]->rot );
			
		}
		bcopy( *dir[i]->rot, wmem->wm_mat, sizeof( mat_t ) );

		Readint( &no_of_members, "" );	/* get number of members */
		members = (int *)rt_calloc( no_of_members, sizeof( int ), "Do_subfigs: members" );
		for( j=0 ; j<no_of_members ; j++ )
			Readint( &members[j], "" );

		RT_LIST_INIT( &head2.l );
		for( j=0 ; j<no_of_members ; j++ )
		{
			int index;

			index = (members[j] - 1)/2;

			if( index >= totentities )
			{
				rt_log( "Subfigure Definition Entity gives Member Entity" );
				rt_log( "\tDE of %d, largest DE in file is %d\n",
					members[j], (totentities * 2) - 1 );
				continue;
			}
			if( dir[index]->param <= pstart )
			{
				rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
						dir[index]->direct , dir[index]->name );
				continue;
			}

			if( dir[index]->type == 416 )
			{
				struct file_list *list_ptr;
				char *file_name;
				int found=0;

				/* external reference */

				Readrec( dir[index]->param );
				Readint( &entity_type, "" );

				if( entity_type != 416 )
				{
					rt_log( "Expected External reference Entity, found %s\n",
						iges_type(entity_type) );
					continue;
				}

				if( dir[index]->form != 1 )
				{
					rt_log( "External Reference Entity of form #%d found\n",
						dir[index]->form );
					rt_log( "\tOnly form #1 is currnetly handled\n" );
					continue;
				}

				Readname( &file_name, "" );

				/* Check if this external reference is already on the list */
				for( RT_LIST_FOR( list_ptr, file_list, &iges_list.l ) )
				{
					if( !strcmp( file_name, list_ptr->file_name ) )
					{
						found = 1;
						name = list_ptr->obj_name;
						break;
					}
				}

				if( !found )
				{
					/* Need to add this one to the list */
					list_ptr = (struct file_list *)rt_malloc( sizeof( struct file_list ),
							"Do_subfigs: list_ptr" );

					list_ptr->file_name = file_name;
					if( no_of_members == 1 )
						strcpy( list_ptr->obj_name, dir[subfigdef_index]->name );
					else
					{
						strcpy( list_ptr->obj_name, "subfig" );
						(void) Make_unique_brl_name( list_ptr->obj_name );
					}


					RT_LIST_APPEND( &curr_file->l, &list_ptr->l );

					name = list_ptr->obj_name;
				}
				else
					rt_free( (char *)file_name, "Do_subfigs: file_name" );

			}
			else
				name = dir[index]->name;

			if( no_of_members > 1 )
			{
				wmem = mk_addmember( name, &head2, WMOP_UNION );
				bcopy( dir[index]->rot, wmem->wm_mat, sizeof( mat_t ) );
			}
		}

		if( no_of_members > 1 )
			(void)mk_lcomb( fdout, dir[subfigdef_index]->name, &head2, 0, 
					(char *)NULL, (char *)NULL, (unsigned char *)NULL, 0 );
	}

	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_mem_barriercheck();

	if( RT_LIST_IS_EMPTY( &head1.l ) )
		return;

	(void) mk_lcomb( fdout, curr_file->obj_name, &head1, 0,
			(char *)NULL, (char *)NULL, (unsigned char *)NULL, 0 );

	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_mem_barriercheck();

}
