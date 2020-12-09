#ifndef TYPEDESCRIPTORLIST_H
#define TYPEDESCRIPTORLIST_H

///header for list, node, and iterator

#include "SingleLinkList.h"

#include "sc_export.h"

class TypeDescriptor;

class SC_CORE_EXPORT TypeDescLinkNode : public SingleLinkNode {
    private:
    protected:
        TypeDescriptor * _typeDesc;
    public:
        TypeDescLinkNode();
        virtual ~TypeDescLinkNode();

        const TypeDescriptor * TypeDesc() const {
            return _typeDesc;
        }
        void TypeDesc( TypeDescriptor * td ) {
            _typeDesc = td;
        }
};

class SC_CORE_EXPORT TypeDescriptorList : public SingleLinkList {
    private:
    protected:
    public:
        TypeDescriptorList();
        virtual ~TypeDescriptorList();

        virtual SingleLinkNode * NewNode() {
            return new TypeDescLinkNode;
        }

        TypeDescLinkNode * AddNode( TypeDescriptor * td ) {
            TypeDescLinkNode * node = ( TypeDescLinkNode * ) NewNode();
            node->TypeDesc( td );
            SingleLinkList::AppendNode( node );
            return node;
        }
};

class SC_CORE_EXPORT TypeDescItr {
    protected:
        const TypeDescriptorList & tdl;
        const TypeDescLinkNode * cur;

    public:
        TypeDescItr( const TypeDescriptorList & tdList );
        virtual ~TypeDescItr();

        void ResetItr() {
            cur = ( TypeDescLinkNode * )( tdl.GetHead() );
        }

        const TypeDescriptor * NextTypeDesc();
};

#endif //TYPEDESCRIPTORLIST_H
