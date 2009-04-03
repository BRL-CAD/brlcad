/*****************************************************************************
 * collect.cc                                                                *
 *                                                                           *
 * Description: ComplexCollect is the container structure for all ofthe com- *
 *              plex entity information in a schema.  It contains a list of  *
 *              ComplexList structures each of which corresponds to one set  *
 *              of subtype/supertype information about the schema. This file *
 *              contains many of the ComplexCollect member functions.        *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        11/14/96                                                     *
 *****************************************************************************/

#include "complexSupport.h"

void ComplexCollect::insert( ComplexList *c )
    /*
     * Inserts a new ComplexList to our list.  The ComplexLists are ordered by
     * supertype name.  Increments count.
     */
{
    ComplexList *prev = NULL, *cl = clists;

    while ( cl && *cl < *c ) {
	prev = cl;
	cl = cl->next;
    }
    if ( prev == NULL ) {
	// I.e., c belongs before the first cl so the above loop was never
	// entered.  (This may also be the case if there's nothing in the
	// collect yet and cl also = NULL.)
	clists = c;
	c->next = cl;
    } else {
	prev->next = c;
	c->next = cl;
    }
    count++;
}

void ComplexCollect::remove( ComplexList *c )
    /*
     * Removes the ComplexList whose supertype name = supername.  "Removing"
     * deletes the list and removes it from this, but does not delete its
     * subtype structure.  This is done when a subtype of other supertypes
     * was temporarily stored in the Collect so that its supertypes would
     * be able to find it, and now that all its supers have accessed it, we
     * remove it from the Collect.
     */
{
    ComplexList *cl = clists, *prev = NULL;

    while ( cl && *cl < *c ) {
	prev = cl;
	cl = cl->next;
    }
    if ( cl == NULL || cl != c ) {
	// Just in case c isn't in the list.
	return;
    }
    if ( prev == NULL ) {
	// c is the first thing in clists (so prev while loop never entered)
	clists = c->next;
    } else {
	prev->next = cl->next;
    }
    cl->next = NULL;
    cl->remove();
    count--;
}

ComplexList *ComplexCollect::find( char *name )
    /*
     * Searches for and returns the ComplexList whose supertype name = name.
     */
{
    ComplexList *cl = clists;

    while ( cl && *cl < name ) {
	cl = cl->next;
    }
    if ( cl && *cl == name ) {
	return cl;
    }
    return NULL;
}

int ComplexCollect::supports( EntNode *ents )
    /*
     * Determines if the parent schema supports the instantiation of a complex
     * type consisting of the the entities named in ents.  Does so by attempt-
     * ing to match ents against the ComplexLists in clists.  If one of the
     * nodes of ents has multSupers set to TRUE (it has >1 supertype), it
     * should be included in >1 CList.  A more complicated algorithm is applied
     * to match it, as described in the commenting.
     */
{
    EntNode *node = ents, *nextnode;
    AndList *alist;
    ComplexList *clist = clists, *cl = NULL, *current;
    int retval;
    EntList *elist, *next;

    // Loop through the nodes of ents.  If 1+ of them have >1 supertype, build
    // a combo-CList to handle it.
    while ( node ) {
        if ( node->multSuprs() ) {
	    // Temporarily slice out node from its list (so that CList->
	    // contains() will work properly below):
	    nextnode = node->next;
	    node->next = NULL;
            if ( !cl ) {
                // We may have created cl already in an earlier pass.
                alist = new AndList;
                cl = new ComplexList( alist );
            }
            current = clists;
            while ( current ) {
                if ( current->contains( node ) ) {
                    // Must add current CList to new CList.  First check if we
                    // added current already (while testing an earlier node).
                    if ( ! cl->toplevel( current->supertype() ) ) {
                        // Below line adds current to cl.  "current->head->
			// childList" points to the EntLists directly under the
			// top-level AND.  We'll add that list right under the
			// new AND we created at cl's top level.
                        alist->appendList( current->head->childList );
                    }
                }
                current = current->next;
            }
	    node->next = nextnode;
        }
	node = node->next;
    }

    // Now figure out if we match ents or not.  Done differently depending on
    // if we had a sub of >1 supers (and built cl as a combo).
    if ( !cl ) {
	// If we never built up cl in the above loop, there were no entities
	// which had mult supers.  Simply go through each CList separately:
	while ( clist != NULL ) {
	    if ( clist->matches( ents ) ) {
		return TRUE;
	    }
	    clist = clist->next;
	}
	// Went through whole list without match:
	return FALSE;
    } else {
	// Use cl to test that the conditions of all supertypes are met:
	cl->multSupers = TRUE;
	cl->buildList();
	retval = cl->matches( ents );

	// We have our return value.  Now get rid of cl:
	// Unlink all the EntLists (gotten from other CLists) which were joined
	// to make cl:
	elist = cl->head->childList;
	while ( elist ) {
	    elist->prev = NULL;
	    elist = elist->next;
	    next = elist->next;
	    elist->next = NULL;
	    elist = next;
	}
	cl->head->childList = NULL;
	// Separate the childList from head.  We don't want to delete any of the
	// sublists when we delete cl - they still belong to other sublists.
	delete cl;
	return retval;
    }
}
