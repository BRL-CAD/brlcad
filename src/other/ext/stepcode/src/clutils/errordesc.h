
#ifndef ERRORDESC_H
#define ERRORDESC_H

/*
* NIST Utils Class Library
* clutils/errordesc.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sc_export.h>
#include <string>
#include <iostream>
using namespace std;

#define _POC_  " report problem to scl-dev at groups.google.com"

typedef enum Severity {
    SEVERITY_MAX    = -5,
    SEVERITY_DUMP   = -4,
    SEVERITY_EXIT   = -3,   // fatal
    SEVERITY_BUG    = -2,   // non-recoverable error -- probably bug
    SEVERITY_INPUT_ERROR = -1,  // non-recoverable error
    SEVERITY_WARNING    = 0,    // recoverable error
    SEVERITY_INCOMPLETE = 1,    // incomplete data
    SEVERITY_USERMSG    = 2,    // possibly an error
    SEVERITY_NULL   = 3 // no error or message
} Severity;

#define  DEBUG_OFF  0
#define  DEBUG_USR  1
#define  DEBUG_ALL  2
typedef int DebugLevel;

/******************************************************************
 ** Class:  ErrorDescriptor
 ** Data Members:
 **    severity level of error
 **    user message
 **    detailed message
 ** Description:
 **    the error is a detailed error message + a severity level
 **    also keeps a user message separately
 **    detailed message gets sent to ostream
 **    uses std::string class to keep the user messages
 **    keeps severity of error
 **    created with or without error
 ** Status:
 ******************************************************************/

class SC_UTILS_EXPORT ErrorDescriptor {
    private:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::string _userMsg, _detailMsg;
#ifdef _MSC_VER
#pragma warning( pop )
#endif
    protected:
        Severity    _severity;

        static DebugLevel   _debug_level;
        static ostream * _out; // note this will not be persistent
    public:
        ErrorDescriptor( Severity s    = SEVERITY_NULL,
                         DebugLevel d  = DEBUG_OFF );
        ~ErrorDescriptor( void );

        void PrintContents( ostream & out = cout ) const;

        void ClearErrorMsg() {
            _severity = SEVERITY_NULL;
            _userMsg.clear();
            _detailMsg.clear();
        }

        // return the enum value of _severity
        Severity severity() const {
            return _severity;
        }
        Severity severity( Severity s ) {
            return ( _severity = s );
        }
        std::string severityString() const;
        Severity GetCorrSeverity( const char * s );
        Severity GreaterSeverity( Severity s ) {
            return ( ( s < _severity ) ?  _severity = s : _severity );
        }

        std::string UserMsg() const {
            return _userMsg;
        }
        void UserMsg( const char * msg );
        void UserMsg( const std::string msg ) {
            _userMsg.assign( msg );
        }

        void AppendToUserMsg( const char * msg );
        void AppendToUserMsg( const char c );
        void AppendToUserMsg( const std::string & msg ) {
            _userMsg.append( msg );
        }
        void PrependToUserMsg( const char * msg );

        std::string DetailMsg() const {
            return _detailMsg;
        }
        void DetailMsg( const std::string msg ) {
            _detailMsg.assign( msg );
        }
        void DetailMsg( const char * msg );
        void AppendToDetailMsg( const char * msg );
        void AppendToDetailMsg( const std::string & msg ) {
            _detailMsg.append( msg );
        }
        void PrependToDetailMsg( const char * msg );
        void AppendToDetailMsg( const char c );

        Severity AppendFromErrorArg( ErrorDescriptor * err ) {
            GreaterSeverity( err->severity() );
            AppendToDetailMsg( err->DetailMsg() );
            AppendToUserMsg( err->UserMsg() );
            return severity();
        }

        DebugLevel debug_level() const        {
            return _debug_level;
        }
        void debug_level( DebugLevel d )   {
            _debug_level = d;
        }
        void SetOutput( ostream * o )      {
            _out = o;
        }
} ;

#endif
