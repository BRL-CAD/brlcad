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
/*	This routine manipulates the boolean tree towards the form
	that BRLCAD likes, i.e. (((a+b)-c)+d) as opposed to (a+(b-(c-d))).
	The tree is traversed in LRN order.	*/

#include "./iges_struct.h"

Arrange( root )
struct node *root;
{
	struct node *Copytree(),*Pop(),*ptr,*ptra,*ptrb,*ptrc,*ptrd,*ptr1,*ptr2;
	int retval=1;

	ptr = root;
	while( 1 )
	{
		while( ptr != NULL )
		{
			Push( ptr );
			ptr = ptr->left;
		}
		ptr = Pop();
		if( ptr->op == Subtract )
		{
			if( ptr->right->op == Subtract || ptr->right->op == Intersect )
			{
			/*	(a-(b"+ or-"c)) => ((a-b)u(a"+ or -"c))	*/
				retval = 0;
				ptra = ptr->left;
				ptrb = ptr->right->left;
				ptrc = ptr->right->right;
				ptr1 = ptr->right;
				ptr2 = (struct node *)bu_malloc( sizeof( struct node ), "Arrange: ptr2" );
				ptr->left = ptr2;
				ptr1->left = ptra;
				ptr2->left = Copytree( ptra , ptr2 );
				ptr2->right = ptrb;
				ptr->op = Union;
				if( ptr1->op == Intersect )
					ptr1->op = Subtract;
				else
					ptr1->op = Intersect;
				ptr2->op = Subtract;
				ptr2->parent = ptr;
				ptrb->parent = ptr2;
				ptra->parent = ptr1;
			}
		}
		if( ptr->op == Intersect && ptr->left->op > Union && ptr->right->op > Union )
		{
		/*	(a"+ or -"b)+(c"+ or -"d) => (((a+c)"+ or -"b)"+ or -"d)	*/
			retval = 0;
			ptra = ptr->left->left;
			ptrb = ptr->left->right;
			ptrc = ptr->right->left;
			ptrd = ptr->right->right;
			ptr1 = ptr->left;
			ptr2 = ptr->right;
			ptr->left = ptr2;
			ptr->right = ptrd;
			ptr2->left = ptr1;
			ptr2->right = ptrb;
			ptr1->right = ptrc;
			ptr->op = ptr2->op;
			ptr2->op = ptr1->op;
			ptr1->op = Intersect;
			ptrb->parent = ptr2;
			ptr1->parent = ptr2;
			ptrc->parent = ptr1;
			ptrd->parent = ptr;
		}
		else if( ptr->op == Intersect )
		{
			if( ptr->right->op > Union )
			{
			/*	(a+(b"+ or -"c)) => ((b"+ or -"c)+a)	*/
				retval = 0;
				ptra = ptr->left;
				ptr->left = ptr->right;
				ptr->right = ptra;
			}
		}

		if( ptr == root )
			return( retval );

		if( ptr != ptr->parent->right )
			ptr = ptr->parent->right;
		else
			ptr = NULL;

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
