/*                     M G E D _ W D B . H
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/mged_wdb.h
 *
 * MGED shared rtg_headwdb resource.
 *
 * Normally wdb_obj.c doesn't use shared global resources with MGED.  However,
 * in this case we are using a variable that was originally global to librt
 * itself, and was thus common to both.
 *
 * Until it's clear what the implications are of having a separate headwdb for
 * the wdb_obj.c code, we'll share this one particular variable.
 */

#ifndef MGED_MGED_WDB_H
#define MGED_MGED_WDB_H

#include "common.h"

#include "raytrace.h"

extern struct rt_wdb rtg_headwdb;

#endif  /* MGED_MGED_WDB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
