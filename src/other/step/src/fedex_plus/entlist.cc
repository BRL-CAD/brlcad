/*****************************************************************************
 * entlist.cc                                                                *
 *                                                                           *
 * Description: EntList is a class type which supports the concept of com-   *
 *              plex inheritance.  EntLists capture information on the       *
 *              allowable types of complex entities.  They represent this    *
 *              information internally and may be used to validate a user    *
 *              attempt to instantiate a complex instance.  This file con-   *
 *              tains member function definitions for EntList and for its    *
 *              SimpleList subtype.                                          *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        9/18/96                                                      *
 *****************************************************************************/

#include "complexSupport.h"

int EntList::siblings()
    /*
     * Returns the number of EntLists in this's list (EntList->next, next->next
     * etc.) including this.
     */
{
    int count;
    EntList *el;

    for ( count=1, el=next; el; count++, el= el->next )
      ;
    return count;
}

EntList *EntList::firstNot( JoinType j )
    /*
     * Returns the first EntList not of type join, starting from this.
     */
{
    EntList *sibling = this;

    while ( sibling != NULL && sibling->join == j ) {
	sibling = sibling->next;
    }
    return sibling;  // (may = NULL)
}

EntList *EntList::firstWanted( MatchType match )
    /*
     * Returns the first EntList where viable = match, starting from this.
     */
{
    EntList *sibling = this;

    while ( sibling != NULL && sibling->viable != match ) {
	sibling = sibling->next;
    }
    return sibling;  // (may = NULL)
}

EntList *EntList::lastNot( JoinType j )
    /*
     * Returns the last EntList not of type join, searching backwards from
     * this.
     */
{
    EntList *sibling = this;

    while ( sibling != NULL && sibling->join == j ) {
	sibling = sibling->prev;
    }
    return sibling;  // (may = NULL)
}

EntList *EntList::lastWanted( MatchType match )
    /*
     * Returns the last EntList where viable = match, searching backwards from
     * this.
     */
{
    EntList *sibling = this;

    while ( sibling != NULL && sibling->viable != match ) {
	sibling = sibling->prev;
    }
    return sibling;  // (may = NULL)
}

int SimpleList::isDependent( const char *ent )
    /*
     * Can we determine that ent can be instantiated independently (a Simple-
     * List could never tell us that an entity is dependent - only a AndList
     * could determine that.)
     */
{
    if ( !strcmp( name, ent ) ) {
	return FALSE;
    }
    return DONT_KNOW;
}

void SimpleList::unmarkAll( EntNode *ents )
    /*
     * Unmarks the node that was marked by this List.  Normally called when
     * undoing an OR choice to try out another.
     */
{
    EntNode *eptr = ents;
    int comp = -1;

    if ( viable < MATCHSOME ) return;

    while ( eptr != NULL && (comp = strcmp( eptr->name, name )) < 0 ) {
	eptr = eptr->next;
    }
    // (We assume we have a match now since viable >= MATCHSOME.)
    if ( eptr->mark <= I_marked ) {
	// Only unmark if we gave it the strongest mark:
	eptr->setmark( NOMARK );
    }
    // Either way (whether or not another List's mark remains), we no longer
    // marked:
    I_marked = NOMARK;
}

int SimpleList::acceptChoice( EntNode *ents )
    /*
     * Marks whichever node we can mark.  We assume there is a match because
     * this function is only called by a parent MultList if its child had a
     * viable val of MATCHSOME.  Return TRUE if we mark a previously unmarked
     * node; otherwise FALSE.
     */
{
    EntNode *eptr = ents;
    int comp;

    while ( eptr != NULL ) {
	if ( (comp = strcmp( name, eptr->name )) == 0 ) {
	    if ( ! eptr->marked() ) {
		eptr->setmark( ORMARK );
		I_marked = ORMARK;
		// Remember that we're the one who marked this.  (Nec. in case
		// we have to unmark later to try out another OR branch.)
		return TRUE;
	    }
	    return FALSE;  // we didn't mark
	}
	if ( comp < 0 ) {
	    // We're beyond name in the ents list.  No more checking to do.
	    return FALSE;
	}
	eptr = eptr->next;
    }
    return FALSE;
}
