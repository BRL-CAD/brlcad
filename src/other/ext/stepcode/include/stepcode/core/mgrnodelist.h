
#ifndef mgrnodelist_h
#define mgrnodelist_h

/*
* NIST STEP Editor Class Library
* cleditor/mgrnodelist.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id$ */

#include "export.h"

#include "utils/gennode.h"
#include "utils/gennodelist.h"
#include "editor/editordefines.h"

//////////////////////////////////////////////////////////////////////////////
// class MgrNodeList
//  This will be used to represent the state lists.
//////////////////////////////////////////////////////////////////////////////

class MgrNode;

class STEPCODE_CORE_EXPORT MgrNodeList : public GenNodeList {
    public:
        MgrNodeList( stateEnum type );
        virtual ~MgrNodeList() { }

// ADDED functions
        virtual MgrNode * FindFileId( int fileId );

// REDEFINED functions
        // deletes node from its previous list & appends
        virtual void Append( GenericNode * node );
        // deletes newNode from its previous list & inserts in
        //  relation to existNode
        virtual void InsertAfter( GenericNode * newNode, GenericNode * existNode );
        virtual void InsertBefore( GenericNode * newNode, GenericNode * existNode );
        virtual void Remove( GenericNode * node );

    protected:
        stateEnum listType;
};

//////////////////////////////////////////////////////////////////////////////
// class MgrNodeList inline functions
// these functions don't rely on any inline functions (its own or
//  other classes) that aren't in this file except for Generic functions
//////////////////////////////////////////////////////////////////////////////

#endif
