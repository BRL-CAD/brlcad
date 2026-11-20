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
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include <string.h>
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
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
#ifdef __APPLE__
#  include <crt_externs.h>
#endif
#if defined(__linux__) && defined(HAVE_SYS_SYSCALL_H)
#  include <sys/syscall.h>
#endif
#include "bio.h"
#include "bnetwork.h"
#include "bu/debug.h"
#include "bu/interrupt.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/sort.h"
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
#ifndef _WIN32
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


static size_t
process_env_name_length(const char *entry)
{
    const char *separator;

    if (!entry || !entry[0] || entry[0] == '=')
	return 0;
    separator = strchr(entry, '=');
    return separator ? (size_t)(separator - entry) : 0;
}


#ifndef _WIN32
#ifndef __APPLE__
extern char **environ;
#endif


static char ***
process_environment_location(void)
{
#ifdef __APPLE__
    return _NSGetEnviron();
#else
    return &environ;
#endif
}


static int
process_wait_status(int status)
{
    if (WIFEXITED(status))
	return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
	return ERROR_PROCESS_ABORTED;

    return -1;
}


#ifdef HAVE_PTHREAD_H
static void *
process_reap_thread(void *data)
{
    pid_t pid = (pid_t)(intptr_t)data;
    int status = 0;

    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
	;

    return NULL;
}
#endif


static void
process_reap_async(pid_t pid)
{
#ifdef HAVE_PTHREAD_H
    pthread_attr_t attributes;
    pthread_t thread;

    if (pthread_attr_init(&attributes) != 0)
	return;
    /* Failure to create the worker must not restore an unbounded wait.  In
     * that resource-exhaustion case the OS reclaims the child at parent exit. */
    if (pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED) == 0)
	(void)pthread_create(&thread, &attributes, process_reap_thread,
	    (void *)(intptr_t)pid);
    (void)pthread_attr_destroy(&attributes);
#else
    (void)pid;
#endif
}


static int
process_wait_pid(pid_t pid, int *status, int timeout_ms)
{
    int64_t deadline = bu_gettime() + (int64_t)timeout_ms * 1000;
    int wait_result;

    while (1) {
	do {
	    wait_result = waitpid(pid, status, WNOHANG);
	} while (wait_result < 0 && errno == EINTR);
	if (wait_result != 0 || bu_gettime() >= deadline)
	    return wait_result;
	(void)bu_snooze(PROCESS_WAIT_POLL_USEC);
    }
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
	    if (bu_fd_read_ready(*fd, 0) <= 0)
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

static int
process_inherit_fd(const struct bu_process_create_info *create_info, int fd)
{
    if (!create_info || !create_info->inherit_fds)
	return 0;

    for (size_t i = 0; i < create_info->inherit_fd_count; i++) {
	if (create_info->inherit_fds[i] == fd)
	    return 1;
    }

    return 0;
}


#ifndef _WIN32
static int
process_prepare_inherited_fds(const struct bu_process_create_info *create_info)
{
    if (!create_info || !create_info->inherit_fds)
	return 0;

    for (size_t i = 0; i < create_info->inherit_fd_count; i++) {
	int fd = create_info->inherit_fds[i];
	int flags;

	if (fd < 0 || (flags = fcntl(fd, F_GETFD)) < 0)
	    return -1;
	if ((flags & FD_CLOEXEC) && fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) < 0)
	    return -1;
    }

    return 0;
}


static void
process_close_unlisted_fds(const struct bu_process_create_info *create_info)
{
#if defined(__linux__) && defined(SYS_close_range)
    unsigned int close_range_first = 3;
    int close_range_available = 1;

    while (close_range_first < UINT_MAX) {
	unsigned int next = UINT_MAX;
	if (create_info && create_info->inherit_fds) {
	    for (size_t i = 0; i < create_info->inherit_fd_count; i++) {
		int fd = create_info->inherit_fds[i];
		if (fd >= 3 && (unsigned int)fd >= close_range_first &&
		    (unsigned int)fd < next)
		    next = (unsigned int)fd;
	    }
	}

	if (next > close_range_first &&
	    syscall(SYS_close_range, close_range_first, next - 1, 0) < 0 &&
	    errno != EBADF) {
	    close_range_available = 0;
	    break;
	}
	if (next == UINT_MAX)
	    return;
	close_range_first = next + 1;
    }

    if (close_range_available)
	return;
#endif

#ifdef HAVE_CLOSEFROM
    int closefrom_first = 3;
    while (1) {
	int next = INT_MAX;
	if (create_info && create_info->inherit_fds) {
	    for (size_t i = 0; i < create_info->inherit_fd_count; i++) {
		int fd = create_info->inherit_fds[i];
		if (fd >= closefrom_first && fd < next)
		    next = fd;
	    }
	}

	if (next == INT_MAX) {
	    closefrom(closefrom_first);
	    return;
	}
	for (int fd = closefrom_first; fd < next; fd++)
	    (void)close(fd);
	if (next == INT_MAX - 1)
	    return;
	closefrom_first = next + 1;
    }
#endif

    long max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd < 0)
	max_fd = 1024;
    for (int fd = 3; fd < max_fd; fd++) {
	if (!process_inherit_fd(create_info, fd))
	    (void)close(fd);
    }
}


static int
process_posix_env_names_equal(const char *left, const char *right)
{
    size_t left_length = process_env_name_length(left);
    size_t right_length = process_env_name_length(right);

    return left_length && left_length == right_length &&
	bu_strncmp(left, right, left_length) == 0;
}


static char **
process_posix_environment(const char * const *overrides)
{
    char **parent_environment = *process_environment_location();
    size_t environment_count = 0;
    size_t override_count = 0;
    size_t output_count = 0;
    char **environment;

    if (!overrides || !overrides[0])
	return NULL;
    while (overrides[override_count]) {
	if (!process_env_name_length(overrides[override_count]))
	    return NULL;
	override_count++;
    }
    while (parent_environment[environment_count])
	environment_count++;

    environment = (char **)bu_calloc(environment_count + override_count + 1,
	sizeof(char *), "process environment");
    for (size_t i = 0; i < environment_count; i++) {
	const char *entry = parent_environment[i];
	for (size_t j = 0; j < override_count; j++) {
	    if (process_posix_env_names_equal(entry, overrides[j]))
		entry = overrides[j];
	}
	environment[output_count++] = (char *)entry;
    }
    for (size_t i = 0; i < override_count; i++) {
	int found = 0;
	for (size_t j = 0; j < environment_count; j++) {
	    if (process_posix_env_names_equal(overrides[i], parent_environment[j])) {
		found = 1;
		break;
	    }
	}
	for (size_t j = i + 1; !found && j < override_count; j++) {
	    if (process_posix_env_names_equal(overrides[i], overrides[j]))
		found = 1;
	}
	if (!found)
	    environment[output_count++] = (char *)overrides[i];
    }

    return environment;
}
#endif


#ifdef _WIN32
static int
process_env_names_equal(const char *left, const char *right)
{
    size_t left_length = process_env_name_length(left);
    size_t right_length = process_env_name_length(right);

    return left_length && left_length == right_length &&
	_strnicmp(left, right, left_length) == 0;
}


static int
process_env_compare(const void *left, const void *right, void *UNUSED(context))
{
    const char *left_entry = *(const char * const *)left;
    const char *right_entry = *(const char * const *)right;
    return _stricmp(left_entry, right_entry);
}


static char *
process_windows_environment(const char * const *overrides)
{
    LPCH parent_environment;
    const char *entry;
    const char **entries;
    size_t entry_count = 0;
    size_t override_count = 0;
    size_t output_size = 1;
    char *environment;
    char *output;

    if (!overrides || !overrides[0])
	return NULL;

    while (overrides[override_count]) {
	if (!process_env_name_length(overrides[override_count]))
	    return NULL;
	override_count++;
    }

    parent_environment = GetEnvironmentStringsA();
    if (!parent_environment)
	return NULL;

    for (entry = parent_environment; entry[0]; entry += strlen(entry) + 1)
	entry_count++;

    entries = (const char **)bu_calloc(entry_count + override_count,
	sizeof(const char *), "process environment entries");
    entry_count = 0;
    for (entry = parent_environment; entry[0]; entry += strlen(entry) + 1) {
	int replaced = 0;
	for (size_t i = 0; i < override_count; i++) {
	    if (process_env_names_equal(entry, overrides[i])) {
		replaced = 1;
		break;
	    }
	}
	if (!replaced)
	    entries[entry_count++] = entry;
    }
    for (size_t i = 0; i < override_count; i++) {
	int superseded = 0;
	for (size_t j = i + 1; j < override_count; j++) {
	    if (process_env_names_equal(overrides[i], overrides[j])) {
		superseded = 1;
		break;
	    }
	}
	if (!superseded)
	    entries[entry_count++] = overrides[i];
    }

    bu_sort(entries, entry_count, sizeof(const char *), process_env_compare,
	NULL);
    for (size_t i = 0; i < entry_count; i++)
	output_size += strlen(entries[i]) + 1;

    environment = (char *)bu_malloc(output_size, "process environment");
    output = environment;
    for (size_t i = 0; i < entry_count; i++) {
	size_t entry_size = strlen(entries[i]) + 1;
	memcpy(output, entries[i], entry_size);
	output += entry_size;
    }
    *output = '\0';

    bu_free(entries, "process environment entries");
    FreeEnvironmentStringsA(parent_environment);
    return environment;
}
#endif


void
bu_process_create_ex(struct bu_process **pinfo, const char **argv, const struct bu_process_create_info *create_info)
{
    int opts = create_info ? create_info->opts : BU_PROCESS_DEFAULT;
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
#ifndef _WIN32
    int pret;
    int pid;
    int pipe_in[2] = {-1, -1};
    int pipe_out[2] = {-1, -1};
    int pipe_err[2] = {-1, -1};
    char **child_environment = NULL;
    char ***environment_location = process_environment_location();

    if (create_info && create_info->env_overrides &&
	create_info->env_overrides[0]) {
	child_environment = process_posix_environment(create_info->env_overrides);
	if (!child_environment)
	    goto process_create_fail;
    }

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

	if (process_prepare_inherited_fds(create_info) < 0)
	    _exit(PROCESS_EXEC_ERROR);
	process_close_unlisted_fds(create_info);

	if (child_environment)
	    *environment_location = child_environment;

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
    if (child_environment)
	bu_free(child_environment, "process environment");
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
    if (child_environment)
	bu_free(child_environment, "process environment");
    process_record_free(*pinfo);
    *pinfo = NULL;

#else
    struct bu_vls cp_cmd = BU_VLS_INIT_ZERO;
    HANDLE pipe_in[2] = {NULL, NULL}, pipe_inDup = NULL;
    HANDLE pipe_out[2] = {NULL, NULL}, pipe_outDup = NULL;
    HANDLE pipe_err[2] = {NULL, NULL}, pipe_errDup = NULL;
    STARTUPINFOEX si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};
    HANDLE *inherit_handles = NULL;
    SIZE_T attribute_size = 0;
    size_t inherit_handle_count = 0;
    char *child_environment = NULL;

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

    si.StartupInfo.cb = sizeof(STARTUPINFOEX);
    si.StartupInfo.lpReserved = NULL;
    si.StartupInfo.lpReserved2 = NULL;
    si.StartupInfo.cbReserved2 = 0;
    si.StartupInfo.lpDesktop = NULL;
    if (opts & BU_PROCESS_HIDE_WINDOW) {
	si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.StartupInfo.wShowWindow = SW_HIDE;
    } else {
	si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    }
    si.StartupInfo.hStdInput = pipe_in[0];
    if (merged_output) {
	si.StartupInfo.hStdOutput = pipe_err[1];
    } else {
	si.StartupInfo.hStdOutput = pipe_out[1];
    }
    si.StartupInfo.hStdError = pipe_err[1];

    inherit_handles = (HANDLE *)bu_calloc(3 +
	(create_info ? create_info->inherit_fd_count : 0), sizeof(HANDLE),
	"process inherited handles");
    inherit_handles[inherit_handle_count++] = si.StartupInfo.hStdInput;
    inherit_handles[inherit_handle_count++] = si.StartupInfo.hStdOutput;
    if (si.StartupInfo.hStdError != si.StartupInfo.hStdOutput)
	inherit_handles[inherit_handle_count++] = si.StartupInfo.hStdError;
    if (create_info && create_info->inherit_fds) {
	for (size_t i = 0; i < create_info->inherit_fd_count; i++) {
	    HANDLE handle = (HANDLE)_get_osfhandle(create_info->inherit_fds[i]);
	    int duplicate = 0;
	    if (handle == INVALID_HANDLE_VALUE)
		goto process_create_fail;
	    for (size_t j = 0; j < inherit_handle_count; j++) {
		if (inherit_handles[j] == handle) {
		    duplicate = 1;
		    break;
		}
	    }
	    if (!duplicate)
		inherit_handles[inherit_handle_count++] = handle;
	}
    }

    for (size_t i = 0; i < inherit_handle_count; i++) {
	DWORD handle_flags = 0;
	if (!GetHandleInformation(inherit_handles[i], &handle_flags) ||
	    !(handle_flags & HANDLE_FLAG_INHERIT))
	    goto process_create_fail;
    }

    (void)InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_size);
    if (!attribute_size)
	goto process_create_fail;
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)bu_malloc(attribute_size,
	"process attribute list");
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0,
	    &attribute_size))
	goto process_create_fail;
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
	    PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit_handles,
	    inherit_handle_count * sizeof(HANDLE), NULL, NULL))
	goto process_create_fail;

    if (create_info && create_info->env_overrides &&
	create_info->env_overrides[0]) {
	child_environment = process_windows_environment(create_info->env_overrides);
	if (!child_environment)
	    goto process_create_fail;
    }

    /* CreateProcess uses one command-line string.  Quote every argument so
     * whitespace, empty strings, embedded quotes, and trailing backslashes
     * survive the child's C runtime parsing unchanged. */
    for (int i = 0; i < argc; i++)
	process_windows_append_arg(&cp_cmd, argv[i]);

    if (!CreateProcess(NULL, bu_vls_addr(&cp_cmd), NULL, NULL, TRUE,
		       DETACHED_PROCESS | EXTENDED_STARTUPINFO_PRESENT,
		       child_environment, NULL, &si.StartupInfo, &pi))
	goto process_create_fail;
    bu_vls_free(&cp_cmd);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    bu_free(si.lpAttributeList, "process attribute list");
    si.lpAttributeList = NULL;
    bu_free(inherit_handles, "process inherited handles");
    inherit_handles = NULL;
    if (child_environment) {
	bu_free(child_environment, "process environment");
	child_environment = NULL;
    }

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
    (*pinfo)->fd_err = _open_osfhandle((intptr_t)pipe_errDup, 0);
    if ((*pinfo)->fd_err < 0)
	goto process_create_fail;
    pipe_errDup = NULL;
    if (merged_output) {
	(*pinfo)->fd_out = (*pinfo)->fd_err;
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
    if (si.lpAttributeList) {
	DeleteProcThreadAttributeList(si.lpAttributeList);
	bu_free(si.lpAttributeList, "process attribute list");
    }
    if (inherit_handles)
	bu_free(inherit_handles, "process inherited handles");
    if (child_environment)
	bu_free(child_environment, "process environment");
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
bu_process_create(struct bu_process **pinfo, const char **argv, int opts)
{
    struct bu_process_create_info create_info = {0};
    create_info.opts = opts;
    bu_process_create_ex(pinfo, argv, &create_info);
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
    int64_t termination_deadline = 0;
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
	    }
	}
	if (!info->timed_out && timeout_usec > 0 &&
	    bu_gettime() - start_time >= timeout_usec) {
	    info->timed_out = 1;
	    (void)bu_pid_terminate(pid);
	    termination_deadline = bu_gettime() +
		(int64_t)PROCESS_TERMINATE_GRACE_MS * 1000;
	}
	if (info->timed_out && bu_gettime() >= termination_deadline) {
	    if (!child_done)
		process_reap_async((pid_t)pid);
	    if (fd_out >= 0) close(fd_out);
	    if (fd_err >= 0) close(fd_err);
	    fd_out = -1;
	    fd_err = -1;
	    break;
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

    if (info->timed_out)
	return ERROR_PROCESS_ABORTED;

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
	if (process_wait_pid((pid_t)pid, &status,
		PROCESS_TERMINATE_GRACE_MS) == 0)
	    process_reap_async((pid_t)pid);
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
	    (void)bu_pid_terminate(process->pid);
	    wait_result = process_wait_pid((pid_t)process->pid, &wait_status,
		PROCESS_TERMINATE_GRACE_MS);

	    if (wait_result == process->pid) {
		rc = process_wait_status(wait_status);
	    } else {
		rc = -1;
		if (wait_result == 0)
		    process_reap_async((pid_t)process->pid);
	    }
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
		wait_result = WaitForSingleObject(process->hProcess,
		    PROCESS_TERMINATE_GRACE_MS);
	    }
	    if (wait_result == WAIT_OBJECT_0) {
		rc = ERROR_PROCESS_ABORTED;
	    } else {
		rc = -1;
	    }
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
bu_fd_read_ready(int fd, int timeout_ms)
{
    if (fd < 0) {
	errno = EBADF;
	return -1;
    }
    if (timeout_ms < -1) {
	errno = EINVAL;
	return -1;
    }

#if defined(_WIN32)
    intptr_t os_handle = _get_osfhandle(fd);
    if (os_handle == -1 || os_handle == (intptr_t)INVALID_HANDLE_VALUE) {
	errno = EBADF;
	return -1;
    }

    HANDLE handle = (HANDLE)os_handle;
    SetLastError(NO_ERROR);
    DWORD file_type = GetFileType(handle);
    if (file_type == FILE_TYPE_UNKNOWN && GetLastError() != NO_ERROR) {
	errno = EBADF;
	return -1;
    }

    if (file_type == FILE_TYPE_PIPE) {
	const DWORD check_interval_ms = 10;
	ULONGLONG start = GetTickCount64();
	for (;;) {
	    DWORD bytes_available = 0;
	    if (PeekNamedPipe(handle, NULL, 0, NULL, &bytes_available, NULL)) {
		if (bytes_available > 0)
		    return 1;
	    } else {
		DWORD error_code = GetLastError();
		if (error_code == ERROR_BROKEN_PIPE ||
		    error_code == ERROR_PIPE_NOT_CONNECTED ||
		    error_code == ERROR_NO_DATA)
		    return 1;
		errno = EIO;
		return -1;
	    }

	    if (timeout_ms == 0)
		return 0;

	    DWORD wait_ms = check_interval_ms;
	    if (timeout_ms > 0) {
		ULONGLONG elapsed = GetTickCount64() - start;
		if (elapsed >= (ULONGLONG)timeout_ms)
		    return 0;
		ULONGLONG remaining = (ULONGLONG)timeout_ms - elapsed;
		if (remaining < wait_ms)
		    wait_ms = (DWORD)remaining;
	    }
	    Sleep(wait_ms);
	}
    }

    if (file_type == FILE_TYPE_DISK)
	return 1;

    if (file_type == FILE_TYPE_CHAR) {
	DWORD console_mode = 0;
	if (!GetConsoleMode(handle, &console_mode))
	    return 1;
	DWORD wait_ms = timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
	DWORD wait_result = WaitForSingleObject(handle, wait_ms);
	if (wait_result == WAIT_OBJECT_0)
	    return 1;
	if (wait_result == WAIT_TIMEOUT)
	    return 0;
	errno = EIO;
	return -1;
    }

    errno = EINVAL;
    return -1;
#elif defined(HAVE_POLL_H)
    struct pollfd descriptor;
    descriptor.fd = fd;
    descriptor.events = POLLIN;
    descriptor.revents = 0;

    int result = poll(&descriptor, 1, timeout_ms);
    if (result <= 0)
	return result;
    if (descriptor.revents & POLLNVAL) {
	errno = EBADF;
	return -1;
    }
    if (descriptor.revents & (POLLIN | POLLHUP | POLLERR))
	return 1;
    return 0;
#else
    if (fd >= FD_SETSIZE) {
	errno = EINVAL;
	return -1;
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);

    struct timeval timeout;
    struct timeval *timeout_ptr = NULL;
    if (timeout_ms >= 0) {
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;
	timeout_ptr = &timeout;
    }
    return select(fd + 1, &read_set, NULL, NULL, timeout_ptr);
#endif
}

int
bu_process_pending(int fd)
{
    return bu_fd_read_ready(fd, 0) > 0 ? 1 : 0;
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
    /* An exit code can be available before process teardown releases
     * inherited files and pipes.  Only report completion once signaled. */
    DWORD wait_result = WaitForSingleObject(pinfo->hProcess, 0);
    if (wait_result == WAIT_TIMEOUT)
	return 0;
    if (wait_result != WAIT_OBJECT_0)
	return -1;

    DWORD status = 0;
    if (!GetExitCodeProcess(pinfo->hProcess, &status))
	return -1;

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
bu_file_is_terminal(FILE *stream)
{
    if (!stream)
	return 0;
    return isatty(fileno(stream)) ? 1 : 0;
}

int
bu_file_is_open(FILE *stream)
{
    struct stat sb;
    int fd;

    if (!stream)
	return 0;

    fd = fileno(stream);
    if (fd < 0)
	return 0;

    return fstat(fd, &sb) == 0;
}

int
bu_interactive(void)
{
#ifdef _WIN32
    /* Winsock select() cannot inspect CRT standard input descriptors. */
    return bu_file_is_terminal(stdin);
#else
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
#endif
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
