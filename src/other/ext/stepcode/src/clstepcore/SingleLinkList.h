#ifndef singlelinklist_h
#define singlelinklist_h

/*
* NIST STEP Core Class Library
* clstepcore/SingleLinkList.h
* April 1997
* David Sauder
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sc_export.h>

/** Base class for a singly-linked list.
 * \sa SingleLinkNode
 *
 * node which represents the value is contained in the subclass
 * since it may have different types for different lists
 */
class SC_CORE_EXPORT SingleLinkList  {
    protected:
        class  SingleLinkNode  * head;
        SingleLinkNode  * tail;
    public:
        virtual SingleLinkNode * NewNode();
        virtual void AppendNode( SingleLinkNode * );
        virtual void DeleteNode( SingleLinkNode * );

        virtual void Empty();
        virtual void DeleteFollowingNodes( SingleLinkNode * );
        virtual SingleLinkNode * GetHead() const;

        int EntryCount() const;

        SingleLinkList();
        virtual ~SingleLinkList();
};

/** Base class for nodes of a single-linked list.
 * \sa SingleLinkList
 */
class SC_CORE_EXPORT SingleLinkNode {
        friend class SingleLinkList;
    public:
        SingleLinkList * owner;
        SingleLinkNode * next;

        virtual SingleLinkNode * NextNode() const {
            return next;
        }

        SingleLinkNode() : owner( 0 ), next( 0 ) {
        }

        virtual ~SingleLinkNode() {
        }
};

#endif
