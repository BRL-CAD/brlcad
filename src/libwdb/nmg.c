/*                           N M G . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2014 United States Government as represented by
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

/** @file libwdb/nmg.c
 *
 * libwdb support for writing an NMG to disk.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"


int
mk_nmg(struct rt_wdb *filep, const char *name, struct model *m)
{
    NMG_CK_MODEL(m);

    /* FIXME: wdb_export is documented as always free'ing the entity
     * passed to it.  that means this routine needs to make a copy of
     * the geometry.
     */

    return wdb_export(filep, name, (genptr_t)m, ID_NMG, mk_conv2mm);
}


int
mk_bot_from_nmg(struct rt_wdb *ofp, const char *name, struct shell *s)
{
    struct rt_bot_internal *botp;

    botp = nmg_bot(s, &ofp->wdb_tol);

    /* FIXME: wdb_export is documented as always free'ing the entity
     * passed to it.  that means this routine needs to make a copy of
     * the geometry.
     */

    return wdb_export(ofp, name, (genptr_t)botp, ID_BOT, mk_conv2mm);
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
