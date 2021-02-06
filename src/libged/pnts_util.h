/*                    P N T S _ U T I L. H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/pnts_util.h
 *
 * utility functionality for simple Point Set (pnts) primitive operations.
 *
 */
#ifndef LIBGED_PNT_GED_PRIVATE_H
#define LIBGED_PNT_GED_PRIVATE_H

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/color.h"
#include "rt/geom.h"
#include "ged/defines.h"
#include "./ged_private.h"

__BEGIN_DECLS

GED_EXPORT extern const char *_ged_pnt_default_fmt_str(rt_pnt_type type);

GED_EXPORT extern void _ged_pnt_v_set(void *point, rt_pnt_type type, char key, fastf_t val);

GED_EXPORT extern void _ged_pnt_c_set(void *point, rt_pnt_type type, char key, fastf_t val);

GED_EXPORT extern void _ged_pnt_s_set(void *point, rt_pnt_type type, char key, fastf_t val);

GED_EXPORT extern void _ged_pnt_n_set(void *point, rt_pnt_type type, char key, fastf_t val);

GED_EXPORT extern rt_pnt_type _ged_pnts_fmt_type(const char *fc);

GED_EXPORT extern rt_pnt_type _ged_pnts_fmt_guess(int numcnt);

GED_EXPORT extern int _ged_pnts_fmt_match(rt_pnt_type t, int numcnt);

GED_EXPORT extern void _ged_pnts_init_head_pnt(struct rt_pnts_internal *pnts);

GED_EXPORT extern void * _ged_pnts_new_pnt(rt_pnt_type t);

GED_EXPORT extern void _ged_pnts_add(struct rt_pnts_internal *pnts, void *point);

GED_EXPORT extern void * _ged_pnts_dup(void *point, rt_pnt_type t);

__END_DECLS

#endif //LIBGED_PNT_GED_PRIVATE_H

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
