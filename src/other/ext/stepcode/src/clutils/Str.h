
#ifndef STR_H
#define STR_H

/*
* NIST Utils Class Library
* clutils/Str.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sc_export.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errordesc.h>

#ifndef STRING_DELIM
#define STRING_DELIM '\''
#endif

SC_UTILS_EXPORT char         ToLower( const char c );
SC_UTILS_EXPORT char         ToUpper( const char c );
SC_UTILS_EXPORT char    *    StrToLower( const char *, char * );
SC_UTILS_EXPORT const char * StrToLower( const char * word, std::string & s );
SC_UTILS_EXPORT const char * StrToUpper( const char * word, std::string & s );
SC_UTILS_EXPORT const char * StrToConstant( const char * word, std::string & s );
SC_UTILS_EXPORT int          StrCmpIns( const char * str1, const char * str2 );
SC_UTILS_EXPORT const char * PrettyTmpName( const char * oldname );
SC_UTILS_EXPORT char    *    PrettyNewName( const char * oldname );
SC_UTILS_EXPORT char    *    EntityClassName( char * oldname );

SC_UTILS_EXPORT bool StrEndsWith( const std::string & s, const char * suffix );
SC_UTILS_EXPORT std::string  GetLiteralStr( istream & in, ErrorDescriptor * err );

extern SC_UTILS_EXPORT Severity CheckRemainingInput( std::istream & in, ErrorDescriptor * err,
  const char * typeName, // used in error message
  const char * tokenList ); // e.g. ",)"
extern SC_UTILS_EXPORT Severity CheckRemainingInput( std::istream & in, ErrorDescriptor * err, const std::string typeName, const char * tokenList );

#endif
