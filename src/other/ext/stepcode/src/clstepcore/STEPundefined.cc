
/*
* NIST STEP Core Class Library
* clstepcore/STEPundefined.cc
* April 1997
* KC Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <stdio.h> // to get the BUFSIZ #define
#include <STEPattribute.h>
#include <STEPundefined.h>
#include "sc_memmgr.h"

/** \class SCLundefined
**    helper functions for reading unknown types
*/

Severity SCLundefined::StrToVal( const char * s, ErrorDescriptor * err ) {
    (void) err; //unused
    val = s;
    return SEVERITY_NULL;
}

Severity SCLundefined::StrToVal( istream & in, ErrorDescriptor * err ) {
    return STEPread( in, err );
}

Severity SCLundefined::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * ) s );
    return STEPread( in, err );
}

Severity SCLundefined::STEPread( istream & in, ErrorDescriptor * err ) {
    char c = '\0';
    ostringstream ss;
    std::string str;

    int terminal = 0;

    in >> ws; // skip white space
    in >> c;
    if( c == '$' ) {
        val = "";
        CheckRemainingInput( in, err, "aggregate item", ",)" );
    } else {
        in.putback( c );
    }

    while( !terminal ) {
        in.get( c );
        switch( c ) {
            case '(':
                in.putback( c );

                PushPastImbedAggr( in, str, err );
                ss << str;
                break;

            case '\'':
                in.putback( c );

                PushPastString( in, str, err );
                ss << str;
                break;

            case ',':
                terminal = 1; // it's a STEPattribute separator
                in.putback( c );
                c = '\0';
                break;

            case ')':
                in.putback( c );
                terminal = 1; // found a valid delimiter
                break;

            case '\0':
            case EOF:
                terminal = 1; // found a valid delimiter
                break;

            default:
                ss.put( c );
                break;
        }

        if( !in.good() ) {
            terminal = 1;
            c = '\0';
        }
    }

    ss << ends;
    val = &( ss.str()[0] );

    err->GreaterSeverity( SEVERITY_NULL );
    return SEVERITY_NULL;
}

const char * SCLundefined::asStr( std::string & s ) const {
    s = val.c_str();
    return const_cast<char *>( s.c_str() );
}

const char * SCLundefined::STEPwrite( std::string & s ) {
    if( val.empty() ) {
        s = "$";
    } else {
        s = val.c_str();
    }
    return const_cast<char *>( s.c_str() );
}

void SCLundefined::  STEPwrite( ostream & out ) {
    if( val.empty() ) {
        out << "$";
    } else {
        out << val;
    }
}

SCLundefined & SCLundefined::operator= ( const SCLundefined & x ) {
    std::string tmp;
    val = x.asStr( tmp );
    return *this;
}

SCLundefined & SCLundefined::operator= ( const char * str ) {
    if( !str ) {
        val.clear();
    } else {
        val = str;
    }
    return *this;
}

SCLundefined::SCLundefined() {
}

SCLundefined::~SCLundefined() {
}

int SCLundefined::set_null() {
    val = "";
    return 1;
}

bool SCLundefined::is_null() {
    return ( val.empty() );
}

