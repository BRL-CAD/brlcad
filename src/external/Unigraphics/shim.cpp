/*                        S H I M . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file shim.cpp
 *
 * This file is provided as a compilation stub to ensure ongoing build testing
 * of the ug-g converter.  This file does not necessarily reflect the
 * Unigraphics api, its values, or type constructs and any similarity is either
 * coincidental or necessary for compilation.
 */

#include "shim.h"

extern "C" int UF_ASSEM_add_to_cset(tag_t,tag_t, int){return 0;}
extern "C" int UF_ASSEM_ask_assem_options(UF_ASSEM_options_t*){return 0;}
extern "C" int UF_ASSEM_ask_component_data(tag_t, char*, char*, char*, double*, double*, double(*)[4]){return 0;}
extern "C" int UF_ASSEM_ask_default_ref_sets(int*, char***){return 0;}
extern "C" int UF_ASSEM_ask_part_name_of_child(tag_t,const char*){return 0;}
extern "C" int UF_ASSEM_ask_part_occ_children(tag_t,tag_t**){return 0;}
extern "C" int UF_ASSEM_ask_ref_set_data(tag_t, char*, double*, double*, int*, tag_t**){return 0;}
extern "C" int UF_ASSEM_ask_suppress_state(tag_t, int*){return 0;}
extern "C" int UF_ASSEM_create_cset(tag_t,const char*, tag_t*){return 0;}
extern "C" int UF_ASSEM_cycle_inst_of_part(tag_t, tag_t){return 0;}
extern "C" int UF_ASSEM_is_occurrence(tag_t){return 0;}
extern "C" int UF_ASSEM_set_assem_options(UF_ASSEM_options_t*){return 0;}
extern "C" int UF_CURVE_ask_arc_data(tag_t, UF_CURVE_arc_s_t*){return 0;}
extern "C" int UF_CURVE_ask_line_data(tag_t, UF_CURVE_line_s_t*){return 0;}
extern "C" int UF_DISP_ask_color(int, int, char**, double*){return 0;}
extern "C" int UF_DISP_ask_color_count(int*){return 0;}
extern "C" int UF_EVAL_ask_arc(UF_EVAL_p_t, UF_EVAL_arc_s_t*){return 0;}
extern "C" int UF_EVAL_ask_limits(void*, double*){return 0;}
extern "C" int UF_EVAL_evaluate(void*,int,double,double*,void*){return 0;}
extern "C" int UF_EVAL_free(void*){return 0;}
extern "C" int UF_EVAL_initialize(tag_t,void*){return 0;}
extern "C" int UF_EVAL_is_arc(UF_EVAL_p_t, int*){return 0;}
extern "C" int UF_FACET_ask_default_parameters(UF_FACET_parameters_t*){return 0;}
extern "C" int UF_FACET_ask_face_id_of_facet(tag_t, int, int*){return 0;}
extern "C" int UF_FACET_ask_max_facet_verts(tag_t, int*){return 0;}
extern "C" int UF_FACET_ask_normals_of_facet(tag_t, int, int*, double(*)[3]){return 0;}
extern "C" int UF_FACET_ask_solid_face_of_facet(tag_t, int, tag_t*){return 0;}
extern "C" int UF_FACET_ask_vertices_of_facet(tag_t, int, int*, double(*)[3]){return 0;}
extern "C" int UF_FACET_cycle_facets(tag_t, int*){return 0;}
extern "C" int UF_FACET_facet_solid(tag_t, UF_FACET_parameters_t*, tag_t*){return 0;}
extern "C" int UF_FACET_set_default_parameters(UF_FACET_parameters_t*){return 0;}
extern "C" int UF_MODL_ask_ball_slot_parms(tag_t, int, char**, char**, char**, int*){return 0;}
extern "C" int UF_MODL_ask_body_feats(tag_t, uf_list_p_t*){return 0;}
extern "C" int UF_MODL_ask_bounding_box(tag_t, double*){return 0;}
extern "C" int UF_MODL_ask_c_bore_hole_parms(tag_t, int, char**, char**, char**, char**, char**, int*){return 0;}
extern "C" int UF_MODL_ask_c_sunk_hole_parms(tag_t, int, char**, char**, char**, char**, char**, int*){return 0;}
extern "C" int UF_MODL_ask_dovetail_slot_parms(tag_t, int, char**, char**, char**, char**, int*){return 0;}
extern "C" int UF_MODL_ask_exp_desc_of_feat(tag_t,int*,char***,tag_t**){return 0;}
extern "C" int UF_MODL_ask_exp_tag_value(tag_t,double*){return 0;}
extern "C" int UF_MODL_ask_extrusion(tag_t,int*,tag_t**,void**,char**,char**,char**,double*,int*,int*,double*){return 0;}
extern "C" int UF_MODL_ask_face_edges(tag_t, uf_list_p_t*){return 0;}
extern "C" int UF_MODL_ask_face_props(tag_t, double*, double*, double*, double*, double*, double*, double*, double*){return 0;}
extern "C" int UF_MODL_ask_face_type(tag_t, int*){return 0;}
extern "C" int UF_MODL_ask_face_uv_minmax(tag_t, double*){return 0;}
extern "C" int UF_MODL_ask_feat_direction(tag_t,double*,double*){return 0;}
extern "C" int UF_MODL_ask_feat_faces(tag_t, uf_list_p_t*){return 0;}
extern "C" int UF_MODL_ask_feat_location(tag_t,double*){return 0;}
extern "C" int UF_MODL_ask_feat_name(tag_t, char**){return 0;}
extern "C" int UF_MODL_ask_feat_relatives(tag_t, int*, tag_t**, int*, tag_t**){return 0;}
extern "C" int UF_MODL_ask_feat_type(tag_t, char**){return 0;}
extern "C" int UF_MODL_ask_feature_sign(tag_t, int*){return 0;}
extern "C" int UF_MODL_ask_features_of_mirror_set(tag_t, tag_t**, int*){return 0;}
extern "C" int UF_MODL_ask_list_count(void*, int*){return 0;}
extern "C" int UF_MODL_ask_list_item(void*, int, tag_t*){return 0;}
extern "C" int UF_MODL_ask_plane(tag_t, double*, double*){return 0;}
extern "C" int UF_MODL_ask_plane_of_mirror_set(tag_t, tag_t*){return 0;}
extern "C" int UF_MODL_ask_rect_slot_parms(tag_t, int, char**, char**, char**, int*){return 0;}
extern "C" int UF_MODL_ask_simple_hole_parms(tag_t, int, char**, char**, char**, int*){return 0;}
extern "C" int UF_MODL_ask_suppress_feature(tag_t, int*){return 0;}
extern "C" int UF_MODL_ask_sweep_curves(tag_t, int*, tag_t**, int*, tag_t**){return 0;}
extern "C" int UF_MODL_ask_t_slot_parms(tag_t, int, char**, char**, char**, char**,char**, int*){return 0;}
extern "C" int UF_MODL_ask_thru_faces(tag_t, tag_t*, tag_t*){return 0;}
extern "C" int UF_MODL_ask_u_slot_parms(tag_t, int, char**, char**, char**, char**, int*){return 0;}
extern "C" int UF_MODL_delete_list(void**){return 0;}
extern "C" int UF_MODL_delete_list_item(void**, tag_t){return 0;}
extern "C" int UF_MODL_dissect_exp_string(char*,char**,char**,tag_t*){return 0;}
extern "C" int UF_MODL_put_list_item(void*, tag_t){return 0;}
extern "C" int UF_MTX3_vec_multiply(double*, double*,point_t){return 0;}
extern "C" int UF_OBJ_ask_display_properties(tag_t, UF_OBJ_disp_props_s_t*){return 0;}
extern "C" int UF_OBJ_ask_type_and_subtype(tag_t,int*,int*){return 0;}
extern "C" int UF_OBJ_cycle_objs_in_part(tag_t, int, tag_t*){return 0;}
extern "C" int UF_OBJ_delete_object(tag_t){return 0;}
extern "C" int UF_PART_ask_part_name(tag_t, char*){return 0;}
extern "C" int UF_PART_ask_units(tag_t, int*){return 0;}
extern "C" int UF_PART_close_cset(tag_t){return 0;}
extern "C" int UF_PART_is_loaded(char*){return 0;}
extern "C" int UF_PART_open(char*, tag_t*, UF_PART_load_status_t*){return 0;}
extern "C" int UF_PART_open_cset(tag_t, UF_PART_load_status_t*){return 0;}
extern "C" int UF_PART_open_quiet(char*, tag_t*, UF_PART_load_status_t*){return 0;}
extern "C" int UF_PART_set_display_part(tag_t){return 0;}
extern "C" int UF_SKET_ask_feature_sketches(tag_t,void**){return 0;}
extern "C" int UF_SKET_ask_sketch_info(tag_t, UF_SKET_info_t*){return 0;}
extern "C" int UF_WAVE_ask_link_mirror_data(tag_t, int, tag_t*, tag_t*, tag_t*, tag_t*){return 0;}
extern "C" int UF_free_string_array(int,char**){return 0;}
extern "C" int UF_get_fail_message(int, char*){return 0;}
extern "C" tag_t UF_ASSEM_ask_child_of_instance(tag_t){return NULL_TAG;}
extern "C" tag_t UF_ASSEM_ask_inst_of_part_occ(tag_t){return NULL_TAG;}
extern "C" tag_t UF_ASSEM_ask_root_part_occ(tag_t){return NULL_TAG;}
extern "C" tag_t UF_PART_ask_display_part(){return NULL_TAG;}
extern "C" void UF_PART_close(tag_t, int, int){}
extern "C" void UF_initialize(){}
extern "C" void UF_terminate(){}
extern "C" void uc6476(int){}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


