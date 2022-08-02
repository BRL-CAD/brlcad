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

#include <sc_export.h>
#include <errordesc.h>
#include <string>
#include <read_func.h>

class SC_CORE_EXPORT SCLundefined
{
    protected:
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::string val;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

    public:
//  INPUT
        virtual Severity StrToVal(const char *s, ErrorDescriptor *err);
        virtual Severity StrToVal(istream &in, ErrorDescriptor *err);

        virtual Severity STEPread(const char *s, ErrorDescriptor *err);
        virtual Severity STEPread(istream &in, ErrorDescriptor *err);

//  OUTPUT
        virtual const char *asStr(std::string &s) const;
        virtual const char *STEPwrite(std::string &s);
        virtual void    STEPwrite(ostream &out = cout);

        int set_null();
        bool is_null();
        SCLundefined &operator= (const SCLundefined &);
        SCLundefined &operator= (const char *str);
        SCLundefined();
        virtual ~SCLundefined();
};

#endif
