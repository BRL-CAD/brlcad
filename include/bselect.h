/*                       B S E L E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file bselect.h
 *
 * BRL-CAD private system compatibility wrapper header that provides
 * declarations for native and standard system select() routines.
 *
 * This header is commonly used in leu of including the following:
 * sys/select.h
 *
 * This header does not belong to any BRL-CAD library but may used by
 * all of them.  Consider this header PRIVATE and subject to change,
 * NOT TO BE USED BY THIRD PARTIES.
 *
 */

#ifndef __BSELECT_H__
#define __BSELECT_H__

#include "common.h"

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

/* compatibility for pedantic bug/limitation in gcc 4.6.2, need to
 * mark macros as extensions else they may emit "ISO C forbids
 * braced-groups within expressions" warnings.
 */

#if defined(FD_SET) && defined(__FD_SET)
#  undef FD_SET
#  define FD_SET(x, y) __extension__ __FD_SET((x), (y))
#endif

#if defined(FD_CLR) && defined(__FD_CLR)
#  undef FD_CLR
#  define FD_CLR(x, y) __extension__ __FD_CLR((x), (y))
#endif

#if defined(FD_ISSET) && defined(__FD_ISSET)
#  undef FD_ISSET
#  define FD_ISSET(x, y) __extension__ __FD_ISSET((x), (y))
#endif

#endif /* __BSELECT_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
