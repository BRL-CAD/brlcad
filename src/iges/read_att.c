/*
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993-2004 by the United States Army.
 *	All rights reserved.
 */

/*
 *	This routine reads the IGES attribute instance entity
 *	at DE (att_de) and stores the values in the structure (att)
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Read_att( att_de , att )
int att_de;
struct brlcad_att *att;
{
	int			entityno;
	int			i;

	if( att_de == 0 )
	{
		/* fill structure with default info */
		att->material_name = (char *)NULL;
		att->material_params = (char *)NULL;
		att->region_flag = 0;
		att->ident = 0;
		att->air_code = 0;
		att->material_code = 0;
		att->los_density = 100;
		att->inherit = 0;
		att->color_defined = 0;
		return;
	}

	/* Acquiring Data */

	entityno = (att_de-1)/2;

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return;
	}

	Readrec( dir[entityno]->param );
	Readint( &i , "" );
	if( i != 422 )
	{
		bu_log( "Read_att: Expecting attribute instance, found type %d\n" , i );
		return;
	}

	Readname( &att->material_name , "" );
	Readname( &att->material_params , "" );
	Readint( &att->region_flag , "" );
	Readint( &att->ident , "" );
	Readint( &att->air_code , "" );
	Readint( &att->material_code , "" );
	Readint( &att->los_density , "" );
	Readint( &att->inherit , "" );
	Readint( &att->color_defined , "" );
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
