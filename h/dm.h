#ifndef SEEN_DM_H
#define SEEN_DM_H

/*  Colors */
#define DM_BLACK	0
#define DM_RED		1
#define DM_BLUE		2
#define DM_YELLOW	3
#define DM_WHITE	4

/* Command parameter to dmr_viewchange() */
#define DM_CHGV_REDO	0	/* Display has changed substantially */
#define DM_CHGV_ADD	1	/* Add an object to the display */
#define DM_CHGV_DEL	2	/* Delete an object from the display */
#define DM_CHGV_REPL	3	/* Replace an object */
#define DM_CHGV_ILLUM	4	/* Make new object the illuminated object */

/*
 * Definitions for dealing with the buttons and lights.
 * BV are for viewing, and BE are for editing functions.
 */
#define LIGHT_OFF	0
#define LIGHT_ON	1
#define LIGHT_RESET	2		/* all lights out */

/* Interface to a specific Display Manager */
struct dm {
  int   (*dmr_init)();         /* Called first */
  int	(*dmr_open)();
  void	(*dmr_close)();
  void	(*dmr_input)();
  void	(*dmr_prolog)();
  void	(*dmr_epilog)();
  void	(*dmr_normal)();
  void	(*dmr_newrot)();
  void	(*dmr_update)();
  void	(*dmr_puts)();
  void	(*dmr_2d_line)();
  void	(*dmr_light)();
  int	(*dmr_object)();	/* Invoke an object subroutine */
  unsigned (*dmr_cvtvecs)();	/* returns size requirement of subr */
  unsigned (*dmr_load)();	/* DMA the subr to device */
  void	(*dmr_statechange)();	/* application provided -- called on editor state change */
  void	(*dmr_viewchange)();	/* add/drop solids from view */
  void	(*dmr_colorchange)();	/* called when color table changes */
  void	(*dmr_window)();	/* Change window boundry */
  void	(*dmr_debug)();		/* Set DM debug level */
  int	(*dmr_cmd)();		/* application provided dm-specific command handler */
  int	(*dmr_eventhandler)();	/* application provided dm-specific event handler */
  int	dmr_displaylist;	/* !0 means device has displaylist */
  int	dmr_releasedisplay;	/* !0 release for other programs */
  double	dmr_bound;		/* zoom-in limit */
  char	*dmr_name;		/* short name of device */
  char	*dmr_lname;		/* long name of device */
  struct mem_map *dmr_map;	/* displaylist mem map */
  genptr_t dmr_vars;		/* pointer to display manager dependant variables */
  struct bu_vls dmr_pathName;	/* full Tcl/Tk name of drawing window */
  char	dmr_dname[80];		/* Display name */
  fastf_t *dmr_vp;              /* Viewscale pointer */
  void (*dmr_cfunc)();          /* XXX application provided color function */
  struct solid *dmr_hp;         /* XXX - Temporary */
};

extern void Nu_void();
extern int Nu_int0();
extern unsigned Nu_unsign();
extern Tcl_Interp *interp;
#endif /* SEEN_DM_H */
