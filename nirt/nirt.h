/*      NIRT.H          */

/*	CONSTANTS	*/
#define	VAR_NULL	((struct VarTable *) 0) 
#define	CT_NULL		((com_table *) 0)
#define	SILENT_UNSET	0
#define	SILENT_YES	1
#define	SILENT_NO	-1
#define	NIRT_PROMPT	"nirt>  "
#define	TITLE_LEN	80 
#if !defined(PI)
#define	PI		3.141592654
#endif
#define	BACKOUT_DIST	1000.0
#define	OFF		0
#define	ON		1
#define	YES		1
#define	NO		0
#define	HIT		1     /* HIT the target  */
#define	MISS		0     /* MISS the target */
#define	END		2
#define	HORZ		0
#define	VERT		1
#define	DIST		2
#define	POS		1   
#define	NEG		0
#define	AIR		1
#define	NO_AIR		0
#define	READING_FILE	1
#define	READING_STRING	2
#define	deg2rad		0.01745329

/*	FLAG VALUES FOR overlap_claims	*/
#define	OVLP_RESOLVE		0
#define	OVLP_REBUILD_FASTGEN	1
#define	OVLP_REBUILD_ALL	2
#define	OVLP_RETAIN		3

/*	FLAG VALUES FOR nirt_debug	*/
#define	DEBUG_INTERACT	0x001
#define	DEBUG_SCRIPTS	0x002
#define	DEBUG_MAT	0x004
#define	DEBUG_BACKOUT	0x008
#define	DEBUG_HITS	0x010
#ifdef	DEBUG_FORMAT
#   define RT_DEBUG_FMT	DEBUG_FORMAT
#endif
#define DEBUG_FMT	"\020\5HITS\4BACKOUT\3MAT\2SCRIPTS\1INTERACT"

/*	STRING FOR USE WITH GETOPT(3)	*/
#define	OPT_STRING      "A:bEe:f:MO:su:vx:X:?"

#define	made_it()	bu_log("Made it to %s:%d\n", __FILE__, __LINE__)

/*	MACROS WITH ARGUMENTS	*/
#define	max(a,b)	(((a)>(b))?(a):(b))
#define	min(a,b)	(((a)<(b))?(a):(b))
#if !defined(abs)
# define	abs(a)	((a)>=0 ? (a):(-a))
#endif
#define	com_usage(c)	fprintf (stderr, "Usage:  %s %s\n", \
				c -> com_name, c -> com_args);

/*	DATA STRUCTURES		*/
typedef struct {
	char	*com_name;		/* for invoking	    	         */
	void	(*com_func)();          /* what to do?      	         */
	char	*com_desc;		/* Help description 	         */
	char	*com_args;		/* Command arguments for usage   */
} com_table; 

struct VarTable 
{
	double	azimuth;
	double	elevation;
	vect_t  direct;
	vect_t  target;
	vect_t  grid;
};

extern struct rt_i		*rtip;
extern int		attrib_use;
extern int		attrib_cnt;
extern char		**attrib;
extern void		attrib_add(char *a);
extern void 		attrib_print(void);
extern void 		attrib_flush(void);
extern void		az_el();
extern void		dir_vect();
extern void	        grid_coor();
extern void		interact();
extern void	        target_coor();
extern void	        backout();
extern void		shoot();
extern void		sh_esc();
extern void	        quit();
extern void		show_menu();
extern void		format_output();
extern void		direct_output();
extern void		nirt_units();
extern void		do_overlap_claims();
extern void		use_air();
extern void		state_file();
extern void		dump_state();
extern void		load_state();
extern void		default_ospec();
extern void		print_item();
extern com_table	*get_comtab_ent();
extern void		read_mat();
extern void		ae2dir();
extern void		grid2targ();
extern void		targ2grid();
extern void		dir2ae();
extern void		set_diameter();
extern void		report();
extern int		check_conv_spec();
extern void                   do_rt_gettrees();
extern int need_prep;
