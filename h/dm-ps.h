#ifndef SEEN_DM_PS
#define SEEN_DM_PS

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2047,
 *  and we define the Postscript file to use 0..4095
 */
#define	GED_TO_PS(x)	((int)((x)+2048))

struct ps_vars {
  struct bu_list l;
  FILE *ps_fp;
  char ttybuf[BUFSIZ];
  vect_t clipmin;
  vect_t clipmax;
  struct bu_vls fname;
  struct bu_vls font;
  struct bu_vls title;
  struct bu_vls creator;
  fastf_t scale;
  int linewidth;
};

extern struct dm *PS_open();
extern struct ps_vars head_ps_vars;

#endif /* SEEN_DM_PS */
