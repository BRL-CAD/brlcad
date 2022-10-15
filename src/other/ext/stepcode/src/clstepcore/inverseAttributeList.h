#ifndef INVERSEATTRIBUTELIST_H
#define INVERSEATTRIBUTELIST_H

///header for list, node, and iterator

#include "SingleLinkList.h"
class Inverse_attribute;

class SC_CORE_EXPORT Inverse_attributeLinkNode : public  SingleLinkNode {
    private:
    protected:
        Inverse_attribute * _invAttr;
    public:
        Inverse_attributeLinkNode();
        virtual ~Inverse_attributeLinkNode();

        Inverse_attribute * Inverse_attr() const {
            return _invAttr;
        }
        void Inverse_attr( Inverse_attribute * ia ) {
            _invAttr = ia;
        }
};

class SC_CORE_EXPORT Inverse_attributeList : public  SingleLinkList {
    private:
    protected:
        virtual SingleLinkNode * NewNode() {
            return new Inverse_attributeLinkNode;
        }
    public:
        Inverse_attributeList();
        virtual ~Inverse_attributeList();
        Inverse_attributeLinkNode * AddNode( Inverse_attribute * ia );
};

class SC_CORE_EXPORT InverseAItr {
    protected:
        const Inverse_attributeList * ial;
        const Inverse_attributeLinkNode * cur;

    public:
        InverseAItr( const Inverse_attributeList * iaList );
        virtual ~InverseAItr();

        void ResetItr( const Inverse_attributeList * iaList = 0 ) {
            if( iaList ) {
                ial = iaList;
            }
            cur = ( Inverse_attributeLinkNode * )( ial->GetHead() );
        }

        Inverse_attribute * NextInverse_attribute();
};

#endif //INVERSEATTRIBUTELIST_H
