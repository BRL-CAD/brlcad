/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2018 United States Government as represented by
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

#include <sys/types.h>
#include "bio.h"
#include "bu/process.h"

int
bu_process_id()
{
#ifdef HAVE_UNISTD_H
    return getpid();
#else
    return (int)GetCurrentProcessId();
#endif
}

#if 0

struct bu_pipe {
#ifndef _WIN32
    int pipe[2];
#else
    HANDLE p[2];
    HANDLE pipeDup;
    SECURITY_ATTRIBUTES sa = {0};
#endif
};

int
#ifndef _WIN32
bu_pipe_create(struct bu_pipe **, int UNUSED(ninherit))
#else
bu_pipe_create(struct bu_pipe **, int ninherit)
#endif
{
    struct bu_pipe *p = NULL;
    BU_GET(p, struct bu_pipe);
#ifndef _WIN32
    int ret = 0;
    ret = pipe(p->pipe);
    if (ret < 0) {
	perror("failed to create pipe");
	return ret;
    }
#else
    p->sa.nLength = sizeof(sa);
    p->sa.bInheritHandle = TRUE;
    p->sa.lpSecurityDescriptor = NULL;
    CreatePipe(&p->pipe[0], &p->pipe[1], &sa, 0);
    if (ninherit == 0 || ninherit == 1) {
	/* Create noninheritable handle and close the inheritable handle. */
	DuplicateHandle(GetCurrentProcess(), p->pipe[ninherit],
		GetCurrentProcess(),  &pipe_Dup ,
		0,  FALSE,
		DUPLICATE_SAME_ACCESS);
	CloseHandle(p->pipe[ninherit]);
    }
    return 0;
#endif
}

void
bu_pipe_destroy(struct bu_pipe *p)
{
    if (!p) return;
    BU_PUT(p, struct bu_pipe);
}

void
bu_pipe_close(struct bu_pipe *p, int id)
{
    if (!p || (id != 0 && id != 1)) return;
#ifdef _WIN32
    CloseHandle(p->pipe[id]);
#else
    (void)close(p->pipe[id]);
#endif
}

struct bu_process_info {
    struct bu_list l;
#if defined(_WIN32) && !defined(__CYGWIN__)
    HANDLE fd;
    HANDLE hProcess;
    DWORD pid;
    void *chan; /* FIXME: uses communication channel instead of file
		 * descriptor to get output */
#else
    int fd;
    int pid;
#endif
    int aborted;
};

void
bu_exec(const char *cmd, int argc, const char **argv, struct bu_pipe *pin, struct bu_pipe *perr)
{
    if (!cmd || !argc || !argv) {
	return;
    }
#ifdef HAVE_UNISTD_H
    int ac = 0;
    const char **av = NULL;
    av = (const char **)bu_calloc(argc+2, sizeof(char *), "argv array");
    if (!BU_STR_EQUAL(cmd, argv[0])) {
	/* By convention the first argument to execvp should match the
	 * cmd string - if it doesn't we can handle it in av, but it
	 * means the actual exec av array will be longer by one. */
	av[0] = cmd;
	for (int i = 1; i <= argc; i++) {
	    av[i] = argv[i-1];
	}
	av[argc+1] = (char *)NULL;
    } else {
	for (int i = 0; i < argc; i++) {
	    av[i] = argv[i];
	}
	av[argc] = (char *)NULL;
    }
    (void)execvp(cmd, av);
#else
    STARTUPINFO si = {0};
#endif
}

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
