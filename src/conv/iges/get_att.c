/*                       G E T _ A T T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
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
/** @file get_att.c
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

/*	This routine loops through all the directory entries and looks
 *	at attribute definition entities, searching for a BRLCAD
 *	attribute definition
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Get_att()
{
	int i,j;
	char *str;

	for( i=0 ; i<totentities ; i++ )
	{
		/* Look for attribute definitions */
		if( dir[i]->type == 322 )
		{
			/* look for form 0 only */
			if( dir[i]->form )
				continue;

			Readrec( dir[i]->param );
			Readint( &j , "" );
			if( j != 322 )
			{
				bu_log( "Parameters at sequence %d are not for entity at DE%d\n" , dir[i]->param , (2*i+1) );
				continue;
			}

			Readname( &str , "" );
			if( !strncmp( str , "BRLCAD" , 6 ) )
			{
				/* this is what we have been looking for */
				brlcad_att_de = 2*i+1;
				return;
			}
		}
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
