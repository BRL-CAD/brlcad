#ifndef MGEDTCL_H
#define MGEDTCL_H seen

extern Tcl_Interp *interp;

/* chgtree.c */
extern Tcl_CmdProc cmd_oed,
                   cmd_pathlist;

/* cmd.c */
extern Tcl_CmdProc cmd_apropos,
                   cmd_echo,
                   cmd_getknob,
                   cmd_nop,
                   cmd_output_hook,
                   cmd_tk;
/* db.c */
extern Tcl_CmdProc cmd_db;

/* dir.c */
extern Tcl_CmdProc cmd_expand;

/* history.c */
extern Tcl_CmdProc cmd_hist_add,
                   cmd_next,
                   cmd_prev;

/* menu.c */
extern Tcl_CmdProc cmd_mmenu_get;

/* rtif.c */
extern Tcl_CmdProc cmd_solids_on_ray;

/* scroll.c */
extern Tcl_CmdProc cmd_sliders;

#define MGED_DISPLAY_VAR "mged_display"

#endif
