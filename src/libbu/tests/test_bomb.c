/*                      T E S T _ B O M B . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

#include "bu/exit.h"
#include "bu/file.h"
#include "bu/parallel.h"

#include "test_api.h"


struct test_bomb_hook_state {
    int called;
    struct bu_vls message;
};


static int
test_bomb_capture_hook(void *data, void *msg)
{
    struct test_bomb_hook_state *state = (struct test_bomb_hook_state *)data;

    if (!state) {
	return 0;
    }

    state->called++;
    bu_vls_trunc(&state->message, 0);
    if (msg) {
	bu_vls_strcat(&state->message, (const char *)msg);
    }

    return 0;
}


static int
test_bomb_trigger(const char *message)
{
    if (!BU_SETJUMP) {
	bu_bomb(message);
	return 0;
    }

    BU_UNSETJUMP;
    return 1;
}


static int
test_bomb_hook_lifecycle(void)
{
    int errors = 0;
    struct bu_hook_list original_hooks = BU_HOOK_LIST_INIT_ZERO;
    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
    struct test_bomb_hook_state hook_state;
    const char *messages[] = {
	"expected bomb hook invocation",
	"expected bomb with deleted hooks",
	"expected bomb hook after restore"
    };

    hook_state.called = 0;
    bu_vls_init(&hook_state.message);

    bu_bomb_save_all_hooks(&original_hooks);
    bu_bomb_delete_all_hooks();
    bu_bomb_add_hook(test_bomb_capture_hook, &hook_state);

    TEST_API_CHECK(test_bomb_trigger(messages[0]),
		   "bu_bomb should longjmp back to the caller when BU_SETJUMP is active");
    TEST_API_CHECK(hook_state.called == 1,
		   "expected exactly one bu_bomb hook invocation, got %d", hook_state.called);
    TEST_API_CHECK(BU_STR_EQUAL(bu_vls_addr(&hook_state.message), messages[0]),
		   "bu_bomb hook received '%s', expected '%s'",
		   bu_vls_addr(&hook_state.message), messages[0]);

    bu_bomb_save_all_hooks(&saved_hooks);
    bu_bomb_delete_all_hooks();
    hook_state.called = 0;
    bu_vls_trunc(&hook_state.message, 0);

    TEST_API_CHECK(test_bomb_trigger(messages[1]),
		   "bu_bomb should still longjmp back after deleting all hooks");
    TEST_API_CHECK(hook_state.called == 0,
		   "bu_bomb hooks should not fire after bu_bomb_delete_all_hooks()");
    TEST_API_CHECK(bu_vls_strlen(&hook_state.message) == 0,
		   "deleted bu_bomb hooks should not capture messages");

    bu_bomb_restore_hooks(&saved_hooks);
    hook_state.called = 0;
    bu_vls_trunc(&hook_state.message, 0);

    TEST_API_CHECK(test_bomb_trigger(messages[2]),
		   "bu_bomb should longjmp back after hook restoration");
    TEST_API_CHECK(hook_state.called == 1,
		   "expected restored bu_bomb hook to fire once, got %d", hook_state.called);
    TEST_API_CHECK(BU_STR_EQUAL(bu_vls_addr(&hook_state.message), messages[2]),
		   "restored bu_bomb hook received '%s', expected '%s'",
		   bu_vls_addr(&hook_state.message), messages[2]);

    bu_bomb_delete_all_hooks();
    bu_bomb_restore_hooks(&original_hooks);
    bu_hook_delete_all(&saved_hooks);
    bu_hook_delete_all(&original_hooks);
    bu_vls_free(&hook_state.message);

    return errors;
}


static int
test_bomb_capture_child(const char *self_path)
{
    int errors = 0;
    const char *message = "expected direct bomb";
    struct bu_vls stderr_text = BU_VLS_INIT_ZERO;

#ifdef HAVE_UNISTD_H
    if (self_path && self_path[0] != '\0') {
	const char *child_argv[4] = {self_path, "test_bomb", "bomb-child", NULL};
	char buffer[512] = {0};
	int pipe_fds[2] = {-1, -1};
	pid_t cpid;
	ssize_t got = 0;
	int status = 0;
	struct bu_vls trace_file = BU_VLS_INIT_ZERO;

	TEST_API_CHECK(pipe(pipe_fds) == 0, "pipe() failed for bu_bomb child capture");
	if (errors) {
	    bu_vls_free(&stderr_text);
	    return errors;
	}

	cpid = fork();
	TEST_API_CHECK(cpid != (pid_t)-1, "fork() failed for bu_bomb child capture");
	if (cpid == (pid_t)-1) {
	    close(pipe_fds[0]);
	    close(pipe_fds[1]);
	    bu_vls_free(&stderr_text);
	    return errors;
	}

	if (cpid == 0) {
	    close(pipe_fds[0]);
	    (void)dup2(pipe_fds[1], STDERR_FILENO);
	    close(pipe_fds[1]);
	    execvp(self_path, (char * const *)child_argv);
	    perror("execvp");
	    _exit(127);
	}

	close(pipe_fds[1]);
	while ((got = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
	    bu_vls_strncat(&stderr_text, buffer, (size_t)got);
	}
	close(pipe_fds[0]);

	TEST_API_CHECK(got >= 0, "read() failed while capturing bu_bomb stderr");
	TEST_API_CHECK(waitpid(cpid, &status, 0) == cpid, "waitpid() failed for bu_bomb child");
	if (WIFEXITED(status)) {
	    TEST_API_CHECK(WEXITSTATUS(status) == 12,
			   "bu_bomb child exited with %d, expected 12",
			   WEXITSTATUS(status));
	} else {
	    TEST_API_CHECK(0, "bu_bomb child did not exit normally");
	}

	TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&stderr_text), message),
		       "bu_bomb stderr did not include the bomb message: %s",
		       bu_vls_addr(&stderr_text));

	/* bu_bomb writes a debug crash report for this intentional child bomb. */
	bu_vls_sprintf(&trace_file, "%s-%ld-bomb.log", bu_getprogname(), (long)cpid);
	if (bu_file_exists(bu_vls_addr(&trace_file), NULL)) {
	    TEST_API_CHECK(bu_file_delete(bu_vls_addr(&trace_file)),
			   "could not remove expected bomb report %s",
			   bu_vls_addr(&trace_file));
	}
	bu_vls_free(&trace_file);
    }
#else
    (void)self_path;
#endif

    bu_vls_free(&stderr_text);

    return errors;
}


int
main(int argc, char *argv[])
{
    int errors = 0;
    const char *self_path = NULL;

    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], "bomb-child")) {
	bu_bomb("expected direct bomb");
	return 0;
    }

    self_path = (argc > 1 && argv[1][0] != '\0') ? argv[1] : NULL;

    errors += test_bomb_hook_lifecycle();
    errors += test_bomb_capture_child(self_path);

    return errors ? 1 : 0;
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
