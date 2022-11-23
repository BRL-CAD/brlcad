#ifndef ENTITYDESCRIPTORLIST_H
#define ENTITYDESCRIPTORLIST_H

///header for list, node, and iterator

#include "SingleLinkList.h"

#include "sc_export.h"

class EntityDescriptor;

class SC_CORE_EXPORT EntityDescLinkNode : public SingleLinkNode {

    private:
    protected:
        EntityDescriptor * _entityDesc;

    public:
        EntityDescLinkNode();
        virtual ~EntityDescLinkNode();

        EntityDescriptor * EntityDesc() const {
            return _entityDesc;
        }
        void EntityDesc( EntityDescriptor * ed ) {
            _entityDesc = ed;
        }
};

class SC_CORE_EXPORT EntityDescriptorList : public SingleLinkList {

    private:
    protected:
    public:
        EntityDescriptorList();
        virtual ~EntityDescriptorList();

        virtual SingleLinkNode * NewNode() {
            return new EntityDescLinkNode;
        }

        EntityDescLinkNode * AddNode( EntityDescriptor * ed ) {
            EntityDescLinkNode * node = ( EntityDescLinkNode * ) NewNode();
            node->EntityDesc( ed );
            SingleLinkList::AppendNode( node );
            return node;
        }
};

typedef EntityDescriptorList * Entity__set_ptr;
typedef Entity__set_ptr Entity__set_var;

class SC_CORE_EXPORT EntityDescItr {
    protected:
        const EntityDescriptorList & edl;
        const EntityDescLinkNode * cur;

    public:
        EntityDescItr( const EntityDescriptorList & edList );
        virtual ~EntityDescItr();

        void ResetItr() {
            cur = ( EntityDescLinkNode * )( edl.GetHead() );
        }

        inline const EntityDescriptor * NextEntityDesc() {
            return NextEntityDesc_nc();
        }

        EntityDescriptor * NextEntityDesc_nc();
};

#endif //ENTITYDESCRIPTORLIST_H
