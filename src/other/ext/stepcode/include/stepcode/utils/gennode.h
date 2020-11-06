
#ifndef gennode_h
#define gennode_h

/*
* NIST Utils Class Library
* clutils/gennode.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id$  */

#include "export.h"
#include <iostream>

class GenNodeList;
class MgrNodeList;
class DisplayNodeList;

//////////////////////////////////////////////////////////////////////////////
// GenericNode
// If you delete this object it first removes itself from any list it is in.
//////////////////////////////////////////////////////////////////////////////

class STEPCODE_UTILS_EXPORT GenericNode {
        friend class GenNodeList;
        friend class MgrNodeList;
        friend class DisplayNodeList;

    protected:
        GenericNode * next;
        GenericNode * prev;
    public:
        GenericNode();
        virtual ~GenericNode();

        GenericNode * Next() {
            return next;
        }
        GenericNode * Prev() {
            return prev;
        }
        virtual void Append( GenNodeList * list );
        virtual void Remove() {
            ( next ) ? ( next->prev = prev ) : 0;
            ( prev ) ? ( prev->next = next ) : 0;
            /*
            //  if(next)
            //      next->prev = prev;
            //  if(prev)
            //      prev->next = next;
            */
            next = 0;
            prev = 0;

        }
};

//////////////////////////////////////////////////////////////////////////////
// GenericNode inline functions
// these functions don't rely on any inline functions (its own or
//  other classes) that aren't in this file
//////////////////////////////////////////////////////////////////////////////

#endif
