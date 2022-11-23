#ifndef ATTRDESCRIPTORLIST_H
#define ATTRDESCRIPTORLIST_H

///header for list, node, and iterator

#include "SingleLinkList.h"

#include "sc_export.h"

class AttrDescriptor;

class SC_CORE_EXPORT AttrDescLinkNode : public SingleLinkNode {
    private:
    protected:
        AttrDescriptor * _attrDesc;
    public:
        AttrDescLinkNode();
        virtual ~AttrDescLinkNode();

        const AttrDescriptor * AttrDesc() const {
            return _attrDesc;
        }
        void AttrDesc( AttrDescriptor * ad ) {
            _attrDesc = ad;
        }
};

class SC_CORE_EXPORT AttrDescriptorList : public SingleLinkList {
    private:
    protected:
    public:
        AttrDescriptorList();
        virtual ~AttrDescriptorList();

        virtual SingleLinkNode * NewNode() {
            return new AttrDescLinkNode;
        }

        AttrDescLinkNode * AddNode( AttrDescriptor * ad );
};

class SC_CORE_EXPORT AttrDescItr {
    protected:
        const AttrDescriptorList & adl;
        const AttrDescLinkNode * cur;

    public:
        AttrDescItr( const AttrDescriptorList & adList );
        virtual ~AttrDescItr();

        void ResetItr() {
            cur = ( AttrDescLinkNode * )( adl.GetHead() );
        }

        const AttrDescriptor * NextAttrDesc();
};

#endif //ATTRDESCRIPTORLIST_H
