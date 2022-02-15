/*                        D Y L I B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#include <string.h>

#ifdef HAVE_DLFCN_H
# include <dlfcn.h>
/* this should not be needed. Maybe an explicit cmake test? */
# ifndef HAVE_DLOPEN
#  define HAVE_DLOPEN 1
# endif
#endif
#include "bio.h"

#include "bu/dylib.h"
#include "bu/malloc.h"


static size_t n_handles = 0;
static size_t max_handles = 0;
static void **handles = NULL;


void *
bu_dlopen(const char *path, int mode)
{
#ifdef HAVE_DLOPEN
    return dlopen(path, mode);
#elif defined(_WIN32)
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
#elif defined(_WIN32)
    return GetProcAddress(handle, symbol);
#else
    bu_log("dlsym not supported\n");
    return NULL;
#endif
}

int
bu_dlclose(void *handle)
{
    if (++n_handles >= max_handles) {

	/* start with just a few but then double quickly */
	if (max_handles < 32) {
	    max_handles = 32;
	    handles = (void **)bu_calloc(max_handles, sizeof(void *), "alloc handles");
	} else {
	    max_handles *= 2;
	    handles = (void **)bu_realloc(handles, sizeof(void *) * max_handles, "realloc handles");
	}

    }
    handles[n_handles-1] = handle;

    return 0;
}

const char *
bu_dlerror(void)
{
#ifdef HAVE_DLOPEN
    return dlerror();
#elif defined(_WIN32)
    static char buf[BUFSIZ];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), LANG_NEUTRAL, buf, BUFSIZ, NULL);
    return (const char *)buf;
#else
    bu_log("dlerror not supported\n");
    return NULL;
#endif
}


int
bu_dlunload()
{
    int ret = 0;
    while (n_handles) {
#ifdef HAVE_DLOPEN
	ret += dlclose(handles[n_handles-1]);
#elif defined(_WIN32)
	ret += !FreeLibrary(handles[n_handles-1]);
#endif
	handles[n_handles-1] = NULL;
	n_handles--;
    }
    bu_free(handles, "free handles");
    return ret;
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
