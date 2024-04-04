#include "STEPaggrGeneric.h"
#include <sstream>

/** \file STEPaggrGeneric.cc
 * implement classes GenericAggregate, GenericAggrNode
 */

GenericAggregate::GenericAggregate() {
}

GenericAggregate::~GenericAggregate() {
}

SingleLinkNode * GenericAggregate::NewNode() {
    return new GenericAggrNode();
}

STEPaggregate & GenericAggregate::ShallowCopy( const STEPaggregate & a ) {
    Empty();

    SingleLinkNode * next = a.GetHead();
    SingleLinkNode * copy;

    while( next ) {
        copy = new GenericAggrNode( *( GenericAggrNode * )next );
        AddNode( copy );
        next = next->NextNode();
    }
    if( head ) {
        _null = 0;
    } else {
        _null = 1;
    }
    return *this;

}

GenericAggrNode::GenericAggrNode( const char * str ) {
    value = str;
}

GenericAggrNode::GenericAggrNode( GenericAggrNode & gan ) {
    value = gan.value;
}

GenericAggrNode::GenericAggrNode() {
}

GenericAggrNode::~GenericAggrNode() {
}

SingleLinkNode * GenericAggrNode::NewNode() {
    return new GenericAggrNode();
}

Severity GenericAggrNode::StrToVal( const char * s, ErrorDescriptor * err ) {
    return value.STEPread( s, err );
}

//TODO
Severity GenericAggrNode::StrToVal( istream & in, ErrorDescriptor * err ) {
    return value.STEPread( in, err );
}

Severity GenericAggrNode::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * ) s );
    return value.STEPread( in, err );
}

Severity GenericAggrNode::STEPread( istream & in, ErrorDescriptor * err ) {
    return value.STEPread( in, err );
}

const char * GenericAggrNode::asStr( std::string & s ) {
    s.clear();
    value.asStr( s );
    return const_cast<char *>( s.c_str() );
}

const char * GenericAggrNode::STEPwrite( std::string & s, const char * currSch ) {
    (void) currSch; //unused
    return value.STEPwrite( s );
}

void GenericAggrNode::STEPwrite( ostream & out ) {
    value.STEPwrite( out );
}
