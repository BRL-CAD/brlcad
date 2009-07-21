#ifndef COMPLEXSUP_H
#define COMPLEXSUP_H

/*****************************************************************************
 * complexSupport.h                                                          *
 *                                                                           *
 * Description: Slightly simplified version of fedex_plus's complexSupport.h.*
 *              This version works with the SCL and is used to validate      *
 *              complex entities (user requests to build them).  It does not *
 *              contain info for building these structures from a fedex      *
 *              Express object, as the fedex_plus version.                   *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        5/9/97                                                       *
 *****************************************************************************/

#include <iostream>
#include <fstream>
using namespace std;
/*
extern "C"
{
#include <stdio.h>
#include <string.h>
#include <ctype.h>
}
*/
#include "Str.h"

#define FALSE     0
#define TRUE      1
#define LISTEND 999
// LISTEND signifies that an OrList has gone beyond its last viable choice
// among its children.

enum MarkType {
    NOMARK, ORMARK, MARK
};
// MARK is the usual value we'd mark with.  If we mark with ORMARK, it means
// an OrList marked this node, so we'll know it may become unmarked later if
// we try another choice.

enum MatchType {
    UNKNOWN, UNSATISFIED, SATISFIED, MATCHSOME, MATCHALL, NEWCHOICE, NOMORE
};
// These are the conditions the EntList::match() functions may return.  They
// are also the assigned values to EntList::viable.
//
//    UNKNOWN     - The default EntList::viable value - before any matching
//                  has been attempted.
//    UNSATISFIED - EntList had conditions that EntNode did not satisfy (e.g.,
//                  EntList specified A AND B while EntNode only contained an
//                  A).
//    SATISFIED   - EntList had no conditions EntNode did not meet.  But,
//                  EntList did not match (mark) any nodes of EntNode which
//                  were not already matched.
//    MATCHSOME   - EntList matched some of the nodes in EntNode.
//    MATCHALL    - EntList matched all the nodes in EntNode (complex entity
//                  can be instantiated).
//    MORECHOICES - Special case - when trying alternate OR paths, this return
//                  value signifies that we have found another choice.
//    NOMORE      - Special case - when trying alternate OR paths, this return
//                  value signifies that no more alternates within this path.

enum JoinType {
    SIMPLE, AND, OR, ANDOR
};

class SimpleList;
class MultList;
class JoinList;
class AndOrList;
class AndList;
class OrList;
class ComplexList;
class ComplexCollect;

class EntNode {
    friend class SimpleList;
    friend class AndOrList;
    friend class AndList;
    friend class OrList;
    friend class ComplexList;

  public:
    EntNode( const char *nm="" ) : next(0), mark(NOMARK), multSupers(0)
        { StrToLower( nm, name ); }
    EntNode( const char ** );                // given a list, create a 
    ~EntNode() { if ( next ) delete next; }  // linked list of EntNodes
    operator const char *() { return name; }
    int operator== ( EntNode &ent )
        { return ( strcmp( name, ent.name ) == 0 ); }
    int operator<  ( EntNode &ent )
        { return ( strcmp( name, ent.name ) < 0 ); }
    int operator>  ( EntNode &ent )
        { return ( strcmp( name, ent.name ) > 0 ); }
    EntNode &operator= ( EntNode &ent );
    void Name( const char *nm ) { strncpy( name, nm, BUFSIZ-1 ); }
    const char *Name() { return name; }
    void setmark( MarkType stamp=MARK ) { mark = stamp; }
    void markAll( MarkType=MARK );
    void unmarkAll() { markAll( NOMARK ); }
    int  marked( MarkType base=ORMARK ) { return ( mark >= base ); }
    int  allMarked();  // returns TRUE if all nodes in list are marked
    int  unmarkedCount();
    int  multSuprs() { return multSupers; }
    void multSuprs( int j ) { multSupers = j; }
    void sort( EntNode ** );

    EntNode *next;

  private:
    MarkType mark;
    char name[BUFSIZ];
    int multSupers;  // do I correspond to an entity with >1 supertype?
    EntNode *lastSmaller( EntNode * );  // used by ::sort()
};

class EntList {
    friend class MultList;
    friend class JoinList;
    friend class OrList;
    friend class ComplexList;
    friend class ComplexCollect;
    friend ostream & operator<< ( ostream &, EntList & );
    friend ostream & operator<< ( ostream &, MultList & );

  public:
    EntList( JoinType j ) : join(j), next(0), prev(0), viable(UNKNOWN),
                            level(0) {}
    virtual ~EntList() {}
    MatchType viableVal() { return viable; }
    virtual void setLevel( int l ) { level = l; }
    virtual int contains( char * ) =0;
    virtual int hit( char * ) =0;
    virtual MatchType matchNonORs( EntNode * ) { return UNKNOWN; }
    virtual int acceptChoice( EntNode * ) =0;
    virtual void unmarkAll( EntNode * ) =0;
    virtual void reset() { viable = UNKNOWN; }
    int siblings();

    // List access functions.  They access desired children based on their
    // join or viable values.  Below is an incomplete list of possible fns,
    // but all we need.
    EntList *firstNot( JoinType );
    EntList *nextNot( JoinType j ) { return next->firstNot( j ); }
    EntList *firstWanted( MatchType );
    EntList *nextWanted( MatchType mat ) { return next->firstWanted( mat ); }
    EntList *lastNot( JoinType );
    EntList *prevNot( JoinType j ) { return prev->lastNot( j ); }
    EntList *lastWanted( MatchType );
    EntList *prevWanted( MatchType mat ) { return prev->lastWanted( mat ); }

    JoinType join;
    int multiple() { return ( join != SIMPLE ); }
    EntList *next, *prev;

  protected:
    MatchType viable;
      // How does this EntList match the complex type.  Used especially if Ent-
      // List's parent is an OrList or AndOrList to record if this child is an
      // acceptable choice or not.  For an AndOr, viable children are accepted
      // right away.  For Or, only one is accepted, but we keep track of the
      // other possible solutions in case we'll want to try them.
    int level;  // How many levels deep are we (main use for printing).
};

class SimpleList : public EntList {
    friend class ComplexList;
    friend ostream & operator<< ( ostream &, SimpleList & );

  public:
    SimpleList( const char *n ) : EntList(SIMPLE), I_marked(NOMARK)
        { strcpy( name, n ); }
    ~SimpleList() {}
    int operator== ( const char *nm )
        { return ( strcmp( name, nm ) == 0 ); }
    const char *Name() { return name; }
    int contains( char *nm ) { return *this == nm; }
    int hit( char *nm ) { return *this == nm; }
    MatchType matchNonORs( EntNode * );
    int acceptChoice( EntNode * );
    void unmarkAll( EntNode * );
    void reset() { viable = UNKNOWN; I_marked = NOMARK; }

  private:
    char name[BUFSIZ];    // Name of entity we correspond to.
    MarkType I_marked; // Did I mark, and with what type of mark.
};

class MultList : public EntList {
    // Supports concepts and functionality common to all the compound list
    // types, especially AND and ANDOR.

    friend class ComplexList;
    friend class ComplexCollect;
    friend ostream & operator<< ( ostream &, MultList & );

  public:
    MultList( JoinType j ) : EntList(j), supertype(0), numchildren(0),
                             childList(0) {}
    ~MultList();
    void setLevel( int );
    int contains( char * );
    int hit( char * );
    void appendList( EntList * );
    EntList *copyList( EntList * );
    virtual MatchType matchORs( EntNode * ) =0;
    virtual MatchType tryNext( EntNode * );

    int childCount() { return numchildren; }
//  EntList *operator[]( int );
    EntList *getChild( int );
    EntList *getLast() { return ( getChild( numchildren-1 ) ); }
    void unmarkAll( EntNode * );
    int prevKnown( EntList * );
    void reset();

  protected:
    int supertype;  // do I represent a supertype?
    int numchildren;
    EntList *childList;
      // Points to a list of "children" of this EntList.  E.g., if join = 
      // AND, it would point to a list of the entity types we are AND'ing.
      // The children may be SIMPLE EntLists (contain entity names) or may
      // themselves be And-, Or-, or AndOrLists.
};

class JoinList : public MultList {
    // A specialized MultList, super for subtypes AndOrList and AndList, or
    // ones which join their multiple children.
  public:
    JoinList( JoinType j ) : MultList(j) {}
    ~JoinList() {}
    void setViableVal( EntNode * );
    int acceptChoice( EntNode * );
};

class AndOrList : public JoinList {
    friend class ComplexList;

  public:
    AndOrList() : JoinList( ANDOR ) {}
    ~AndOrList() {}
    MatchType matchNonORs( EntNode * );
    MatchType matchORs( EntNode * );
};

class AndList : public JoinList {
    friend class ComplexList;
    friend ostream & operator<< ( ostream &, ComplexList & );

  public:
    AndList() : JoinList( AND ) {}
    ~AndList() {}
    MatchType matchNonORs( EntNode * );
    MatchType matchORs( EntNode * );
};

class OrList : public MultList {
  public:
    OrList() : MultList( OR ), choice(-1), choiceCount(0) {}
    ~OrList() {}
    int hit( char * );
    MatchType matchORs( EntNode * );
    MatchType tryNext( EntNode * );
    void unmarkAll( EntNode * );
    int acceptChoice( EntNode * );
    int acceptNextChoice( EntNode *ents )
        { choice++; return ( acceptChoice( ents ) ); }
    void reset() { choice = -1; choiceCount = 0; MultList::reset(); }

  private:
    int choice, choice1, choiceCount;
      // Which choice of our childList did we select from this OrList; what's
      // the first viable choice; and how many choices are there entirely.
};

class ComplexList {
    // Contains the entire list of EntLists which describe the set of
    // instantiable complex entities defined by an EXPRESS expression.

    friend class ultList;
    friend class ComplexCollect;
    friend ostream & operator<< ( ostream &, ComplexList & );

  public:
    ComplexList( AndList *alist = NULL ) : list(0), head(alist), next(0),
                                         abstract(0), dependent(0),
                                         multSupers(0) {}
    ~ComplexList();
    void buildList();
    void remove();
    int operator<  ( ComplexList &c )
        { return ( strcmp( supertype(), c.supertype() ) < 0 ); }
    int operator< ( char *name )
        { return ( strcmp( supertype(), name ) < 0 ); }
    int operator== ( char *name )
        { return ( strcmp( supertype(), name ) == 0 ); }
    const char *supertype() { return ((SimpleList *)head->childList)->name; }
      // Based on knowledge that ComplexList always created by ANDing supertype
      // with subtypes.
    int toplevel( const char * );
    int contains( EntNode * );
    int matches( EntNode * );

    EntNode *list; // List of all entities contained in this complex type,
                   // regardless of how.  (Used as a quick way of determining
                   // if this List *may* contain a certain complex type.)
    AndList *head;
    ComplexList *next;
    int Dependent() { return dependent; }

  private:
    void addChildren( EntList * );
    int hitMultNodes( EntNode * );
    int abstract;   // is our supertype abstract?
    int dependent;  // is our supertype also a subtype of other supertype(s)?
    int multSupers; // am I a combo-CList created to test a subtype which has
};                  // >1 supertypes?

class ComplexCollect {
    // The collection of all the ComplexLists defined by the current schema.
  public:
    ComplexCollect( ComplexList *c = NULL ) : clists(c)
        { count = ( c ? 1 : 0 ); }
    ~ComplexCollect() { delete clists; }
    void insert( ComplexList * );
    void remove( ComplexList * );
      // Remove this list but don't delete its hierarchy structure, because
      // it's used elsewhere.
    ComplexList *find( char * );
    int supports( EntNode * ) const;

    ComplexList *clists;

  private:
    int count;  // # of clist children
};

#endif
