/*****************************************************************************
 * print.cc                                                                  *
 *                                                                           *
 * Description: Contains the "<<" functions for all the EntList types.       *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        10/31/96                                                     *
 *****************************************************************************/

#include "complexSupport.h"
#include "sc_memmgr.h"

// Local function prototypes:
static char * joinText( JoinType, char * );

/**
 * Prints out a ComplexList, by iterating through its children.
 */
ostream & operator << ( ostream & os, ComplexList & clist ) {
    os << "ComplexList - \"" << *( SimpleList * )clist.head->childList
       << "\" supertype\n";
    // head->childList will call << for head's 1st child.  We know by def
    // that this is the supertype.
    os << *clist.head;
    return os;
}

/**
 * Prints out an EntList.  Calls appropriate function based on JoinType.
 */
ostream & operator << ( ostream & os, EntList & list ) {
    if( list.join == SIMPLE ) {
        os << *( SimpleList * )&list;
    } else {
        os << *( MultList * )&list;
    }
    return os;
}

/**
 * Prints out a SimpleList.
 */
ostream & operator << ( ostream & os, SimpleList & slist ) {
    os << slist.name;
    return os;
}

/**
 * Prints out a MultList.
 */
ostream & operator << ( ostream & os, MultList & mlist ) {
    char jointype[7];
    int k, lastSimple = 0;
    // lastSimple - is the last child simple?  If so, we need to print another
    // line at the end.  If not, the children of last child did already.
    EntList * child = mlist.childList;

    os << joinText( mlist.join, jointype ) << endl;
    for( k = 0; k <= mlist.level; k++ ) {
        // Indent 1 more than our level (hence the "<=" ):
        os << "    ";
    }
    while( child != NULL ) {
        os << *child;
        if( child->next == NULL ) {
            lastSimple = ( child->join == SIMPLE );
            break;
            // We don't want to do the conditions below if we're done.
        }
        if( child->join == SIMPLE ) {
            // If so, we're just going to continue printing the next child
            // (if exists) in same line.
            os << "  ";
        } else {
            // If another MultList is coming, it printed a new line for its
            // childList (as we're doing).  We must now start a new line at
            // our indent level:
            for( k = 0; k <= mlist.level; k++ ) {
                os << "    ";
            }
        }
        child = child->next;
    }
    if( lastSimple ) {
        os << endl;
    }
    return os;
}

/**
 * Copies and returns the string equivalent of a JoinType.
 */
static char * joinText( JoinType j, char * buf ) {
    switch( j ) {
        case SIMPLE:
            strcpy( buf, "SIMPLE" );
            return buf;
        case AND:
            strcpy( buf, "AND" );
            return buf;
        case OR:
            strcpy( buf, "OR" );
            return buf;
        case ANDOR:
            strcpy( buf, "ANDOR" );
            return buf;
    };
    return NULL;
}
