/*
 *			C O M P A T 4 . H
 *
 *  A compatability header file for LIBBU which provides
 *  BRL-CAD Release 4.4 style rt_xxx() names for the new bu_xxx() routines.
 *  So that users don't have to struggle with upgrading their source code.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  $Header$
 */

#ifndef SEEN_COMPAT4_H
#define SEEN_COMPAT4_H seen
#ifdef __cplusplus
extern "C" {
#endif

/* The ordering of these sections should track the order of externs in bu.h */

/* badmagic.c */
#define RT_CKMAG	BU_CKMAG
#define rt_badmagic	bu_badmagic

/* bomb.c */
#define rt_bomb(_s)	{ if(rt_g.debug || rt_g.NMG_debug) \
				bu_debug |= BU_DEBUG_COREDUMP; \
			bu_bomb(_s);}
#define RT_SETJUMP	BU_SETJUMP
#define RT_UNSETJUMP	BU_UNSETJUMP

/* log.c */
#define rt_log		bu_log
#define rt_add_hook	bu_add_hook
#define rt_delete_hook	bu_delete_hook
#define rt_putchar	bu_putchar
#define rt_log		bu_log
#define rt_flog		bu_flog

/* magic.c */
#define rt_identify_magic	bu_identify_magic

/* malloc.c -- Note that some types have changed from char* to genptr_t */
#define rt_malloc(_cnt,_str)	((char *)bu_malloc(_cnt,_str))
#define rt_free(_ptr,_str)	bu_free((genptr_t)(_ptr),_str)
#define rt_realloc(_p,_ct,_str)	((char *)bu_realloc(_p,_ct,_str))
#define rt_calloc(_n,_sz,_str)	((char *)bu_calloc(_n,_sz,_str))
#define rt_prmem		bu_prmem
#define rt_strdup		bu_strdup
#define rt_byte_roundup		bu_malloc_len_roundup
#define rt_ck_malloc_ptr(_p,_s)	bu_ck_malloc_ptr((genptr_t)_p,_s)
#define rt_mem_barriercheck	bu_mem_barriercheck

/* parallel.c */
#define rt_pri_set	bu_nice_set
#define rt_cpuget	bu_cpulimit_get
#define rt_cpuset	bu_cpulimit_set
#define rt_avail_cpus	bu_avail_cpus
#define rt_parallel	bu_parallel

/* printb.c */
#define rt_printb	bu_printb

/*
 * semaphore.c
 *
 * Backwards compatability for existing Release 4.4 LIBRT applications.
 *
 * RES_ACQUIRE( &rt_g.res_syscall )   becomes  bu_semaphore_acquire( BU_SEM_SYSCALL )
 * RES_RELEASE( &rt_g.res_syscall )   becomes  bu_semaphore_release( BU_SEM_SYSCALL )
 * 
 * res_syscall is 0		RT_SEM_SYSCALL == BU_SEM_SYSCALL
 * res_worker  is 1		RT_SEM_WORKER
 * res_stats   is 2		RT_SEM_STATS
 * res_results is 3		RT_SEM_RESULTS
 * res_model   is 4		RT_SEM_MODEL
 */
#undef RES_INIT		/* machine.h may have defined these */
#undef RES_ACQUIRE
#undef RES_RELEASE
#define RES_INIT(p) 	bu_semaphore_init(5)
#define RES_ACQUIRE(p)	bu_semaphore_acquire( (p) - (&rt_g.res_syscall) )
#define RES_RELEASE(p)	bu_semaphore_release( (p) - (&rt_g.res_syscall) )

#define RT_SEM_SYSCALL	BU_SEM_SYSCALL		/* res_syscall */
#define RT_SEM_WORKER	1			/* res_worker */
#define RT_SEM_STATS	2			/* res_stats */
#define RT_SEM_RESULTS	3			/* res_results */
#define RT_SEM_MODEL	4			/* res_model */

/* vls.c */
#define rt_vls			bu_vls		/* struct rt_vls */
#define RT_VLS_CHECK(_vp)	BU_CK_VLS(_vp)
#define RT_VLS_ADDR		bu_vls_addr
#define RT_VLS_INIT		bu_vls_init

#ifdef __cplusplus
}
#endif
#endif /* SEEN_COMPAT4_H */
