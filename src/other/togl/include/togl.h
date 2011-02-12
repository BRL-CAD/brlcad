/* $Id$ */

/* vi:set sw=4: */

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */


#ifndef TOGL_H
#  define TOGL_H

#  include "togl_config.h"

#  ifdef TOGL_WGL
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    undef WIN32_LEAN_AND_MEAN
#    if defined(_MSC_VER)
#      	define DllEntryPoint DllMain
#    endif
#  endif

#  if defined(TOGL_AGL) || defined(TOGL_NSOPENGL)
#    ifndef MAC_OSX_TCL
#      define MAC_OSX_TCL 1
#    endif
#    ifndef MAC_OSX_TK
#      define MAC_OSX_TK 1
#    endif
#  endif

#  ifdef USE_TOGL_STUBS
#    ifndef USE_TCL_STUBS
#      define USE_TCL_STUBS
#    endif
#    ifndef USE_TK_STUBS
#      define USE_TK_STUBS
#    endif
#  endif

#  include <tcl.h>
#  include <tk.h>
#  include "GL/glew.h"
#  if defined(TOGL_AGL)
#    include <OpenGL/gl.h>
#  elif defined(TOGL_NSOPENGL)
#    include <OpenGL/OpenGL.h>
#  else
#    include <GL/gl.h>
#  endif

#  ifdef BUILD_togl
#    undef TCL_STORAGE_CLASS
#    define TCL_STORAGE_CLASS DLLEXPORT
#  endif

#  ifndef CONST84
#    define CONST84
#  endif

#  ifndef NULL
#    define NULL 0
#  endif

#  ifndef EXTERN
#    define EXTERN extern
#  endif

#  ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#  endif

#  define TOGL_VERSION "2.1"
#  define TOGL_MAJOR_VERSION 2
#  define TOGL_MINOR_VERSION 1

/* 
 * "Standard" fonts which can be specified to Togl_LoadBitmapFont()
 * Deprecated.  Use the Tk font name or description instead.
 */
#  define TOGL_BITMAP_8_BY_13		"8x13"
#  define TOGL_BITMAP_9_BY_15		"9x15"
#  define TOGL_BITMAP_TIMES_ROMAN_10 	"Times 10"
#  define TOGL_BITMAP_TIMES_ROMAN_24 	"Times 24"
#  define TOGL_BITMAP_HELVETICA_10 	"Helvetica 10"
#  define TOGL_BITMAP_HELVETICA_12 	"Helvetica 12"
#  define TOGL_BITMAP_HELVETICA_18 	"Helvetica 18"

/* 
 * Normal and overlay plane constants
 */
#  define TOGL_NORMAL	1
#  define TOGL_OVERLAY	2

/* 
 * Stereo techniques:
 *      Only the native method uses OpenGL quad-buffered stereo.
 *      All need the eye offset and eye distance set properly.
 */
/* These versions need one eye drawn */
#  define TOGL_STEREO_NONE		0
#  define TOGL_STEREO_LEFT_EYE		1       /* just the left eye */
#  define TOGL_STEREO_RIGHT_EYE		2       /* just the right eye */
#  define TOGL_STEREO_ONE_EYE_MAX	127
/* These versions need both eyes drawn */
#  define TOGL_STEREO_NATIVE		128
#  define TOGL_STEREO_SGIOLDSTYLE	129     /* interlaced, SGI API */
#  define TOGL_STEREO_ANAGLYPH		130
#  define TOGL_STEREO_CROSS_EYE		131
#  define TOGL_STEREO_WALL_EYE		132
#  define TOGL_STEREO_DTI		133     /* dti3d.com */
#  define TOGL_STEREO_ROW_INTERLEAVED	134     /* www.vrex.com/developer/interleave.htm */

struct Togl;
typedef struct Togl Togl;
typedef void (*Togl_FuncPtr) ();

const char *Togl_InitStubs _ANSI_ARGS_((Tcl_Interp *interp, const char *version,
                int exact));

#  ifndef USE_TOGL_STUBS
#    define Togl_InitStubs(interp, version, exact) \
	Tcl_PkgRequire(interp, "Togl", version, exact)
#  endif

/* 
 * Platform independent exported functions
 */

#  include "toglDecls.h"

#  ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#  endif

#endif
