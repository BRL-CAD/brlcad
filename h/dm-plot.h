#ifndef SEEN_DM_PLOT
#define SEEN_DM_PLOT

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  and we define the PLOT file to use the same space.  Easy!
 */
#define	GED_TO_PLOT(x)	(x)
#define PLOT_TO_GED(x)	(x)

struct plot_vars {
  struct bu_list l;
  FILE *up_fp;
  char ttybuf[BUFSIZ];
#if 0
  int floating;
  int zclip;
#endif
  int grid;
  int is_3D;
  int is_pipe;
  vect_t clipmin;
  vect_t clipmax;
  struct bu_vls vls;
};

extern struct plot_vars head_plot_vars;

#endif /* SEEN_DM_PLOT */

