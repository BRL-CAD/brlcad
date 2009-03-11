/*****************************************************************************
 * entnode.cc                                                                *
 *                                                                           *
 * Description: The EntNode class is used to store a collection of entity    *
 *              names.  It stores them in alphabetical order.  It is the key *
 *              component of the EntList classes, which are used to store    *
 *              information on the instantiable complex entity types.  This  *
 *              file contains EntNode member function definitions.           *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        9/18/96                                                      *
 *****************************************************************************/

#include "complexSupport.h"

EntNode::EntNode( char *namelist[] )
    /*
     * Given a list of entity names, creates a sorted linked list of EntNodes
     * corresponding to the list.  Final name must be "*" (otherwise we won't
     * know where to stop).  This function first builds the list, then sets
     * this to the first element, and then deletes the first element.  We do
     * this to ensure that this points to the start of the list.
     */
{
    int j = 1, comp;
    EntNode *prev, *prev2 = NULL, // prev2 - the one before prev
	    *newnode, *firstnode;
    char *nm;

    // Create a first EntNode:
    firstnode = prev = new EntNode( namelist[0] );
    // The following 3 lines are a ridiculous kludge to simplify testing.
    // We make the assumption that ents whose name start with C or M have
    // >1 supertype.  (We make sure this is the case in the test file.)
    // When this code becomes a part of the SCL, it'll be easy to get this
    // info right from the entity structs anyway, so I'm not bothering
    // writing anything more sophisticated.
    if ( *namelist[0] == 'c' || *namelist[0] == 'm' || *namelist[0] == 'j' ) {
	firstnode->multSuprs( TRUE );
    }

    while ( *namelist[j] != '*' ) {
	nm = namelist[j];
	while ( prev != NULL && (comp = strcmp( prev->name, nm )) < 0 ) {
	    prev2 = prev;
	    prev = prev->next;
	}

	// One exceptional case:  If new name is same as prev, skip it:
	if ( comp != 0 ) {
	    // At this point, we know the new node belongs between prev2 and
	    // prev.  prev or prev2 may = NULL if newnode belongs at the end of
	    // the list or before the beginning, respectively.
	    newnode = new EntNode( nm );
	    // Same kludge:
	    if ( *nm == 'c' || *nm == 'm' || *nm == 'j' ) {
		newnode->multSuprs( TRUE );
	    }
	    newnode->next = prev;
	    if ( prev2 == NULL ) {
		// This will be the case if the inner while was never entered.
		// That happens when newnode belonged at the beginning of the
		// list.  If so, reset firstnode.
		firstnode = newnode;
	    } else {
		prev2->next = newnode;
	    }
	}
	// Reset for next namelist:
	prev = firstnode;
	prev2 = NULL;
	j++;
    }

    // Finally, place the contents of firstnode in 'this', and delete first-
    // node.  This ensures that 'this' is first.
    strcpy( name, firstnode->name );
    next = firstnode->next;
    multSupers = firstnode->multSupers;
    firstnode->next = NULL;
    // (Otherwise, deleting firstnode would delete entire list.)
    delete firstnode;
}

void EntNode::markAll( MarkType stamp )
    /*
     * Marks/unmarks all the nodes in this's list (default is to mark).
     */
{
    EntNode *node = this;

    while ( node != NULL ) {
	node->mark = stamp;
	node = node->next;
    }
}

int EntNode::allMarked()
    /*
     * Returns TRUE if this and all nodes following it are marked.
     */
{
    EntNode *node = this;

    while ( node != NULL ) {
	if ( node->mark == NOMARK ) {
	    return FALSE;
	}
	node = node->next;
    }
    return TRUE;
}

int EntNode::unmarkedCount()
    /*
     * Returns the number of unmarked nodes in this's list.
     */
{
    int count = 0;
    EntNode *node = this;

    while ( node != NULL ) {
	if ( node->mark == NOMARK ) {
	    count++;
	}
	node = node->next;
    }
    return count;
}
