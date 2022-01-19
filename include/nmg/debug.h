/*                        N M G / D E B U G . H
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
/** @addtogroup libnmg
 *
 * Debugging flags for NMG
 *
 */
/** @{ */
/** @file include/nmg/debug.h */

#ifndef NMG_DEBUG_H
#define NMG_DEBUG_H

#include "common.h"

#include "nmg/defines.h"

#define NMG_DEBUG_PL_ANIM     0x00000001	/* 1 mged: animated evaluation */
#define NMG_DEBUG_PL_SLOW     0x00000002	/* 2 mged: add delays to animation */
#define NMG_DEBUG_GRAPHCL     0x00000004	/* 3 mged: graphic classification */
#define NMG_DEBUG_PL_LOOP     0x00000008	/* 4 loop class (needs GRAPHCL) */
#define NMG_DEBUG_PLOTEM      0x00000010	/* 5 make plots in debugged routines (needs other flags set too) */
#define NMG_DEBUG_POLYSECT    0x00000020	/* 6 nmg_inter: face intersection */
#define NMG_DEBUG_VERIFY      0x00000040	/* 7 nmg_vshell() frequently, verify health */
#define NMG_DEBUG_BOOL        0x00000080	/* 8 nmg_bool:  */
#define NMG_DEBUG_CLASSIFY    0x00000100	/* 9 nmg_class: */
#define NMG_DEBUG_BOOLEVAL    0x00000200	/* 10 nmg_eval: what to retain */
#define NMG_DEBUG_BASIC       0x00000400	/* 11 nmg_mk.c and nmg_mod.c routines */
#define NMG_DEBUG_MESH        0x00000800	/* 12 nmg_mesh: describe edge search */
#define NMG_DEBUG_MESH_EU     0x00001000	/* 13 nmg_mesh: list edges meshed */
#define NMG_DEBUG_POLYTO      0x00002000	/* 14 nmg_misc: polytonmg */
#define NMG_DEBUG_LABEL_PTS   0x00004000	/* 15 label points in plot files */
#define NMG_DEBUG_UNUSED1     0x00008000	/* 16 UNUSED */
#define NMG_DEBUG_NMGRT       0x00010000	/* 17 ray tracing */
#define NMG_DEBUG_FINDEU      0x00020000	/* 18 nmg_mod: nmg_findeu() */
#define NMG_DEBUG_CMFACE      0x00040000	/* 19 nmg_mod: nmg_cmface() */
#define NMG_DEBUG_CUTLOOP     0x00080000	/* 20 nmg_mod: nmg_cut_loop */
#define NMG_DEBUG_VU_SORT     0x00100000	/* 21 nmg_fcut: coincident vu sort */
#define NMG_DEBUG_FCUT        0x00200000	/* 22 nmg_fcut: face cutter */
#define NMG_DEBUG_RT_SEGS     0x00400000	/* 23 nmg_rt_segs: */
#define NMG_DEBUG_RT_ISECT    0x00800000	/* 24 nmg_rt_isect: */
#define NMG_DEBUG_TRI         0x01000000	/* 25 nmg_tri */
#define NMG_DEBUG_PNT_FU      0x02000000	/* 26 nmg_pt_fu */
#define NMG_DEBUG_MANIF       0x04000000	/* 27 nmg_manif */
#define NMG_DEBUG_UNUSED2     0x08000000	/* 28 UNUSED */
#define NMG_DEBUG_UNUSED3     0x10000000	/* 29 UNUSED */
#define NMG_DEBUG_UNUSED4     0x20000000	/* 30 UNUSED */
#define NMG_DEBUG_UNUSED5     0x40000000	/* 31 UNUSED */
#define NMG_DEBUG_UNUSED6     0x80000000	/* 32 UNUSED */
#define NMG_DEBUG_FORMAT  "\020" /* print hex */ \
    "\040UNUSED6" \
    "\037UNUSED5" \
    "\036UNUSED4" \
    "\035UNUSED3" \
    "\034UNUSED2" \
    "\033MANIF" \
    "\032PT_FU" \
    "\031TRI" \
    "\030RT_ISECT" \
    "\027RT_SEGS" \
    "\026FCUT" \
    "\025VU_SORT" \
    "\024CUTLOOP" \
    "\023CMFACE" \
    "\022FINDEU" \
    "\021NMGRT" \
    "\020UNUSED1" \
    "\017LABEL_PTS" \
    "\016POLYTO" \
    "\015MESH_EU" \
    "\014MESH" \
    "\013BASIC" \
    "\012BOOLEVAL" \
    "\011CLASSIFY" \
    "\010BOOL" \
    "\7VERIFY" \
    "\6POLYSECT" \
    "\5PLOTEM" \
    "\4PL_LOOP" \
    "\3GRAPHCL" \
    "\2PL_SLOW" \
    "\1PL_ANIM"

#endif /* NMG_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
