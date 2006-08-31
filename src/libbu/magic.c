/*                         M A G I C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
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

/** \addtogroup magic */
/*@{*/
/** @file magic.c
 *  Routines involved with handling "magic numbers" used to identify
 *  various in-memory data structures.
 *
 *  The one ugly thing about this strategy is that every BRL-CAD
 *  library needs to have it's magic numbers registered here.
 *  XXX What is needed is an extension mechanism.
 *  It is a shame that C does not provide a wextern (weak extern) declaration.
 *
 *
 *  @author	Lee A. Butler
 *  @author	Michael John Muuss
 *
 *  @par Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 * @n	The U. S. Army Ballistic Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5066
 */

#ifndef lint
static const char RCSmagic[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "bu.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "msr.h"
#include "wdb.h"
#include "spm.h"
#include "../libbu/rb_internals.h"

/**
 *			B U _ I D E N T I F Y _ M A G I C
 *
 *  Given a number which has been found in the magic number field of
 *  a structure (which is typically the first entry),
 *  determine what kind of structure this magic number pertains to.
 *  This is called by the macro BU_CK_MAGIC() to provide a "hint"
 *  as to what sort of pointer error might have been made.
 */
const char *
bu_identify_magic(register long int magic)
{
	switch(magic)  {
	case 0:
		return "Zero_Magic_Number";
	default:
		return "Unknown_Magic";
	/*
	 *  tabdata.h
	 */
	case BN_TABLE_MAGIC:
		return "bn_table";
	case BN_TABDATA_MAGIC:
		return "bn_tabdata";
	/*
	 *  bu.h
	 */
	case BU_LIST_HEAD_MAGIC:
		return "bu_list";
	case BU_BITV_MAGIC:
		return "bu_bitv";
	case BU_HIST_MAGIC:
		return "bu_hist";
	case BU_PTBL_MAGIC:
		return "bu_ptbl";
	case BU_MAPPED_FILE_MAGIC:
		return "bu_mapped_file";
	case BU_AVS_MAGIC:
		return "bu_attribute_value_set";
	case BU_VLS_MAGIC:
		return "bu_vls";
	case BU_EXTERNAL_MAGIC:
		return "bu_external";
	case BU_COLOR_MAGIC:
		return "bu_color";
	case BU_RB_TREE_MAGIC:
		return "bu_rb_tree";

	/*
	 *  bn.h
	 */
	case BN_TOL_MAGIC:
		return "bn_tol";
	case BN_POLY_MAGIC:
		return "bn_poly";
	case BN_UNIF_MAGIC:
		return "bn_unif";
	case BN_GAUSS_MAGIC:
		return "bn_gauss";
	case BN_VLIST_MAGIC:
		return("bn_vlist");
	case BN_VLBLOCK_MAGIC:
		return("bn_vlblock");

	/*
	 *  nmg.h:  NMG magic numbers
	 */
	case NMG_MODEL_MAGIC:
		return("model");
	case NMG_REGION_MAGIC:
		return("region");
	case NMG_REGION_A_MAGIC:
		return("region_a");
	case NMG_SHELL_MAGIC:
		return("shell");
	case NMG_SHELL_A_MAGIC:
		return("shell_a");
	case NMG_FACE_MAGIC:
		return("face");
	case NMG_FACE_G_PLANE_MAGIC:
		return("face_g_plane");
	case NMG_FACE_G_SNURB_MAGIC:
		return("face_g_snurb");
	case NMG_FACEUSE_MAGIC:
		return("faceuse");
	case NMG_LOOP_MAGIC:
		return("loop");
	case NMG_LOOP_G_MAGIC:
		return("loop_g");
	case NMG_LOOPUSE_MAGIC:
		return("loopuse");
	case NMG_EDGE_MAGIC:
		return("edge");
	case NMG_EDGE_G_LSEG_MAGIC:
		return("edge_g_lseg");
	case NMG_EDGE_G_CNURB_MAGIC:
		return("edge_g_cnurb");
	case NMG_KNOT_VECTOR_MAGIC:
		return("knot_vector");
	case NMG_EDGEUSE_MAGIC:
		return("edgeuse");
	case NMG_EDGEUSE2_MAGIC:
		return("edgeuse2 [midway into edgeuse]");
	case NMG_VERTEX_MAGIC:
		return("vertex");
	case NMG_VERTEX_G_MAGIC:
		return("vertex_g");
	case NMG_VERTEXUSE_MAGIC:
		return("vertexuse");
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
		return("vertexuse_a_plane");
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
		return("vertexuse_a_cnurb");
	/*
	 *  raytrace.h
	 */
	case RT_TESS_TOL_MAGIC:
		return("rt_tess_tol");
	case RT_DB_INTERNAL_MAGIC:
		return("rt_db_internal");
	case RT_RAY_MAGIC:
		return "librt xray";
	case RT_HIT_MAGIC:
		return "librt hit";
	case RT_SEG_MAGIC:
		return("librt seg");
	case RT_SOLTAB_MAGIC:
		return("librt soltab");
	case RT_REGION_MAGIC:
		return("librt region");
	case PT_MAGIC:
		return("librt partition");
	case DBI_MAGIC:
		return("librt db_i");
	case RT_DIR_MAGIC:
		return "librt directory";
	case DB_FULL_PATH_MAGIC:
		return "librt db_full_path";
	case RT_CTS_MAGIC:
		return "librt combined_tree_state";
	case RT_TREE_MAGIC:
		return "librt union tree";
	case RT_WDB_MAGIC:
		return "rt_wdb";
	case ANIMATE_MAGIC:
		return("librt animate");
	case RESOURCE_MAGIC:
		return("librt resource");
	case PIXEL_EXT_MAGIC:
		return "librt pixel_ext";
	case RT_AP_MAGIC:
		return "librt application";
	case RTI_MAGIC:
		return("rt_i");
	case RT_FUNCTAB_MAGIC:
		return "rt_functab";
	case RT_HTBL_MAGIC:
		return "rt_htbl";

	/*
	 *  rtgeom.h
	 */
	case RT_TOR_INTERNAL_MAGIC:
		return("rt_tor_internal");
	case RT_TGC_INTERNAL_MAGIC:
		return("rt_tgc_internal");
	case RT_ELL_INTERNAL_MAGIC:
		return("rt_ell_internal");
	case RT_ARB_INTERNAL_MAGIC:
		return("rt_arb_internal");
	case RT_ARS_INTERNAL_MAGIC:
		return("rt_ars_internal");
	case RT_HALF_INTERNAL_MAGIC:
		return("rt_half_internal");
	case RT_PG_INTERNAL_MAGIC:
		return("rt_pg_internal");
	case RT_EBM_INTERNAL_MAGIC:
		return("rt_ebm_internal");
	case RT_VOL_INTERNAL_MAGIC:
		return("rt_vol_internal");
	case RT_ARBN_INTERNAL_MAGIC:
		return("rt_arbn_internal");
	case RT_PIPE_INTERNAL_MAGIC:
		return("rt_pipe_internal");
	case RT_PART_INTERNAL_MAGIC:
		return("rt_part_internal");
	/*
	 * wdb.h
	 */
	case WMEMBER_MAGIC:
		return("wdb.h wmember_magic");
	case WDB_PIPESEG_MAGIC:
		return("wdb.h wdb_pipeseg_magic");

	/*
	 * fb.h -- not a good idea to include for real. (lib dependency)
	 */
	case 0xfbfb00fb /* FB_MAGIC */:
		return("fb.h fb_magic");

	/*
	 *  spm.h
	 */
	case SPM_MAGIC:
		return "spm.h spm_map_t";

	/*
	 *  ../libbu/rb_internals.h
	 */
	case BU_RB_NODE_MAGIC:
		return "red-black node";
	case BU_RB_PKG_MAGIC:
		return "red-black package";

	}

}

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
