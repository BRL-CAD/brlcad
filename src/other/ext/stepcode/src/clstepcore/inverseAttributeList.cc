#include "inverseAttributeList.h"
#include "inverseAttribute.h"

Inverse_attributeLinkNode::Inverse_attributeLinkNode() {
    _invAttr = 0;
}

Inverse_attributeLinkNode::~Inverse_attributeLinkNode() {
}

Inverse_attributeList::Inverse_attributeList() {
}

Inverse_attributeList::~Inverse_attributeList() {
    Inverse_attributeLinkNode * node;

    node = ( Inverse_attributeLinkNode * ) head;
    while( node ) {
        delete node->Inverse_attr();
        node = ( Inverse_attributeLinkNode * ) node->NextNode();
    }
}

Inverse_attributeLinkNode * Inverse_attributeList::AddNode( Inverse_attribute * ad ) {
    Inverse_attributeLinkNode * node = ( Inverse_attributeLinkNode * ) NewNode();
    node->Inverse_attr( ad );
    SingleLinkList::AppendNode( node );
    return node;
}

InverseAItr::InverseAItr( const Inverse_attributeList * iaList )
    : ial( iaList ) {
    cur = ( Inverse_attributeLinkNode * )( ial->GetHead() );
}

InverseAItr::~InverseAItr() {
}

Inverse_attribute * InverseAItr::NextInverse_attribute() {
    if( cur ) {
        Inverse_attribute * ia = cur->Inverse_attr();
        cur = ( Inverse_attributeLinkNode * )( cur->NextNode() );
        return ia;
    }
    return 0;
}
