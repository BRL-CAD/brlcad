/*                        D Y L I B. H
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

#ifndef BU_DYLIB_H
#define BU_DYLIB_H

#include "common.h"

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>   /* for RTLD_* */
#endif

#include "bu/defines.h"


__BEGIN_DECLS


/**
 * @addtogroup bu_dylib
 *
 * @brief Dynamic Library functionality
 */
/** @{ */

#ifdef HAVE_DLOPEN
# define BU_RTLD_LAZY RTLD_LAZY
# define BU_RTLD_NOW RTLD_NOW
# define BU_RTLD_GLOBAL RTLD_GLOBAL
# define BU_RTLD_LOCAL RTLD_LOCAL
#else
# define BU_RTLD_LAZY 1
# define BU_RTLD_NOW 2
# define BU_RTLD_GLOBAL 0x100
# define BU_RTLD_LOCAL 0
#endif

BU_EXPORT extern void *bu_dlopen(const char *path, int mode);
BU_EXPORT extern void *bu_dlsym(void *path, const char *symbol);
BU_EXPORT extern int bu_dlclose(void *handle);
BU_EXPORT extern const char *bu_dlerror(void);

/** @} */

__END_DECLS

#endif  /* BU_DYLIB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
