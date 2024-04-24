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
#include <signal.h> /* terminate */
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
#include "bu/time.h"
#include "bu/vls.h"
#include "./process.h"

#ifndef HAVE_KILL
#  include <TlHelp32.h>
#endif

/* c99 doesn't declare these */
#if defined(HAVE_KILL) && !defined(__cplusplus)
extern int kill(pid_t, int);
#endif

#if !defined(HAVE_DECL_WAIT) && !defined(wait) && !defined(_WINSOCKAPI_)
extern pid_t wait(int *);
#endif


int
bu_pid(void)
{
#ifdef HAVE_UNISTD_H
    return getpid();
#else
    return (int)GetCurrentProcessId();
#endif
}

int
bu_process_id(void)
{
    return bu_pid();
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
bu_process_file_close(struct bu_process *pinfo, bu_process_io_t d)
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

void
bu_process_close(struct bu_process *pinfo, bu_process_io_t d)
{
    bu_process_file_close(pinfo, d);
}

FILE *
bu_process_file_open(struct bu_process *pinfo, bu_process_io_t d)
{
    if (!pinfo)
	return NULL;

    bu_process_file_close(pinfo, d);

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

FILE *
bu_process_open(struct bu_process *pinfo, bu_process_io_t d)
{
    return bu_process_file_open(pinfo, d);
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
bu_process_args_n(struct bu_process *pinfo, const char **cmd, const char * const **argv)
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
bu_process_args(const char **cmd, const char * const **argv, struct bu_process *pinfo)
{
    return bu_process_args_n(pinfo, cmd, argv);
}

int
bu_process_read_n(struct bu_process *pinfo, bu_process_io_t d, int n, char *buff)
{
    if (!pinfo || !buff || !n)
	return -1;

    int read_fd = -1;
    if (d == BU_PROCESS_STDOUT)
	read_fd = (int)pinfo->fd_out;
    else if (d == BU_PROCESS_STDERR)
	read_fd = (int)pinfo->fd_err;
    else
	return -1;	// invalid channel specified

    int ret = read(read_fd, buff, n);

    if (ret < 0)
	perror("READ ERROR");

    return ret;
}

int
bu_process_read(char *buff, int *count, struct bu_process *pinfo, bu_process_io_t d, int n)
{
    int read_ret = bu_process_read_n(pinfo, d, n, buff);

    /* sanity clamping */
    if (read_ret < 0) {
	(*count) = 0;
    } else if (read_ret > n) {
	(*count) = n;
    } else {
	(*count) = read_ret;
    }

    // maintain consistent behavior with old read which returned 1 on success and -1 on error
    return (read_ret > 0) ? 1 : -1;
}

int
bu_process_create(struct bu_process **pinfo, const char **argv, bu_process_create_opts opts)
{
    if (!pinfo || !argv)
	return -1;

    int ret = 0;
    /* get argc count */
    int argc = 0;
    while (argv[argc] != NULL)
	argc++;
    /* by convention - first value of argv is the cmd */
    const char* cmd = (*argv);

    /* alloc and zero-out pinfo */
    BU_GET(*pinfo, struct bu_process);
    (*pinfo)->fp_in = NULL;
    (*pinfo)->fp_out = NULL;
    (*pinfo)->fp_err = NULL;
    (*pinfo)->fd_in = -1;
    (*pinfo)->fd_out = -1;
    (*pinfo)->fd_err = -1;

    /* Make a copy of the final execvp args */
    (*pinfo)->cmd = bu_strdup(cmd);
    (*pinfo)->argc = argc;
    (*pinfo)->argv = (const char **)bu_calloc(argc, sizeof(char *), "bu_process argv cpy");
    for (int i = 0; i < argc; i++) {
	(*pinfo)->argv[i] = bu_strdup(argv[i]);
    }
    //(*pinfo)->argv[ac] = (char *)NULL;	// SHOULD ALREADY BE NULL? 

#ifdef HAVE_UNISTD_H
    int pret;
    int pid;
    int pipe_in[2];
    int pipe_out[2];
    int pipe_err[2];

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
	int d1, d2, d3;
	/* make this a process group leader */
	setpgid(0, 0);

	/* Redirect stdin and stderr */
	(void)close(BU_PROCESS_STDIN);
	d1 = dup(pipe_in[0]);
	if (d1 < 0) {
	    perror("dup");
	}
	(void)close(BU_PROCESS_STDOUT);
	d2 = dup(pipe_out[1]);
	if (d2 < 0) {
	    perror("dup");
	}
	(void)close(BU_PROCESS_STDERR);
	d3 = dup(pipe_err[1]);
	if (d3 < 0) {
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

	if (execvp(cmd, (char * const*)(*pinfo)->argv) == -1)
	    ret = errno;
	perror(cmd);
#if 0
	// TODO - do we need to close the dup handles?
	fflush(NULL);
	close(d1);
	close(d2);
	close(d3);
#endif
	exit(16);
    }

    (void)close(pipe_in[0]);
    (void)close(pipe_out[1]);
    (void)close(pipe_err[1]);

    /* Save necessary information for parental process manipulation */
    (*pinfo)->fd_in = pipe_in[1];
    if (opts & BU_PROCESS_OUT_EQ_ERR) {
	(*pinfo)->fd_out = pipe_err[0];
    } else {
	(*pinfo)->fd_out = pipe_out[0];
    }
    (*pinfo)->fd_err = pipe_err[0];
    (*pinfo)->pid = pid;

#else
    struct bu_vls cp_cmd = BU_VLS_INIT_ZERO;
    HANDLE pipe_in[2], pipe_inDup;
    HANDLE pipe_out[2], pipe_outDup;
    HANDLE pipe_err[2], pipe_errDup;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};

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
    if (opts & BU_PROCESS_HIDE_WINDOW) {
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
    } else {
	si.dwFlags = STARTF_USESTDHANDLES;
    }
    si.hStdInput   = pipe_in[0];
    if (opts & BU_PROCESS_OUT_EQ_ERR) {
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

    if (!CreateProcess(NULL, bu_vls_addr(&cp_cmd), NULL, NULL, TRUE,
		       DETACHED_PROCESS, NULL, NULL,
		       &si, &pi)) {
	ret = GetLastError();
    }
    bu_vls_free(&cp_cmd);

    CloseHandle(pipe_in[0]);
    CloseHandle(pipe_out[1]);
    CloseHandle(pipe_err[1]);

    /* Save necessary information for parental process manipulation.
     * Switching from HANDLE to file descriptor so the rest of the code can be
     * consistent - see
     * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/open-osfhandle
     */
    (*pinfo)->fd_in = _open_osfhandle((intptr_t)pipe_inDup, 0);
    if (opts & BU_PROCESS_OUT_EQ_ERR) {
	(*pinfo)->fd_out = _open_osfhandle((intptr_t)pipe_errDup, 0);
    } else {
	(*pinfo)->fd_out = _open_osfhandle((intptr_t)pipe_outDup, 0);
    }
    (*pinfo)->fd_err = _open_osfhandle((intptr_t)pipe_errDup, 0);
    (*pinfo)->hProcess = pi.hProcess;
    (*pinfo)->pid = pi.dwProcessId;
    (*pinfo)->aborted = 0;

#endif

    return ret;
}


void
bu_process_exec(struct bu_process **p, const char *cmd, int argc, const char **argv, int out_eql_err, int hide_window)
{
    if (!p || !cmd)
	return;

    // make sure cmd starts the argv, and argv is null terminated
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
    } else {
	for (int i = 0; i < argc; i++) {
	    av[i] = argv[i];
	}
	av[argc] = (char *)NULL;
    }

    // combine opts for new call
    int opts = 0;
    if (out_eql_err) opts |= BU_PROCESS_OUT_EQ_ERR;
    if (hide_window) opts |= BU_PROCESS_HIDE_WINDOW;

    bu_process_create(p, av, (bu_process_create_opts)opts);
}

int
bu_process_wait_n(struct bu_process *pinfo, int wtime)
{
    if (!pinfo)
	return -1;

    int rc = 0;
#ifndef _WIN32
    int rpid;
    int retcode = 0;

    close(pinfo->fd_in);
    close(pinfo->fd_out);
    close(pinfo->fd_err);

    /* wait for process to end, or timeout */
    int64_t start_time = bu_gettime();
    while ((rpid = waitpid((pid_t)-pinfo->pid, &retcode, WNOHANG) != pinfo->pid)) {
	if (wtime && ((bu_gettime() - start_time) > wtime))	// poll wait() up to wtime if requested
	    break;
    }

    /* check wait() status and filter retcode */
    if (rpid == -1 || rpid == 0) {
	/* timed-out */
	bu_pid_terminate(pinfo->pid);
	rc = 0;	// process concluded, albeit forcibly
    } else {
	if (WIFEXITED(retcode))		    // normal exit
	    rc = 0;
	else if (WIFSIGNALED(retcode))	    // terminated
	    rc = ERROR_PROCESS_ABORTED;
	else
	    rc = retcode;
    }
#else
    DWORD retcode = 0;

    /* wait for the forked process */
    if (wtime > 0) {
	WaitForSingleObject(pinfo->hProcess, wtime);
    } else {
	WaitForSingleObject(pinfo->hProcess, INFINITE);
    }

    if (GetExitCodeProcess(pinfo->hProcess, &retcode)) {    // make sure function succeeds
	if (GetLastError() == ERROR_PROCESS_ABORTED || retcode == BU_MSVC_ABORT_EXIT) {
	    // collapse abort into our abort code
	    rc = ERROR_PROCESS_ABORTED;
	} else {
	    rc = (int)retcode;
	}
    } else {
	rc = -1;
    }

    CloseHandle(pinfo->hProcess);
#endif
    /* Clean up */
    bu_process_file_close(pinfo, BU_PROCESS_STDOUT);
    bu_process_file_close(pinfo, BU_PROCESS_STDERR);

    /* Free copy of exec args */
    bu_free((void *)pinfo->cmd, "pinfo cmd copy");

    if (pinfo->argv) {
	for (int i = 0; i < pinfo->argc; i++) {
	    bu_free((void *)pinfo->argv[i], "pinfo argv member");
	}
	bu_free((void *)pinfo->argv, "pinfo argv array");
    }
    BU_PUT(pinfo, struct bu_process);

    return rc;
}

int
bu_process_wait(int *aborted, struct bu_process *pinfo, int wtime)
{
    int wait_ret = bu_process_wait_n(pinfo, wtime);

    if (aborted && wait_ret == ERROR_PROCESS_ABORTED)
	(*aborted) = 1;

    return wait_ret;
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
bu_process_alive(struct bu_process* pinfo)
{
    if (!pinfo)
	return 0;

#if defined(_WIN32)
    const unsigned long win_wait_timeout = 0x00000102L;

    // if process is alive, timeout should immediately come back
    return WaitForSingleObject(pinfo->hProcess, 0) == win_wait_timeout;
#else
    // TODO
#endif


    return 0;
}

int
bu_pid_alive(int pid)
{
    if (!pid)
	return 0;

#if defined(_WIN32)
    HANDLE pHandle = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
    if (pHandle == NULL) { // couldn't open - process is not alive
	return 0;
    } else {
	const unsigned long win_wait_timeout = 0x00000102L;

	// if process is alive, timeout should immediately come back
	DWORD ret = WaitForSingleObject(pHandle, 0);
	CloseHandle(pHandle);
	return ret == win_wait_timeout;
    }
#else
    return kill((pid_t)pid, 0) == 0;
#endif


    return 0;
}

int
bu_pid_terminate(int process)
{
    int successful = 0;
#ifdef HAVE_KILL
    /* kill process and all children (negative pid, sysv extension) */
    successful = kill((pid_t)-process, SIGKILL);
    /* kill() returns 0 for success */
    successful = !successful;
#else /* !HAVE_KILL */
    HANDLE hProcessSnap;
    HANDLE hProcess;
    PROCESSENTRY32 pe32 = {0};

    pe32.dwSize = sizeof(PROCESSENTRY32);
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
	return successful;
    }

    if (!Process32First(hProcessSnap, &pe32)) {
	CloseHandle(hProcessSnap);
	return successful;
    }

    /* First, find and kill the children */
    do {
	if (pe32.th32ParentProcessID == (DWORD)process) {
	    bu_pid_terminate((int)pe32.th32ProcessID);
	}
    } while(Process32Next(hProcessSnap, &pe32));

    /* Finally, kill the parent */
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, (DWORD)process);
    if (hProcess != NULL) {
	successful = TerminateProcess(hProcess, BU_MSVC_ABORT_EXIT);
	CloseHandle(hProcess);
    }

    CloseHandle(hProcessSnap);
#endif	/* HAVE_KILL */
    return successful;
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
