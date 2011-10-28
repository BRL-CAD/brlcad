//
//  File: StrUtil.hh
//
//  Description: Declaration of string utility functions.
//
//  Rev:         $Revision: 1.1 $
//  Created:     $Date: 1998/01/09 21:39:30 $
//  Author:      $Author: sauderd $
//
//  Copyright Industrial Technology Institute 1997 -- All Rights Reserved
//

#ifndef STRUTIL_HH
#define STRUTIL_HH

//
//   Returns a pointer to a new string that is a  duplicate of the string
// pointed to by srcStr.  If the destination string pointer is not NULL,
// the memory the destination string points to is deallocated using delete().
// The space for the new string is obtained using ::new(). If the new string
// cannot be created, a null pointer is returned.
//
char * strDup(char *destStr, const char *srcStr);

//
//   Free the memory allocated for the given string
//
void strFree(char * str);

//
//   Get the current date, caller has the responsibility to deallocate 
//   the returned string.
//
char * getCurrentDate(void);

#endif

