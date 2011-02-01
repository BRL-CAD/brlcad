/*                     B A C K T R A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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

/* system headers */
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_TIMES_H
#  include <sys/times.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_PROCESS_H
#  include <process.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#include "bio.h"

/* common headers */
#include "bu.h"


/* c99 doesn't declare these */
#ifdef HAVE_KILL
extern int kill(pid_t, int);
#endif

#ifndef fileno
extern int fileno(FILE*);
#endif


/* so we don't have to worry as much about stack stomping */
#define BT_BUFSIZE 4096
static char buffer[BT_BUFSIZE] = {0};

static pid_t pid = (pid_t)0;
static int backtrace_done = 0;
static int interrupt_wait = 0;

/* avoid stack variables for backtrace() */
static int input[2] = {0, 0};
static int output[2] = {0, 0};
static fd_set fdset;
static fd_set readset;
static struct timeval tv;
static int result;
static int position;
static int processing_bt;
static char c = 0;
static int warned;

/* avoid stack variables for bu_backtrace() */
static char *debugger_args[4] = { NULL, NULL, NULL, NULL };
static const char *locate_gdb = NULL;


/* SIGCHLD handler for backtrace() */
HIDDEN void
backtrace_sigchld(int signum)
{
    if (LIKELY(signum)) {
	backtrace_done = 1;
	interrupt_wait = 1;
    }
}


/* SIGINT handler for bu_backtrace() */
HIDDEN void
backtrace_sigint(int signum)
{
    if (LIKELY(signum)) {
	interrupt_wait = 1;
    }
}


/* actual guts to bu_backtrace() used to invoke gdb and parse out the
 * backtrace from gdb's output.
 */
HIDDEN void
backtrace(char * const *args, int fd)
{
    /* receiving a SIGCHLD signal indicates something happened to a
     * child process, which should be this backtrace since it is
     * invoked after a fork() call as the child.
     */
#ifdef SIGCHLD
    signal(SIGCHLD, backtrace_sigchld);
#endif
#ifdef SIGINT
    signal(SIGINT, backtrace_sigint);
#endif

    if (UNLIKELY((pipe(input) == -1) || (pipe(output) == -1))) {
	perror("unable to open pipe");
	fflush(stderr);
	/* can't call bu_bomb()/bu_exit(), recursive */
	return;
    }

    pid = fork();
    if (pid == 0) {
	int ret;

	close(0);
	ret = dup(input[0]); /* set the stdin to the in pipe */
	if (ret == -1)
	    perror("dup");

	close(1);
	ret = dup(output[1]); /* set the stdout to the out pipe */
	if (ret == -1)
	    perror("dup");

	close(2);
	ret = dup(output[1]); /* set the stderr to the out pipe */
	if (ret == -1)
	    perror("dup");

	execvp(args[0], args); /* invoke debugger */
	perror("exec failed");
	fflush(stderr);
	/* can't call bu_bomb()/bu_exit(), recursive */
	exit(1);
    } else if (pid == (pid_t) -1) {
	perror("unable to fork");
	fflush(stderr);
	/* can't call bu_bomb()/bu_exit(), recursive */
	exit(1);
    }

    FD_ZERO(&fdset);
    FD_SET(output[0], &fdset);

    if (write(input[1], "set prompt\n", 12) != 12) {
	perror("write [set prompt] failed");
    } else if (write(input[1], "set confirm off\n", 16) != 16) {
	perror("write [set confirm off] failed");
    } else if (write(input[1], "set backtrace past-main on\n", 27) != 27) {
	perror("write [set backtrace past-main on] failed");
    } else if (write(input[1], "bt full\n", 8) != 8) {
	perror("write [bt full] failed");
    }
    /* can add additional gdb commands here.  output will contain
     * everything up to the "Detaching from process" statement from
     * quit.
     */
    if (write(input[1], "quit\n", 5) != 5) {
	perror("write [quit] failed");
    }

    position = 0;
    processing_bt = 0;
    memset(buffer, 0, BT_BUFSIZE);

    /* get/print the trace */
    warned = 0;
    while (1) {
	readset = fdset;

	tv.tv_sec = 0;
	tv.tv_usec = 42;

	result = select(FD_SETSIZE, &readset, NULL, NULL, &tv);
	if (result == -1) {
	    break;
	}

	if ((result > 0) && (FD_ISSET(output[0], &readset))) {
	    if (read(output[0], &c, 1)) {
		switch (c) {
		    case '\n':
			if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
			    bu_log("BACKTRACE DEBUG: [%s]\n", buffer);
			}
			if (position+1 < BT_BUFSIZE) {
			    buffer[position++] = c;
			    buffer[position] = '\0';
			} else {
			    position++;
			}
			if (strncmp(buffer, "No locals", 9) == 0) {
			    /* skip it */
			} else if (strncmp(buffer, "No symbol table", 15) == 0) {
			    /* skip it */
			} else if (strncmp(buffer, "Detaching", 9) == 0) {
			    /* done processing backtrace output */
			    processing_bt = 0;
			} else if (processing_bt == 1) {
			    if ((size_t)write(fd, buffer, strlen(buffer)) != strlen(buffer)) {
				perror("error writing stack to file");
				break;
			    }
			    if (position > BT_BUFSIZE) {
				if (write(fd, " [TRIMMED]\n", 11) != 11) {
				    perror("error writing trim message to file");
				    break;
				}
			    }
			}
			position = 0;
			continue;
		    case '#':
			/* once we find a # on the beginning of a
			 * line, begin keeping track of the output.
			 * the first #0 backtrace frame (i.e. that for
			 * the bu_backtrace() call) is not included in
			 * the output intentionally (because of the
			 * gdb prompt).
			 */
			if (position == 0) {
			    processing_bt = 1;
			}
			break;
		    default:
			break;
		}
		if (position+1 < BT_BUFSIZE) {
		    buffer[position++] = c;
		    buffer[position] = '\0';
		} else {
		    if (UNLIKELY(!warned && (bu_debug & BU_DEBUG_ATTACH))) {
			bu_log("Warning: debugger output overflow\n");
			warned = 1;
		    }
		    position++;
		}
	    }
	} else if (backtrace_done) {
	    break;
	}
    }

    fflush(stdout);
    fflush(stderr);

    close(input[0]);
    close(input[1]);
    close(output[0]);
    close(output[1]);

    if (UNLIKELY(bu_debug & BU_DEBUG_ATTACH)) {
	bu_log("\nBacktrace complete.\nAttach debugger or interrupt to continue...\n");
    } else {
#  ifdef HAVE_KILL
	/* not attaching, so let the parent continue */
#    ifdef SIGINT
	kill(getppid(), SIGINT);
#    endif
#    ifdef SIGCHLD
	kill(getppid(), SIGCHLD);
#    endif
#  endif
	sleep(2);
    }

    exit(0);
}


int
bu_backtrace(FILE *fp)
{
    if (!fp) {
	fp = stdout;
    }

    /* make sure the debugger exists */
    if ((locate_gdb = bu_which("gdb"))) {
	debugger_args[0] = bu_strdup(locate_gdb);
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("Found gdb in USER path: %s\n", locate_gdb);
	}
    } else if ((locate_gdb = bu_whereis("gdb"))) {
	debugger_args[0] = bu_strdup(locate_gdb);
	if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	    bu_log("Found gdb in SYSTEM path: %s\n", locate_gdb);
	} else {
	    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
		bu_log("gdb was NOT found, no backtrace available\n");
	    }
	    return 0;
	}
    }
    locate_gdb = NULL;

#ifdef SIGINT
    signal(SIGINT, backtrace_sigint);
#endif

    snprintf(buffer, BT_BUFSIZE, "%d", bu_process_id());

    debugger_args[1] = (char*) bu_argv0_full_path();
    debugger_args[2] = buffer;

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("CALL STACK BACKTRACE REQUESTED\n");
	bu_log("Invoking Debugger: %s %s %s\n\n", debugger_args[0], debugger_args[1], debugger_args[2]);
    }

    /* fork so that trace symbols stop _here_ instead of in some libc
     * routine (e.g., in wait(2)).
     */
    pid = fork();
    if (pid == 0) {
	/* child */
	backtrace(debugger_args, fileno(fp));
	bu_free(debugger_args[0], "gdb strdup");
	debugger_args[0] = NULL;
	exit(0);
    } else if (pid == (pid_t) -1) {
	/* failure */
	bu_free(debugger_args[0], "gdb strdup");
	debugger_args[0] = NULL;
	perror("unable to fork for gdb");
	return 0;
    }
    /* parent */
    if (debugger_args[0]) {
	bu_free(debugger_args[0], "gdb strdup");
	debugger_args[0] = NULL;
    }
    fflush(fp);

    /* could probably do something better than this to avoid hanging
     * indefinitely.  keeps the trace clean, though, and allows for a
     * debugger to be attached interactively if needed.
     */
    interrupt_wait = 0;
#ifdef HAVE_KILL
    {
	struct timeval start, end;
	gettimeofday(&start, NULL);
	gettimeofday(&end, NULL);
	while ((interrupt_wait == 0) && (end.tv_sec - start.tv_sec < 60)) {
	    /* do nothing, wait for debugger to attach but don't wait too long */;
	    gettimeofday(&end, NULL);
	    sleep(1);
	}
    }
#else
    /* FIXME: need something better here for win32 */
    sleep(10);
#endif

    if (UNLIKELY(bu_debug & BU_DEBUG_BACKTRACE)) {
	bu_log("\nContinuing.\n");
    }

#ifdef SIGINT
    signal(SIGINT, SIG_DFL);
#endif
#ifdef SIGCHLD
    signal(SIGCHLD, SIG_DFL);
#endif

    fflush(fp);

    return 1;
}


#ifdef TEST_BACKTRACE
int bar(char **argv)
{
    int moo = 5;
    bu_backtrace(NULL);
    return 0;
}


int foo(char **argv)
{
    return bar(argv);
}


int
main(int argc, char *argv[])
{
    if (argc > 1) {
	bu_bomb("this is a test\n");
    } else {
	(void)foo(argv);
    }
    return 0;
}
#endif  /* TEST_BACKTRACE */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
