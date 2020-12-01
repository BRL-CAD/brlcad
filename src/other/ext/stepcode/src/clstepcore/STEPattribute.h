#ifndef STEPATTRIBUTE_H
#define STEPATTRIBUTE_H 1

/*
* NIST STEP Core Class Library
* clstepcore/STEPattribute.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/


#include <sc_export.h>
#include <stdio.h>
#include <errordesc.h>
#include <baseType.h>

#include <sdai.h>

/** \def REAL_NUM_PRECISION
 * this is used to set a const int Real_Num_Precision
 * in STEPaggregate.cc and STEPattribute.cc
 */
#define REAL_NUM_PRECISION 15

class InstMgrBase;
class SDAI_Application_instance;
class STEPaggregate;
class SCLundefined;

class TypeDescriptor;
class AttrDescriptor;
class EntityDescriptor;

#include <sstream>

extern SC_CORE_EXPORT int SetErrOnNull( const char * attrValue, ErrorDescriptor * error );

extern SC_CORE_EXPORT SDAI_Application_instance * ReadEntityRef( istream & in, ErrorDescriptor * err, const char * tokenList,
        InstMgrBase * instances, int addFileId );

extern SC_CORE_EXPORT SDAI_Application_instance * ReadEntityRef( const char * s, ErrorDescriptor * err, const char * tokenList,
        InstMgrBase * instances, int addFileId );

extern SC_CORE_EXPORT Severity EntityValidLevel( SDAI_Application_instance * se,
        const TypeDescriptor * ed, ///< entity type that entity se needs to match. (this must be an EntityDescriptor)
        ErrorDescriptor * err );

extern SC_CORE_EXPORT Severity EntityValidLevel( const char * attrValue, ///< string containing entity ref
        const TypeDescriptor * ed, /**< entity type that entity in attrValue (if it exists) needs
                                             *  to match. (this must be an EntityDescriptor)
                                             */
        ErrorDescriptor * err, InstMgrBase * im, int clearError );

////////////////////
////////////////////

extern SC_CORE_EXPORT SDAI_Application_instance * STEPread_reference( const char * s, ErrorDescriptor * err,
        InstMgrBase * instances, int addFileId );
////////////////////

extern SC_CORE_EXPORT int QuoteInString( istream & in );

extern SC_CORE_EXPORT void AppendChar( char c, int & index, char *& s, int & sSize );

extern SC_CORE_EXPORT void PushPastString( istream & in, std::string & s, ErrorDescriptor * err );

extern SC_CORE_EXPORT void PushPastImbedAggr( istream & in, std::string & s, ErrorDescriptor * err );

extern SC_CORE_EXPORT void PushPastAggr1Dim( istream & in, std::string & s, ErrorDescriptor * err );

class SC_CORE_EXPORT STEPattribute {
        friend ostream & operator<< ( ostream &, STEPattribute & );
        friend class SDAI_Application_instance;
    protected:
        bool _derive;
        bool _mustDeletePtr; ///if a member uses new to create an object in ptr
        ErrorDescriptor _error;
        STEPattribute * _redefAttr;
        const AttrDescriptor * aDesc;
        int refCount;

        /** \union ptr
        ** You know which of these to use based on the return value of
        ** NonRefType() - see below. BASE_TYPE is defined in baseType.h
        ** This variable points to an appropriate member variable in the entity
        ** class in the generated schema class library (the entity class is
        ** inherited from SDAI_Application_instance)
        */
        union attrUnion {
            SDAI_String * S;                 // STRING_TYPE
            SDAI_Integer * i;                // INTEGER_TYPE (Integer is a long int)
            SDAI_Binary * b;                 // BINARY_TYPE
            SDAI_Real * r;                   // REAL_TYPE and NUMBER_TYPE (Real is a double)
            SDAI_Application_instance * * c; // ENTITY_TYPE
            STEPaggregate * a;               // AGGREGATE_TYPE
            SDAI_Enum * e;                   // ENUM_TYPE, BOOLEAN_TYPE, and LOGICAL_TYPE
            SDAI_Select * sh;                // SELECT_TYPE
            SCLundefined * u;                // UNKNOWN_TYPE
            void * p;
        } ptr;

        char SkipBadAttr( istream & in, char * StopChars );
        void AddErrorInfo();
        void STEPwriteError( ostream& out, unsigned int line, const char* desc );

    public:
        void incrRefCount() {
            ++ refCount;
        }
        void decrRefCount() {
            -- refCount;
        }
        int getRefCount() {
            return refCount;
        }
        const AttrDescriptor * getADesc() {
            return aDesc;
        }
        void Derive( bool n = true )  {
            _derive = n;
        }

        void RedefiningAttr( STEPattribute * a ) {
            _redefAttr = a;
        }

///////////// Read, Write, Assign attr value

        Severity StrToVal( const char * s, InstMgrBase * instances = 0,
                           int addFileId = 0 );
        Severity STEPread( istream & in = cin, InstMgrBase * instances = 0,
                           int addFileId = 0, const char * currSch = NULL, bool strict = true );

        /// return the attr value as a string
        string asStr( const char * currSch = 0 ) const;

        /// put the attr value in ostream
        void STEPwrite( ostream & out = cout, const char * currSch = 0 );
        void ShallowCopy( const STEPattribute * sa );

        Severity set_null();

        /**
         * These functions verify that the attribute contains the requested type and
         * returns a pointer. The pointer is null if the requested type does not match.
         *
         * \sa Raw()
         * \sa NonRefType()
         * \sa is_null()
         */
        ///@{
        SDAI_Integer              * Integer();
        SDAI_Real                 * Real();
        SDAI_Real                 * Number();
        SDAI_String               * String();
        SDAI_Binary               * Binary();
        SDAI_Application_instance * Entity();
        STEPaggregate             * Aggregate();
        SDAI_Enum                 * Enum();
        SDAI_LOGICAL              * Logical();
        SDAI_BOOLEAN              * Boolean();
        SDAI_Select               * Select();
        SCLundefined              * Undefined();
        ///@}

        /// allows direct access to the union containing attr data (dangerous!)
        attrUnion * Raw() {
            return & ptr;
        }

        /**
         * These functions allow setting the attribute value.
         * Attr type is verified using an assertion.
         *
         * TODO should they check that the pointer was null?
         * what about ptr.c, which is ( SDAI_Application_instance ** ) ?
         */
        ///@{
        void Integer( SDAI_Integer * n );
        void Real( SDAI_Real * n );
        void Number( SDAI_Real * n );
        void String( SDAI_String * str );
        void Binary( SDAI_Binary * bin );
        void Entity( SDAI_Application_instance * ent );
        void Aggregate( STEPaggregate * aggr );
        void Enum( SDAI_Enum * enu );
        void Logical( SDAI_LOGICAL * log );
        void Boolean( SDAI_BOOLEAN * boo );
        void Select( SDAI_Select * sel );
        void Undefined( SCLundefined * undef );
        ///@}

////////////// Return info on attr

        bool Nullable() const; ///< may this attribute be null?
        bool is_null() const;  ///< is this attribute null?
        bool IsDerived() const  {
            return _derive;
        }
        STEPattribute * RedefiningAttr() {
            return _redefAttr;
        }

        const char  *  Name() const;
        const char  *  TypeName() const;
        BASE_TYPE   Type() const;
        BASE_TYPE   NonRefType() const;
        BASE_TYPE   BaseType() const;

        const TypeDescriptor * ReferentType() const;

        ErrorDescriptor & Error()    {
            return _error;
        }
        void ClearErrorMsg()    {
            _error.ClearErrorMsg();
        }

        Severity ValidLevel( const char* attrValue, ErrorDescriptor* error, InstMgrBase * im, bool clearError = true );

////////////////// Constructors

        STEPattribute( const STEPattribute & a );
        STEPattribute(): _derive( false ), _mustDeletePtr( false ),
                         _redefAttr( 0 ), aDesc( 0 ), refCount( 0 )  {
            memset( & ptr, 0, sizeof( ptr ) );
        }
        ~STEPattribute();
        //  INTEGER
        STEPattribute( const class AttrDescriptor & d, SDAI_Integer * p );
        //  BINARY
        STEPattribute( const class AttrDescriptor & d, SDAI_Binary * p );
        //  STRING
        STEPattribute( const class AttrDescriptor & d, SDAI_String * p );
        //  REAL & NUMBER
        STEPattribute( const class AttrDescriptor & d, SDAI_Real * p );
        //  ENTITY
        STEPattribute( const class AttrDescriptor & d, SDAI_Application_instance* *p );
        //  AGGREGATE
        STEPattribute( const class AttrDescriptor & d, STEPaggregate * p );
        //  ENUMERATION  and Logical
        STEPattribute( const class AttrDescriptor & d, SDAI_Enum * p );
        //  SELECT
        STEPattribute( const class AttrDescriptor & d, SDAI_Select * p );
        //  UNDEFINED
        STEPattribute( const class AttrDescriptor & d, SCLundefined * p );

        /// return true if attr types and values match
        SC_CORE_EXPORT friend bool operator == ( const STEPattribute & a1, const STEPattribute & a2 );
        SC_CORE_EXPORT friend bool operator != ( const STEPattribute & a1, const STEPattribute & a2 );

        /// return true if aDesc's match (behavior of old operator==)
        SC_CORE_EXPORT friend bool sameADesc ( const STEPattribute & a1, const STEPattribute & a2 );
};

#endif
