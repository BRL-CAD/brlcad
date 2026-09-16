/*                     R T _ R E P O R T . H
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
/** @file rt_report.h
 *
 * Header-only logger for the rt_primitives tool: a per-row result
 * record, a console table + coverage summary, and a CSV writer.
 *
 * Two kinds of rows share one table:
 *   - coverage rows (no object): a functab candidate that is MISSING from
 *     the .g, or one we deliberately EXCLUDE from the gate;
 *   - raytrace rows (one per solid object in the .g): the prep/shot result.
 *
 * Validity statuses gate the test (non-zero exit); everything else is
 * informational.  Performance numbers (rays/sec, CV%) are noisy and must
 * NOT be used for any golden comparison.
 */

#ifndef RT_PRIMITIVES_REPORT_H
#define RT_PRIMITIVES_REPORT_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "bu/sort.h"
#include "bu/str.h"

typedef enum {
    PRIM_OK = 0,         /* prepped, shot, hit */
    PRIM_SKIP_NONRT,     /* object whose type lacks ft_prep/ft_shot */
    PRIM_GETTREE_FAIL,   /* rt_gettree failed (load/import)         [GATES] */
    PRIM_PREP_FAIL,      /* rt_prep produced no solids              [GATES] */
    PRIM_SAMPLE_FAIL,    /* ray sampler failed                      [GATES] */
    PRIM_MISSING,        /* candidate type has no object in the .g  [GATES] */
    PRIM_ZERO_HITS,      /* real-shot solid that hit nothing	    [GATES] */
    PRIM_EXCLUDED,       /* candidate deliberately not required (skip-list) */
    PRIM_EXPECTED_ZERO,  /* stub-shot pseudo-solid: zero hits is correct */
    PRIM_UNEXPECTED_HITS /* stub-shot pseudo-solid that DID hit (sentinel) */
} prim_status_t;

struct prim_row {
    char name[64];          /* object name in the .g ("" for coverage rows) */
    char type[16];          /* primitive label, e.g. "ell" */
    int id;                 /* primitive minor type (functab index) */
    prim_status_t status;
    char note[80];          /* free-form reason (e.g. why excluded) */
    size_t nrays;
    size_t hits;
    double rays_per_sec;    /* median (informational) */
    double cv_pct;          /* informational */
    double prep_us;
};

static const char *
prim_status_str(prim_status_t s)
{
    switch (s) {
	case PRIM_OK:             return "OK";
	case PRIM_SKIP_NONRT:     return "SKIP_NONRT";
	case PRIM_GETTREE_FAIL:   return "GETTREE_FAIL";
	case PRIM_PREP_FAIL:      return "PREP_FAIL";
	case PRIM_SAMPLE_FAIL:    return "SAMPLE_FAIL";
	case PRIM_MISSING:        return "MISSING";
	case PRIM_ZERO_HITS:      return "ZERO_HITS";
	case PRIM_EXCLUDED:       return "EXCLUDED";
	case PRIM_EXPECTED_ZERO:  return "EXPECTED_ZERO";
	case PRIM_UNEXPECTED_HITS:return "UNEXPECTED_HITS";
    }
    return "UNKNOWN";
}

/* Statuses that constitute a validity regression (non-zero exit). */
static inline int
prim_status_is_failure(prim_status_t s)
{
    return (s == PRIM_GETTREE_FAIL
	    || s == PRIM_PREP_FAIL
	    || s == PRIM_SAMPLE_FAIL
	    || s == PRIM_ZERO_HITS
	    || s == PRIM_MISSING);
}

/* True if this row carries raytrace measurements worth tabulating. */
static inline int
prim_row_has_perf(const struct prim_row *r)
{
    return (r->status == PRIM_OK
	    || r->status == PRIM_ZERO_HITS
	    || r->status == PRIM_EXPECTED_ZERO
	    || r->status == PRIM_UNEXPECTED_HITS);
}

/* Stable ordering for reproducible console + CSV output: by id, then name. */
static inline int
prim_row_cmp(const void *a, const void *b, void *UNUSED(context))
{
    const struct prim_row *ra = (const struct prim_row *)a;
    const struct prim_row *rb = (const struct prim_row *)b;
    if (ra->id != rb->id)
	return (ra->id < rb->id) ? -1 : 1;
    return bu_strcmp(ra->name, rb->name);
}

static inline void
prim_report_sort(struct prim_row *rows, size_t n)
{
    bu_sort(rows, n, sizeof(struct prim_row), prim_row_cmp, NULL);
}

static inline void
prim_report_console(const struct prim_row *rows, size_t n)
{
    size_t i;

    printf("\n%-16s %3s %-9s %-15s %9s %7s %15s %7s\n",
	   "object", "id", "type", "status", "rays", "hit%", "rays/sec", "cv%");
    printf("--------------------------------------------------------------------------------------\n");

    for (i = 0; i < n; i++) {
	const struct prim_row *r = &rows[i];
	const char *nm = (r->name[0]) ? r->name : "-";
	if (prim_row_has_perf(r)) {
	    double hitpct = (r->nrays > 0) ? (100.0 * (double)r->hits / (double)r->nrays) : 0.0;
	    printf("%-16s %3d %-9s %-15s %9zu %6.1f%% %15.0f %6.1f%%\n",
		   nm, r->id, r->type, prim_status_str(r->status),
		   r->nrays, hitpct, r->rays_per_sec, r->cv_pct);
	} else {
	    printf("%-16s %3d %-9s %-15s", nm, r->id, r->type, prim_status_str(r->status));
	    if (r->note[0])
		printf("   (%s)", r->note);
	    printf("\n");
	}
    }
    printf("--------------------------------------------------------------------------------------\n");
}

/*
 * Coverage tally over the functab candidate set.  required = candidates we
 * gate on (not skip-listed); covered = of those, how many had >=1 object.
 * Prints the headline pass/fail-relevant line and any MISSING types.
 */
static inline void
prim_report_coverage(const struct prim_row *rows, size_t n)
{
    size_t i, excluded = 0, expzero = 0, unexp = 0, zerohits = 0, missing = 0;

    for (i = 0; i < n; i++) {
	switch (rows[i].status) {
	    case PRIM_EXCLUDED:        excluded++; break;
	    case PRIM_EXPECTED_ZERO:   expzero++;  break;
	    case PRIM_UNEXPECTED_HITS: unexp++;    break;
	    case PRIM_MISSING:         missing++;  break;
	    case PRIM_ZERO_HITS:       zerohits++; break;
	    default: break;
	}
    }

    if (missing) {
	printf("\nMISSING required primitives (no example in the database):\n");
	for (i = 0; i < n; i++)
	    if (rows[i].status == PRIM_MISSING)
		printf("    %-9s (id %d)%s%s\n", rows[i].type, rows[i].id,
		       rows[i].note[0] ? " - " : "", rows[i].note);
    }
    if (unexp) {
	printf("\nSENTINEL: stub-shot primitives that unexpectedly produced hits:\n");
	for (i = 0; i < n; i++)
	    if (rows[i].status == PRIM_UNEXPECTED_HITS)
		printf("    %-9s (id %d)\n", rows[i].type, rows[i].id);
    }
    if (zerohits) {
	printf("\nZERO HIT primitives (expected to raytrace, got zero hits):\n");
	for (i = 0; i < n; i++)
	    if (rows[i].status == PRIM_ZERO_HITS)
		printf("    %-9s (id %d)\n", rows[i].type, rows[i].id);
    }

    printf("\ncoverage: %zu excluded, %zu stub-shot expected-zero, %zu missing\n",
	   excluded, expzero, missing);
}

/* Returns 0 on success, <0 on file error. */
static inline int
prim_report_csv(const char *path, const struct prim_row *rows, size_t n)
{
    FILE *fp;
    size_t i;

    if (!path)
	return -1;
    fp = fopen(path, "wb");
    if (!fp)
	return -1;

    fprintf(fp, "id,name,type,status,note,n_rays,hits,hit_rate_pct,rays_per_sec,cv_pct,prep_us\n");
    for (i = 0; i < n; i++) {
	const struct prim_row *r = &rows[i];
	double hitpct = (r->nrays > 0) ? (100.0 * (double)r->hits / (double)r->nrays) : 0.0;
	fprintf(fp, "%d,%s,%s,%s,%s,%zu,%zu,%.3f,%.1f,%.1f,%.3f\n",
		r->id, r->name, r->type, prim_status_str(r->status), r->note,
		r->nrays, r->hits, hitpct, r->rays_per_sec, r->cv_pct, r->prep_us);
    }
    fclose(fp);
    return 0;
}

#endif /* RT_PRIMITIVES_REPORT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
