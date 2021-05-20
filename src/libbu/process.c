/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h> /* exit */
#include <sys/types.h>
#include "bio.h"
#include "bu/file.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "./process.h"

#if !defined(HAVE_DECL_WAIT) && !defined(wait) && !defined(_WINSOCKAPI_)
extern pid_t wait(int *);
#endif


int
bu_process_id()
{
#ifdef HAVE_UNISTD_H
    return getpid();
#else
    return (int)GetCurrentProcessId();
#endif
}


struct bu_process {
    struct bu_list l;
    const char *cmd;
    int argc;
    const char **argv;
    FILE *fp_in;
    FILE *fp_out;
    FILE *fp_err;
    int fd_in;
    int fd_out;
    int fd_err;
#if defined(_WIN32) && !defined(__CYGWIN__)
    HANDLE hProcess;
    DWORD pid;
#else
    int pid;
#endif
    int aborted;
};


void
bu_process_close(struct bu_process *pinfo, bu_process_io_t d)
{
    if (!pinfo)
	return;

    if (d == BU_PROCESS_STDIN) {
	if (!pinfo->fp_in)
	    return;
	(void)fclose(pinfo->fp_in);
	pinfo->fp_in = NULL;
	return;
    }
    if (d == BU_PROCESS_STDOUT) {
	if (!pinfo->fp_out)
	    return;
	(void)fclose(pinfo->fp_out);
	pinfo->fp_out = NULL;
	return;
    }
    if (d == BU_PROCESS_STDERR) {
	if (!pinfo->fp_err)
	    return;
	(void)fclose(pinfo->fp_err);
	pinfo->fp_err = NULL;
	return;
    }
}


FILE *
bu_process_open(struct bu_process *pinfo, bu_process_io_t d)
{
    if (!pinfo)
	return NULL;

    bu_process_close(pinfo, d);

    if (d == BU_PROCESS_STDIN) {
	pinfo->fp_in = fdopen(pinfo->fd_in, "wb");
	return pinfo->fp_in;
    }
    if (d == BU_PROCESS_STDOUT) {
	pinfo->fp_out = fdopen(pinfo->fd_out, "rb");
	return pinfo->fp_out;
    }
    if (d == BU_PROCESS_STDERR) {
	pinfo->fp_err = fdopen(pinfo->fd_err, "rb");
	return pinfo->fp_err;
    }

    return NULL;
}


int
bu_process_fileno(struct bu_process *pinfo, bu_process_io_t d)
{
    if (!pinfo)
	return -1;

    if (d == BU_PROCESS_STDIN)
	return pinfo->fd_in;
    if (d == BU_PROCESS_STDOUT)
	return pinfo->fd_out;
    if (d == BU_PROCESS_STDERR)
	return pinfo->fd_err;

    return -1;
}


int
bu_process_pid(struct bu_process *pinfo)
{
    if (!pinfo)
	return -1;
    return (int)pinfo->pid;
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
bu_process_read(char *buff, int *count, struct bu_process *pinfo, bu_process_io_t d, int n)
{
    int ret = 1;
    if (!pinfo || !buff || !n || !count)
	return -1;

    if (d == BU_PROCESS_STDOUT) {
	(*count) = read((int)pinfo->fd_out, buff, n);
	if ((*count) <= 0) {
	    ret = -1;
	}
    }
    if (d == BU_PROCESS_STDERR) {
	(*count) = read((int)pinfo->fd_err, buff, n);
	if ((*count) <= 0) {
	    ret = -1;
	}
    }

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
bu_process_exec(
    struct bu_process **p, const char *cmd, int argc, const char **argv, int out_eql_err,
#ifdef _WIN32
    int hide_window
#else
    int UNUSED(hide_window)
#endif
    )
{
    int pret = 0;
    int ac = argc;
#ifdef HAVE_UNISTD_H
    int pid;
    int pipe_in[2];
    int pipe_out[2];
    int pipe_err[2];
    const char **av = NULL;

    if (!p || !cmd)
	return;

    BU_GET(*p, struct bu_process);
    (*p)->fp_in = NULL;
    (*p)->fp_out = NULL;
    (*p)->fp_err = NULL;
    (*p)->fd_in = -1;
    (*p)->fd_out = -1;
    (*p)->fd_err = -1;

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

    pret = pipe(pipe_in);
    if (pret < 0) {
	perror("pipe");
    }

    pret = pipe(pipe_out);
    if (pret < 0) {
	perror("pipe");
    }

    pret = pipe(pipe_err);
    if (pret < 0) {
	perror("pipe");
    }

    /* fork + exec */
    if ((pid = fork()) == 0) {
	/* make this a process group leader */
	setpgid(0, 0);

	/* Redirect stdin and stderr */
	(void)close(BU_PROCESS_STDIN);
	pret = dup(pipe_in[0]);
	if (pret < 0) {
	    perror("dup");
	}
	(void)close(BU_PROCESS_STDOUT);
	pret = dup(pipe_out[1]);
	if (pret < 0) {
	    perror("dup");
	}
	(void)close(BU_PROCESS_STDERR);
	pret = dup(pipe_err[1]);
	if (pret < 0) {
	    perror("dup");
	}

	/* close pipes */
	(void)close(pipe_in[0]);
	(void)close(pipe_in[1]);
	(void)close(pipe_out[0]);
	(void)close(pipe_out[1]);
	(void)close(pipe_err[0]);
	(void)close(pipe_err[1]);

	// TODO - should we be doing this for more than 20? See
	// https://docs.fedoraproject.org/en-US/Fedora_Security_Team/1/html/Defensive_Coding/sect-Defensive_Coding-Tasks-Descriptors-Child_Processes.html
	for (int i = 3; i < 20; i++) {
	    (void)close(i);
	}

	(void)execvp(cmd, (char * const*)av);
	perror(cmd);
	exit(16);
    }

    (void)close(pipe_in[0]);
    (void)close(pipe_out[1]);
    (void)close(pipe_err[1]);

    /* Save necessary information for parental process manipulation */
    (*p)->fd_in = pipe_in[1];
    if (out_eql_err) {
	(*p)->fd_out = pipe_err[0];
    } else {
	(*p)->fd_out = pipe_out[0];
    }
    (*p)->fd_err = pipe_err[0];
    (*p)->pid = pid;

#else
    struct bu_vls cp_cmd = BU_VLS_INIT_ZERO;
    HANDLE pipe_in[2], pipe_inDup;
    HANDLE pipe_out[2], pipe_outDup;
    HANDLE pipe_err[2], pipe_errDup;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};

    if (!cmd || !argc || !argv)
	return;

    BU_GET(*p, struct bu_process);
    (*p)->fp_in = NULL;
    (*p)->fp_out = NULL;
    (*p)->fp_err = NULL;
    (*p)->fd_in = -1;
    (*p)->fd_out = -1;
    (*p)->fd_err = -1;

    (*p)->cmd = bu_strdup(cmd);
    (*p)->argc = argc;
    (*p)->argv = (const char **)bu_calloc(argc+1, sizeof(char *), "bu_process argv cpy");
    for (int i = 0; i < argc; i++) {
	(*p)->argv[i] = bu_strdup(argv[i]);
    }
    (*p)->argv[ac] = (char *)NULL;



    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT. */
    CreatePipe(&pipe_out[0], &pipe_out[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_out[0],
		    GetCurrentProcess(),  &pipe_outDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_out[0]);

    /* Create a pipe for the child process's STDERR. */
    CreatePipe(&pipe_err[0], &pipe_err[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_err[0],
		    GetCurrentProcess(),  &pipe_errDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_err[0]);

    /* Create a pipe for the child process's STDIN. */
    CreatePipe(&pipe_in[0], &pipe_in[1], &sa, 0);

    /* Duplicate the write handle to the pipe so it is not inherited. */
    DuplicateHandle(GetCurrentProcess(), pipe_in[1],
		    GetCurrentProcess(), &pipe_inDup,
		    0, FALSE,                  /* not inherited */
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_in[1]);

    si.cb = sizeof(STARTUPINFO);
    si.lpReserved = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop = NULL;
    if (hide_window) {
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
    } else {
	si.dwFlags = STARTF_USESTDHANDLES;
    }
    si.hStdInput   = pipe_in[0];
    if (out_eql_err) {
	si.hStdOutput  = pipe_err[1];
    } else {
	si.hStdOutput  = pipe_out[1];
    }
    si.hStdError   = pipe_err[1];

    /* Create_Process uses a string, not a char array */
    for (int i = 0; i < argc; i++) {
	/* Quote all path names or arguments with spaces for CreateProcess */
	if (strstr(argv[i], " ") || bu_file_exists(argv[i], NULL)) {
	    bu_vls_printf(&cp_cmd, "\"%s\" ", argv[i]);
	} else {
	    bu_vls_printf(&cp_cmd, "%s ", argv[i]);
	}
    }

    CreateProcess(NULL, bu_vls_addr(&cp_cmd), NULL, NULL, TRUE,
		  DETACHED_PROCESS, NULL, NULL,
		  &si, &pi);
    bu_vls_free(&cp_cmd);

    CloseHandle(pipe_in[0]);
    CloseHandle(pipe_out[1]);
    CloseHandle(pipe_err[1]);

    /* Save necessary information for parental process manipulation.
     * Switching from HANDLE to file descriptor so the rest of the code can be
     * consistent - see
     * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/open-osfhandle
     */
    (*p)->fd_in = _open_osfhandle((intptr_t)pipe_inDup, 0);
    if (out_eql_err) {
	(*p)->fd_out = _open_osfhandle((intptr_t)pipe_errDup, 0);
    } else {
	(*p)->fd_out = _open_osfhandle((intptr_t)pipe_outDup, 0);
    }
    (*p)->fd_err = _open_osfhandle((intptr_t)pipe_errDup, 0);
    (*p)->hProcess = pi.hProcess;
    (*p)->pid = pi.dwProcessId;
    (*p)->aborted = 0;

#endif
}

int
bu_process_wait(
    int *aborted, struct bu_process *pinfo,
#ifndef _WIN32
    int UNUSED(wtime)
#else
    int wtime
#endif
    )
{
    int rc = 0;
#ifndef _WIN32
    int rpid;
    int retcode = 0;

    if (!pinfo)
	return -1;

    close(pinfo->fd_in);
    close(pinfo->fd_out);
    close(pinfo->fd_err);

    while ((rpid = wait(&retcode)) != pinfo->pid && rpid != -1) {
    }
    rc = retcode;
    if (rc) {
	pinfo->aborted = 1;
    }
#else
    DWORD retcode = 0;
    if (!pinfo)
	return -1;

    /* wait for the forked process */
    if (wtime > 0) {
	WaitForSingleObject(pinfo->hProcess, wtime);
    } else {
	WaitForSingleObject(pinfo->hProcess, INFINITE);
    }

    GetExitCodeProcess(pinfo->hProcess, &retcode);

    if (GetLastError() == ERROR_PROCESS_ABORTED || retcode == BU_MSVC_ABORT_EXIT) {
	pinfo->aborted = 1;
    }

    /* may be useful to try pr_wait_status() here */
    rc = (int)retcode;
#endif
    if (aborted) {
	(*aborted) = pinfo->aborted;
    }

    /* Clean up */
    bu_process_close(pinfo, BU_PROCESS_STDOUT);
    bu_process_close(pinfo, BU_PROCESS_STDERR);

    /* Free copy of exec args */
    if (pinfo->cmd) {
	bu_free((void *)pinfo->cmd, "pinfo cmd copy");
    }
    if (pinfo->argv) {
	for (int i = 0; i < pinfo->argc; i++) {
	    if (pinfo->argv[i]) {
		bu_free((void *)pinfo->argv[i], "pinfo argv member");
	    }
	}
	bu_free((void *)pinfo->argv, "pinfo argv array");
    }
    BU_PUT(pinfo, struct bu_process);

    return rc;
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
