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

#include <stdio.h>
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "wdb.h"

Convtree()
{

	int i,notdone=2,conv=0,tottrees=0;
	struct node *ptr,*Readtree(),*oldptr,*Copytree();

	printf( "\nConverting boolean tree entities:\n" );

	for( i=0 ; i<totentities ; i++ ) /* loop through all entities */
	{
		if( dir[i]->type != 180 )	/* This is not a tree */
			continue;

		tottrees++;

		if( dir[i]->param <= pstart )	/* Illegal parameter address */
		{
			printf( "Entity number %d (Boolean Tree) does not have a legal parameter pointer\n" , i );
			continue;
		}

		Readrec( dir[i]->param ); /* read first record into buffer */
		ptr = Readtree(); /* construct the tree */

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

			head.wm_forw = &head;
			head.wm_back = &head;


			/* make member records */
			Makemembers( ptr , &head );

			/* Make the object (Not using regions to take advantage of nesting) */
			if( dir[i]->colorp != 0 )
				mk_lcomb( fdout , dir[i]->name , &head , 0 , (char *)NULL , (char *)NULL , dir[i]->rgb , 1 );
			else
				mk_lcomb( fdout , dir[i]->name , &head , 0 , (char *)NULL , (char *)NULL , (char *)0 , 1 );

			conv++;
		}
		else
		{
			printf( "'%s'Tree cannot be converted to BRLCAD format\n",dir[i]->name );
			printf( "\tOriginal tree from IGES file:\n\t" );
			Showtree( oldptr );
			printf( "\tAfter attempted conversion to BRLCAD format:\n\t" );
			Showtree( ptr );
		}

		Freetree( ptr );
		Freetree( oldptr );
	}
	printf( "Converted %d trees successfully out of %d total trees\n", conv , tottrees );
}
