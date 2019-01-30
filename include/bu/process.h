/*                      P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2019 United States Government as represented by
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

/** @ingroup process */
/** @{ */
/** @file include/bu/process.h */
/** @} */
#ifndef BU_PROCESS_H
#define BU_PROCESS_H

#include "common.h"

#include <stdio.h> /* FILE */
#include "bu/defines.h"

__BEGIN_DECLS

/**
 * returns the process ID of the calling process
 */
BU_EXPORT extern int bu_process_id(void);



/* Wrappers for using subprocess execution */
struct bu_process;

#define BU_PROCESS_STDIN 0
#define BU_PROCESS_STDOUT 1
#define BU_PROCESS_STDERR 2

/* Open and return FILE pointer associated with process input (0), output (1),
 * or error (2) fd.  Caller should not close these FILE pointers directly -
 * call bu_process_input_close instead. Input will be opened write, output
 * and error will be opened read. */
BU_EXPORT extern FILE *bu_process_open(struct bu_process *pinfo, int fd);

/* Close input FILE pointer and manage internal state */
BU_EXPORT extern void bu_process_close(struct bu_process *pinfo, int fd);

/* Retrieve the pointer to the input (0), output (1), or error (2) file
 * descriptor associated with the process.  To use this in calling code, the
 * caller must cast the supplied pointer to the file handle type of the
 * calling code's specific platform.
 */
BU_EXPORT void *bu_process_fd(struct bu_process *pinfo, int fd);

/* Return the pid of the subprocess. */
BU_EXPORT int bu_process_pid(struct bu_process *pinfo);

/* Returns argc and argv used to exec the subprocess - argc is returned as the
 * return integer, a pointer to the specified command in the first pointer, and
 * a pointer to the argv array in the second argument. If either cmd or argv are
 * NULL they will be skipped. */
BU_EXPORT int bu_process_args(const char **cmd, const char * const **argv, struct bu_process *pinfo);

/* Read n bytes from specified output channel associated with process (fd == 1 for output, fd == 2 for err). */
BU_EXPORT extern int bu_process_read(char *buff, int *count, struct bu_process *pinfo, int fd, int n);

/** @brief Wrapper for executing a sub-process (execvp and CreateProcess) */
BU_EXPORT extern void bu_process_exec(struct bu_process **info, const char *cmd, int argc, const char **argv, int out_eql_err, int hide_window);

/** @brief Wrapper for waiting on a sub-process to complete (wait or WaitForSingleObject) and
 * cleaning up pinfo.  After this call completes, pinfo will be freed. */
BU_EXPORT extern int bu_process_wait(int *aborted, struct bu_process *pinfo, int wtime);

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
