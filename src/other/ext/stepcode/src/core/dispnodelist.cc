
/*
* NIST STEP Editor Class Library
* cleditor/dispnodelist.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id$ */

#include "utils/gennode.h"
#include "utils/gennodelist.h"

#include "core/mgrnode.h"
#include "core/mgrnodelist.h"
#include "core/dispnode.h"
#include "core/dispnodelist.h"


void DisplayNodeList::Remove( GenericNode * node ) {
    GenNodeList::Remove( node );
// DON'T DO THIS    ((DisplayNode *)node)->displayState = noMapState;
}

// deletes node from its previous list & appends
// actually it puts it at the front of the list.
void DisplayNodeList::Append( GenericNode * node ) {
    InsertBefore( node, head );
}

// deletes newNode from its previous list & inserts after
//  existNode
void DisplayNodeList::InsertAfter( GenericNode * newNode,
                                   GenericNode * existNode ) {
    if( newNode->next != 0 ) { // remove the node from its previous
        newNode->Remove();  //  display state list
    }
    GenNodeList::InsertAfter( newNode, existNode );
// DON'T DO THIS    ((DisplayNode *)newNode)->displayState = listType;
}

// deletes newNode from its previous list & inserts before
//  existNode
void DisplayNodeList::InsertBefore( GenericNode * newNode,
                                    GenericNode * existNode ) {
    if( newNode->next != 0 ) { // remove the node from its previous
        newNode->Remove();  //  display state list
    }
    GenNodeList::InsertBefore( newNode, existNode );
// DON'T DO THIS!!!   ((DisplayNode *)newNode)->displayState = listType;
}

