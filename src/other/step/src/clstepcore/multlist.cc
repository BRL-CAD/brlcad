/*****************************************************************************
 * multlist.cc                                                               *
 *                                                                           *
 * Description: MultList is a class type derived from EntList which is the   *
 *              parent class to the compound list types, AND, OR and ANDOR.  *
 *              It supports the concepts and functionality which is shared   *
 *              by all compound types, especially And- and AndOr-List's.     *
 *              This file contains many of the MultList and JoinList member  *
 *              functions.                                                   *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        9/18/96                                                      *
 *****************************************************************************/

#include "complexSupport.h"

MultList::~MultList()
    /*
     * Deletes the childList of this, before this is deleted.
     */
{
    EntList *child = childList, *cnext;

    while ( child ) {
	cnext = child->next;
	delete child;
	child = cnext;
    }
}

void MultList::setLevel( int l )
    /*
     * Sets this's level, and tells all its children to set their level to our
     * level +1.
     */
{
    EntList *child = childList;

    level = l;
    for ( ; child != NULL; child = child->next ) {
	child->setLevel( l+1 );
    }
}

int MultList::contains( char *nm )
    /*
     * Check if one of this's descendants matches nm.
     */
{
    EntList *child = childList;

    while ( child ) {
        if ( child->contains( nm ) ) return TRUE;
        child = child->next;
    }
    return FALSE;
}

int MultList::hit( char *nm )
    /*
     * Check if one of our descendants matches nm.
     */
{
    EntList *child = childList;

    while ( child ) {
        if ( child->viable > UNSATISFIED && child->hit( nm ) ) {
	    // For most child->join types ruling out UNSATs just saves us
	    // trouble - we know nm won't be hit since child didn't hit any-
	    // thing.  If child->join = AND, we must skip child.  One of its
	    // children may "hit" nm, but it wouldn't be valid because overall
	    // child is an UNSAT.
	    return TRUE;
	}
        child = child->next;
    }
    return FALSE;
}

EntList *MultList::getChild( int num )
    /*
     * Returns a pointer to the num'th child of MultList.
     */
{
    EntList *child = childList;
    int j;

    if ( num < 0 || num >= numchildren ) {
	// Check for error situations (shouldn't normally occur):
	return NULL;
    }
    for ( j=0; j<num; j++, child = child->next )
        ;
    return child;
}

void MultList::appendList( EntList *ent )
    /*
     * Appends a new entry into this's childList.  The siblings of ent (ent->
     * next ...) are automatically also appended.
     */
{
    EntList *eprev;

    if ( numchildren == 0 ) {
	childList = ent;
    } else {
	eprev = getLast();
	eprev->next = ent;
	ent->prev = eprev;
    }
    numchildren += ent->siblings();
}

EntList *MultList::copyList( EntList *ent )
    /*
     * Makes a copy of ent (and its children if it's a MultList) and appends it
     * to the end of our list.
     */
{
    EntList *newlist, *child;

    switch ( ent->join ) {
      case SIMPLE:
	newlist = new SimpleList( ((SimpleList *)ent)->Name() );
	break;
      case AND:
	newlist = new AndList;
	break;
      case OR:
	newlist = new OrList;
	break;
      case ANDOR:
	newlist = new AndOrList;
	break;
    };
    appendList( newlist );
    if ( ent->multiple() ) {
	// For the multlists, we must recurse for all their children:
	child = ((MultList *)ent)->childList;
	while ( child ) {
	    ((MultList *)newlist)->copyList( child );
	    child = child->next;
	}
    }
    return newlist;
}

void MultList::unmarkAll( EntNode *ents )
    /*
     * Unmarks all nodes of ents marked by any of the descendants of this.
     * This function is invoked by AndList and AndOrList.  It is redefined for
     * OrList.
     */
{
    EntList *child = childList;

    while ( child != NULL ) {
	child->unmarkAll( ents );
	child = child->next;
    }
}

void MultList::reset()
    /*
     * Resets this to default values.  Iterates through child list, calling
     * each child's reset function.
     */
{
    EntList *child;

    viable = UNKNOWN;
    for ( child = childList; child; child = child->next ) {
	child->reset();
    }
}

void JoinList::setViableVal( EntNode *ents )
    /*
     * Sets this's viable value based on the value of its children.  This is
     * called at the end of matchNonOR() and matchOR() to determine the result
     * of the matching.  Since matchNonOR() is always called first and it
     * skips OR children, some of the children may still be UNKNOWN.  In that
     * case, we still don't know the overall result and set viable to UNKNOWN.
     * As an important note, for an AndList the match fns themselves check for
     * children which are UNSATISFIED and return UNSAT if found, we don't
     * worry about coming across them down here.
     */
{
    EntList *child = childList;

    viable = UNKNOWN;
    // Start viable at UNKNOWN.  This is default val and the lowest enum val.

    while ( child != NULL ) {
	if ( child->viable == UNKNOWN ) {
	    viable = UNKNOWN;
	    return;
	}
	if ( child->viable > viable ) {
	    viable = child->viable;
	}
	child = child->next;
    }
    if ( viable == MATCHALL && !ents->allMarked() ) {
	// There are some situations where this may happen - a child claims
	// MATCHALL while that is not the case.  If child #2 was checked and
	// later child #1 was unmarked (because we tried its OR's and ran into
	// problems), child #2's MATCHALL may no longer be true.
	viable = MATCHSOME;
    }
}

int JoinList::acceptChoice( EntNode *ents )
    /*
     * Accept the path we're a part of:  Mark all nodes of ents we can.  Mark
     * value will = mark (either MARK or ORMARK).  Return TRUE if we mark any-
     * thing; FALSE otherwise.
     */
{
    EntList *child;
    int result = FALSE;

    for ( child = childList; child != NULL; child = child->next ) {
	if ( child->viable >= MATCHSOME ) {
	    // Only mark children which have new nodes they can mark.  (This
	    // condition is important.  Sometimes, there will be children who
	    // can mark but whose vaiable val = SATISFIED.  This will be the
	    // case if there's another EntList with higher priority which can
	    // also mark this node.  (For example, if an AND has an OR and a
	    // SIMPLE child, the SIMPLE wins so that we'll have fewer OR
	    // choices, and so that a node we can mark with an unconditional
	    // EntList we won't mark with a conditional which may be undone
	    // later.)  Thus, our test here is - is this child the one who
	    // MATCHSOME'd when we originally went through the hierarchy.)
	    result = child->acceptChoice( ents ) || result;
	    // (NOTE - must run acceptChoice() first in above line.  If result
	    // were TRUE and we ||'ed it with acceptChoice(), aC() would never
	    // be run.)
	}
    }
    return result;
}

int MultList::prevKnown( EntList *desc )
    /*
     * Specialized function to test that none of the children prior to desc
     * (a pointer to one of the EntLists of childList) have viable = UNKNOWN.
     * Used in MatchNonORs() (see).
     */
{
    EntList *child = childList;

    while ( child != NULL && child != desc ) {
	if ( child->viable == UNKNOWN ) {
	    return FALSE;
	}
	child = child->next;
    }
    return TRUE;
}
