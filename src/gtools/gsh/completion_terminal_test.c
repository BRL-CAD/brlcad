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
		quiet = 0;
		continue;
	    }
	}
	if (used && (quiet += 50) >= 150)
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

    if (argc != 2)
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
	execl(argv[1], argv[1], (char *)NULL);
	_exit(122);
    }
    close(slave);

    (void)collect_output(master, output, sizeof(output), 5000);
    if (!strstr(output, "g> ")) {
	stop_child(master, child);
	return 7;
    }

    if (!write_bytes(master, "\t", 1)) {
	stop_child(master, child);
	return 8;
    }
    (void)collect_output(master, output, sizeof(output), 5000);

    display_rows = count_text(output, "\r\n\033[2K");
    snprintf(cursor_up, sizeof(cursor_up), "\033[%zuA\r", display_rows);
    if (display_rows < 1 || display_rows > 5 ||
	    (!strstr(output, " (") && !strstr(output, "matches)") &&
	     !strstr(output, "more)")) ||
	    !strstr(output, cursor_up) ||
	    strstr(output, "\033[s") || strstr(output, "\033[u")) {
	fprintf(stderr, "initial completion display was not cursor-stable: [%s]\n", output);
	stop_child(master, child);
	return 9;
    }

    if (!write_bytes(master, "\t\t\t\t\t", 5)) {
	stop_child(master, child);
	return 10;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    if (strstr(output, "\r\n") || strstr(output, "matches)") || strstr(output, "more)")) {
	fprintf(stderr, "repeated Tab redrew or scrolled the candidate list: [%s]\n", output);
	stop_child(master, child);
	return 11;
    }

    if (!write_bytes(master, "q", 1)) {
	stop_child(master, child);
	return 12;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    if (!strstr(output, "\033[1B\r\033[2K") || !strstr(output, cursor_up) ||
	    !strstr(output, "\rg> ")) {
	fprintf(stderr, "candidate display was not erased before editing: [%s]\n", output);
	stop_child(master, child);
	return 13;
    }

    stop_child(master, child);
    return 0;
}
