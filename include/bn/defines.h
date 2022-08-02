/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/** @addtogroup bn_defines
 *
 *  Common definitions for the headers used in libbn (i.e. the headers found in include/bn)
 */
/** @{ */
/** @file bn/defines.h */

#ifndef BN_DEFINES_H
#define BN_DEFINES_H

#include "common.h"

#ifndef BN_EXPORT
#  if defined(BN_DLL_EXPORTS) && defined(BN_DLL_IMPORTS)
#    error "Only BN_DLL_EXPORTS or BN_DLL_IMPORTS can be defined, not both."
#  elif defined(BN_DLL_EXPORTS)
#    define BN_EXPORT COMPILER_DLLEXPORT
#  elif defined(BN_DLL_IMPORTS)
#    define BN_EXPORT COMPILER_DLLIMPORT
#  else
#    define BN_EXPORT
#  endif
#endif

#define BN_AZIMUTH   0
#define BN_ELEVATION 1
#define BN_TWIST     2

#endif  /* BN_DEFINES_H */

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
