/*                      D E F I N E S . H
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

/** @addtogroup qged_defines
 *
 * @brief
 * These are definitions specific to libqged
 *
 */
/** @{ */
/** @file qged/defines.h */

#ifndef QGED_DEFINES_H
#define QGED_DEFINES_H

#include "common.h"

#ifndef QGED_EXPORT
#  if defined(QGED_DLL_EXPORTS) && defined(QGED_DLL_IMPORTS)
#    error "Only QGED_DLL_EXPORTS or QGED_DLL_IMPORTS can be defined, not both."
#  elif defined(QGED_DLL_EXPORTS)
#    define QGED_EXPORT COMPILER_DLLEXPORT
#  elif defined(QGED_DLL_IMPORTS)
#    define QGED_EXPORT COMPILER_DLLIMPORT
#  else
#    define QGED_EXPORT
#  endif
#endif

#endif  /* QGED_DEFINES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
