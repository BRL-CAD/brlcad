/*                 R T _ P R I M I T I V E S . C
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
/** @file rt_primitives.c
 *
 * Per-primitive coverage + validity + rays/sec tool.
 *
 * Opens a .g database (a pre-populated "primitives.g" holding one example
 * of every raytraceable primitive) and does two passes:
 *
 *  1. COVERAGE: walk librt's global OBJ[] functab.  Every slot that has
 *     both ft_prep and ft_shot is a raytraceable "candidate".  For each
 *     candidate, the database must contain at least one object of that
 *     minor type -- otherwise the candidate is MISSING and the test FAILS.
 *     A small skip-list (submodel/poly/hf) is excluded from the gate, and
 *     --optional <label> relaxes a type the current build cannot generate.
 *
 *  2. RAYTRACE: for each present solid, prep it and shoot a deterministic
 *     bundle of quasi-random chord rays (two Sobol points on the bounding
 *     sphere per ray) through it, single-threaded, reporting median
 *     rays/sec + CV%.
 *
 * Gating statuses (non-zero exit): a missing candidate, or a present solid
 * that fails gettree/prep/sampling.  Zero hits on a real solid is suspect
 * but not gated; the pseudo-solids that legitimately never hit (grip,
 * joint, sketch, annot, datum, script) are reported EXPECTED_ZERO, and a
 * sentinel fires if one of them ever starts hitting.  Performance numbers
 * are logged only (CI timing is too noisy to gate).
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bn/sobol.h"
#include "raytrace.h"

#include "rt_report.h"

#define MAX_ROWS        64	/* max number of output rows (there's only 46 prims in table.cpp) */
#define MAX_SAMPLES     256	/* max number of timing samples per prim */
#define UNBOUNDED_R     1.0e8   /* bounding radius effective max; above this (e.g. half-space) are "infinite" */
#define SEED_BASE       1UL     /* bn_sobol_create() seed base (per-primitive Sobol seed = SEED_BASE + id) */

/*
 * Candidates we intentionally do NOT require in the database.  Each has a
 * reason it cannot be exercised as a plain volumetric solid in a portable
 * regression database.
 */
struct id_note { int id; const char *note; };
static const struct id_note skip_list[] = {
    { ID_REC,      "(deprecated?) shares rt_tgc import/export; no distinct on-disk minor type" },
    { ID_SPH,      "(deprecated?) shares rt_ell import/export; no distinct on-disk minor type" },
    { ID_POLY,     "(deprecated) v4 polysolid (superseded by bot)" },
    { ID_HF,       "(deprecated) needs external data file (superseded by dsp)" },
    { ID_BSPLINE,  "(deprecated) asc2g no longer imports bspline (superseded by brep)" },
};
static const size_t skip_list_n = sizeof(skip_list) / sizeof(skip_list[0]);

/*
 * Pseudo-solids: prep+shot are present but the shot is either a WIP stub or
 * something that does not produce volumetric hits.
 */
static const int pseudo_solid_ids[] = {
    ID_GRIP, ID_JOINT, ID_SKETCH, ID_ANNOT, ID_DATUM, ID_SCRIPT,
};
static const size_t pseudo_solid_ids_n = sizeof(pseudo_solid_ids) / sizeof(pseudo_solid_ids[0]);

struct opts {
    size_t nrays;
    size_t warmup;
    size_t samples;
};

/* GLOBAL hit counter (NOT thread safe) */
static size_t g_hits;

/* dummy hit function to track global hits */
static int hit_f(struct application* UNUSED(ap), struct partition* UNUSED(ph), struct seg* UNUSED(sg)) {
    g_hits++;
    return 1;
}

/* dummy miss function - we don't care about them */
static int miss_f(struct application* UNUSED(ap)) {
    return 0;
}

/* shoot n rays */
static void shoot_all(struct application *ap, const fastf_t *rays, size_t n) {
    for (size_t i = 0; i < n; i++) {
	const fastf_t *r = &rays[i * 6];
	VSET(ap->a_ray.r_pt, r[0], r[1], r[2]);
	VSET(ap->a_ray.r_dir, r[3], r[4], r[5]);
	(void)rt_shootray(ap);
    }
}

/* compare function for qsort */
static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/* assume an entry in rt_functab is raytrace-able if it has:
 * - valid ID
 * - valid magic
 * - prep function registered
 * - shot function registered
 */
static int is_candidate(int id) {
    return (id > 0 && id <= ID_MAX_SOLID
	    && OBJ[id].magic == RT_FUNCTAB_MAGIC
	    && OBJ[id].ft_prep && OBJ[id].ft_shot);
}

static const char* id_label(int id) {
    return (id > 0 && id <= ID_MAX_SOLID && OBJ[id].magic == RT_FUNCTAB_MAGIC)
	? OBJ[id].ft_label : "?";
}

static int in_id_set(int id, const int *set, size_t n) {
    for (size_t i = 0; i < n; i++)
	if (set[i] == id)
	    return 1;
    return 0;
}

static const char* get_skip_note(int id) {
    for (size_t i = 0; i < skip_list_n; i++)
	if (skip_list[i].id == id)
	    return skip_list[i].note;
    return NULL;
}

/*
 * Deterministic quasi-random rays
 * Each ray is the chord between two Sobol-sampled points on the bounding 
 * sphere: the ray starts at point1 and is in the direction of point2 (ie
 * through the bounding sphere). 
 * 
 * Returns a malloc'd array of got*6 fastf_t, or NULL. 
 *  NOTE: caller's responsibility to free array
 */
static fastf_t* sample_chords(const point_t center, double radius, size_t want,
			      unsigned long seed, size_t *got)
{
    struct bn_soboldata *s;
    fastf_t *rays;
    size_t i, made = 0;

    *got = 0;
    if (want == 0)
	return NULL;

    s = bn_sobol_create(2, seed);
    if (!s)
	return NULL;

    rays = (fastf_t *)bu_calloc(want * 6, sizeof(fastf_t), "chord rays");

    for (i = 0; i < want; i++) {
	point_t p1, p2;
	vect_t d;
	bn_sobol_sph_sample(p1, center, radius, s);
	bn_sobol_sph_sample(p2, center, radius, s);
	VSUB2(d, p2, p1);
	if (MAGNITUDE(d) < SMALL_FASTF)
	    continue;   /* degenerate pair; the generator has still advanced */
	VUNITIZE(d);
	VMOVE(&rays[made * 6], p1);
	VMOVE(&rays[made * 6 + 3], d);
	made++;
    }

    bn_sobol_destroy(s);

    if (made == 0) {
	bu_free(rays, "chord rays");
	return NULL;
    }
    *got = made;
    return rays;
}

static void run_solid(struct db_i *dbip, const char *name, int id, int is_stub,
		      const struct opts *o, struct prim_row *row)
{
    struct rt_i *rtip;
    struct application ap;
    point_t center;
    double R;
    fastf_t *rays;
    size_t got = 0, w, s, ns = 0;
    double vals[MAX_SAMPLES];
    int64_t t0;

    memset(row, 0, sizeof(*row));
    bu_strlcpy(row->name, name, sizeof(row->name));
    bu_strlcpy(row->type, id_label(id), sizeof(row->type));
    row->id = id;

    rtip = rt_new_rti(dbip);
    if (!rtip || rt_gettree(rtip, name) != 0) {
	row->status = PRIM_GETTREE_FAIL;
	if (rtip)
	    rt_free_rti(rtip);
	return;
    }

    t0 = bu_gettime();
    /* we use a global hit counter; MUST run single threaded */
    rt_prep_parallel(rtip, 1);
    row->prep_us = (double)(bu_gettime() - t0);

    if (rtip->stats.nsolids == 0) {
	row->status = PRIM_PREP_FAIL;
	rt_free_rti(rtip);
	return;
    }

    /* Bounding sphere for generating rays; guard infinite/degenerate (e.g. half). */
    VADD2(center, rtip->mdl_min, rtip->mdl_max);
    VSCALE(center, center, 0.5);
    R = rtip->rti_radius;
    if (!(R > SMALL_FASTF) || !isfinite(R) || R > UNBOUNDED_R) {
	R = 1000.0;
	VSETALL(center, 0.0);
    }

    rays = sample_chords(center, R, o->nrays, SEED_BASE + (unsigned long)id, &got);
    if (!rays || got == 0) {
	row->status = PRIM_SAMPLE_FAIL;
	rt_free_rti(rtip);
	return;
    }
    row->nrays = got;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = &rt_uniresource;
    ap.a_hit = hit_f;
    ap.a_miss = miss_f;
    ap.a_onehit = 0;

    /* Prime + count hits (deterministic across passes). */
    g_hits = 0;
    shoot_all(&ap, rays, got);
    row->hits = g_hits;

    /* warmup runs before timing */
    for (w = 0; w < o->warmup; w++)
	shoot_all(&ap, rays, got);

    /* primed + warmed - collect shoot times */
    for (s = 0; s < o->samples && ns < MAX_SAMPLES; s++) {
	int64_t a = bu_gettime();
	double dt;
	shoot_all(&ap, rays, got);
	dt = (double)(bu_gettime() - a) / 1.0e6;
	if (dt > 0.0)
	    vals[ns++] = (double)got / dt;
    }

    /* if we got samples calculate rays_per_sec and convergence */
    if (ns > 0) {
	double mean = 0.0, var = 0.0;
	size_t i;
	qsort(vals, ns, sizeof(double), cmp_double);
	row->rays_per_sec = (ns % 2) ? vals[ns / 2]
	    : 0.5 * (vals[ns / 2 - 1] + vals[ns / 2]);
	for (i = 0; i < ns; i++)
	    mean += vals[i];
	mean /= (double)ns;
	for (i = 0; i < ns; i++) {
	    double d = vals[i] - mean;
	    var += d * d;
	}
	var /= (double)ns;
	row->cv_pct = (mean > 0.0) ? (100.0 * sqrt(var) / mean) : 0.0;
    }

    if (is_stub)
	/* protect against stubs getting implemented without updating this regression test */
	row->status = (row->hits == 0) ? PRIM_EXPECTED_ZERO : PRIM_UNEXPECTED_HITS;
    else
	/* assume we're ok if we got atleast one hit (this isn't a validation test) */
	row->status = (row->hits == 0) ? PRIM_ZERO_HITS : PRIM_OK;

    bu_free(rays, "chord rays");
    rt_free_rti(rtip);
}

/* An empty filter table matches everything; otherwise match name or type. */
static int filter_match(const char *name, const char *type, struct bu_ptbl *filters) {
    size_t i;
    if (BU_PTBL_LEN(filters) == 0)
	return 1;
    for (i = 0; i < BU_PTBL_LEN(filters); i++) {
	const char *f = (const char *)BU_PTBL_GET(filters, i);
	if (BU_STR_EQUAL(f, name) || BU_STR_EQUAL(f, type))
	    return 1;
    }
    return 0;
}

/* bu_opt callback for the repeatable -p/--prim filter; stores the argv string
 * (which outlives the run) directly, no copy needed. */
static int add_filter(struct bu_vls *msg, size_t argc, const char **argv, void *set_var) {
    struct bu_ptbl *filters = (struct bu_ptbl *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "prim");
    bu_ptbl_ins(filters, (long *)argv[0]);
    return 1;
}

static void usage(const struct bu_opt_desc *d) {
    char *opts = bu_opt_describe((struct bu_opt_desc *)d, NULL);
    bu_log("Usage: %s [options] primitives.g\n", bu_getprogname());
    bu_log("  Per-primitive functab coverage + validity + rays/sec over a .g database.\n\n");
    bu_log("Options:\n%s", opts);
    bu_free(opts, "opt usage");
}

int main(int argc, char **argv)
{
    struct opts o;
    struct prim_row rows[MAX_ROWS];
    struct bu_ptbl filters;
    int present[ID_MAXIMUM + 1];
    size_t nrows = 0, i, fails = 0;
    int id;
    const char *dbpath = NULL;
    const char *csv_path = NULL;
    struct db_i *dbip;
    struct directory *dp;

    int print_help = 0;
    int nrays = 10000, warmup = 2, samples = 5;
    int uac;
    struct bu_vls pmsg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[7];

    bu_setprogname(argv[0]);
    bu_ptbl_init(&filters, 8, "prim filters");

    BU_OPT(d[0], "h", "help",    "",     NULL,        &print_help, "Print help and exit");
    BU_OPT(d[1], "n", "rays",    "#",    &bu_opt_int, &nrays,      "rays per primitive (default 10000)");
    BU_OPT(d[2], "w", "warmup",  "#",    &bu_opt_int, &warmup,     "discarded warmup passes (default 2)");
    BU_OPT(d[3], "s", "samples", "#",    &bu_opt_int, &samples,    "measured passes (default 5)");
    BU_OPT(d[4], "p", "prim",    "name", &add_filter, &filters,    "restrict raytrace pass to object name/type (repeatable)");
    BU_OPT(d[5], "",  "csv",     "file", &bu_opt_str, &csv_path,   "write results as CSV");
    BU_OPT_NULL(d[6]);

    /* skip the program name, then let bu_opt handle the rest */
    argc--; argv++;
    uac = bu_opt_parse(&pmsg, (size_t)argc, (const char **)argv, d);
    if (uac < 0) {
	bu_log("%s", bu_vls_cstr(&pmsg));
	bu_vls_free(&pmsg);
	usage(d);
	bu_ptbl_free(&filters);
	return 1;
    }
    bu_vls_free(&pmsg);

    if (print_help) {
	usage(d);
	bu_ptbl_free(&filters);
	return 0;
    }

    /* assume leftover is .g */
    if (uac < 1) {
	usage(d);
	bu_ptbl_free(&filters);
	return 1;
    }
    dbpath = argv[0];

    o.nrays   = (nrays   > 0) ? (size_t)nrays   : 0;
    o.warmup  = (warmup  > 0) ? (size_t)warmup  : 0;
    o.samples = (samples > 0) ? (size_t)samples : 0;

    /* open samples db */
    dbip = db_open(dbpath, DB_OPEN_READONLY);
    if (!dbip) {
	bu_log("ERROR: cannot open database '%s'\n", dbpath);
	bu_ptbl_free(&filters);
	return 1;
    }
    if (db_dirbuild(dbip) < 0) {
	bu_log("ERROR: db_dirbuild('%s') failed\n", dbpath);
	db_close(dbip);
	bu_ptbl_free(&filters);
	return 1;
    }
    db_update_nref(dbip);

    bu_log("rt_primitives: %s  rays=%zu warmup=%zu samples=%zu\n",
	   dbpath, o.nrays, o.warmup, o.samples);

    /*** 
     * first pass: consolidate coverage of .g and rt_functab 
     ***/
    /* ingest .g - solids are marked 1/0 for existance */
    memset(present, 0, sizeof(present));
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_flags & RT_DIR_COMB)
	    continue;
	if (!(dp->d_flags & RT_DIR_SOLID))
	    continue;
	if (dp->d_minor_type > 0 && dp->d_minor_type <= ID_MAX_SOLID)
	    present[dp->d_minor_type] = 1;
    } FOR_ALL_DIRECTORY_END;
    /* ingest rt_functab - identify 'candidates' that *should* be raytrace-able  */
    for (id = 1; id <= ID_MAX_SOLID; id++) {
	const char *note;
	if (!is_candidate(id))
	    continue;
	if (nrows >= MAX_ROWS)
	    break;

	note = get_skip_note(id);
	if (note) {
	    /* skip-listed: excluded from the gate regardless of presence. */
	    struct prim_row *r = &rows[nrows++];
	    memset(r, 0, sizeof(*r));
	    r->id = id;
	    bu_strlcpy(r->type, id_label(id), sizeof(r->type));
	    r->status = PRIM_EXCLUDED;
	    bu_strlcpy(r->note, note, sizeof(r->note));
	    continue;
	}
	if (present[id])
	    continue;   /* covered; the raytrace pass will report it */

	/* absent and required: a coverage regression. */
	{
	    struct prim_row *r = &rows[nrows++];
	    memset(r, 0, sizeof(*r));
	    r->id = id;
	    bu_strlcpy(r->type, id_label(id), sizeof(r->type));
	    r->status = PRIM_MISSING;
	}
    }

    /*** 
     * second pass: test what we found
     ***/
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	int mt = (int)dp->d_minor_type;
	if (dp->d_flags & RT_DIR_COMB)
	    continue;
	if (dp->d_flags & RT_DIR_HIDDEN)
	    continue;
	if (!(dp->d_flags & RT_DIR_SOLID))
	    continue;
	if (nrows >= MAX_ROWS)
	    break;

	if (!filter_match(dp->d_namep, id_label(mt), &filters))
	    continue;
	if (get_skip_note(mt))
	    continue;   /* excluded type; coverage row already recorded */

	if (!is_candidate(mt)) {
	    struct prim_row *r = &rows[nrows++];
	    memset(r, 0, sizeof(*r));
	    bu_strlcpy(r->name, dp->d_namep, sizeof(r->name));
	    bu_strlcpy(r->type, id_label(mt), sizeof(r->type));
	    r->id = mt;
	    r->status = PRIM_SKIP_NONRT;
	    continue;
	}


	bu_log("  %s ...\n", dp->d_namep);
	run_solid(dbip, dp->d_namep, mt, in_id_set(mt, pseudo_solid_ids, pseudo_solid_ids_n), &o, &rows[nrows]);
	nrows++;
    } FOR_ALL_DIRECTORY_END;

    /* report */
    prim_report_sort(rows, nrows);
    prim_report_console(rows, nrows);
    prim_report_coverage(rows, nrows);
    if (csv_path) {
	if (prim_report_csv(csv_path, rows, nrows) < 0)
	    bu_log("ERROR: could not write CSV to %s\n", csv_path);
	else
	    bu_log("CSV written to %s\n", csv_path);
    }

    /* count failures */
    for (i = 0; i < nrows; i++)
	if (prim_status_is_failure(rows[i].status))
	    fails++;

    /* cleanup resources */
    db_close(dbip);
    bu_ptbl_free(&filters);

    if (fails) {
	bu_log("FAIL: %zu primitive(s) failed coverage/validity\n", fails);
	return 1;
    }

    bu_log("PASS: all primitives accounted for\n");
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
