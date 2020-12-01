
#include <sdai.h>
#include "sc_memmgr.h"

/*
* NIST STEP Core Class Library
* cldai/sdaiEnum.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sstream>

///////////////////////////////////////////////////////////////////////////////
// class Logical
///////////////////////////////////////////////////////////////////////////////

SDAI_LOGICAL::SDAI_LOGICAL( const char * val ) {
    set_value( val );
}

SDAI_LOGICAL::SDAI_LOGICAL( Logical state ) {
    set_value( state );
}

SDAI_LOGICAL::SDAI_LOGICAL( const SDAI_LOGICAL & source ) {
    set_value( source.asInt() );
}

SDAI_LOGICAL::SDAI_LOGICAL( int i ) {
    if( i == 0 ) {
        v =  LFalse ;
    } else {
        v =  LTrue ;
    }
}

SDAI_LOGICAL::~SDAI_LOGICAL() {
}

const char * SDAI_LOGICAL::Name() const {
    return "Logical";
}

int SDAI_LOGICAL::no_elements() const {
    return 3;
}

const char * SDAI_LOGICAL::element_at( int n ) const {
    switch( n )  {
        case  LUnknown :
            return "U";
        case  LFalse :
            return "F";
        case  LTrue :
            return "T";
        default:
            return "UNSET";
    }
}

int SDAI_LOGICAL::exists() const { // return 0 if unset otherwise return 1
    return !( v == 2 );
}

void SDAI_LOGICAL::nullify() { // change the receiver to an unset status
    v = 2;
}

SDAI_LOGICAL::operator  Logical() const  {
    switch( v ) {
        case  LFalse :
            return  LFalse ;
        case  LTrue :
            return  LTrue ;
        case  LUnknown :
            return  LUnknown ;
        case  LUnset :
        default:
            return  LUnset ;
    }
}

SDAI_LOGICAL & SDAI_LOGICAL::operator= ( const SDAI_LOGICAL & t ) {
    set_value( t.asInt() );
    return *this;
}

SDAI_LOGICAL SDAI_LOGICAL::operator ==( const SDAI_LOGICAL & t ) const {
    if( v == t.asInt() ) {
        return  LTrue ;
    }
    return  LFalse ;
}

int SDAI_LOGICAL::set_value( const int i )  {
    if( i > no_elements() + 1 )  {
        v = 2;
        return v;
    }
    const char * tmp = element_at( i );
    if( tmp[0] != '\0' ) {
        return ( v = i );
    }
    // otherwise
    cerr << "(OLD Warning:) invalid enumeration value " << i
         << " for " <<  Name() << "\n";
    DebugDisplay();
    return  no_elements() + 1 ;
}

int SDAI_LOGICAL::set_value( const char * n )  {
    //  assigns the appropriate value based on n
    if( !n || ( !strcmp( n, "" ) ) ) {
        nullify();
        return asInt();
    }

    int i = 0;
    std::string tmp;
    while( ( i < ( no_elements() + 1 ) )  &&
            ( strcmp( ( char * )StrToUpper( n, tmp ),  element_at( i ) ) != 0 ) ) {
        ++i;
    }
    if( ( no_elements() + 1 ) == i ) { //  exhausted all the possible values
        nullify();
        return v;
    }
    v = i;
    return v;
}

Severity SDAI_LOGICAL::ReadEnum( istream & in, ErrorDescriptor * err, int AssignVal,
                                 int needDelims ) {
    if( AssignVal ) {
        set_null();
    }

    std::string str;
    char messageBuf[512];
    messageBuf[0] = '\0';

    in >> ws; // skip white space

    if( in.good() ) {
        char c;
        in.get( c );
        if( c == '.' || isalpha( c ) ) {
            int validDelimiters = 1;
            if( c == '.' ) {
                in.get( c ); // push past the delimiter
                // since found a valid delimiter it is now invalid until the
                //   matching ending delim is found
                validDelimiters = 0;
            }

            // look for UPPER
            if( in.good() && ( isalpha( c ) || c == '_' ) ) {
                str += c;
                in.get( c );
            }

            // look for UPPER or DIGIT
            while( in.good() && ( isalnum( c ) || c == '_' ) ) {
                str += c;
                in.get( c );
            }
            // if character is not the delimiter unread it
            if( in.good() && ( c != '.' ) ) {
                in.putback( c );
            }

            // a value was read
            if( str.length() > 0 ) {
                int i = 0;
                const char * strval = str.c_str();
                std::string tmp;
                while( ( i < ( no_elements() + 1 ) )  &&
                        ( strcmp( ( char * )StrToUpper( strval, tmp ),
                                  element_at( i ) ) != 0 ) ) {
                    ++i;
                }
                if( ( no_elements() + 1 ) == i ) {
                    //  exhausted all the possible values
                    err->GreaterSeverity( SEVERITY_WARNING );
                    err->AppendToDetailMsg( "Invalid Enumeration value.\n" );
                    err->AppendToUserMsg( "Invalid Enumeration value.\n" );
                } else {
                    if( AssignVal ) {
                        v = i;
                    }
                }

                // now also check the delimiter situation
                if( c == '.' ) { // if found ending delimiter
                    // if expecting delim (i.e. validDelimiter == 0)
                    if( !validDelimiters ) {
                        validDelimiters = 1; // everything is fine
                    } else { // found ending delimiter but no initial delimiter
                        validDelimiters = 0;
                    }
                }
                // didn't find any delimiters at all and need them.
                else if( needDelims ) {
                    validDelimiters = 0;
                }

                if( !validDelimiters ) {
                    err->GreaterSeverity( SEVERITY_WARNING );
                    if( needDelims )
                        sprintf( messageBuf,
                                 "Enumerated value has invalid period delimiters.\n" );
                    else
                        sprintf( messageBuf,
                                 "Mismatched period delimiters for enumeration.\n" );
                    err->AppendToDetailMsg( messageBuf );
                    err->AppendToUserMsg( messageBuf );
                }
                return err->severity();
            }
            // found valid or invalid delimiters with no associated value
            else if( ( c == '.' ) || !validDelimiters ) {
                err->GreaterSeverity( SEVERITY_WARNING );
                err->AppendToDetailMsg(
                    "Enumerated has valid or invalid period delimiters with no value.\n"
                );
                err->AppendToUserMsg(
                    "Enumerated has valid or invalid period delimiters with no value.\n"
                );
                return err->severity();
            } else { // no delims and no value
                err->GreaterSeverity( SEVERITY_INCOMPLETE );
            }

        } else if( ( c == ',' ) || ( c == ')' ) ) {
            in.putback( c );
            err->GreaterSeverity( SEVERITY_INCOMPLETE );
        } else {
            in.putback( c );
            err->GreaterSeverity( SEVERITY_WARNING );
            sprintf( messageBuf, "Invalid enumeration value.\n" );
            err->AppendToDetailMsg( messageBuf );
            err->AppendToUserMsg( messageBuf );
        }
    } else { // hit eof (assuming there was no error state for istream passed in)
        err->GreaterSeverity( SEVERITY_INCOMPLETE );
    }
    return err->severity();
}

///////////////////////////////////////////////////////////////////////////////
// class BOOLEAN  Jan 97
///////////////////////////////////////////////////////////////////////////////

const char * SDAI_BOOLEAN::Name() const {
    return "Bool";
}

SDAI_BOOLEAN::SDAI_BOOLEAN( char * val ) {
    set_value( val );
}

SDAI_BOOLEAN::SDAI_BOOLEAN( Boolean state ) {
    set_value( state );
}

SDAI_BOOLEAN::SDAI_BOOLEAN( const SDAI_BOOLEAN & source ) {
    set_value( source.asInt() );
}

SDAI_BOOLEAN::~SDAI_BOOLEAN() {
}

int SDAI_BOOLEAN::no_elements() const {
    return 2;
}

SDAI_BOOLEAN::SDAI_BOOLEAN( int i ) {
    if( i == 0 ) {
        v =  BFalse ;
    } else {
        v =  BTrue ;
    }
}

SDAI_BOOLEAN::SDAI_BOOLEAN( const SDAI_LOGICAL & val )  {
    if( val.asInt() == LUnknown ) {
        // this should set error code sdaiVT_NVLD i.e. Invalid value type.
        v = BUnset;
        return;
    }
    set_value( val );
}

SDAI_BOOLEAN::operator  Boolean() const  {
    switch( v ) {
        case  BFalse :
            return  BFalse ;
        case  BTrue :
            return  BTrue ;
        case  BUnset :
        default:
            return  BUnset ;
    }
}

SDAI_BOOLEAN & SDAI_BOOLEAN::operator= ( const SDAI_LOGICAL & t ) {
    set_value( t.asInt() );
    return *this;
}

SDAI_BOOLEAN & SDAI_BOOLEAN::operator= ( const  Boolean t ) {
    v = t;
    return *this;
}

const char * SDAI_BOOLEAN::element_at( int n )  const {
    switch( n )  {
        case  BFalse :
            return "F";
        case  BTrue :
            return "T";
        default:
            return "UNSET";
    }
}

SDAI_LOGICAL SDAI_BOOLEAN::operator ==( const SDAI_LOGICAL & t ) const {
    if( v == t.asInt() ) {
        return  LTrue ;
    }
    return  LFalse ;
}

///////////////////////////////////////////////////////////////////////////////

SDAI_Enum::SDAI_Enum() {
    v = 0;
}

/**
 * \copydoc set_value( const char * n )
 */
int SDAI_Enum::put( int val ) {
    return set_value( val );
}

/**
 * \copydoc set_value( const char * n )
 */
int SDAI_Enum::put( const char * n ) {
    return set_value( n );
}

/// return 0 if unset otherwise return 1
/// WARNING it appears that exists() will return true after a call to nullify(). is this intended?
int SDAI_Enum::exists() const {
    return !( v > no_elements() );
}
/**
 * change the receiver to an unset status
 * unset is generated to be 1 greater than last element
 */
void SDAI_Enum::nullify() {
    set_value( no_elements() + 1 );
}

/**************************************************************//**
 ** prints out some information on the enumerated item for
 ** debugging purposes
 ** Status:  ok 2/1/91
 ******************************************************************/
void SDAI_Enum::DebugDisplay( ostream & out ) const {
    std::string tmp;
    out << "Current " << Name() << " value: " << endl
        << "  cardinal: " <<  v  << endl
        << "  string: " << asStr( tmp ) << endl
        << "  Part 21 file format: ";
    STEPwrite( out );
    out << endl;

    out << "Valid values are: " << endl;
    int i = 0;
    while( i < ( no_elements() + 1 ) ) {
        out << i << " " << element_at( i ) << endl;
        i++;
    }
    out << "\n";
}

/**
** Read an Enumeration value
** ENUMERATION = "." UPPER { UPPER | DIGIT } "."
** *note* UPPER is defined as alpha or underscore.
**
** \returns Severity of the error.
** \param err error message and error Severity is written to ErrorDescriptor *err.
** \param AssignVal is:
**  true => value is assigned to the SDAI_Enum;
**  true or false => value is read and appropriate error info is set and returned.
** \param int needDelims is:
**  false => absence of the period delimiters is not an error;
**  true => delimiters must be valid;
**  true or false => non-matching delimiters are flagged as an error
*/
Severity SDAI_Enum::ReadEnum( istream & in, ErrorDescriptor * err, int AssignVal,
                              int needDelims ) {
    if( AssignVal ) {
        set_null();
    }

    std::string str;
    char messageBuf[512];
    messageBuf[0] = '\0';

    in >> ws; // skip white space

    if( in.good() ) {
        char c;
        in.get( c );
        if( c == '.' || isalpha( c ) ) {
            int validDelimiters = 1;
            if( c == '.' ) {
                in.get( c ); // push past the delimiter
                // since found a valid delimiter it is now invalid until the
                //   matching ending delim is found
                validDelimiters = 0;
            }

            // look for UPPER
            if( in.good() && ( isalpha( c ) || c == '_' ) ) {
                str += c;
                in.get( c );
            }

            // look for UPPER or DIGIT
            while( in.good() && ( isalnum( c ) || c == '_' ) ) {
                str += c;
                in.get( c );
            }
            // if character is not the delimiter unread it
            if( in.good() && ( c != '.' ) ) {
                in.putback( c );
            }

            // a value was read
            if( str.length() > 0 ) {
                int i = 0;
                const char * strval = str.c_str();
                std::string tmp;
                while( ( i < no_elements() )  &&
                        ( strcmp( ( char * )StrToUpper( strval, tmp ),
                                  element_at( i ) ) != 0 ) ) {
                    ++i;
                }
                if( no_elements() == i ) {
                    //  exhausted all the possible values
                    err->GreaterSeverity( SEVERITY_WARNING );
                    err->AppendToDetailMsg( "Invalid Enumeration value.\n" );
                    err->AppendToUserMsg( "Invalid Enumeration value.\n" );
                } else {
                    if( AssignVal ) {
                        v = i;
                    }
                }

                // now also check the delimiter situation
                if( c == '.' ) { // if found ending delimiter
                    // if expecting delim (i.e. validDelimiter == 0)
                    if( !validDelimiters ) {
                        validDelimiters = 1; // everything is fine
                    } else { // found ending delimiter but no initial delimiter
                        validDelimiters = 0;
                    }
                }
                // didn't find any delimiters at all and need them.
                else if( needDelims ) {
                    validDelimiters = 0;
                }

                if( !validDelimiters ) {
                    err->GreaterSeverity( SEVERITY_WARNING );
                    if( needDelims )
                        sprintf( messageBuf,
                                 "Enumerated value has invalid period delimiters.\n" );
                    else
                        sprintf( messageBuf,
                                 "Mismatched period delimiters for enumeration.\n" );
                    err->AppendToDetailMsg( messageBuf );
                    err->AppendToUserMsg( messageBuf );
                }
                return err->severity();
            }
            // found valid or invalid delimiters with no associated value
            else if( ( c == '.' ) || !validDelimiters ) {
                err->GreaterSeverity( SEVERITY_WARNING );
                err->AppendToDetailMsg(
                    "Enumerated has valid or invalid period delimiters with no value.\n"
                );
                err->AppendToUserMsg(
                    "Enumerated has valid or invalid period delimiters with no value.\n"
                );
                return err->severity();
            } else { // no delims and no value
                err->GreaterSeverity( SEVERITY_INCOMPLETE );
            }

        } else if( ( c == ',' ) || ( c == ')' ) ) {
            in.putback( c );
            err->GreaterSeverity( SEVERITY_INCOMPLETE );
        } else {
            in.putback( c );
            err->GreaterSeverity( SEVERITY_WARNING );
            sprintf( messageBuf, "Invalid enumeration value.\n" );
            err->AppendToDetailMsg( messageBuf );
            err->AppendToUserMsg( messageBuf );
        }
    } else { // hit eof (assuming there was no error state for istream passed in)
        err->GreaterSeverity( SEVERITY_INCOMPLETE );
    }
    return err->severity();
}

Severity SDAI_Enum::StrToVal( const char * s, ErrorDescriptor * err, int optional ) {
    istringstream in( ( char * )s ); // sz defaults to length of s

    ReadEnum( in, err, 1, 0 );
    if( ( err->severity() == SEVERITY_INCOMPLETE ) && optional ) {
        err->severity( SEVERITY_NULL );
    }

    return err->severity();
}

/// reads an enumerated value in STEP file format
Severity SDAI_Enum::STEPread( const char * s, ErrorDescriptor * err, int optional ) {
    istringstream in( ( char * )s );
    return STEPread( in, err, optional );
}

/// reads an enumerated value in STEP file format
Severity SDAI_Enum::STEPread( istream & in, ErrorDescriptor * err, int optional ) {
    ReadEnum( in, err, 1, 1 );
    if( ( err->severity() == SEVERITY_INCOMPLETE ) && optional ) {
        err->severity( SEVERITY_NULL );
    }

    return err->severity();
}

const char * SDAI_Enum::asStr( std::string & s ) const  {
    if( exists() ) {
        s = element_at( v );
        return s.c_str();
    } else {
        s.clear();
        return "";
    }
}

void SDAI_Enum::STEPwrite( ostream & out )  const  {
    if( is_null() ) {
        out << '$';
    } else {
        std::string tmp;
        out << "." <<  asStr( tmp ) << ".";
    }
}

const char * SDAI_Enum::STEPwrite( std::string & s ) const {
    if( is_null() ) {
        s.clear();
    } else {
        std::string tmp;
        s = ".";
        s.append( asStr( tmp ) );
        s.append( "." );
    }
    return const_cast<char *>( s.c_str() );
}

Severity SDAI_Enum::EnumValidLevel( istream & in, ErrorDescriptor * err,
                                    int optional, char * tokenList,
                                    int needDelims, int clearError ) {
    if( clearError ) {
        err->ClearErrorMsg();
    }

    in >> ws; // skip white space
    char c = ' ';
    c = in.peek();
    if( c == '$' || in.eof() ) {
        if( !optional ) {
            err->GreaterSeverity( SEVERITY_INCOMPLETE );
        }
        if( in ) {
            in >> c;
        }
        CheckRemainingInput( in, err, "enumeration", tokenList );
        return err->severity();
    } else {
        ErrorDescriptor error;

        ReadEnum( in, &error, 0, needDelims );
        CheckRemainingInput( in, &error, "enumeration", tokenList );

        Severity sev = error.severity();
        if( sev < SEVERITY_INCOMPLETE ) {
            err->AppendToDetailMsg( error.DetailMsg() );
            err->AppendToUserMsg( error.UserMsg() );
            err->GreaterSeverity( error.severity() );
        } else if( sev == SEVERITY_INCOMPLETE && !optional ) {
            err->GreaterSeverity( SEVERITY_INCOMPLETE );
        }
    }
    return err->severity();
}

Severity SDAI_Enum::EnumValidLevel( const char * value, ErrorDescriptor * err,
                                    int optional, char * tokenList,
                                    int needDelims, int clearError ) {
    istringstream in( ( char * )value );
    return EnumValidLevel( in, err, optional, tokenList, needDelims,
                           clearError );
}

/**************************************************************//**
** sets the value of an enumerated attribute case is not important
** in the character based version if value is not acceptable, a
** warning is printed and processing continues
**
**  set_value is the same function as put
**
** Parameter: value to be set
** Status:  ok 2.91
** \returns:  value set
******************************************************************/
int SDAI_Enum::set_value( const char * n )  {
    if( !n || ( !strcmp( n, "" ) ) ) {
        nullify();
        return asInt();
    }

    int i = 0;
    std::string tmp;
    while( ( i < no_elements() )  &&
            ( strcmp( ( char * )StrToUpper( n, tmp ),  element_at( i ) ) != 0 ) ) {
        ++i;
    }
    if( no_elements() == i )  {   //  exhausted all the possible values
        return v = no_elements() + 1; // defined as UNSET
    }
    v = i;
    return v;

}

/**
 * \copydoc set_value( const char * n )
 */
int SDAI_Enum::set_value( const int i )  {
    if( i > no_elements() )  {
        v = no_elements() + 1;
        return v;
    }
    const char * tmp = element_at( i );
    if( tmp[0] != '\0' ) {
        return ( v = i );
    }
    // otherwise
    cerr << "(OLD Warning:) invalid enumeration value " << i
         << " for " <<  Name() << "\n";
    DebugDisplay();
    return  no_elements() + 1 ;
}

SDAI_Enum & SDAI_Enum::operator= ( const int i ) {
    put( i );
    return *this;
}

SDAI_Enum & SDAI_Enum::operator= ( const SDAI_Enum & Senum ) {
    put( Senum.asInt() );
    return *this;
}

ostream & operator<< ( ostream & out, const SDAI_Enum & a ) {
    std::string tmp;
    out << a.asStr( tmp );
    return out;

}
