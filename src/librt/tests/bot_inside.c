/*                     B O T _ I N S I D E . C
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

#include <math.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

/* ARS prep also uses BoT raytracing, but its RPP bounds can refresh r_min.
 * Compare converted ARS and native BoT for direct and scene shots. */
#define RING_SIDES 8
#define RING_POINTS (RING_SIDES + 1)

static int
check_shot(const struct rt_functab *ftp, struct soltab *stp, struct application *ap, struct xray *ray, const char *label)
{
    struct seg seghead;
    struct seg *segp;
    struct resource *resp = ap->a_resource;
    int nhits;
    int failed = 0;

    BU_LIST_INIT(&seghead.l);
    nhits = ftp->ft_shot(stp, ray, ap, &seghead);
    if (nhits <= 0 || BU_LIST_IS_EMPTY(&seghead.l)) {
        bu_log("%s: inside-origin ray missed\n", label);
        failed = 1;
    } else {
        segp = BU_LIST_FIRST(seg, &seghead.l);
        if (!NEAR_EQUAL(segp->seg_in.hit_dist, -9.0, SMALL_FASTF) ||
            !NEAR_EQUAL(segp->seg_out.hit_dist, 7.0, SMALL_FASTF)) {
            bu_log("%s: interval %g..%g, expected -9..7\n", label,
                segp->seg_in.hit_dist, segp->seg_out.hit_dist);
            failed = 1;
        }
    }

    while (BU_LIST_WHILE(segp, seg, &seghead.l)) {
        BU_LIST_DEQUEUE(&segp->l);
        RT_FREE_SEG(segp, resp);
    }

    return failed;
}

struct scene_result {
    int subject_hits;
    fastf_t out_dist;
};

static int
scene_hit(struct application *ap, struct partition *part_head,
    struct seg *UNUSED(finished_segs))
{
    struct scene_result *result = (struct scene_result *)ap->a_uptr;
    struct partition *part;

    for (part = part_head->pt_forw; part != part_head; part = part->pt_forw) {
        if (strstr(part->pt_regionp->reg_name, "subject.r")) {
            result->subject_hits++;
            result->out_dist = part->pt_outhit->hit_dist;
        }
    }
    return result->subject_hits > 0;
}

static int
scene_miss(struct application *UNUSED(ap))
{
    return 0;
}

/* The support sphere makes the cut tree start the ray's first cell
 * inside the subject, while the subject itself spans the entire cell. */
static int
check_scene(int subject_id, const fastf_t *const curve_data[3],
    fastf_t *vertices, int *faces, const char *file)
{
    struct db_i *dbip = NULL;
    struct rt_wdb *wdbp = NULL;
    struct rt_i *rtip = NULL;
    struct resource resp = RT_RESOURCE_INIT_ZERO;
    struct wmember members;
    struct application ap;
    struct scene_result result = {0};
    point_t support_center;
    int failed = 0;

    if (file) {
        wdbp = wdb_fopen(file);
        if (wdbp)
            dbip = wdbp->dbip;
    } else {
        dbip = db_create_inmem();
        if (dbip)
            wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    }
    if (!wdbp) {
        if (dbip)
            db_close(dbip);
        return 1;
    }

    if (subject_id == ID_ARS) {
        fastf_t **curves = (fastf_t **)bu_malloc(3 * sizeof(fastf_t *), "ARS curves");
        size_t i;
        for (i = 0; i < 3; i++) {
            curves[i] = (fastf_t *)bu_malloc(RING_POINTS * ELEMENTS_PER_POINT *
                sizeof(fastf_t), "ARS curve");
            memcpy(curves[i], curve_data[i],
                RING_POINTS * ELEMENTS_PER_POINT * sizeof(fastf_t));
        }
        failed += mk_ars(wdbp, "subject.s", 3, RING_POINTS, curves) != 0;
    } else {
        failed += mk_bot(wdbp, "subject.s", RT_BOT_SOLID, RT_BOT_CCW, 0,
            RING_SIDES + 2, RING_SIDES * 2, vertices, faces,
            NULL, NULL) != 0;
    }
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("subject.s", &members.l, NULL, WMOP_UNION))
        failed++;
    failed += mk_lcomb(wdbp, "subject.r", &members, 1, NULL, NULL, NULL, 0) != 0;

    VSET(support_center, 1.5, 0, 4);
    failed += mk_sph(wdbp, "support.s", support_center, 1.0) != 0;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("support.s", &members.l, NULL, WMOP_UNION))
        failed++;
    failed += mk_lcomb(wdbp, "support.r", &members, 1, NULL, NULL, NULL, 0) != 0;
    if (failed)
        goto done;

    rtip = rt_new_rti(dbip);
    if (!rtip || rt_gettree(rtip, "subject.r") ||
        rt_gettree(rtip, "support.r")) {
        failed = 1;
        goto done;
    }
    rt_prep(rtip);
    rt_init_resource(&resp, 0, rtip);

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = &resp;
    ap.a_hit = scene_hit;
    ap.a_miss = scene_miss;
    ap.a_uptr = &result;
    VSET(ap.a_ray.r_pt, 3, 0, 4);
    VSET(ap.a_ray.r_dir, 1, 0, 0);
    rt_shootray(&ap);
    if (result.subject_hits != 1 ||
        !NEAR_EQUAL(result.out_dist, 5.0, rtip->rti_tol.dist)) {
        bu_log("%s scene: %d subject partitions, exit %.17g; expected one exit at 5\n",
            subject_id == ID_ARS ? "ARS" : "BoT",
            result.subject_hits, result.out_dist);
        failed = 1;
    }

done:
    if (rtip)
        rt_free_rti(rtip);
    wdb_close(wdbp);
    return failed;
}

int
main(int argc, char **argv)
{
    fastf_t curve_data[3][RING_POINTS * ELEMENTS_PER_POINT] = {{0}};
    fastf_t *curves[3] = {curve_data[0], curve_data[1], curve_data[2]};
    fastf_t bot_vertices[(RING_SIDES + 2) * ELEMENTS_PER_POINT] = {0};
    int bot_faces[RING_SIDES * 2 * 3];
    struct rt_bot_internal bot = {0};
    struct rt_ars_internal ars = {0};
    struct rt_db_internal intern;
    struct rt_db_internal bot_intern;
    struct soltab st = {0};
    struct soltab bot_st = {0};
    struct rt_i *rtip;
    struct resource *resp = &rt_uniresource;
    struct application ap;
    struct xray ray = {0};
    vect_t invdir;
    size_t i;
    int failed = 0;

    (void)argc;
    bu_setprogname(argv[0]);

    for (i = 0; i < RING_POINTS; i++) {
        fastf_t angle = 2.0 * M_PI * (i % RING_SIDES) / RING_SIDES;
        VSET(&curve_data[1][i * ELEMENTS_PER_POINT],
            10.0 * cos(angle), 10.0 * sin(angle), 5.0);
        if (i < RING_SIDES)
            VMOVE(&bot_vertices[(i + 1) * ELEMENTS_PER_POINT],
                &curve_data[1][i * ELEMENTS_PER_POINT]);
        curve_data[2][i * ELEMENTS_PER_POINT + Z] = 10.0;
    }
    bot_vertices[(RING_SIDES + 1) * ELEMENTS_PER_POINT + Z] = 10.0;
    for (i = 0; i < RING_SIDES; i++) {
        int current = (int)i + 1;
        int next = (int)((i + 1) % RING_SIDES) + 1;
        int *bottom = &bot_faces[i * 6];
        int *top = &bottom[3];
        bottom[0] = 0;
        bottom[1] = next;
        bottom[2] = current;
        top[0] = current;
        top[1] = next;
        top[2] = RING_SIDES + 1;
    }
    bot.magic = RT_BOT_INTERNAL_MAGIC;
    bot.mode = RT_BOT_SOLID;
    bot.orientation = RT_BOT_CCW;
    bot.num_vertices = RING_SIDES + 2;
    bot.vertices = bot_vertices;
    bot.num_faces = RING_SIDES * 2;
    bot.faces = bot_faces;
    ars.magic = RT_ARS_INTERNAL_MAGIC;
    ars.ncurves = 3;
    ars.pts_per_curve = RING_POINTS;
    ars.curves = curves;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ARS;
    intern.idb_meth = &OBJ[ID_ARS];
    intern.idb_ptr = &ars;
    RT_DB_INTERNAL_INIT(&bot_intern);
    bot_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    bot_intern.idb_type = ID_BOT;
    bot_intern.idb_meth = &OBJ[ID_BOT];
    bot_intern.idb_ptr = &bot;

    rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip)
        return 1;
    rt_init_resource(resp, 0, rtip);

    st.l.magic = RT_SOLTAB_MAGIC;
    st.l2.magic = RT_SOLTAB2_MAGIC;
    st.st_rtip = rtip;
    st.st_meth = &OBJ[ID_ARS];
    st.st_id = ID_ARS;
    if (OBJ[ID_ARS].ft_prep(&st, &intern, rtip)) {
        bu_log("ARS prep failed\n");
        return 1;
    }

    bot_st.l.magic = RT_SOLTAB_MAGIC;
    bot_st.l2.magic = RT_SOLTAB2_MAGIC;
    bot_st.st_rtip = rtip;
    bot_st.st_meth = &OBJ[ID_BOT];
    bot_st.st_id = ID_BOT;
    if (OBJ[ID_BOT].ft_prep(&bot_st, &bot_intern, rtip)) {
        bu_log("BoT prep failed\n");
        return 1;
    }

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;

    ray.magic = RT_RAY_MAGIC;
    VSET(ray.r_pt, 1, 0, 4);
    VSET(ray.r_dir, 1, 0, 0);
    ray.r_min = -0.05; /* the current spatial cell starts inside the mesh */
    failed += check_shot(&OBJ[ID_ARS], &st, &ap, &ray, "ARS cell bound");
    failed += check_shot(&OBJ[ID_BOT], &bot_st, &ap, &ray, "BoT cell bound");

    VSET(invdir, 1, INFINITY, INFINITY);
    if (!rt_in_rpp(&ray, invdir, st.st_min, st.st_max)) {
        bu_log("ARS bounding box missed the test ray\n");
        failed++;
    } else {
        failed += check_shot(&OBJ[ID_ARS], &st, &ap, &ray, "ARS solid bound");
    }

    ray.r_min = -0.05;
    if (!rt_in_rpp(&ray, invdir, bot_st.st_min, bot_st.st_max)) {
        bu_log("BoT bounding box missed the test ray\n");
        failed++;
    } else {
        failed += check_shot(&OBJ[ID_BOT], &bot_st, &ap, &ray, "BoT solid bound");
    }

    OBJ[ID_ARS].ft_free(&st);
    OBJ[ID_BOT].ft_free(&bot_st);
    rt_free_rti(rtip);

    if (argc != 1 && argc != 3) {
        bu_log("Usage: %s [ars_scene.g bot_scene.g]\n", argv[0]);
        return 1;
    }
    {
        const fastf_t *scene_curves[3] = {curve_data[0], curve_data[1], curve_data[2]};
        failed += check_scene(ID_ARS, scene_curves, bot_vertices, bot_faces,
            argc == 3 ? argv[1] : NULL);
        failed += check_scene(ID_BOT, scene_curves, bot_vertices, bot_faces,
            argc == 3 ? argv[2] : NULL);
    }
    return failed ? 1 : 0;
}
