/*                      P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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

typedef enum {
    BU_PROCESS_STDIN,
    BU_PROCESS_STDOUT,
    BU_PROCESS_STDERR
} bu_process_io_t;

typedef enum {
    BU_PROCESS_DEFAULT = 0x0,	    // default creation: equiv to (bu_process_create_opts)0
    BU_PROCESS_OUT_EQ_ERR = 0x1,    // stderr output is written to stdout
    BU_PROCESS_HIDE_WINDOW = 0x2,   // hide creation window, if process would normall spawn one
} bu_process_create_opts;

/**
 * @brief Wrapper for executing a sub-process
 *
 * @deprecated use bu_process_create() instead.
 *
 * @note FIXME: eliminate the last two options so all callers are not
 * exposed to parameters not relevant to them.
 */
DEPRECATED BU_EXPORT extern void bu_process_exec(struct bu_process **info, const char *cmd, int argc, const char **argv, int out_eql_err, int hide_window);
/**
 * @brief Wrapper for creating a sub-process. Allocates bu_process and starts process
 *
 * @param[out] pinfo - bu_process struct to be created
 * @param[in] argv - array of command line arguments to executed. Last element MUST be NULL
 * @param[in] opts - creation options
 *
 * @return
 * 0 on successful process start; Otherwise, platform specific error code is returned
 */
BU_EXPORT extern int bu_process_create(struct bu_process **pinfo, const char **argv, bu_process_create_opts opts);


/**
 * @brief wait for a sub-process to complete, release all process
 * allocations, and release the process itself.
 *
 * @deprecated use bu_process_wait_n instead.
 *
 * @note FIXME: 'aborted' argument may be unnecessary (could make function
 * provide return value of the process waited for).  wtime
 * undocumented.
 */
 DEPRECATED BU_EXPORT extern int bu_process_wait(int *aborted, struct bu_process *pinfo, int wtime);
/**
 * @brief wait for a sub-process to complete, release all process
 * allocations, and release the process itself.
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] wtime - maximum wait time (in ms) before forcibly stopping process. NOTE: 0 is treated as INFINITE
 *
 * @return
 * 0 on success; ERROR_PROCESS_ABORTED for aborted process; Otherwise, platform specific exit status
 */
 BU_EXPORT extern int bu_process_wait_n(struct bu_process *pinfo, int wtime);


/**
 * @brief determine whether process is still running
 *
 * @param[in] pinfo - bu_process structure of interest
 *
 * @return
 * 1 if alive, else 0
 */
BU_EXPORT extern int bu_process_alive(struct bu_process* pinfo);


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
 * Read up to n bytes into buff from a process's specified output
 * channel (fd == 1 for output, fd == 2 for err).
 *
 * FIXME: arg ordering and input/output grouping is wrong.  partially
 * redundant with bu_process_fd() and/or bu_process_open().
 */
BU_EXPORT extern int bu_process_read(char *buff, int *count, struct bu_process *pinfo, bu_process_io_t d, int n);
/**
 * @brief Read from a process's specified output channel
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] d - channel (BU_PROCESS_STDOUT, BU_PROCESS_STDERR)
 * @param[in] n - max number of bytes to be read
 * @param[out] buff - data read from channel
 * @param[out] count - number of bytes actually read into buff
 *
 * @return
 * returns 0 on successful read
 */
// TODO: deprecate _read, replace callers
BU_EXPORT extern int bu_process_read_n(struct bu_process *pinfo, bu_process_io_t d, int n, char *buff, int *count);


/**
 * @brief Open and return a FILE pointer associated with the specified channel.
 *
 * Input will be opened write, output and error will be opened
 * read.
 *
 * Caller should not close these FILE pointers directly.  Call
 * bu_process_close() instead.
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] d - channel (BU_PROCESS_STDIN, BU_PROCESS_STDOUT, BU_PROCESS_STDERR)
 *
 * @return
 * FILE pointer for specified channel
 */
// TODO: rename to something more indicitive -> bu_process_file_open?
BU_EXPORT extern FILE *bu_process_open(struct bu_process *pinfo, bu_process_io_t d);


/**
 * @brief Close any FILE pointers internally opened via bu_process_open().
 *
 * @param[in] pinfo - bu_process structure of interest
 * @param[in] d - channel (BU_PROCESS_STDIN, BU_PROCESS_STDOUT, BU_PROCESS_STDERR)
 */
// TODO: parallel name change from process_open
BU_EXPORT extern void bu_process_close(struct bu_process *pinfo, bu_process_io_t d);


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
 * @param[out] cmd - pointer to the cmd string used to launch pinfo
 * @param[out] argv - pointer to the argv array used to launch pinfo
 * @param[in] pinfo - the bu_process structure of interest
 *
 * @return
 * the corresponding argc count for pinfo's argv array.
 */
BU_EXPORT int bu_process_args(const char **cmd, const char * const **argv, struct bu_process *pinfo);


/**
 * @brief Return the process ID of the calling process
 *
 * @return
 * process ID
 */
BU_EXPORT extern int bu_process_id(void);
// TODO: dep process_id
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
 * @param[in] process - process ID of interest
 *
 * @return
 * returns truthfully whether the process could be killed
 */
BU_EXPORT extern int bu_terminate(int process);
// TODO: dep bu_terminate
BU_EXPORT extern int bu_pid_terminate(int pid);


/**
 * @brief detect whether or not a program is being run in interactive mode
 *
 * @return
 * 1 if interactive, else 0
 */
BU_EXPORT extern int bu_interactive(void);

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
