#ifndef STEPUNDEFINED_H
#define STEPUNDEFINED_H

/*
* NIST STEP Core Class Library
* clstepcore/STEPundefined.h
* April 1997
* KC Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include "export.h"
#include "utils/errordesc.h"
#include <string>
#include "core/read_func.h"

class STEPCODE_CORE_EXPORT SCLundefined  {
    protected:
        std::string val;

    public:
//  INPUT
        virtual Severity StrToVal( const char * s, ErrorDescriptor * err );
        virtual Severity StrToVal( istream & in, ErrorDescriptor * err );

        virtual Severity STEPread( const char * s, ErrorDescriptor * err );
        virtual Severity STEPread( istream & in, ErrorDescriptor * err );

//  OUTPUT
        virtual const char * asStr( std::string & s ) const;
        virtual const char * STEPwrite( std::string & s );
        virtual void    STEPwrite( std::ostream & out = std::cout );

        int set_null();
        int is_null();
        SCLundefined & operator= ( const SCLundefined & );
        SCLundefined & operator= ( const char * str );
        SCLundefined();
        virtual ~SCLundefined();
};

#endif
