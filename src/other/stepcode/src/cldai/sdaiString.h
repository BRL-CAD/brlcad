#ifndef STEPSTRING_H
#define STEPSTRING_H  1

/*
* NIST STEP Core Class Library
* clstepcore/sdaiString.h
* April 1997
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sc_export.h>
#include <limits>


class SC_DAI_EXPORT SDAI_String {
    private:
        std::string content;
    public:

        //constructor(s) & destructor
        SDAI_String( const char * str = "", size_t max = std::string::npos );
        SDAI_String( const std::string & s );
        SDAI_String( const SDAI_String & s );
        ~SDAI_String( void );

//  operators
        SDAI_String & operator= ( const char * s );
        bool operator== ( const char * s ) const;

        void clear( void );
        bool empty( void ) const;
        const char * c_str( void ) const;
        // format for STEP
        const char * asStr( std::string & s ) const {
            s = c_str();
            return s.c_str();
        }
        void STEPwrite( ostream & out = cout )  const;
        void STEPwrite( std::string & s ) const;

        Severity StrToVal( const char * s );
        Severity STEPread( istream & in, ErrorDescriptor * err );
        Severity STEPread( const char * s, ErrorDescriptor * err );

};

#endif
