/*              T E S T _ O B S E R V E R . C
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


struct observer_notify_capture {
    size_t count;
    char commands[8][128];
};


static void
observer_capture_append(void *data, const char *cmd)
{
    struct observer_notify_capture *capture = (struct observer_notify_capture *)data;

    if (capture->count >= ARRAY_LEN(capture->commands)) {
	return;
    }

    bu_strlcpy(capture->commands[capture->count], cmd, sizeof(capture->commands[capture->count]));
    capture->count++;
}


static int
observer_capture_has(const struct observer_notify_capture *capture, const char *cmd)
{
    size_t i;

    for (i = 0; i < capture->count; i++) {
	if (BU_STR_EQUAL(capture->commands[i], cmd)) {
	    return 1;
	}
    }

    return 0;
}


static int
test_observer_api(void)
{
    int errors = 0;
    struct bu_observer_list observers = BU_OBSERVER_LIST_INIT_ZERO;
    struct observer_notify_capture notify;
    struct test_api_log_capture capture;
    const char *attach_alpha[] = {"attach", "alpha", NULL};
    const char *attach_beta[] = {"attach", "beta", "beta refresh", NULL};
    const char *reattach_beta[] = {"attach", "beta", "beta check", NULL};
    const char *detach_alpha[] = {"detach", "alpha", NULL};
    const char *detach_missing[] = {"detach", "ghost", NULL};
    const char *show_argv[] = {"show", NULL};

    memset(&notify, 0, sizeof(notify));

    test_api_log_capture_begin(&capture);
    TEST_API_CHECK(bu_observer_cmd(&observers, 1, attach_alpha) == BRLCAD_ERROR,
		   "attach requires an observer name");
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "expecting only three arguments"),
		   "attach error path should explain the argument contract");
    test_api_log_capture_free(&capture);

    TEST_API_CHECK(bu_observer_cmd(&observers, 2, attach_alpha) == BRLCAD_OK,
		   "attach without command should succeed");
    TEST_API_CHECK(bu_observer_cmd(&observers, 3, attach_beta) == BRLCAD_OK,
		   "attach with custom command should succeed");
    TEST_API_CHECK(observers.size == 2, "expected two observers after attach, got %zu", observers.size);

    test_api_log_capture_begin(&capture);
    TEST_API_CHECK(bu_observer_cmd(&observers, 1, show_argv) == BRLCAD_OK, "show should succeed");
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "alpha"),
		   "show output should include observer names");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "beta refresh"),
		   "show output should include custom commands");
    test_api_log_capture_free(&capture);

    bu_observer_notify(&notify, &observers, (char *)"subject", observer_capture_append);
    TEST_API_CHECK(notify.count == 2, "expected two notify callbacks, got %zu", notify.count);
    TEST_API_CHECK(observer_capture_has(&notify, "alpha update subject"),
		   "default observer notification should synthesize an update command");
    TEST_API_CHECK(observer_capture_has(&notify, "beta refresh"),
		   "custom observer command should be delivered verbatim");

    memset(&notify, 0, sizeof(notify));
    TEST_API_CHECK(bu_observer_cmd(&observers, 3, reattach_beta) == BRLCAD_OK,
		   "reattach should update existing observer");
    bu_observer_notify(&notify, &observers, (char *)"subject", observer_capture_append);
    TEST_API_CHECK(observer_capture_has(&notify, "beta check"),
		   "reattach should replace existing custom command");

    TEST_API_CHECK(bu_observer_cmd(&observers, 2, detach_alpha) == BRLCAD_OK, "detach should succeed");
    TEST_API_CHECK(observers.size == 1, "expected one observer after detach, got %zu", observers.size);
    test_api_log_capture_begin(&capture);
    TEST_API_CHECK(bu_observer_cmd(&observers, 2, detach_missing) == BRLCAD_ERROR,
		   "detaching a missing observer should fail");
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "ghost"),
		   "missing observer detach should identify the unknown name");
    test_api_log_capture_free(&capture);

    memset(&notify, 0, sizeof(notify));
    bu_observer_notify(&notify, &observers, (char *)"subject", observer_capture_append);
    TEST_API_CHECK(notify.count == 1, "only remaining observer should be notified");
    TEST_API_CHECK(observer_capture_has(&notify, "beta check"),
		   "remaining observer should still receive notifications");

    bu_observer_free(&observers);

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    return test_observer_api();
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
