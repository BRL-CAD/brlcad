/*          C O M P L E T I O N _ T E R M I N A L _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "bu/app.h"
#include "bu/str.h"

static int reported_cursor_row = 23;
static int answer_cursor_queries = 1;

static size_t count_text(const char *input, const char *pattern);

static int
write_bytes(int fd, const char *bytes, size_t count)
{
    size_t written = 0;
    while (written < count) {
	ssize_t ret = write(fd, bytes + written, count - written);
	if (ret < 0 && errno == EINTR)
	    continue;
	if (ret <= 0)
	    return 0;
	written += (size_t)ret;
    }
    return 1;
}


static size_t
collect_output(int fd, char *buffer, size_t capacity, int timeout_ms)
{
    size_t used = 0;
    size_t answered_queries = 0;
    int elapsed = 0;
    int quiet = 0;

    if (!capacity)
	return 0;
    while (elapsed < timeout_ms && used + 1 < capacity) {
	struct pollfd descriptor;
	descriptor.fd = fd;
	descriptor.events = POLLIN;
	descriptor.revents = 0;
	int ret = poll(&descriptor, 1, 50);
	elapsed += 50;
	if (ret < 0 && errno == EINTR)
	    continue;
	if (ret > 0 && (descriptor.revents & POLLIN)) {
	    ssize_t got = read(fd, buffer + used, capacity - used - 1);
	    if (got > 0) {
		used += (size_t)got;
		buffer[used] = '\0';
		size_t queries = count_text(buffer, "\033[6n");
		while (answer_cursor_queries && answered_queries < queries) {
		    char response[32];
		    int length = snprintf(response, sizeof(response), "\033[%d;4R",
			    reported_cursor_row);
		    if (length > 0)
			(void)write_bytes(fd, response, (size_t)length);
		    answered_queries++;
		}
		quiet = 0;
		continue;
	    }
	}
	/* Cursor-position queries are answered by this harness, and the child may
	 * need another scheduling slice before it can render the resulting layout.
	 * A very short quiet window makes this test flaky under parallel CTest
	 * load by returning only the query itself. */
	if (used && (quiet += 50) >= 500)
	    break;
    }
    buffer[used] = '\0';
    return used;
}


static size_t
count_text(const char *input, const char *pattern)
{
    size_t count = 0;
    size_t pattern_length = strlen(pattern);
    const char *found = input;

    while (pattern_length && (found = strstr(found, pattern)) != NULL) {
	count++;
	found += pattern_length;
    }
    return count;
}


static void
stop_child(int master, pid_t child)
{
    int status = 0;
    (void)write_bytes(master, "\003", 1);
    close(master);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
}


int
main(int argc, char **argv)
{
    int master = -1;
    int slave = -1;
    pid_t child = -1;
    char *slave_name = NULL;
    char output[16384];
    char cursor_up[32];
    size_t display_rows = 0;
    struct winsize size;

    bu_setprogname(argv[0]);

    if (argc != 2 && argc != 3)
	return 1;

    master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0)
	return 2;
    slave_name = ptsname(master);
    if (!slave_name)
	return 3;
    slave = open(slave_name, O_RDWR | O_NOCTTY);
    if (slave < 0)
	return 4;

    memset(&size, 0, sizeof(size));
    size.ws_row = 24;
    size.ws_col = 80;
    if (ioctl(slave, TIOCSWINSZ, &size) != 0)
	return 5;

    child = fork();
    if (child < 0)
	return 6;
    if (child == 0) {
	close(master);
	if (setsid() < 0 || ioctl(slave, TIOCSCTTY, 0) != 0)
	    _exit(120);
	if (dup2(slave, STDIN_FILENO) < 0 || dup2(slave, STDOUT_FILENO) < 0 ||
		dup2(slave, STDERR_FILENO) < 0)
	    _exit(121);
	if (slave > STDERR_FILENO)
	    close(slave);
	(void)setenv("TERM", "xterm-256color", 1);
	if (argc == 3 && !BU_STR_EQUAL(argv[2], "nodsr"))
	    execl(argv[1], argv[1], "--completion-mode", argv[2], (char *)NULL);
	else
	    execl(argv[1], argv[1], (char *)NULL);
	_exit(122);
    }
    close(slave);

    (void)collect_output(master, output, sizeof(output), 5000);
    if (!strstr(output, "g> ")) {
	stop_child(master, child);
	return 7;
    }

    if (argc == 3 && BU_STR_EQUAL(argv[2], "off")) {
	if (!write_bytes(master, "\t", 1)) {
	    stop_child(master, child);
	    return 8;
	}
	(void)collect_output(master, output, sizeof(output), 2000);
	if (strstr(output, "matches)") || strstr(output, "more)")) {
	    fprintf(stderr, "disabled completion displayed candidates: [%s]\n", output);
	    stop_child(master, child);
	    return 9;
	}
	stop_child(master, child);
	return 0;
    }

    if (argc == 3 && BU_STR_EQUAL(argv[2], "prefix")) {
	if (!write_bytes(master, "bre\t", 4)) {
	    stop_child(master, child);
	    return 10;
	}
	(void)collect_output(master, output, sizeof(output), 3000);
	if (!strstr(output, "g> brep")) {
	    fprintf(stderr, "prefix completion did not commit brep: [%s]\n", output);
	    stop_child(master, child);
	    return 11;
	}
	stop_child(master, child);
	return 0;
    }

    if (argc == 3 && BU_STR_EQUAL(argv[2], "nodsr")) {
	answer_cursor_queries = 0;
	/* Queue editing input with Tab.  The cursor probe must neither consume it
	 * nor delay every subsequent completion when DSR is unavailable. */
	if (!write_bytes(master, "\tq", 2)) {
	    stop_child(master, child);
	    return 19;
	}
	(void)collect_output(master, output, sizeof(output), 2000);
	size_t cursor_queries = count_text(output, "\033[6n");
	if (cursor_queries > 1 || !strstr(output, "\033[3Cq")) {
	    fprintf(stderr, "queued input was consumed by an unsupported DSR probe (%zu queries): [%s]\n",
		cursor_queries, output);
	    stop_child(master, child);
	    return 20;
	}
	/* Complete once without queued input so an unanswered DSR establishes the
	 * terminal capability, then dismiss the candidates. */
	if (!write_bytes(master, "\025\t", 2)) {
	    stop_child(master, child);
	    return 21;
	}
	(void)collect_output(master, output, sizeof(output), 2000);
	if (count_text(output, "\033[6n") > 1 || !write_bytes(master, "\033", 1)) {
	    fprintf(stderr, "unsupported DSR capability probe was repeated: [%s]\n", output);
	    stop_child(master, child);
	    return 22;
	}
	(void)collect_output(master, output, sizeof(output), 1000);
	/* Once DSR is known to be unavailable, later completions must not repeat
	 * the timeout or consume the ordinary editing byte queued after Tab. */
	if (!write_bytes(master, "\tq", 2)) {
	    stop_child(master, child);
	    return 23;
	}
	(void)collect_output(master, output, sizeof(output), 2000);
	if (count_text(output, "\033[6n") != 0 || !strstr(output, "\033[3Cq")) {
	    fprintf(stderr, "unsupported DSR was queried repeatedly or lost input: [%s]\n", output);
	    stop_child(master, child);
	    return 24;
	}
	stop_child(master, child);
	return 0;
    }

    if (argc == 3) {
	stop_child(master, child);
	return 12;
    }

    reported_cursor_row = 2;
    if (!write_bytes(master, "\t", 1)) {
	stop_child(master, child);
	return 8;
    }
    (void)collect_output(master, output, sizeof(output), 5000);

    display_rows = count_text(output, "\r\n\033[2K");
    snprintf(cursor_up, sizeof(cursor_up), "\033[%zuA\r", display_rows);
    if (display_rows <= 15 || display_rows > 22 ||
	    (!strstr(output, " (") && !strstr(output, "matches)") &&
	     !strstr(output, "more)")) ||
	    !strstr(output, cursor_up) ||
	    strstr(output, "\033[s") || strstr(output, "\033[u")) {
	fprintf(stderr, "initial completion display was not cursor-stable: [%s]\n", output);
	stop_child(master, child);
	return 9;
    }

    memset(&size, 0, sizeof(size));
    size.ws_row = 12;
    size.ws_col = 52;
    reported_cursor_row = 11;
    if (ioctl(master, TIOCSWINSZ, &size) != 0) {
	stop_child(master, child);
	return 10;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    display_rows = count_text(output, "\r\n\033[2K");
    snprintf(cursor_up, sizeof(cursor_up), "\033[%zuA\r", display_rows);
    if (!strstr(output, "\033[J") || display_rows < 1 || display_rows > 7 ||
	    !strstr(output, cursor_up)) {
	fprintf(stderr, "resized completion display was not repainted: [%s]\n", output);
	stop_child(master, child);
	return 11;
    }

    if (!write_bytes(master, "\t\t\t\t\t", 5)) {
	stop_child(master, child);
	return 12;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    if (strstr(output, "\r\n") || strstr(output, "matches)") || strstr(output, "more)")) {
	fprintf(stderr, "repeated Tab redrew or scrolled the candidate list: [%s]\n", output);
	stop_child(master, child);
	return 13;
    }

    /* A bare Escape must dismiss completion promptly.  It is not the start of
     * an ANSI key sequence and must not leave the line editor blocked waiting
     * for two more bytes. */
    if (!write_bytes(master, "\033", 1)) {
	stop_child(master, child);
	return 14;
    }
    (void)collect_output(master, output, sizeof(output), 1000);
    if (!strstr(output, "\033[J") || !strstr(output, "\rg> ")) {
	fprintf(stderr, "bare Escape did not dismiss completion: [%s]\n", output);
	stop_child(master, child);
	return 15;
    }

    if (!write_bytes(master, "\t", 1)) {
	stop_child(master, child);
	return 16;
    }
    (void)collect_output(master, output, sizeof(output), 3000);

    if (!write_bytes(master, "q", 1)) {
	stop_child(master, child);
	return 17;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    if (!strstr(output, "\033[J") || !strstr(output, "\rg> ")) {
	fprintf(stderr, "candidate display was not erased before editing: [%s]\n", output);
	stop_child(master, child);
	return 18;
    }

    stop_child(master, child);
    return 0;
}
