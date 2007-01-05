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

// opennurbs_dll.cpp : Defines the entry point for the Windows DLL application.
//
#include "opennurbs.h"

#if defined(ON_OS_WINDOWS) && defined(ON_DLL_EXPORTS)

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
  if ( DLL_PROCESS_ATTACH == ul_reason_for_call ) 
  {
    ON_ClassId::IncrementMark(); // make sure each DLL that each process that 
                                 // uses OpenNURBS has a unique mark.
  }

  return TRUE;
}


#endif
