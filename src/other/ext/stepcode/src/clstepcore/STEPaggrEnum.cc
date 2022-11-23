#include "STEPaggrEnum.h"
#include <sstream>

/** \file StepaggrEnum.cc
 * implement classes EnumAggregate, LOGICALS, BOOLEANS, EnumNode
 */


/// COPY
STEPaggregate & EnumAggregate::ShallowCopy( const STEPaggregate & a ) {
    const EnumNode * tmp = ( const EnumNode * ) a.GetHead();
    EnumNode * to;

    while( tmp ) {
        to = ( EnumNode * ) NewNode();
        to -> node -> put( tmp -> node ->asInt() );
        AddNode( to );
        tmp = ( const EnumNode * ) tmp -> NextNode();
    }
    if( head ) {
        _null = 0;
    } else {
        _null = 1;
    }

    return *this;
}

EnumAggregate::EnumAggregate() {

}

EnumAggregate::~EnumAggregate() {

}

/******************************************************************
 * * \returns a new EnumNode which is of the correct derived type
 ** \details  creates a node to put in an list of enumerated values
 **           function is virtual so that the right node will be
 **           created
 ** Status:  ok 2/91
 ******************************************************************/
SingleLinkNode * EnumAggregate::NewNode() {
    //  defined in subclass
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  EnumAggregate::NewNode () called instead of virtual function. \n"
    << _POC_ << "\n";
    return 0;
}

LOGICALS::LOGICALS() {
}

LOGICALS::~LOGICALS() {
}

SingleLinkNode * LOGICALS::NewNode() {
    return new EnumNode( new SDAI_LOGICAL );
}

LOGICALS * create_LOGICALS() {
    return new LOGICALS;
}

BOOLEANS::BOOLEANS() {
}

BOOLEANS::~BOOLEANS() {
}

SingleLinkNode * BOOLEANS::NewNode() {
    return new EnumNode( new SDAI_BOOLEAN );
}

BOOLEANS * create_BOOLEANS() {
    return new BOOLEANS ;
}

EnumNode::EnumNode( SDAI_Enum  * e ) :  node( e ) {
}

EnumNode::EnumNode() {
}

EnumNode::~EnumNode() {
}

///  defined in subclass
SingleLinkNode * EnumNode::NewNode() {
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  EnumNode::NewNode () called instead of virtual function. \n"
    << _POC_ << "\n";
    return 0;
}

/**
 * non-whitespace chars following s are considered garbage and is an error.
 * a valid value will still be assigned if it exists before the garbage.
 */
Severity EnumNode::StrToVal( const char * s, ErrorDescriptor * err ) {
    return STEPread( s, err );
}
/**
 * this function assumes you will check for garbage following input
 */
Severity EnumNode::StrToVal( istream & in, ErrorDescriptor * err ) {
    return node->STEPread( in, err );
}

/**
 * non-whitespace chars following s are considered garbage and is an error.
 * a valid value will still be assigned if it exists before the garbage.
 */
Severity EnumNode::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * )s ); // sz defaults to length of s

    int nullable = 0;
    node->STEPread( in, err,  nullable );
    CheckRemainingInput( in, err, "enumeration", ",)" );
    return err->severity();
}

/**
 * this function assumes you will check for garbage following input
 */
Severity EnumNode::STEPread( istream & in, ErrorDescriptor * err ) {
    int nullable = 0;
    node->STEPread( in, err,  nullable );
    return err->severity();
}

const char * EnumNode::asStr( std::string & s ) {
    node -> asStr( s );
    return const_cast<char *>( s.c_str() );
}

const char * EnumNode::STEPwrite( std::string & s, const char * ) {
    node->STEPwrite( s );
    return const_cast<char *>( s.c_str() );
}

void EnumNode::STEPwrite( ostream & out ) {
    node->STEPwrite( out );
}
