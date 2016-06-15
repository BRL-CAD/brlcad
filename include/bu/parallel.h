/*                      P A R A L L E L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/**  @defgroup parallel  Parallel Processing */
/**   @defgroup thread Multithreading */

/** @file parallel.h
 *
 */
#ifndef BU_PARALLEL_H
#define BU_PARALLEL_H

#include "common.h"

#include <setjmp.h> /* for bu_setjmp */

#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup thread */
/** @{ */

/**
 * MAX_PSW - The maximum number of processors that can be expected on
 * this hardware.  Used to allocate application-specific per-processor
 * tables at compile-time and represent a hard limit on the number of
 * processors/threads that may be spawned. The actual number of
 * available processors is found at runtime by calling bu_avail_cpus()
 */
#define MAX_PSW 1024

/** @file libbu/parallel.c
 *
 * subroutine to determine if we are multi-threaded
 *
 * This subroutine is separated off from parallel.c so that bu_bomb()
 * and others can call it, without causing either parallel.c or
 * semaphore.c to get referenced and thus causing the loader to drag
 * in all the parallel processing stuff from the vendor library.
 *
 */

/**
 * A clean way for bu_bomb() to tell if this is a parallel
 * application.  If bu_parallel() is active, this routine will return
 * non-zero.
 */
BU_EXPORT extern int bu_is_parallel(void);

/**
 * Used by bu_bomb() to help terminate parallel threads,
 * without dragging in the whole parallel library if it isn't being used.
 */
BU_EXPORT extern void bu_kill_parallel(void);

/**
 * returns the CPU number of the current bu_parallel() invoked thread.
 */
BU_EXPORT extern int bu_parallel_id(void);

/** @file libbu/kill.c
 *
 * terminate a given process.
 *
 */

/**
 * terminate a given process.
 *
 * returns truthfully whether the process could be killed.
 */
BU_EXPORT extern int bu_terminate(int process);

/** @file libbu/process.c
 *
 * process management routines
 *
 */

/**
 * returns the process ID of the calling process
 */
BU_EXPORT extern int bu_process_id(void);

/** @file libbu/parallel.c
 *
 * routines for parallel processing
 *
 * Machine-specific routines for portable parallel processing.
 *
 */

/**
 * Without knowing what the current UNIX "nice" value is, change to a
 * new absolute "nice" value.  (The system routine makes a relative
 * change).
 */
BU_EXPORT extern void bu_nice_set(int newnice);

/**
 * Return the maximum number of physical CPUs that are considered to
 * be available to this process now.
 */
BU_EXPORT extern size_t bu_avail_cpus(void);

/**
 * Create 'ncpu' copies of function 'func' all running in parallel,
 * with private stack areas.  Locking and work dispatching are handled
 * by 'func' using a "self-dispatching" paradigm.
 *
 * 'func' is called with one parameter, its thread number.  Threads
 * are given increasing numbers, starting with zero.  Processes may
 * also call bu_parallel_id() to obtain their thread number.
 *
 * Threads created with bu_parallel() automatically set CPU affinity
 * where available for improved performance.  This behavior can be
 * disabled at runtime by setting the LIBBU_AFFINITY environment
 * variable to 0.
 *
 * This function will not return control until all invocations of the
 * subroutine are finished.
 */
BU_EXPORT extern void bu_parallel(void (*func)(int ncpu, void *arg), int ncpu, void *arg);

/** @file libbu/semaphore.c
 *
 * semaphore implementation
 *
 * Machine-specific routines for parallel processing.  Primarily for
 * handling semaphores to protect critical sections of code.
 *
 * The new paradigm: semaphores are referred to, not by a pointer, but
 * by a small integer.  This module is now responsible for obtaining
 * whatever storage is needed to implement each semaphore.
 *
 * Note that these routines can't use bu_log() for error logging,
 * because bu_log() acquires semaphore #0 (BU_SEM_SYSCALL).
 */

/*
 * Section for manifest constants for bu_semaphore_acquire()
 */
#define BU_SEM_SYSCALL 0
#define BU_SEM_LISTS 1
#define BU_SEM_BN_NOISE 2
#define BU_SEM_MAPPEDFILE 3
#define BU_SEM_THREAD 4
#define BU_SEM_MALLOC 5
#define BU_SEM_DATETIME 6
#define BU_SEM_LAST (BU_SEM_DATETIME+1) /* allocate this many for LIBBU+LIBBN */
/*
 * Automatic restart capability in bu_bomb().  The return from
 * BU_SETJUMP is the return from the setjmp().  It is 0 on the first
 * pass through, and non-zero when re-entered via a longjmp() from
 * bu_bomb().  This is only safe to use in non-parallel applications.
 */
#define BU_SETJUMP setjmp((bu_setjmp_valid=1, bu_jmpbuf))
#define BU_UNSETJUMP (bu_setjmp_valid=0)
/* These are global because BU_SETJUMP must be macro.  Please don't touch. */
BU_EXPORT extern int bu_setjmp_valid;           /* !0 = bu_jmpbuf is valid */
BU_EXPORT extern jmp_buf bu_jmpbuf;                     /* for BU_SETJUMP() */


/**
 * Prepare 'nsemaphores' independent critical section semaphores.  Die
 * on error.
 *
 * Takes the place of 'n' separate calls to old RES_INIT().  Start by
 * allocating array of "struct bu_semaphores", which has been arranged
 * to contain whatever this system needs.
 *
 */
BU_EXPORT extern void bu_semaphore_init(unsigned int nsemaphores);

/**
 * Release all initialized semaphores and any associated memory.
 */
BU_EXPORT extern void bu_semaphore_free(void);

/**
 * Prepare 'nsemaphores' independent critical section semaphores.  Die
 * on error.
 */
BU_EXPORT extern void bu_semaphore_reinit(unsigned int nsemaphores);

BU_EXPORT extern void bu_semaphore_acquire(unsigned int i);

BU_EXPORT extern void bu_semaphore_release(unsigned int i);

/** @} */

__END_DECLS

#endif  /* BU_PARALLEL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
