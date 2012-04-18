
/*
* NIST STEP Core Class Library
* clstepcore/sdaiString.cc
* April 1997
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sdai.h>
#include <sstream>

SDAI_String & SDAI_String::operator= ( const char * s ) {
    std::string::operator= ( s );
    return *this;
}

void SDAI_String::STEPwrite( ostream & out ) const {
    const char * str = 0;
// strings that exist but do not contain any chars should be written as '',
// not $ --Josh L, 4/28/95
//    if (is_null ())
    if( empty() ) {
        out << "$";
    } else {
        out << "\'";
        str = c_str();
        while( *str ) {
            if( *str == STRING_DELIM ) {
                out << STRING_DELIM;
            }
            out << *str;
            str++;
        }
        out << "\'";
    }
}

void SDAI_String::STEPwrite( std::string & s ) const {
    const char * str = 0;
// null strings should be represented by '', not $ --Josh L, 4/28/95
//    if (is_null ())
    if( empty() ) {
//  s.set_null(); // this would free up space? nope
        s = "$";
    } else {
        s = "\'";
        str = c_str();
        while( *str ) {
            if( *str == STRING_DELIM ) {
                s += STRING_DELIM;
            }
            s += *str;
            str++;
        }
        s += STRING_DELIM;
    }
}

Severity SDAI_String::StrToVal( const char * s ) {
    operator= ( s );
    if( ! strcmp( c_str(),  s ) ) {
        return SEVERITY_NULL ;
    } else {
        return SEVERITY_INPUT_ERROR;
    }
}

/**
 *  STEPread reads a string in exchange file format
 *  starting with a single quote
 */
Severity SDAI_String::STEPread( istream & in, ErrorDescriptor * err ) {
    int foundEndQuote = 0; // need so this string is not ok: 'hi''
    clear();  // clear the old string
    char c;
    in >> ws; // skip white space
    in >> c;

    // remember the current format state to restore the previous settings
    ios_base::fmtflags flags = in.flags();
    in.unsetf( ios::skipws );

    if( c == STRING_DELIM ) {
        while( ( c != '\0' ) && in.good() && in.get( c ) ) {
            if( c == STRING_DELIM )   {
                in.get( c );
                if( ! in.good() ) {
                    // it is the final quote and no extra char was read
                    foundEndQuote = 1;
                    c = '\0';
                    continue;
                } else if( !( c == STRING_DELIM ) ) {
                    // it is the final quote and extra char was read
                    in.putback( c ); // put back non-quote extra char
                    foundEndQuote = 1;
                    c = '\0';
                    continue;
                }
                // else { ; } // do nothing it is an embedded quote
            }
            operator+= ( c );
        }

        if( foundEndQuote ) {
            return SEVERITY_NULL;
        } else {
            // non-recoverable error
            err->AppendToDetailMsg( "Missing closing quote on string value.\n" );
            err->AppendToUserMsg( "Missing closing quote on string value.\n" );
            err->GreaterSeverity( SEVERITY_INPUT_ERROR );
            return SEVERITY_INPUT_ERROR;
        }
    }
    //  otherwise there was not a quote
    in.putback( c );
    in.flags( flags ); // set the format state back to previous settings

    clear();

    return err -> GreaterSeverity( SEVERITY_INCOMPLETE );
}

Severity SDAI_String::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * )s );
    return STEPread( in, err );
}
