/* $Header$ */
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

#if !defined(OPENNURBS_VERSION_DEFINITION)
#error Do NOT include opennurbs_version.h in your code.  Use ON::Version() instead.
#endif

// OpenNURBS users:
//   Do not change OPENNURBS_VERSION or the OpenNURBS code
//   that reads 3DM files not work correctly.

// The YYYYMMDD portion of the _DEBUG and release
// version numbers is always the same.  
// The last digit of a debug build version number is 9. 
// The last digit of a pulic opennurbs release build version number is 0.
// The last digit of a Rhino V4 release build version number is 4.
// The last digit of a Rhino V5 release build version number is 5.
#if defined(_DEBUG)
#define OPENNURBS_VERSION 200707189
#else
#define OPENNURBS_VERSION 200707180
#endif

