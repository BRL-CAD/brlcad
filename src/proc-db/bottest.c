/*                       B O T T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2016 United States Government as represented by
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
/** @file proc-db/bottest.c
 *
 * Program to generate test BOT solids
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "rt/geom.h"


static void
printusage(void) {
    fprintf(stderr,"Usage: bottest [filename]\n");
}


int
main(int argc, char **argv)
{
    int faces[15];
    fastf_t vertices[36];
    fastf_t thickness[4];
    struct rt_wdb *outfp = NULL;
    struct bu_bitv *face_mode = NULL;
    static const char *filename = "bot-test.g";

    if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?")) {
	printusage();
	return 0;
    }
    if (argc == 1) {
	printusage();
	fprintf(stderr,"       Program continues running (will create file bot-test.g because 'filename' was blank):\n");
    }
    else if (argc > 1)
	filename = argv[1];

    outfp = wdb_fopen(filename);
    mk_id(outfp, "BOT test");

    VSET(vertices, 0.0, 0.0, 0.0);
    VSET(&vertices[3], 0.0, 100.0, 0.0);
    VSET(&vertices[6], 0.0, 100.0, 50.0);
    VSET(&vertices[9], 200.0, 0.0, 0.0);

    /* face #1 */
    faces[0] = 0;
    faces[1] = 1;
    faces[2] = 2;

    /* face #2 */
    faces[3] = 0;
    faces[4] = 2;
    faces[5] = 3;

    /* face #3 */
    faces[6] = 0;
    faces[7] = 1;
    faces[8] = 3;

    /* face #4 */
    faces[9] = 1;
    faces[10] = 2;
    faces[11] = 3;

    mk_bot(outfp, "bot_u_surf", RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);

    /* face #1 */
    faces[0] = 0;
    faces[1] = 2;
    faces[2] = 1;

    /* face #2 */
    faces[3] = 0;
    faces[4] = 3;
    faces[5] = 2;

    /* face #3 */
    faces[6] = 0;
    faces[7] = 1;
    faces[8] = 3;

    /* face #4 */
    faces[9] = 1;
    faces[10] = 2;
    faces[11] = 3;

    mk_bot(outfp, "bot_ccw_surf", RT_BOT_SURFACE, RT_BOT_CCW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);


    /* face #1 */
    faces[0] = 1;
    faces[1] = 2;
    faces[2] = 0;

    /* face #2 */
    faces[3] = 2;
    faces[4] = 3;
    faces[5] = 0;

    /* face #3 */
    faces[6] = 3;
    faces[7] = 1;
    faces[8] = 0;

    /* face #4 */
    faces[9] = 3;
    faces[10] = 2;
    faces[11] = 1;

    mk_bot(outfp, "bot_cw_surf", RT_BOT_SURFACE, RT_BOT_CW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);


    /* face #1 */
    faces[0] = 0;
    faces[1] = 1;
    faces[2] = 2;

    /* face #2 */
    faces[3] = 0;
    faces[4] = 2;
    faces[5] = 3;

    /* face #3 */
    faces[6] = 0;
    faces[7] = 1;
    faces[8] = 3;

    /* face #4 */
    faces[9] = 1;
    faces[10] = 2;
    faces[11] = 3;

    mk_bot(outfp, "bot_u_solid", RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);

    /* face #1 */
    faces[0] = 0;
    faces[1] = 2;
    faces[2] = 1;

    /* face #2 */
    faces[3] = 0;
    faces[4] = 3;
    faces[5] = 2;

    /* face #3 */
    faces[6] = 0;
    faces[7] = 1;
    faces[8] = 3;

    /* face #4 */
    faces[9] = 1;
    faces[10] = 2;
    faces[11] = 3;

    mk_bot(outfp, "bot_ccw_solid", RT_BOT_SOLID, RT_BOT_CCW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);


    /* face #1 */
    faces[0] = 1;
    faces[1] = 2;
    faces[2] = 0;

    /* face #2 */
    faces[3] = 2;
    faces[4] = 3;
    faces[5] = 0;

    /* face #3 */
    faces[6] = 3;
    faces[7] = 1;
    faces[8] = 0;

    /* face #4 */
    faces[9] = 3;
    faces[10] = 2;
    faces[11] = 1;

    mk_bot(outfp, "bot_cw_solid", RT_BOT_SOLID, RT_BOT_CW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);

    face_mode = bu_bitv_new(4);
    BU_BITSET(face_mode, 1);

    thickness[0] = 2.1;
    thickness[1] = 2.2;
    thickness[2] = 2.3;
    thickness[3] = 2.4;

    /* face #1 */
    faces[0] = 0;
    faces[1] = 1;
    faces[2] = 2;

    /* face #2 */
    faces[3] = 0;
    faces[4] = 2;
    faces[5] = 3;

    /* face #3 */
    faces[6] = 0;
    faces[7] = 1;
    faces[8] = 3;

    /* face #4 */
    faces[9] = 1;
    faces[10] = 2;
    faces[11] = 3;

    mk_bot(outfp, "bot_u_plate", RT_BOT_PLATE, RT_BOT_UNORIENTED, 0, 4, 4, vertices, faces, thickness, face_mode);

    /* face #1 */
    faces[0] = 0;
    faces[1] = 2;
    faces[2] = 1;

    /* face #2 */
    faces[3] = 0;
    faces[4] = 3;
    faces[5] = 2;

    /* face #3 */
    faces[6] = 0;
    faces[7] = 1;
    faces[8] = 3;

    /* face #4 */
    faces[9] = 1;
    faces[10] = 2;
    faces[11] = 3;

    mk_bot(outfp, "bot_ccw_plate", RT_BOT_PLATE, RT_BOT_CCW, 0, 4, 4, vertices, faces, thickness, face_mode);


    /* face #1 */
    faces[0] = 1;
    faces[1] = 2;
    faces[2] = 0;

    /* face #2 */
    faces[3] = 2;
    faces[4] = 3;
    faces[5] = 0;

    /* face #3 */
    faces[6] = 3;
    faces[7] = 1;
    faces[8] = 0;

    /* face #4 */
    faces[9] = 3;
    faces[10] = 2;
    faces[11] = 1;

    mk_bot(outfp, "bot_cw_plate", RT_BOT_PLATE, RT_BOT_CW, 0, 4, 4, vertices, faces, thickness, face_mode);

    /* Make a bot with duplicate vertices to test the "fuse" and "condense" code */
    VSET(vertices, 0.0, 0.0, 0.0);
    VSET(&vertices[3], 0.0, 100.0, 0.0);
    VSET(&vertices[6], 0.0, 100.0, 50.0);
    VMOVE(&vertices[9], &vertices[0]);
    VMOVE(&vertices[12], &vertices[6]);
    VSET(&vertices[15], 200.0, 0.0, 0.0);
    VMOVE(&vertices[18], &vertices[0]);
    VMOVE(&vertices[21], &vertices[3]);
    VMOVE(&vertices[24], &vertices[15]);
    VMOVE(&vertices[27], &vertices[3]);
    VMOVE(&vertices[30], &vertices[6]);
    VMOVE(&vertices[33], &vertices[15]);

    /* face #1 */
    faces[0] = 0;
    faces[1] = 1;
    faces[2] = 2;

    /* face #2 */
    faces[3] = 3;
    faces[4] = 4;
    faces[5] = 5;

    /* face #3 */
    faces[6] = 6;
    faces[7] = 7;
    faces[8] = 8;

    /* face #4 */

    faces[9] = 9;
    faces[10] = 10;
    faces[11] = 11;

    mk_bot(outfp, "bot_solid_dup_vs", RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, 12, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);

    faces[12] = 9;
    faces[13] = 10;
    faces[14] = 11;

    mk_bot(outfp, "bot_solid_dup_fs", RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, 12, 5, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL);

    bu_free((char *)face_mode, "bottest: face_mode");

    wdb_close(outfp);

    return 0;
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
