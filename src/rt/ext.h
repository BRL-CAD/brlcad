/*                           E X T . H
 * BRL-CAD
 *
 * Copyright (C) 1989-2005 United States Government as represented by
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
/** @file ext.h
 *
 *  External variable declarations for the RT family of analysis programs.
 *
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  
 *  @(#)$Header$
 */

/***** Variables declared in viewXXX.c */
extern int		use_air;		/* Handling of air in librt */
/***** end of sharing with viewing model *****/

/***** Variables declared in liboptical or multispectral *****/
#ifdef RT_MULTISPECTRAL
extern const struct bn_table		*spectrum;
#endif  /* RT_MULTISPECTRAL */

/***** Variables declared in opt.c *****/
extern char		*framebuffer;		/* desired framebuffer */
extern double		azimuth, elevation;
extern int		lightmodel;		/* Select lighting model */
extern int		rpt_overlap;		/* Warn about overlaps? */
extern int		rpt_dist;		/* Output depth along w/ RGB? */
extern int		space_partition;		/* Space partitioning algorithm to use */
extern int		nugrid_dimlimit;		/* limit to dimensions of nugrid; <= 0 means no limit */
extern double		nu_gfactor;		/* constant factor in NUgrid algorithm */
extern int		sub_grid_mode;		/* mode to raytrace a rectangular portion of view */
extern int		sub_xmin;		/* lower left of sub rectangle */
extern int		sub_ymin;
extern int		sub_xmax;		/* upper right of sub rectangle */
extern int		sub_ymax;
extern int              transpose_grid;         /* reverse the order of grid traversal */

/***** variables from main.c *****/
extern FILE		*outfp;			/* optional output file */
extern int		output_is_binary;	/* !0 means output is binary */
extern mat_t		view2model;
extern mat_t		model2view;
extern vect_t		left_eye_delta;
extern int		report_progress;	/* !0 = user wants progress report */
extern struct application	ap;
extern vect_t		left_eye_delta;
extern int		save_overlaps;		/* flag for setting rti_save_overlaps */

/***** variables shared with worker() ******/
extern int		stereo;			/* stereo viewing */
extern int		hypersample;		/* number of extra rays to fire */
extern unsigned int	jitter;			/* jitter (bit vector) */
#define JITTER_CELL	0x1			/* jitter position of ray in each cell */
#define JITTER_FRAME	0x2			/* jitter position of entire frame */
extern fastf_t		rt_perspective;		/* presp (degrees X) 0 => ortho */
extern fastf_t		aspect;			/* view aspect ratio X/Y */
extern point_t		viewbase_model;		/* model-space location of viewplane corner */
extern vect_t		dx_model;		/* view delta-X as model-space vect (width of pixel as vector) */
extern vect_t		dy_model;		/* view delta-Y as model-space vect (height of pixel as vector) */
extern vect_t		dx_unit;		/* unit-len dir vector of pixel side-to-side */
extern vect_t		dy_unit;		/* unit-len dir vector of pixel top-to-bottom */
extern fastf_t		cell_width;		/* model space grid cell width */
extern fastf_t		cell_height;		/* model space grid cell height */
extern int		cell_newsize;		/* new grid cell size (for worker) */
extern point_t		eye_model;		/* model-space location of eye */
extern fastf_t		eye_backoff;		/* dist from eye to center */
extern int		width;			/* # of pixels in X */
extern int		height;			/* # of lines in Y */
extern mat_t		Viewrotscale;
extern fastf_t		viewsize;
extern char		*scanbuf;		/* pixels for REMRT */
extern int		incr_mode;		/* !0 for incremental resolution */
extern int		incr_level;		/* current incremental level */
extern int		incr_nlevel;		/* number of levels */
extern int		npsw;			/* number of worker PSWs to run */
extern struct resource	resource[];		/* memory resources */
extern int		fullfloat_mode;
extern int		reproject_mode;
extern int		reproj_cur;	/* number of pixels reprojected this frame */
extern int		reproj_max;	/* out of total number of pixels */
extern struct floatpixel	*curr_float_frame;	/* buffer of full frame */
extern struct floatpixel	*prev_float_frame;
/***** end variables shared with worker() *****/

/***** Photon Mapping Variables *****/
extern	double		pmargs[9];
extern	char		pmfile[255];
/***** ************************ *****/

/***** variables shared with do.c *****/
extern int		pix_start;		/* pixel to start at */
extern int		pix_end;		/* pixel to end at */
extern int		nobjs;			/* Number of cmd-line treetops */
extern char		**objtab;		/* array of treetop strings */
extern int		matflag;		/* read matrix from stdin */
extern int		desiredframe;		/* frame to start at */
extern int		curframe;		/* current frame number */
extern fastf_t		frame_delta_t;		/* 1.0 / frames_per_second_playback */
extern char		*outputfile;		/* name of base of output file */
extern int		interactive;		/* human is watching results */
extern int		benchmark;		/* No random numbers:  benchmark */
/***** end variables shared with do.c *****/


extern void		worker(int cpu, genptr_t arg);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
