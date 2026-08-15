/*              C O M P L E T I O N _ T E R M I N A L _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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

    while (elapsed < timeout_ms && used + 1 < capacity) {
	struct pollfd descriptor = {fd, POLLIN, 0};
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
	if (used && (quiet += 50) >= 200)
	    break;
    }
    buffer[used] = '\0';
    return used;
}


static size_t
count_sequence(const char *text, const char *sequence)
{
    size_t count = 0;
    size_t length = strlen(sequence);
    while ((text = strstr(text, sequence)) != NULL) {
	count++;
	text += length;
    }
    return count;
}


static void
stop_child(int master, pid_t child)
{
    int status = 0;
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
    char output[32768];
    struct winsize size;
    const char *mode = (argc == 3) ? argv[2] : "filter";

    bu_setprogname(argv[0]);

    if (argc != 2 && argc != 3)
	return 1;

    master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0)
	return 2;
    slave_name = ptsname(master);
    if (!slave_name || (slave = open(slave_name, O_RDWR | O_NOCTTY)) < 0)
	return 3;

    memset(&size, 0, sizeof(size));
    size.ws_row = 24;
    size.ws_col = 80;
    if (ioctl(slave, TIOCSWINSZ, &size) != 0)
	return 4;

    child = fork();
    if (child < 0)
	return 5;
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
	(void)setenv("BRLCAD_MGED_COMPLETION_MODE", mode, 1);
	execl(argv[1], argv[1], "-c", "--attach", "nu", "--no-rc", (char *)NULL);
	_exit(122);
    }
    close(slave);

    (void)collect_output(master, output, sizeof(output), 10000);
    if (!strstr(output, "mged> ")) {
	fprintf(stderr, "MGED prompt not found: [%s]\n", output);
	stop_child(master, child);
	return 6;
    }

    if (BU_STR_EQUAL(mode, "off")) {
	(void)write_bytes(master, "\t", 1);
	(void)collect_output(master, output, sizeof(output), 2000);
	if (strstr(output, "\r\n\033[2K")) {
	    fprintf(stderr, "disabled completion displayed candidates: [%s]\n", output);
	    stop_child(master, child);
	    return 7;
	}
	stop_child(master, child);
	return 0;
    }

    if (BU_STR_EQUAL(mode, "prefix")) {
	(void)write_bytes(master, "zo\t", 3);
	(void)collect_output(master, output, sizeof(output), 3000);
	if (!strstr(output, "mged> zoom")) {
	    fprintf(stderr, "prefix completion did not commit zoom: [%s]\n", output);
	    stop_child(master, child);
	    return 8;
	}
	stop_child(master, child);
	return 0;
    }

    (void)write_bytes(master, "\t", 1);
    (void)collect_output(master, output, sizeof(output), 5000);
    if (!strstr(output, "\r\n\033[2K")) {
	fprintf(stderr, "candidate panel not displayed in the terminal: [%s]\n", output);
	stop_child(master, child);
	return 9;
    }
    size_t candidate_rows = count_sequence(output, "\r\n\033[2K");
    if (candidate_rows <= 5 || candidate_rows > 15) {
	fprintf(stderr, "terminal did not use its dynamic 15-row candidate budget (%zu rows): [%s]\n",
		candidate_rows, output);
	stop_child(master, child);
	return 10;
    }

    memset(&size, 0, sizeof(size));
    size.ws_row = 12;
    size.ws_col = 52;
    if (ioctl(master, TIOCSWINSZ, &size) != 0) {
	stop_child(master, child);
	return 11;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    candidate_rows = count_sequence(output, "\r\n\033[2K");
    if (!strstr(output, "\033[J") || candidate_rows < 1 || candidate_rows > 7) {
	fprintf(stderr, "terminal resize did not repaint candidates: [%s]\n", output);
	stop_child(master, child);
	return 12;
    }

    (void)write_bytes(master, "\033[Z", 3);
    (void)collect_output(master, output, sizeof(output), 3000);
    if (strstr(output, "\r\n\033[2K") || strstr(output, "matches)") ||
	strstr(output, "more)")) {
	fprintf(stderr, "Shift-Tab redrew the candidate panel: [%s]\n", output);
	stop_child(master, child);
	return 13;
    }

    (void)write_bytes(master, "\033", 1);
    (void)collect_output(master, output, sizeof(output), 3000);
    if (!strstr(output, "\033[J") || !strstr(output, "mged> ")) {
	fprintf(stderr, "Escape did not restore the completion seed: [%s]\n", output);
	stop_child(master, child);
	return 14;
    }

    /* Exercise erase after the terminal reflows a formerly single-row input.
     * At eight columns "mged> draw --" occupies multiple rows, so using the
     * old width would omit the required cursor-up sequence. */
    memset(&size, 0, sizeof(size));
    size.ws_row = 12;
    size.ws_col = 20;
    if (ioctl(master, TIOCSWINSZ, &size) != 0 ||
	    !write_bytes(master, "draw --\t", 8)) {
	stop_child(master, child);
	return 17;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    memset(&size, 0, sizeof(size));
    size.ws_row = 12;
    size.ws_col = 8;
    if (ioctl(master, TIOCSWINSZ, &size) != 0) {
	stop_child(master, child);
	return 18;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    if (!strstr(output, "\033[J") || !strstr(output, "\033[1A")) {
	fprintf(stderr, "wrapped input resize used stale erase geometry: [%s]\n", output);
	stop_child(master, child);
	return 19;
    }
    memset(&size, 0, sizeof(size));
    size.ws_row = 12;
    size.ws_col = 20;
    if (ioctl(master, TIOCSWINSZ, &size) != 0) {
	stop_child(master, child);
	return 20;
    }
    (void)collect_output(master, output, sizeof(output), 3000);
    if (!strstr(output, "\033[J")) {
	fprintf(stderr, "widened input completion display was not repainted: [%s]\n", output);
	stop_child(master, child);
	return 21;
    }
    (void)write_bytes(master, "\033\025", 2);
    (void)collect_output(master, output, sizeof(output), 2000);
    memset(&size, 0, sizeof(size));
    size.ws_row = 12;
    size.ws_col = 52;
    (void)ioctl(master, TIOCSWINSZ, &size);

    (void)write_bytes(master, "\t", 1);
    (void)collect_output(master, output, sizeof(output), 3000);
    (void)write_bytes(master, "\t\t\t\t", 4);
    (void)collect_output(master, output, sizeof(output), 3000);
    if (strstr(output, "\r\n\033[2K") || strstr(output, "matches)") ||
	strstr(output, "more)")) {
	fprintf(stderr, "repeated Tab redrew the candidate panel: [%s]\n", output);
	stop_child(master, child);
	return 15;
    }

    (void)write_bytes(master, "q", 1);
    (void)collect_output(master, output, sizeof(output), 3000);
    int edit_ok = BU_STR_EQUAL(mode, "cycle") ?
	(strstr(output, "mged> q") == NULL && strchr(output, 'q') != NULL) :
	(strstr(output, "mged> \r\033[6Cq") != NULL);
    if (!strstr(output, "\033[J") || !edit_ok) {
	fprintf(stderr, "candidate panel was not erased before editing: [%s]\n", output);
	stop_child(master, child);
	return 16;
    }

    stop_child(master, child);
    return 0;
}
