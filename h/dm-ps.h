#define	GED_TO_PS(x)	((int)((x)+2048))

struct ps_vars {
  struct bu_list l;
  void (*color_func)();
  genptr_t app_vars;   /* application specific variables */
};

extern FILE    *ps_fp;                 /* PostScript file pointer */
