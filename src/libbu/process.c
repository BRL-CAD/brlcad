/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2024 United States Government as represented by
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

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include <stdlib.h> /* exit */
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "bio.h"
#include "bnetwork.h"
#include "bu/debug.h"
#include "bu/file.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "./process.h"
#include "./subprocess.h"

#if !defined(HAVE_DECL_WAIT) && !defined(wait) && !defined(_WINSOCKAPI_)
extern pid_t wait(int *);
#endif


int
bu_process_id(void)
{
#ifdef HAVE_UNISTD_H
    return getpid();
#else
    return (int)GetCurrentProcessId();
#endif
}


struct bu_process {
    struct subprocess_s* subprocess_p;

    const char *cmd;
    int argc;
    const char **argv;
    int aborted;    // TODO: do we still need this?
};


void
bu_process_close(struct bu_process *pinfo, bu_process_io_t stream_type)
{
    if (stream_type == BU_PROCESS_STDIN)
	(void)subprocess_close_stdin(pinfo->subprocess_p);

    /* we're not actually opening anything with bu_process_open */
}


FILE *
bu_process_open(struct bu_process *pinfo, bu_process_io_t stream_type)
{
    if (!pinfo)
	return NULL;

    switch (stream_type)
    {
    case BU_PROCESS_STDIN:
	return subprocess_stdin(pinfo->subprocess_p);
    case BU_PROCESS_STDOUT:
	return subprocess_stdout(pinfo->subprocess_p);
    case BU_PROCESS_STDERR:
	return subprocess_stderr(pinfo->subprocess_p);
    }

    return NULL;
}


int
bu_process_fileno(struct bu_process *pinfo, bu_process_io_t stream_type)
{
    if (!pinfo || !pinfo->subprocess_p)
	return -1;

    switch (stream_type)
    {
    case BU_PROCESS_STDIN:
	return fileno(subprocess_stdin(pinfo->subprocess_p));
    case BU_PROCESS_STDOUT:
	return fileno(subprocess_stdout(pinfo->subprocess_p));
    case BU_PROCESS_STDERR:
	return fileno(subprocess_stderr(pinfo->subprocess_p));
    }

    return -1;
}


int
bu_process_pid(struct bu_process *pinfo)
{
    if (!pinfo) {
	/* if no process specified, return the current processes ID */
	return bu_process_id();
    }

    return pinfo->subprocess_p->pid;
}


int
bu_process_args(const char **cmd, const char * const **argv, struct bu_process *pinfo)
{
    if (!pinfo)
	return 0;

    if (cmd)
	*cmd = pinfo->cmd;
    if (argv)
	*argv = (const char * const *)(pinfo->argv);

    return pinfo->argc;
}


int
bu_process_read(char *buff, int *count, struct bu_process *pinfo, bu_process_io_t stream_type, int n)
{
    int ret = 1;
    if (!pinfo || !buff || !n || !count)
	return -1;

    switch (stream_type)
    {
    case BU_PROCESS_STDOUT:
	*count = subprocess_read_stdout(pinfo->subprocess_p, buff, n);
	break;
    case BU_PROCESS_STDERR:
	*count = subprocess_read_stderr(pinfo->subprocess_p, buff, n);
	break;
    default:	// invalid stream_type
	return -1;
    }

    if ((*count) <= 0)
	ret = -1;

    /* sanity clamping */
    if ((*count) < 0) {
	perror("READ ERROR");
	(*count) = 0;
    } else if ((*count) > n) {
	(*count) = n;
    }

    return ret;
}


void
bu_process_exec_(struct bu_process **p, const char *cmd, int argc, const char **argv, int options)
{
    if (!p || !cmd)
	return;

    BU_GET(*p, struct bu_process);
    BU_GET((*p)->subprocess_p, struct subprocess_s);

    int ac = argc;
    const char **av = NULL;

    av = (const char **)bu_calloc(argc+2, sizeof(char *), "argv array");
    if (!argc || !BU_STR_EQUAL(cmd, argv[0])) {
	/* By convention the first argument to execvp should match the
	 * cmd string - if it doesn't we can handle it in av, but it
	 * means the actual exec av array will be longer by one. */
	av[0] = cmd;
	for (int i = 1; i <= argc; i++) {
	    av[i] = argv[i-1];
	}
	av[argc+1] = (char *)NULL;
	ac++;
    } else {
	for (int i = 0; i < argc; i++) {
	    av[i] = argv[i];
	}
	av[argc] = (char *)NULL;
    }

    /* Make a copy of the final execvp args */
    (*p)->cmd = bu_strdup(cmd);
    (*p)->argc = ac;
    (*p)->argv = (const char **)bu_calloc(ac+1, sizeof(char *), "bu_process argv cpy");
    for (int i = 0; i < ac; i++) {
	(*p)->argv[i] = bu_strdup(av[i]);
    }
    (*p)->argv[ac] = (char *)NULL;

    (*p)->aborted = 0;

    // TODO: return whether process was created successfully?
    subprocess_create(argv, options, (*p)->subprocess_p);
}

void
bu_process_exec(struct bu_process **p, const char *cmd, int argc, const char **argv, int out_eql_err, int hide_window)
{
    /* match out_eql_err and hide_window flags to subprocess options */
    int options = 0;
    if (out_eql_err) options |= subprocess_option_combined_stdout_stderr;
    if (hide_window) options |= subprocess_option_no_window;

    bu_process_exec_(p, cmd, argc, argv, options);
}

int
bu_process_wait(int *aborted, struct bu_process **pinfo, int UNUSED(wtime))
{
    int rc = 0;
    if ((*pinfo)->subprocess_p && subprocess_join((*pinfo)->subprocess_p, &rc)) {
	/* unsuccessful join, forcefully destroy */
	(void)subprocess_destroy((*pinfo)->subprocess_p);
    }

    // NOTE: if we want to maintain an 'aborted' var, we have to do some platform specific gunk
#ifndef _WIN32
    while ((rpid = wait(&retcode)) != pinfo->pid && rpid != -1) {
    }
    rc = retcode;
    if (rc) {
	pinfo->aborted = 1;
    }
#else
    if (GetLastError() == ERROR_PROCESS_ABORTED || rc == BU_MSVC_ABORT_EXIT) {
	(*pinfo)->aborted = 1;
    }
#endif

    if (aborted)  {
	(*aborted) = (*pinfo)->aborted;
    }

    /* Clean up */
    bu_process_close(*pinfo, BU_PROCESS_STDOUT);
    bu_process_close(*pinfo, BU_PROCESS_STDERR);

    /* Free copy of exec args and struct allocs */
    bu_free((void *)(*pinfo)->cmd, "pinfo cmd copy");

    if ((*pinfo)->argv) {
	for (int i = 0; i < (*pinfo)->argc; i++) {
	    bu_free((void *)(*pinfo)->argv[i], "pinfo argv member");
	}
	bu_free((void *)(*pinfo)->argv, "pinfo argv array");
    }
    BU_PUT((*pinfo)->subprocess_p, struct subprocess_t);
    BU_PUT(*pinfo, struct bu_process);

    return rc;
}


#include <time.h>
#ifdef HAVE_POLL_H
#  include <poll.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

int
bu_process_pending(int fd)
{
    int result;

#if defined(_WIN32)
    HANDLE out_fd = (HANDLE)_get_osfhandle(fd);
    DWORD bytesAvailable = 0;
    /* returns 1 on success, 0 on error */
    if (PeekNamedPipe(out_fd, NULL, 0, NULL, &bytesAvailable, NULL)) {
	result = bytesAvailable;
    } else {
	result = -1;
    }
#else
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    /* returns 1 on success, 0 on timeout, -1 on error */
    result = select(fd+1, &read_set, NULL, NULL, 0);
#endif

    /* collapse return to ignore amount to read or errors */
    return result > 0 ? 1 : 0;
}

int
bu_process_alive(struct bu_process *pinfo)
{
    if (pinfo && pinfo->subprocess_p)
	return subprocess_alive(pinfo->subprocess_p);

    return 0;
}

int
bu_interactive(void)
{
    int interactive = 1;

    fd_set read_set;
    fd_set exception_set;
    int result;

    struct timeval timeout;

    /* wait 1/10sec for input, in case we're piped */
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    /* check if there is data on stdin, first relying on whether
     * there is standard input pending, second on whether there's
     * a controlling terminal (isatty).
     */
    FD_ZERO(&read_set);
    FD_SET(fileno(stdin), &read_set);
    result = select(fileno(stdin)+1, &read_set, NULL, NULL, &timeout);
    if (bu_debug > 0) {
	fprintf(stdout, "DEBUG: select result: %d, stdin read: %ld\n", result, (long int)FD_ISSET(fileno(stdin), &read_set));
	if (result < 0) {
	    fprintf(stdout, "DEBUG: select error: %s\n", strerror(errno));
	}
    }

    if (result <= 0) {
	if (!isatty(fileno(stdin))) {
	    interactive = 0;
	}
    } else if (result > 0 && FD_ISSET(fileno(stdin), &read_set)) {
	/* stdin pending, probably not interactive */
	interactive = 0;

	/* check if there's an out-of-bounds exception.  sometimes
	 * the case if mged -c is started via desktop GUI.
	 */
	FD_ZERO(&exception_set);
	FD_SET(fileno(stdin), &exception_set);
	result = select(fileno(stdin)+1, NULL, NULL, &exception_set, &timeout);
	if (bu_debug > 0)
	    fprintf(stdout, "DEBUG: select result: %d, stdin exception: %ld\n", result, (long int)FD_ISSET(fileno(stdin), &exception_set));

	/* see if there's valid input waiting (more reliable than select) */
	if (result > 0 && FD_ISSET(fileno(stdin), &exception_set)) {
#ifdef HAVE_POLL_H
	    struct pollfd pfd;
	    pfd.fd = fileno(stdin);
	    pfd.events = POLLIN;
	    pfd.revents = 0;

	    result = poll(&pfd, 1, 100);
	    if (bu_debug > 0)
		fprintf(stdout, "DEBUG: poll result: %d, revents: %d\n", result, pfd.revents);

	    if (pfd.revents & POLLNVAL) {
		interactive = 1;
	    }
#else
	    /* just in case we get input too quickly, see if it's coming from a tty */
	    if (isatty(fileno(stdin))) {
		interactive = 1;
	    }
#endif /* HAVE_POLL_H */

	}

    } /* read_set */

    return interactive;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
