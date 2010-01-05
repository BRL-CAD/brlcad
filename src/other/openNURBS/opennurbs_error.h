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

#if !defined(OPENNURBS_ERROR_INC_)
#define OPENNURBS_ERROR_INC_

/*
// Macros used to log errors and warnings.  The ON_Warning() and ON_Error()
// functions are defined in opennurbs_error.cpp.
*/

#define ON_ERROR(msg) ON_Error(__FILE__,__LINE__,msg)
#define ON_WARNING(msg) ON_Warning(__FILE__,__LINE__,msg)

// 15 May 2008 Dale Lear
// Never, ever, make system error handling macros that work
// differently in release and debug builds.
//#if defined(ON_DEBUG)
#define ON_ASSERT(cond) ON_Assert(cond,__FILE__,__LINE__, #cond " is false")
#define ON_ASSERT_OR_RETURN(cond,returncode) do{if (!(cond)) {ON_Assert(false,__FILE__,__LINE__, #cond " is false");return(returncode);}}while(0)
//#else
//#define ON_ASSERT(cond)
//#define ON_ASSERT_OR_RETURN(cond,returncode) do{if (!(cond)) return(returncode);}while(0)
//#endif

ON_BEGIN_EXTERNC

/*
// All error/warning messages are sent to ON_ErrorMessage().  Replace the
// default handler (defined in opennurbs_error_message.cpp) with something
// that is appropriate for debugging your application.
*/
ON_DECL
void ON_ErrorMessage( 
       int,         /* 0 = warning message, 1 = serious error message, 2 = assert failure */
       const char*  
       ); 

/*
Returns:
  Number of opennurbs errors since program started.
*/
ON_DECL
int     ON_GetErrorCount(void);

/*
Returns:
  Number of opennurbs warnings since program started.
*/
ON_DECL
int     ON_GetWarningCount(void);

/*
Returns:
  Number of math library or floating point errors that have 
  been handled since program started.
*/
ON_DECL
int     ON_GetMathErrorCount(void);

ON_DECL
int     ON_GetDebugBreak(void);
ON_DECL
void    ON_EnableDebugBreak( int bEnableDebugBreak );

ON_DECL
int     ON_GetDebugErrorMessage(void);
ON_DECL
void    ON_EnableDebugErrorMessage( int bEnableDebugErrorMessage );

ON_DECL
void    ON_Error( const char*, /* sFileName:   __FILE__ will do fine */
                  int,         /* line number: __LINE__ will do fine */
                  const char*, /* printf() style format string */
                  ...          /* printf() style ags */
                  );
ON_DECL
void    ON_Warning( const char*, /* sFileName:   __FILE__ will do fine */
                    int,         /* line number: __LINE__ will do fine */
                    const char*, /* printf() style format string */
                    ...          /* printf() style ags */
                  );
ON_DECL
void    ON_Assert( int,         /* if false, execution is halted in debugged code */
                   const char*, /* sFileName:   __FILE__ will do fine */
                   int,         /* line number: __LINE__ will do fine */
                   const char*, /* printf() style format string */
                   ...          /* printf() style ags */
                  );
ON_DECL
void    ON_MathError( 
        const char*, /* sModuleName */
        const char*, /* sErrorType */
        const char*  /* sFunctionName */
        );

ON_END_EXTERNC

#endif
