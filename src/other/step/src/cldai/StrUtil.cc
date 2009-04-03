//
//  File: StrUtil.cc
//
//  Description: Implementation of string utility functions 
//
//  Rev:         $Revision: 1.1 $
//  Created:     $Date: 1998/01/09 21:39:30 $
//  Author:      $Author: sauderd $
//
//  Copyright Industrial Technology Institute 1997 -- All Rights Reserved
//

#include <string.h>
#include <time.h>


#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#include <ostore/coll.hh>
#endif

#include "StrUtil.hh"
//#include "imppackages.hh"
#include "ErrorRpt.hh"


//
//  Duplicate a string
//
//   Returns a pointer to a new string that is a  duplicate of the string
// pointed to by srcStr.  If the destination string pointer is not NULL,
// the memory the destination string points to is deallocated using delete().
// The space for the new string is obtained using ::new(). If the new string
// cannot be created, a null pointer is returned.
//
char * strDup(char *destStr, const char *srcStr)
{
  if (destStr != NULL)
    {
      delete [] destStr;

      destStr = NULL;
    }

  if (srcStr != NULL)
   {
     destStr = new char[strlen(srcStr)+1];

     if (destStr != NULL)
       {
         strcpy(destStr, srcStr);
       }
     else
       {
         reportServerError(serverNoMemoryError);
       }
   }

  return destStr;
}

#ifdef __OSTORE__
char * strDup(char * destStr, const char * srcStr, os_segment * segment) 
{
  if (destStr != NULL)
    {
      delete [] destStr;

      destStr = NULL;
    }

  if (srcStr != NULL)
   {
     destStr = new (segment, os_typespec::get_char(),
                        strlen(srcStr)+1) char[strlen(srcStr) + 1];

     if (destStr != NULL)
       {
         strcpy(destStr, srcStr);
       }
     else
       {
         reportServerError(serverNoMemoryError);
       }
   }

  return destStr;
}

char * strDup(char * destStr, const char * srcStr, os_database * database) 
{
  if (destStr != NULL)
    {
      delete [] destStr;

      destStr = NULL;
    }

  if (srcStr != NULL)
   {
     destStr = new (database, os_typespec::get_char(),
                        strlen(srcStr)+1) char[strlen(srcStr) + 1];

     if (destStr != NULL)
       {
         strcpy(destStr, srcStr);
       }
     else
       {
         reportServerError(serverNoMemoryError);
       }
   }

  return destStr;
}
#endif __OSTORE__

//
//  Free the memory allocated to a string
//
void strFree(char * str)
{
  if (str != NULL)
    {
      delete [] str;
    }
}

//
//  Return the current date and time as a string
//
//    It is the caller's responsibility to deallocate the memory for the string.
//
char * getCurrentDate(void)
{
  // need to get format of time stamp from ISO 8601

#define TIME_STR_LEN  256

  char * timeStr = NULL;
  char time_buf[TIME_STR_LEN];
  char * time_format = NULL; // use default locale format for now
  struct timeval time_of_day;
  struct tm * time_struct;

  (void) gettimeofday(&time_of_day, (void *) NULL);

  time_struct = localtime(&(time_of_day.tv_sec));

  int length = strftime(time_buf, TIME_STR_LEN, time_format , time_struct);

  if (length > 0)
    {
      timeStr = strDup(timeStr, time_buf);
    }
  else
    {
      reportServerError(serverTimeFormatError);
    }

  return timeStr;
}

