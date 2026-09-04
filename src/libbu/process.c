/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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
#include <errno.h>
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
#include "bio.h"
#include "bnetwork.h"
#include "bu/debug.h"
#include "bu/interrupt.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "bu/datetime.h"
#include "bu/vls.h"
#include "./process_private.h"

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
    int exited;
    int exit_status;
#if defined(_WIN32) && !defined(__CYGWIN__)
    HANDLE hProcess;
    DWORD pid;
#else
    int pid;
#endif
    int aborted;
};


enum {
    PROCESS_EXEC_ERROR = 16,
    PROCESS_WAIT_POLL_USEC = 1000,
    PROCESS_TERMINATE_GRACE_MS = 5000
};


static void
process_record_free(struct bu_process *process)
{
    if (!process)
	return;

    bu_free((void *)process->cmd, "pinfo cmd copy");
    if (process->argv) {
	for (int i = 0; i < process->argc; i++)
	    bu_free((void *)process->argv[i], "pinfo argv member");
	bu_free((void *)process->argv, "pinfo argv array");
    }
    BU_PUT(process, struct bu_process);
}


#ifndef _WIN32
static int
process_wait_status(int status)
{
    if (WIFEXITED(status))
	return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
	return ERROR_PROCESS_ABORTED;

    return -1;
}
#else
static void
process_windows_append_backslashes(struct bu_vls *command, size_t count)
{
    for (size_t i = 0; i < count; i++)
	bu_vls_putc(command, '\\');
}


/* Quote one argument according to the Microsoft C runtime argv rules. */
static void
process_windows_append_arg(struct bu_vls *command, const char *argument)
{
    size_t backslashes = 0;

    bu_vls_putc(command, '"');
    for (const char *current = argument; ; current++) {
	if (*current == '\\') {
	    backslashes++;
	    continue;
	}
	if (*current == '"') {
	    process_windows_append_backslashes(command, backslashes);
	    process_windows_append_backslashes(command, backslashes + 1);
	    bu_vls_putc(command, '"');
	    backslashes = 0;
	    continue;
	}
	if (*current == '\0') {
	    process_windows_append_backslashes(command, backslashes);
	    process_windows_append_backslashes(command, backslashes);
	    break;
	}
	process_windows_append_backslashes(command, backslashes);
	backslashes = 0;
	bu_vls_putc(command, *current);
    }
    bu_vls_strcat(command, "\" ");
}
#endif


static void
process_func_info_reset(struct bu_process_func_info *info)
{
    if (!info)
	return;

    info->exit_code = -1;
    info->signaled = 0;
    info->signal_num = 0;
    info->timed_out = 0;
}


#ifndef _WIN32
static void
process_func_capture(int *fd, struct bu_vls *vls)
{
    char buff[1024] = {0};

    if (!fd || *fd < 0)
	return;

    while (*fd >= 0) {
	int ret = read(*fd, buff, sizeof(buff));
	if (ret > 0) {
	    if (vls)
		bu_vls_strncat(vls, buff, (size_t)ret);
	    if (!bu_process_pending(*fd))
		return;
	    continue;
	}

	if (ret < 0 && (errno == EINTR || errno == EAGAIN))
	    return;

	close(*fd);
	*fd = -1;
	return;
    }
}
#endif


void
bu_process_file_close(struct bu_process *pinfo, bu_process_io_t d)
{
    if (!pinfo)
	return;

    FILE *fp = NULL;
    if (d == BU_PROCESS_STDIN) {
	fp = pinfo->fp_in;
    }
    if (d == BU_PROCESS_STDOUT) {
	fp = pinfo->fp_out;
    }
    if (d == BU_PROCESS_STDERR) {
	fp = pinfo->fp_err;
    }
    if (!fp)
	return;

    int fd = fileno(fp);
    if (pinfo->fp_in == fp)
	pinfo->fp_in = NULL;
    if (pinfo->fp_out == fp)
	pinfo->fp_out = NULL;
    if (pinfo->fp_err == fp)
	pinfo->fp_err = NULL;
    (void)fclose(fp);

    /* OUT_EQ_ERR aliases stdout and stderr.  Invalidate every matching
     * descriptor so neither wait nor another stream wrapper closes it twice. */
    if (pinfo->fd_in == fd)
	pinfo->fd_in = -1;
    if (pinfo->fd_out == fd)
	pinfo->fd_out = -1;
    if (pinfo->fd_err == fd)
	pinfo->fd_err = -1;
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

    if (d == BU_PROCESS_STDIN) {
	if (pinfo->fp_in)
	    return pinfo->fp_in;
	pinfo->fp_in = fdopen(pinfo->fd_in, "wb");
	return pinfo->fp_in;
    }
    if (d == BU_PROCESS_STDOUT) {
	if (pinfo->fp_out)
	    return pinfo->fp_out;
	if (pinfo->fd_out == pinfo->fd_err && pinfo->fp_err) {
	    pinfo->fp_out = pinfo->fp_err;
	    return pinfo->fp_out;
	}
	pinfo->fp_out = fdopen(pinfo->fd_out, "rb");
	return pinfo->fp_out;
    }
    if (d == BU_PROCESS_STDERR) {
	if (pinfo->fp_err)
	    return pinfo->fp_err;
	if (pinfo->fd_err == pinfo->fd_out && pinfo->fp_out) {
	    pinfo->fp_err = pinfo->fp_out;
	    return pinfo->fp_err;
	}
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

void
bu_process_create(struct bu_process **pinfo, const char **argv, int opts)
{
    if (!pinfo)
	return;
    *pinfo = NULL;
    if (!argv || !argv[0])
	return;

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
    (*pinfo)->exited = 0;
    (*pinfo)->exit_status = -1;

    /* Make a copy of the final execvp args */
    (*pinfo)->cmd = bu_strdup(cmd);
    (*pinfo)->argc = argc;
    (*pinfo)->argv = (const char **)bu_calloc(argc + 1, sizeof(char *), "bu_process argv cpy"); // +1 for NULL termination
    for (int i = 0; i < argc; i++) {
	(*pinfo)->argv[i] = bu_strdup(argv[i]);
    }
    (*pinfo)->argv[argc] = (char *)NULL;	// sanity check

    int merged_output = (opts & BU_PROCESS_OUT_EQ_ERR);
#ifdef HAVE_UNISTD_H
    int pret;
    int pid;
    int pipe_in[2] = {-1, -1};
    int pipe_out[2] = {-1, -1};
    int pipe_err[2] = {-1, -1};

    pret = pipe(pipe_in);
    if (pret < 0) {
	perror("pipe");
	goto process_create_fail;
    }

    if (!merged_output) {
	pret = pipe(pipe_out);
	if (pret < 0) {
	    perror("pipe");
	    goto process_create_fail;
	}
    }

    pret = pipe(pipe_err);
    if (pret < 0) {
	perror("pipe");
	goto process_create_fail;
    }

    /* fork + exec */
    pid = fork();
    if (pid < 0) {
	perror("fork");
	goto process_create_fail;
    }
    if (pid == 0) {
	/* make this a process group leader */
	setpgid(0, 0);

	/* Redirect stdin and stderr */
	if (dup2(pipe_in[0], BU_PROCESS_STDIN) < 0)
	    _exit(PROCESS_EXEC_ERROR);
	if (merged_output) {
	    if (dup2(pipe_err[1], BU_PROCESS_STDOUT) < 0)
		_exit(PROCESS_EXEC_ERROR);
	} else {
	    if (dup2(pipe_out[1], BU_PROCESS_STDOUT) < 0)
		_exit(PROCESS_EXEC_ERROR);
	}
	if (dup2(pipe_err[1], BU_PROCESS_STDERR) < 0)
	    _exit(PROCESS_EXEC_ERROR);

	/* close pipes */
	(void)close(pipe_in[0]);
	(void)close(pipe_in[1]);
	if (!merged_output) {
	    (void)close(pipe_out[0]);
	    (void)close(pipe_out[1]);
	}
	(void)close(pipe_err[0]);
	(void)close(pipe_err[1]);

	// TODO - should we be doing this for more than 20? See
	// https://docs.fedoraproject.org/en-US/Fedora_Security_Team/1/html/Defensive_Coding/sect-Defensive_Coding-Tasks-Descriptors-Child_Processes.html
	for (int i = 3; i < 20; i++) {
	    (void)close(i);
	}

	// TODO / FIXME - parent does not know whether child successfully started or not
	(void)execvp(cmd, (char * const*)(*pinfo)->argv);
	perror(cmd);

	_exit(PROCESS_EXEC_ERROR);
    }

    /* Set the child's process group from both sides of fork so it is ready
     * before an immediate timeout attempts group-directed termination. */
    (void)setpgid((pid_t)pid, (pid_t)pid);

    (void)close(pipe_in[0]);
    if (!merged_output)
	(void)close(pipe_out[1]);
    (void)close(pipe_err[1]);

    /* Save necessary information for parental process manipulation */
    (*pinfo)->fd_in = pipe_in[1];
    if (merged_output) {
	(*pinfo)->fd_out = pipe_err[0];
    } else {
	(*pinfo)->fd_out = pipe_out[0];
    }
    (*pinfo)->fd_err = pipe_err[0];
    (*pinfo)->pid = pid;
    return;

process_create_fail:
    for (size_t i = 0; i < 2; i++) {
	if (pipe_in[i] >= 0)
	    (void)close(pipe_in[i]);
	if (pipe_out[i] >= 0)
	    (void)close(pipe_out[i]);
	if (pipe_err[i] >= 0)
	    (void)close(pipe_err[i]);
    }
    process_record_free(*pinfo);
    *pinfo = NULL;

#else
    struct bu_vls cp_cmd = BU_VLS_INIT_ZERO;
    HANDLE pipe_in[2] = {NULL, NULL}, pipe_inDup = NULL;
    HANDLE pipe_out[2] = {NULL, NULL}, pipe_outDup = NULL;
    HANDLE pipe_err[2] = {NULL, NULL}, pipe_errDup = NULL;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!merged_output) {
	/* Create a pipe for the child process's STDOUT. */
	if (!CreatePipe(&pipe_out[0], &pipe_out[1], &sa, 0))
	    goto process_create_fail;

	/* Create noninheritable read handle and close the inheritable read handle. */
	if (!DuplicateHandle(GetCurrentProcess(), pipe_out[0],
			GetCurrentProcess(),  &pipe_outDup ,
			0,  FALSE,
			DUPLICATE_SAME_ACCESS))
	    goto process_create_fail;
	CloseHandle(pipe_out[0]);
	pipe_out[0] = NULL;
    }

    /* Create a pipe for the child process's STDERR. */
    if (!CreatePipe(&pipe_err[0], &pipe_err[1], &sa, 0))
	goto process_create_fail;

    /* Create noninheritable read handle and close the inheritable read handle. */
    if (!DuplicateHandle(GetCurrentProcess(), pipe_err[0],
		    GetCurrentProcess(),  &pipe_errDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS))
	goto process_create_fail;
    CloseHandle(pipe_err[0]);
    pipe_err[0] = NULL;

    /* Create a pipe for the child process's STDIN. */
    if (!CreatePipe(&pipe_in[0], &pipe_in[1], &sa, 0))
	goto process_create_fail;

    /* Duplicate the write handle to the pipe so it is not inherited. */
    if (!DuplicateHandle(GetCurrentProcess(), pipe_in[1],
		    GetCurrentProcess(), &pipe_inDup,
		    0, FALSE,                  /* not inherited */
		    DUPLICATE_SAME_ACCESS))
	goto process_create_fail;
    CloseHandle(pipe_in[1]);
    pipe_in[1] = NULL;

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
    if (merged_output) {
	si.hStdOutput  = pipe_err[1];
    } else {
	si.hStdOutput  = pipe_out[1];
    }
    si.hStdError   = pipe_err[1];

    /* CreateProcess uses one command-line string.  Quote every argument so
     * whitespace, empty strings, embedded quotes, and trailing backslashes
     * survive the child's C runtime parsing unchanged. */
    for (int i = 0; i < argc; i++)
	process_windows_append_arg(&cp_cmd, argv[i]);

    if (!CreateProcess(NULL, bu_vls_addr(&cp_cmd), NULL, NULL, TRUE,
		       DETACHED_PROCESS, NULL, NULL,
		       &si, &pi))
	goto process_create_fail;
    bu_vls_free(&cp_cmd);

    CloseHandle(pipe_in[0]);
    pipe_in[0] = NULL;
    if (!merged_output) {
	CloseHandle(pipe_out[1]);
	pipe_out[1] = NULL;
    }
    CloseHandle(pipe_err[1]);
    pipe_err[1] = NULL;

    /* Save necessary information for parental process manipulation.
     * Switching from HANDLE to file descriptor so the rest of the code can be
     * consistent - see
     * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/open-osfhandle
     */
    (*pinfo)->fd_in = _open_osfhandle((intptr_t)pipe_inDup, 0);
    if ((*pinfo)->fd_in < 0)
	goto process_create_fail;
    pipe_inDup = NULL;
    int err_fd = _open_osfhandle((intptr_t)pipe_errDup, 0);
    if (err_fd < 0)
	goto process_create_fail;
    pipe_errDup = NULL;
    (*pinfo)->fd_err = err_fd;
    if (merged_output) {
	(*pinfo)->fd_out = err_fd;
    } else {
	(*pinfo)->fd_out = _open_osfhandle((intptr_t)pipe_outDup, 0);
	if ((*pinfo)->fd_out < 0)
	    goto process_create_fail;
	pipe_outDup = NULL;
    }
    (*pinfo)->hProcess = pi.hProcess;
    (*pinfo)->pid = pi.dwProcessId;
    (*pinfo)->aborted = 0;
    CloseHandle(pi.hThread);
    return;

process_create_fail:
    bu_vls_free(&cp_cmd);
    if (pi.hProcess) {
	(void)TerminateProcess(pi.hProcess, BU_MSVC_ABORT_EXIT);
	CloseHandle(pi.hProcess);
    }
    if (pi.hThread)
	CloseHandle(pi.hThread);
    for (size_t i = 0; i < 2; i++) {
	if (pipe_in[i]) CloseHandle(pipe_in[i]);
	if (pipe_out[i]) CloseHandle(pipe_out[i]);
	if (pipe_err[i]) CloseHandle(pipe_err[i]);
    }
    if (pipe_inDup) CloseHandle(pipe_inDup);
    if (pipe_outDup) CloseHandle(pipe_outDup);
    if (pipe_errDup) CloseHandle(pipe_errDup);
    if ((*pinfo)->fd_in >= 0) (void)close((*pinfo)->fd_in);
    if ((*pinfo)->fd_out >= 0) (void)close((*pinfo)->fd_out);
    if ((*pinfo)->fd_err >= 0 && (*pinfo)->fd_err != (*pinfo)->fd_out)
	(void)close((*pinfo)->fd_err);
    process_record_free(*pinfo);
    *pinfo = NULL;

#endif
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

    bu_process_create(p, av, (bu_process_opts)opts);
}


int
bu_process_func(struct bu_process_func_info *info, bu_process_func_t func, void *data, int timeout_ms, int flags)
{
    if (!func)
	return -1;

    struct bu_process_func_info local_info = {-1, 0, 0, 0, NULL, NULL};
    struct bu_vls *out = NULL;
    struct bu_vls *err = NULL;

    if (!info)
	info = &local_info;

    process_func_info_reset(info);
    out = info->out;
    err = info->err;

    if (out)
	bu_vls_trunc(out, 0);
    if (err && err != out)
	bu_vls_trunc(err, 0);

#ifndef _WIN32
    int merged_err = (flags & BU_PROCESS_OUT_EQ_ERR);
    int pipe_out[2] = {-1, -1};
    int pipe_err[2] = {-1, -1};
    int fd_out = -1;
    int fd_err = -1;
    int status = 0;
    int child_done = 0;
    int pid = -1;
    struct bu_vls *capture_out = out;
    struct bu_vls *capture_err = err;
    int64_t start_time = bu_gettime();
    int64_t timeout_usec = (timeout_ms > 0) ? ((int64_t)timeout_ms * 1000) : 0;

    if (merged_err) {
	capture_out = out ? out : err;
	capture_err = NULL;
    }

    if (capture_out && pipe(pipe_out) < 0)
	goto process_func_fail;
    if (capture_err && pipe(pipe_err) < 0)
	goto process_func_fail;

    fflush(NULL);
    pid = fork();
    if (pid < 0)
	goto process_func_fail;

    if (pid == 0) {
	int ret = 0;

	setpgid(0, 0);

	if (capture_out) {
	    close(pipe_out[0]);
	    if (dup2(pipe_out[1], BU_PROCESS_STDOUT) < 0)
		_exit(16);
	}
	if (capture_err) {
	    close(pipe_err[0]);
	    if (dup2(pipe_err[1], BU_PROCESS_STDERR) < 0)
		_exit(16);
	} else if (merged_err && capture_out) {
	    if (dup2(pipe_out[1], BU_PROCESS_STDERR) < 0)
		_exit(16);
	}

	if (capture_out)
	    close(pipe_out[1]);
	if (capture_err)
	    close(pipe_err[1]);

	ret = func(data);
	fflush(NULL);
	_exit(ret);
    }

    /* Avoid racing the child's setpgid call if the timeout is very short. */
    (void)setpgid((pid_t)pid, (pid_t)pid);

    if (capture_out) {
	close(pipe_out[1]);
	fd_out = pipe_out[0];
	pipe_out[0] = -1;
	pipe_out[1] = -1;
    }
    if (capture_err) {
	close(pipe_err[1]);
	fd_err = pipe_err[0];
	pipe_err[0] = -1;
	pipe_err[1] = -1;
    }

    while (!child_done || fd_out >= 0 || fd_err >= 0) {
	if (!child_done) {
	    int wret = waitpid((pid_t)pid, &status, WNOHANG);
	    if (wret == pid) {
		child_done = 1;
	    } else if (wret < 0) {
		goto process_func_fail;
	    } else if (timeout_usec > 0 && (bu_gettime() - start_time) > timeout_usec) {
		info->timed_out = 1;
		(void)bu_pid_terminate(pid);
		if (waitpid((pid_t)pid, &status, 0) == pid)
		    child_done = 1;
	    }
	}

	if (fd_out < 0 && fd_err < 0) {
	    if (!child_done) {
		struct timeval tv = {0, 10000};
		(void)select(0, NULL, NULL, NULL, &tv);
	    }
	    continue;
	}

	fd_set read_set;
	int maxfd = -1;
	int sret = 0;
	struct timeval tv = {0, 10000};

	FD_ZERO(&read_set);
	if (fd_out >= 0) {
	    FD_SET(fd_out, &read_set);
	    maxfd = fd_out;
	}
	if (fd_err >= 0) {
	    FD_SET(fd_err, &read_set);
	    if (fd_err > maxfd)
		maxfd = fd_err;
	}

	sret = select(maxfd + 1, &read_set, NULL, NULL, &tv);
	if (sret < 0) {
	    if (errno == EINTR)
		continue;
	    goto process_func_fail;
	}
	if (sret == 0)
	    continue;

	if (fd_out >= 0 && FD_ISSET(fd_out, &read_set))
	    process_func_capture(&fd_out, capture_out);
	if (fd_err >= 0 && FD_ISSET(fd_err, &read_set))
	    process_func_capture(&fd_err, capture_err);
    }

    if (WIFEXITED(status)) {
	info->exit_code = WEXITSTATUS(status);
	return info->exit_code;
    }

    if (WIFSIGNALED(status)) {
	info->signaled = 1;
	info->signal_num = WTERMSIG(status);
	return ERROR_PROCESS_ABORTED;
    }

    return -1;

process_func_fail:
    if (pipe_out[0] >= 0) close(pipe_out[0]);
    if (pipe_out[1] >= 0) close(pipe_out[1]);
    if (pipe_err[0] >= 0) close(pipe_err[0]);
    if (pipe_err[1] >= 0) close(pipe_err[1]);
    if (fd_out >= 0) close(fd_out);
    if (fd_err >= 0) close(fd_err);
    if (pid > 0) {
	(void)bu_pid_terminate(pid);
	(void)waitpid((pid_t)pid, &status, 0);
    }
    return -1;
#else
    (void)data;
    (void)timeout_ms;
    (void)flags;
    return -1;
#endif
}

int
bu_process_wait_n(struct bu_process **pinfo, int wtime)
{
    if (!pinfo || !*pinfo)
	return -1;

    struct bu_process *process = *pinfo;
    int rc = 0;

    /* A FILE owns its descriptor, so close streams first.  Close any
     * remaining raw descriptors exactly once and invalidate all aliases. */
    bu_process_file_close(process, BU_PROCESS_STDIN);
    bu_process_file_close(process, BU_PROCESS_STDOUT);
    bu_process_file_close(process, BU_PROCESS_STDERR);
    int *fds[3] = {&process->fd_in, &process->fd_out, &process->fd_err};
    for (size_t i = 0; i < 3; i++) {
	int fd = *fds[i];
	if (fd < 0)
	    continue;
	(void)close(fd);
	for (size_t j = i; j < 3; j++) {
	    if (*fds[j] == fd)
		*fds[j] = -1;
	}
    }

#ifndef _WIN32
    if (process->exited) {
	rc = process->exit_status;
    } else {
	int wait_status = 0;
	int64_t start_time = bu_gettime();
	int64_t timeout_usec = (wtime > 0) ? (int64_t)wtime * 1000 : 0;
	int wait_result = 0;
	int timed_out = 0;

	while (1) {
	    do {
		wait_result = waitpid((pid_t)process->pid, &wait_status, WNOHANG);
	    } while (wait_result < 0 && errno == EINTR);

	    if (wait_result == process->pid) {
		rc = process_wait_status(wait_status);
		break;
	    }
	    if (wait_result < 0) {
		rc = -1;
		break;
	    }
	    if (timeout_usec && bu_gettime() - start_time >= timeout_usec) {
		timed_out = 1;
		break;
	    }

	    (void)bu_snooze(PROCESS_WAIT_POLL_USEC);
	}

	if (timed_out) {
	    int terminated = bu_pid_terminate(process->pid);
	    int wait_options = terminated ? 0 : WNOHANG;

	    do {
		wait_result = waitpid((pid_t)process->pid, &wait_status, wait_options);
	    } while (wait_result < 0 && errno == EINTR);

	    rc = (wait_result == process->pid) ? process_wait_status(wait_status) : -1;
	}
    }
#else
    DWORD retcode = 0;

    if (process->exited) {
	rc = process->exit_status;
    } else {
	DWORD timeout = (wtime > 0) ? (DWORD)wtime : INFINITE;
	DWORD wait_result = WaitForSingleObject(process->hProcess, timeout);

	if (wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(process->hProcess, &retcode)) {
	    if (retcode == BU_MSVC_ABORT_EXIT) {
		rc = ERROR_PROCESS_ABORTED;
	    } else {
		rc = (int)retcode;
	    }
	} else if (wait_result == WAIT_TIMEOUT) {
	    /* Descendants may hold inherited pipes open, so stop the process tree
	     * before falling back to terminating only the direct child. */
	    (void)bu_pid_terminate(process->pid);
	    wait_result = WaitForSingleObject(process->hProcess, PROCESS_TERMINATE_GRACE_MS);
	    if (wait_result != WAIT_OBJECT_0 &&
		TerminateProcess(process->hProcess, BU_MSVC_ABORT_EXIT)) {
		wait_result = WaitForSingleObject(process->hProcess, INFINITE);
	    }
	    rc = (wait_result == WAIT_OBJECT_0) ? ERROR_PROCESS_ABORTED : -1;
	} else {
	    rc = -1;
	}
    }

    CloseHandle(process->hProcess);
#endif
    process_record_free(*pinfo);
    *pinfo = NULL;

    return rc;
}

int
bu_process_wait(int *aborted, struct bu_process *pinfo, int wtime)
{
    int wait_ret = bu_process_wait_n(&pinfo, wtime);

    if (aborted && wait_ret == ERROR_PROCESS_ABORTED)
	(*aborted) = 1;

    return wait_ret;
}

int
bu_process_pending(int fd)
{
    if (fd < 0)
	return 0;

    int result;

#if defined(_WIN32)
    intptr_t os_handle = _get_osfhandle(fd);
    if (os_handle == -1)
	return 0;

    HANDLE out_fd = (HANDLE)os_handle;
    DWORD bytes_available = 0;
    if (PeekNamedPipe(out_fd, NULL, 0, NULL, &bytes_available, NULL)) {
	result = (int)bytes_available;
    } else {
	result = -1;
    }
#else
    if (fd >= FD_SETSIZE)
	return 0;

    fd_set read_set;
    struct timeval timeout = {0, 0};
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    /* returns 1 on success, 0 on timeout, -1 on error */
    result = select(fd+1, &read_set, NULL, NULL, &timeout);
#endif

    /* collapse return to ignore amount to read or errors */
    return result > 0 ? 1 : 0;
}

int
bu_process_alive(struct bu_process *pinfo)
{
    return bu_process_poll(pinfo, NULL) == 0;
}

int
bu_process_poll(struct bu_process *pinfo, int *exit_status)
{
    if (!pinfo)
	return -1;

    if (pinfo->exited) {
	if (exit_status)
	    *exit_status = pinfo->exit_status;
	return 1;
    }

#if defined(_WIN32)
    DWORD status = 0;
    if (!GetExitCodeProcess(pinfo->hProcess, &status))
	return -1;
    if (status == STILL_ACTIVE)
	return 0;

    pinfo->exit_status = (status == BU_MSVC_ABORT_EXIT) ?
	ERROR_PROCESS_ABORTED : (int)status;
#else
    int status = 0;
    int wait_result = 0;
    do {
	wait_result = waitpid((pid_t)pinfo->pid, &status, WNOHANG);
    } while (wait_result < 0 && errno == EINTR);
    if (wait_result == 0)
	return 0;
    if (wait_result < 0)
	return -1;

    pinfo->exit_status = process_wait_status(status);
    if (pinfo->exit_status < 0)
	return -1;
#endif

    pinfo->exited = 1;
    if (exit_status)
	*exit_status = pinfo->exit_status;
    return 1;
}

int
bu_process_terminate(struct bu_process *pinfo)
{
    if (!pinfo)
	return 0;

    int poll_result = bu_process_poll(pinfo, NULL);
    /* The process group may outlive its leader, so always attempt group/tree
     * termination even when the direct child has already completed. */
    if (bu_pid_terminate((int)pinfo->pid))
	return 1;

    if (poll_result == 1)
	return 1;
    if (poll_result < 0)
	return 0;

    /* The child may have exited between polling and termination. */
    return bu_process_poll(pinfo, NULL) == 1;
}

int
bu_pid_alive(int pid)
{
    if (pid <= 0)
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
    return waitpid(pid, NULL, WNOHANG) == 0;
#endif


    return 0;
}

int
bu_pid_terminate(int process)
{
    int successful = 0;
    if (process <= 0)
	return successful;

#ifdef HAVE_KILL
    /* kill process and all children (negative pid, sysv extension) */
    successful = kill((pid_t)-process, SIGKILL);
    if (successful != 0)
	successful = kill((pid_t)process, SIGKILL);
    /* kill() returns zero for success. */
    successful = (successful == 0);
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

int
bu_process_alive_id(int pid)
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

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
