/*
	Authors:	Gary S. Moss
			Jeff Hanes	(math consultation)

			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651
*/
#if ! defined(FAST)
#include "machine.h"
#endif

#if ! defined(INCL_FB)
#include "fb.h"
#endif

#if ! defined(INCL_HM)
#include "./hmenu.h"
#endif

/* Set pre-processor switch for getting signal() handler declarations right.
 */
#if __STDC__ || (defined(SYSV) && ! defined(cray))
#define STD_SIGNAL_DECLS 1
#else
#define STD_SIGNAL_DECLS 0
#endif

#if STD_SIGNAL_DECLS
extern void (*norml_sig)(), (*abort_sig)();
extern void abort_RT();
#else
extern int (*norml_sig)(), (*abort_sig)();
extern int abort_RT();
#endif

/* C library functions. */
#if __STDC__
#include <stdlib.h>
#else
#	ifdef BSD
	extern int exit();
	extern int free();
#	else
	extern void exit();
	extern void free();
#	endif
extern char *getenv();
extern char *malloc();
#endif
extern char *sbrk();
extern char *strcpy(), *strncpy();
#ifdef BSD
extern char *tmpnam(), *gets(), *strtok();
extern int perror();
#else
extern void perror();
#endif

/* other functions */
extern char *get_Input();

extern fastf_t pow_Of_2();

extern int fb_Setup();
extern int f_IR_Model(), f_IR_Backgr();
extern int get_Answer();
extern int lgt_Edit_Db_Entry();
extern int lgt_Print_Db();
extern int pars_Argv();

extern void append_Octp();
extern void close_Output_Device();
extern void cons_Vector();
extern void delete_OcList();
extern void display_Temps();
extern void do_line();
extern void exit_Neatly();
extern void fb_Zoom_Window();
extern void grid_Rotate();
extern void init_Status();
extern void loc_Perror();
extern void note_IRmapping();
extern void prnt_Event();
extern void prnt_IR_Status();
extern void prnt_Lgt_Status();
extern void prnt_Menu();
extern void prnt_Octree();
extern void prnt_Pixel();
extern void prnt_Prompt();
extern void prnt_Scroll();
extern void prnt_Status();
extern void prnt_Timer();
extern void prnt_Title();
extern void prnt_Trie();
extern void prnt_Usage();
extern void prnt3vec();
extern void render_Model();
extern void ring_Bell();
extern void rt_log();
extern void set_IRmapping();
extern void set_Size_Grid();
extern void update_Screen();
extern void user_Interaction();

/* variables */
extern FBIO *fbiop;

extern RGBpixel bgpixel;
extern RGBpixel *ir_table;

extern char *CS, *DL;
extern char *beginptr;
extern char *ged_file;
extern char **template;

extern char err_file[];
extern char fb_file[];
extern char input_ln[];
extern char ir_file[];
extern char ir_db_file[];
extern char lgt_db_file[];
extern char mat_db_file[];
extern char prompt[];
extern char script_file[];
extern char title[];
extern char timer[];
extern char version[];

extern fastf_t bg_coefs[];
extern fastf_t cell_sz;
extern fastf_t degtorad;
extern fastf_t grid_dist;
extern fastf_t grid_hor[];
extern fastf_t grid_ver[];
extern fastf_t grid_loc[];
extern fastf_t grid_scale;
extern fastf_t grid_roll;
extern fastf_t modl_cntr[];
extern fastf_t modl_radius;
extern fastf_t x_grid_offset, y_grid_offset;
extern fastf_t rel_perspective;
extern fastf_t sample_sz;
extern fastf_t view2model[];
extern fastf_t view_rots[];
extern fastf_t view_size;
extern int LI, CO;
extern int anti_aliasing;
extern int aperture_sz;
extern int background[];
extern int co;
extern int debug;
extern int errno;
extern int fatal_error;
extern int frame_no;
extern int grid_sz;
extern int grid_position;
extern int grid_x_org, grid_y_org;
extern int grid_x_fin, grid_y_fin;
extern int grid_x_cur, grid_y_cur;
extern int hiddenln_draw;
extern int ir_aperture;
extern int ir_offset;
extern int ir_min, ir_max;
extern int ir_paint;
extern int ir_doing_paint;
extern int ir_mapx, ir_mapy;
extern int ir_noise;
extern int ir_mapping;
extern int lgt_db_size;
extern int li;
extern int max_bounce;
extern int npsw;
extern int pix_buffered;
extern int query_region;
extern int report_overlaps;
extern int save_view_flag;
extern int sgi_console;
extern int sgi_usemouse;
extern int shadowing;
extern int tracking_cursor;
extern int tty;
extern int user_interrupt;
extern int x_fb_origin, y_fb_origin;
extern int zoom;

extern unsigned char arrowcursor[];
extern unsigned char menucursor[];
extern unsigned char sweeportrack[];
extern unsigned char target1[];

extern struct resource resource[];
extern struct rt_i *rt_ip;

#define C_TAGPIXEL	0
#define C_SWEEPREC	1
#define C_I_WINDOW	2
#define C_O_WINDOW	3
#define C_QUERYREG	4
#define XSCR2MEM(_x)	(_x)
#define YSCR2MEM(_y)	(_y)

#ifdef sgi
extern int win_active;
extern long defpup(), qtest();
#define	SGI_XCVT( v_ ) (((v_) - xwin) / (fbiop->if_width/grid_sz))
#define SGI_YCVT( v_ ) (((v_) - ywin) / (fbiop->if_width/grid_sz))
#else
#define	SGI_XCVT( v_ ) (v_)
#define SGI_YCVT( v_ ) (v_)
#endif
