/*                        D L F C N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#ifdef HAVE_DLFCN_H
# include <dlfcn.h>
/* this should not be needed. Maybe an explicit cmake test? */
# ifndef HAVE_DLOPEN
#  define HAVE_DLOPEN 1
# endif
#endif
#include "bio.h"

#include "bu.h"

void *
bu_dlopen(const char *path, int mode)
{
#ifdef HAVE_DLOPEN
    return dlopen(path, mode);
#elif defined(WIN32)
    return LoadLibrary(path);
#else
    bu_log("dlopen not supported\n");
    return NULL;
#endif
}

void *
bu_dlsym(void *handle, const char *symbol)
{
#ifdef HAVE_DLOPEN
    return dlsym(handle, symbol);
#elif defined(WIN32)
    return GetProcAddress(handle, symbol);
#else
    bu_log("dlsym not supported\n");
    return NULL;
#endif
}

int
bu_dlclose(void *handle)
{
#ifdef HAVE_DLOPEN
    return dlclose(handle);
#elif defined(WIN32)
    return 0;
#else
    bu_log("dlclose not supported\n");
    return 0;
#endif
}

const char *
bu_dlerror(void)
{
#ifdef HAVE_DLOPEN
    return dlerror();
#elif defined(WIN32)
    static char buf[BUFSIZ];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), LANG_NEUTRAL, buf, BUFSIZ, NULL);
    return (const char *)buf;
#else
    bu_log("dlerror not supported\n");
    return NULL;
#endif
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
