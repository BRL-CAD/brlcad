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

/* $Id: sdaiEnum.h,v 1.5 1997/11/05 21:59:14 sauderd DP3.1 $ */

class SCLP23_NAME( Enum )  {
        friend     ostream & operator<< ( ostream &, const SCLP23_NAME( Enum ) & );
    protected:
        int v;  //  integer value of enumeration instance
        //  mapped to a symbolic value in the elements

        virtual int set_value( const char * n );
        virtual int set_value( const int n );
        SCLP23_NAME( Enum )();
        virtual ~SCLP23_NAME( Enum )();

    public:

        void PrintContents( ostream & out = cout ) const {
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
        void STEPwrite( ostream & out = cout )  const;
        const char * STEPwrite( std::string & s ) const;

        Severity StrToVal( const char * s, ErrorDescriptor * err, int optional = 1 );
        Severity STEPread( istream & in, ErrorDescriptor * err, int optional = 1 );
        Severity STEPread( const char * s, ErrorDescriptor * err, int optional = 1 );

        virtual int put( int val );
        virtual int put( const char * n );
        int is_null() const {
            return !( exists() );
        }
        void set_null() {
            nullify();
        }
        SCLP23_NAME( Enum ) & operator= ( const int );
        SCLP23_NAME( Enum ) & operator= ( const SCLP23_NAME( Enum ) & );

//    operator ULong() const { return v; } // return the integer equivalent

        virtual int exists() const; // return 0 if unset otherwise return 1
        virtual void nullify(); // change the receiver to an unset status
        // unset is generated to be 1 greater than last element
        void DebugDisplay( ostream & out = cout ) const;

    protected:
        virtual Severity ReadEnum( istream & in, ErrorDescriptor * err,
                                   int AssignVal = 1, int needDelims = 1 );
};




enum Boolean { BFalse, BTrue, BUnset };
enum Logical { LFalse, LTrue, LUnset, LUnknown };

// old SCL definition
//enum LOGICAL { sdaiFALSE, sdaiTRUE, sdaiUNKNOWN };

class SCLP23_NAME( LOGICAL )  :
    public SCLP23_NAME( Enum )  {
    public:
        const char * Name() const;

        SCLP23_NAME( LOGICAL )( const char * val = 0 );
        SCLP23_NAME( LOGICAL )( Logical state );
        SCLP23_NAME( LOGICAL )( const SCLP23_NAME( LOGICAL )& source );
        SCLP23_NAME( LOGICAL )( int i );

        virtual ~SCLP23_NAME( LOGICAL )();

        virtual int no_elements() const;
        virtual const char * element_at( int n ) const;

//    operator int () const;
        operator Logical() const;
        SCLP23_NAME( LOGICAL ) & operator=( const SCLP23_NAME( LOGICAL )& t );

        SCLP23_NAME( LOGICAL ) operator==( const SCLP23_NAME( LOGICAL )& t ) const;

        // these 2 are redefined because LUnknown has cardinal value > LUnset
        int exists() const; // return 0 if unset otherwise return 1
        void nullify(); // change the receiver to an unset status

    protected:
        virtual int set_value( const int n );
        virtual int set_value( const char * n );
        virtual Severity ReadEnum( istream & in, ErrorDescriptor * err,
                                   int AssignVal = 1, int needDelims = 1 );

}
;

class SCLP23_NAME( BOOLEAN )  :
    public SCLP23_NAME( Enum )  {
    public:
        const char * Name() const;

        SCLP23_NAME( BOOLEAN )( char * val = 0 );
        SCLP23_NAME( BOOLEAN )( Boolean state );
        SCLP23_NAME( BOOLEAN )( const SCLP23_NAME( BOOLEAN )& source );
        SCLP23_NAME( BOOLEAN )( int i );
        SCLP23_NAME( BOOLEAN )( const SCLP23_NAME( LOGICAL )& val );
        virtual ~SCLP23_NAME( BOOLEAN )();

        virtual int no_elements() const;
        virtual const char * element_at( int n ) const;

        operator Boolean() const;
        SCLP23_NAME( BOOLEAN ) & operator=( const SCLP23_NAME( LOGICAL )& t );

        SCLP23_NAME( BOOLEAN ) & operator=( const Boolean t );
//    operator int () const;
//    operator Logical () const;
        SCLP23_NAME( LOGICAL ) operator==( const SCLP23_NAME( LOGICAL )& t ) const;

}
;

static const SCLP23_NAME( BOOLEAN ) SCLP23_NAME( TRUE );
static const SCLP23_NAME( BOOLEAN ) SCLP23_NAME( FALSE );
static const SCLP23_NAME( BOOLEAN ) SCLP23_NAME( UNSET );
static const SCLP23_NAME( LOGICAL ) SCLP23_NAME( UNKNOWN );

//}; // end struct P23_NAMESPACE

#endif
