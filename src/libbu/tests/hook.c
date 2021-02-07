/*                       H O O K . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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

#include "bu.h"

#define NUM_TEST_HOOKS 1

static int was_called[NUM_TEST_HOOKS];

static int
test_bu_hook_basic_hook(void *cdata, void *UNUSED(buf))
{
    int *which_hook = (int *)cdata;

    was_called[*which_hook] = 1;

    return 0;
}


static int
test_bu_hook_basic(void)
{
    struct bu_hook_list hl;
    int which_hook = 0;

    bu_hook_list_init(&hl);

    /**
     * Add a NULL hook
     */
    bu_hook_add(&hl, NULL, NULL);

    if (hl.capacity < 1 || hl.size != 1) {
	printf("\nbu_hook_basic FAILED");
	return BRLCAD_ERROR;
    }

    /**
     * Add a hook and test that it was properly called.
     */
    bu_hook_add(&hl, test_bu_hook_basic_hook, &which_hook);

    if (hl.capacity < 2 || hl.size != 2 || hl.hooks == NULL) {
	printf("\nbu_hook_basic FAILED");
	return BRLCAD_ERROR;
    }

    bu_hook_call(&hl, NULL);

    if (!was_called[which_hook]) {
	printf("\nbu_hook_basic FAILED");
	return BRLCAD_ERROR;
    }

    /**
     * Delete the previous hook and test that it's not called this time.
     */
    was_called[which_hook] = 0;

    bu_hook_delete(&hl, test_bu_hook_basic_hook, &which_hook);

    bu_hook_call(&hl, NULL);

    if (was_called[which_hook]) {
	printf("\nbu_hook_basic FAILED");
	return BRLCAD_ERROR;
    }

    printf("\nbu_hook_basic PASSED");
    return BRLCAD_OK;
}


static int
test_bu_hook_multiadd(void)
{
    struct bu_hook_list hl;
    int which_hook[NUM_TEST_HOOKS];
    size_t i;

    bu_hook_list_init(&hl);

    for (i = 0; i < NUM_TEST_HOOKS; i++) {
	which_hook[i] = i;
	was_called[i] = 0;
	bu_hook_add(&hl, test_bu_hook_basic_hook, &which_hook[i]);
    }

    bu_hook_call(&hl, NULL);

    for (i = 0; i < NUM_TEST_HOOKS; i++) {
	if (!was_called[which_hook[i]]) {
	    printf("\nbu_hook_multiadd FAILED");
	    return BRLCAD_ERROR;
	}
    }

    bu_hook_delete_all(&hl);

    for (i = 0; i < NUM_TEST_HOOKS; i++) {
	was_called[i] = 0;
    }

    bu_hook_call(&hl, NULL);

    for (i = 0; i < NUM_TEST_HOOKS; i++) {
	if (was_called[which_hook[i]]) {
	    printf("\nbu_hook_multiadd FAILED");
	    return BRLCAD_ERROR;
	}
    }

    printf("\nbu_hook_multiadd PASSED");
    return BRLCAD_OK;
}


static int
test_bu_hook_saverestore(void)
{
    struct bu_hook_list from_hl, to_hl;
    int which_hook[NUM_TEST_HOOKS];
    size_t i;

    bu_hook_list_init(&from_hl);
    bu_hook_list_init(&to_hl);

    for (i = 0; i < NUM_TEST_HOOKS; i++) {
	which_hook[i] = i;
	was_called[i] = 0;
	bu_hook_add(&from_hl, test_bu_hook_basic_hook, &which_hook[i]);
    }

    bu_hook_save_all(&from_hl, &to_hl);
    bu_hook_delete_all(&from_hl);

    bu_hook_call(&from_hl, NULL);

    for (i = 0; i < NUM_TEST_HOOKS; i++) {
	if (was_called[which_hook[i]]) {
	    printf("\nbu_hook_saverestore FAILED");
	    return BRLCAD_ERROR;
	}
    }

    bu_hook_restore_all(&from_hl, &to_hl);
    bu_hook_call(&from_hl, NULL);

    for (i = 0; i < NUM_TEST_HOOKS; i++) {
	if (!was_called[which_hook[i]]) {
	    printf("\nbu_hook_saverestore FAILED");
	    return BRLCAD_ERROR;
	}
    }

    printf("\nbu_hook_saverestore PASSED");
    return BRLCAD_OK;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_exit(1, "Usage: %s {basic|multiadd|saverestore} [args...]\n", argv[0]);
    }

    if (BU_STR_EQUAL(argv[1], "basic")) {
	return test_bu_hook_basic();
    } else if (BU_STR_EQUAL(argv[1], "multiadd")) {
	return test_bu_hook_multiadd();
    } else if (BU_STR_EQUAL(argv[1], "saverestore")) {
	return test_bu_hook_saverestore();
    }

    bu_log("\nERROR: Unknown hook test '%s'", argv[1]);
    return BRLCAD_ERROR;
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
