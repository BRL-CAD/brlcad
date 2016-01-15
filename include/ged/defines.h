/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @addtogroup ged_defines
 *
 * Geometry EDiting Library specific definitions.
 *
 */
/** @{ */
/** @file ged/defines.h */

#ifndef GED_DEFINES_H
#define GED_DEFINES_H

#include "common.h"

__BEGIN_DECLS

#ifndef GED_EXPORT
#  if defined(GED_DLL_EXPORTS) && defined(GED_DLL_IMPORTS)
#    error "Only GED_DLL_EXPORTS or GED_DLL_IMPORTS can be defined, not both."
#  elif defined(GED_DLL_EXPORTS)
#    define GED_EXPORT __declspec(dllexport)
#  elif defined(GED_DLL_IMPORTS)
#    define GED_EXPORT __declspec(dllimport)
#  else
#    define GED_EXPORT
#  endif
#endif

/** all okay return code, not a maskable result */
#define GED_OK    0x0000

/**
 * possible maskable return codes from ged functions.  callers should
 * not rely on the actual values but should instead test via masking.
 */
#define GED_ERROR 0x0001 /**< something went wrong, the action was not performed */
#define GED_HELP  0x0002 /**< invalid specification, result contains usage */
#define GED_MORE  0x0004 /**< incomplete specification, can specify again interactively */
#define GED_QUIET 0x0008 /**< don't set or modify the result string */

#define GED_VMIN -2048.0
#define GED_VMAX 2047.0
#define GED_VRANGE 4095.0
#define INV_GED_V 0.00048828125
#define INV_4096_V 0.000244140625

#define GED_NULL ((struct ged *)0)
#define GED_DISPLAY_LIST_NULL ((struct display_list *)0)
#define GED_DRAWABLE_NULL ((struct ged_drawable *)0)
#define GED_VIEW_NULL ((struct bview *)0)
#define GED_DM_VIEW_NULL ((struct ged_dm_view *)0)

#define GED_RESULT_NULL ((void *)0)

#define GED_FUNC_PTR_NULL ((ged_func_ptr)0)
#define GED_REFRESH_CALLBACK_PTR_NULL ((ged_refresh_callback_ptr)0)
#define GED_CREATE_VLIST_SOLID_CALLBACK_PTR_NULL ((ged_create_vlist_solid_callback_ptr)0)
#define GED_CREATE_VLIST_CALLBACK_PTR_NULL ((ged_create_vlist_callback_ptr)0)
#define GED_FREE_VLIST_CALLBACK_PTR_NULL ((ged_free_vlist_callback_ptr)0)

/**
 * Definition of global parallel-processing semaphores.
 *
 */
#define GED_SEM_WORKER RT_SEM_LAST
#define GED_SEM_STATS GED_SEM_WORKER+1
#define GED_SEM_LIST GED_SEM_STATS+1
#define GED_SEM_LAST GED_SEM_LIST+1

#define GED_INIT(_gedp, _wdbp) { \
	ged_init((_gedp)); \
	(_gedp)->ged_wdbp = (_wdbp); \
    }

#define GED_INITIALIZED(_gedp) ((_gedp)->ged_wdbp != RT_WDB_NULL)
#define GED_LOCAL2BASE(_gedp) ((_gedp)->ged_wdbp->dbip->dbi_local2base)
#define GED_BASE2LOCAL(_gedp) ((_gedp)->ged_wdbp->dbip->dbi_base2local)

/* From include/dm.h */
#define GED_MAX 2047.0
#define GED_MIN -2048.0
#define GED_RANGE 4095.0
#define INV_GED 0.00048828125
#define INV_4096 0.000244140625

/* FIXME: leftovers from dg.h */
#define RT_VDRW_PREFIX          "_VDRW"
#define RT_VDRW_PREFIX_LEN      6
#define RT_VDRW_MAXNAME         31
#define RT_VDRW_DEF_COLOR       0xffff00
__END_DECLS

#endif /* GED_DEFINES_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
