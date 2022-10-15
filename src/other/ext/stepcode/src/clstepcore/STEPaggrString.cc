#include "STEPaggrString.h"
#include <sstream>

/** \file STEPaggrString.cc
 * implement classes StringAggregate, StringNode
 */


StringAggregate::StringAggregate() {
}

StringAggregate::~StringAggregate() {
}

STEPaggregate & StringAggregate::ShallowCopy( const STEPaggregate & a ) {
    Empty();

    SingleLinkNode * next = a.GetHead();
    SingleLinkNode * copy;

    while( next ) {
        copy = new StringNode( *( StringNode * )next );
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

SingleLinkNode * StringAggregate::NewNode() {
    return new StringNode();
}


StringNode::StringNode() {
    value = "";
}

StringNode::~StringNode() {
}

StringNode::StringNode( StringNode & sn ) {
    value = sn.value.c_str();
}

StringNode::StringNode( const char * sStr ) {
    // value is an SDAI_String (the memory is copied)
    value = sStr;
}

SingleLinkNode * StringNode::NewNode() {
    return new StringNode();
}

/**
 * non-whitespace chars following s are considered garbage and is an error.
 * a valid value will still be assigned if it exists before the garbage.
 */
Severity StringNode::StrToVal( const char * s, ErrorDescriptor * err ) {
    return STEPread( s, err );
}

/**
 * this function assumes you will check for garbage following input
 */
Severity StringNode::StrToVal( istream & in, ErrorDescriptor * err ) {
    return value.STEPread( in, err );
}

/**
 * non-whitespace chars following s are considered garbage and is an error.
 * a valid value will still be assigned if it exists before the garbage.
 */
Severity StringNode::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * )s );

    value.STEPread( in, err );
    CheckRemainingInput( in, err, "string", ",)" );
    return err->severity();
}

/**
 * this function assumes you will check for garbage following input
 */
Severity StringNode::STEPread( istream & in, ErrorDescriptor * err ) {
    return value.STEPread( in, err );
}

const char * StringNode::asStr( std::string & s ) {
    value.asStr( s );
    return const_cast<char *>( s.c_str() );
}

const char * StringNode::STEPwrite( std::string & s, const char * ) {
    value.STEPwrite( s );
    return const_cast<char *>( s.c_str() );
}

void StringNode::STEPwrite( ostream & out ) {
    value.STEPwrite( out );
}

