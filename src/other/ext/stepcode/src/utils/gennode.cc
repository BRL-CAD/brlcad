
/*
* NIST Utils Class Library
* clutils/gennode.cc
* September 1996
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id$  */

#include "utils/gennode.h"
#include "utils/gennodelist.h"


//////////////////////////////////////////////////////////////////////////////
// class GenericNode inline functions that depend on other classes
//  inline functions
//
// depends on:
//  void GenNodeList::Append(GenericNode *node) from the gennodelist.h
//////////////////////////////////////////////////////////////////////////////

void GenericNode::Append( GenNodeList * list ) {
//    if(debug_level >= PrintFunctionTrace)
//  cout << "GenericNode::Append()\n";
//    if(debug_level >= PrintValues)
//  cout << "GenericNode::this : '" << this << "'\n";
    list->Append( this );
}

