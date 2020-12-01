#include "STEPaggrBinary.h"
#include <sstream>

/** \file STEPaggrBinary.cc
 * implement classes BinaryAggregate, BinaryNode
 */


BinaryAggregate::BinaryAggregate() {
}

BinaryAggregate::~BinaryAggregate() {
}

STEPaggregate & BinaryAggregate::ShallowCopy( const STEPaggregate & a ) {
    Empty();

    SingleLinkNode * next = a.GetHead();
    SingleLinkNode * copy;

    while( next ) {
        copy = new BinaryNode( *( BinaryNode * )next );
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

SingleLinkNode * BinaryAggregate::NewNode() {
    return new BinaryNode();
}


BinaryNode::BinaryNode() {
    value = 0;
}

BinaryNode::~BinaryNode() {
}

BinaryNode::BinaryNode( BinaryNode & bn ) {
    value = bn.value.c_str();
}

BinaryNode::BinaryNode( const char * sStr ) {
    // value is an SDAI_Binary (the memory is copied)
    value = sStr;
}

SingleLinkNode *
BinaryNode::NewNode() {
    return new BinaryNode();
}

/**
 * non-whitespace chars following s are considered garbage and is an error.
 * a valid value will still be assigned if it exists before the garbage.
 */
Severity BinaryNode::StrToVal( const char * s, ErrorDescriptor * err ) {
    return STEPread( s, err );
}

/**
 * this function assumes you will check for garbage following input
 */
Severity BinaryNode::StrToVal( istream & in, ErrorDescriptor * err ) {
    return value.STEPread( in, err );
}

/**
 * non-whitespace chars following s are considered garbage and is an error.
 * a valid value will still be assigned if it exists before the garbage.
 */
Severity BinaryNode::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * )s );

    value.STEPread( in, err );
    CheckRemainingInput( in, err, "binary", ",)" );
    return err->severity();
}

/**
 * this function assumes you will check for garbage following input
 */
Severity BinaryNode::STEPread( istream & in, ErrorDescriptor * err ) {
    return value.STEPread( in, err );
}

const char * BinaryNode::asStr( std::string & s ) {
    s = value.c_str();
    return const_cast<char *>( s.c_str() );
}

const char * BinaryNode::STEPwrite( std::string & s, const char * ) {
    value.STEPwrite( s );
    return const_cast<char *>( s.c_str() );
}

void BinaryNode::STEPwrite( ostream & out ) {
    value.STEPwrite( out );
}


