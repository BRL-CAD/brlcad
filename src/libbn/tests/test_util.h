/*                    T E S T _ U T I L . H
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

#ifndef LIBBN_TEST_UTIL_H
#define LIBBN_TEST_UTIL_H

#include "common.h"

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bn.h"


struct bn_api_case {
    const char *name;
    int (*func)(void);
};


void report_failure(const char *test, const char *fmt, ...);
int scalar_close(double a, double b, double tol);
int mat_close(const mat_t a, const mat_t b, double tol);
int vect_close(const vect_t a, const vect_t b, double tol);
int hvect_close(const hvect_t a, const hvect_t b, double tol);
int table_close(const struct bn_table *a, const struct bn_table *b, double tol);
int tabdata_close(const struct bn_tabdata *a, const struct bn_tabdata *b, double tol);
int finite_vec(const vect_t v);
int orthonormal_rotation(const mat_t m, double tol);
void normalize_quat(quat_t q);
int make_temp_path(char path[MAXPATHLEN]);
struct bn_table *make_table(const fastf_t *xs, size_t nx);
struct bn_tabdata *make_tabdata(const struct bn_table *tabp, const fastf_t *ys);
int bn_api_single(int argc, char *argv[], const char *name, int (*func)(void));
int bn_api_dispatch(int argc, char *argv[], const struct bn_api_case *cases);


#endif /* LIBBN_TEST_UTIL_H */
