/*                      P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#ifndef BU_PROCESS_H
#define BU_PROCESS_H

#include "common.h"

#include <stdio.h> /* FILE */
#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_process
 *
 * @brief
 * Routines for process and sub-process management.
 */
/** @{ */
/** @file bu/process.h */

/* Wrappers for using subprocess execution */
struct bu_process;
struct bu_vls;

typedef enum {
    BU_PROCESS_STDIN,
    BU_PROCESS_STDOUT,
    BU_PROCESS_STDERR
} bu_process_io_t;

typedef enum {
    BU_PROCESS_DEFAULT = 0x0,	    // default process options: equiv to (bu_process_opts)0
    BU_PROCESS_OUT_EQ_ERR = 0x1,    // stdout reads from stderr instead
    BU_PROCESS_HIDE_WINDOW = 0x2,   // (Windows only)hide creation window if process would normally spawn one
} bu_process_opts;

#ifndef ERROR_PROCESS_ABORTED
// have a consistent 'aborted' return code on cross-platforms
#define ERROR_PROCESS_ABORTED 1067L
#endif

/**
 * Callback type used by bu_process_func().
 *
 * The callback runs in a subprocess.  On POSIX platforms this is
 * implemented using fork(), so the callback should restrict itself to
 * fork-safe operations.
 *
 * @param[in] data - opaque pointer supplied to bu_process_func()
 *
 * @return
 * integer exit status for the subprocess
 */
typedef int (*bu_process_func_t)(void *data);

/**
 * Status and optional capture buffers for bu_process_func().
 *
 * If out and/or err are non-NULL, they must point to initialized
 * bu_vls containers.  bu_process_func() truncates any supplied capture
 * buffers before appending child process output.
 */
struct bu_process_func_info {
    int exit_code;
    int signaled;
    int signal_num;
    int timed_out;
    struct bu_vls *out;
    struct bu_vls *err;
};

/**
 * @brief Wrapper for creating a sub-process. Allocates bu_process and starts process
 *
 * @param[out] pinfo - newly allocated process handle, or NULL if setup fails
 * @param[in] argv - array of command line arguments to executed. Last element MUST be NULL
 * @param[in] process_creation_opts - bit field for bu_process_opts
 *
 * @note Process creation does not guarantee the child started successfully.
 * Use bu_process_wait_n() to check its exit status.
 */
BU_EXPORT extern void bu_process_create(struct bu_process **pinfo, const char **argv, int process_creation_opts);

/**
 * @brief Run a callback in a subprocess and wait for it to complete.
 *
 * The callback runs in an isolated child process.  On POSIX
 * platforms this is implemented using fork(), so the callback should
 * limit itself to fork-safe operations and communicate results back to
 * the parent via its return code and any captured output.
 *
 * If info is non-NULL, the status fields are populated on return.  If
 * info->out and/or info->err are non-NULL, those initialized bu_vls
 * buffers are truncated and filled with captured child output.  When
 * BU_PROCESS_OUT_EQ_ERR is supplied, stderr is merged into the stdout
 * capture buffer; if no stdout buffer is supplied, the merged stream is
 * captured in info->err when available.
 *
 * @param[in,out] info - status record and optional capture buffers.  May be NULL
 * @param[in] func - callback to run in the subprocess
 * @param[in] data - opaque pointer supplied to func
 * @param[in] timeout_ms - maximum wait time in milliseconds before forcibly
 * stopping the subprocess.  A value of 0 waits indefinitely
 * @param[in] flags - subprocess behavior flags.  BU_PROCESS_OUT_EQ_ERR is
 * currently supported; on non-POSIX platforms this API is not yet supported
 *
 * @return
 * callback return code if the subprocess exits normally;
 * ERROR_PROCESS_ABORTED if it is terminated by a signal or times out;
 * -1 on setup failure or unsupported platforms
 */
BU_EXPORT extern int bu_process_func(struct bu_process_func_info *info, bu_process_func_t func, void *data, int timeout_ms, int flags);


/**
 * @brief wait for a sub-process to complete, release all process
 * allocations, and release the process itself.
 *
 * @param[in,out] pinfo - address of the process handle; set to NULL on return
 * @param[in] wtime - maximum wait time (in ms) before forcibly stopping the
 * process.  A value of 0 waits indefinitely
 *
 * @return
 * 0 on success; ERROR_PROCESS_ABORTED for aborted process; Otherwise, platform specific exit status
 */
BU_EXPORT extern int bu_process_wait_n(struct bu_process **pinfo, int wtime);


/**
 * @brief determine whether process is still running
 *
 * @param[in] pinfo - bu_process structure of interest
 *
 * @return
 * 1 if alive, else 0
 */
BU_EXPORT extern int bu_process_alive(struct bu_process *pinfo);


/**
 * @brief Poll a subprocess without blocking or releasing its resources.
 *
 * Unlike bu_process_alive(), this routine also reports and preserves the
 * subprocess exit status for a later bu_process_wait_n() call.  Completion
 * does not imply that all subprocess output has been read; callers may continue
 * reading the process channels before releasing the process with
 * bu_process_wait_n().
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[out] exit_status - subprocess exit status when complete; may be NULL
 *
 * @return
 * 1 if the subprocess has completed; 0 if it is still running; -1 on error
 */
BU_EXPORT extern int bu_process_poll(struct bu_process *pinfo, int *exit_status);


/**
 * @brief Forcefully terminate a subprocess and its descendants.
 *
 * The subprocess remains available to bu_process_poll() and
 * bu_process_wait_n() so callers can drain output and reap resources.
 *
 * @param[in] pinfo - bu_process structure of interest
 *
 * @return
 * non-zero if the process was already complete or termination was requested
 * successfully; zero on error
 */
BU_EXPORT extern int bu_process_terminate(struct bu_process *pinfo);


/**
 * @brief determine whether there is data pending on fd
 *
 * @param[in] fd - file descriptor of interest
 *
 * @return
 * 1 if there is data on fd, else 0
 */
BU_EXPORT extern int bu_process_pending(int fd);


/**
 * @brief Read from a process's specified output channel
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] d - channel (BU_PROCESS_STDOUT, BU_PROCESS_STDERR)
 * @param[in] n - max number of bytes to be read
 * @param[out] buff - data read from channel
 *
 * @return
 * returns the number of bytes read into buff; 0 if read is at EOF; -1 on error
 *
 * @note the returned number of bytes read may be less than 'n'
 * in a successful read.
 */
BU_EXPORT extern int bu_process_read_n(struct bu_process *pinfo, bu_process_io_t d, int n, char *buff);


/**
 * @brief Open and return a FILE pointer associated with the specified channel.
 *
 * Input will be opened write, output and error will be opened
 * read.
 *
 * Caller should not close these FILE pointers directly.  Call
 * bu_process_file_close() instead.
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] d - channel (BU_PROCESS_STDIN, BU_PROCESS_STDOUT, BU_PROCESS_STDERR)
 *
 * @return
 * FILE pointer for specified channel
 */
BU_EXPORT extern FILE *bu_process_file_open(struct bu_process *pinfo, bu_process_io_t d);


/**
 * @brief Close any FILE pointers internally opened via bu_process_file_open().
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] d - channel (BU_PROCESS_STDIN, BU_PROCESS_STDOUT, BU_PROCESS_STDERR)
 */
BU_EXPORT extern void bu_process_file_close(struct bu_process *pinfo, bu_process_io_t d);


/**
 * @brief Retrieve the file descriptor to the I/O channel associated with the process.
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] d - channel (BU_PROCESS_STDIN, BU_PROCESS_STDOUT, BU_PROCESS_STDERR)
 *
 * @return
 * file descriptor
 *
 * @note For Windows cases where HANDLE is needed, use _get_osfhandle
 */
BU_EXPORT int bu_process_fileno(struct bu_process *pinfo, bu_process_io_t d);


/**
 * @brief Return the pid of the subprocess.
 *
 * @param[in] pinfo - bu_process structure of interest
 *
 * @return
 * process ID
 */
BU_EXPORT int bu_process_pid(struct bu_process *pinfo);


/**
 * Reports one or both of the command string and the argv array
 * used to execute the process.
 *
 * The bu_process container owns all strings for both cmd and argv -
 * for the caller they are read-only.
 *
 * If either cmd or argv are NULL they will be skipped - if the
 * caller only wants one of these outputs the other argument can
 * be set to NULL.
 *
 * @param[in] pinfo - the bu_process structure of interest
 * @param[out] cmd - pointer to the cmd string used to launch pinfo
 * @param[out] argv - pointer to the argv array used to launch pinfo
 *
 * @return
 * the corresponding argc count for pinfo's argv array.
 */
BU_EXPORT int bu_process_args_n(struct bu_process *pinfo, const char **cmd, const char * const **argv);


/**
 * @brief Return the process ID of the calling process
 *
 * @return
 * process ID
 */
BU_EXPORT extern int bu_pid(void);


/**
 * @brief determine whether process is still running using its ID
 *
 * @param[in] pid - process ID of interest
 *
 * @return
 * 1 if alive, else 0
 */
BU_EXPORT extern int bu_pid_alive(int pid);


/**
 * @brief terminate a given process and any children.
 *
 * @param[in] pid - process ID of interest
 *
 * @return
 * returns truthfully whether the process could be killed
 */
BU_EXPORT extern int bu_pid_terminate(int pid);


/**
 * @brief detect whether or not a program is being run in interactive mode
 *
 * @return
 * 1 if interactive, else 0
 */
BU_EXPORT extern int bu_interactive(void);


/** @name Deprecated process APIs
 * @{ */

/** @deprecated Use bu_process_create(). */
DEPRECATED BU_EXPORT extern void bu_process_exec(struct bu_process **info, const char *cmd, int argc, const char **argv, int out_eql_err, int hide_window);

/** @deprecated Use bu_process_wait_n(). */
DEPRECATED BU_EXPORT extern int bu_process_wait(int *aborted, struct bu_process *pinfo, int wtime);

/** @deprecated Use bu_process_read_n(). */
DEPRECATED BU_EXPORT extern int bu_process_read(char *buff, int *count, struct bu_process *pinfo, bu_process_io_t d, int n);

/** @deprecated Use bu_process_file_open(). */
DEPRECATED BU_EXPORT extern FILE *bu_process_open(struct bu_process *pinfo, bu_process_io_t d);

/** @deprecated Use bu_process_file_close(). */
DEPRECATED BU_EXPORT extern void bu_process_close(struct bu_process *pinfo, bu_process_io_t d);

/** @deprecated Use bu_process_args_n(). */
DEPRECATED BU_EXPORT int bu_process_args(const char **cmd, const char * const **argv, struct bu_process *pinfo);

/** @deprecated Use bu_pid(). */
DEPRECATED BU_EXPORT extern int bu_process_id(void);

/** @deprecated Use bu_pid_terminate(). */
DEPRECATED BU_EXPORT extern int bu_terminate(int process);

/** @} */

/** @} */

__END_DECLS

#endif  /* BU_PROCESS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
