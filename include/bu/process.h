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


#if 0
/* Wrappers for using subprocess execution */
struct bu_process;

/* Open and return FILE pointer associated with process input fd.  Caller should
 * not close this FILE pointer directly - call bu_process_close_input instead. */
BU_EXPORT extern FILE *bu_process_open_input(struct bu_process *pinfo);

/* Close input FILE pointer and manage internal state */
BU_EXPORT extern void bu_process_close_input(struct bu_process *pinfo);

/* Read n bytes from output channel associated with process. */
BU_EXPORT extern int bu_process_read(char *buff, int *count, struct bu_process *pinfo, int n);

/** @brief Wrapper for executing a sub-process (execvp and CreateProcess) */
BU_EXPORT extern void bu_process_exec(struct bu_process **info, const char *cmd, int argc, const char **argv);

/** @brief Wrapper for waiting on a sub-process to complete (wait or WaitForSingleObject) and
 * cleaning up pinfo.  After this call completes, pinfo will be freed. */
BU_EXPORT extern int bu_process_wait(struct bu_process *pinfo);
#endif

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
