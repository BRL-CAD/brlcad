/* $Id */

/* vi:set sw=4: */

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

#include "togl.h"

#if defined(TOGL_OSMESA) || defined(TOGL_WGL)
/* nothing extra to include */
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#else
#  if !defined(TOGL_X11) || !defined(GLX_VERSION_1_4)
#    include <dlfcn.h>
#  endif
#endif

Togl_FuncPtr
Togl_GetProcAddr(const char *funcname)
{
#if defined(TOGL_OSMESA)
    return (Togl_FuncPtr) OSMesaGetProcAddress(funcname);
#elif defined(TOGL_WGL)
    return (Togl_FuncPtr) wglGetProcAddress(funcname);
#elif defined(__APPLE__)
    char    buf[256];

    snprintf(buf, sizeof buf - 1, "_%s", funcname);
    buf[sizeof buf - 1] = '\0';
    if (NSIsSymbolNameDefined(buf)) {
        NSSymbol nssym;

        nssym = NSLookupAndBindSymbol(buf);
        if (nssym)
            return (Togl_FuncPtr) NSAddressOfSymbol(nssym);
    }
    return NULL;
#else
#  if defined(TOGL_X11) && defined(GLX_VERSION_1_4)
    /* Strictly speaking, we can only call glXGetProcAddress if glXQueryVersion 
     * says we're using version 1.4 or later. */
    return (Togl_FuncPtr) glXGetProcAddress(funcname);
#  else
    /* Linux, IRIX, OSF/1, ? */
    static void *dlHandle = NULL;

    if (dlHandle == NULL)
        dlHandle = dlopen(NULL, RTLD_LAZY);
    /* Strictly speaking, the following cast of a data pointer to a function
     * pointer is not legal in ISO C, but we don't have any choice. */
    return (Togl_FuncPtr) dlsym(dlHandle, funcname);
#  endif
#endif
}
