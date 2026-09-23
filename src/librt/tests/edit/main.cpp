/*                         M A I N . C P P
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
/** @file main.cpp
 *
 * Dispatch primitive edit tests by name.  CTest runs each case in its
 * own process so that fixture state and failures remain isolated.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/defines.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/edit.h"

struct edit_case {
    const char *name;
    int (*run)(void);
};

#include "cases.inc"
#include "operations.inc"

static const struct edit_case *
find_case(const char *name)
{
    for (const struct edit_case *test = edit_cases; test->name; ++test) {
        if (BU_STR_EQUAL(name, test->name))
            return test;
    }
    return NULL;
}

static int
expected_has(const char *primitive, int command_id)
{
    for (const struct expected_operation *op = expected_operations;
         op->primitive; ++op) {
        if (BU_STR_EQUAL(op->primitive, primitive) &&
            op->command_id == command_id)
            return 1;
    }
    return 0;
}

/* This checks descriptor inventory, not execution of each command. */
static int
list_operations(int check)
{
    int errors = 0;
    int matched = 0;
    bu_log("primitive\tcommand_id\tfixture\toperation\n");
    for (int i = 1; EDOBJ[i].magic == RT_FUNCTAB_MAGIC; ++i) {
        if (!EDOBJ[i].ft_edit)
            continue;

        const char *name = EDOBJ[i].ft_label;
        if (!name)
            return BRLCAD_ERROR;
        const struct edit_case *test = find_case(name);
        const struct rt_edit_prim_desc *desc =
            EDOBJ[i].ft_edit_desc ? EDOBJ[i].ft_edit_desc() : NULL;
        if (!desc || !desc->prim_type || !BU_STR_EQUAL(desc->prim_type, name)) {
            bu_log("%s\t-\t%s\tno native descriptor\n",
                   name, test ? "yes" : "no");
            if (expected_has(name, NO_DESCRIPTOR))
                ++matched;
            else if (check)
                ++errors;
            continue;
        }
        if (desc->ncmd > 0 && !desc->cmds) {
            bu_log("%s has commands but no command array\n", name);
            return BRLCAD_ERROR;
        }
        for (int j = 0; j < desc->ncmd; ++j) {
            const struct rt_edit_cmd_desc *cmd = &desc->cmds[j];
            bu_log("%s\t%d\t%s\t%s\n", name, cmd->cmd_id,
                   test ? "yes" : "no", cmd->label ? cmd->label : "");
            if (!test)
                ++errors;
            if (expected_has(name, cmd->cmd_id))
                ++matched;
            else if (check)
                ++errors;
            for (int k = 0; k < j; ++k) {
                if (desc->cmds[k].cmd_id == cmd->cmd_id) {
                    bu_log("%s has duplicate command ID %d\n",
                           name, cmd->cmd_id);
                    return BRLCAD_ERROR;
                }
            }
        }
    }
    if (check) {
        int expected = 0;
        for (const struct expected_operation *op = expected_operations;
             op->primitive; ++op)
            ++expected;
        if (matched != expected) {
            bu_log("Descriptor inventory changed: matched %d of %d entries\n",
                   matched, expected);
            ++errors;
        }
    }
    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}

static void
usage(const char *progname)
{
    bu_log("Usage: %s <case|--list|--inventory|--check-inventory>\n",
           progname);
}

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc != 2) {
	usage(argv[0]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "--list")) {
	for (const struct edit_case *test = edit_cases; test->name; ++test)
	    bu_log("%s\n", test->name);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[1], "--inventory"))
        return list_operations(0);
    if (BU_STR_EQUAL(argv[1], "--check-inventory"))
        return list_operations(1);

    const struct edit_case *test = find_case(argv[1]);
    if (test)
        return test->run();

    bu_log("Unknown edit test: %s\n", argv[1]);
    usage(argv[0]);
    return BRLCAD_ERROR;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
