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

void
Do_subfigs()
{
	int i,j;
	int entity_type;
	struct wmember head1;
	struct wmember *wmem;

	RT_LIST_INIT( &head1.l );

	for( i=0 ; i<totentities ; i++ )
	{
		int subfigdef_de;
		int subfigdef_index;
		int no_of_members;
		int *members;
		char *name;
		struct wmember head2;

		if( dir[i]->type != 408 )
			continue;

		if( dir[i]->param <= pstart )
		{
			rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
					dir[i]->direct , dir[i]->name );
			continue;
		}

		Readrec( dir[i]->param );
		Readint( &entity_type , "Entity Type: " );
		if( entity_type != 408 )
		{
			rt_log( "Expected Singular Subfigure Instance Entity, found %s\n",
				iges_type(entity_type) );
			continue;
		}

		Readint( &subfigdef_de, "Subfigure DE: " );
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
		Readint( &entity_type , "Entity Type: " );
		if( entity_type != 308 )
		{
			rt_log( "Expected Subfigure Definition Entity, found %s\n",
				iges_type(entity_type) );
			continue;
		}

		Readint( &j, "" );		/* ignore depth */
		Readname( &name, "Name: ");		/* get subfigure name */

		dir[subfigdef_index]->name = Make_unique_brl_name( name );
		rt_free( name, "Do_subfigs: name" );

		wmem = mk_addmember( dir[subfigdef_index]->name, &head1, WMOP_UNION );
		bcopy( dir[subfigdef_index]->rot, wmem->wm_mat, sizeof( mat_t ) );

		Readint( &no_of_members, "No. of members: " );	/* get number of members */
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

				/* external reference */

				Readrec( dir[index]->param );
				Readint( &entity_type, "Entity type: " );

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

				list_ptr = (struct file_list *)rt_malloc( sizeof( struct file_list ),
						"Do_subfigs: list_ptr" );

				Readname( &list_ptr->file_name, "File Name: " );
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
				name = dir[index]->name;

			if( no_of_members > 1 )
			{
				wmem = mk_addmember( name, &head2, WMOP_UNION );
				bcopy( dir[index]->rot, wmem->wm_mat, sizeof( mat_t ) );
			}
		}

		if( no_of_members > 1 )
		{
rt_log( "Making group (%s)\n", dir[subfigdef_index]->name );
for( RT_LIST_FOR( wmem, wmember, &head2.l ) )
	rt_log( "\tu %s\n", wmem->wm_name );
			(void)mk_lcomb( fdout, dir[subfigdef_index]->name, &head2, 0, 
					(char *)NULL, (char *)NULL, (unsigned char *)NULL, 0 );
		}
	}

	if( RT_LIST_IS_EMPTY( &head1.l ) )
		return;

rt_log( "Making group (%s)\n", curr_file->obj_name );
for( RT_LIST_FOR( wmem, wmember, &head1.l ) )
	rt_log( "\tu %s\n", wmem->wm_name );
	(void) mk_lcomb( fdout, curr_file->obj_name, &head1, 0,
			(char *)NULL, (char *)NULL, (unsigned char *)NULL, 0 );
}
