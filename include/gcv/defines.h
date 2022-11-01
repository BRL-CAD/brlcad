/*                       D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @file gcv/defines.h
 *
 * Brief description
 *
 */

#ifndef GCV_DEFINES_H
#define GCV_DEFINES_H

#include "common.h"

__BEGIN_DECLS

#ifndef GCV_EXPORT
#  if defined(GCV_DLL_EXPORTS) && defined(GCV_DLL_IMPORTS)
#    error "Only GCV_DLL_EXPORTS or GCV_DLL_IMPORTS can be defined, not both."
#  elif defined(GCV_DLL_EXPORTS)
#    define GCV_EXPORT COMPILER_DLLEXPORT
#  elif defined(GCV_DLL_IMPORTS)
#    define GCV_EXPORT COMPILER_DLLIMPORT
#  else
#    define GCV_EXPORT
#  endif
#endif

__END_DECLS

#endif /* GCV_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
