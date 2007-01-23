/*                           L G T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file lgt.h
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647

	$Header$ (BRL)
*/
#ifndef INCL_LGT
#define INCL_LGT

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif


#define OVERLAPTOL	0.25	/* Thinner overlaps won't be reported. */
#define MAX_SCANSIZE	2048	/* Longest output scanline. */
#define MAX_HL_SIZE	2048	/* Maximum size of hidden-line image. */
#define MAX_LGTS	10	/* Maximum number of light sources. */
#define MAX_LGT_NM	16
#define MAX_LN		81
#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif
#ifndef true
#define true		1
#define false		0
#endif
#define Swap_Integers( i_, j_ ) \
		{	register int	k_ = i_; \
		i_ = j_; \
		j_ = k_; \
		}
#define Toggle(f)	(f) = !(f)
#define Malloc_Bomb( _bytes_ ) \
		fb_log( "\"%s\"(%d) : allocation of %d bytes failed.\n", \
				__FILE__, __LINE__, _bytes_ )

/* Guess whether or not a frame buffer name is a disk file. (XXX) */
#define DiskFile(fil)	(*fil != '\0'\
			&& strncmp(fil, "/dev", 4 ) != 0 \
			&& strchr( fil, ':' ) == (char *)NULL)

/* Values for grid type. */
#define GT_RPP_CENTERED	0 /* Grid origin aligned with centroid of model RPP. */
#define GT_ORG_CENTERED	1 /* Grid aligned with model origin. */

/* Flag (hiddenln_draw) values for hidden line drawing. */
#define HL_DISABLED		0
#define HL_ENABLED		1
#define HL_REVERSE_VIDEO	2

/* Flag (pix_buffered) values for writing pixels to the frame buffer.	*/
#define B_PIO		0	/* Programmed I/O.			*/
#define B_PAGE		1	/* Buffered I/O (DMA paging scheme).	*/
#define B_LINE		2	/* Line-buffered I/O (DMA).		*/
#define Rotate( f )	(f) = (f) + 1 > 2 ? 0 : (f) + 1

#ifndef DEBUG_OFF /* This is from "librt/debug.h", #include <debug.h> would
			be better.					*/
#define DEBUG_OFF	0	/* No debugging */

/* These definitions are each for one bit */

/* Options useful for debugging applications */
#define DEBUG_ALLRAYS	0x00000001	/* Print calls to shootray() */
#define DEBUG_SHOOT	0x00000002	/* Info about shootray() processing */

/* Options useful for debugging the database */
#define DEBUG_DB	0x00000010	/* Database debugging */
#define DEBUG_SOLIDS	0x00000020	/* Print prep'ed solids */
#define DEBUG_REGIONS	0x00000040	/* Print regions & boolean trees */
#define DEBUG_ARB8	0x00000080	/* Print voluminus ARB8 details */
#define DEBUG_SPLINE	0x00000100	/* Splines */

/* Options useful for debugging the library */
#define DEBUG_ROOTS	0x00001000	/* Print rootfinder details */
#define DEBUG_PARTITION	0x00002000	/* Info about bool_weave() */
#define DEBUG_CUT	0x00004000	/* Print space cutting details */
#define DEBUG_BOXING	0x00008000	/* Object/box checking details */
#define DEBUG_MEM	0x00010000	/* Debug dynamic memory operations */
#define DEBUG_TESTING	0x00020000	/* One-shot debugging flag */
#define DEBUG_FDIFF	0x00040000	/* bool/fdiff debugging */

/* These will cause binary debugging output */
#define DEBUG_PLOTBOX	0x80000000	/* Plot(3) bounding boxes to stdout */
#endif

/* Application debugging flags.						*/
#define DEBUG_RGB	0x80000
#define DEBUG_REFRACT	0x100000
#define DEBUG_NORML	0x200000
#define DEBUG_SHADOW	0x400000
#define DEBUG_CELLSIZE	0x800000
#define DEBUG_OCTREE	0x1000000

typedef int	bool;

/* Light source (LS) specific global information.
	Directions are with respect to the center of the model as calculated
	by 'librt.a'.
 */
typedef struct
	{
	char	name[MAX_LGT_NM];/* Name of entry (i.e. ambient).	*/
	int	beam;	/* Flag denotes gaussian beam intensity.	*/
	int	over;	/* Flag denotes manual overide of position.	*/
	int	rgb[3];	/* Pixel color of LS (0 to 255) for RGB.	*/
	fastf_t	loc[3];	/* Location of LS in model space.		*/
	fastf_t	azim;	/* Azimuthal direction of LS in radians.	*/
	fastf_t	elev;	/* Elevational direction of LS in radians.	*/
	fastf_t	dir[3];	/* Direction vector to LS.			*/
	fastf_t	dist;	/* Distance to LS in from centroid of model.	*/
	fastf_t	energy;	/* Intensity of LS.				*/
	fastf_t	coef[3];/* Color of LS as coefficient (0.0 to 1.0).	*/
	fastf_t	radius;	/* Radius of beam.				*/
	struct soltab	*stp;	/* Solid table pointer to LIGHT solid.	*/
	}
Lgt_Source;
#define LGT_NULL	(Lgt_Source *) NULL

typedef struct
	{
	bool	m_fullscreen;
	bool	m_lgts;
	bool	m_over;
	bool	m_keys;
	int	m_noframes;
	int	m_curframe;
	int	m_endframe;
	int	m_frame_sz;
	FILE	*m_keys_fp;
	fastf_t	m_azim_beg;
	fastf_t m_azim_end;
	fastf_t	m_elev_beg;
	fastf_t m_elev_end;
	fastf_t	m_roll_beg;
	fastf_t m_roll_end;
	fastf_t	m_dist_beg;
	fastf_t m_dist_end;
	fastf_t	m_grid_beg;
	fastf_t m_grid_end;
	fastf_t	m_pers_beg;
	fastf_t m_pers_end;
	}
Movie;
#define MovieSize( sz, nf )	(int)sqrt((double)(nf)+0.5)*(sz)
#define IK_INTENSITY	255.0
#define RGB_INVERSE	(1.0 / IK_INTENSITY)
#define EYE_SIZE	12.7
#define TITLE_LEN	72
#define TIMER_LEN	72

extern Lgt_Source	lgts[];
extern Movie		movie;

#endif /* INCL_LGT */
char		*get_Input();
int		setup_Lgts();
void		user_Input();
int		interpolate_Frame();
int		ready_Output_Device();
int		exec_Shell();
void		set_Cbreak();
void		clr_Echo();
int		pixel_To_Temp();
void		reset_Tty();
int		read_IR();
int		write_Trie();
int		lgt_Save_Db();
int		read_Trie();
int		lgt_Rd_Db();
int		ClrText();
int		MvCursor();
int		InitTermCap();
int		SetScrlReg();
int		ClrEOL();
int		ResetScrlReg();
int		DeleteLn();
int		init_Temp_To_RGB();
int		get_Font();
int		do_More();
int		append_PtList();
int		delete_Node_OcList();
int		SetStandout();
int		ClrStandout();
int		save_Tty();
int		set_Raw();
int		ir_shootray_octree();
int		ir_Chk_Table();
int		tex_Entry();
int		icon_Entry();
int		write_Octree();


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
