/*****************************************************************************
 * orlist.cc                                                                 *
 *                                                                           *
 * Description: OrList is a specialized class type which supports OR lists.  *
 *              This file contains the OrList member functions, which pro-   *
 *              vide the capability to undo previous choices, and select new *
 *              ones.                                                        *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        9/18/96                                                      *
 *****************************************************************************/

#include "complexSupport.h"
#include "sc_memmgr.h"

/**
 * Check if we matched nm.  We have two possibilities here:  If we have a
 * choice selected, we only check the selected choice.  Say we're an OR
 * between SimpleLists C and D, and nm = C.  If choice = D, we can't check
 * C.  This is because we're already using this OR to match D, and it can
 * not match C.  If however, this OR doesn't have any choice selected (it
 * = SATISFIED), we can check all our children.  Since we don't need to
 * have a particular choice selected (D), so long as we could be used to
 * match C, we potentially have a match.  (This all makes sense in the
 * context of a sub w/ >1 super, in which case we build a combo-CList and
 * may need to check if all sub-CLists matched the multi-sub, C.)
 */
bool OrList::hit( char * nm ) {
    EntList * child = getChild( choice );

    if( child ) {
        // I.e., if we have a choice selected, check it only.
        return ( child->hit( nm ) );
    } else {
        child = childList;
        while( child ) {
            if( child->viable > UNSATISFIED && child->hit( nm ) ) {
                // See MultList::hit() on why we must skip UNSATs.
                return true;
            }
            child = child->next;
        }
    }
    return false;
}

/**
 * Unmarks all the nodes of ents marked by the descendants of this.
 */
void OrList::unmarkAll( EntNode * ents ) {
    EntList * child;

    if( ( child = getChild( choice ) ) != NULL ) {
        // choice = the last selected path which we'll now undo.
        child->unmarkAll( ents );
    }
}

/**
 * Accepts the first choice of our childList which marks any unmarked
 * nodes.  If none of our current choices mark anything, choice is set to
 * LISTEND.  If choice was set to LISTEND before calling aC(), we reset
 * choice to choice1, and search again.
 */
bool OrList::acceptChoice( EntNode * ents ) {
    EntList * child;

    if( choice == LISTEND ) {
        choice = choice1;
    }
    child = getChild( choice );
    while( child ) {
        if( child->viable >= MATCHSOME && child->acceptChoice( ents ) ) {
            // acceptChoice() returns true if we marked something.
            return true;
        }
        child = child->next;
        choice++;
    }
    // If we got here, we must have gotten to the end of the childList without
    // finding a choice which marks something.
    choice = LISTEND;
    return false;
}
