
/*
* NIST STEP Core Class Library
* clstepcore/sdaiBinary.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sstream>
#include <sdai.h>
#include "sc_memmgr.h"

SDAI_Binary::SDAI_Binary( const char * str, int max ) {
    content = std::string( str, max );
}

SDAI_Binary::SDAI_Binary( const std::string & s ) {
    content = std::string( s );
}

SDAI_Binary::~SDAI_Binary( void ) {
}

SDAI_Binary & SDAI_Binary::operator= ( const char * s ) {
    content = std::string( s );
    return *this;
}

void SDAI_Binary::clear( void ) {
    content.clear();
}

bool SDAI_Binary::empty( void ) const {
    return content.empty();
}

const char * SDAI_Binary::c_str( void ) const {
    return content.c_str();
}

void SDAI_Binary::STEPwrite( ostream & out ) const {
    const char * str = 0;
    if( empty() ) {
        out << "$";
    } else {
        out << '\"';
        str = c_str();
        while( *str ) {
            out << *str;
            str++;
        }
        out << '\"';
    }
}

const char * SDAI_Binary::STEPwrite( std::string & s ) const {
    const char * str = 0;
    if( empty() ) {
        s = "$";
    } else {
        s = "\"";
        str = c_str();
        while( *str ) {
            s += *str;
            str++;
        }
        s += BINARY_DELIM;
    }
    return const_cast<char *>( s.c_str() );
}

Severity SDAI_Binary::ReadBinary( istream & in, ErrorDescriptor * err, int AssignVal,
                                  int needDelims ) {
    if( AssignVal ) {
        clear();
    }

    std::string str;
    char messageBuf[512];
    messageBuf[0] = '\0';

    in >> ws; // skip white space

    if( in.good() ) {
        char c;
        in.get( c );
        if( ( c == '\"' ) || isxdigit( c ) ) {
            int validDelimiters = 1;
            if( c == '\"' ) {
                in.get( c ); // push past the delimiter
                // since found a valid delimiter it is now invalid until the
                //   matching ending delim is found
                validDelimiters = 0;
            }
            while( in.good() && isxdigit( c ) ) {
                str += c;
                in.get( c );
            }
            if( in.good() && ( c != '\"' ) ) {
                in.putback( c );
            }
            if( AssignVal && ( str.length() > 0 ) ) {
                operator= ( str.c_str() );
            }

            if( c == '\"' ) { // if found ending delimiter
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
                             "Binary value missing double quote delimiters.\n" );
                else
                    sprintf( messageBuf,
                             "Mismatched double quote delimiters for binary.\n" );
                err->AppendToDetailMsg( messageBuf );
                err->AppendToUserMsg( messageBuf );
            }
        } else {
            err->GreaterSeverity( SEVERITY_WARNING );
            sprintf( messageBuf, "Invalid binary value.\n" );
            err->AppendToDetailMsg( messageBuf );
            err->AppendToUserMsg( messageBuf );
        }
    } else {
        err->GreaterSeverity( SEVERITY_INCOMPLETE );
    }
    return err->severity();
}

Severity SDAI_Binary::StrToVal( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * )s ); // sz defaults to length of s
    return ReadBinary( in, err, 1, 0 );
}

/////////////////////////////////////////////////

/// reads a binary in exchange file format delimited by double quotes
Severity SDAI_Binary::STEPread( istream & in, ErrorDescriptor * err ) {
    return ReadBinary( in, err, 1, 1 );
}

Severity SDAI_Binary::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * )s );
    return STEPread( in, err );
}

/***************************************************************************//**
** * attrValue is validated.
** * err is set if there is an error
** * If optional is 1 then a missing value will be valid otherwise severity
**   incomplete will be set.
** * If you don't know if the value may be optional then set it false and
**   check to see if SEVERITY_INCOMPLETE is set. You can change it later to
**   SEVERITY_NULL if it is valid for the value to be missing.  No error
**   'message' will be associated with the value being missing so you won\'t
**   have to worry about undoing an error message.
** * tokenList contains characters that terminate the expected value.
** * If tokenList is not zero then the value is expected to terminate before
**   a character found in tokenlist.  All values read up to the
**   terminating character (delimiter) must be valid or err will be set with an
**   appropriate error message.  White space between the value and the
**   terminating character is not considered to be invalid.  If tokenList is
**   null then attrValue must only contain a valid value and nothing else
**   following.
******************************************************************************/
Severity SDAI_Binary::BinaryValidLevel( istream & in, ErrorDescriptor * err,
                                        int optional, char * tokenList,
                                        int needDelims, int clearError ) {
    if( clearError ) {
        err->ClearErrorMsg();
    }

    in >> ws; // skip white space
    char c = in.peek();
    if( c == '$' || in.eof() ) {
        if( !optional ) {
            err->GreaterSeverity( SEVERITY_INCOMPLETE );
        }
        if( in ) {
            in >> c;
        }
        CheckRemainingInput( in, err, "binary", tokenList );
        return err->severity();
    } else {
        ErrorDescriptor error;
        ReadBinary( in, &error, 0, needDelims );
        CheckRemainingInput( in, &error, "binary", tokenList );

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

Severity SDAI_Binary::BinaryValidLevel( const char * value, ErrorDescriptor * err,
                                        int optional, char * tokenList,
                                        int needDelims, int clearError ) {
    istringstream in( ( char * )value );
    return BinaryValidLevel( in, err, optional, tokenList,
                             needDelims, clearError );
}
