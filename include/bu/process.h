/*                      P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/**
 * returns the process ID of the calling process
 */
BU_EXPORT extern int bu_process_id(void);

/**
 * @brief terminate a given process and any children.
 *
 * returns truthfully whether the process could be killed.
 */
BU_EXPORT extern int bu_terminate(int process);


/* Wrappers for using subprocess execution */
struct bu_process;

typedef enum {
    BU_PROCESS_STDIN,
    BU_PROCESS_STDOUT,
    BU_PROCESS_STDERR
} bu_process_io_t;

/**
 * Open and return a FILE pointer associated with the specified file
 * descriptor for input (0), output (1), or error (2) respectively.
 *
 * Input will be opened write, output and error will be opened
 * read.
 *
 * Caller should not close these FILE pointers directly.  Call
 * bu_process_close() instead.
 *
 * FIXME: misnomer, this does not open a process.  Probably doesn't
 * need to exist; just call fdopen().
 */
BU_EXPORT extern FILE *bu_process_open(struct bu_process *pinfo, bu_process_io_t d);


/**
 * Close any FILE pointers internally opened via bu_process_open().
 *
 * FIXME: misnomer, this does not close a process.  Probably doesn't
 * need to exist; just call fclose().
 */
BU_EXPORT extern void bu_process_close(struct bu_process *pinfo, bu_process_io_t d);


/**
 * Retrieve the file descriptor to the input (BU_PROCESS_STDIN), output
 * (BU_PROCESS_STDOUT), or error (BU_PROCESS_STDERR) I/O channel associated
 * with the process.
 *
 * For Windows cases where HANDLE is needed, use _get_osfhandle
 */
BU_EXPORT int bu_process_fileno(struct bu_process *pinfo, bu_process_io_t d);


/**
 * Return the pid of the subprocess.
 *
 * FIXME: seemingly redundant or combinable with bu_process_id()
 * (perhaps make NULL be equivalent to the current process).
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
 * Read up to n bytes into buff from a process's specified output
 * channel (fd == 1 for output, fd == 2 for err).
 *
 * FIXME: arg ordering and input/output grouping is wrong.  partially
 * redundant with bu_process_fd() and/or bu_process_open().
 */
BU_EXPORT extern int bu_process_read(char *buff, int *count, struct bu_process *pinfo, bu_process_io_t d, int n);


/**
 * @brief Wrapper for executing a sub-process
 *
 * FIXME: eliminate the last two options so all callers are not
 * exposed to parameters not relevant to them.
 */
BU_EXPORT extern void bu_process_exec(struct bu_process **info, const char *cmd, int argc, const char **argv, int out_eql_err, int hide_window);


/**
 * @brief wait for a sub-process to complete, release all process
 * allocations, and release the process itself.
 *
 * FIXME: 'aborted' argument may be unnecessary (could make function
 * provide return value of the process waited for).  wtime
 * undocumented.
 */
 BU_EXPORT extern int bu_process_wait(int *aborted, struct bu_process *pinfo, int wtime);

/**
 * @brief detect whether or not a program is being run in interactive mode
 *
 * Returns 1 if interactive, else 0
 */
BU_EXPORT extern int bu_interactive();

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
