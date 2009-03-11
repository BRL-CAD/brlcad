/*****************************************************************************
 * trynext.cc                                                                *
 *                                                                           *
 * Description: Contains the tryNext() functions for the EntList descen-     *
 *              dants.  Their purpose is to search for additional branches   *
 *              of OrList's to try the next available untried solution.  AND *
 *              and ANDOR iterate through their children looking for OR's.   *
 *              OR tries the next branch of its children if more remain.     *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        10/24/96                                                     *
 *****************************************************************************/

#include "complexSupport.h"

// Local function prototypes:
static EntList *firstCandidate( EntList * );
static EntList *nextCandidate( EntList * );

MatchType MultList::tryNext( EntNode *ents )
    /*
     * Loops backwards through the children of this, recursively searching for
     * alternate solutions (i.e., OR's which have alternate paths we haven't
     * tried yet).  We loop through all MultList type children which may have
     * an OR with additional choices.  (Which children to try is determined in
     * a local function.  We loop backwards because this is the reverse of the
     * order we set the OR's.  Later choices must be redone before earlier
     * (reasons discussed in notes, 10/17).  This function is the tryNext()
     * for AND and ANDOR; the OR version is redefined.
     */
{
    MatchType retval;
    EntList *child = getLast();

    child = firstCandidate( child );
    while ( child != NULL ) {
	if ( (retval = ((MultList *)child)->tryNext( ents )) == MATCHALL ) {
	    // We're done - a good solution was found.
	    return MATCHALL;
	}
	if ( retval == NEWCHOICE ) {
	    // If a new viable choice was found below, we must now reset all
	    // later OR's to their first choice.  (That's what acceptChoice()
	    // does when choice = LISTEND.)  This is necessary so that we can
	    // exhaust every possibility with this new choice.  To do this we
	    // first reset all our children, and then return NEWCHOICE so that
	    // our parent (if exists) will also know to reset all its later OR
	    // children.
	    while ( (child = nextCandidate( child )) != NULL ) {
		if ( child->acceptChoice( ents ) && ents->allMarked() ) {
		    return MATCHALL;
		}
	    }
	    return NEWCHOICE;
	}
	child = firstCandidate( child->prev );
    }
    // If we got here, we didn't find any new OR choices:
    return NOMORE;
}

static EntList *firstCandidate( EntList *child )
    /*
     * Finds an EntList from child's list which may have an OR with more
     * choices below it.  The acceptable choices are described in commenting
     * below.
     */
{
    EntList *ent = child->lastNot( SIMPLE );

    while ( ent != NULL ) {
	if ( ent->viableVal() >= MATCHSOME ) {
	    // Return any non-SIMPLE ent where viable >= MATCHSOME.  We even
	    // want to check an OR where numChoices = 1, because it may have
	    // an OR descendant with more choices.
	    return ent;
	}
	ent = ent->prevNot( SIMPLE );
    }
    return ent;
}

static EntList *nextCandidate( EntList *child )
    /*
     * Same as prev function, searches forwards from ent after child.
     */
{
    EntList *ent = child->nextNot( SIMPLE );

    while ( ent != NULL ) {
	if ( ent->viableVal() >= MATCHSOME ) {
	    return ent;
	}
	ent = ent->nextNot( SIMPLE );
    }
    return ent;
}

MatchType OrList::tryNext( EntNode *ents )
    /*
     * Tries out the next choice of this.  Basic algorithm is to first recurse
     * to check for other solutions in the descendants of the current choice,
     * and then to try our next choice.
     */
{
    MatchType retval;
    EntList *child;


    if ( choice == LISTEND ) {
	// if we've already exhausted all the choices in this OR,
	return NOMORE;
    }

    // First try other choices of descendants of current choice:
    child = getChild( choice );
    if ( child->multiple() ) { 
	// I.e., if there are (or may be) more choices within the current
	// choice, try those first.  We must be sure to exhaust all choices in
	// our descendants before moving on.
	retval = ((MultList *)child)->tryNext( ents );
	if ( retval == MATCHALL ) {
	    return MATCHALL;
	}
	if ( retval == NEWCHOICE ) {
	    // I.e., we found a next choice to go to, return so that the
	    // EntLists on the higher levels (if there are) can retry all the
	    // later choices with the new choice we just found.  Otherwise,
	    // we'll continue below looking into our next choice.
	    return NEWCHOICE;
	}
    }

    // No other choices among our descendants.  Look for new choice at our
    // level:
    child->unmarkAll( ents );
    if ( choiceCount == 1 ) {
	// Quick way to determine that there won't be any more choices here.
	// (Also, it's nec. to unmark now, as we did above before returning and
	// before the calling tryNext() tries earlier OR's - see notes, 11/12.)
	choice = LISTEND;
	return NOMORE;
    }

    // Otherwise, try our next:
    if ( acceptNextChoice( ents ) ) {
	if ( ents->allMarked() ) {
	    return MATCHALL;
	}
	return NEWCHOICE;
    } else {
	// Must have been no next choice (or none which mark anything new).
	// acceptNextChoice() has set choice to LISTEND.  We leave this OR
	// unset and return NOMORE.  If our calling function finds another
	// choice elsewhere, it will reloop forwards and set all later OR's to
	// their first choices, so that we'll be able to test every possibility
	// with the new choice.  At that time, this OrList will be reset to its
	// first choice too.	
	return NOMORE;
    }
}
