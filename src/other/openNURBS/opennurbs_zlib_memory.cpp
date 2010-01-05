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

// If you are using opennurbs as a statically linked library, then
// you may make calls to the same zlib that opennurbs uses.  This
// zlib is compiled with z_ symbol projectection.  All the necessary
// header files are included by opennurbs.h.
// 
// If you are using opennurbs as a DLL or writing a Rhino plug-in
// and you want to use the same zlib that opennurbs uses, then
// compile opennurbs_zlib_memory.cpp (this file) into your application
// and statically link with the zlib library. All the necessary
// header files are included by opennurbs.h.


voidpf zcalloc (voidpf, unsigned items, unsigned size)
{
    return oncalloc(items, size);
}

void  zcfree (voidpf, voidpf ptr)
{
    onfree(ptr);
}
