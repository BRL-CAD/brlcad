/*                  T E S S _ T I M I N G . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file tess_timing.c
 *
 * Standalone utility for measuring per-primitive tessellation time from
 * a BRL-CAD .g database file.
 *
 * Opens a .g database, iterates every solid primitive, times how long
 * rt_obj_tess() takes for each one, and prints a report sorted by
 * elapsed time (slowest first).
 *
 * Usage:
 *   rt_tess_timing [-t <threshold_ms>] [-v] [-p] [-s <type>]
 *                  [-a <abs>] [-r <rel>] [-n <norm>] <file.g>
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"
#include "bu/time.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/db_internal.h"


/* ------------------------------------------------------------------ */
/* Per-primitive result record                                          */
/* ------------------------------------------------------------------ */

struct tess_result {
    char     name[256];
    char     type[32];
    double   time_ms;
    long     nfaces;
    long     nverts;
    int      failed;   /* 1 if rt_obj_tess() returned non-zero or bombed */
};


/* ------------------------------------------------------------------ */
/* Count faces and vertices in a tessellated NMG model                 */
/* ------------------------------------------------------------------ */

static void
count_nmg_mesh(struct model *m, long *nfaces_out, long *nverts_out)
{
    struct nmgregion *r;
    struct shell     *s;
    long nf = 0;
    long nv = 0;

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    struct faceuse *fu;
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		if (fu->orientation != OT_SAME) continue;
		nf++;
		/* Count vertices via edgeuses in each loop */
		struct loopuse *lu;
		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			struct edgeuse *eu;
			for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd))
			    nv++;
		    }
		}
	    }
	}
    }

    *nfaces_out = nf;
    *nverts_out = nv;
}


/* ------------------------------------------------------------------ */
/* Comparison function for bu_sort (descending by time_ms)               */
/* ------------------------------------------------------------------ */

static int
cmp_result_desc(const void *a, const void *b, void *UNUSED(data))
{
    const struct tess_result *ra = (const struct tess_result *)a;
    const struct tess_result *rb = (const struct tess_result *)b;
    if (rb->time_ms > ra->time_ms) return  1;
    if (rb->time_ms < ra->time_ms) return -1;
    return 0;
}


/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    const char *usage =
	"Usage: rt_tess_timing [-t <ms>] [-v] [-p] [-s <type>,...]\n"
	"                      [-a <abs>] [-r <rel>] [-n <norm>] <file.g>\n"
	"  -t <ms>        Only print primitives slower than this many ms (default: 0)\n"
	"  -v             Verbose: print all primitives, even those that took 0 ms\n"
	"  -p             Progress: print each primitive name before tessellating\n"
	"  -s <type>,...  Skip primitives of these type names (comma-separated).\n"
	"                 Example: -s ID_DSP,ID_BOT  skips DSP and BOT primitives.\n"
	"                 Brep (ID_BREP) is always skipped.\n"
	"  -a <abs>       Absolute tessellation tolerance in mm (default: 0.0)\n"
	"  -r <rel>       Relative tessellation tolerance    (default: 0.01)\n"
	"  -n <norm>      Normal tolerance in radians         (default: 0.0)\n";

    double    threshold_ms = 0.0;
    int       verbose      = 0;
    int       progress     = 0;
    double    abs_tol      = 0.0;
    double    rel_tol      = 0.01;
    double    norm_tol     = 0.0;

    /* Up to 32 type-name prefixes to skip */
#define MAX_SKIP_TYPES 32
    char *skip_types[MAX_SKIP_TYPES];
    int   nskip = 0;

    /* Parse options */
    int opt;
    while ((opt = bu_getopt(argc, argv, "t:vps:a:r:n:h")) != -1) {
	switch (opt) {
	    case 't': threshold_ms = atof(bu_optarg); break;
	    case 'v': verbose = 1; break;
	    case 'p': progress = 1; break;
	    case 's': {
		/* Parse comma-separated list of type names to skip */
		char *buf = bu_strdup(bu_optarg);
		char *tok = strtok(buf, ",");
		while (tok && nskip < MAX_SKIP_TYPES) {
		    skip_types[nskip++] = bu_strdup(tok);
		    tok = strtok(NULL, ",");
		}
		bu_free(buf, "skip_types buf");
		break;
	    }
	    case 'a': abs_tol  = atof(bu_optarg); break;
	    case 'r': rel_tol  = atof(bu_optarg); break;
	    case 'n': norm_tol = atof(bu_optarg); break;
	    case 'h':
	    default:
		fputs(usage, stderr);
		return (opt == 'h') ? 0 : 1;
	}
    }

    if (bu_optind >= argc) {
	fputs(usage, stderr);
	return 1;
    }

    const char *filename = argv[bu_optind];

    /* Set up tolerances */
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    ttol.magic = BG_TESS_TOL_MAGIC;
    ttol.abs   = abs_tol;
    ttol.rel   = rel_tol;
    ttol.norm  = norm_tol;

    struct bn_tol tol = BN_TOL_INIT_ZERO;
    tol.magic   = BN_TOL_MAGIC;
    tol.dist    = 0.005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp    = 1e-6;
    tol.para    = 1.0 - tol.perp;

    printf("rt_tess_timing: opening %s\n", filename);
    printf("rt_tess_timing: tolerance: abs=%f rel=%f norm=%f\n",
	   abs_tol, rel_tol, norm_tol);

    /* Open the database */
    struct db_i *dbip = db_open(filename, DB_OPEN_READONLY);
    if (dbip == DBI_NULL) {
	fprintf(stderr, "rt_tess_timing: cannot open %s\n", filename);
	return 1;
    }

    if (db_dirbuild(dbip) < 0) {
	fprintf(stderr, "rt_tess_timing: db_dirbuild() failed\n");
	db_close(dbip);
	return 1;
    }

    /* Count total directory entries first */
    long total_objects = 0;
    {
	struct directory *dp;
	FOR_ALL_DIRECTORY_START(dp, dbip)
	    if (!(dp->d_flags & RT_DIR_COMB) && !(dp->d_flags & RT_DIR_HIDDEN))
		total_objects++;
	FOR_ALL_DIRECTORY_END
    }

    printf("rt_tess_timing: scanning %ld database objects...\n", total_objects);
    fflush(stdout);

    /* Allocate result array (ensure at least 1 element to avoid zero-size calloc) */
    struct tess_result *results = (struct tess_result *)bu_calloc(
	(size_t)(total_objects > 0 ? total_objects : 1),
	sizeof(struct tess_result), "results");
    long nresults = 0;

    /* Iterate all solid primitives */
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_flags & RT_DIR_COMB)   continue;
	if (dp->d_flags & RT_DIR_HIDDEN) continue;

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);

	int got = rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
	if (got < 0) {
	    /* Cannot load; record as failed with 0 time */
	    struct tess_result *tr = &results[nresults++];
	    bu_strlcpy(tr->name, dp->d_namep, sizeof(tr->name));
	    bu_strlcpy(tr->type, "?", sizeof(tr->type));
	    tr->failed = 1;
	    continue;
	}

	/* Skip brep primitives – brep tessellation has known issues that can
	 * hang the process; they are excluded from this timing survey. */
	if (intern.idb_type == ID_BREP) {
	    rt_db_free_internal(&intern);
	    continue;
	}

	/* Capture object type name */
	const char *type_name = "?";
	if (intern.idb_meth)
	    type_name = intern.idb_meth->ft_name;

	/* Check user-requested skip list */
	int skip_this = 0;
	for (int si = 0; si < nskip; si++) {
	    if (bu_strcmp(type_name, skip_types[si]) == 0) {
		skip_this = 1;
		break;
	    }
	}
	if (skip_this) {
	    rt_db_free_internal(&intern);
	    continue;
	}

	/* Optionally print name before tessellating so a hang is identifiable */
	if (progress) {
	    printf("  tessellating: %-20s  %s\n", type_name, dp->d_namep);
	    fflush(stdout);
	}

	struct model      *m   = nmg_mm();
	struct nmgregion  *r   = NULL;
	int                ret = -1;
	long               nf  = 0;
	long               nv  = 0;
	int                bombed = 0;

	int64_t t0 = bu_gettime();

	if (!BU_SETJUMP) {
	    ret = rt_obj_tess(&r, m, &intern, &ttol, &tol);
	} else {
	    BU_UNSETJUMP;
	    bombed = 1;
	}
	BU_UNSETJUMP;

	int64_t t1 = bu_gettime();
	double elapsed_ms = (double)(t1 - t0) / 1000.0;

	if (!bombed && ret == 0 && m != NULL) {
	    count_nmg_mesh(m, &nf, &nv);
	    nmg_km(m);
	} else {
	    nmg_km(m);
	}

	rt_db_free_internal(&intern);

	struct tess_result *tr = &results[nresults++];
	bu_strlcpy(tr->name, dp->d_namep, sizeof(tr->name));
	bu_strlcpy(tr->type, type_name, sizeof(tr->type));
	tr->time_ms = elapsed_ms;
	tr->nfaces  = nf;
	tr->nverts  = nv;
	tr->failed  = (bombed || ret != 0) ? 1 : 0;

    } FOR_ALL_DIRECTORY_END

    /* Sort descending by time */
    bu_sort(results, (size_t)nresults, sizeof(struct tess_result), cmp_result_desc, NULL);

    /* Compute total elapsed time */
    double total_ms = 0.0;
    for (long i = 0; i < nresults; i++)
	total_ms += results[i].time_ms;

    /* Print report */
    printf("\n  %3s  %9s  %6s  %6s  %-12s  %s\n",
	   "#", "time_ms", "faces", "verts", "type", "name");

    long shown = 0;
    for (long i = 0; i < nresults; i++) {
	struct tess_result *tr = &results[i];

	/* Apply threshold / verbose filter */
	if (!verbose && threshold_ms <= 0.0 && tr->time_ms <= 0.0)
	    continue;
	if (!verbose && threshold_ms > 0.0 && tr->time_ms < threshold_ms)
	    continue;

	shown++;
	if (tr->failed) {
	    printf("  %3ld  %9.1f  %6s  %6s  %-12s  %s [FAILED]\n",
		   shown, tr->time_ms, "-", "-", tr->type, tr->name);
	} else {
	    printf("  %3ld  %9.1f  %6ld  %6ld  %-12s  %s\n",
		   shown, tr->time_ms, tr->nfaces, tr->nverts,
		   tr->type, tr->name);
	}
    }

    printf("  --\n");
    printf("  Total measured: %ld objects in %.0f ms\n", nresults, total_ms);
    printf("  Threshold: %.0f ms  (showing %ld of %ld)\n",
	   threshold_ms, shown, nresults);

    bu_free(results, "results");
    db_close(dbip);

    /* Free skip type strings */
    for (int si = 0; si < nskip; si++)
	bu_free(skip_types[si], "skip type");

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
