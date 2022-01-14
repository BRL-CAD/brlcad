/*                      P A R A L L E L . H
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

#ifndef BU_PARALLEL_H
#define BU_PARALLEL_H

#include "common.h"

#include <setjmp.h> /* for bu_setjmp */

#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_parallel
 * @brief
 * Thread based parallelism routines.
 */
/** @{ */

/**
 * MAX_PSW - The maximum number of processors that can be expected on
 * this hardware.  Used to allocate application-specific per-processor
 * tables at compile-time and represent a hard limit on the number of
 * processors/threads that may be spawned. The actual number of
 * available processors is found at runtime by calling bu_avail_cpus()
 */
#define MAX_PSW 1024

/**
 * @brief
 * subroutine to determine if we are multi-threaded
 *
 * This subroutine is separated off from parallel.c so that bu_bomb()
 * and others can call it, without causing either parallel.c or
 * semaphore.c to get referenced and thus causing the loader to drag
 * in all the parallel processing stuff from the vendor library.
 *
 */

/**
 * This routine is DEPRECATED, do not use it.  If you need a means to
 * determine when an application is running bu_parallel(), please
 * report this to our developers.
 *
 * Previously, this was a library-stateful way for bu_bomb() to tell
 * if a parallel application is running.  This routine now simply
 * returns zero all the time, which permits BU_SETJUMP() error
 * handling during bu_bomb().
 */
DEPRECATED BU_EXPORT extern int bu_is_parallel(void);

/**
 * returns the CPU number of the current bu_parallel() invoked thread.
 */
BU_EXPORT extern int bu_parallel_id(void);

/**
 * @brief
 * process management routines
 */

/**
 * @brief
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
 * Create parallel threads of execution.
 *
 * This function creates (at most) 'ncpu' copies of function 'func'
 * all running in parallel, passing 'data' to each invocation.
 * Specifying ncpu=0 will specify automatic parallelization, invoking
 * parallel threads as cores become available.  This is particularly
 * useful during recursive invocations where the ncpu core count is
 * limited by the parent context.
 *
 * Locking and work dispatching are handled by 'func' using a
 * "self-dispatching" paradigm.  This means you must manually protect
 * shared data structures, e.g., via BU_SEMAPHORE_ACQUIRE().
 * Lock-free execution is often possible by creating data containers
 * with MAX_PSW elements as bu_parallel will never execute more than
 * that many threads of execution.
 *
 * All invocations of the specified 'func' callback function are
 * passed two parameters: 1) it's assigned thread number and 2) a
 * shared 'data' pointer for application use.  Threads are assigned
 * increasing numbers, starting with zero.  Processes may also call
 * bu_parallel_id() to obtain their thread number.
 *
 * Threads created with bu_parallel() may specify utilization of
 * affinity locking to keep threads on a given physical CPU core.
 * This behavior can be enabled at runtime by setting the environment
 * variable LIBBU_AFFINITY=1.  Note that this option may increase or
 * even decrease performance, particularly on platforms with advanced
 * scheduling, so testing is recommended.
 *
 * This function will not return control until all invocations of the
 * subroutine are finished.
 *
 * In following is a working stand-alone example demonstrating how to
 * call the bu_parallel() interface.
 *
 * @code
 * void shoot_cells_in_series(int width, int height) {
 *   int i, j;
 *   for (i=0; i<height; i++) {
 *     for (j=0; j<width; j++) {
 *       printf("Shooting cell (%d, %d) on CPU %d\n", i, j, bu_parallel_id());
 *     }
 *   }
 * }
 *
 * void shoot_row_per_thread(int cpu, void *mydata) {
 *   int i, j, width;
 *   width = *(int *)mydata;
 *   for (i=0; i<width; i++) {
 *     printf("Shooting cell (%d, %d) on CPU %d\n", i, cpu, bu_parallel_id());
 *   }
 * }
 *
 * void shoot_cells_in_parallel(int width, int height) {
 *   bu_parallel(shoot_row_per_thread, height, &width);
 *   // we don't reach here until all threads complete
 * }
 *
 * int main(int ac, char *av[]) {
 *   int width = 4, height = 4;
 *   printf("\nShooting cells one at a time, 4x4 grid:\n");
 *   shoot_cells_in_series(width, height);
 *   printf("\nShooting cells in parallel with 4 threads, one per row:\n");
 *   shoot_cells_in_parallel(width, height);
 *   return 0;
 * }
 * @endcode
 */
BU_EXPORT extern void bu_parallel(void (*func)(int func_cpu_id, void *func_data), size_t ncpu, void *data);


/**
 * @brief
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

/**
 *
 */
BU_EXPORT extern int bu_semaphore_register(const char *name);


/**
 * emaphores available for both library and application
 * use.
 *
 */
#define BU_SEMAPHORE_DEFINE(x) x = bu_semaphore_register(CPP_STR(x))

/**
 * This semaphore is intended for short-lived protection.
 *
 * It is provided for both library and application use, code that
 * doesn't call into a BRL-CAD library.
 */
BU_EXPORT extern int BU_SEM_GENERAL;

/**
 * This semaphore is intended to protect general system calls.
 *
 * It is provided for both library and application use, code that
 * doesn't call into a BRL-CAD library.
 */
BU_EXPORT extern int BU_SEM_SYSCALL;

/**
 * FIXME: this one shouldn't need to be global.
 */
BU_EXPORT extern int BU_SEM_MAPPEDFILE;


/*
 * Automatic restart capability in bu_bomb().  The return from
 * BU_SETJUMP is the return from the setjmp().  It is 0 on the first
 * pass through, and non-zero when re-entered via a longjmp() from
 * bu_bomb().  This is only safe to use in non-parallel applications.
 */
#define BU_SETJUMP setjmp((bu_setjmp_valid[bu_parallel_id()]=1, bu_jmpbuf[bu_parallel_id()]))
#define BU_UNSETJUMP (bu_setjmp_valid[bu_parallel_id()]=0)

/* These are global because BU_SETJUMP must be macro.  Please don't touch. */
BU_EXPORT extern int bu_setjmp_valid[MAX_PSW]; /* !0 = bu_jmpbuf is valid */
BU_EXPORT extern jmp_buf bu_jmpbuf[MAX_PSW];   /* for BU_SETJUMP() */


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
 *
 * FIXME: per hacking, rename to bu_semaphore_clear()
 */
BU_EXPORT extern void bu_semaphore_free(void);

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
