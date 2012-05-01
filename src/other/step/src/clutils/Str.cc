/*
* NIST Utils Class Library
* clutils/Str.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <Str.h>
#include <sstream>

/******************************************************************
 ** Procedure:  string functions
 ** Description:  These functions take a character or a string and return
 ** a temporary copy of the string with the function applied to it.
 ** Parameters:
 ** Returns:  temporary copy of characters
 ** Side Effects:
 ** Status:  complete
 ******************************************************************/

char ToLower( const char c ) {
    if( isupper( c ) ) {
        return ( tolower( c ) );
    } else {
        return ( c );
    }

}

char ToUpper( const char c ) {
    if( islower( c ) ) {
        return ( toupper( c ) );
    } else {
        return ( c );
    }
}

// Place in strNew a lowercase version of strOld.
char * StrToLower( const char * strOld, char * strNew ) {
    int i = 0;

    while( strOld[i] != '\0' ) {
        strNew[i] = ToLower( strOld[i] );
        i++;
    }
    strNew[i] = '\0';
    return strNew;
}

const char * StrToLower( const char * word, std::string & s ) {
    char newword [BUFSIZ];
    int i = 0;

    while( word [i] != '\0' ) {
        newword [i] = ToLower( word [i] );
        ++i;
    }
    newword [i] = '\0';
    s = newword;
    return const_cast<char *>( s.c_str() );
}

const char * StrToUpper( const char * word, std::string & s ) {
    char newword [BUFSIZ];
    int i = 0;

    while( word [i] != '\0' ) {
        newword [i] = ToUpper( word [i] );
        ++i;
    }
    newword [i] = '\0';
    s = newword;
    return const_cast<char *>( s.c_str() );
}

const char * StrToConstant( const char * word, std::string & s ) {
    char newword [BUFSIZ];
    int i = 0;

    while( word [i] != '\0' ) {
        if( word [i] == '/' || word [i] == '.' ) {
            newword [i] = '_';
        } else {
            newword [i] = ToUpper( word [i] );
        }
        ++i;
    }
    newword [i] = '\0';
    s = newword;
    return const_cast<char *>( s.c_str() );
}

/**************************************************************//**
 ** \fn  StrCmpIns (const char * str1, const char * str2)
 ** \returns  Comparison result
 ** Compares two strings case insensitive (lowercase).
 ** Returns < 0  when str1 less then str2
 **         == 0 when str1 equals str2
 **         > 0  when str1 greater then str2
 ******************************************************************/
int StrCmpIns( const char * str1, const char * str2 ) {
    char c1, c2;
    while ((c1 = tolower(*str1)) == (c2 = tolower(*str2)) && c1 != '\0') {
        str1++;
        str2++;
    }
    return c1 - c2;
}

/**************************************************************//**
 ** \fn  PrettyTmpName (char * oldname)
 ** \returns  a new capitalized name in a static buffer
 ** Capitalizes first char of word, rest is lowercase. Removes '_'.
 ** Status:   OK  7-Oct-1992 kcm
 ******************************************************************/
const char * PrettyTmpName( const char * oldname ) {
    int i = 0;
    static char newname [BUFSIZ];
    newname [0] = '\0';
    while( ( oldname [i] != '\0' ) && ( i < BUFSIZ ) ) {
        newname [i] = ToLower( oldname [i] );
        if( oldname [i] == '_' ) { /*  character is '_'   */
            ++i;
            newname [i] = ToUpper( oldname [i] );
        }
        if( oldname [i] != '\0' ) {
            ++i;
        }
    }
    newname [0] = ToUpper( oldname [0] );
    newname [i] = '\0';
    return newname;
}

/**************************************************************//**
 ** \fn  PrettyNewName (char * oldname)
 ** \returns  a new capitalized name
 ** Capitalizes first char of word, rest is lowercase. Removes '_'.
 ** Side Effects:  allocates memory for the new name
 ** Status:   OK  7-Oct-1992 kcm
 ******************************************************************/
char * PrettyNewName( const char * oldname ) {
    char * name = new char [strlen( oldname ) + 1];
    strcpy( name, PrettyTmpName( oldname ) );
    return name;
}

// This function is used to check an input stream following a read.  It writes
// error messages in the 'ErrorDescriptor &err' argument as appropriate.
// 'const char *delimiterList' argument contains a string made up of delimiters
// that are used to move the file pointer in the input stream to the end of
// the value you are reading (i.e. the ending marked by the presence of the
// delimiter).  The file pointer is moved just prior to the delimiter.  If the
// delimiterList argument is a null pointer then this function expects to find
// EOF.
//
// If input is being read from a stream then a delimiterList should be provided
// so this function can push the file pointer up to but not past the delimiter
// (i.e. not removing the delimiter from the input stream).  If you have a
// string containing a single value and you expect the whole string to contain
// a valid value, you can change the string to an istrstream, read the value
// then send the istrstream to this function with delimiterList set to null
// and this function will set an error for you if any input remains following
// the value.
Severity
CheckRemainingInput( istream & in,
                     ErrorDescriptor * err,
                     const char * typeName,     // used in error message
                     const char * delimiterList ) { // e.g. ",)"
    string skipBuf;
    ostringstream errMsg;

    if( in.eof() ) {
        // no error
        return err->severity();
    } else if( in.bad() ) {
        // Bad bit must have been set during read. Recovery is impossible.
        err->GreaterSeverity( SEVERITY_INPUT_ERROR );
        errMsg << "Invalid " << typeName << " value.\n";
        err->AppendToUserMsg( errMsg.str().c_str() );
        err->AppendToDetailMsg( errMsg.str().c_str() );
    } else {
        // At most the fail bit is set, so stream can still be read.
        // Clear errors and skip whitespace.
        in.clear();
        in >> ws;

        if( in.eof() ) {
            // no error
            return err->severity();
        }

        if( delimiterList != NULL ) {
            // If the next char is a delimiter then there's no error.
            char c = in.peek();
            if( strchr( delimiterList, c ) == NULL ) {
                // Error. Extra input is more than just a delimiter and is
                // now considered invalid. We'll try to recover by skipping
                // to the next delimiter.
                for( in.get( c ); in && !strchr( delimiterList, c ); in.get( c ) ) {
                    skipBuf += c;
                }

                if( strchr( delimiterList, c ) != NULL ) {
                    // Delimiter found. Recovery succeeded.
                    in.putback( c );

                    errMsg << "\tFound invalid " << typeName << " value...\n";
                    err->AppendToUserMsg( errMsg.str().c_str() );
                    err->AppendToDetailMsg( errMsg.str().c_str() );
                    err->AppendToDetailMsg( "\tdata lost looking for end of "
                                            "attribute: " );
                    err->AppendToDetailMsg( skipBuf.c_str() );
                    err->AppendToDetailMsg( "\n" );

                    err->GreaterSeverity( SEVERITY_WARNING );
                } else {
                    // No delimiter found. Recovery failed.
                    errMsg << "Unable to recover from input error while "
                           << "reading " << typeName << " value.\n";
                    err->AppendToUserMsg( errMsg.str().c_str() );
                    err->AppendToDetailMsg( errMsg.str().c_str() );

                    err->GreaterSeverity( SEVERITY_INPUT_ERROR );
                }
            }
        } else if( in.good() ) {
            // Error. Have more input, but lack of delimiter list means we
            // don't know where we can safely resume. Recovery is impossible.
            err->GreaterSeverity( SEVERITY_WARNING );

            errMsg << "Invalid " << typeName << " value.\n";

            err->AppendToUserMsg( errMsg.str().c_str() );
            err->AppendToDetailMsg( errMsg.str().c_str() );
        }
    }
    return err->severity();
}
