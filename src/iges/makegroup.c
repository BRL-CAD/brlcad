/*                     M A K E G R O U P . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** @file makegroup.c
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

/*	This routine creates a group called "all" consisting of
	all unreferenced entities	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Makegroup()
{

	int i,j,comblen=0,nurbs=0;
	struct wmember head,*wmem;
	fastf_t *flt;

	BU_LIST_INIT( &head.l );

	/* loop through all entities */
	for( i=0 ; i<totentities ; i++ )
	{
		/* Only consider CSG entities */
		if( (dir[i]->type > 149 && dir[i]->type < 185) || dir[i]->type == 430 )
		{
			/* If not referenced, count it */
			if( !(dir[i]->referenced) )
				comblen++;
		}
		else if( dir[i]->type == 128 )
			nurbs = 1;
	}

	if( comblen > 1 || nurbs )	/* We need to make a group */
	{

		/* Loop through everything again */
		for( i=0 ; i<totentities ; i++ )
		{
			/* Again, only consider CSG entities */
			if( (dir[i]->type > 149 && dir[i]->type < 185) || dir[i]->type == 430 )
			{
				if( !(dir[i]->referenced) )
				{
					/* Make the BRLCAD member record */
					wmem = mk_addmember( dir[i]->name , &head.l, NULL, operator[Union] );
					flt = (fastf_t *)dir[i]->rot;
					for( j=0 ; j<16 ; j++ )
					{
						wmem->wm_mat[j] = (*flt);
						flt++;
					}
				}
			}
		}
		if( nurbs )
		{
			wmem = mk_addmember( "nurb.s" , &head.l, NULL, operator[Union] );
		}
		/* Make the group named "all" */
		mk_lcomb( fdout , "all" , &head , 0 ,
			(char *)0 , (char *)0 , (unsigned char *)0 , 0 );
	}
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
