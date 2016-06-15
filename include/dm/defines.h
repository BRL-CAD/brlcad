/*                     D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file defines.h
 *
 */
#ifndef DM_EXPORT
#  if defined(DM_DLL_EXPORTS) && defined(DM_DLL_IMPORTS)
#    error "Only DM_DLL_EXPORTS or DM_DLL_IMPORTS can be defined, not both."
#  elif defined(DM_DLL_EXPORTS)
#    define DM_EXPORT __declspec(dllexport)
#  elif defined(DM_DLL_IMPORTS)
#    define DM_EXPORT __declspec(dllimport)
#  else
#    define DM_EXPORT
#  endif
#endif

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
