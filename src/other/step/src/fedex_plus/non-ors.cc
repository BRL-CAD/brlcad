/*****************************************************************************
 * non-ors.cc                                                                *
 *                                                                           *
 * Description: Contains the matchNonORs() functions for the EntList descen- *
 *              dants.  matchNonORs() functions loop through the EntList     *
 *              children of this whose JoinType != OR (and check a Simple-   *
 *              List itself) to determine how well they match an EntNode.    *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        10/17/96                                                     *
 *****************************************************************************/

#include "complexSupport.h"

MatchType SimpleList::matchNonORs( EntNode *ents )
    /*
     * Checks if we match the nodes of ents.  If only one unmarked is left
     * and we match it, return MATCHALL.  More likely, we'll return one of the
     * other return values.  (See descript of MatchType values in complex-
     * Support.h.)
     */
{
    EntNode *eptr = ents;
    int comp;

    while ( eptr != NULL ) {
	if ( (comp = strcmp( name, eptr->name )) == 0 ) {
	    if ( ! eptr->marked( MARK ) ) {
		// NOTE - this cond also returns TRUE if eptr did have an OR-
		// MARK.  We don't want to remark now (since we're also trying
		// out OR choices -- we know this because no OR's are done
		// until after all the non-OR's).  But, we do want to return
		// MATCHSOME, as below.  Since The prev OR which marked this
		// may one time later try another path, we want to record that
		// our OR can also mark it.  So we return MATCHSOME saying
		// this is a viable option we may one time want to try.
		if ( eptr->mark == NOMARK ) {
		    eptr->setmark();
		    I_marked = MARK;
		    // Remember that we're the one who marked this.  (Nec. in
		    // case we have to unmark later to try out another OR
		    // branch.)
		    if ( ents->allMarked() ) {
		        // If this was the only unmarked left,
			viable = MATCHALL;
		        return MATCHALL;
		    }
		}
		viable = MATCHSOME;
		return MATCHSOME;
	    }
	    viable = SATISFIED;
	    return SATISFIED;
	    // Couldn't mark any more, but at least we're not placing a re-
	    // quirement ents couldn't meet.
	}
	if ( comp < 0 ) {
	    // We're beyond name in the ents list.  No more checking to do.
	    break;
	}
	eptr = eptr->next;
    }

    // At this point, we went through the list without finding a match.  Result
    // = UNSATISFIED - no match.
    viable = UNSATISFIED;
    return UNSATISFIED;
}

MatchType AndOrList::matchNonORs( EntNode *ents )
    /*
     * Loop through the children of this matching as many of the nodes of ents
     * as we can.  We skip all OrList descendants.  Those are processed later
     * only after all the non-conditional descendants, so that when we start
     * processing them we'll be able to tell which OR choices are viable, and
     * which are unnec.
     */
{
    EntList *child = childList->firstNot( OR );
    MatchType retval;

    while ( child != NULL ) {
	if ( (retval = child->matchNonORs( ents )) == MATCHALL ) {
	    if ( prevKnown( child ) ) {
		viable = MATCHALL;
		return MATCHALL;
		// We found a good solution.  Nothing else to do.  (Some higher
		// AND may have some other req's ents can't meet, but that's
		// not our problem.  If that happens, AND will return UNSATIS-
		// FIED, or an even higher OR will undo all and will try
		// another path.)
		// The prevKnown() fn is used to check that we haven't come
		// across any UNKNOWNs yet.  If we have, we can't be sure the
		// MATCHALL is valid.  An illustration:  One of our previous
		// children was an AND who had a SIMPLE child and an OR child.
		// The SIMPLE child matched one EntNode.  The OR child is
		// UNKNOWN, since we haven't matchOR()'ed yet.  Possibly, our
		// retval of MATCHALL is in part due to the SIMPLE child of
		// the AND.  But if the OR child of the AND returns UNSAT when
		// we run matchOR(), we'll unmark the SIMPLE child too.  Thus,
		// we can only be sure of a MATCHALL if nothing is UNKNOWN.
		// (Note that future UNKNOWN children don't bother us (in fact
		// all future children are UNKNOWN by default since they have
		// not been touched).  This is because we're an ANDOR and can
		// skip them altogether.  But previous children we have visi-
		// ted already, and we can't ignore, as above.  Compare to 
		// AndOrList::matchORs(), where since all children had been
		// visited already in matchNonORs(), we were not able to stop
		// in process as here at all.)
	    }
	} else if ( retval == UNSATISFIED ) {
	    // Unmark whatever we may have marked.  (E.g., there may have
	    // been an AND beneath and it started marking and then found one
	    // it couldn't match.)
	    child->unmarkAll( ents );
	}
	child = child->nextNot( OR );
    }
    setViableVal( ents );
    return viable;
}

MatchType AndList::matchNonORs( EntNode *ents )
    /*
     * Checks if the AndList contains the set of nodes in ents.  Skip OrList
     * descendants.
     */
{
    EntList *child = childList->firstNot( OR );

    while ( child != NULL ) {
	if ( child->matchNonORs( ents ) == UNSATISFIED ) {
	    viable = UNSATISFIED;
	    return UNSATISFIED;
	    // This means the whole AndList has failed, by definition.
	}
	child = child->nextNot( OR );
	// Note - we loop through all even if one of our children returned
	// MATCHALL.  Since we're an AND, we must look through all branches -
        // to search for any other conditions we can't meet.  If one of our
	// children did MATCHALL, its viable val will be set to MATCHALL and
	// we'll catch it in setViableVal() called below.
    }
    setViableVal( ents );
    return viable;
}
