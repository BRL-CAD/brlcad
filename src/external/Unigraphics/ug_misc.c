/*                       U G _ M I S C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file ug_misc.c
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <uf.h>
#include <uf_ui.h>
#include <uf_disp.h>
#include <uf_assem.h>
#include <uf_part.h>
#include <uf_facet.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_facet.h>
#include <uf_modl.h>
#include <malloc.h>
#include <tcl.h>


#include "./log.h"
#include "./ug_misc.h"
#include "./conv.h"

/* 10501462 10651848-1 */


/*
 *	A S K _ C S E T _ O F _ U N L O A D E D _ C H I L D R E N
 *
 */
tag_t ask_cset_of_unloaded_children(tag_t part, tag_t part_occ)
{
    tag_t cset = NULL_TAG;
    tag_t *children;
    int   n_children = UF_ASSEM_ask_part_occ_children(part_occ, &children);
    int   i;
    tag_t instance;
    char  part_fspec[MAX_FSPEC_SIZE+1];
    logical no_children = false;

    if (UF_func(UF_ASSEM_create_cset(part, "UNLOADED_CHILDREN", &cset))) {
	return NULL_TAG;
    }

    for (i = 0;i < n_children;i++)		{
	instance=UF_ASSEM_ask_inst_of_part_occ(children[i]);

	UF_ASSEM_ask_part_name_of_child(instance, part_fspec);
	if (!UF_PART_is_loaded(part_fspec))			{
	    /* This is an unloaded child part, add to set. */
	    UF_ASSEM_add_to_cset(cset, children[i],no_children);
	}
    }
    return cset;
}


/*
 *
 * move elements from one UG list to another
 */
void
Add_lists(uf_list_p_t dest, uf_list_p_t src)
{
    int count, i;
    tag_t obj;

    UF_MODL_ask_list_count(src, &count);

    for (i=count-1 ; i >= 0 ; i--) {
	UF_MODL_ask_list_item(src, i, &obj);
	UF_MODL_delete_list_item(&src, obj);

	UF_MODL_put_list_item(dest, obj);
    }
}

/*	F E A T U R E _ S I G N
 *
 *	Get a text string that describes the "sign" of a feature
 */
const char *feature_sign(tag_t feat)
{
    UF_FEATURE_SIGN sign;
    char *p;

    /* XXX For certain classes of feature, this might be handy.
     * This is just here as a memory-jogger.  If the feature is
     * a hole, we might be able to suppress it, and do a boolean subtraction
     * of a real cylinder.
     */
    UF_MODL_ask_feature_sign (feat, &sign);
    switch (sign) {
    case UF_NULLSIGN: p = "create new target solid"; break;
    case UF_POSITIVE: p = "add to target solid"; break;
    case UF_NEGATIVE: p = "subtract from target solid"; break;
    case UF_UNSIGNED: p = "intersect with target solid"; break;
    case UF_DEFORM_POSITIVE: p = "deform on the positive side of target"; break;
    case UF_DEFORM_NEGATIVE: p = "deform on the negative side of target"; break;
    }
    return p;
}

/*	U F U S R _ A S K _ U N L O A D
 *
 *  This is supposed to be helpful when UG thinks it might unload us.
 */
int ufusr_ask_unload ( void )
{
    return UF_UNLOAD_IMMEDIATELY;
}


jmp_buf my_env;

/*	R E P O R T
 *
 */
int report(char *call, char *file, int line, int code)
{
    char message[133] = "";
    extern char *progname;

    /* if code is 0 just return */
    if (!code) return code;

    if (UF_get_fail_message(code, message)) {
	fprintf(stderr, "%s Error in %s:%d\ncall: %s\n(no message)\n",
		progname, file, line, call);
    } else {
	fprintf(stderr, "%s Error in %s:%d\ncall: %s\nmessage:%s\n",
		progname, file, line, call, message);
    }
#if 1
    exit( 1 );
#else
    longjmp(my_env, 1);
#endif

    return 0; /* NOTREACHED */
}

static Tcl_HashTable parts_tbl;
static int tbl_uninitialized = 1;

tag_t
open_part(char *p_name, int level)
{
    UF_PART_load_status_t error_status;
    int e_part;
    tag_t ugpart;

    UF_func(UF_PART_open(p_name, &ugpart, &error_status));
    if (error_status.n_parts > 0) {

	dprintf("%*sPART_open status: failed:%d user_abort:%d n_parts:%d\n",
		level, "",
		error_status.failed,
		error_status.user_abort,
		error_status.n_parts);
	for (e_part = 0 ; e_part < error_status.n_parts ; e_part++) {
	    dprintf("%*sPart %s status %d\n",
		    level, "",
		    error_status.file_names[e_part],
		    error_status.statuses[e_part]);
	}

	UF_free_string_array(error_status.n_parts,
			     error_status.file_names);
	UF_free(error_status.statuses);
    }
    return ugpart;
}

/*
 *	L O A D _ S U B _ P A R T S
 *
 */
tag_t
load_sub_parts(tag_t displayed_part)
{
    tag_t		  unloaded_child_cset = NULL_TAG;
    UF_PART_load_status_t load_status;
    int                   i;
    tag_t root_part_occ = NULL_TAG;


    /* We have a displayed part, find the root part occurrence. */
    root_part_occ = UF_ASSEM_ask_root_part_occ(displayed_part);

    if (root_part_occ == NULL_TAG) {
	/* It is an assembly, get a cset of unloaded children. */
	unloaded_child_cset =
	    ask_cset_of_unloaded_children(displayed_part, root_part_occ);

	if (unloaded_child_cset != NULL_TAG) {
	    /* Now load the component set. */

	    UF_func(UF_PART_open_cset(unloaded_child_cset, &load_status));

	    for (i = 0;i < load_status.n_parts;i++) {
		printf("Error number %d. Open child part %s failed\n",
		       load_status.statuses[i],
		       load_status.file_names[i]);
	    }
	    if (load_status.n_parts > 0) {
		UF_free_string_array(load_status.n_parts,
				     load_status.file_names);
		UF_free(load_status.file_names);
	    }

	}
    }

    return unloaded_child_cset;
}

/*
 *	U N L O A D _ S U B _ P A R T S
 *
 */
void unload_sub_parts(tag_t loaded_child_cset)
{
    if (loaded_child_cset) {
	/* And unload the parts. */
	UF_func(UF_PART_close_cset(loaded_child_cset));
	/* We're now done with the cset so delete it. */
	UF_func(UF_OBJ_delete_object(loaded_child_cset));
    }
}

tag_t
Lee_open_part(char *p_name, int level)
{
    UF_PART_load_status_t error_status;
    int e_part;
    tag_t ugpart;
    int new_ent;	/* flag:  new entry created */
    Tcl_HashEntry *he;

    if (tbl_uninitialized) {
	Tcl_InitHashTable(&parts_tbl, TCL_STRING_KEYS);
	tbl_uninitialized = 0;
    }

    he = Tcl_CreateHashEntry(&parts_tbl, p_name, &new_ent);
    if (new_ent) {
	Tcl_SetHashValue(he, 1);
    } else {
	return (tag_t) 0;
    }

    if (level) {
	UF_func(UF_PART_open_quiet(p_name, &ugpart, &error_status));
    } else {
	UF_func(UF_PART_open(p_name, &ugpart, &error_status));
    }

    if (error_status.n_parts > 0) {

	dprintf("%*sPART_open status: failed:%d user_abort:%d n_parts:%d\n",
		level, "",
		error_status.failed,
		error_status.user_abort,
		error_status.n_parts);
	for (e_part = 0 ; e_part < error_status.n_parts ; e_part++) {
	    dprintf("%*sPart %s status %d\n",
		    level, "",
		    error_status.file_names[e_part],
		    error_status.statuses[e_part]);
	}

	UF_free_string_array(error_status.n_parts,
			     error_status.file_names);
	UF_free(error_status.statuses);
    }
    return ugpart;
}



struct ug_type_data {
    char	*name;
    int		value;
    char	*comment;
};


struct ug_type_data ug_typelist[] = {
{                 "",			         1, ""},
{                 "UF_point_type",	         2, ""},
{                  "UF_line_type",	         3, ""},
{      "UF_solid_collection_type",	         4,  "To hold deleted solids for undo" },
{                "UF_circle_type",	         5,  "Also called arc"},
{                 "UF_conic_type",	         6, ""},
{               "UF_spcurve_type",	         7,  "Obsolete in V10" },
{                "UF_spline_type",	         8,  "Obsolete in V10" },
{                "UF_spline_type",	         9,  "Renamed in V10" },
{               "UF_pattern_type",	        10, ""},
{        "UF_part_attribute_type",	        11, ""},
{        "UF_layer_category_type",	        12, ""},
{                 "UF_kanji_type",	        13, ""},
{              "UF_boundary_type",	        14, ""},
{                 "UF_group_type",	        15, ""},
{              "UF_cylinder_type",	        16,  "Obsolete in V10" },
{                  "UF_cone_type",	        17,  "Obsolete in V10" },
{                "UF_sphere_type",	        18,  "Obsolete in V10" },
{ "UF_surface_of_revolution_type",	        19,  "Obsolete in V10" },
{    "UF_tabulated_cylinder_type",	        20,  "Obsolete in V10" },
{         "UF_ruled_surface_type",	        21,  "Obsolete in V10" },
{         "UF_bounded_plane_type",	        22,  "Obsolete in V10" },
{          "UF_blended_face_type",	        23,  "Obsolete in V10" },
{    "UF_sculptured_surface_type",	        24,  "Obsolete in V10" },
{       "UF_drafting_entity_type",	        25, ""},
{             "UF_dimension_type",	        26, ""},
{            "UF_font_table_type",	        27, ""},
{           "UF_color_table_type",	        28, ""},
{                "UF_margin_type",	        29, ""},
{     "UF_gfem_property_set_type",	        30,  "Obsolete in V16 "},
{             "UF_gfem_load_type",	        31,  "Obsolete in V16 "},
{             "UF_gfem_node_type",	        32,  "Obsolete in V16 "},
{          "UF_gfem_element_type",	        33,  "Obsolete in V16 "},
{             "UF_gfem_ferd_type",	        34,  "Obsolete in V16 "},
{       "UF_gfem_annotation_type",	        35,  "Obsolete in V16 "},
{"UF_gfem_control_node_list_type",	        36,  "Obsolete in V16 "},
{         "UF_gfem_material_type",	        37,  "checked no reference to UF_master_symbol_type is found GFEM has been using hard coded 37 as material type for years "},
{              "UF_instance_type",	        38,  "Obsolete in V4 "},
{            "UF_connection_type",	        39,  "Obsolete in V4 "},
{                "UF_prefix_type",	        40,  "Obsolete in V4 "},
{                  "UF_tool_type",	        41,  "Obsolete "},
{    "UF_sub_entity_for_dim_type",	        42, ""},
{             "UF_b_surface_type",	        43, ""},
{        "UF_picture_entity_type",	        44,  "Obsolete in V16 "},
{     "UF_coordinate_system_type",	        45, ""},
{                 "UF_plane_type",	        46, ""},
{    "UF_bounded_plane_loop_type",	        47,  "Obsolete in V7 "},
{         "UF_report_entity_type",	        48, ""},
{   "UF_report_entry_entity_type",	        49, ""},
{               "UF_nesting_type",	        50, ""},
{          "UF_tool_display_type",	        51, ""},
{          "UF_skeleton_ent_type",	        52, ""},
{             "UF_parameter_type",	        53,  "Global Data "},
{                "UF_device_type",	        54,  "Obsolete in V15 "},
{                "UF_matrix_type",	        55, ""},
{                  "UF_gear_type",	        56,  "Obsolete in V10 "},
{             "UF_gear_mesh_type",	        57,  "Obsolete in V10 "},
{            "UF_gear_train_type",	        58,  "Obsolete in V10 "},
{               "UF_fatigue_type",	        59,  "Obsolete in V10 "},
{                  "UF_view_type",	        60, ""},
{                "UF_layout_type",	        61, ""},
{               "UF_drawing_type",	        62, ""},
{             "UF_component_type",	        63, ""},
{         "UF_reference_set_type",	        64, ""},
{        "UF_offset_surface_type",	        65, ""},
{       "UF_foreign_surface_type",	        66,  "Customer defined "},
{  "UF_foreign_surface_data_type",	        67,  "Customer defined "},
{          "UF_occ_instance_type",	        68, ""},
{       "UF_occ_shadow_part_type",	        69, ""},
{                 "UF_solid_type",	        70, ""},
{                  "UF_face_type",	        71,  "Obsolete in V10 "},
{                  "UF_edge_type",	        72,  "Obsolete in V10 "},
{       "UF_solid_composite_type",	        73,  "Obsolete in V10 "},
{               "UF_history_type",	        74, ""},
{        "UF_gfem_post_data_type",	        75,  "Obsolete in V16 "},
{        "UF_gfem_post_ferd_type",	        76,  "Obsolete in V16 "},
{     "UF_gfem_post_display_type",	        77,  "Obsolete in V16 "},
{       "UF_gfem_post_group_type",	        78,  "Obsolete in V16 "},
{          "UF_gfem_outline_type",	        79,  "Obsolete in V16 "},
{       "UF_gfem_local_csys_type",	        80,  "Obsolete in V16 "},
{           "UF_gfem_loader_type",	        81,  "Obsolete in V16 "},
{   "UF_sketch_tol_equation_type",	        82, ""},
{   "UF_sketch_tol_variable_type",	        83, ""},
{"UF_sketch_tol_output_reqst_type",	        84, ""},
{         "UF_mdm_mechanism_type",	        85, ""},
{             "UF_mdm_joint_type",	        86, ""},
{              "UF_mdm_link_type",	        87, ""},
{            "UF_mdm_spring_type",	        88, ""},
{     "UF_mdm_motion_vector_type",	        89, ""},
{             "UF_mdm_force_type",	        90, ""},
{          "UF_shaft_stress_type",	        91,  "Obsolete in V10 "},
{         "UF_shaft_feature_type",	        92,  "Obsolete in V10 "},
{            "UF_shaft_load_type",	        93,  "Obsolete in V10 "},
{         "UF_shaft_support_type",	        94,  "Obsolete in V10 "},
{         "UF_shaft_section_type",	        95,  "Obsolete in V10 "},
{                 "UF_shaft_type",	        96,  "Obsolete in V10 "},
{       "UF_mdm_analysis_pt_type",	        97,  "Obsolete in V10 "},
{            "UF_mdm_marker_type",	        97,  "replaces UF_mdm_analysis_pt_type "},
{            "UF_mdm_damper_type",	        98, ""},
{            "UF_mdm_torque_type",	        99, ""},
{   "UF_machining_operation_type",	       100, ""},
{        "UF_machining_path_type",	       101, ""},
{          "UF_table_column_type",	       102, ""},
{     "UF_machining_ude_map_type",	       103, ""},
{      "UF_data_declaration_type",	       104, ""},
{"UF_machining_geometry_grp_type",	       105, ""},
{"UF_machining_mach_tool_grp_type",	       106, ""},
{"UF_machining_parameter_set_type",	       107, ""},
{"UF_last_operation_pointer_type",	       108, ""},
{        "UF_machining_tool_type",	       109, ""},
{ "UF_machining_global_data_type",	       110, ""},
{        "UF_machining_geom_type",	       111, ""},
{    "UF_machining_null_grp_type",	       112, ""},
{"UF_machining_boundary_member_type",	       114, ""},
{"UF_machining_master_operation_type",	       115, ""},
{"UF_machining_post_command_type",	       116, ""},
{    "UF_machining_boundary_type",	       118, ""},
{"UF_machining_control_event_type",	       119, ""},
{         "UF_machining_ncm_type",	       120, ""},
{        "UF_machining_task_type",	       121, ""},
{       "UF_machining_setup_type",	       122, ""},
{    "UF_machining_feedrate_type",	       123, ""},
{     "UF_machining_display_type",	       124, ""},
{          "UF_machining_dp_type",	       125, ""},
{   "UF_machining_pathindex_type",	       126, ""},
{       "UF_machining_tldsp_type",	       127, ""},
{        "UF_machining_mode_type",	       128, ""},
{        "UF_machining_mthd_type",	       128, ""},
{        "UF_machining_clip_type",	       129, ""},
{            "UF_render_set_type",	       130, ""},
{       "UF_sketch_tol_csys_type",	       131, ""},
{    "UF_sketch_tol_feature_type",	       132,  "Obsolete in V10 "},
{     "UF_sketch_tol_mating_type",	       133,  "Obsolete in V10 "},
{                "UF_sketch_type",	       134, ""},
{       "UF_ordinate_margin_type",	       135, ""},
{         "UF_phys_material_type",	       136, ""},
{          "UF_ug_libraries_type",	       137, ""},
{    "UF_faceted_model_data_type",	       138, ""},
{         "UF_faceted_model_type",	       139, ""},
{                "UF_flange_type",	       140, ""},
{                  "UF_bend_type",	       141, ""},
{          "UF_flat_pattern_type",	       142, ""},
{           "UF_sheet_metal_type",	       143, ""},
{                 "UF_table_type",	       144, ""},
{          "UF_mdm_genforce_type",	       145, ""},
{        "UF_sfem_composite_type",	       146, ""},
{        "UF_cam_cut_method_type",	       147, ""},
{         "UF_dimension_set_type",	       148, ""},
{        "UF_display_object_type",	       149, ""},
{"UF_mdm_curve_curve_contact_type",	       150, ""},
{               "UF_prefix1_type",	       151,  "Obsolete in V10 "},
{         "UF_symbol_master_type",	       152,  "Obsolete in V10 "},
{     "UF_logic_part_master_type",	       153,  "Obsolete in V10 "},
{         "UF_draft_callout_type",	       154, ""},
{"UF_smsp_product_definition_type",	       155, ""},
{                "UF_symbol_type",	       156,  "Obsolete in V10 "},
{            "UF_logic_part_type",	       157,  "Obsolete in V10 "},
{  "UF_smart_model_instance_type",	       158, ""},
{ "UF_datum_reference_frame_type",	       159, ""},
{                   "UF_net_type",	       160,  "Obsolete in V10 "},
{           "UF_connection1_type",	       161,  "Obsolete in V10 "},
{                  "UF_node_type",	       162,  "Obsolete in V10 "},
{       "UF_report_net_list_type",	       163,  "Obsolete in V10 "},
{        "UF_component_list_type",	       164,  "Obsolete in V10 "},
{          "UF_tabular_note_type",	       165, ""},
{          "UF_cam_material_type",	       166, ""},
{                 "UF_rlist_type",	       167, ""},
{           "UF_route_route_type",	       168, ""},
{              "UF_analysis_type",	       169, ""},
{                   "UF_cam_type",	       171,  "Obsolete in V10 "},
{              "UF_cam_body_type",	       172,  "Obsolete in V10 "},
{          "UF_cam_follower_type",	       173,  "Obsolete in V10 "},
{          "UF_cam_function_type",	       174,  "Obsolete in V10 "},
{              "UF_material_type",	       180, ""},
{               "UF_texture_type",	       181, ""},
{          "UF_light_source_type",	       182, ""},
{           "UF_curve_group_type",	       183, ""},
{      "UF_general_face_set_type",	       184, ""},
{             "UF_anim_traj_type",	       185, ""},
{           "UF_sheet_group_type",	       186, ""},
{         "UF_cs2_rigid_set_type",	       187, ""},
{           "UF_design_rule_type",	       188, ""},
{     "UF_thd_symbolic_data_type",	       189, ""},
{          "UF_foreign_surf_type",	       190, ""},
{   "UF_user_defined_object_type",	       191, ""},
{"UF_generic_ent_int_sub_ent_type",	       192,  "Obsolete in V10 "},
{"UF_generic_ent_real_sub_ent_type",	       193,  "Obsolete in V10 "},
{           "UF_symbol_font_type",	       194, ""},
{          "UF_dataum_point_type",	       195,  "not use "},
{            "UF_datum_axis_type",	       196, ""},
{           "UF_datum_plane_type",	       197, ""},
{         "UF_solid_section_type",	       198, ""},
{          "UF_section_edge_type",	       199, ""},
{       "UF_section_segment_type",	       200, ""},
{      "UF_solid_silhouette_type",	       201, ""},
{          "UF_section_line_type",	       202, ""},
{         "UF_solid_in_view_type",	       203, ""},
{         "UF_component_set_type",	       204, ""},
{               "UF_feature_type",	       205, ""},
{                  "UF_zone_type",	       206, ""},
{                "UF_filter_type",	       207, ""},
{             "UF_promotion_type",	       208, ""},
{           "UF_mdm_measure_type",	       209, ""},
{             "UF_mdm_trace_type",	       210, ""},
{      "UF_mdm_interference_type",	       211, ""},
{                "UF_script_type",	       212, ""},
{           "UF_spreadsheet_type",	       213, ""},
{             "UF_reference_type",	       214,  "obsolete "},
{                "UF_scalar_type",	       215, ""},
{                "UF_offset_type",	       216, ""},
{             "UF_direction_type",	       217, ""},
{       "UF_parametric_text_type",	       218, ""},
{                 "UF_xform_type",	       219, ""},
{   "UF_route_control_point_type",	       220, ""},
{            "UF_route_port_type",	       221, ""},
{         "UF_route_segment_type",	       222, ""},
{      "UF_route_connection_type",	       223, ""},
{           "UF_route_stock_type",	       224, ""},
{     "UF_route_part_anchor_type",	       225, ""},
{   "UF_route_cross_section_type",	       226, ""},
{      "UF_route_stock_data_type",	       227, ""},
{          "UF_route_corner_type",	       228, ""},
{       "UF_route_part_type_type",	       229, ""},
{                   "UF_fam_type",	       230, ""},
{              "UF_fam_attr_type",	       231, ""},
{             "UF_sfem_mesh_type",	       232, ""},
{      "UF_sfem_mesh_recipe_type",	       233, ""},
{               "UF_faceset_type",	       234, ""},
{    "UF_sfem_mesh_geometry_type",	       235, ""},
{         "UF_feature_cache_type",	       236, ""},
{             "UF_sfem_load_type",	       237, ""},
{          "UF_sfem_bndcond_type",	       238, ""},
{         "UF_sfem_property_type",	       239, ""},
{    "UF_sfem_property_name_type",	       240, ""},
{                  "UF_axis_type",	       241, ""},
{            "UF_cs2_vertex_type",	       242, ""},
{        "UF_cs2_constraint_type",	       243, ""},
{ "UF_cs2_constraint_system_type",	       244, ""},
{    "UF_attribute_category_type",	       245, ""},
{             "UF_attribute_type",	       246, ""},
{                  "UF_note_type",	       247, ""},
{  "UF_tol_feature_instance_type",	       248, ""},
{      "UF_engineering_text_type",	       249, ""},
{            "UF_annotation_type",	       250, ""},
{     "UF_tolerance_feature_type",	       251, ""},
{                "UF_leader_type",	       252, ""},
{    "UF_engineering_symbol_type",	       253, ""},
{ "UF_feature_control_frame_type",	       254, ""},
{            "UF_max_entity_type",	       254, ""},
{            (char *)NULL,			 0, (char *)NULL}
};




struct ug_type_data ug_subtypelist[] = {

{    "UF_sketch_ref_line_subtype",	       101,  "This subtype is obsolete in V17.0. Used only by part_conversion. "},
{      "UF_circle_closed_subtype",	         1, ""},
{  "UF_sketch_ref_circle_subtype",	       101,  "This subtype is obsolete in V17.0. Used only by part_conversion. "},
{      "UF_conic_ellipse_subtype",	         2, ""},
{     "UF_conic_parabola_subtype",	         3, ""},
{    "UF_conic_hyperbola_subtype",	         4, ""},
{       "UF_spcurve_open_subtype",	         1, ""},
{     "UF_spcurve_closed_subtype",	         2, ""},
{        "UF_spline_open_subtype",	         1,  "Obsolete in V10 "},
{      "UF_spline_closed_subtype",	         2,  "Obsolete in V10 "},
{   "UF_b_curve_b_spline_subtype",	         1,  "Obsolete in V10 "},
{  "UF_sketch_ref_spline_subtype",	       101,  "This subtype is obsolete in V17.0. Used only by part_conversion. "},
{      "UF_pattern_point_subtype",	         1, ""},
{"UF_part_attribute_cache_subtype",	         1, ""},
{         "UF_draft_note_subtype",	         1, ""},
{        "UF_draft_label_subtype",	         2, ""},
{    "UF_draft_id_symbol_subtype",	         3, ""},
{          "UF_draft_fpt_subtype",	         4, ""},
{     "UF_draft_cntrline_subtype",	         5, ""},
{   "UF_draft_crosshatch_subtype",	         6, ""},
{"UF_draft_assorted_parts_subtype",	         7, ""},
{ "UF_draft_intersection_subtype",	         8, ""},
{ "UF_draft_target_point_subtype",	         9, ""},
{ "UF_draft_user_defined_subtype",	        10, ""},
{    "UF_draft_area_fill_subtype",	        11, ""},
{   "UF_draft_solid_fill_subtype",	        12, ""},
{"UF_draft_linear_cntrln_subtype",	        13, ""},
{"UF_draft_full_cir_cntrln_subtype",	        14, ""},
{"UF_draft_part_cir_cntrln_subtype",	        15, ""},
{"UF_draft_full_blt_circle_subtype",	        16, ""},
{"UF_draft_part_blt_circle_subtype",	        17, ""},
{"UF_draft_offset_cntrpt_subtype",	        18, ""},
{   "UF_draft_cyl_cntrln_subtype",	        19, ""},
{   "UF_draft_sym_cntrln_subtype",	        20, ""},
{        "UF_draft_point_subtype",	        37, ""},
{        "UF_draft_facet_subtype",	        41, ""},
{     "UF_dim_horizontal_subtype",	         1, ""},
{       "UF_dim_vertical_subtype",	         2, ""},
{       "UF_dim_parallel_subtype",	         3, ""},
{    "UF_dim_cylindrical_subtype",	         4, ""},
{  "UF_dim_perpendicular_subtype",	         5, ""},
{  "UF_dim_angular_minor_subtype",	         6, ""},
{  "UF_dim_angular_major_subtype",	         7, ""},
{     "UF_dim_arc_length_subtype",	         8, ""},
{         "UF_dim_radius_subtype",	         9, ""},
{       "UF_dim_diameter_subtype",	        10, ""},
{           "UF_dim_hole_subtype",	        11, ""},
{    "UF_dim_conc_circle_subtype",	        12, ""},
{ "UF_dim_ordinate_horiz_subtype",	        13, ""},
{  "UF_dim_ordinate_vert_subtype",	        14, ""},
{ "UF_dim_assorted_parts_subtype",	        15, ""},
{  "UF_dim_folded_radius_subtype",	        16, ""},
{"UF_dim_chain_dimensions_subtype",	        17, ""},
{"UF_dim_ordinate_origin_subtype",	        18,  "CATa" },
{      "UF_dim_perimeter_subtype",	        19, ""},
{ "UF_connection_offpage_subtype",	         1, ""},
{ "UF_connection_special_subtype",	         2, ""},
{       "UF_dim_sub_line_subtype",	         1, ""},
{        "UF_dim_sub_arc_subtype",	         2, ""},
{       "UF_dim_sub_text_subtype",	         3, ""},
{   "UF_b_surface_bezier_subtype",	         1,  "Obsolete in V10 "},
{ "UF_b_surface_b_spline_subtype",	         2,  "Obsolete in V10 "},
{           "UF_csys_wcs_subtype",	         1, ""},
{         "UF_parts_list_subtype",	         1, ""},
{   "UF_parts_list_entry_subtype",	         1, ""},
{        "UF_mcs_display_subtype",	         1, ""},
{      "UF_skeleton_grid_subtype",	         1, ""},
{"UF_skeleton_wind_bords_subtype",	         2, ""},
{"UF_skeleton_wcs_display_subtype",	         3, ""},
{   "UF_parm_mach_global_subtype",	         1,  "Obsolete in V5 "},
{  "UF_parm_lathe_global_subtype",	         2,  "Obsolete in V5 "},
{   "UF_parm_lathe_rough_subtype",	         3,  "Obsolete in V5 "},
{  "UF_parm_lathe_finish_subtype",	         4,  "Obsolete in V5 "},
{  "UF_parm_lathe_groove_subtype",	         5,  "Obsolete in V5 "},
{  "UF_parm_lathe_thread_subtype",	         6,  "Obsolete in V5 "},
{         "UF_parm_drill_subtype",	         7,  "Obsolete in V5 "},
{   "UF_parm_mill_global_subtype",	         8,  "Obsolete in V5 "},
{       "UF_parm_profile_subtype",	         9,  "Obsolete in V5 "},
{ "UF_parm_follow_pocket_subtype",	        10,  "Obsolete in V5 "},
{       "UF_parm_zig_zag_subtype",	        11,  "Obsolete in V5 "},
{  "UF_parm_surf_contour_subtype",	        12,  "Obsolete in V5 "},
{"UF_parm_line_machining_subtype",	        13,  "Obsolete in V5 "},
{"UF_parm_rough_to_depth_subtype",	        14,  "Obsolete in V5 "},
{"UF_parm_point_to_point_subtype",	        15,  "Obsolete in V5 "},
{"UF_parm_dimensions_data_subtype",	        16, ""},
{    "UF_parm_kanji_data_subtype",	        17, ""},
{"UF_parm_schematics_data_subtype",	        18,  "Obsolete in V10 "},
{"UF_parm_menu_table_data_subtype",	        19, ""},
{       "UF_parm_ug_data_subtype",	        20, ""},
{  "UF_parm_display_data_subtype",	        21, ""},
{    "UF_parm_layer_data_subtype",	        22, ""},
{  "UF_parm_model_bounds_subtype",	        25, ""},
{       "UF_parm_diagram_subtype",	        26, ""},
{   "UF_parm_sheet_metal_subtype",	        30, ""},
{"UF_parm_ladder_diagram_subtype",	        31, ""},
{    "UF_parm_calculator_subtype",	        32, ""},
{   "UF_parm_member_view_subtype",	        33, ""},
{"UF_parm_sketch_tol_data_subtype",	        34, ""},
{   "UF_parm_hidden_line_subtype",	        35, ""},
{   "UF_parm_rapid_proto_subtype",	        37, ""},
{  "UF_parm_section_line_subtype",	        39, ""},
{    "UF_parm_retain_ann_subtype",	        40, ""},
{          "UF_parm_sfem_subtype",	        41, ""},
{    "UF_parm_annotation_subtype",	        42, ""},
{    "UF_parm_crvtr_disp_subtype",	        43, ""},
{      "UF_parm_drawings_subtype",	        44, ""},
{      "UF_canned_layout_subtype",	         1, ""},
{    "UF_part_occurrence_subtype",	         1, ""},
{    "UF_shadow_part_occ_subtype",	         2, ""},
{     "UF_reference_tool_subtype",	         1, ""},
{"UF_reference_parameter_subtype",	         2, ""},
{"UF_reference_cam_template_subtype",	         3, ""},
{ "UF_reference_cam_task_subtype",	         4, ""},
{   "UF_solid_swept_body_subtype",	         1,  "Internal use only -" },
{         "UF_solid_face_subtype",	         2, ""},
{         "UF_solid_edge_subtype",	         3, ""},
{   "UF_solid_silhouette_subtype",	         4,  "Moved to type 201 in V10 "},
{ "UF_solid_foreign_surf_subtype",	         5, ""},
{           "UF_cylinder_subtype",	        16,  "Obsolete in V10 "},
{               "UF_cone_subtype",	        17,  "Obsolete in V10 "},
{             "UF_sphere_subtype",	        18,  "Obsolete in V10 "},
{"UF_surface_of_revolution_subtype",	        19,  "Obsolete in V10 "},
{ "UF_tabulated_cylinder_subtype",	        20,  "Obsolete in V10 "},
{      "UF_ruled_surface_subtype",	        21,  "Obsolete in V10 "},
{      "UF_bounded_plane_subtype",	        22,  "Obsolete in V10 "},
{     "UF_fillet_surface_subtype",	        23,  "Obsolete in V10 "},
{ "UF_sculptured_surface_subtype",	        24,  "Obsolete in V10 "},
{          "UF_b_surface_subtype",	        43,  "Obsolete in V10 "},
{     "UF_offset_surface_subtype",	        65,  "Obsolete in V10 "},
{    "UF_foreign_surface_subtype",	        66,  "Obsolete in V10 "},
{  "UF_gfem_control_ferd_subtype",	         1,  "Obsolete in V16 "},
{  "UF_gfem_element_ferd_subtype",	         2,  "Obsolete in V16 "},
{     "UF_gfem_node_ferd_subtype",	         3,  "Obsolete in V16 "},
{  "UF_gfem_vctr_display_subtype",	         1,  "Obsolete in V16 "},
{ "UF_gfem_deflected_dsp_subtype",	         2,  "Obsolete in V16 "},
{"UF_gfem_vctr_deflctd_dsp_subtype",	         4,  "Obsolete in V16 "},
{   "UF_sketch_1_var_equ_subtype",	         1, ""},
{"UF_sketch_dimension_equ_subtype",	         4, ""},
{ "UF_sketch_regular_equ_subtype",	         5, ""},
{"UF_sketch_inferred_equ_subtype",	        10, ""},
{"UF_sketch_param_pnt_var_subtype",	         1, ""},
{"UF_sketch_invisible_var_subtype",	         2, ""},
{"UF_sketch_line_slope_var_subtype",	         3, ""},
{"UF_sketch_line_angle_var_subtype",	         3, ""},
{"UF_sketch_arc_radius_var_subtype",	         4, ""},
{"UF_sketch_arc_angle_var_subtype",	         4, ""},
{"UF_sketch_bcurve_slope_var_subtype",	         5, ""},
{"UF_sketch_fixed_circle_ep_var_subtype",	       100, ""},
{"UF_sketch_fixed_param_pnt_var_subtype",	       101, ""},
{"UF_sketch_fixed_invisible_var_subtype",	       102, ""},
{"UF_sketch_fixed_line_slope_var_subtype",	       103, ""},
{"UF_sketch_fixed_line_angle_var_subtype",	       103, ""},
{"UF_sketch_fixed_arc_radius_var_subtype",	       104, ""},
{"UF_sketch_fixed_arc_angle_var_subtype",	       104, ""},
{"UF_sketch_fixed_bcurve_slope_var_subtype",	       105, ""},
{       "UF_mdm_revolute_subtype",	         3, ""},
{ "UF_mdm_revolute_fixed_subtype",	         4, ""},
{         "UF_mdm_slider_subtype",	         5, ""},
{   "UF_mdm_slider_fixed_subtype",	         6, ""},
{       "UF_mdm_cylinder_subtype",	         7, ""},
{ "UF_mdm_cylinder_fixed_subtype",	         8, ""},
{          "UF_mdm_screw_subtype",	         9, ""},
{    "UF_mdm_screw_fixed_subtype",	        10, ""},
{      "UF_mdm_universal_subtype",	        11, ""},
{"UF_mdm_universal_fixed_subtype",	        12, ""},
{         "UF_mdm_sphere_subtype",	        13, ""},
{   "UF_mdm_sphere_fixed_subtype",	        14, ""},
{         "UF_mdm_planar_subtype",	        15, ""},
{   "UF_mdm_planar_fixed_subtype",	        16, ""},
{           "UF_mdm_gear_subtype",	        17, ""},
{     "UF_mdm_gear_fixed_subtype",	        18, ""},
{          "UF_mdm_rckpn_subtype",	        19, ""},
{    "UF_mdm_rckpn_fixed_subtype",	        20, ""},
{         "UF_mdm_pt_crv_subtype",	        21, ""},
{"UF_mdm_pt_crv_fixed_curve_subtype",	        22, ""},
{"UF_mdm_pt_crv_fixed_point_subtype",	        23, ""},
{        "UF_mdm_crv_crv_subtype",	        24, ""},
{          "UF_mdm_cable_subtype",	        25, ""},
{"UF_mdm_user_defined_marker_subtype",	         1, ""},
{    "UF_mdm_cofm_marker_subtype",	         2, ""},
{"UF_mach_instanced_oper_subtype",	         1, ""},
{   "UF_mach_orphan_oper_subtype",	         2, ""},
{    "UF_mill_geom_featr_subtype",	        10, ""},
{     "UF_mill_bnd_featr_subtype",	        20, ""},
{        "UF_mill_orient_subtype",	        30, ""},
{  "UF_mill_volume_featr_subtype",	        35, ""},
{          "UF_turn_geom_subtype",	        40, ""},
{           "UF_turn_bnd_subtype",	        50, ""},
{        "UF_turn_orient_subtype",	        60, ""},
{         "UF_turn_featr_subtype",	        65, ""},
{"UF_mach_wedm_external_group_subtype",	        70, ""},
{"UF_mach_wedm_internal_group_subtype",	        80, ""},
{"UF_mach_wedm_open_group_subtype",	        90, ""},
{"UF_mach_wedm_nocore_group_subtype",	       100, ""},
{"UF_mach_wedm_feature_group_subtype",	       110, ""},
{        "UF_wedm_orient_subtype",	       120, ""},
{   "UF_drill_geom_featr_subtype",	       130, ""},
{        "UF_ncfeatr_udf_subtype",	       140, ""},
{        "UF_ncfeatr_uda_subtype",	       150, ""},
{"UF_machining_mach_turret_subtype",	         1, ""},
{"UF_machining_mach_pocket_subtype",	         2, ""},
{ "UF_machining_mach_kim_subtype",	         3, ""},
{"UF_machining_mach_kim_comp_subtype",	         4, ""},
{"UF_machining_mach_kim_degof_subtype",	         5, ""},
{"UF_machining_mach_kim_junction_subtype",	         6, ""},
{"UF_machining_mach_kim_valuator_subtype",	         7, ""},
{"UF_machining_mach_sim_kim_pocket_subtype",	         8, ""},
{ "UF_machining_mach_ipw_subtype",	       999, ""},
{"UF_mach_mill_post_cmnds_subtype",	        11, ""},
{"UF_mach_lathe_post_cmnds_subtype",	        13, ""},
{"UF_mach_wed_post_cmnds_subtype",	        17, ""},
{        "UF_mach_pocket_subtype",	       110, ""},
{"UF_mach_surface_contour_subtype",	       210, ""},
{          "UF_mach_vasc_subtype",	       211, ""},
{  "UF_mach_gssm_main_op_subtype",	       220, ""},
{   "UF_mach_gssm_sub_op_subtype",	       221, ""},
{     "UF_mach_gssm_grip_subtype",	       222, ""},
{    "UF_mach_param_line_subtype",	       230, ""},
{  "UF_mach_zig_zag_surf_subtype",	       240, ""},
{"UF_mach_rough_to_depth_subtype",	       250, ""},
{"UF_mach_cavity_milling_subtype",	       260, ""},
{  "UF_mach_face_milling_subtype",	       261, ""},
{"UF_mach_volumn_milling_subtype",	       262, ""},
{"UF_mach_zlevel_milling_subtype",	       263, ""},
{   "UF_mach_lathe_rough_subtype",	       310, ""},
{  "UF_mach_lathe_finish_subtype",	       320, ""},
{  "UF_mach_lathe_groove_subtype",	       330, ""},
{  "UF_mach_lathe_thread_subtype",	       340, ""},
{         "UF_mach_drill_subtype",	       350, ""},
{    "UF_mach_lathe_face_subtype",	       360, ""},
{"UF_mach_point_to_point_subtype",	       450, ""},
{"UF_mach_seq_curve_mill_subtype",	       460, ""},
{"UF_mach_seq_curve_lathe_subtype",	       461, ""},
{    "UF_mach_turn_rough_subtype",	       510, ""},
{   "UF_mach_turn_finish_subtype",	       520, ""},
{"UF_mach_turn_teachmode_subtype",	       530, ""},
{   "UF_mach_turn_thread_subtype",	       540, ""},
{   "UF_mach_turn_cdrill_subtype",	       550, ""},
{"UF_mach_turn_auxiliary_subtype",	       560, ""},
{   "UF_mach_hole_making_subtype",	       600, ""},
{          "UF_mach_wedm_subtype",	       700, ""},
{       "UF_mach_mill_ud_subtype",	       800, ""},
{       "UF_mach_mill_mc_subtype",	      1100, ""},
{      "UF_mach_lathe_mc_subtype",	      1200, ""},
{       "UF_mach_wedm_mc_subtype",	      1300, ""},
{      "UF_mach_lathe_ud_subtype",	      1400, ""},
{       "UF_mach_wedm_ud_subtype",	      1500, ""},
{     "UF_mach_mass_edit_subtype",	      1600, ""},
{"UF_mach_geom_planar_mill_subtype",	         1, ""},
{"UF_mach_geom_surf_mill_subtype",	         2, ""},
{    "UF_mach_geom_lathe_subtype",	         3, ""},
{"UF_mach_geom_pnt_to_pnt_subtype",	         4, ""},
{ "UF_mach_geom_boundary_subtype",	         5, ""},
{"UF_mach_geom_seq_curve_subtype",	         5, ""},
{ "UF_mach_geom_seq_surf_subtype",	         6, ""},
{ "UF_mach_geom_face_bnd_subtype",	         7, ""},
{  "UF_mach_geom_camgeom_subtype",	         9, ""},
{"UF_mach_geom_cut_level_subtype",	        11, ""},
{"UF_mach_geom_contain_edge_subtype",	        12, ""},
{  "UF_mach_geom_feature_subtype",	        13, ""},
{           "UF_mach_ncm_subtype",	        10, ""},
{     "UF_mach_ncm_point_subtype",	        20, ""},
{    "UF_mach_ncm_engret_subtype",	        30, ""},
{  "UF_mach_ncm_transfer_subtype",	        40, ""},
{    "UF_mach_ncm_clgeom_subtype",	        50, ""},
{    "UF_mach_order_task_subtype",	       160, ""},
{     "UF_mach_clsf_task_subtype",	       161, ""},
{    "UF_mach_optim_task_subtype",	       162, ""},
{      "UF_mach_dp_point_subtype",	        10, ""},
{      "UF_mach_dp_curve_subtype",	        20, ""},
{    "UF_mach_dp_surface_subtype",	        30, ""},
{   "UF_mach_dp_boundary_subtype",	        40, ""},
{  "UF_mach_dp_tool_path_subtype",	        50, ""},
{"UF_mach_dp_radial_curve_subtype",	        60, ""},
{     "UF_mach_dp_spiral_subtype",	        70, ""},
{         "UF_mach_dp_ud_subtype",	        80, ""},
{      "UF_mach_dpm_none_subtype",	       100, ""},
{     "UF_mach_dpm_amill_subtype",	       110, ""},
{     "UF_mach_dpm_curve_subtype",	       120, ""},
{   "UF_mach_dpm_surface_subtype",	       130, ""},
{  "UF_mach_dpm_boundary_subtype",	       140, ""},
{ "UF_mach_dpm_tool_path_subtype",	       150, ""},
{"UF_mach_dpm_radial_curve_subtype",	       160, ""},
{    "UF_mach_dpm_spiral_subtype",	       170, ""},
{        "UF_mach_dpm_ud_subtype",	       180, ""},
{      "UF_mach_dpm_fcut_subtype",	       190, ""},
{      "UF_mach_dpm_line_subtype",	       191, ""},
{       "UF_mach_dpm_arc_subtype",	       192, ""},
{   "UF_mach_dpm_contour_subtype",	       200, ""},
{     "UF_mach_mill_mode_subtype",	        10, ""},
{    "UF_mach_lathe_mode_subtype",	        20, ""},
{    "UF_mach_drill_mode_subtype",	        30, ""},
{     "UF_mach_wedm_mode_subtype",	        40, ""},
{     "UF_mach_turn_mode_subtype",	       100, ""},
{     "UF_mach_mill_mthd_subtype",	        10, ""},
{    "UF_mach_lathe_mthd_subtype",	        20, ""},
{    "UF_mach_drill_mthd_subtype",	        30, ""},
{     "UF_mach_wedm_mthd_subtype",	        40, ""},
{     "UF_mach_turn_mthd_subtype",	       100, ""},
{     "UF_mach_hole_mthd_subtype",	       110, ""},
{         "UF_v13_sketch_subtype",	         1, ""},
{   "UF_extracted_sketch_subtype",	         2, ""},
{"UF_ord_margin_horizontal_subtype",	        13, ""},
{"UF_ord_margin_vertical_subtype",	        14, ""},
{"UF_faceted_model_cloud_subtype",	         1, ""},
{          "UF_table_fam_subtype",	         1, ""},
{         "UF_mdm_vforce_subtype",	         1, ""},
{        "UF_mdm_vtorque_subtype",	         2, ""},
{        "UF_mdm_contact_subtype",	         3, ""},
{"UF_sfem_composite_face_subtype",	         2, ""},
{"UF_sfem_composite_edge_subtype",	         3, ""},
{"UF_sfem_composite_vertex_subtype",	         4, ""},
{"UF_sfem_composite_subface_subtype",	         5, ""},
{"UF_sfem_composite_subedge_subtype",	         6, ""},
{       "UF_dim_baseline_subtype",	         1, ""},
{"UF_smsp_product_definition_geom_subtype",	         1, ""},
{         "UF_smsp_group_subtype",	         2, ""},
{          "UF_smsp_root_subtype",	         3, ""},
{"UF_smsp_product_attribute_subtype",	         4, ""},
{ "UF_smsp_product_value_subtype",	         5, ""},
{"UF_user_defined_attribute_instance_subtype",	         1, ""},
{"UF_smart_model_instance_mark_subtype",	         2, ""},
{          "UF_composite_subtype",	         1, ""},
{         "UF_rlist_list_subtype",	         1, ""},
{       "UF_rlist_format_subtype",	         2, ""},
{       "UF_rlist_filter_subtype",	         3, ""},
{    "UF_fmbd_annotation_subtype",	         4, ""},
{         "UF_route_wire_subtype",	         1, ""},
{      "UF_route_harness_subtype",	         2, ""},
{         "UF_route_path_subtype",	         3, ""},
{    "UF_route_path_fmbd_subtype",	         4, ""},
{  "UF_route_path_offset_subtype",	         5, ""},
{"UF_route_built_in_path_subtype",	         6, ""},
{        "UF_route_cable_subtype",	         7, ""},
{  "UF_route_jumper_wire_subtype",	         8, ""},
{    "UF_surface_section_subtype",	         1, ""},
{   "UF_simplified_group_subtype",	         1, ""},
{ "UF_invis_solid_record_subtype",	         2, ""},
{"UF_dropped_curve_group_subtype",	         3, ""},
{"UF_design_rule_violation_subtype",	         1, ""},
{"UF_design_rule_override_subtype",	         2, ""},
{"UF_design_rule_function_subtype",	         3, ""},
{      "UF_arrow_segment_subtype",	         1, ""},
{        "UF_cut_segment_subtype",	         2, ""},
{       "UF_bend_segment_subtype",	         3, ""},
{"UF_solid_silhouette_uvhatch_subtype",	         1, ""},
{            "UF_vicurve_subtype",	         2, ""},
{"UF_simple_section_line_subtype",	         1, ""},
{"UF_stepped_section_line_subtype",	         2, ""},
{"UF_aligned_section_line_subtype",	         3, ""},
{  "UF_half_section_line_subtype",	         4, ""},
{"UF_unfolded_section_line_subtype",	         5, ""},
{          "UF_breakline_subtype",	         6, ""},
{         "UF_zone_plane_subtype",	         1, ""},
{   "UF_component_filter_subtype",	         1, ""},
{  "UF_spreadsheet_excel_subtype",	         1, ""},
{ "UF_route_miter_corner_subtype",	         1, ""},
{  "UF_route_cope_corner_subtype",	         2, ""},
{  "UF_route_disc_corner_subtype",	         3, ""},
{           "UF_fam_part_subtype",	         1, ""},
{      "UF_fam_attr_text_subtype",	         1, ""},
{   "UF_fam_attr_numeric_subtype",	         2, ""},
{   "UF_fam_attr_integer_subtype",	         3, ""},
{    "UF_fam_attr_double_subtype",	         4, ""},
{    "UF_fam_attr_string_subtype",	         5, ""},
{      "UF_fam_attr_part_subtype",	         6, ""},
{      "UF_fam_attr_name_subtype",	         7, ""},
{  "UF_fam_attr_instance_subtype",	         8, ""},
{       "UF_fam_attr_exp_subtype",	         9, ""},
{    "UF_fam_attr_mirror_subtype",	        10, ""},
{   "UF_fam_attr_density_subtype",	        11, ""},
{   "UF_fam_attr_feature_subtype",	        12, ""},
{"UF_sfem_weld_mesh_recipe_subtype",	         1, ""},
{"UF_sfem_connection_mesh_recipe_subtype",	         2, ""},
{"UF_sfem_conn_weld_mesh_recipe_subtype",	         3, ""},
{"UF_sfem_2d_contact_mesh_recipe_subtype",	         4, ""},
{"UF_sfem_mesh_geometry_face_subtype",	         1, ""},
{"UF_sfem_mesh_geometry_edge_subtype",	         2, ""},
{"UF_sfem_mesh_geometry_curve_subtype",	         3, ""},
{"UF_sfem_mesh_geometry_point_subtype",	         4, ""},
{"UF_sfem_mesh_geometry_comp_body_subtype",	         5, ""},
{"UF_sfem_mesh_geometry_comp_face_subtype",	         6, ""},
{"UF_sfem_mesh_geometry_comp_edge_subtype",	         7, ""},
{"UF_sfem_generic_property_subtype",	         1, ""},
{  "UF_sfem_mat_property_subtype",	         2, ""},
{ "UF_sfem_sect_property_subtype",	         3, ""},
{            "UF_cs2_dim_subtype",	         1, ""},
{         "UF_cs2_helped_subtype",	         2, ""},
{     "UF_cs2_dim_helped_subtype",	         3, ""},
{"UF_cs2_string_constraint_subtype",	         4, ""},
{"UF_cs2_trim_constraint_subtype",	         5, ""},
{"UF_cs2_offset_constraint_subtype",	         6, ""},
{"UF_cs2_equation_constraint_subtype",	         7, ""},
{   "UF_cs2_permanent_cs_subtype",	         1, ""},
{   "UF_string_attribute_subtype",	         1, ""},
{  "UF_integer_attribute_subtype",	         2, ""},
{   "UF_scalar_attribute_subtype",	         3, ""},
{   "UF_length_attribute_subtype",	         4, ""},
{     "UF_area_attribute_subtype",	         5, ""},
{   "UF_volume_attribute_subtype",	         6, ""},
{     "UF_date_attribute_subtype",	         7, ""},
{"UF_reference_attribute_subtype",	         8, ""},
{     "UF_null_attribute_subtype",	         9, ""},
{      "UF_appended_text_subtype",	         1, ""},
{       "UF_limit_or_fit_subtype",	         2, ""},
{"UF_datum_identifier_instance_subtype",	         1, ""},
{"UF_datum_point_instance_subtype",	         2, ""},
{"UF_datum_line_instance_subtype",	         3, ""},
{"UF_datum_area_instance_subtype",	         4,  "obsolete in V15.0 "},
{"UF_rectangular_area_instance_subtype",	         5, ""},
{"UF_circular_area_instance_subtype",	         6, ""},
{"UF_cylindrical_area_instance_subtype",	         7, ""},
{"UF_directed_datum_ident_instance_subtype",	         8, ""},
{"UF_user_defined_area_instance_subtype",	         9, ""},
{"UF_complex_feature_instance_subtype",	        10, ""},
{  "UF_tolerance_feature_subtype",	         1, ""},
{"UF_complex_tolerance_feature_subtype",	         2, ""},
{"UF_complex_tolerance_subfeature_subtype",	         3, ""},
{       "UF_datum_object_subtype",	         4, ""},
{           "UF_flatness_subtype",	         1, ""},
{           "UF_circular_subtype",	         2, ""},
{        "UF_cylindrical_subtype",	         3, ""},
{       "UF_line_profile_subtype",	         4, ""},
{    "UF_surface_profile_subtype",	         5, ""},
{            "UF_angular_subtype",	         6, ""},
{      "UF_perpendicular_subtype",	         7, ""},
{           "UF_parallel_subtype",	         8, ""},
{           "UF_position_subtype",	         9, ""},
{         "UF_concentric_subtype",	        10, ""},
{          "UF_symmetric_subtype",	        11, ""},
{    "UF_circular_runout_subtype",	        12, ""},
{       "UF_total_runout_subtype",	        13, ""},
{            (char *)NULL,			 0, (char *)NULL},
};


char *
lookup_ug_type(int t, char **comment)
{
    int i;

    for (i=0 ; ug_typelist[i].name != (char *)NULL ; i++ ) {
	if ( ug_typelist[i].value == t) {
	    goto found;
	}
    }
    return (char *)NULL;


found:
    if (comment && *ug_typelist[i].comment != '\0')
	*comment = ug_typelist[i].comment;

    return ug_typelist[i].name;
 }

char *
lookup_ug_subtype(int t, char **comment)
{
    int i;

    for (i=0 ; ug_subtypelist[i].name != (char *)NULL ; i++ ) {
	if ( ug_subtypelist[i].value == t ) {
	    goto found;
	}
    }
    return (char *)NULL;

found:
    if (comment && *ug_subtypelist[i].comment != '\0')
	*comment = ug_typelist[i].comment;

    return ug_subtypelist[i].name;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
