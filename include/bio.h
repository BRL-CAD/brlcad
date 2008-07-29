/*                           B I O . H
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file bio.h
 *
 * BRL-CAD private system compatibility wrapper header that provides
 * declarations for native and standard input/output system routines.
 *
 * This header does not belong to any BRL-CAD library but is used by
 * all of them.  Consider this header private and subject to change,
 * not to be used by third parties.
 *
 * BRL-CAD libraries and applications should include this library in
 * place of including existing native and standard headers.
 *
 */

#ifndef __BIO_H__
#define __BIO_H__

/* Do not rely on common.h's HAVE_* defines.  Do not include the
 * common.h header.
 */

#include <stdio.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#  define NOMINMAX
#  include <windows.h>
#  include <io.h>

#   undef rad1 /* Win32 radio button 1 */
#   undef rad2 /* Win32 radio button 2 */
#   undef small /* defined as part of the Microsoft Interface Definition Language (MIDL) */
#   undef IN
#   undef OUT

#else
#  include <unistd.h>
#endif

#include <fcntl.h>

/* account for badness in Tcl regex header */
#ifdef regfree
#  undef regfree
#endif

#endif /* __BIO_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
