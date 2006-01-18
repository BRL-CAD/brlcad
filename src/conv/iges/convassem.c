/*                     C O N V A S S E M . C
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
/** @file convassem.c
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


/*	This routine controls the conversion of IGES solid assemblies
	to BRL-CAD groups	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

/* Linked list to hold needed data for the group members */
struct solid_list
{
	int item;	/* Index into "dir" structure for this group member */
	int matrix;	/* Pointer to transformation entity for this member */
	char *name;	/* BRL-CAD name for this member */
	mat_t rot;	/* Pointer to BRL-CAD matrix */
	struct solid_list *next;
};

void
Convassem()
{
	int			i,j,k,comblen,conv=0,totass=0;
	struct solid_list	*root,*ptr;
	struct wmember		head,*wmem;
	int			no_of_assoc=0;
	int			no_of_props=0;
	int			att_de=0;
	unsigned char		*rgb;
	struct brlcad_att	brl_att;
	fastf_t			*flt;

	bu_log( "\nConverting solid assembly entities:\n" );

	ptr = NULL;
	root = NULL;
	BU_LIST_INIT( &head.l );

	for( i=0 ; i<totentities ; i++ ) /* loop through all entities */
	{
		if( dir[i]->type != 184 )	/* This is not a solid assembly */
			continue;

		/* Increment count of solid assemblies */
		totass++;

		if( dir[i]->param <= pstart )
		{
			bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
					dir[i]->direct , dir[i]->name );
			continue;
		}
		Readrec( dir[i]->param ); /* read first record into buffer */

		Readint( &j , "" );	/* read entity type */
		if( j != 184 )
		{
			bu_log( "Incorrect entity type in Parameter section for entity %d\n" , i );
			return;
		}

		Readint( &comblen , "" );	/* read number of members in group */

		/* Read pointers to group members */
		for( j=0 ; j<comblen ; j++ )
		{
			if( ptr == NULL )
			{
				root = (struct solid_list *)bu_malloc( sizeof( struct solid_list ),
						"Convassem: root" );
				ptr = root;
			}
			else
			{
				ptr->next = (struct solid_list *)bu_malloc( sizeof( struct solid_list ),
						"Convassem: ptr->next" );
				ptr = ptr->next;
			}
			ptr->next = NULL;

			/* Read pointer to an object */
			Readint( &ptr->item , "" );
			if( ptr->item < 0 )
				ptr->item = (-ptr->item);

			/* Convert pointer to a "dir" index */
			ptr->item = (ptr->item-1)/2;

			/* Save name of object */
			ptr->name = dir[ptr->item]->name;

			/* increment reference count */
			dir[ptr->item]->referenced++;
		}

		/* Read pointer to transformation matrix for each member */
		ptr = root;
		for( j=0 ; j<comblen ; j++ )
		{
			ptr->matrix = 0;

			/* Read pointer to a transformation */
			Readint( &ptr->matrix , "" );

			if( ptr->matrix < 0 )
				ptr->matrix = (-ptr->matrix);

			/* Convert to a "dir" index */
			if( ptr->matrix )
				ptr->matrix = (ptr->matrix-1)/2;
			else
				ptr->matrix = (-1); /* flag to indicate "none" */

			ptr = ptr->next;
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

		/* Make the members */
		ptr = root;
		while( ptr != NULL )
		{
			/* copy the members original transformation matrix */
			for( j=0 ; j<16 ; j++ )
				ptr->rot[j] = (*dir[ptr->item]->rot)[j];

			/* Apply any matrix indicated for this group member */
			if( ptr->matrix > (-1) )
				Matmult( ptr->rot , *(dir[ptr->matrix]->rot)  , ptr->rot );

			wmem = mk_addmember( ptr->name , &head.l, NULL, operator[Union] );
			flt = (fastf_t *)ptr->rot;
			for( j=0 ; j<16 ; j++ )
			{
				wmem->wm_mat[j] = (*flt);
				flt++;
			}
			ptr = ptr->next;
		}

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


		/* Increment the count of successful conversions */
		conv++;

		/* Free some memory */
		ptr = root;
		while( ptr != NULL )
		{
			bu_free( (char *)ptr, "convassem: ptr" );
			ptr = ptr->next;
		}
	}
	bu_log( "Converted %d solid assemblies successfully out of %d total assemblies\n" , conv , totass );
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
