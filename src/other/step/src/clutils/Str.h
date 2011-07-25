
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

/* $Id: Str.h,v 3.0.1.3 1997/11/05 22:33:52 sauderd DP3.1 $  */ 

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <ctype.h>

//#include <std.h> // not found in CenterLine C++
// the two includes stdio.h and stdlib.h below are replacing std.h since 
// CenterLine doesn't have std.h

#include <stdio.h> // added to have the definition for BUFSIZE
#include <stdlib.h> 
#include <string.h>
#include <errordesc.h>

char         ToLower (const char c);
char         ToUpper  (const char c);
char *       StrToLower (const char *, char *);
const char * StrToLower (const char * word, std::string &s);
const char * StrToUpper (const char * word, std::string &s);
const char * StrToConstant (const char * word, std::string &s);
const char * PrettyTmpName (const char * oldname);
char *       PrettyNewName (const char * oldname);
int          StrCmpIns (const char *, const char *);
char *       EntityClassName ( char * oldname);

extern Severity CheckRemainingInput
   (istream &in, ErrorDescriptor *err, 
    const char *typeName, // used in error message
    const char *tokenList); // e.g. ",)"


#endif 
