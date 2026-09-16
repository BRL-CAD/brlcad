/*                 C R E A T E _ P A R I T Y . H
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
/** @file create_parity.h
 *
 * mged commands that deal with "creation" (ie 'make' and 'in')
 * 
 * The command under test ('make', 'in', ...) is supplied at link time by an
 * implementation file (make_cases.c / in_cases.c) that defines the test data
 * (pass_cases/fail_labels/skip_labels), a build_obj() that invokes the command,
 * and any command-specific extra_checks().
 * 
 * The driver is primitive-unaware: each primitive is just a {label, args,
 * expected} row. To add one, append a row with expected==NULL, run the test with
 * the "dump" argument, and paste the printed expected back into the table.
 */

#ifndef CREATE_PARITY_H
#define CREATE_PARITY_H

#include "common.h"

#include "vmath.h"
#include "raytrace.h"
#include "ged.h"

/**
 * each test row
 * 
 * label: rt_functab label
 * args: (optional) command specific args - used only internally by build_obj()
 * expected: expected output for the case - assumes ft_get formatting (see snapshot())
 */
struct cp_case {
    const char* label;
    const char* args;
    const char* expected;
};

/* what is actually being tested. All labels registered in rt_functab should be accounted 
* for; either with an actual test (expect pass), an expected fail, or intentional skip */
extern const struct cp_case pass_cases[];
extern const char *fail_labels[];
extern const char *skip_labels[];

/* used for printing which command is using the driver */
extern const char *cmd_name;

/**
 * build a named object in a database
 * 
 * gedp: ged to create object in
 * name: unique object name
 * label: rt_functab label
 * args: build-specific args (optional -> NULL)
 * 
 * Returns ged_exec status
 */
int build_obj(struct ged *gedp, const char *name, const char *label, const char *args);

/**
 * command-specific extra passes. intentionally generic
 * 
 * Returns number of failures
 */
int extra_checks(struct ged *gedp);

/**
 * snapshot object via ft_get. [DEFINED BY THE DRIVER] - shared so extra_checks() can use it
 * 
 * gedp: ged with object
 * name: name of object in database
 * mat: matrix, applied at import (optional -> NULL == identity)
 * out: captured output
 * 
 * Returns 0 on success
 */
int snapshot(struct ged *gedp, const char *name, const mat_t mat, struct bu_vls *out);

#endif /* CREATE_PARITY_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
