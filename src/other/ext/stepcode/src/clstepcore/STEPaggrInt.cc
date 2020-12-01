#include "STEPaggrInt.h"


IntAggregate::IntAggregate() {
}

IntAggregate::~IntAggregate() {
}

SingleLinkNode * IntAggregate::NewNode() {
    return new IntNode();
}

/// COPY
STEPaggregate & IntAggregate::ShallowCopy( const STEPaggregate & a ) {
    const IntNode * tmp = ( const IntNode * ) a.GetHead();
    IntNode * to;

    while( tmp ) {
        to = ( IntNode * ) NewNode();
        to -> value = tmp -> value;
        AddNode( to );
        tmp = ( const IntNode * ) tmp -> NextNode();
    }
    if( head ) {
        _null = 0;
    } else {
        _null = 1;
    }
    return *this;
}




IntNode::IntNode() {
    value = S_INT_NULL;
}

IntNode::IntNode( SDAI_Integer v ) {
    value = v;
}

IntNode::~IntNode() {
}

SingleLinkNode * IntNode::NewNode() {
    return new IntNode();
}

Severity IntNode::StrToVal( const char * s, ErrorDescriptor * err ) {
    if( ReadInteger( value, s, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_INT_NULL;
    }
    return err->severity();
}

Severity IntNode::StrToVal( istream & in, ErrorDescriptor * err ) {
    if( ReadInteger( value, in, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_INT_NULL;
    }
    return err->severity();
}

Severity IntNode::STEPread( const char * s, ErrorDescriptor * err ) {
    if( ReadInteger( value, s, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_INT_NULL;
    }
    return err->severity();
}

Severity IntNode::STEPread( istream & in, ErrorDescriptor * err ) {
    if( ReadInteger( value, in, err, ",)" ) ) { // returns true if value is assigned
        _null = 0;
    } else {
        set_null();
        value = S_INT_NULL;
    }
    return err->severity();
}

const char * IntNode::asStr( std::string & s ) {
    STEPwrite( s );
    return const_cast<char *>( s.c_str() );
}

const char * IntNode::STEPwrite( std::string & s, const char * ) {
    char tmp[BUFSIZ];
    if( value != S_INT_NULL ) {
        sprintf( tmp, "%ld", value );
        s = tmp;
    } else {
        s.clear();
    }
    return const_cast<char *>( s.c_str() );
}

void IntNode::STEPwrite( ostream & out ) {
    std::string s;
    out << STEPwrite( s );
}
