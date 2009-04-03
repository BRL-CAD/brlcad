/*****************************************************************************
 * match-ors.cc                                                              *
 *                                                                           *
 * Description: Contains the matchORs() functions for the EntList descen-    *
 *              dants.  The matchORs() of AndOrList and AndList recursively  *
 *              loop through their non-SIMPLE EntList children to determine  *
 *              how well their OR descendants match an EntNode.  OrList's    *
 *              matchORs() function checks its children one at a time for    *
 *              both complete and incomplete matches of EntNode and stores   *
 *              the resulting information.                                   *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        10/17/96                                                     *
 *****************************************************************************/

#include "complexSupport.h"

MatchType AndOrList::matchORs( EntNode *ents )
    /*
     * Loops through descendants of this, invoking their matchOR functions.
     * Returns the status of how well this's OR descendants match the nodes of
     * ents.  We only check our children whose viable values = UNKNOWN.  This
     * signifies that when we checked them previously with matchNonORs(), we
     * couldn't determine their match value.  And that happens when there are
     * OR descendants we didn't test.  Thus, UNKNOWN tells us that this child
     * is an OR, or has an OR somewhere beneath it which we must process now.
     */
{
    EntList *child = childList->firstWanted( UNKNOWN );

    while ( child != NULL ) {
	if ( ((MultList *)child)->matchORs( ents ) == UNSATISFIED ) {
	    // Unmark whatever we may have marked.  (E.g., there may have
	    // been an AND beneath and it started marking and then found one
	    // it couldn't match.)
	    child->unmarkAll( ents );
	}
	child = child->nextWanted( UNKNOWN );
    }
    // NOTE - We went through entire loop above even if we found a MATCHALL
    // sometime in the middle.  After finding a bug, I realized we couldn't
    // stop in the middle.  So long as there are more UNKNOWN children, one
    // of those children may become UNSAT later and we'll have to unmark all
    // its descendants.  If so, some of the marks we have now may disappear.
    setViableVal( ents );
    return viable;
}

MatchType AndList::matchORs( EntNode *ents )
    /*
     * Loops through the descendants of this with viable val = UNKNOWN, invo-
     * king their matchOR functions.  Returns the status of how well this's OR
     * descendants match the nodes of ents.  We only take UNKNOWN's because
     * they will lead us to OR's, as explained in AndOrList::matchORs().
     */
{
    EntList *child = childList->firstWanted( UNKNOWN );

    while ( child != NULL ) {
	if ( ((MultList *)child)->matchORs( ents ) == UNSATISFIED ) {
	    viable = UNSATISFIED;
	    return UNSATISFIED;
	    // This means the whole AndList has failed, by definition.
	}
	child = child->nextWanted( UNKNOWN );
	// Note - we loop through all even if one of our children returned
	// MATCHALL.  Since we're an AND, we must look through all branches -
        // to search for any other conditions we can't meet.  If one of our
	// children did MATCHALL, its viable val will be set to MATCHALL and
	// we'll catch it in setViableVal() called below.
    }
    setViableVal( ents );
    return viable;
}

MatchType OrList::matchORs( EntNode *ents )
    /*
     * Checks the branches of an OrList to search for a match to the nodes of
     * ents.  This function searches this's children and marks all the viable
     * paths, or paths which mark off at least some of the remaining nodes of
     * ents.  It then selects the first one, marking it only with an ORMARK to
     * retain the info that these were only conditionally marked.  Also, if a
     * MATCHALL solution was found, that is returned immediately.
     */
{
    int count;
    EntList *child = childList;
    MatchType retval;

    for ( count=0; count<numchildren; count++, child = child->next ) {

	// First call (recursively) matchNonORs() to check off all nodes that
	// the descendants of this branch can definitely mark off:
	if ( child->join != OR ) {
	    retval = child->matchNonORs( ents );
	}

	// Then try the OR's.  At this point, any OR's that we get to (in
	// recursively checking the descendants of child) will know that if
	// it can mark new node(s), it's a viable option.
	if ( child->viable == UNKNOWN ) {
	    // If viable = UNKNOWN, this child must either be an OR or a Mult
	    // with an OR underneath.  Only ORs are still indeterminate after
	    // running matchNonORs() above.  (We also exclude the case of an
	    // AND child who may have OR desc's, but already determined that
	    // it can't satisfy one of its paths and so returned UNSAT.)
	    retval = ((MultList *)child)->matchORs( ents );
	}

	// Now register the result:
	if ( retval >= MATCHSOME ) {
	    // Note:  In the past I would return immediately if retval =
	    // MATCHALL, thinking our job was done.  I changed it when we
	    // started dealing with combo-CLists (sub w/ >1 super).  I realized
	    // that even if down here we got a MATCHALL, we may have to reject
	    // above, so we must keep searching.
	    if ( choice == -1 ) {
		choice1 = choice = count;
	    }
	    choiceCount++;
	    if ( viable < retval ) {
		viable = retval;
	    }
	} else {
	    // At this point, retval must = SATISFIED or UNSAT.  Neither
	    // result gives us a possible solution since neither marks any
	    // new nodes.  He we keep track of the best retval we've gotten
	    // between the two remaining values.  It's important to know if
	    // retval = SATISFIED.  In case there is no better result, this
	    // Will cause us to tell our parent that we have at least one
 	    // satisfactory path.  Thus, if our parent is an AND, it'll know
	    // that this branch doesn't violate anything.
	    if ( viable < retval ) {
		viable = retval;
	    }
	}

	// Undo this choice before we try the next:
	child->unmarkAll( ents );
    }

    // Accept the first viable solution, if there is one:
    if ( viable >= MATCHSOME ) {
	// If there are some MATCHSOME solutions, accept the first.  accept-
	// Choice() begins by accepting the child at "choice".  But if this
	// does not mark anything new, it loops until it finds a choice that
	// does mark (if there is one).  There's no point marking any earlier
	// ones.  There may be earlier children which are set to MATCHSOME
	// because they *may* mark (since they match nodes which are only
	// conditionally marked).  But now we're looking for a child which
	// *actually* marks under the current circumstances.
	acceptChoice( ents );
    }
    if ( viable == MATCHALL ) {
	return getChild( choice1 )->viable;
	// viable == MATCHALL because we found a MATCHALL sol'n along the way,
	// but that wasn't necessarily the choice acceptChoice() took now.
	// (See note above why we don't drop everything and just accept the
	// MATCHALL.)
    } else {
	return viable;
    }
}
