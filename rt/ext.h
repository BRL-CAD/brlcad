/*
 *			E X T . H
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 *  
 *  @(#)$Header$
 */

/***** Variables declared in viewXXX.c */
extern int		use_air;		/* Handling of air in librt */
/***** end of sharing with viewing model *****/

/***** Variables declared in opt.c *****/
extern double		AmbientIntensity;	/* Ambient light intensity */
extern double		azimuth, elevation;
extern int		lightmodel;		/* Select lighting model */
extern int		rpt_overlap;		/* Warn about overlaps? */
extern int		rpt_dist;		/* Output depth along w/ RGB? */
extern int		space_partition;		/* Space partitioning algorithm to use */
extern double		nu_gfactor;	/* constant factor in NUgrid algorithm,
					   if applicable */

/***** variables declared in rt.c *****/
extern struct application	ap;
extern vect_t		left_eye_delta;

/***** variables shared with worker() ******/
extern int		stereo;			/* stereo viewing */
extern int		hypersample;		/* number of extra rays to fire */
extern int		jitter;			/* jitter (bit vector) */
#define JITTER_CELL	0x1			/* jitter position of ray in each cell */
#define JITTER_FRAME	0x2			/* jitter position of entire frame */
extern fastf_t		rt_perspective;		/* presp (degrees X) 0 => ortho */
extern fastf_t		aspect;			/* view aspect ratio X/Y */
extern vect_t		dx_model;		/* view delta-X as model-space vect */
extern vect_t		dy_model;		/* view delta-Y as model-space vect */
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
/***** end variables shared with worker() *****/

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

/***** variables from rt.c *****/
extern char		*framebuffer;		/* desired framebuffer */
extern FILE		*outfp;			/* optional output file */
extern int		output_is_binary;	/* !0 means output is binary */
extern mat_t		view2model;
extern mat_t		model2view;
extern vect_t		left_eye_delta;

extern void		worker();
