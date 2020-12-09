#include "attrDescriptorList.h"

#include "attrDescriptor.h"

AttrDescriptorList::AttrDescriptorList() {
}

AttrDescriptorList::~AttrDescriptorList() {
}

AttrDescLinkNode * AttrDescriptorList::AddNode( AttrDescriptor * ad ) {
    AttrDescLinkNode * node = ( AttrDescLinkNode * ) NewNode();
    node->AttrDesc( ad );
    SingleLinkList::AppendNode( node );
    return node;
}

AttrDescLinkNode::AttrDescLinkNode() {
    _attrDesc = 0;
}

AttrDescLinkNode::~AttrDescLinkNode() {
    if( _attrDesc ) {
        delete _attrDesc;
    }
}

AttrDescItr::AttrDescItr( const AttrDescriptorList & adList ) : adl( adList ) {
    cur = ( AttrDescLinkNode * )( adl.GetHead() );
}

AttrDescItr::~AttrDescItr() {
}

const AttrDescriptor * AttrDescItr::NextAttrDesc() {
    if( cur ) {
        const AttrDescriptor * ad = cur->AttrDesc();
        cur = ( AttrDescLinkNode * )( cur->NextNode() );
        return ad;
    }
    return 0;
}
