/*                           G C V . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file gcv.h
 *
 * API of the LIBGCV geometry conversion library.
 *
 */

#ifndef GCV_H
#define GCV_H

#include "common.h"

__BEGIN_DECLS

/* TODO: needs to move to subdir */
#ifndef GCV_EXPORT
#  if defined(GCV_DLL_EXPORTS) && defined(GCV_DLL_IMPORTS)
#    error "Only GCV_DLL_EXPORTS or GCV_DLL_IMPORTS can be defined, not both."
#  elif defined(GCV_DLL_EXPORTS)
#    define GCV_EXPORT __declspec(dllexport)
#  elif defined(GCV_DLL_IMPORTS)
#    define GCV_EXPORT __declspec(dllimport)
#  else
#    define GCV_EXPORT
#  endif
#endif

/* TODO: needs to move to subdir */
#include "gcv_util.h"

/* TODO: main API goes here */
#include "gcv_api.h"


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
