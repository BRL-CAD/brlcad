/*                         E D G R I P . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2026 United States Government as represented by
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
/** @file primitives/edgrip.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"

C_DECL const char *
rt_edit_grp_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	struct rt_edit *s,
	const struct bn_tol *tol)
{
    struct rt_db_internal *ip = &s->es_int;
    const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, keystr, mat, ip, tol);
    if (!strp) {
	static const char *c_str = "C";
	strp = OBJ[ip->idb_type].ft_keypoint(pt, c_str, mat, ip, tol);
    }
    return strp;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

C_DECL void
rt_edit_grp_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_grip_internal *grip = (struct rt_grip_internal *)ip->idb_ptr;
    RT_GRIP_CK_MAGIC(grip);

    bu_vls_printf(p, "Center: %.9f %.9f %.9f\n", V3BASE2LOCAL(grip->center));
    bu_vls_printf(p, "Normal: %.9f %.9f %.9f\n", V3ARGS(grip->normal));
    bu_vls_printf(p, "Magnitude: %.9f\n", grip->mag*base2local);
}

C_DECL int
rt_edit_grp_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    struct rt_grip_internal *grip = (struct rt_grip_internal *)ip->idb_ptr;
    RT_GRIP_CK_MAGIC(grip);

    if (!fc)
	return BRLCAD_ERROR;

    struct rt_grip_internal staged = *grip;
    char *buffer = bu_strdup(fc);
    char *cursor = buffer;
    int result = BRLCAD_ERROR;
    if (edit_param_read_vector(staged.center, &cursor, "Center", local2base) != BRLCAD_OK ||
	edit_param_read_vector(staged.normal, &cursor, "Normal", 1.0) != BRLCAD_OK ||
	edit_param_read_scalar(&staged.mag, &cursor, "Magnitude", local2base) != BRLCAD_OK ||
	edit_param_next_line(&cursor))
	goto cleanup;

    *grip = staged;
    result = BRLCAD_OK;

cleanup:
    bu_free(buffer, "GRIP parameter text");
    return result;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
