#include "entityDescriptorList.h"

EntityDescLinkNode::EntityDescLinkNode() {
    _entityDesc = 0;
}

EntityDescLinkNode::~EntityDescLinkNode() {
}

EntityDescriptorList::EntityDescriptorList() {
}

EntityDescriptorList::~EntityDescriptorList() {
}

EntityDescItr::EntityDescItr( const EntityDescriptorList & edList ) : edl( edList ) {
    cur = ( EntityDescLinkNode * )( edl.GetHead() );
}
EntityDescItr::~EntityDescItr() {
}


EntityDescriptor * EntityDescItr::NextEntityDesc_nc() {
    if( cur ) {
        EntityDescriptor * ed = cur->EntityDesc();
        cur = ( EntityDescLinkNode * )( cur->NextNode() );
        return ed;
    }
    return 0;
}
