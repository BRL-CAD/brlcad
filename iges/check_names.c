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

int
Fix_name( entityno )
int entityno;
{
	char *ptr;
	int i;

	ptr = dir[entityno]->name;
	i = 0;
	while( *ptr != '\0' && i < NAMELEN )
	{
		i++;
		if( isspace(*ptr) || *ptr == '/' )
			*ptr = '_';
		ptr++;
	}
	*ptr = '\0';

	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->name == (char *)NULL )
			continue;
		if( i == entityno )
			continue;
		if( (dir[i]->type > 149 && dir[i]->type < 187)
			|| dir[i]->type == 128
			|| dir[i]->type == 430 )
		{
			if( !strcmp( dir[i]->name , ptr ) )
			{
				printf( "Name (%s) already used\n" , ptr );
				return( 0 );
			}
		}
	}

	return( 1 );
}

void
Skip_field()
{
	int		done=0;
	int		lencard;

	if( card[counter] == eof ) /* This is an empty field */
	{
		counter++;
		return;
	}
	else if( card[counter] == eor ) /* Up against the end of record */
		return;

	if( card[72] == 'P' )
		lencard = PARAMLEN;
	else
		lencard = CARDLEN;

	if( counter >= lencard )
		Readrec( ++currec );

	while( !done )
	{
		while( card[counter++] != eof && card[counter] != eor &&
			counter <= lencard );
		if( counter > lencard && card[counter] != eor && card[counter] != eof )
			Readrec( ++currec );
		else
			done = 1;
	}

	if( card[counter] == eor )
		counter--;
}

void
Get_name( entityno , skip )
int entityno;
int skip;
{
	int			sol_num;
	int			i,j,k;
	int			no_of_assoc=0;
	int			no_of_props=0;
	int			name_de=0;

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return;
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	for( i=0 ; i<skip ; i++ )
		Skip_field();

	/* skip over the associativities */
	Readint( &no_of_assoc , "" );
	for( k=0 ; k<no_of_assoc ; k++ )
		Readint( &j , "" );

	/* get property entity DE's */
	Readint( &no_of_props , "" );
	for( k=0 ; k<no_of_props ; k++ )
	{
		j = 0;
		Readint( &j , "" );
		if( dir[(j-1)/2]->type == 406 &&
		    dir[(j-1)/2]->form == 15 )
		{
			/* this is a name */
			name_de = j;
			break;
		}
	}

	if( !name_de )
		return;

	Readrec( dir[(name_de-1)/2]->param );
	Readint( &sol_num , "" );
	if( sol_num != 406 )
	{
		/* this is not a property entity */
		rt_log( "Check_names: entity at DE %d is not a property entity\n" , name_de );
		return;
	}

	Readint( &i , "" );
	if( i != 1 )
	{
		rt_log( "Bad property entity, form 15 (name) should have only one value, not %d\n" , i );
		return;
	}

	Readname( &dir[entityno]->name , "" );

	if( !Fix_name( entityno ) )
	{
		rt_free( (char *)dir[entityno]->name , "Get_name: name" );
		dir[entityno]->name = (char *)NULL;
	}
}

void
Get_drawing_name( entityno )
int entityno;
{
	int entity_type;
	int no_of_views;
	int no_of_annot;
	int no_of_assoc;
	int no_of_props;
	int i,j,k;
	int name_de=0;

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return;
	}

	Readrec( dir[entityno]->param );
	Readint( &entity_type , "" );
	if( entity_type != 404 )
	{
		rt_log( "Get_drawing_name: entity at P%07d (type %d) is not a drawing entity\n" , dir[entityno]->param , entity_type );
		return;
	}

	Readint( &no_of_views , "" );
	for( i=0 ; i<no_of_views ; i++ )
	{
		for( j=0 ; j<3 ; j++ )
			Skip_field();
	}

	Readint( &no_of_annot , "" );
	for( i=0 ; i<no_of_annot ; i++ )
		Skip_field();
	/* skip over the associativities */
	Readint( &no_of_assoc , "" );
	for( k=0 ; k<no_of_assoc ; k++ )
		Readint( &j , "" );

	/* get property entity DE's */
	Readint( &no_of_props , "" );
	for( k=0 ; k<no_of_props ; k++ )
	{
		j = 0;
		Readint( &j , "" );
		if( dir[(j-1)/2]->type == 406 &&
		    dir[(j-1)/2]->form == 15 )
		{
			/* this is a name */
			name_de = j;
			break;
		}
	}

	if( !name_de )
		return;

	Readrec( dir[(name_de-1)/2]->param );
	Readint( &entity_type , "" );
	if( entity_type != 406 )
	{
		/* this is not a property entity */
		rt_log( "Get_drawing_name: entity at DE %d is not a property entity\n" , name_de );
		return;
	}

	Readint( &i , "" );
	if( i != 1 )
	{
		rt_log( "Bad property entity, form 15 (name) should have only one value, not %d\n" , i );
		return;
	}

	Readname( &dir[entityno]->name , "" );

	if( !Fix_name( entityno ) )
	{
		rt_free( (char *)dir[entityno]->name , "Get_name: name" );
		dir[entityno]->name = (char *)NULL;
	}
}

void
Get_csg_name( entityno )
int entityno;
{
	int			sol_num;
	int			i,j,k;
	int			num;
	int			skip;
	int			no_of_assoc=0;
	int			no_of_props=0;
	int			name_de=0;

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return;
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &num , "" );
	if( sol_num == 180 )
		skip = num;
	else if( sol_num == 184 )
		skip = 2*num;
	else
	{
		rt_log( "Get_csg_name: entity (type %d), not a CSG\n" , sol_num );
		return;
	}

	for( i=0 ; i<skip ; i++ )
		Skip_field();

	/* skip over the associativities */
	Readint( &no_of_assoc , "" );
	for( k=0 ; k<no_of_assoc ; k++ )
		Readint( &j , "" );

	/* get property entity DE's */
	Readint( &no_of_props , "" );
	for( k=0 ; k<no_of_props ; k++ )
	{
		j = 0;
		Readint( &j , "" );
		if( dir[(j-1)/2]->type == 406 &&
		    dir[(j-1)/2]->form == 15 )
		{
			/* this is a name */
			name_de = j;
			break;
		}
	}

	if( !name_de )
		return;

	Readrec( dir[(name_de-1)/2]->param );
	Readint( &sol_num , "" );
	if( sol_num != 406 )
	{
		/* this is not a property entity */
		rt_log( "Check_names: entity at DE %d is not a property entity\n" , name_de );
		return;
	}

	Readint( &i , "" );
	if( i != 1 )
	{
		rt_log( "Bad property entity, form 15 (name) should have only one value, not %d\n" , i );
		return;
	}

	Readname( &dir[entityno]->name , "" );

	if( !Fix_name( entityno ) )
	{
		rt_free( (char *)dir[entityno]->name , "Get_name: name" );
		dir[entityno]->name = (char *)NULL;
	}
}


void
Get_brep_name( entityno )
int entityno;
{
	int			sol_num;
	int			i,j,k;
	int			num;
	int			skip;
	int			no_of_assoc=0;
	int			no_of_props=0;
	int			name_de=0;

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return;
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	if( sol_num != 186 )
	{
		rt_log( "Get_brep_name: Entity (type %d) is not a BREP\n" , sol_num );
		return;
	}
	Skip_field();
	Skip_field();
	Readint( &num , "" );
	skip = 2*num;

	for( i=0 ; i<skip ; i++ )
		Skip_field();

	/* skip over the associativities */
	Readint( &no_of_assoc , "" );
	for( k=0 ; k<no_of_assoc ; k++ )
		Readint( &j , "" );

	/* get property entity DE's */
	Readint( &no_of_props , "" );
	for( k=0 ; k<no_of_props ; k++ )
	{
		j = 0;
		Readint( &j , "" );
		if( dir[(j-1)/2]->type == 406 &&
		    dir[(j-1)/2]->form == 15 )
		{
			/* this is a name */
			name_de = j;
			break;
		}
	}

	if( !name_de )
		return;

	Readrec( dir[(name_de-1)/2]->param );
	Readint( &sol_num , "" );
	if( sol_num != 406 )
	{
		/* this is not a property entity */
		rt_log( "Check_names: entity at DE %d is not a property entity\n" , name_de );
		return;
	}

	Readint( &i , "" );
	if( i != 1 )
	{
		rt_log( "Bad property entity, form 15 (name) should have only one value, not %d\n" , i );
		return;
	}

	Readname( &dir[entityno]->name , "" );

	if( !Fix_name( entityno ) )
	{
		rt_free( (char *)dir[entityno]->name , "Get_name: name" );
		dir[entityno]->name = (char *)NULL;
	}
}

void
Check_names()
{

	int i;

	printf( "Looking for Name Entities...\n" );
	for( i=0 ; i < totentities ; i++ )
	{
		switch( dir[i]->type )
		{
			case 152:
				Get_name( i , 13 );
				break;
			case 150:
			case 168:
				Get_name( i , 12 );
				break;
			case 156:
				Get_name( i , 9 );
				break;
			case 154:
			case 160:
			case 162:
				Get_name( i , 8 );
				break;
			case 164:
				Get_name( i , 5 );
				break;
			case 158:
				Get_name( i , 4 );
				break;
			case 180:
			case 184:
				Get_csg_name( i );
				break;
			case 186:
				Get_brep_name( i );
				break;
			case 404:
				Get_drawing_name( i );
				break;
			case 410:
				if( dir[i]->form == 0 )
					Get_name( i , 8 );
				else if( dir[i]->form == 1 )
					Get_name( i , 22 );
				break;
			case 430:
				Get_name( i , 1 );
				break;
			default:
				break;
		}
	}

	printf( "Assigning names to entities without names...\n" );
	for( i=0 ; i < totentities ; i++ )
	{
		if( dir[i]->name == (char *)NULL )
		{
			switch( dir[i]->type )
			{
				case 128:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "nurb.%d" , i );
					break;
				case 150:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "block.%d" , i );
					break;
				case 152:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "wedge.%d" , i );
					break;
				case 154:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "cyl.%d" , i );
					break;
				case 156:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "cone.%d" , i );
					break;
				case 158:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "sphere.%d" , i );
					break;
				case 160:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "torus.%d" , i );
					break;
				case 162:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "revolution.%d" , i );
					break;
				case 164:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "extrusion.%d" , i );
					break;
				case 168:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "ell.%d" , i );
					break;
				case 180:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "region.%d" , i );
					break;
				case 184:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "group.%d" , i );
					break;
				case 186:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "brep.%d" , i );
					break;
				case 404:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "drawing.%d" , i );
					break;
				case 410:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "view.%d" , i );
					break;
				case 430:
					dir[i]->name = (char *)rt_malloc( NAMELEN+1 , "Check_names: dir->name" );
					sprintf( dir[i]->name , "inst.%d" , i );
					break;
			}
		}
	}
}
