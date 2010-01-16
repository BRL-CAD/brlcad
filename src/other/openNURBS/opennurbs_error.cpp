/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2007 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"

// openNURBS Geometry Library Errors and Warnings
//
//   If an error condition occurs during a openNURBS Geometry Library
//   computation, the ON_Error() function is called, the computation is
//   stopped, and an error code (negative integer ) is returned.  If a
//   warning condition occurs during a Trout Lake Geometry Library 
//   computation, the ON_Warning() function is called and the computation
//   continues.
//
//   ON_GetErrorCount()
//   ON_GetWarningCount()
//   ON_Error()
//   ON_Warning()
//

static int ON_ERROR_COUNT = 0;
static int ON_WARNING_COUNT = 0;
static int ON_MATH_ERROR_COUNT = 0;

// 0 = no break
// 1 = break on errors, warnings, and asserts
#if defined(ON_DEBUG)

// debug build defaults
static int ON_DEBUG_BREAK_OPTION = 0; 
static int ON_DEBUG_ERROR_MESSAGE_OPTION = 1; 

#else

// release build defaults
static int ON_DEBUG_BREAK_OPTION = 0; 
static int ON_DEBUG_ERROR_MESSAGE_OPTION = 0; 

#endif


int ON_GetErrorCount(void)
{
  return ON_ERROR_COUNT;	
}	


int ON_GetWarningCount(void)
{
  return ON_WARNING_COUNT;	
}	

int ON_GetMathErrorCount(void)
{
  return ON_MATH_ERROR_COUNT;	
}	

int ON_GetDebugBreak(void)
{
  return ON_DEBUG_BREAK_OPTION?true:false;
}


void ON_EnableDebugBreak( int bEnableDebugBreak )
{
  ON_DEBUG_BREAK_OPTION = bEnableDebugBreak ? 1 : 0;
}


int ON_GetDebugErrorMessage(void)
{
  return ON_DEBUG_ERROR_MESSAGE_OPTION?true:false;
}


void ON_EnableDebugErrorMessage( int bEnableDebugErrorMessage )
{
  ON_DEBUG_ERROR_MESSAGE_OPTION = bEnableDebugErrorMessage ? 1 : 0;
}

// The sMessage[] string is used by ON_Error()
// and ON_Warning() to hold the message.  The static function 
// ON_FormatMessage() is used to do most of the actual formatting.  
// When ON_DEBUG is defined, the "PRINT_STRING" macro is used to 
// print the formatted string.

#define MAX_MSG_LENGTH 2048
static char sMessage[MAX_MSG_LENGTH];
static int FormatMessage(const char*, va_list );


void ON_DebugBreak()
{
#if defined(ON_DEBUG) && defined (ON_COMPILER_MSC)
  static int i = 0;
  if ( 0 == i )
  {
    // Dear Rhino Developer:
    //   If you find this annoying, then either fix the bug
    //   or look at the call stack, figure out why this 
    //   happening, create an RR item and birddog the
    //   error until it gets fixed.
    ::DebugBreak(); // Windows debug break
    i = 1;
  }
  else
#endif

  if ( ON_DEBUG_BREAK_OPTION )
  {
#if defined(ON_COMPILER_MSC)
    ::DebugBreak(); // Windows debug break
#endif
  }

}

void ON_MathError( 
        const char* sModuleName,
        const char* sErrorType,
        const char* sFunctionName
        )
{
  ON_MATH_ERROR_COUNT++; // <- Good location for a debugger breakpoint.

  if ( !sModuleName)
    sModuleName = "";
  if ( !sErrorType )
    sErrorType = "";
  if ( !sFunctionName )
    sFunctionName = "";

  ON_Error(__FILE__,__LINE__,
           "Math library or floating point ERROR # %d module=%s type=%s function=%s",
           ON_MATH_ERROR_COUNT, 
           sModuleName, // rhino.exe, opennurbs.dll, etc.
           sErrorType,   
           sFunctionName 
           );
}	


#if defined(ON_COMPILER_MSC)
// Disable the MSC /W4 warning
//   C4189: 'first_error' : local variable is initialized but not referenced
// on the line
//   int first_error = 0.  
// The warning disable is here because MS is ignoring it
// if I put it inside of the function.
#pragma warning( push )
#pragma warning( disable : 4189 ) 
#endif

void ON_Error(const char* sFileName, int line_number, 
              const char* sFormat, ...)
{
  int bPrintMessage = false;
  int rc = 0;
  ON_ERROR_COUNT++; // <- Good location for a debugger breakpoint.
  sMessage[0] = 0;


#if defined(ON_DEBUG) && !defined(ON_COMPILER_GNU)
  if ( 1 == ON_ERROR_COUNT )
  {
    // If you have a really buggy situation, use this
    // breakpoint instead. (Code generates gcc warning)
    int first_error = 0; // <- Good location for a debugger breakpoint.
  }
#endif

  if (ON_DEBUG_ERROR_MESSAGE_OPTION)
  {
    if ( ON_ERROR_COUNT < 50 )
    {
      // put file and line number info for debug mode
      sprintf(sMessage,"openNURBS ERROR # %d %s:%d ",ON_ERROR_COUNT,sFileName,line_number);
      bPrintMessage = true;
    }
    else if ( 50 == ON_ERROR_COUNT )
    {
      sprintf(sMessage,"openNURBS ERROR # %d - Too many errors.  No more printed messages.",ON_ERROR_COUNT);
      bPrintMessage = true;
    }
  }

  if ( bPrintMessage ) {
    if (sFormat)  {
      // append formatted error message to sMessage[]
      va_list args;
      va_start(args, sFormat);
      rc = FormatMessage(sFormat,args);
      va_end(args);
    }
    if (!rc && bPrintMessage ) { 
      ON_ErrorMessage(1,sMessage);
    }
  }

  ON_DebugBreak();
}

#if defined(ON_COMPILER_MSC)
#pragma warning(pop)
#endif



void ON_Warning(const char* sFileName, int line_number, 
                const char* sFormat, ...)
{
  int bPrintMessage = false;
  int rc = 0;
  ON_WARNING_COUNT++; // <- Good location for a debugger breakpoint.
  sMessage[0] = 0;

  if (ON_DEBUG_ERROR_MESSAGE_OPTION)
  {
    // put file and line number info for debug mode
    sprintf(sMessage,"openNURBS WARNING # %d %s:%d ",ON_ERROR_COUNT,sFileName,line_number);
    bPrintMessage = true;
  }

  if ( bPrintMessage ) {
    if (sFormat)  {
      // append formatted error message to sMessage[]
      va_list args;
      va_start(args, sFormat);
      rc = FormatMessage(sFormat,args);
      va_end(args);
    }
    if (!rc && bPrintMessage ) {
      ON_ErrorMessage(0,sMessage);
    }
  }

  ON_DebugBreak();
}


void ON_Assert(int bCondition,
               const char* sFileName, int line_number, 
               const char* sFormat, ...)
{
  if ( !bCondition ) 
  {
    int bPrintMessage = false;
    int rc = 0;
    ON_ERROR_COUNT++; // <- Good location for a debugger breakpoint.
    sMessage[0] = 0;

    if (ON_DEBUG_ERROR_MESSAGE_OPTION)
    {
      // put file and line number info for debug mode
      sprintf(sMessage,"openNURBS ON_Assert ERROR # %d %s:%d ",ON_ERROR_COUNT,sFileName,line_number);
      bPrintMessage = true;
    }

    if ( bPrintMessage ) {
      if (sFormat)  {
        // append formatted error message to sMessage[]
        va_list args;
        va_start(args, sFormat);
        rc = FormatMessage(sFormat,args);
        va_end(args);
      }
      if (!rc && bPrintMessage ) { 
        ON_ErrorMessage(2,sMessage);
      }
    }

    ON_DebugBreak();
  }
}

static int FormatMessage(const char* format, va_list args)
{
  // appends formatted message to sMessage[]
  int len = ((int)strlen(sMessage));
  if (len < 0 )
    return -1;
  if (MAX_MSG_LENGTH-1-len < 2)
    return -1;
  sMessage[MAX_MSG_LENGTH-1] = 0;
  on_vsnprintf(sMessage+len, MAX_MSG_LENGTH-1-len, format, args);
  return 0;
}	


