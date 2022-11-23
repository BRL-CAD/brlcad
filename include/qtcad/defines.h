/*                      D E F I N E S . H
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

/** @addtogroup qtcad_defines
 *
 * @brief
 * These are definitions specific to libqtcad
 *
 */
/** @{ */
/** @file qtcad/defines.h */

#ifndef QTCAD_DEFINES_H
#define QTCAD_DEFINES_H

#include "common.h"

#ifndef QTCAD_EXPORT
#  if defined(QTCAD_DLL_EXPORTS) && defined(QTCAD_DLL_IMPORTS)
#    error "Only QTCAD_DLL_EXPORTS or QTCAD_DLL_IMPORTS can be defined, not both."
#  elif defined(QTCAD_DLL_EXPORTS)
#    define QTCAD_EXPORT COMPILER_DLLEXPORT
#  elif defined(QTCAD_DLL_IMPORTS)
#    define QTCAD_EXPORT COMPILER_DLLIMPORT
#  else
#    define QTCAD_EXPORT
#  endif
#endif

/* Need to have a sense of how deep to go before bailing - a cyclic
 * path is a possibility and would be infinite recursion, so we need
 * some limit.  If there is a limit in BRL-CAD go with that, but until
 * it is found use this */
#define CADTREE_RECURSION_LIMIT 10000

#endif  /* QTCAD_DEFINES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
