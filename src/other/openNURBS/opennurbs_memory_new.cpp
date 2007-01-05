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
#include "opennurbs.h"

#if defined(ON_DLL_EXPORTS) || defined(ON_DLL_IMPORTS)

/* See comments at the start of opennurbs_memory.h and 
// opennurbs_object.cpp for details.
//
*/
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//
// OPTIONAL replacements for operator new and operator delete that use 
// onmalloc()/onfree().  The openNURBS toolkit does NOT require you to
// override new and delete.  
//
// If you choose to not use these overrides and you are using openNURBS
// as a Windows DLL, see the comments at the top of opennurbs_object.cpp.
//
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

#if defined(ON_OS_WINDOWS)
#pragma message( " --- OpenNURBS overriding new and delete" )
#endif

void* operator new[]( size_t sz )
{
  return onmalloc(sz);
}

void operator delete[]( void* p )
{
  onfree(p);
}

void* operator new( size_t sz )
{
  return onmalloc(sz);
}

void operator delete( void* p )
{
  onfree(p);
}

#endif
