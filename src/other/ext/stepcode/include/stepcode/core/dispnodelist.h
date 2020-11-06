
#ifndef dispnodelist_h
#define dispnodelist_h

/*
* NIST STEP Editor Class Library
* cleditor/dispnodelist.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id$ */

#include "export.h"

#include "utils/gennode.h"
#include "editor/editordefines.h"
#include "core/mgrnode.h"
#include "core/dispnode.h"
#include "utils/gennodelist.h"

///////////////////////////////////////////////////////////////////////////////
// class DisplayNodeList
//  This will be used to represent the display state lists.
//////////////////////////////////////////////////////////////////////////////

class STEPCODE_CORE_EXPORT DisplayNodeList : public GenNodeList {
    public:
        DisplayNodeList( displayStateEnum type );
        ~DisplayNodeList() { }

// ADDED functions
        displayStateEnum GetState() {
            return listType;
        }

// REDEFINED functions
        // deletes node from its previous list & appends
        virtual void Append( GenericNode * node );
        // deletes newNode from its previous list & inserts in
        //  relation to existNode
        virtual void InsertAfter( GenericNode * newNode, GenericNode * existNode );
        virtual void InsertBefore( GenericNode * newNode, GenericNode * existNode );
        virtual void Remove( GenericNode * node );

    protected:
    private:
        displayStateEnum listType;
};

//////////////////////////////////////////////////////////////////////////////
// class DisplayNodeList inline functions
// these functions don't rely on any inline functions (its own or
//  other classes) that aren't in this file except for Generic functions
//////////////////////////////////////////////////////////////////////////////

inline DisplayNodeList::DisplayNodeList( displayStateEnum type )
    : GenNodeList( new DisplayNode() ) {
    listType = type;
    ( ( DisplayNode * )head )->displayState = type;
}

#endif
