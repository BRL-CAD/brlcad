#ifndef _STEPSELECT_H
#define _STEPSELECT_H

/*
* NIST STEP Core Class Library
* clstepcore/sdaiSelect.h
* April 1997
* Dave Helfrick
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sc_export.h>

/**
 ** \file sdaiSelect.h class definition for the select superclass SDAI_Select.
 **/
class SC_CORE_EXPORT SDAI_Select {
    protected:
        const SelectTypeDescriptor * _type;
        const TypeDescriptor    *   underlying_type;
        BASE_TYPE                     base_type; // used by the subtypes

        // it looks like this member, val, is not used anywhere 9/27/96 - DAS
        SDAI_String val;
        ErrorDescriptor _error;

        const TypeDescriptor * SetUnderlyingType( const TypeDescriptor * );

        const TypeDescriptor * CanBe( const char * ) const;
        const TypeDescriptor * CanBe( BASE_TYPE ) const;
        const TypeDescriptor * CanBe( const TypeDescriptor * td ) const;
        const TypeDescriptor * CanBeSet( const char *, const char * ) const;

        int IsUnique( const BASE_TYPE bt ) const;

        virtual const TypeDescriptor * AssignEntity( SDAI_Application_instance * se ) = 0;
        virtual SDAI_Select * NewSelect() = 0;
    public:
        Severity severity() const;
        Severity severity( Severity );
        std::string Error();
        void Error( const char * );
        // clears select's error
        void ClearError();
        // clears error

        virtual BASE_TYPE ValueType() const = 0;

        // constructors
        SDAI_Select( const SelectTypeDescriptor * s = 0,
                     const TypeDescriptor * td = 0 );
        SDAI_Select( const SDAI_Select & other );
        virtual ~SDAI_Select();

        // from SDAI binding
        SDAI_String UnderlyingTypeName() const;
        const TypeDescriptor * CurrentUnderlyingType() const;
        bool exists() const;
        void nullify();

        Severity SelectValidLevel( const char * attrValue, ErrorDescriptor * err,
                                   InstMgrBase * im );

        // reading and writing
        const char * STEPwrite( std::string & s, const char * currSch = 0 ) const;
        void STEPwrite( ostream & out = cout, const char * currSch = 0 ) const;

        // IMS 8/2/95: added as part of new select implementation
        virtual void STEPwrite_verbose( ostream & out = cout, const char * = 0 )
        const;

        virtual void STEPwrite_content( ostream & out, const char * = 0 ) const = 0;


        Severity StrToVal( const char * val, const char * selectType,
                           ErrorDescriptor * err, InstMgrBase * instances = 0 );
        virtual Severity StrToVal_content( const char *,
                                           InstMgrBase * instances = 0 ) = 0;

        Severity STEPread( istream & in, ErrorDescriptor * err,
                           InstMgrBase * instances = 0, const char * utype = 0,
                           int addFileId = 0, const char * = NULL );

        // abstract function
        virtual Severity STEPread_content( istream & in = cin,
                                           InstMgrBase * instances = 0,
                                           const char * utype = 0,
                                           int addFileId = 0,
                                           const char * currSch = 0 ) = 0;

        //windows complains if operator= is pure virtual, perhaps because the impl is not in the lib with the definition
        //linux has a regression if the pure virtual operator= is commented out
        virtual SDAI_Select & operator =( const SDAI_Select & other );

        //FIXME set_null always returns true. why not void?!
        bool set_null();
        bool is_null();

};        /** end class  **/

typedef SDAI_Select * SDAI_Select_ptr;
typedef SDAI_Select_ptr SDAI_Select_var;

#endif
