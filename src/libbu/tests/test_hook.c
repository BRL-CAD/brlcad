/*                     T E S T _ H O O K . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include "bu.h"


#define NUM_TEST_HOOKS 1


static int was_called[NUM_TEST_HOOKS] = {0};


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
    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
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
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
