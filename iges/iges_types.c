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
 *	This software is Copyright (C) 1995 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "./iges_struct.h"
#include "./iges_extern.h"

char *
iges_type( type )
int type;
{
	int i;
	int type_no=0;

	for( i=1 ; i<ntypes ; i++ )
	{
		if( typecount[i].type == type )
		{
			type_no = i;
			break;
		}
	}

	return( typecount[type_no].name );
}
