/*                   T E S T _ C M D . C
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

#include <string.h>

#include "test_api.h"


struct cmd_api_state {
    int calls;
    int argc_seen;
    int data_matches;
    char token_seen[32];
};


static int
cmd_test_run(void *data, int argc, const char **argv)
{
    struct cmd_api_state *state = (struct cmd_api_state *)data;

    state->calls++;
    state->argc_seen = argc;
    state->data_matches = (data == state);
    bu_strlcpy(state->token_seen, (argc > 1) ? argv[1] : argv[0], sizeof(state->token_seen));

    return argc + 7;
}


static int
cmd_test_noop(void *data, int argc, const char **argv)
{
    struct cmd_api_state *state = (struct cmd_api_state *)data;

    state->calls++;
    state->argc_seen = argc;
    state->data_matches = (data == state);
    bu_strlcpy(state->token_seen, argv[0], sizeof(state->token_seen));

    return argc + 3;
}


static const struct bu_cmdtab cmd_api_cmds[] = {
    {"run", cmd_test_run},
    {"noop", cmd_test_noop},
    {(const char *)NULL, BU_CMD_NULL}
};


static int
test_cmd_api(void)
{
    int errors = 0;
    struct cmd_api_state state;
    struct test_api_log_capture capture;
    int retval = -1;
    const char *run_argv[] = {"prefix", "run", "tail", NULL};
    const char *noop_argv[] = {"noop", NULL};
    const char *bad_argv[] = {"prefix", "missing", NULL};

    memset(&state, 0, sizeof(state));

    TEST_API_CHECK(bu_cmd_valid(cmd_api_cmds, "run") == BRLCAD_OK, "known command should validate");
    TEST_API_CHECK(bu_cmd_valid(cmd_api_cmds, "missing") == BRLCAD_ERROR,
		   "unknown command should not validate");
    TEST_API_CHECK(bu_cmd_valid(NULL, "run") == BRLCAD_ERROR,
		   "NULL command table should fail validation");
    TEST_API_CHECK(bu_cmd_valid(cmd_api_cmds, NULL) == BRLCAD_ERROR,
		   "NULL command name should fail validation");

    TEST_API_CHECK(bu_cmd(cmd_api_cmds, 3, run_argv, 1, &state, &retval) == BRLCAD_OK,
		   "known command should dispatch");
    TEST_API_CHECK(state.calls == 1, "expected one callback invocation, got %d", state.calls);
    TEST_API_CHECK(state.argc_seen == 3, "callback should receive original argc");
    TEST_API_CHECK(state.data_matches, "callback should receive original data pointer");
    TEST_API_CHECK(BU_STR_EQUAL(state.token_seen, "run"), "callback should see requested subcommand");
    TEST_API_CHECK(retval == 10, "expected callback return value 10, got %d", retval);

    TEST_API_CHECK(bu_cmd(cmd_api_cmds, 1, noop_argv, 0, &state, NULL) == BRLCAD_OK,
		   "dispatch without retval should succeed");
    TEST_API_CHECK(state.calls == 2, "expected callback count to increase after noop");
    TEST_API_CHECK(BU_STR_EQUAL(state.token_seen, "noop"), "dispatch at index 0 should see argv[0]");

    TEST_API_CHECK(bu_cmd(cmd_api_cmds, 2, bad_argv, 2, &state, &retval) == BRLCAD_ERROR,
		   "cmd_index beyond argc should fail");

    test_api_log_capture_begin(&capture);
    TEST_API_CHECK(bu_cmd(cmd_api_cmds, 2, bad_argv, 1, &state, &retval) == BRLCAD_ERROR,
		   "unknown command should fail");
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "unknown command"),
		   "unknown command log should explain the failure");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "run"),
		   "unknown command log should list available commands");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "noop"),
		   "unknown command log should list all commands");
    test_api_log_capture_free(&capture);

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    return test_cmd_api();
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
