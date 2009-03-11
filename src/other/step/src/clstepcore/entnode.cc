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

EntNode::EntNode( const char **names )
    /*
     * Given a list of entity names, creates a sorted linked list of EntNodes
     * corresponding to the list.  A name which = "*" is a dummy trailer
     * implying the list has been completed.  This function first builds the
     * list, then sets this to the first element, and then deletes the first
     * element.  This ensures that `this' points to the start of the list.
     */
{
    int j = 1, comp;
    EntNode *prev, *prev2 = NULL, // prev2 - the one before prev
	    *newnode, *firstnode;
    const char *nm;

    // Create a first EntNode:
    firstnode = prev = new EntNode( names[0] );

    while ( names[j] && *names[j] != '*' ) {
	nm = names[j];
	while ( prev != NULL && (comp = StrCmpIns( prev->name, nm )) < 0 ) {
	    prev2 = prev;
	    prev = prev->next;
	}

	// One exceptional case:  If new name is same as prev, skip it:
	if ( comp != 0 ) {
	    // At this point, we know the new node belongs between prev2 and
	    // prev.  prev or prev2 may = NULL if newnode belongs at the end of
	    // the list or before the beginning, respectively.
	    newnode = new EntNode( nm );
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
	// Reset for next name:
	prev = firstnode;
	prev2 = NULL;
	j++;
    }

    // Finally, place the contents of firstnode in 'this', and delete first-
    // node.  This ensures that 'this' is first.
    *this = *firstnode;
    firstnode->next = NULL;
    // (Otherwise, deleting firstnode would delete entire list.)
    delete firstnode;
}

EntNode &EntNode::operator= ( EntNode &ent )
    /*
     * Copies all of ent's values here.
     */
{
    Name( ent.name );
    setmark( ent.mark );
    multSuprs( ent.multSupers );
    next = ent.next;
    return *this;
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

EntNode *EntNode::lastSmaller( EntNode *ent )
    /*
     * Starting from `this', search for the last node (along our next's) which
     * alphabetically precedes ent.
     */
{
    EntNode *eptr = next, *prev = this;

    if ( *this > *ent ) {
	return NULL;
    }
    while ( eptr && *eptr > *prev && *eptr < *ent ) {
	prev = eptr;
	eptr = eptr->next;
    }
    return prev;
}

void EntNode::sort( EntNode **first )
    /*
     * Check if our nodes are in order.  If not, re-sort.  The earlier part of
     * the list has been sorted but not necessarily the remainder.  This algo-
     * rithm sorts our nodes in chunks, and is fairly efficient when we know
     * our nodes are basically in order.  (sort() is called from STEPcomplex::
     * Initialize() when we are creating a complex entity and have renamed a
     * few of the nodes.  The nodes at first were in order; only the renamed
     * ones may be out of place.)
     */
{
    EntNode *eptr1, *eptr2, *temp1, *temp2;

    while ( next && *this > *next ) {

	// Find the earliest node greater than next.  (I.e., is not only this >
	// next but also some of this's preceding nodes.  Since the nodes
	// earlier in the list are sorted already, we can easily determine the
	// ordered chunk of nodes which next should precede.)  (eptr1 is
	// actually set to the one before, so that we'll know that eptr1->next
	// should now be set to next.)
	eptr1 = (*first)->lastSmaller( next );

	// Take the latest node eptr1 is greater than.  (I.e., beyond next are
	// there more nodes which should be immediately after eptr1.)  (The
	// succeeding nodes are not necessarily in order, so lastSmaller() also
	// checks that nodes later than next are also > their prev's.)
	if ( eptr1 ) {
	    eptr2 = next->lastSmaller( eptr1->next );
	} else {
	    eptr2= next->lastSmaller( *first );
	}

	// Switch the two lists:
	if ( eptr1 ) {
	    temp1 = eptr1->next;
	    eptr1->next = next;
	    temp2 = eptr2->next;
	    eptr2->next = temp1;
	    next = temp2;
	} else {
	    // `this' should be first in the list.
	    temp2 = eptr2->next;
	    eptr2->next = *first;
	    *first = next;
	    next = temp2;
	}
    }
	    
    if ( next ) {
	next->sort( first );
    }
}
