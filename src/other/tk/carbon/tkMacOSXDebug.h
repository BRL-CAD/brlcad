/*
 * tkMacOSXDebug.h --
 *
 *	Declarations of Macintosh specific functions for debugging MacOS events,
 *	regions, etc...
 *
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *	The following terms apply to all files originating from Apple
 *	Computer, Inc. ("Apple") and associated with the software
 *	unless explicitly disclaimed in individual files.
 *
 *
 *	Apple hereby grants permission to use, copy, modify,
 *	distribute, and license this software and its documentation
 *	for any purpose, provided that existing copyright notices are
 *	retained in all copies and that this notice is included
 *	verbatim in any distributions. No written agreement, license,
 *	or royalty fee is required for any of the authorized
 *	uses. Modifications to this software may be copyrighted by
 *	their authors and need not follow the licensing terms
 *	described here, provided that the new terms are clearly
 *	indicated on the first page of each file where they apply.
 *
 *
 *	IN NO EVENT SHALL APPLE, THE AUTHORS OR DISTRIBUTORS OF THE
 *	SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 *	INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF
 *	THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
 *	EVEN IF APPLE OR THE AUTHORS HAVE BEEN ADVISED OF THE
 *	POSSIBILITY OF SUCH DAMAGE.  APPLE, THE AUTHORS AND
 *	DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
 *	BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.	 THIS
 *	SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, AND APPLE,THE
 *	AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 *	MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *	GOVERNMENT USE: If you are acquiring this software on behalf
 *	of the U.S. government, the Government shall have only
 *	"Restricted Rights" in the software and related documentation
 *	as defined in the Federal Acquisition Regulations (FARs) in
 *	Clause 52.227.19 (c) (2).  If you are acquiring the software
 *	on behalf of the Department of Defense, the software shall be
 *	classified as "Commercial Computer Software" and the
 *	Government shall have only "Restricted Rights" as defined in
 *	Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 *	foregoing, the authors grant the U.S. Government and others
 *	acting in its behalf permission to use and distribute the
 *	software in accordance with the terms specified in this
 *	license.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TKMACDEBUG
#define _TKMACDEBUG

#ifndef _TKMACINT
#include "tkMacOSXInt.h"
#endif

#ifdef TK_MAC_DEBUG

MODULE_SCOPE char* TkMacOSXCarbonEventToAscii(EventRef eventRef);

#ifdef TK_MACOSXDEBUG_UNUSED
MODULE_SCOPE char* TkMacOSXCarbonEventKindToAscii(EventRef eventRef, char * buf );
MODULE_SCOPE char* TkMacOSXClassicEventToAscii(EventRecord * eventPtr, char * buf );

MODULE_SCOPE void TkMacOSXPrintRect(char * tag, Rect * r );
MODULE_SCOPE void TkMacOSXPrintPoint(char * tag, Point * p );

MODULE_SCOPE void TkMacOSXPrintRegion(char * tag, RgnHandle rgn );
MODULE_SCOPE void TkMacOSXPrintWindowTitle(char * tag, WindowRef window );
MODULE_SCOPE char* TkMacOSXMenuMessageToAscii(int msg, char * s);

MODULE_SCOPE char* TkMacOSXMouseTrackingResultToAscii(MouseTrackingResult r, char * buf );
#endif

MODULE_SCOPE void TkMacOSXDebugFlashRegion(Drawable d, HIShapeRef rgn);

MODULE_SCOPE void* TkMacOSXGetNamedDebugSymbol(const char* module, const char* symbol);

/* Macro to abstract common use of TkMacOSXGetNamedDebugSymbol to initialize named symbols */
#define TkMacOSXInitNamedDebugSymbol(module, ret, symbol, ...) \
    static ret (* symbol)(__VA_ARGS__) = (void*)(-1L); \
    if (symbol == (void*)(-1L)) { \
	symbol = TkMacOSXGetNamedDebugSymbol(STRINGIFY(module), STRINGIFY(_##symbol));\
    }

#endif /* TK_MAC_DEBUG */

#endif
