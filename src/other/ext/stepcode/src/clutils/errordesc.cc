
/*
* NIST Utils Class Library
* clutils/errordesc.cc
* February, 1993
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <errordesc.h>
#include <Str.h>
#include <sc_memmgr.h>

DebugLevel ErrorDescriptor::_debug_level = DEBUG_OFF;
ostream  * ErrorDescriptor::_out = 0;

void
ErrorDescriptor::PrintContents( ostream & out ) const {
    out << "Severity: " << severityString() << endl;
    if( !_userMsg.empty() ) {
        out << "User message in parens:" << endl << "(";
        out << UserMsg() << ")" << endl;
    }
    if( !_detailMsg.empty() ) {
        out << "Detailed message in parens:" << endl << "(";
        out << DetailMsg() << ")" << endl;
    }
}

std::string ErrorDescriptor::severityString() const {
    std::string s;
    switch( severity() ) {
        case SEVERITY_NULL : {
            s.assign( "SEVERITY_NULL" );
            break;
        }
        case SEVERITY_USERMSG : {
            s.assign( "SEVERITY_USERMSG" );
            break;
        }
        case SEVERITY_INCOMPLETE : {
            s.assign( "SEVERITY_INCOMPLETE" );
            break;
        }
        case SEVERITY_WARNING : {
            s.assign( "SEVERITY_WARNING" );
            break;
        }
        case SEVERITY_INPUT_ERROR : {
            s.assign( "SEVERITY_INPUT_ERROR" );
            break;
        }
        case SEVERITY_BUG : {
            s.assign( "SEVERITY_BUG" );
            break;
        }
        case SEVERITY_EXIT : {
            s.assign( "SEVERITY_EXIT" );
            break;
        }
        case SEVERITY_DUMP : {
            s.assign( "SEVERITY_DUMP" );
            break;
        }
        case SEVERITY_MAX : {
            s.assign( "SEVERITY_MAX" );
            break;
        }
    }
    return s;
}


Severity
ErrorDescriptor::GetCorrSeverity( const char * s ) {
    if( s && s[0] != 0 ) {
        std::string s2;
        StrToUpper( s, s2 );
        if( !s2.compare( "SEVERITY_NULL" ) ) {
            return SEVERITY_NULL;
        }
        if( !s2.compare( "SEVERITY_USERMSG" ) ) {
            return SEVERITY_USERMSG;
        }
        if( !s2.compare( "SEVERITY_INCOMPLETE" ) ) {
            return SEVERITY_INCOMPLETE;
        }
        if( !s2.compare( "SEVERITY_WARNING" ) ) {
            return SEVERITY_WARNING;
        }
        if( !s2.compare( "SEVERITY_INPUT_ERROR" ) ) {
            return SEVERITY_INPUT_ERROR;
        }
        if( !s2.compare( "SEVERITY_BUG" ) ) {
            return SEVERITY_BUG;
        }
        if( !s2.compare( "SEVERITY_EXIT" ) ) {
            return SEVERITY_EXIT;
        }
        if( !s2.compare( "SEVERITY_DUMP" ) ) {
            return SEVERITY_DUMP;
        }
        if( !s2.compare( "SEVERITY_MAX" ) ) {
            return SEVERITY_MAX;
        }
    }
    cerr << "Internal error:  " << __FILE__ <<  __LINE__
         << "\n" << _POC_ "\n";
    cerr << "Calling ErrorDescriptor::GetCorrSeverity() with null string\n";
    return SEVERITY_BUG;
}

ErrorDescriptor::ErrorDescriptor( Severity s,  DebugLevel d ) : _severity( s ) {
    if( d  != DEBUG_OFF ) {
        _debug_level = d;
    }
}

ErrorDescriptor::~ErrorDescriptor( void ) {
}

void ErrorDescriptor::UserMsg( const char * msg ) {
    _userMsg.assign( msg );
}

void ErrorDescriptor::PrependToUserMsg( const char * msg ) {
    _userMsg.insert( 0, msg );
}

void ErrorDescriptor::AppendToUserMsg( const char c ) {
    _userMsg.append( &c );
}

void ErrorDescriptor::AppendToUserMsg( const char * msg ) {
    _userMsg.append( msg );
}

void ErrorDescriptor::DetailMsg( const char * msg ) {
    _detailMsg.assign( msg );
}

void ErrorDescriptor::PrependToDetailMsg( const char * msg ) {
    _detailMsg.insert( 0, msg );
}

void ErrorDescriptor::AppendToDetailMsg( const char c ) {
    _detailMsg.append( &c );
}

void ErrorDescriptor::AppendToDetailMsg( const char * msg ) {
    _detailMsg.append( msg );
}
