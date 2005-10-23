/*                       S U M M A R Y . C
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
/** @file summary.c
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

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Summary()
{
	int i;
	int indep_entities=0;

	bu_log( "Summary of entity types found:\n" );
	for( i=0 ; i<=ntypes ; i++ )
	{
		if( typecount[i].count != 0 )
			bu_log( "%10d %s (type %d)\n",typecount[i].count , typecount[i].name , typecount[i].type );
	}

	for( i=0 ; i<totentities ; i++ )
	{
		int subord;

		subord = (dir[i]->status/10000)%100;
		if( !subord )
			indep_entities++;
	}
	bu_log( "%d Independent entities\n" , indep_entities );
}

void
Zero_counts()
{
	int i;

	for( i=0 ; i<=ntypes ; i++ )
		typecount[i].count = 0;
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
