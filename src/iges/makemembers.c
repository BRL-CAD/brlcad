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
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */

/*	This routine traverses a boolean tree that has been converted
	to a BRLCAD acceptable format, and creates the member records
	for the region.  The tree is traversed in LNR order.	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Makemembers( root , head )
struct node *root;
struct wmember *head;
{
	struct node *ptr;
	struct wmember *wmem;
	fastf_t *flt;
	int op=Union,entno,i;

	Freestack();
	ptr = root;
	while( 1 )
	{
		while( ptr != NULL )
		{
			Push( (union tree *)ptr );
			ptr = ptr->left;
		}
		ptr = ( struct node *)Pop();

		if( ptr == NULL )
			return;

		if( ptr->op < 0 ) /* this is an operand */
		{
			entno = (-(1+ptr->op)/2); /* entity number */

			/* make the member record */
			wmem = mk_addmember( dir[entno]->name , &(head->l), NULL, operator[op] );
			flt = (fastf_t *)dir[entno]->rot;
			for( i=0 ; i<16 ; i++ )
			{
				wmem->wm_mat[i] = (*flt);
				flt++;
			}

			/* increment the reference count */
			dir[entno]->referenced++;
		}
		else	/* this is an operator, save it*/
			op = ptr->op;

		ptr = ptr->right;

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
