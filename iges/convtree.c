/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

/*	This routine controls the conversion of IGES boolean trees
	to BRLCAD objects	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Convtree()
{

	int			notdone=2;
	int			conv=0;
	int			tottrees=0;
	union tree		*ptr,*oldptr;
	union tree		*Readtree(),*Copytree();
	struct rt_comb_internal	*comb;
	int			no_of_assoc=0;
	int			no_of_props=0;
	int			att_de=0;
	unsigned char 		*rgb;
	struct brlcad_att	brl_att;
	int			i,j,k;

	rt_log( "\nConverting boolean tree entities:\n" );

	for( i=0 ; i<totentities ; i++ ) /* loop through all entities */
	{
		if( dir[i]->type != 180 )	/* This is not a tree */
			continue;

		att_de = 0;			/* For default if there is no attribute entity */

		tottrees++;

		if( dir[i]->param <= pstart )	/* Illegal parameter address */
		{
			rt_log( "Entity number %d (Boolean Tree) does not have a legal parameter pointer\n" , i );
			continue;
		}

		Readrec( dir[i]->param ); /* read first record into buffer */
		ptr = Readtree( dir[i]->rot ); /* construct the tree */
		if( !ptr )	/* failure */
		{
			bu_log( "\tFailed to convert Boolean tree at D%07d\n", dir[i]->direct );
			continue;
		}

		/* skip over the associativities */
		Readint( &no_of_assoc , "" );
		for( k=0 ; k<no_of_assoc ; k++ )
			Readint( &j , "" );

		/* get property entity DE's */
		Readint( &no_of_props , "" );
		for( k=0 ; k<no_of_props ; k++ )
		{
			Readint( &j , "" );
			if( dir[(j-1)/2]->type == 422 &&
				 dir[(j-1)/2]->referenced == brlcad_att_de )
			{
				/* this is one of our attribute instances */
				att_de = j;
			}
		}

		Read_att( att_de , &brl_att );
		/* Read_att will supply defaults if att_de is 0 */
		if( att_de == 0 )
			brl_att.region_flag = 1;

		BU_GETSTRUCT( comb, rt_comb_internal );
		comb->magic = RT_COMB_MAGIC;
		comb->tree = ptr;
		if( brl_att.region_flag )
		{
			comb->region_flag = 1;
			comb->region_id = brl_att.ident;
			comb->aircode = brl_att.air_code;
			comb->GIFTmater = brl_att.material_code;
			comb->los = brl_att.los_density;
		}
		if( dir[i]->colorp != 0 )
		{
			comb->rgb_valid = 1;
			comb->rgb[0] = dir[i]->rgb[0];
			comb->rgb[1] = dir[i]->rgb[1];
			comb->rgb[2] = dir[i]->rgb[2];
		}
		comb->inherit = brl_att.inherit;
		bu_vls_init( &comb->shader );
		if( brl_att.material_name )
		{
			bu_vls_strcpy( &comb->shader, brl_att.material_name );
			if( brl_att.material_params )
			{
				bu_vls_putc( &comb->shader, ' ' );
				bu_vls_strcat( &comb->shader, brl_att.material_params );
			}
		}
		bu_vls_init( &comb->material );

		if( mk_export_fwrite( fdout, dir[i]->name, (genptr_t)comb, ID_COMBINATION ) )
		{
			bu_log( "mk_export_fwrite() failed for combination (%s)\n", dir[i]->name );
			exit( 1 );
		}
		conv++;
	}
	rt_log( "Converted %d trees successfully out of %d total trees\n", conv , tottrees );
}
