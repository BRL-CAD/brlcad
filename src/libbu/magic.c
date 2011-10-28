/*                         M A G I C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include "common.h"

#include "magic.h"


const char *
bu_identify_magic(register uint32_t magic)
{
    switch (magic) {
	case 0:
	    return "Zero_Magic_Number";

	    /*
	     * libbu
	     */
	case BU_AVS_MAGIC:
	    return "bu_attribute_value_set";
	case BU_BITV_MAGIC:
	    return "bu_bitv";
	case BU_COLOR_MAGIC:
	    return "bu_color";
	case BU_EXTERNAL_MAGIC:
	    return "bu_external";
	case BU_HASH_ENTRY_MAGIC:
	    return "bu_hash_entry";
	case BU_HASH_RECORD_MAGIC:
	    return "bu_hash_record";
	case BU_HASH_TBL_MAGIC:
	    return "bu_hash_table";
	case BU_HIST_MAGIC:
	    return "bu_hist";
	case BU_HOOK_LIST_MAGIC:
	    return "bu_hook_list";
	case BU_IMAGE_FILE_MAGIC:
	    return "bu_image_file";
	case BU_LIST_HEAD_MAGIC:
	    return "bu_list";
	case BU_MAPPED_FILE_MAGIC:
	    return "bu_mapped_file";
	case BU_PTBL_MAGIC:
	    return "bu_ptbl";
	case BU_RB_LIST_MAGIC:
	    return "red-black list";
	case BU_RB_NODE_MAGIC:
	    return "red-black node";
	case BU_RB_PKG_MAGIC:
	    return "red-black package";
	case BU_RB_TREE_MAGIC:
	    return "red-black tree";
	case BN_SPM_MAGIC:
	    return "spm.h bn_spm_map_t";
	case BU_VLB_MAGIC:
	    return "bu_vlb";
	case BU_VLS_MAGIC:
	    return "bu_vls";

	    /*
	     * libbn
	     */
	case BN_GAUSS_MAGIC:
	    return "bn_gauss";
	case BN_POLY_MAGIC:
	    return "bn_poly";
	case BN_TABDATA_MAGIC:
	    return "bn_tabdata";
	case BN_TABLE_MAGIC:
	    return "bn_table";
	case BN_TOL_MAGIC:
	    return "bn_tol";
	case BN_UNIF_MAGIC:
	    return "bn_unif";
	case BN_VLBLOCK_MAGIC:
	    return "bn_vlblock";
	case BN_VLIST_MAGIC:
	    return "bn_vlist";

	    /*
	     * Primitives
	     */
	case RT_ARBN_INTERNAL_MAGIC:
	    return "rt_arbn_internal";
	case RT_ARB_INTERNAL_MAGIC:
	    return "rt_arb_internal";
	case RT_ARS_INTERNAL_MAGIC:
	    return "rt_ars_internal";
	case RT_BINUNIF_INTERNAL_MAGIC:
	    return "rt_binuf_internal";
	case RT_BOT_INTERNAL_MAGIC:
	    return "rt_bot_internal";
	case RT_BREP_INTERNAL_MAGIC:
	    return "rt_brep_internal";
	case RT_CLINE_INTERNAL_MAGIC:
	    return "rt_cline_internal";
	case RT_DSP_INTERNAL_MAGIC:
	    return "rt_dsp_internal";
	case RT_EBM_INTERNAL_MAGIC:
	    return "rt_ebm_internal";
	case RT_EHY_INTERNAL_MAGIC:
	    return "rt_ehy_internal";
	case RT_ELL_INTERNAL_MAGIC:
	    return "rt_ell_internal";
	case RT_EPA_INTERNAL_MAGIC:
	    return "rt_epa_internal";
	case RT_ETO_INTERNAL_MAGIC:
	    return "rt_eto_internal";
	case RT_EXTRUDE_INTERNAL_MAGIC:
	    return "rt_extrude_internal";
	case RT_GRIP_INTERNAL_MAGIC:
	    return "rt_grip_internal";
	case RT_HALF_INTERNAL_MAGIC:
	    return "rt_half_internal";
	case RT_HF_INTERNAL_MAGIC:
	    return "rt_hf_internal";
	case RT_METABALL_INTERNAL_MAGIC:
	    return "rt_metaball_internal";
	case RT_NURB_INTERNAL_MAGIC:
	    return "rt_nurb_internal";
	case RT_PART_INTERNAL_MAGIC:
	    return "rt_part_internal";
	case RT_PG_INTERNAL_MAGIC:
	    return "rt_pg_internal";
	case RT_PIPE_INTERNAL_MAGIC:
	    return "rt_pipe_internal";
	case RT_PNTS_INTERNAL_MAGIC:
	    return "rt_pnts_internal";
	case RT_REVOLVE_INTERNAL_MAGIC:
	    return "rt_revolve_internal";
	case RT_RHC_INTERNAL_MAGIC:
	    return "rt_rhc_internal";
	case RT_RPC_INTERNAL_MAGIC:
	    return "rt_rpc_internal";
	case RT_SKETCH_INTERNAL_MAGIC:
	    return "rt_sketch_internal";
	case RT_SUBMODEL_INTERNAL_MAGIC:
	    return "rt_submodel_internal";
	case RT_SUPERELL_INTERNAL_MAGIC:
	    return "rt_superell_internal";
	case RT_TGC_INTERNAL_MAGIC:
	    return "rt_tgc_internal";
	case RT_TOR_INTERNAL_MAGIC:
	    return "rt_tor_internal";
	case RT_VOL_INTERNAL_MAGIC:
	    return "rt_vol_internal";

	    /*
	     * N-manifold geometry
	     */
	case NMG_EDGEUSE2_MAGIC:
	    return "edgeuse2 [midway into edgeuse]";
	case NMG_EDGEUSE_MAGIC:
	    return "edgeuse";
	case NMG_EDGE_G_CNURB_MAGIC:
	    return "edge_g_cnurb";
	case NMG_EDGE_G_LSEG_MAGIC:
	    return "edge_g_lseg";
	case NMG_EDGE_MAGIC:
	    return "edge";
	case NMG_FACEUSE_MAGIC:
	    return "faceuse";
	case NMG_FACE_G_PLANE_MAGIC:
	    return "face_g_plane";
	case NMG_FACE_G_SNURB_MAGIC:
	    return "face_g_snurb";
	case NMG_FACE_MAGIC:
	    return "face";
	case NMG_KNOT_VECTOR_MAGIC:
	    return "knot_vector";
	case NMG_LOOPUSE_MAGIC:
	    return "loopuse";
	case NMG_LOOP_G_MAGIC:
	    return "loop_g";
	case NMG_LOOP_MAGIC:
	    return "loop";
	case NMG_MODEL_MAGIC:
	    return "model";
	case NMG_RADIAL_MAGIC:
	    return "radial";
	case NMG_RAY_DATA_MAGIC:
	    return "ray_data";
	case NMG_REGION_A_MAGIC:
	    return "region_a";
	case NMG_REGION_MAGIC:
	    return "region";
	case NMG_RT_HIT_MAGIC:
	    return "rt_hit";
	case NMG_RT_HIT_SUB_MAGIC:
	    return "rt_hit_sub";
	case NMG_RT_MISS_MAGIC:
	    return "rt_miss";
	case NMG_SHELL_A_MAGIC:
	    return "shell_a";
	case NMG_SHELL_MAGIC:
	    return "shell";
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
	    return "vertexuse_a_cnurb";
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
	    return "vertexuse_a_plane";
	case NMG_VERTEXUSE_MAGIC:
	    return "vertexuse";
	case NMG_VERTEX_G_MAGIC:
	    return "vertex_g";
	case NMG_VERTEX_MAGIC:
	    return "vertex";

	    /*
	     * Raytracing
	     */
	case RT_ANP_MAGIC:
	    return "librt Aprp";
	case RT_AP_MAGIC:
	    return "librt application";
	case RT_COMB_MAGIC:
	    return "librt rt_comb_internal";
	case RT_CONSTRAINT_MAGIC:
	    return "librt rt_constraint_internal";
	case RT_CTS_MAGIC:
	    return "librt combined_tree_state";
	case RT_DB_TRAVERSE_MAGIC:
	    return "librt dbtr";
	case RT_DBTS_MAGIC:
	    return "librt dbts";
	case RT_DB_INTERNAL_MAGIC:
	    return "rt_db_internal";
	case RT_DIR_MAGIC:
	    return "librt directory";
	case RT_FUNCTAB_MAGIC:
	    return "rt_functab";
	case RT_HIT_MAGIC:
	    return "librt hit";
	case RT_HTBL_MAGIC:
	    return "rt_htbl";
	case RT_PIECELIST_MAGIC:
	    return "librt piecelist";
	case RT_PIECESTATE_MAGIC:
	    return "librt piecestate";
	case RT_RAY_MAGIC:
	    return "librt xray";
	case RT_REGION_MAGIC:
	    return "librt region";
	case RT_SEG_MAGIC:
	    return "librt seg";
	case RT_SOLTAB_MAGIC:
	    return "librt soltab";
	case RT_SOLTAB2_MAGIC:
	    return "librt soltab2";
	case RT_TESS_TOL_MAGIC:
	    return "rt_tess_tol";
	case RT_TREE_MAGIC:
	    return "librt union tree";
	case RT_WDB_MAGIC:
	    return "rt_wdb";

	    /*
	     * Misc
	     */
	case ANIMATE_MAGIC:
	    return "librt animate";
	case CURVE_BEZIER_MAGIC:
	    return "curve_bezier";
	case CURVE_CARC_MAGIC:
	    return "curve_carc";
	case CURVE_LSEG_MAGIC:
	    return "curve_lseg";
	case CURVE_NURB_MAGIC:
	    return "curve_nurb";
	case DB5_RAW_INTERNAL_MAGIC:
	    return "db5 raw internal";
	case DBI_MAGIC:
	    return "librt db_i";
	case DB_FULL_PATH_MAGIC:
	    return "librt db_full_path";
	case FB_MAGIC:
	    return "fb.h fb_magic";
	case LIGHT_MAGIC:
	    return "light magic";
	case MF_MAGIC:
	    return "mf magic";
	case PIXEL_EXT_MAGIC:
	    return "librt pixel_ext";
	case PL_MAGIC:
	    return "pl magic";
	case PT_HD_MAGIC:
	    return "pt_hd magic";
	case PT_MAGIC:
	    return "librt partition";
	case RESOURCE_MAGIC:
	    return "librt resource";
	case RTI_MAGIC:
	    return "rt_i";
	case VERT_TREE_MAGIC:
	    return "vert_tree";
	case WDB_METABALLPT_MAGIC:
	    return "wdb metaball pt";
	case WDB_PIPESEG_MAGIC:
	    return "wdb.h wdb_pipeseg_magic";
	case WMEMBER_MAGIC:
	    return "wdb.h wmember_magic";

    }

    return "Unknown_Magic";
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
