/*                       C O M P A T 4 . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup deprecated */
/*@{*/
/** @file compat4.h
 *@deprecated
 *  A compatability header file for LIBBU and LIBBN which provides
 *  BRL-CAD Release 4.4 style rt_xxx() names for the new bu_xxx() routines.
 *  So that users don't have to struggle with upgrading their source code.
 *
 *  @author
 *	Michael John Muuss
 *
 *  @par Source
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  $Header$
 */

#ifndef DONT_USE_COMPAT4_H	/* Define this to scrub out Release 4-isms */

#ifndef SEEN_COMPAT4_H
#define SEEN_COMPAT4_H seen

__BEGIN_DECLS

/* raytrace.h macro replacements */
#define GETSTRUCT	BU_GETSTRUCT
#define GETUNION	BU_GETUNION
#define RT_CKMAG	BU_CKMAG
#define RT_TOL_MAGIC	BN_TOL_MAGIC
#define RT_CK_TOL	BN_CK_TOL
#define RT_VECT_ARE_PARALLEL	BN_VECT_ARE_PARALLEL
#define RT_VECT_ARE_PERP	BN_VECT_ARE_PERP

/* The ordering of these sections should track the order of externs in bu.h */

/* badmagic.c */
#define rt_badmagic	bu_badmagic

#define RT_SETJUMP	BU_SETJUMP
#define RT_UNSETJUMP	BU_UNSETJUMP

/* const.c */
#define rt_pi	bn_pi
#define rt_twopi	bn_twopi
#define rt_halfpi	bn_halfpi
#define rt_quarterpi	bn_quarterpi
#define rt_invpi	bn_invpi
#define rt_inv2pi	bn_inv2pi
#define rt_inv4pi	bn_inv4pi
#define rt_inv255	bn_inv255

#define rt_degtorad	bn_degtorad
#define rt_radtodeg	bn_radtodeg

/* hist.c */
#define histogram		bu_hist		/* struct */
#define RT_HISTOGRAM_MAGIC	BU_HIST_MAGIC
#define RT_CK_HISTOGRAM		BU_CK_HIST
#define RT_HISTOGRAM_TALLY	BU_HIST_TALLY
#define rt_hist_free		bu_hist_free
#define rt_hist_init		bu_hist_init
#define rt_hist_range		bu_hist_range
#define rt_hist_pr		bu_hist_pr

/* list.c */
#define rt_list_len		bu_list_len
#define rt_list_reverse		bu_list_reverse
#define rt_list			bu_list		/* struct */
#define RT_LIST_HEAD_MAGIC	BU_LIST_HEAD_MAGIC
#define RT_LIST_NULL		BU_LIST_NULL
#define RT_LIST_INSERT		BU_LIST_INSERT
#define RT_LIST_APPEND		BU_LIST_APPEND
#define RT_LIST_DEQUEUE		BU_LIST_DEQUEUE
#define RT_LIST_PUSH		BU_LIST_PUSH
#define RT_LIST_POP		BU_LIST_POP
#define RT_LIST_INSERT_LIST	BU_LIST_INSERT_LIST
#define RT_LIST_APPEND_LIST	BU_LIST_APPEND_LIST
#define RT_LIST_IS_EMPTY	BU_LIST_IS_EMPTY
#define RT_LIST_NON_EMPTY	BU_LIST_NON_EMPTY
#define RT_LIST_NON_EMPTY_P	BU_LIST_NON_EMPTY_P
#define RT_LIST_IS_CLEAR	BU_LIST_IS_CLEAR
#define	RT_LIST_UNINITIALIZED	BU_LIST_UNINITIALIZED
#define RT_LIST_INIT		BU_LIST_INIT
#define RT_LIST_MAGIC_SET	BU_LIST_MAGIC_SET
#define RT_LIST_MAGIC_OK	BU_LIST_MAGIC_OK
#define RT_LIST_MAGIC_WRONG	BU_LIST_MAGIC_WRONG
#define RT_LIST_LAST		BU_LIST_LAST
#define RT_LIST_PREV		BU_LIST_PREV
#define RT_LIST_FIRST		BU_LIST_FIRST
#define RT_LIST_NEXT		BU_LIST_NEXT
#define RT_LIST_IS_HEAD		BU_LIST_IS_HEAD
#define RT_LIST_NOT_HEAD	BU_LIST_NOT_HEAD
#define RT_CK_LIST_HEAD		BU_CK_LIST_HEAD
#define RT_LIST_NEXT_IS_HEAD	BU_LIST_NEXT_IS_HEAD
#define RT_LIST_NEXT_NOT_HEAD	BU_LIST_NEXT_NOT_HEAD
#define RT_LIST_FOR		BU_LIST_FOR
#define	RT_LIST_FOR2		BU_LIST_FOR2
#define RT_LIST_WHILE		BU_LIST_WHILE
#define RT_LIST_FIRST_MAGIC	BU_LIST_FIRST_MAGIC
#define RT_LIST_LAST_MAGIC	BU_LIST_LAST_MAGIC
#define RT_LIST_PNEXT		BU_LIST_PNEXT
#define RT_LIST_PLAST		BU_LIST_PLAST
#define RT_LIST_PNEXT_PNEXT	BU_LIST_PNEXT_PNEXT
#define RT_LIST_PNEXT_PLAST	BU_LIST_PNEXT_PLAST
#define RT_LIST_PLAST_PNEXT	BU_LIST_PLAST_PNEXT
#define RT_LIST_PLAST_PLAST	BU_LIST_PLAST_PLAST
#define RT_LIST_PNEXT_CIRC	BU_LIST_PNEXT_CIRC
#define RT_LIST_PPREV_CIRC	BU_LIST_PPREV_CIRC
#define RT_LIST_PLAST_CIRC	BU_LIST_PPREV_CIRC
#define RT_LIST_MAIN_PTR	BU_LIST_MAIN_PTR

/* log.c */
/* Change variable rt_g.rtg_logindent to a call to bu_log_indent_delta() */
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

/* mappedfile.c */
#define rt_mapped_file		bu_mapped_file		/* struct */
#define rt_open_mapped_file	bu_open_mapped_file
#define rt_close_mapped_file	bu_close_mapped_file
#define RT_CK_MAPPED_FILE	BU_CK_MAPPED_FILE

/* mat.c */
#define rt_identity		bn_mat_identity		/* compat */
#define rt_mat_is_equal		bn_mat_is_equal		/* compat */
#define vtoh_move(_d,_s)	bn_vtoh_move(_d,_s)	/* compat */
#define htov_move(_d,_s)	bn_htov_move(_d,_s)	/* compat */
#define ae_vec(_az,_el,_vec)	bn_ae_vec(_az,_el,_vec)	/* compat */
#define eigen2x2(_val1,_val2,_vec1,_vec2,_a,_b,_c)	\
	bn_eigen2x2(_val1,_val2,_vec1,_vec2,_a,_b,_c)	/* compat */
#define vec_ortho(_d,_s)	bn_vec_ortho(_d,_s)	/* compat */
#define vec_perp(_d,_s)		bn_vec_perp(_d,_s)	/* compat */

#define mat_degtorad	bn_degtorad			/* const.c */
#define mat_radtodeg	bn_radtodeg			/* const.c */
#define	mat_identity	bn_mat_identity
#define mat_print	bn_mat_print
#define mat_atan2	bn_atan2
#define mat_zero	MAT_ZERO
#define mat_idn		MAT_IDN
#define mat_copy	MAT_COPY
#define mat_mul		bn_mat_mul
#define mat_mul2	bn_mat_mul2
#define matXvec		bn_matXvec
#define mat_inv		bn_mat_inv
#define mat_vtoh_move	bn_vtoh_move
#define mat_htov_move	bn_htov_move
#define mat_trn		bn_mat_trn
#define mat_ae		bn_mat_ae
#define mat_ae_vec	bn_ae_vec
#define mat_aet_vec	bn_aet_vec
#define mat_angles	bn_mat_angles
#define mat_eigen2x2	bn_eigen2x2
#define mat_vec_perp	bn_vec_perp
#define mat_fromto	bn_mat_fromto
#define mat_xrot	bn_mat_xrot
#define mat_yrot	bn_mat_yrot
#define mat_zrot	bn_mat_zrot
#define mat_lookat	bn_mat_lookat
#define mat_vec_ortho	bn_vec_ortho
#define mat_scale_about_pt	bn_mat_scale_about_pt
#define mat_xform_about_pt	bn_mat_xform_about_pt
#define mat_is_identity	bn_mat_is_identity
#define mat_arb_rot	bn_mat_arb_rot
#define mat_dup		bn_mat_dup
#define mat_is_equal	bn_mat_is_equal


/* msr.c */
#define msr_unif_init	bn_unif_init
#define msr_unif_long_fill	bn_unif_long_fill
#define msr_unif_double_fill	bn_unif_double_fill
#define msr_gauss_init	bn_gauss_init
#define msr_gauss_fill	bn_gauss_fill
#define msr_unif	bn_unif		/* structure */
#define msr_gauss	bn_gauss	/* structure */
#define MSR_CK_UNIF	BN_CK_UNIF
#define MSR_CK_GAUSS	BN_CK_GAUSS
#define MSR_UNIF_LONG	BN_UNIF_LONG
#define MSR_UNIF_DOUBLE	BN_UNIF_DOUBLE
#define MSR_UNIF_CIRCLE	BN_UNIF_CIRCLE
#define MSR_UNIF_SPHERE	BN_UNIF_SPHERE
#define MSR_GAUSS_DOUBLE BN_GAUSS_DOUBLE

/* noise.c */
#define noise_init	bn_noise_init
#define noise_perlin	bn_noise_perlin
#define noise_vec	bn_noise_vec
#define noise_fbm	bn_noise_fbm
#define noise_turb	bn_noise_turb

/* parallel.c */
#define rt_pri_set	bu_nice_set
#define rt_cpuget	bu_cpulimit_get
#define rt_cpuset	bu_cpulimit_set
#define rt_avail_cpus	bu_avail_cpus
#define rt_parallel	bu_parallel

/* parse.c */
#define rt_struct_export	bu_struct_export
#define rt_struct_import	bu_struct_import
#define rt_struct_put		bu_struct_put
#define rt_struct_get		bu_struct_get
#define rt_gshort		bu_gshort
#define rt_glong		bu_glong
#define rt_pshort		bu_pshort
#define rt_plong		bu_plong
#define rt_struct_buf		bu_struct_wrap_buf
#define rt_structparse		bu_struct_parse
#define structparse		bu_structparse		/* struct */

#define rt_vls_item_print( v, sp, b )	 bu_vls_struct_item( v, sp, b, ',' )
#define rt_vls_item_print_nc( v, sp, b ) bu_vls_struct_item( v, sp, b, ' ' )
#define rt_vls_name_print( v, sp, n, b ) \
	bu_vls_struct_item_named( v, sp, n, b, ',' )
#define rt_vls_name_print_nc( v, sp, n, b ) \
	bu_vls_struct_item_named( v, sp, n, b, ' ' )
#define rt_structprint		bu_struct_print
#define rt_vls_structprint	bu_vls_struct_print
#if !__STDC__ && !defined(offsetof)
# define offsetof(_t, _m)	(int)(&(((_t *)0)->_m))
#endif
#define offsetofarray		bu_offsetofarray
#define FUNC_NULL		BU_STRUCTPARSE_FUNC_NULL

#define rt_external		bu_external
#define RT_EXTERNAL_MAGIC	BU_EXTERNAL_MAGIC
#define RT_INIT_EXTERNAL	BU_INIT_EXTERNAL
#define RT_CK_EXTERNAL		BU_CK_EXTERNAL

/* Red-black tree package */
#define	rb_tree			bu_rb_tree
#define	RB_TREE_NULL		BU_RB_TREE_NULL
#define	rb_min			bu_rb_min
#define	rb_max			bu_rb_max
#define	rb_pred			bu_rb_pred
#define	rb_succ			bu_rb_succ
#define	rb_create		bu_rb_create
#define	rb_create1		bu_rb_create1
#define	rb_delete		bu_rb_delete
#define	rb_diagnose_tree	bu_rb_diagnose_tree
#define	rb_extreme		bu_rb_extreme
#define	rb_insert		bu_rb_insert
#define	rb_neighbor		bu_rb_neighbor
#define	rb_search		bu_rb_search
#define	rb_search1		bu_rb_search1
#define	rb_summarize_tree	bu_rb_summarize_tree
#define	rb_walk			bu_rb_walk
#define	rb_walk1		bu_rb_walk1

/* plane.c */
#define rt_dist_pt3_lseg3	bn_dist_pt3_lseg3
#define rt_3pts_collinear	bn_3pts_collinear
#define rt_pt3_pt3_equal	bn_pt3_pt3_equal
#define rt_dist_pt2_lseg2	bn_dist_pt2_lseg2
#define rt_isect_lseg3_lseg3	bn_isect_lseg3_lseg3
#define rt_isect_line3_line3	bn_isect_line3_line3
#define rt_2line3_colinear	bn_2line3_colinear
#define rt_isect_pt2_lseg2	bn_isect_pt2_lseg2
#define rt_isect_line2_lseg2	bn_isect_line2_lseg2
#define rt_isect_lseg2_lseg2	bn_isect_lseg2_lseg2
#define rt_isect_line2_line2	bn_isect_line2_line2
#define rt_dist_line_point	bn_dist_line3_pt3
#define	rt_dist_pt3_pt3		bn_dist_pt3_pt3
#define	rt_3pts_distinct	bn_3pts_distinct
#define	rt_mk_plane_3pts	bn_mk_plane_3pts
#define	rt_mkpoint_3planes	bn_mkpoint_3planes
#define	rt_isect_line3_plane	bn_isect_line3_plane
#define	rt_isect_2planes	bn_isect_2planes
#define	rt_isect_2lines		bn_isect_2lines
#define	rt_isect_line_lseg	bn_isect_line_lseg
#define	rt_dist_line3_pt3	bn_dist_line3_pt3
#define	rt_distsq_line3_pt3	bn_distsq_line3_pt3
#define	rt_dist_line_origin	bn_dist_line_origin
#define	rt_dist_line2_point2	bn_dist_line2_point2
#define	rt_distsq_line2_point2	bn_distsq_line2_point2
#define	rt_area_of_triangle	bn_area_of_triangle
#define	rt_isect_pt_lseg	bn_isect_pt_lseg
#define	rt_dist_pt_lseg		bn_dist_pt_lseg
#define	rt_rotate_bbox		bn_rotate_bbox
#define	rt_rotate_plane		bn_rotate_plane
#define	rt_coplanar		bn_coplanar
#define	rt_angle_measure	bn_angle_measure
#define	rt_dist_pt3_along_line3	bn_dist_pt3_along_line3
#define	rt_dist_pt2_along_line2	bn_dist_pt2_along_line2
#define	rt_between		bn_between


/* printb.c */
#define rt_printb		bu_printb

/* ptbl.c */
#define nmg_tbl			bu_ptbl		/* main subroutine */
#define nmg_ptbl		bu_ptbl		/* data structure */
#define TBL_INIT		BU_PTBL_INIT
#define TBL_INS			BU_PTBL_INS
#define TBL_LOC			BU_PTBL_LOC
#define TBL_FREE		BU_PTBL_FREE
#define TBL_RST			BU_PTBL_RST
#define TBL_CAT			BU_PTBL_CAT
#define TBL_RM			BU_PTBL_RM
#define TBL_INS_UNIQUE		BU_PTBL_INS_UNIQUE
#define TBL_ZERO		BU_PTBL_ZERO
#define NMG_CK_PTBL		BU_CK_PTBL
#define NMG_TBL_BASEADDR	BU_PTBL_BASEADDR
#define NMG_TBL_LASTADDR	BU_PTBL_LASTADDR
#define NMG_TBL_END		BU_PTBL_END
#define NMG_TBL_GET		BU_PTBL_GET

/* rand.c */
#define RT_RAND_TABSIZE BN_RAND_TABSIZE
#define RT_RANDSEED	BN_RANDSEED
#define RT_RANDOM	BN_RANDOM
#define rt_rand_table	bn_rand_table


/* raytrace.h */
#define rt_tol bn_tol


/*
 * semaphore.c
 *
 * Backwards compatability for existing Release 4.4 LIBRT applications.
 *  No #define compatability, edit source with "sed4" script for
 *  automatic upgrade.
 *
 * RES_ACQUIRE( &rt_g.res_syscall )   becomes  bu_semaphore_acquire( BU_SEM_SYSCALL )
 * RES_RELEASE( &rt_g.res_syscall )   becomes  bu_semaphore_release( BU_SEM_SYSCALL )
 *
 */
#undef RES_INIT		/* machine.h may have defined these */
#undef RES_ACQUIRE
#undef RES_RELEASE

/* vls.c */
#define rt_vls			bu_vls		/* struct rt_vls */
#define RT_VLS_CHECK(_vp)	BU_CK_VLS(_vp)
#define RT_VLS_ADDR		bu_vls_addr
#define RT_VLS_INIT		bu_vls_init
#define rt_vls_init		bu_vls_init
#define rt_vls_vlsinit		bu_vls_vlsinit
#define rt_vls_addr		bu_vls_addr
#define rt_vls_strdup		bu_vls_strdup
#define rt_vls_strgrab		bu_vls_strgrab
#define rt_vls_extend		bu_vls_extend
#define rt_vls_setlen		bu_vls_setlen
#define rt_vls_strlen		bu_vls_strlen
#define rt_vls_trunc		bu_vls_trunc
#define rt_vls_trunc2		bu_vls_trunc2
#define rt_vls_nibble		bu_vls_nibble
#define rt_vls_free		bu_vls_free
#define rt_vls_vlsfree		bu_vls_vlsfree
#define rt_vls_strcpy		bu_vls_strcpy
#define rt_vls_strncpy		bu_vls_strncpy
#define rt_vls_strcat		bu_vls_strcat
#define rt_vls_strncat		bu_vls_strncat
#define rt_vls_vlscat		bu_vls_vlscat
#define rt_vls_vlscatzap	bu_vls_vlscatzap
#define rt_vls_from_argv	bu_vls_from_argv
#define rt_vls_fwrite		bu_vls_fwrite
#define rt_vls_gets		bu_vls_gets
#define rt_vls_putc		bu_vls_putc
#define rt_vls_printf		bu_vls_printf

/*----------------------------------------------------------------------*/
/*
 *  Macros for LIBBN
 *  Again in source file order.
 */

/* complex.c */
#define complex			bn_complex_t	/* typedef */
#define	CxCopy			bn_cx_copy
#define	CxNeg			bn_cx_neg
#define	CxReal			bn_cx_real
#define	CxImag			bn_cx_imag
#define CxAdd			bn_cx_add
#define CxAmpl			bn_cx_ampl
#define CxAmplSq		bn_cx_amplsq
#define CxConj			bn_cx_conj
#define CxCons			bn_cx_cons
#define CxPhas			bn_cx_phas
#define CxScal			bn_cx_scal
#define CxSub			bn_cx_sub
#define CxMul			bn_cx_mul
#define CxMul2			bn_cx_mul2
#define CxDiv			bn_cx_div
#define CxSqrt( cp )		bn_cx_sqrt( cp, cp )

/* poly.c */
/* WARNING:  The argument order has been changed, outputs now on the left */
#define poly				bn_poly_t	/* typedef */
#define rt_poly_mul(a,b,prod)		bn_poly_mul(prod,a,b)
#define rt_poly_scale			bn_poly_scale
#define rt_poly_add(a,b,sum)		bn_poly_add(sum,a,b)
#define rt_poly_sub(a,b,diff)		bn_poly_sub(diff,a,b)
#define rt_poly_synthetic_division(dvdend,dvsor,quo,rem)	\
		bn_poly_synthetic_division(quo, rem, dvdend, dvsor)
#define rt_poly_quadratic_roots(p,root)	bn_poly_quadratic_roots(root,p)
#define rt_poly_cubic_roots(p,root)	bn_poly_cubic_roots(root,p)
#define rt_poly_quartic_roots(p,root)	bn_poly_quartic_roots(root,p)
#define rt_pr_poly			bn_pr_poly
#define rt_pr_roots			bn_pr_roots


/* units.c */
#define rt_units_conversion bu_units_conversion
#define rt_units_string bu_units_string
#define rt_mm_value bu_mm_value

/* vlist.c */
#define RT_VLIST_CHUNK	BN_VLIST_CHUNK
#define rt_vlist	bn_vlist
#define RT_VLIST_NULL	BN_VLIST_NULL
#define RT_VLIST_MAGIC	BN_VLIST_MAGIC
#define RT_CK_VLIST(_p) BN_CK_VLIST(_p)
#define RT_VLIST_LINE_MOVE	BN_VLIST_LINE_MOVE
#define RT_VLIST_LINE_DRAW	BN_VLIST_LINE_DRAW
#define RT_VLIST_POLY_START	BN_VLIST_POLY_START
#define RT_VLIST_POLY_MOVE	BN_VLIST_POLY_MOVE
#define RT_VLIST_POLY_DRAW	BN_VLIST_POLY_DRAW
#define RT_VLIST_POLY_END	BN_VLIST_POLY_END
#define RT_VLIST_POLY_VERTNORM	BN_VLIST_POLY_VERTNORM
#define rt_vlblock	bn_vlblock
#define RT_VLBLOCK_MAGIC	BN_VLBLOCK_MAGIC
#define RT_CK_VLBLOCK(_p)	BN_CK_VLBLOCK(_p)

/* font.c */
#define rt_vlist_3string( vhead, string, origin, rot, scale )	\
	bn_vlist_3string( vhead, &rt_g.rtg_vlfree, string, origin, rot, scale )
#define rt_vlist_2string( vhead, string, x, y, scale, theta )	\
	bn_vlist_2string( vhead, &rt_g.rtg_vlfree, string, x, y, scale, theta )

__END_DECLS

#endif /* SEEN_COMPAT4_H */

#endif /* DONT_USE_COMPAT4_H */
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

