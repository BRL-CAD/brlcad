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

/*	This routine finds union operators in the boolean tree
	that cannot be handled by BRLCAD (due to BRLCAD's odd
	way of handling the union operator), and moves them
	toward the root of the tree.  To get the tree into
	a BRLCAD-acceptable format, no intersection or
	subtraction operators can be above any union operator
	in the tree.  Traverses the tree in LRN order.  Returns 1
	if no change was made to the tree, 0 otherwise.	*/

#include "./iges_struct.h"

Bubbleup( root )
struct node *root;
{
	struct node *Copytree(),*Pop(),*ptr,*ptra,*ptrb,*ptrc,*ptr1,*ptr2;
	int op,retval=1;

	ptr = root;
	while( 1 )
	{
		while( ptr != NULL )
		{
			Push( ptr );
			ptr = ptr->left;
		}
		ptr = Pop();
		if( ptr->right != NULL && ptr->right->op == Union )
		{
			if( ptr->op == Subtract )
			{
			/*	(a-(buc)) => ((a-b)-c)	*/
				retval = 0;
				ptr1 = ptr->right;
				ptra = ptr->left;
				ptrb = ptr->right->left;
				ptrc = ptr->right->right;
				ptr->right = ptrc;
				ptr->left = ptr1;
				ptr1->op = Subtract;
				ptr1->left = ptra;
				ptr1->right = ptrb;
				ptra->parent = ptr1;
				ptrb->parent = ptr1;
				ptrc->parent = ptr;
			}
			else if( ptr->op == Intersect )
			{
			/*	(a+(buc)) => (a+b)u(a+c)	*/
				retval = 0;
				ptr2 = (struct node *)bu_malloc( sizeof( struct node ), "Bubbleup: ptr2" );
				ptr1 = ptr->right;
				ptra = ptr->left;
				ptrb = ptr->right->left;
				ptrc = ptr->right->right;
				ptr->left = ptr2;
				ptr2->op = Intersect;
				ptr2->left = ptra;
				ptr2->right = ptrb;
				ptr1->right = ptrc;
				ptr1->op = Intersect;
				ptr->op = Union;
				ptr1->left = Copytree( ptra , ptr1 );
				ptra->parent = ptr2;
				ptrb->parent = ptr2;
				ptr2->parent = ptr;
			}
		}
		else if( ptr->left != NULL && ptr->left->op == Union )
		{
			if( ptr->op == Intersect || ptr->op == Subtract )
			{
			/*	((aub)"+ or -"c) => (a"+ or -"c)u(b"+ or -"c)	*/
				retval = 0;
				op = ptr->op;
				ptra = ptr->left->left;
				ptrb = ptr->left->right;
				ptrc = ptr->right;
				ptr1 = ptr->left;
				ptr2 = (struct node *)bu_malloc( sizeof( struct node ), "Bubbleup: ptr2" );
				ptr->op = Union;
				ptr->right = ptr2;
				ptr2->op = op;
				ptr2->left = ptrb;
				ptr2->right = Copytree( ptrc , ptr2 );
				ptr1->op = op;
				ptr1->right = ptrc;
				ptrc->parent = ptr1;
				ptrb->parent = ptr2;
				ptr2->parent = ptr;
			}
		}

		if( ptr == root ) /* entire tree has been looked at */
			return( retval );

		if( ptr != ptr->parent->right ) /* we must be at the left node */
			ptr = ptr->parent->right; /* so push the right node */
		else				/* we must be at the right node */
			ptr = NULL;	/* so don't push anything */

	}
}
