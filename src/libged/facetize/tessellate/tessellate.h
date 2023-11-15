/*                  T E S S E L L A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file tessellate.h
 *
 * Private header for ged_tessellate executable.
 *
 */

#ifndef TESSELLATE_EXEC_H
#define TESSELLATE_EXEC_H

#include "common.h"

#ifdef __cplusplus
#include "manifold/manifold.h"
#endif

#include "vmath.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "raytrace.h"

__BEGIN_DECLS

extern int
half_to_bot(struct rt_bot_internal **obot, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

extern int
bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

extern int
plate_eval(struct rt_bot_internal **obot, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

extern struct rt_bot_internal *
_tess_facetize_decimate(struct rt_bot_internal *bot, fastf_t feature_size);

extern int
_tess_facetize_write_bot(struct ged *gedp, struct rt_bot_internal *bot, const char *name);

extern int
_ged_continuation_obj(struct ged *gedp, const char *objname, const char *newname);

extern int
_ged_spsr_obj(struct ged *gedp, const char *objname, const char *newname);

__END_DECLS

#endif // TESSELLATE_EXEC_H

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
