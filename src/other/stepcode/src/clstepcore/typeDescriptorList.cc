#include "typeDescriptorList.h"

TypeDescLinkNode::TypeDescLinkNode() {
    _typeDesc = 0;
}

TypeDescLinkNode::~TypeDescLinkNode() {
}

TypeDescriptorList::TypeDescriptorList() {
}

TypeDescriptorList::~TypeDescriptorList() {
}

TypeDescItr::TypeDescItr( const TypeDescriptorList & tdList ) : tdl( tdList ) {
    cur = ( TypeDescLinkNode * )( tdl.GetHead() );
}

TypeDescItr::~TypeDescItr() {
}

const TypeDescriptor * TypeDescItr::NextTypeDesc() {
    if( cur ) {
        const TypeDescriptor * td = cur->TypeDesc();
        cur = ( TypeDescLinkNode * )( cur->NextNode() );
        return td;
    }
    return 0;
}
