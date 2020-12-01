#ifndef P23ENUM_H
#define P23ENUM_H

/*
* NIST STEP Core Class Library
* clstepcore/Enum.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <iostream>
#include <sc_export.h>

class SC_DAI_EXPORT SDAI_Enum {
        friend     ostream & operator<< ( ostream &, const SDAI_Enum & );
    protected:
        int v;  //  integer value of enumeration instance
        //  mapped to a symbolic value in the elements

        virtual int set_value( const char * n );
        virtual int set_value( const int n );
        SDAI_Enum();

    public:
        virtual ~SDAI_Enum() {};

        void PrintContents( ostream & out = std::cout ) const {
            DebugDisplay( out );
        }

        virtual int no_elements() const = 0;
        virtual const char * Name() const = 0;
        const char * get_value_at( int n ) const {
            return element_at( n );
        }
        virtual const char * element_at( int n ) const = 0;

        Severity EnumValidLevel( const char * value, ErrorDescriptor * err,
                                 int optional, char * tokenList,
                                 int needDelims = 0, int clearError = 1 );

        Severity EnumValidLevel( istream & in, ErrorDescriptor * err,
                                 int optional, char * tokenList,
                                 int needDelims = 0, int clearError = 1 );

        int asInt() const {
            return v;
        }

        const char * asStr( std::string & s ) const;
        void STEPwrite( ostream & out = std::cout )  const;
        const char * STEPwrite( std::string & s ) const;

        Severity StrToVal( const char * s, ErrorDescriptor * err, int optional = 1 );
        Severity STEPread( istream & in, ErrorDescriptor * err, int optional = 1 );
        Severity STEPread( const char * s, ErrorDescriptor * err, int optional = 1 );

        virtual int put( int val );
        virtual int put( const char * n );
        bool is_null() const {
            return ( exists() == 0 );
        }
        void set_null() {
            nullify();
        }
        SDAI_Enum & operator= ( const int );
        SDAI_Enum & operator= ( const SDAI_Enum & );

        /// WARNING it appears that exists() will return true after a call to nullify(). is this intended?
        ///FIXME need to rewrite this function, but strange implementation...
        virtual int exists() const;
        virtual void nullify();
        void DebugDisplay( ostream & out = std::cout ) const;

    protected:
        virtual Severity ReadEnum( istream & in, ErrorDescriptor * err,
                                   int AssignVal = 1, int needDelims = 1 );
};



class SDAI_LOGICAL;
class SDAI_BOOLEAN;

enum Boolean { BFalse, BTrue, BUnset };
enum Logical { LFalse, LTrue, LUnset, LUnknown };

class SC_DAI_EXPORT SDAI_LOGICAL :
    public SDAI_Enum  {
    public:
        const char * Name() const;

        SDAI_LOGICAL( const char * val = 0 );
        SDAI_LOGICAL( Logical state );
        SDAI_LOGICAL( const SDAI_LOGICAL & source );
        SDAI_LOGICAL( int i );

        virtual ~SDAI_LOGICAL();

        virtual int no_elements() const;
        virtual const char * element_at( int n ) const;

        operator Logical() const;
        SDAI_LOGICAL & operator=( const SDAI_LOGICAL & t );

        SDAI_LOGICAL operator==( const SDAI_LOGICAL & t ) const;

        // these 2 are redefined because LUnknown has cardinal value > LUnset
        int exists() const; // return 0 if unset otherwise return 1
        void nullify(); // change the receiver to an unset status

    protected:
        virtual int set_value( const int n );
        virtual int set_value( const char * n );
        virtual Severity ReadEnum( istream & in, ErrorDescriptor * err,
                                   int AssignVal = 1, int needDelims = 1 );

};

class SC_DAI_EXPORT SDAI_BOOLEAN :
    public SDAI_Enum  {
    public:
        const char * Name() const;

        SDAI_BOOLEAN( char * val = 0 );
        SDAI_BOOLEAN( ::Boolean state );
        SDAI_BOOLEAN( const SDAI_BOOLEAN & source );
        SDAI_BOOLEAN( int i );
        SDAI_BOOLEAN( const SDAI_LOGICAL & val );
        virtual ~SDAI_BOOLEAN();

        virtual int no_elements() const;
        virtual const char * element_at( int n ) const;

        operator ::Boolean() const;
        SDAI_BOOLEAN & operator=( const SDAI_LOGICAL & t );

        SDAI_BOOLEAN & operator=( const ::Boolean t );
        SDAI_LOGICAL operator==( const SDAI_LOGICAL & t ) const;

};

static const SDAI_BOOLEAN SDAI_TRUE;
static const SDAI_BOOLEAN SDAI_FALSE;
static const SDAI_BOOLEAN SDAI_UNSET;
static const SDAI_LOGICAL SDAI_UNKNOWN;


#endif
