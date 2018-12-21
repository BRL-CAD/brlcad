/*                      P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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

#include "bu/defines.h"

__BEGIN_DECLS

/**
 * returns the process ID of the calling process
 */
BU_EXPORT extern int bu_process_id(void);

#if 0
/** @brief Wrappers for pipe communication mechanisms */
struct bu_pipe;

BU_EXPORT extern int bu_pipe_create(struct bu_pipe **p, int ninherit);

/* Frees memory associated with struct bu_pipe - does NOT close pipes */
BU_EXPORT extern void bu_pipe_destroy(struct bu_pipe *p);

/* Close file id 0 or 1 in the specified pipe.  id values other than
 * 0 or 1 are ignored. */
BU_EXPORT void bu_pipe_close(struct bu_pipe *p, int id);

/* TODO - ged_run_rt info should be generic - turn into a container */
struct bu_process_info;

/** @brief Wrapper for executing a sub-process (execvp and CreateProcess) */
BU_EXPORT void bu_exec(struct bu_process_info **i, const char *cmd, int argc, const char **argv, struct bu_pipe *pin, struct bu_pipe *perr);
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
