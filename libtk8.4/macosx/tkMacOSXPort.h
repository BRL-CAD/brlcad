/*
 * tkMacOSXPort.h --
 *
 *	This file is included by all of the Tk C files.  It contains
 *	information that may be configuration-dependent, such as
 *	#includes for system include files and a few other things.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TKMACPORT
#define _TKMACPORT

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.  This macro may be used in some of the include
 * files below, which is why it is defined here.
 */

#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef HAVE_LIMITS_H
#   include <limits.h>
#else
#   include "../compat/limits.h"
#endif
#include <math.h>
#include <pwd.h>
#ifdef NO_STDLIB_H
#   include "../compat/stdlib.h"
#else
#   include <stdlib.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif
#include <sys/stat.h>
#ifndef _TCL
#   include <tcl.h>
#endif
#if TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#else
#   if HAVE_SYS_TIME_H
#       include <sys/time.h>
#   else
#       include <time.h>
#   endif
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#else
#   include "../compat/unistd.h"
#endif
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xfuncproto.h>
#include <X11/Xutil.h>
#include "tkIntXlibDecls.h"

/*
 * The following macro defines the type of the mask arguments to
 * select:
 */

#ifndef NO_FD_SET
#   define SELECT_MASK fd_set
#else
#   ifndef _AIX
	typedef long fd_mask;
#   endif
#   if defined(_IBMR2)
#	define SELECT_MASK void
#   else
#	define SELECT_MASK int
#   endif
#endif

/*
 * The following macro defines the number of fd_masks in an fd_set:
 */

#ifndef FD_SETSIZE
#   ifdef OPEN_MAX
#	define FD_SETSIZE OPEN_MAX
#   else
#	define FD_SETSIZE 256
#   endif
#endif
#if !defined(howmany)
#   define howmany(x, y) (((x)+((y)-1))/(y))
#endif
#ifndef NFDBITS
#   define NFDBITS NBBY*sizeof(fd_mask)
#endif
#define MASK_SIZE howmany(FD_SETSIZE, NFDBITS)

/*
 * Not all systems declare the errno variable in errno.h. so this
 * file does it explicitly.
 */

extern int errno;

/*
 * Define "NBBY" (number of bits per byte) if it's not already defined.
 */

#ifndef NBBY
#   define NBBY 8
#endif

/*
 * Declarations for various library procedures that may not be declared
 * in any other header file.
 */

#ifndef panic	/* In a stubs-aware setting, this could confuse the #define */
extern void 		panic  _ANSI_ARGS_(TCL_VARARGS(char *, string));
#endif
#ifndef strcasecmp
extern int		strcasecmp _ANSI_ARGS_((CONST char *s1,
			    CONST char *s2));
#endif
#ifndef strncasecmp			    
extern int		strncasecmp _ANSI_ARGS_((CONST char *s1,
			    CONST char *s2, size_t n));
#endif
/*
 * Defines for X functions that are used by Tk but are treated as
 * no-op functions on the Macintosh.
 */

#define XFlush(display)
#define XFree(data) {if ((data) != NULL) ckfree((char *) (data));}
#define XGrabServer(display)
#define XNoOp(display) {display->request++;}
#define XUngrabServer(display)
#define XSynchronize(display, bool) {display->request++;}
#define XVisualIDFromVisual(visual) (visual->visualid)

int XSync(Display *display, Bool discard);

/*
 * The following functions are not used on the Mac, so we stub them out.
 */

#define TkFreeWindowId(dispPtr,w)
#define TkInitXId(dispPtr)
#define TkpButtonSetDefaults(specPtr) {}
#define TkpCmapStressed(tkwin,colormap) (0)
#define TkpFreeColor(tkColPtr)
#define TkSetPixmapColormap(p,c) {}
#define TkpSync(display)

/*
 * The following macro returns the pixel value that corresponds to the
 * RGB values in the given XColor structure.
 */

#define PIXEL_MAGIC ((unsigned char) 0x69)
#define TkpGetPixel(p) ((((((PIXEL_MAGIC << 8) \
	| (((p)->red >> 8) & 0xff)) << 8) \
	| (((p)->green >> 8) & 0xff)) << 8) \
	| (((p)->blue >> 8) & 0xff))

/*
 * This macro stores a representation of the window handle in a string.
 * This should perhaps use the real size of an XID.
 */

#define TkpPrintWindowId(buf,w) \
	sprintf((buf), "0x%x", (unsigned int) (w))
	    
/*
 * TkpScanWindowId is just an alias for Tcl_GetInt on Unix.
 */

#define TkpScanWindowId(i,s,wp) \
	Tcl_GetInt((i),(s),(int *) (wp))

/*
 * Magic pixel values for dynamic (or active) colors.
 */

#define HIGHLIGHT_PIXEL			31
#define HIGHLIGHT_TEXT_PIXEL		33
#define CONTROL_TEXT_PIXEL		35
#define CONTROL_BODY_PIXEL		37
#define CONTROL_FRAME_PIXEL		39
#define WINDOW_BODY_PIXEL		41
#define MENU_ACTIVE_PIXEL		43
#define MENU_ACTIVE_TEXT_PIXEL		45
#define MENU_BACKGROUND_PIXEL		47
#define MENU_DISABLED_PIXEL		49
#define MENU_TEXT_PIXEL			51
#define APPEARANCE_PIXEL		52

#endif /* _TKMACPORT */
