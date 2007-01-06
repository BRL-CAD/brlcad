/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
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

/*
////////////////////////////////////////////////////////////////
//
//   Includes all system headers required to use the openNURBS toolkit.
//
////////////////////////////////////////////////////////////////
*/

#if !defined(OPENNURBS_SYSTEM_INC_)
#define OPENNURBS_SYSTEM_INC_


/* compiler choice */
#if defined(_MSC_VER)

/* using a Microsoft compiler */
#define ON_COMPILER_MSC

#if _MSC_VER >= 1300
#define ON_COMPILER_MSC1300
// If you are using VC7/.NET and are having trouble linking 
// to functions that have whcar_t types in arguments, then
// read the documentation about the wchar_t type and
// the /Zc:wchar_t compiler option.

#if _MSC_VER >= 1400
#define ON_COMPILER_MSC1400

#if !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE
// Visual Studio 2005 issues a C4996 warning for lots of
// standard C runtime functions that take string pointers.
// The _CRT_SECURE_NO_DEPRECATE suppresses these warnings.
// If you are an IT manager type and really care about these
// sorts of things, then comment out the define.
#endif

#endif

#endif

#endif

#if defined(__GNUC__) || defined(__GNUG_) || defined(__GNUC_) || defined(_GNU_SOURCE)
/* using Gnu's compiler */
#if !defined(ON_COMPILER_GNU)
#define ON_COMPILER_GNU
#endif
#endif

#if defined(__BORLANDC__)
/* using Borland's compiler */
#define ON_COMPILER_BORLAND
#endif

/*
// Define ON_NO_WINDOWS if you are compiling on a Windows system but want
// to explicitly exclude inclusion of windows.h.
*/

#if !defined(ON_NO_WINDOWS)

/*
/////////////////////////////////////////////////////////////////////////
//
// Begin Windows system includes - 
*/
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)

/*
// From windows.h openNURBS only needs definitions of BOOL, TRUE,
// and FALSE, and a declarations of OutputDebugString(), and
// WideCharToMultiByte().  These 
// defines disable the inclusion of most of the Windows garbage.
*/

#if !defined(_WINDOWS_)
/* windows.h has not been read - read just what we need */
#define WIN32_LEAN_AND_MEAN  /* Exclude rarely-used stuff from Windows headers */
#include <windows.h>
#endif

/*
// if ON_OS_WINDOWS is defined, debugging and error
// handing uses some Windows calls and ON_String
// includes resource support.
*/

#if !defined(ON_OS_WINDOWS)
#define ON_OS_WINDOWS
#endif

#if defined(ON_OS_WINDOWS) && !defined(NOGDI)
// ok to use Windows GDI RECT, LOGFONT, ... stucts.
#define ON_OS_WINDOWS_GDI
#endif

#if defined(_MSC_VER)
/* Microsoft's Visual C/C++ requires functions that use vargs to be declared with __cdecl */
#define ON_VARG_CDECL __cdecl
#endif

#endif

#endif

#include <stdlib.h>
#include <memory.h>
//#include <malloc.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <float.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>

#if defined(ON_COMPILER_IRIX)
#include <alloca.h>
#endif

#if !defined(ON_COMPILER_BORLAND)
#include <wchar.h>
#endif

#if defined(ON_OS_WINDOWS)
#include <io.h>
#include <sys\stat.h>
#include <tchar.h>

// ON_CreateUuid calls Windows's ::UuidCreate() which
// is declared in Rpcdce.h and defined in Rpcrt4.lib.
#include <Rpc.h>

#endif

#if defined(ON_COMPILER_GNU)
#include <sys/types.h>
#include <sys/stat.h>
#include <wctype.h>
#endif

#if defined (cplusplus) || defined(_cplusplus) || defined(__cplusplus)
// C++ system includes

#if !defined(ON_CPLUSPLUS)
#define ON_CPLUSPLUS
#endif

#include <new> // for declaration of placement versions of new used in onClassArray<>.

#endif

#if !defined(ON_VARG_CDECL)
#define ON_VARG_CDECL
#endif

#if !defined(ON_OS_WINDOWS)

/* define wchar_t, BOOL, TRUE, FALSE, NULL */

#if !defined(BOOL) && !defined(_WINDEF_)
typedef int BOOL;
#endif

#if !defined(TRUE)
#define TRUE true
#endif

#if !defined(FALSE)
#define FALSE false
#endif

#if !defined(NULL)
#define NULL 0
#endif

#if !defined(_WCHAR_T_DEFINED)
// If you are using VC7/.NET and are having trouble linking 
// to functions that have whcar_t types in arguments, then
// read the documentation about the wchar_t type and
// the /Zc:wchar_t compiler option.  Since 

/* 16-bit wide character ("UNICODE") */

#if !defined(ON_COMPILER_MSC) && !defined(ON_COMPILER_GNU)
typedef unsigned short wchar_t;
#endif

#define _WCHAR_T_DEFINED
#endif

#endif


// As 64 bit compilers become more common, the definitions
// of the next 6 typedefs may need to vary with compiler.
// As much as possible, the size of runtime types is left 
// up to the compiler so performance and ease of use can 
// be maximized.  In the rare cases where it is critical 
// to use an integer that is exactly 16 bits, 32 bits 
// or 64 bits, the ON__INT16, ON__INT32, and ON__INT64
// typedefs are used.


#if defined(_WIN64) || defined(WIN64) || defined(__LP64__)
// 64 bit (8 byte) pointers
#define ON_SIZEOF_POINTER 8
#else
// 32 bit (4 byte) pointers
#define ON_SIZEOF_POINTER 4
#endif

// 16 bit integer
typedef short ON__INT16;

// 16 bit unsigned integer
typedef unsigned short ON__UINT16;

// 32 bit integer
typedef int ON__INT32;

// 32 bit unsigned integer
typedef unsigned int ON__UINT32;

#if defined(ON_COMPILER_GNU)

// GNU uses long long

// 64 bit integer
typedef long long ON__INT64;

// 64 bit unsigned integer
typedef unsigned long long ON__UINT64;

#else

// Microsoft uses __int64

// 64 bit integer
typedef __int64 ON__INT64;

// 64 bit unsigned integer
typedef unsigned __int64 ON__UINT64;

#endif


// on_vsnprintf()/on_vsnwprintf() call _vsnprintf()/_vsnwprintf() in Windows
// and something equivalent in other OSs

int on_vsnprintf( char *buffer, size_t count, const char *format, va_list argptr );

int on_vsnwprintf( wchar_t *buffer, size_t count, const wchar_t *format, va_list argptr );


#endif

