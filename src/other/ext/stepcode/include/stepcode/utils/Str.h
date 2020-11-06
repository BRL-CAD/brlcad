
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

#include "export.h"
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/errordesc.h"

#ifndef STRING_DELIM
#define STRING_DELIM '\''
#endif

STEPCODE_UTILS_EXPORT char         ToLower( const char c );
STEPCODE_UTILS_EXPORT char         ToUpper( const char c );
STEPCODE_UTILS_EXPORT char    *    StrToLower( const char *, char * );
STEPCODE_UTILS_EXPORT const char * StrToLower( const char * word, std::string & s );
STEPCODE_UTILS_EXPORT const char * StrToUpper( const char * word, std::string & s );
STEPCODE_UTILS_EXPORT const char * StrToConstant( const char * word, std::string & s );
STEPCODE_UTILS_EXPORT int          StrCmpIns( const char * str1, const char * str2 );
STEPCODE_UTILS_EXPORT const char * PrettyTmpName( const char * oldname );
STEPCODE_UTILS_EXPORT char    *    PrettyNewName( const char * oldname );
STEPCODE_UTILS_EXPORT char    *    EntityClassName( char * oldname );

STEPCODE_UTILS_EXPORT bool StrEndsWith( const std::string & s, const char * suffix );
STEPCODE_UTILS_EXPORT std::string  GetLiteralStr( istream & in, ErrorDescriptor * err );

extern STEPCODE_UTILS_EXPORT Severity CheckRemainingInput
( istream & in, ErrorDescriptor * err,
  const char * typeName, // used in error message
  const char * tokenList ); // e.g. ",)"


#endif
