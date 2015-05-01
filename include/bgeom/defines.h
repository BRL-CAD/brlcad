/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file defines.h */
/** @addtogroup libbgeom */
/** @{ */
/**
 *  @brief Common definitions for the headers used in bgeom.h (i.e. the headers found in include/bgeom)
 */

#ifndef BGEOM_DEFINES_H
#define BGEOM_DEFINES_H

#include "common.h"

#ifndef BGEOM_EXPORT
#  if defined(BGEOM_DLL_EXPORTS) && defined(BGEOM_DLL_IMPORTS)
#    error "Only BGEOM_DLL_EXPORTS or BGEOM_DLL_IMPORTS can be defined, not both."
#  elif defined(BGEOM_DLL_EXPORTS)
#    define BGEOM_EXPORT __declspec(dllexport)
#  elif defined(BGEOM_DLL_IMPORTS)
#    define BGEOM_EXPORT __declspec(dllimport)
#  else
#    define BGEOM_EXPORT
#  endif
#endif

#endif  /* BGEOM_DEFINES_H */
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
