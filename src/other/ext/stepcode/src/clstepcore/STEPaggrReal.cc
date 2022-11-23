#include "STEPaggrReal.h"

/** \file STEPaggrReal.cc
 * implementation of classes RealAggregate and RealNode
 */

RealAggregate::RealAggregate() {
}

RealAggregate::~RealAggregate() {
}

SingleLinkNode * RealAggregate::NewNode() {
    return new RealNode();
}

// COPY
STEPaggregate & RealAggregate::ShallowCopy( const STEPaggregate & a ) {
    const RealNode * tmp = ( const RealNode * ) a.GetHead();
    RealNode * to;

    while( tmp ) {
        to = ( RealNode * ) NewNode();
        to -> value = tmp -> value;
        AddNode( to );
        tmp = ( const RealNode * ) tmp -> NextNode();
    }
    if( head ) {
        _null = 0;
    } else {
        _null = 1;
    }
    return *this;
}


RealNode::RealNode() {
    value = S_REAL_NULL;
}

RealNode::RealNode( SDAI_Real v) {
    value = v;
}

RealNode::~RealNode() {
}

SingleLinkNode * RealNode::NewNode() {
    return new RealNode();
}

Severity RealNode::StrToVal( const char * s, ErrorDescriptor * err ) {
    if( ReadReal( value, s, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_REAL_NULL;
    }
    return err->severity();
}

Severity RealNode::StrToVal( istream & in, ErrorDescriptor * err ) {
    if( ReadReal( value, in, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_REAL_NULL;
    }
    return err->severity();
}

Severity RealNode::STEPread( const char * s, ErrorDescriptor * err ) {
    if( ReadReal( value, s, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_REAL_NULL;
    }
    return err->severity();
}

Severity RealNode::STEPread( istream & in, ErrorDescriptor * err ) {
    if( ReadReal( value, in, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_REAL_NULL;
    }
    return err->severity();
}

const char * RealNode::asStr( std::string & s ) {
    STEPwrite( s );
    return const_cast<char *>( s.c_str() );
}

const char * RealNode::STEPwrite( std::string & s, const char * ) {
    //use memcmp to work around -Wfloat-equal warning
    SDAI_Real z = S_REAL_NULL;
    if( 0 != memcmp( &value, &z, sizeof z ) ) {
        s = WriteReal( value );
    } else {
        s.clear();
    }
    return s.c_str();
}

void RealNode::STEPwrite( ostream & out ) {
    std::string s;
    out << STEPwrite( s );
}
