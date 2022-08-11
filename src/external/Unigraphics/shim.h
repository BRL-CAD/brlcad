/*                          S H I M . H
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file shim.h
 *
 * This file is provided as a compilation stub to ensure ongoing build testing
 * of the ug-g converter.  This file does not necessarily reflect the
 * Unigraphics api, its values, or type constructs and any similarity is either
 * coincidental or necessary for compilation.
 */

#ifndef UG_SHIM_H
#define UG_SHIM_H 1

#include "common.h"

#include "vmath.h"
#include "bu/malloc.h"


#ifdef __cplusplus
#define EXTLANG "C"
#else
#define EXTLANG
#endif

#define MAX_FSPEC_SIZE 100
#define NULL_TAG 0
#define UF_NULLSIGN 0
#define UF_free(X) bu_free((void*)X, "UF_free");

#define UF_ASSEM_dont_maintain_work_part 	1
#define UF_DEFORM_NEGATIVE 			2
#define UF_DEFORM_POSITIVE 			3
#define UF_DISP_rgb_model 			4
#define UF_FACET_NULL_FACET_ID 			5
#define UF_FACET_TYPE_DOUBLE 			6
#define UF_INTERSECT 				7
#define UF_MODL_CYLINDRICAL_FACE 		8
#define UF_MODL_PLANAR_FACE 			9
#define UF_MODL_TOROIDAL_FACE 			10
#define UF_NEGATIVE 				11
#define UF_OBJ_BLANKED 				12
#define UF_PART_ENGLISH 			13
#define UF_POSITIVE 				14
#define UF_SUBTRACT 				15
#define UF_TOP_TARGET 				16
#define UF_UNITE 				17
#define UF_UNLOAD_IMMEDIATELY 			18
#define UF_UNSIGNED 				19
#define UF_circle_type 				20
#define UF_faceted_model_normal_subtype 	21
#define UF_faceted_model_type 			22
#define UF_line_type 				23
#define UF_occ_instance_subtype 		24
#define UF_occ_instance_type 			25
#define UF_reference_set_type 			26
#define UF_solid_body_subtype 			27
#define UF_solid_type 				28

typedef int UF_FEATURE_SIGN;
typedef int logical;
typedef int tag_t;
typedef struct UF_ASSEM_options {int maintain_work_part;} UF_ASSEM_options_t;
typedef struct UF_CURVE_arc_s {double *arc_center; double start_angle; double end_angle; double radius;} UF_CURVE_arc_s_t;
typedef struct UF_CURVE_line_s {double *start_point; double*end_point;} UF_CURVE_line_s_t;
typedef struct UF_EVAL_arc_s {double radius;} UF_EVAL_arc_s_t;
typedef struct UF_FACET_parameters {int max_facet_edges; int number_storage_type; int specify_surface_tolerance; double surface_dist_tolerance; double surface_angular_tolerance;} UF_FACET_parameters_t;
typedef struct UF_OBJ_disp_props_s {int color; int blank_status;} UF_OBJ_disp_props_s_t;
typedef struct UF_PART_load_status{int n_parts;int failed;int user_abort;char **file_names;char *statuses;} UF_PART_load_status_t;
typedef struct UF_SKET_info {char *name; double *csys;} UF_SKET_info_t;
typedef void* UF_EVAL_p_t;
typedef void* UF_MODL_SWEEP_TRIM_object_p_t;
typedef void* uf_list_p_t;
typedef void* uf_list_t;

extern EXTLANG int UF_ASSEM_add_to_cset(tag_t,tag_t, int);
extern EXTLANG int UF_ASSEM_ask_assem_options(UF_ASSEM_options_t*);
extern EXTLANG int UF_ASSEM_ask_component_data(tag_t, char*, char*, char*, double*, double*, double(*)[4]);
extern EXTLANG int UF_ASSEM_ask_default_ref_sets(int*, char***);
extern EXTLANG int UF_ASSEM_ask_part_name_of_child(tag_t,const char*);
extern EXTLANG int UF_ASSEM_ask_part_occ_children(tag_t,tag_t**);
extern EXTLANG int UF_ASSEM_ask_ref_set_data(tag_t, char*, double*, double*, int*, tag_t**);
extern EXTLANG int UF_ASSEM_ask_suppress_state(tag_t, int*);
extern EXTLANG int UF_ASSEM_create_cset(tag_t,const char*, tag_t*);
extern EXTLANG int UF_ASSEM_cycle_inst_of_part(tag_t, tag_t);
extern EXTLANG int UF_ASSEM_is_occurrence(tag_t);
extern EXTLANG int UF_ASSEM_set_assem_options(UF_ASSEM_options_t*);
extern EXTLANG int UF_CURVE_ask_arc_data(tag_t, UF_CURVE_arc_s_t*);
extern EXTLANG int UF_CURVE_ask_line_data(tag_t, UF_CURVE_line_s_t*);
extern EXTLANG int UF_DISP_ask_color(int, int, char**, double*);
extern EXTLANG int UF_DISP_ask_color_count(int*);
extern EXTLANG int UF_EVAL_ask_arc(UF_EVAL_p_t, UF_EVAL_arc_s_t*);
extern EXTLANG int UF_EVAL_ask_limits(void*, double*);
extern EXTLANG int UF_EVAL_evaluate(void*,int,double,double*,void*);
extern EXTLANG int UF_EVAL_free(void*);
extern EXTLANG int UF_EVAL_initialize(tag_t,void*);
extern EXTLANG int UF_EVAL_is_arc(UF_EVAL_p_t, int*);
extern EXTLANG int UF_FACET_ask_default_parameters(UF_FACET_parameters_t*);
extern EXTLANG int UF_FACET_ask_face_id_of_facet(tag_t, int, int*);
extern EXTLANG int UF_FACET_ask_max_facet_verts(tag_t, int*);
extern EXTLANG int UF_FACET_ask_normals_of_facet(tag_t, int, int*, double(*)[3]);
extern EXTLANG int UF_FACET_ask_solid_face_of_facet(tag_t, int, tag_t*);
extern EXTLANG int UF_FACET_ask_vertices_of_facet(tag_t, int, int*, double(*)[3]);
extern EXTLANG int UF_FACET_cycle_facets(tag_t, int*);
extern EXTLANG int UF_FACET_facet_solid(tag_t, UF_FACET_parameters_t*, tag_t*);
extern EXTLANG int UF_FACET_set_default_parameters(UF_FACET_parameters_t*);
extern EXTLANG int UF_MODL_ask_ball_slot_parms(tag_t, int, char**, char**, char**, int*);
extern EXTLANG int UF_MODL_ask_body_feats(tag_t, uf_list_p_t*);
extern EXTLANG int UF_MODL_ask_bounding_box(tag_t, double*);
extern EXTLANG int UF_MODL_ask_c_bore_hole_parms(tag_t, int, char**, char**, char**, char**, char**, int*);
extern EXTLANG int UF_MODL_ask_c_sunk_hole_parms(tag_t, int, char**, char**, char**, char**, char**, int*);
extern EXTLANG int UF_MODL_ask_dovetail_slot_parms(tag_t, int, char**, char**, char**, char**, int*);
extern EXTLANG int UF_MODL_ask_exp_desc_of_feat(tag_t,int*,char***,tag_t**);
extern EXTLANG int UF_MODL_ask_exp_tag_value(tag_t,double*);
extern EXTLANG int UF_MODL_ask_extrusion(tag_t,int*,tag_t**,void**,char**,char**,char**,double*,int*,int*,double*);
extern EXTLANG int UF_MODL_ask_face_edges(tag_t, uf_list_p_t*);
extern EXTLANG int UF_MODL_ask_face_props(tag_t, double*, double*, double*, double*, double*, double*, double*, double*);
extern EXTLANG int UF_MODL_ask_face_type(tag_t, int*);
extern EXTLANG int UF_MODL_ask_face_uv_minmax(tag_t, double*);
extern EXTLANG int UF_MODL_ask_feat_direction(tag_t,double*,double*);
extern EXTLANG int UF_MODL_ask_feat_faces(tag_t, uf_list_p_t*);
extern EXTLANG int UF_MODL_ask_feat_location(tag_t,double*);
extern EXTLANG int UF_MODL_ask_feat_name(tag_t, char**);
extern EXTLANG int UF_MODL_ask_feat_relatives(tag_t, int*, tag_t**, int*, tag_t**);
extern EXTLANG int UF_MODL_ask_feat_type(tag_t, char**);
extern EXTLANG int UF_MODL_ask_feature_sign(tag_t, int*);
extern EXTLANG int UF_MODL_ask_features_of_mirror_set(tag_t, tag_t**, int*);
extern EXTLANG int UF_MODL_ask_list_count(void*, int*);
extern EXTLANG int UF_MODL_ask_list_item(void*, int, tag_t*);
extern EXTLANG int UF_MODL_ask_plane(tag_t, double*, double*);
extern EXTLANG int UF_MODL_ask_plane_of_mirror_set(tag_t, tag_t*);
extern EXTLANG int UF_MODL_ask_rect_slot_parms(tag_t, int, char**, char**, char**, int*);
extern EXTLANG int UF_MODL_ask_simple_hole_parms(tag_t, int, char**, char**, char**, int*);
extern EXTLANG int UF_MODL_ask_suppress_feature(tag_t, int*);
extern EXTLANG int UF_MODL_ask_sweep_curves(tag_t, int*, tag_t**, int*, tag_t**);
extern EXTLANG int UF_MODL_ask_t_slot_parms(tag_t, int, char**, char**, char**, char**,char**, int*);
extern EXTLANG int UF_MODL_ask_thru_faces(tag_t, tag_t*, tag_t*);
extern EXTLANG int UF_MODL_ask_u_slot_parms(tag_t, int, char**, char**, char**, char**, int*);
extern EXTLANG int UF_MODL_delete_list(void**);
extern EXTLANG int UF_MODL_delete_list_item(void**, tag_t);
extern EXTLANG int UF_MODL_dissect_exp_string(char*,char**,char**,tag_t*);
extern EXTLANG int UF_MODL_put_list_item(void*, tag_t);
extern EXTLANG int UF_MTX3_vec_multiply(double*, double*,point_t);
extern EXTLANG int UF_OBJ_ask_display_properties(tag_t, UF_OBJ_disp_props_s_t*);
extern EXTLANG int UF_OBJ_ask_type_and_subtype(tag_t,int*,int*);
extern EXTLANG int UF_OBJ_cycle_objs_in_part(tag_t, int, tag_t*);
extern EXTLANG int UF_OBJ_delete_object(tag_t);
extern EXTLANG int UF_PART_ask_part_name(tag_t, char*);
extern EXTLANG int UF_PART_ask_units(tag_t, int*);
extern EXTLANG int UF_PART_close_cset(tag_t);
extern EXTLANG int UF_PART_is_loaded(char*);
extern EXTLANG int UF_PART_open(char*, tag_t*, UF_PART_load_status_t*);
extern EXTLANG int UF_PART_open_cset(tag_t, UF_PART_load_status_t*);
extern EXTLANG int UF_PART_open_quiet(char*, tag_t*, UF_PART_load_status_t*);
extern EXTLANG int UF_PART_set_display_part(tag_t);
extern EXTLANG int UF_SKET_ask_feature_sketches(tag_t,void**);
extern EXTLANG int UF_SKET_ask_sketch_info(tag_t, UF_SKET_info_t*);
extern EXTLANG int UF_WAVE_ask_link_mirror_data(tag_t, int, tag_t*, tag_t*, tag_t*, tag_t*);
extern EXTLANG int UF_free_string_array(int,char**);
extern EXTLANG int UF_get_fail_message(int, char*);
extern EXTLANG tag_t UF_ASSEM_ask_child_of_instance(tag_t);
extern EXTLANG tag_t UF_ASSEM_ask_inst_of_part_occ(tag_t);
extern EXTLANG tag_t UF_ASSEM_ask_root_part_occ(tag_t);
extern EXTLANG tag_t UF_PART_ask_display_part();
extern EXTLANG void UF_PART_close(tag_t, int, int);
extern EXTLANG void UF_initialize();
extern EXTLANG void UF_terminate();
extern EXTLANG void uc6476(int);

#endif /* UG_SHIM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

