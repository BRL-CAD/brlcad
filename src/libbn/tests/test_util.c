/*                      T E S T _ U T I L . C
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

#include "test_util.h"


void
report_failure(const char *test, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "FAIL %s: ", test);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}


int
scalar_close(double a, double b, double tol)
{
    return fabs(a - b) <= tol;
}


int
mat_close(const mat_t a, const mat_t b, double tol)
{
    size_t i;

    for (i = 0; i < 16; i++) {
	if (!scalar_close(a[i], b[i], tol)) {
	    return 0;
	}
    }

    return 1;
}


int
vect_close(const vect_t a, const vect_t b, double tol)
{
    return VNEAR_EQUAL(a, b, tol);
}


int
hvect_close(const hvect_t a, const hvect_t b, double tol)
{
    return HNEAR_EQUAL(a, b, tol);
}


int
table_close(const struct bn_table *a, const struct bn_table *b, double tol)
{
    size_t i;

    if (!a || !b || a->nx != b->nx) {
	return 0;
    }

    for (i = 0; i <= a->nx; i++) {
	if (!scalar_close(a->x[i], b->x[i], tol)) {
	    return 0;
	}
    }

    return 1;
}


int
tabdata_close(const struct bn_tabdata *a, const struct bn_tabdata *b, double tol)
{
    size_t i;

    if (!a || !b || a->ny != b->ny) {
	return 0;
    }

    if (!table_close(a->table, b->table, tol)) {
	return 0;
    }

    for (i = 0; i < a->ny; i++) {
	if (!scalar_close(a->y[i], b->y[i], tol)) {
	    return 0;
	}
    }

    return 1;
}


int
finite_vec(const vect_t v)
{
    return isfinite(v[X]) && isfinite(v[Y]) && isfinite(v[Z]);
}


int
orthonormal_rotation(const mat_t m, double tol)
{
    vect_t c0, c1, c2;
    vect_t cross;
    fastf_t det;

    VSET(c0, m[0], m[4], m[8]);
    VSET(c1, m[1], m[5], m[9]);
    VSET(c2, m[2], m[6], m[10]);

    if (!scalar_close(MAGNITUDE(c0), 1.0, tol)) return 0;
    if (!scalar_close(MAGNITUDE(c1), 1.0, tol)) return 0;
    if (!scalar_close(MAGNITUDE(c2), 1.0, tol)) return 0;
    if (!scalar_close(VDOT(c0, c1), 0.0, tol)) return 0;
    if (!scalar_close(VDOT(c0, c2), 0.0, tol)) return 0;
    if (!scalar_close(VDOT(c1, c2), 0.0, tol)) return 0;

    VCROSS(cross, c0, c1);
    det = VDOT(cross, c2);

    return scalar_close(det, 1.0, tol);
}


void
normalize_quat(quat_t q)
{
    fastf_t mag;

    mag = sqrt(QMAGSQ(q));
    if (mag > SMALL_FASTF) {
	q[X] /= mag;
	q[Y] /= mag;
	q[Z] /= mag;
	q[W] /= mag;
    }
}


int
make_temp_path(char path[MAXPATHLEN])
{
    FILE *fp;

    fp = bu_temp_file(path, MAXPATHLEN);
    if (!fp) {
	return 0;
    }

    fclose(fp);
    return 1;
}


struct bn_table *
make_table(const fastf_t *xs, size_t nx)
{
    struct bn_table *tabp;
    size_t i;

    BN_GET_TABLE(tabp, nx);
    for (i = 0; i <= nx; i++) {
	tabp->x[i] = xs[i];
    }

    return tabp;
}


struct bn_tabdata *
make_tabdata(const struct bn_table *tabp, const fastf_t *ys)
{
    struct bn_tabdata *data;
    size_t i;

    BN_GET_TABDATA(data, tabp);
    for (i = 0; i < data->ny; i++) {
	data->y[i] = ys[i];
    }

    return data;
}


int
bn_api_single(int argc, char *argv[], const char *name, int (*func)(void))
{
    bu_setprogname(argv[0]);

    if (argc == 1) {
	return func();
    }

    if (argc == 2 && (BU_STR_EQUAL(argv[1], name) || BU_STR_EQUAL(argv[1], "all"))) {
	return func();
    }

    fprintf(stderr, "Usage: %s [%s]\n", argv[0], name);
    return 1;
}


int
bn_api_dispatch(int argc, char *argv[], const struct bn_api_case *cases)
{
    size_t i;
    int failures = 0;

    bu_setprogname(argv[0]);

    if (argc == 1 || (argc == 2 && BU_STR_EQUAL(argv[1], "all"))) {
	for (i = 0; cases[i].name != NULL; i++) {
	    failures += cases[i].func();
	}
	return failures;
    }

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <subtest|all>\n", argv[0]);
	fprintf(stderr, "Available subtests:\n");
	for (i = 0; cases[i].name != NULL; i++) {
	    fprintf(stderr, "  %s\n", cases[i].name);
	}
	return 1;
    }

    for (i = 0; cases[i].name != NULL; i++) {
	if (BU_STR_EQUAL(argv[1], cases[i].name)) {
	    return cases[i].func();
	}
    }

    fprintf(stderr, "Unknown subtest: %s\n", argv[1]);
    return 1;
}
