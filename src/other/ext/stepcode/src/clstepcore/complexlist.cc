/*************************************************************************//**
 * complexlist.cc      \class ComplexList                                    *
 *                                                                           *
 * Description: ComplexList is a class type which represents inheritance     *
 *              information for a set of entities related through supertype  *
 *              and subtype information.  This file contains many of the     *
 *              ComplexList member functions.                                *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        01/07/97                                                     *
 *****************************************************************************/

#include "complexSupport.h"
#include "sc_memmgr.h"

/**
 * Destructor for ComplexList.
 */
ComplexList::~ComplexList() {
    if( next ) {
        delete next;
    }
    delete head;
    delete list;
}

/**
 * Delete myself but don't take my sublist too.  Done when a ComplexList
 * was temporarily created to represent a complex structure which later
 * became a subtype of other supertypes.  After being incorporated into
 * the supertypes' ComplexLists, this temp one can be deleted.  Its sub-
 * structure, however, cannot be deleted since it's still being used.
 */
void ComplexList::remove() {
    head = NULL;
    // Only the overall AND will be deleted.
    delete this;
}

/**
 * Returns true if name is already contained at the top level of our
 * EntList hierarchy.  By top level, we mean the level under head.  This
 * is a highly specialized function which is used during the building of
 * a temporary CList to test entities which are subtypes of >1 supertype.
 */
bool ComplexList::toplevel( const char * name ) {
    EntList * slist = head->childList;

    while( slist ) {
        if( * dynamic_cast< SimpleList * >(slist) == name ) {
            return true;
        }
        slist = slist->next;
        if( slist ) {
            slist = slist->next;
        }
    }
    return false;
}

/**
 * Constructs this's list member.  Contains an alphabetically-ordered
 * linked list of EntNodes corresponding to all the leaf nodes in this.
 * Used as a quick method of determining if this *may* allow a complex
 * entity to be instantiated.  If the user wants to construct a complex
 * entity which contains an entity which is not contained in list, this
 * ComplexList certainly can't support it.
 */
void ComplexList::buildList() {
    EntList * sibling = head->childList->next;
    // sibling = the first EntList (below the overall AND) after the supertype.

    // If there was a list before, delete it:
    if( list ) {
        delete list;
    }

    // Add first node based on supertype:
    list = new EntNode( ( dynamic_cast< SimpleList * >(head->childList ))->name );
    // Recursively add all descendents:
    while( sibling ) {
        addChildren( sibling );
        sibling = sibling->next;
        // Note - a CList usually has no more than 1 sibling, corresponding to
        // the subtype info of a supertype.  But this may be a combo-CList used
        // to test sub's with >1 supertype.  If so, there will be more siblings.
    }

}

/**
 * Recursive function to add all the SimpleList descendents of ent into
 * this's list.
 */
void ComplexList::addChildren( EntList * ent ) {
    EntList * child;
    char * nm;
    EntNode * prev = list, *prev2 = NULL, *newnode;
    int comp = 0;

    if( ent->multiple() ) {
        child = ( ( MultList * )ent )->childList;
        while( child ) {
            addChildren( child );
            child = child->next;
        }
    } else {
        nm = ( dynamic_cast<SimpleList *>(ent) )->name;
        while( prev != NULL && ( comp = strcmp( prev->name, nm ) ) < 0 ) {
            prev2 = prev;
            prev = prev->next;
        }
        // One exceptional case:  If new name is same as prev, skip it:
        if( comp != 0 ) {
            // At this point, we know the new node belongs between prev2 and
            // prev.  prev or prev2 may = NULL if newnode belongs at the end
            // of the list or before the beginning, respectively.
            newnode = new EntNode( nm );
            newnode->next = prev;
            if( prev2 == NULL ) {
                // This will be the case if the inner while was never entered.
                // That happens when newnode belonged at the beginning of the
                // list.  If so, reset firstnode.
                list = newnode;
            } else {
                prev2->next = newnode;
            }
        }
    }
}

/**
 * Does a simple search to determine if this contains all the nodes of an
 * EntNode list.  If not, there's no way this will match ents.  If so,
 * we'll have to run matches() (below) to check more closely.  This func-
 * tion is simplified greatly because both EntNodes are ordered alphabeti-
 * cally.
 */
bool ComplexList::contains( EntNode * ents ) {
    EntNode * ours = list, *theirs = ents;

    while( theirs != NULL ) {
        while( ours != NULL && *ours < *theirs ) {
            ours = ours->next;
        }
        if( ours == NULL || *ours > *theirs ) {
            // If either of these occured, we couldn't find one of ours which
            // matched the current "theirs".
            return false;
        }
        theirs = theirs->next;
        ours = ours->next;
    }
    // At this point we must have matched them all.  (We may have extra, but
    // there's nothing wrong with that.)
    return true;
}

/**
 * Receives as input an EntNode list, corresponding to a user request to
 * instantiate the corresponding complex type.  Returns true if such a list
 * can be instantiated based on the list of EntLists which were generated
 * when the schema was read; false otherwise.
 */
bool ComplexList::matches( EntNode * ents ) {
    MatchType retval;
    int result = false;

    // First check if this ComplexList at least contains all the nodes of ents.
    // If it does, we'll search in detail.  If not, we're done.
    if( ! contains( ents ) ) {
        return false;
    }

    // Now start a thorough search through this ComplexList:
    if( ( retval = head->matchNonORs( ents ) ) == MATCHALL ) {
        result = true;
    } else if( retval != UNKNOWN ) {
        result = false;
        // UNKNOWN is the return val if there are ORs matchNonORs can't
        // analyze.  Unless we got a MATCHALL already, that's our only hope.
    } else {
        if( ( ( retval = head->matchORs( ents ) ) == MATCHALL ) &&
                ( hitMultNodes( ents ) ) ) {
            // hitMultNodes() checks that in case we're a combo-CList (see
            // CColect->supports()) we have a legal choice (see comments in
            // hitMultNodes()).
            result = true;
        } else if( retval >= MATCHSOME ) {
            // We have a partial answer.  Check if other solutions exist (i.e.,
            // if there are OR's with other choices):
            MatchType otherChoices = NEWCHOICE;
            while( otherChoices == NEWCHOICE ) {
                otherChoices = head->tryNext( ents );
                if( otherChoices == MATCHALL ) {
                    if( hitMultNodes( ents ) ) {
                        result = true;
                    } else {
                        otherChoices = NEWCHOICE;
                    }
                }
            }
        }
    }
    head->reset();
    // This recursively sets all EntLists in our list.
    ents->unmarkAll();
    // NOTE - may want to put this at ComplexCollect level.
    return result;
}

/**
 * This function has a specialized application.  If the user wants to
 * instantiate a complex type containing an entity with >1 supertype (call
 * it C), we temporarily join all the ComplexLists together corresponding
 * to the supertypes of C into a big CList (this).  We then test the user
 * request as normal.  If the regular matches() functions return MATCHALL,
 * we must check if each MultList corresponding to a CList hit C.  If not,
 * we do not have a valid match.  We only have the right to join the CLists
 * together (and make use of all the matching they afford) in order to
 * create a complete instance of C.  Matches which do not contain C are not
 * valid.  (This function is actually slightly more complicated because it
 * also deals with the possibility that >1 entities like C exist.)
 */
bool ComplexList::hitMultNodes( EntNode * ents ) {
    EntNode * node;
    EntList * child;

    // First get rid of the trivial case:  If this is not a combo-CList at all,
    // we have nothing to check for.  (CList::matches() routinely checks for
    // hitMultNodes in case we're a combo.)
    if( !multSupers ) {
        return true;
    }

    for( node = ents; node != NULL; node = node->next ) {
        if( node->multSuprs() ) {
            child = head->childList->next;
            // child points to the sublist of the first CList.  (head is the
            // AndList which AND's them all together.)
            while( child ) {
                // child is one of the EntList members of this which corre-
                // sponds to one of the combined CLists.  If child has node as
                // a member, it must have matched node, or we do not have a
                // legal match (see function header comments).  We check this
                // below.
                if( child->contains( node->name ) ) {
                    if( ! child->hit( node->name ) ) {
                        return false;
                    }
                }
                child = child->next;
                if( child ) {
                    child = child->next;
                }
                // We increment child twice.  We know this is how CLists are
                // joined:  super1 -> sublist1 -> super2 -> sublist2 -> ...
            }
        }
    }
    return true;
    // If we got here, we didn't find any unmatched complex subtypes.
}
