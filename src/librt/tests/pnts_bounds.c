/*                       P N T S _ B O U N D S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/geom.h"

int
main(int argc, char **argv)
{
    struct pnt_scale head = {0};
    struct pnt_scale large = {0};
    struct pnt_scale small = {0};
    struct rt_pnts_internal pnts = {0};
    struct rt_db_internal intern;
    struct soltab st = {0};
    struct rt_i *rtip;
    struct resource *resp = &rt_uniresource;
    struct application ap;
    struct xray ray = {0};
    struct seg seghead;
    struct seg *segp;
    point_t min, max, expected_min, expected_max;
    vect_t invdir;
    int failed = 0;

    (void)argc;
    bu_setprogname(argv[0]);

    BU_LIST_INIT(&head.l);
    VSET(large.v, 0, 0, 0);
    large.s = 3.0;
    BU_LIST_APPEND(&head.l, &large.l);
    VSET(small.v, 10, 0, 0);
    small.s = 0.5;
    BU_LIST_APPEND(&head.l, &small.l);

    pnts.magic = RT_PNTS_INTERNAL_MAGIC;
    pnts.scale = 1.0;
    pnts.type = RT_PNT_TYPE_SCA;
    pnts.count = 2;
    pnts.point = &head;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_PNTS;
    intern.idb_meth = &OBJ[ID_PNTS];
    intern.idb_ptr = &pnts;

    if (OBJ[ID_PNTS].ft_bbox(&intern, &min, &max, NULL)) {
        bu_log("PNTS bbox failed\n");
        return 1;
    }
    VSET(expected_min, -3, -3, -3);
    VSET(expected_max, 10.5, 3, 3);
    if (!VNEAR_EQUAL(min, expected_min, SMALL_FASTF) ||
        !VNEAR_EQUAL(max, expected_max, SMALL_FASTF)) {
        bu_log("PNTS bbox (%g %g %g) .. (%g %g %g) ignored a per-point scale\n",
            V3ARGS(min), V3ARGS(max));
        failed = 1;
    }
    if (!OBJ[ID_PNTS].ft_use_rpp) {
        bu_log("PNTS solid-box pruning is disabled\n");
        failed = 1;
    }
    pnts.count = 0;
    if (!OBJ[ID_PNTS].ft_bbox(&intern, &min, &max, NULL)) {
        bu_log("empty PNTS unexpectedly returned a usable box\n");
        failed = 1;
    }
    pnts.count = 2;

    rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip)
        return 1;
    rt_init_resource(resp, 0, rtip);
    st.l.magic = RT_SOLTAB_MAGIC;
    st.l2.magic = RT_SOLTAB2_MAGIC;
    st.st_rtip = rtip;
    st.st_meth = &OBJ[ID_PNTS];
    st.st_id = ID_PNTS;
    if (OBJ[ID_PNTS].ft_prep(&st, &intern, rtip)) {
        bu_log("PNTS prep failed\n");
        rt_i_destroy(rtip);
        return 1;
    }

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;
    ray.magic = RT_RAY_MAGIC;
    VSET(ray.r_pt, -5, 2.5, 0);
    VSET(ray.r_dir, 1, 0, 0);
    VSET(invdir, 1, INFINITY, INFINITY);
    if (!rt_in_rpp(&ray, invdir, st.st_min, st.st_max)) {
        bu_log("PNTS bounds rejected a ray that hits the larger point\n");
        failed = 1;
    } else {
        BU_LIST_INIT(&seghead.l);
        if (OBJ[ID_PNTS].ft_shot(&st, &ray, &ap, &seghead) <= 0 ||
            BU_LIST_IS_EMPTY(&seghead.l)) {
            bu_log("PNTS shot missed the larger point\n");
            failed = 1;
        }
        while (BU_LIST_WHILE(segp, seg, &seghead.l)) {
            BU_LIST_DEQUEUE(&segp->l);
            RT_FREE_SEG(segp, resp);
        }
    }

    OBJ[ID_PNTS].ft_free(&st);
    rt_i_destroy(rtip);
    return failed ? 1 : 0;
}
