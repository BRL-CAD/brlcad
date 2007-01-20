/*                          N I R T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file nirt.h
 *
 */

/*      NIRT.H          */

/* Needed for some struct definitions */
#include "./usrfmt.h"

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
#define	OPT_STRING      "A:bB:Ee:f:MO:su:vx:X:?"

#define	made_it()	bu_log("Made it to %s:%d\n", __FILE__, __LINE__)

/*	MACROS WITH ARGUMENTS	*/
#ifndef _WIN32
/* already defined */
#  define	max(a,b)	(((a)>(b))?(a):(b))
#  define	min(a,b)	(((a)<(b))?(a):(b))
#endif
#if !defined(abs)
#  define	abs(a)	((a)>=0 ? (a):(-a))
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

typedef struct attributes {
    int	attrib_use;
    int	attrib_cnt;
    char **attrib;
} attr_table;

extern attr_table a_tab;

extern struct rt_i	*rtip;
extern void		attrib_add(char *a);
extern void 		attrib_print(void);
extern void 		attrib_flush(void);
extern void		az_el();
extern void		dir_vect();
extern void	        grid_coor();
extern void		interact(int input_source, void *sPtr);
extern void	        target_coor();
extern void	        backout();
extern void		shoot();
extern void		sh_esc();
extern void	        quit();
extern void		show_menu();
extern void		format_output(char *buffer, com_table *ctp);
extern void		direct_output(char *buffer, com_table *ctp);
extern void		nirt_units();
extern void		do_overlap_claims();
extern void		use_air();
extern void		state_file(char *buffer, com_table *ctp);
extern void		dump_state(char *buffer, com_table *ctp);
extern void		load_state(char *buffer, com_table *ctp);
extern void		default_ospec(void);
extern void		print_item(char *buffer, com_table *ctp);
extern com_table	*get_comtab_ent(char *pattern, int pat_len);
extern void		read_mat(void);
extern void		ae2dir(void);
extern void		grid2targ(void);
extern void		targ2grid(void);
extern void		dir2ae(void);
extern void		set_diameter(struct rt_i *rtip);
extern void		report(int outcom_type);
extern int		check_conv_spec(outitem *oip);
extern void             do_rt_gettrees(struct rt_i *rtip, char **object_name, int nm_objects);
extern void		bot_minpieces();
extern int		need_prep;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
