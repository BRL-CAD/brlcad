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
    EntList *child = childList, *nxt;

    while ( child ) {
	nxt = child->next;
	delete child;
	child = nxt;
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

int MultList::contains( const char *nm )
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

int MultList::hit( const char *nm )
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

int MultList::isDependent( const char *ent )
    /*
     * Can one of our descendants tell us that entity ent can or cannot be
     * instantiated independently (i.e., not as a complex entity with external
     * mapping).  This is primarily the case if ent is AND'ed with 1+ other
     * entities (e.g., if A is super of B AND C, B can only be created together
     * with C).  This is the version of isDependent for OR and ANDOR.  It is
     * redefined for AND.  Detailed commenting may be found in AndList::is-
     * Dependent().
     */
{
    EntList *child = childList;
    int result = DONT_KNOW, retval;

    while ( child ) {
        if ( (retval = child->isDependent( ent )) == FALSE ) {
	    return FALSE;
	}
	if ( retval == TRUE ) {
	    // If child tells us that ent must be created together with another
	    // leaf node (e.g., child is an AndList AND'ing ent + ent_b), save
	    // the result.  Don't return TRUE yet because a later child may
	    // tell us that ent may also be instantiable on its own.  That re-
	    // sult will override this since it would mean that ent does not
	    // *have* to be created with ext mapping.
	    result = TRUE;
	}
        child = child->next;
    }
    return result;
    // either DONT_KNOW or TRUE if we got here
}

int AndList::isDependent( const char *ent )
    /*
     * Tells us if entity ent cannot be instantiated independently.  Say ent
     * A is a supertype of ( B AND C ).  Neither B nor C can be instantiated
     * without the other.  An instance of A-B-C can only be created using
     * external mapping (see Part 21, sect 11.2.5.1).  This function returns
     * TRUE if `this' can show that ent requires external mapping.  If this
     * can show that ent does not require ext mapping, it returns FALSE, and
     * if nothing can be determined, it returns DONT_KNOW.
     */
{
    if ( supertype ) {
	// If we're a supertype, we have to make one exception.  Normally if
	// we're an AND of A & B and ent = A, we'd be able to conclude that A
	// requires ext mapping.  But here, the first child of the AND is a
	// supertype (super statements are represented as AND - super - subs).
	// Since a super + sub combo such as A-B does not require ext mapping,
	// we skip the first child.  We then continue to check if among the
	// subtypes of A there are children requiring ext mapping (such as B
	// AND C).
	return ( childList->next->isDependent( ent ) );
	// NOTE - actually the algorithm for a supertype is more complex.  We
        // did not address here the possibility that ent = the super (A).  In
	// such a case, if A is non-abstract, then by def it can be instanti-
	// ated by itself (barring ANDs higher up in the ComplexList).  If A is
	// abstract then it depends if its subtypes require ext mapping (maybe
	// it's a super of B AND C).  The reason I don't bother is because if
	// A is non-abstract, isDependent() would have found an "OR A" at the
	// level above this (that's how non-abs super's are implemented -
	//     OR +
	//        - A - AND +
	//                  - A - <A's subtype struct>
	// i.e., either A alone OR A + its subs).  So at the level above,
	// isDependent() would have realized that A is not dependent.  If A is
	// abstract, we don't bother checking further (though it's complex, as
	// above).  The reason is that this check is done to determine if an
	// instance of A can be created using internal mapping ("#10=A(...)")
	// or does ext mapping have to be used.  Here, there is no way to
	// create an A altogether since it's abstract.
    }

    // Next possibility:  We don't represent a supertype.  Thus, if we have >1
    // child and ent is one of them, it can only be created by being AND'ed
    // with at least 1 other child.
    if ( numchildren > 1 ) {
	if ( contains( ent ) ) {
	    return TRUE;
	}
	return DONT_KNOW;
    }

    // If we have 1 child only, just move on.  At this point, the fact that
    // we're an AND didn't go very far in telling us that our children are
    // dependent on one another since we only *have* one child.
    return ( childList->isDependent( ent ) );
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
    EntList *prv;

    if ( numchildren == 0 ) {
	childList = ent;
    } else {
	prv = getLast();
	prv->next = ent;
	ent->prev = prv;
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
	((AndList *)newlist)->supertype = ((AndList *)ent)->supertype;
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
