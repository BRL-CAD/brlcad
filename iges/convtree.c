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
	struct node		*ptr,*oldptr;
	struct node		*Readtree(),*Copytree();
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
		ptr = Readtree(); /* construct the tree */

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

		oldptr = Copytree( ptr , (struct node *)NULL ); /* save a copy */

		/* keep calling the tree manipulating routines until they
			stop working	*/
		notdone = 2;
		while( notdone )
		{
			notdone = 2;
			notdone -= Arrange( ptr );
			notdone -= Bubbleup( ptr );
		}

		/* Check for success of above routines */
		if( Treecheck( ptr ) )
		{
			struct wmember head;

			RT_LIST_INIT( &head.l );

			/* make member records */
			Makemembers( ptr , &head );

			/* Make the object */
			if( dir[i]->colorp != 0 )
				rgb = (unsigned char*)dir[i]->rgb;
			else
				rgb = (unsigned char *)0;

			mk_lrcomb( fdout , 
				dir[i]->name,		/* name */
				&head,			/* members */
				brl_att.region_flag,	/* region flag */
				brl_att.material_name,	/* material name */
				brl_att.material_params, /* material parameters */
				rgb,			/* color */
				brl_att.ident,		/* ident */
				brl_att.air_code,	/* air code */
				brl_att.material_code,	/* GIFT material */
				brl_att.los_density,	/* los density */
				brl_att.inherit );	/* inherit */

			conv++;
		}
		else
		{
			rt_log( "'%s'Tree cannot be converted to BRLCAD format\n",dir[i]->name );
			rt_log( "\tOriginal tree from IGES file:\n\t" );
			Showtree( oldptr );
			rt_log( "\tAfter attempted conversion to BRLCAD format:\n\t" );
			Showtree( ptr );
		}

		Freetree( ptr );
		Freetree( oldptr );
	}
	rt_log( "Converted %d trees successfully out of %d total trees\n", conv , tottrees );
}
