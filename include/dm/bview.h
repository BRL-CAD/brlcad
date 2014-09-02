/*                           D L . H
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
/** @{ */
/** @file dl.h
 *
 * Types, functions and definitions related to display lists.  This
 * header is intended to be independent of any one BRL-CAD library
 * and is specifically intended to allow the easy definition of common
 * display list types between otherwise independent libraries (libdm
 * and libged, for example).*
 */

#ifndef DL_H
#define DL_H

#include "common.h"
#include "bu/list.h"
#include "bu/vls.h"

struct display_list {
    struct bu_list      l;
    void               *dl_dp;                 /* Normally this will be a struct directory pointer */
    struct bu_vls       dl_path;
    struct bu_list      dl_headSolid;          /**< @brief  head of solid list for this object */
    int                 dl_wflag;
};

#endif /* DL_H */

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
