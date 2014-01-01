/*                F I X _ P O L Y S O L I D S . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2014 United States Government as represented by
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
/** @file util/fix_polysolids.c
 *
 * Program to fix polysolids with bad normals.
 *
 */

/* === for auto-man-page use: === */
/*
 * purpose: fix polysolids with bad normals
 *
 * description:
 *
 * Manually edited or auto-generated polysolids can have bad normals.
 * This program attempts to correct those.
 *
 *
 * opt: -v turn on verbose mode
 *
 * opt: -x <librt debug flag> turn on librt debugging
 *          (option may used more than once)
 *
 * opt: -X <nmg debug flag> turn on nmg debugging
 *         (option may used more than once)
 *
 * opt: -h brief help
 *
 * opt: -? brief help
 *
 */

#include "common.h"

#include <bio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "db.h"
#include "bn.h"
#include "bu.h"

/*
 * M A I N
 */
int
main(int argc, char *argv[])
{

    const char usage[] =
      "Usage: %s [-v] [-x LDEBUG_FLAG] [-X NDEBUG_FLAG] < old_db.g > new_db.g\n"
      "\n"
      "Options:\n"
      "  v - verbose\n"
      "  x - librt debug flag\n"
      "  X - nmg debug flag\n"
      ;
    const char optstring[] = "vx:X:h?";

    static int verbose;
    static struct bn_tol tol;

    union record rec;
    int c;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct bu_ptbl faces;
    int done=0;

    /* XXX These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	bu_exit(1, usage, argv[0]);

    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, optstring)) != -1) {
	switch (c) {
	    unsigned int debug;
	    case 'v':
		verbose++;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", &debug);
		RTG.debug = debug;
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", &debug);
		RTG.NMG_debug = debug;
		break;
	    default:
		bu_exit(1, usage, argv[0]);
	}
    }

    bu_ptbl_init(&faces, 64, "faces");
    m = nmg_mmr();
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    while (1) {
	struct vertex *verts[5];
	union record rec2 = {0};
	int i;
	size_t ret;

	if (done == 2) {
	    rec = rec2;
	    done = 0;
	} else {
	    if (fread(&rec, sizeof(union record), 1, stdin) != 1)
		break;
	}

	switch (rec.u_id) {
	    case ID_FREE:
		continue;
		break;
	    case ID_P_HEAD:
		bu_log("Polysolid (%s)\n", rec.p.p_name);
		s = nmg_msv(r);
		bu_ptbl_reset(&faces);
		while (!done) {
		    struct faceuse *fu;
		    struct loopuse *lu;
		    struct edgeuse *eu;
		    point_t pt;

		    if (fread(&rec2, sizeof(union record), 1, stdin) != 1)
			done = 1;
		    if (rec2.u_id != ID_P_DATA)
			done = 2;

		    if (done)
			break;

		    for (i = 0; i < 5; i++)
			verts[i] = (struct vertex *)NULL;

		    fu = nmg_cface(s, verts, rec2.q.q_count);
		    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
		    for (i = 0; i < rec2.q.q_count; i++) {
			VMOVE(pt, rec2.q.q_verts[i]);
			nmg_vertex_gv(eu->vu_p->v_p, pt);
			eu = BU_LIST_NEXT(edgeuse, &eu->l);
		    }

		    if (nmg_calc_face_g(fu)) {
			bu_log("\tEliminating degenerate face\n");
			nmg_kfu(fu);
		    } else {
			bu_ptbl_ins(&faces, (long *)fu);
		    }
		}
		nmg_rebound(m, &tol);
		(void)nmg_break_long_edges(s, &tol);
		(void)nmg_vertex_fuse(&m->magic, &tol);
		nmg_gluefaces((struct faceuse **)BU_PTBL_BASEADDR(&faces), BU_PTBL_END(&faces), &tol);
		nmg_fix_normals(s, &tol);

		break;
	    default:
		ret = fwrite(&rec, sizeof(union record), 1, stdout);
		if (ret < 1)
		    perror("fwrite");
		break;
	}
    }
    bu_ptbl_free(&faces);

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
