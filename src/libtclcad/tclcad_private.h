/*                T C L C A D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2012-2021 United States Government as represented by
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
/**
 *
 * Private header for libtclcad.
 *
 */

#ifndef LIBTCLCAD_TCLCAD_PRIVATE_H
#define LIBTCLCAD_TCLCAD_PRIVATE_H

#include "common.h"

#include <string.h>
#include <tcl.h>

#include "ged/defines.h"

__BEGIN_DECLS

#define TO_UNLIMITED -1

extern struct tclcad_obj HeadTclcadObj;
extern struct tclcad_obj *current_top;

struct path_edit_params {
    int edit_mode;
    double dx;
    double dy;
    mat_t edit_mat;
};

/**
 * function returns truthfully whether the library has been
 * initialized.  calling this routine with setit true considers the
 * library henceforth initialized.  there is presently no way to unset
 * or reset initialization.
 */
extern int library_initialized(int setit);


/**
 * Evaluates a TCL command, escaping the list of arguments.
 */
extern int tclcad_eval(Tcl_Interp *interp, const char *command, size_t num_args, const char * const *args);


/**
 * Evaluates a TCL command, escaping the list of arguments and preserving the TCL result object.
 */
extern int tclcad_eval_noresult(Tcl_Interp *interp, const char *command, size_t num_args, const char * const *args);


/* Tcl initialization routines */
extern int Bu_Init(Tcl_Interp *interp);
extern int Bn_Init(Tcl_Interp *interp);
extern int Cho_Init(Tcl_Interp *interp);
extern int Dm_Init(Tcl_Interp *interp);
extern int Dmo_Init(Tcl_Interp *interp);
extern int Fbo_Init(Tcl_Interp *interp);
extern int Ged_Init(Tcl_Interp *interp);
extern int Rt_Init(Tcl_Interp *interp);

/* Fb functions */
extern int to_close_fbs(struct bview *gdvp);
extern void to_fbs_callback();
extern int to_open_fbs(struct bview *gdvp, Tcl_Interp *interp);
extern int to_set_fb_mode(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
extern int to_listen(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *usage,
		     int maxargs);

/* Dm functions */
extern int
dm_list_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    const char **argv);

/* Tclcad mouse routines */
extern int to_get_prev_mouse(struct ged *gedp,
                            int argc,
                            const char *argv[],
                            ged_func_ptr func,
                            const char *usage,
                            int maxargs);
extern int to_mouse_append_pnt_common(struct ged *gedp,
                                     int argc,
                                     const char *argv[],
                                     ged_func_ptr func,
                                     const char *usage,
                                     int maxargs);
extern int to_mouse_brep_selection_append(struct ged *gedp,
                                         int argc,
                                         const char *argv[],
                                         ged_func_ptr func,
                                         const char *usage,
                                         int maxargs);
extern int to_mouse_brep_selection_translate(struct ged *gedp,
                                            int argc,
                                            const char *argv[],
                                            ged_func_ptr func,
                                            const char *usage,
                                            int maxargs);
extern int to_mouse_constrain_rot(struct ged *gedp,
                                 int argc,
                                 const char *argv[],
                                 ged_func_ptr func,
                                 const char *usage,
                                 int maxargs);
extern int to_mouse_constrain_trans(struct ged *gedp,
                                   int argc,
                                   const char *argv[],
                                   ged_func_ptr func,
                                   const char *usage,
                                   int maxargs);
extern int to_mouse_find_arb_edge(struct ged *gedp,
                                 int argc,
                                 const char *argv[],
                                 ged_func_ptr func,
                                 const char *usage,
                                 int maxargs);
extern int to_mouse_find_bot_edge(struct ged *gedp,
                                 int argc,
                                 const char *argv[],
                                 ged_func_ptr func,
                                 const char *usage,
                                 int maxargs);
extern int to_mouse_find_bot_pnt(struct ged *gedp,
                                int argc,
                                const char *argv[],
                                ged_func_ptr func,
                                const char *usage,
                                int maxargs);
extern int to_mouse_find_metaball_pnt(struct ged *gedp,
                                     int argc,
                                     const char *argv[],
                                     ged_func_ptr func,
                                     const char *usage,
                                     int maxargs);
extern int to_mouse_find_pipe_pnt(struct ged *gedp,
                                 int argc,
                                 const char *argv[],
                                 ged_func_ptr func,
                                 const char *usage,
                                 int maxargs);
extern int to_mouse_joint_select(struct ged *gedp,
                                int argc,
                                const char *argv[],
                                ged_func_ptr func,
                                const char *usage,
                                int maxargs);
extern int to_mouse_joint_selection_translate(struct ged *gedp,
                                             int argc,
                                             const char *argv[],
                                             ged_func_ptr func,
                                             const char *usage,
                                             int maxargs);
extern int to_mouse_move_arb_edge(struct ged *gedp,
                                 int argc,
                                 const char *argv[],
                                 ged_func_ptr func,
                                 const char *usage,
                                 int maxargs);
extern int to_mouse_move_arb_face(struct ged *gedp,
                                 int argc,
                                 const char *argv[],
                                 ged_func_ptr func,
                                 const char *usage,
                                 int maxargs);
extern int to_mouse_move_bot_pnt(struct ged *gedp,
                                int argc,
                                const char *argv[],
                                ged_func_ptr func,
                                const char *usage,
                                int maxargs);
extern int to_mouse_move_bot_pnts(struct ged *gedp,
                                 int argc,
                                 const char *argv[],
                                 ged_func_ptr func,
                                 const char *usage,
                                 int maxargs);
extern int to_mouse_move_pnt_common(struct ged *gedp,
                                   int argc,
                                   const char *argv[],
                                   ged_func_ptr func,
                                   const char *usage,
                                   int maxargs);
extern int to_mouse_orotate(struct ged *gedp,
                           int argc,
                           const char *argv[],
                           ged_func_ptr func,
                           const char *usage,
                           int maxargs);
extern int to_mouse_oscale(struct ged *gedp,
                          int argc,
                          const char *argv[],
                          ged_func_ptr func,
                          const char *usage,
                          int maxargs);
extern int to_mouse_otranslate(struct ged *gedp,
                              int argc,
                              const char *argv[],
                              ged_func_ptr func,
                              const char *usage,
                              int maxargs);
extern int to_mouse_poly_circ(struct ged *gedp,
                             int argc,
                             const char *argv[],
                             ged_func_ptr func,
                             const char *usage,
                             int maxargs);
extern int to_mouse_poly_circ_func(Tcl_Interp *interp,
                                  struct ged *gedp,
                                  struct bview *gdvp,
                                  int argc,
                                  const char *argv[],
                                  const char *usage);
extern int to_mouse_poly_cont(struct ged *gedp,
                             int argc,
                             const char *argv[],
                             ged_func_ptr func,
                             const char *usage,
                             int maxargs);
extern int to_mouse_poly_cont_func(Tcl_Interp *interp,
                                  struct ged *gedp,
                                  struct bview *gdvp,
                                  int argc,
                                  const char *argv[],
                                  const char *usage);
extern int to_mouse_poly_ell(struct ged *gedp,
                            int argc,
                            const char *argv[],
                            ged_func_ptr func,
                            const char *usage,
                            int maxargs);
extern int to_mouse_poly_ell_func(Tcl_Interp *interp,
                                 struct ged *gedp,
                                 struct bview *gdvp,
                                 int argc,
                                 const char *argv[],
                                 const char *usage);
extern int to_mouse_poly_rect(struct ged *gedp,
                             int argc,
                             const char *argv[],
                             ged_func_ptr func,
                             const char *usage,
                             int maxargs);
extern int to_mouse_poly_rect_func(Tcl_Interp *interp,
                                  struct ged *gedp,
                                  struct bview *gdvp,
                                  int argc,
                                  const char *argv[],
                                  const char *usage);
extern int to_mouse_ray(struct ged *gedp,
                       int argc,
                       const char *argv[],
                       ged_func_ptr func,
                       const char *usage,
                       int maxargs);
extern int to_mouse_rect(struct ged *gedp,
                        int argc,
                        const char *argv[],
                        ged_func_ptr func,
                        const char *usage,
                        int maxargs);
extern int to_mouse_rot(struct ged *gedp,
                       int argc,
                       const char *argv[],
                       ged_func_ptr func,
                       const char *usage,
                       int maxargs);
extern int to_mouse_rotate_arb_face(struct ged *gedp,
                                   int argc,
                                   const char *argv[],
                                   ged_func_ptr func,
                                   const char *usage,
                                   int maxargs);
extern int to_mouse_data_scale(struct ged *gedp,
                              int argc,
                              const char *argv[],
                              ged_func_ptr func,
                              const char *usage,
                              int maxargs);
extern int to_mouse_scale(struct ged *gedp,
                         int argc,
                         const char *argv[],
                         ged_func_ptr func,
                         const char *usage,
                         int maxargs);
extern int to_mouse_protate(struct ged *gedp,
                           int argc,
                           const char *argv[],
                           ged_func_ptr func,
                           const char *usage,
                           int maxargs);
extern int to_mouse_pscale(struct ged *gedp,
                          int argc,
                          const char *argv[],
                          ged_func_ptr func,
                          const char *usage,
                          int maxargs);
extern int to_mouse_ptranslate(struct ged *gedp,
                              int argc,
                              const char *argv[],
                              ged_func_ptr func,
                              const char *usage,
                              int maxargs);
extern int to_mouse_trans(struct ged *gedp,
                         int argc,
                         const char *argv[],
                         ged_func_ptr func,
                         const char *usage,
                         int maxargs);

/* Tclcad polygon routines */
extern int to_data_polygons_func(Tcl_Interp *interp,
                                 struct ged *gedp,
                                 struct bview *gdvp,
                                 int argc,
                                 const char *argv[]);
extern int to_data_polygons(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);

extern int to_poly_circ_mode(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
extern int to_poly_circ_mode_func(Tcl_Interp *interp,
				  struct ged *gedp,
				  struct bview *gdvp,
				  int argc,
				  const char *argv[],
				  const char *usage);
extern int to_poly_cont_build(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
extern int to_poly_cont_build_end(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
extern int to_poly_cont_build_end_func(struct bview *gdvp,
				       int argc,
				       const char *argv[]);
extern int to_poly_ell_mode(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
extern int to_poly_ell_mode_func(Tcl_Interp *interp,
				 struct ged *gedp,
				 struct bview *gdvp,
				 int argc,
				 const char *argv[],
				 const char *usage);
extern int to_poly_rect_mode(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
extern int to_poly_rect_mode_func(Tcl_Interp *interp,
				  struct ged *gedp,
				  struct bview *gdvp,
				  int argc,
				  const char *argv[],
				  const char *usage);


/* Tclcad obj wrapper routines */
extern int to_pass_through_func(struct ged *gedp,
				int argc,
				const char *argv[],
				ged_func_ptr func,
				const char *usage,
				int maxargs);
extern int to_pass_through_and_refresh_func(struct ged *gedp,
					    int argc,
					    const char *argv[],
					    ged_func_ptr func,
					    const char *usage,
					    int maxargs);
extern int to_view_func(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
extern int to_view_func_common(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs,
			       int cflag,
			       int rflag);
extern int to_view_func_less(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
extern int to_view_func_plus(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
extern int to_dm_func(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs);

extern int to_more_args_func(struct ged *gedp,
                             int argc,
                             const char *argv[],
                             ged_func_ptr func,
                             const char *usage,
                             int maxargs);
__END_DECLS

#endif /* LIBTCLCAD_TCLCAD_PRIVATE_H */

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
