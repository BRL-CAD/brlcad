/*                           E X T . H
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @file rt/ext.h
 *
 * External variable declarations for the RT family of analysis programs.
 *
 */

#include "optical.h"

/***** Variables declared in opt.c *****/
extern char *framebuffer;		/* desired framebuffer */
extern double azimuth, elevation;
extern double nu_gfactor;		/* constant factor in NUgrid algorithm */
extern int Query_one_pixel;
extern int benchmark;
extern int lightmodel;			/* Select lighting model */
extern int nugrid_dimlimit;		/* limit to dimensions of nugrid; <= 0 means no limit */
extern int query_debug;
extern int query_rdebug;
extern int query_x;
extern int query_y;
extern int rpt_dist;			/* Output depth along w/ RGB? */
extern int rpt_overlap;			/* Warn about overlaps? */
extern int space_partition;		/* Space partitioning algorithm to use */
extern int sub_grid_mode;		/* mode to raytrace a rectangular portion of view */
extern int sub_xmax;			/* upper right of sub rectangle */
extern int sub_xmin;			/* lower left of sub rectangle */
extern int sub_ymax;
extern int sub_ymin;
extern int transpose_grid;		/* reverse the order of grid traversal */
extern int use_air;			/* Handling of air in librt */
extern int random_mode;                 /* Mode to shoot rays at random directions */

/***** variables from main.c *****/
extern FILE *outfp;			/* optional output file */
extern int output_is_binary;		/* !0 means output is binary */
extern int report_progress;		/* !0 = user wants progress report */
extern int save_overlaps;		/* flag for setting rti_save_overlaps */
extern mat_t model2view;
extern mat_t view2model;
extern struct application APP;
extern struct bu_image_file *bif;
extern vect_t left_eye_delta;
extern vect_t left_eye_delta;

/***** variables shared with worker() ******/
extern unsigned char *scanbuf;		/* pixels for REMRT */
extern fastf_t aspect;			/* view aspect ratio X/Y */
extern fastf_t cell_height;		/* model space grid cell height */
extern fastf_t cell_width;		/* model space grid cell width */
extern fastf_t eye_backoff;		/* dist from eye to center */
extern fastf_t rt_perspective;		/* presp (degrees X) 0 => ortho */
extern fastf_t viewsize;
extern int cell_newsize;		/* new grid cell size (for worker) */
extern int fullfloat_mode;
extern int hypersample;			/* number of extra rays to fire */
extern int incr_mode;			/* !0 for incremental resolution */
extern int full_incr_mode;              /* !0 for fully incremental resolution */
extern int npsw;			/* number of worker PSWs to run */
extern int reproj_cur;			/* number of pixels reprojected this frame */
extern int reproj_max;			/* out of total number of pixels */
extern int reproject_mode;
extern int stereo;			/* stereo viewing */
extern mat_t Viewrotscale;
extern point_t eye_model;		/* model-space location of eye */
extern point_t viewbase_model;		/* model-space location of viewplane corner */
extern size_t height;			/* # of lines in Y */
extern size_t incr_level;		/* current incremental level */
extern size_t incr_nlevel;		/* number of levels */
extern size_t full_incr_sample;         /* current fully incremental sample */
extern size_t full_incr_nsamples;       /* number of fully incremental samples */
extern size_t width;			/* # of pixels in X */
extern struct floatpixel *curr_float_frame;	/* buffer of full frame */
extern struct floatpixel *prev_float_frame;
extern struct resource resource[];	/* memory resources */
extern unsigned int jitter;		/* jitter (bit vector) */
extern vect_t dx_model;			/* view delta-X as model-space vect (width of pixel as vector) */
extern vect_t dx_unit;			/* unit-len dir vector of pixel side-to-side */
extern vect_t dy_model;			/* view delta-Y as model-space vect (height of pixel as vector) */
extern vect_t dy_unit;			/* unit-len dir vector of pixel top-to-bottom */
/** 'jitter' variable values **/
#define JITTER_CELL 0x1			/* jitter position of ray in each cell */
#define JITTER_FRAME 0x2		/* jitter position of entire frame */

/***** end variables shared with worker() *****/

/***** Photon Mapping Variables *****/
extern char pmfile[255];
extern double pmargs[9];
/***** ************************ *****/

/***** variables shared with do.c *****/
extern char **objtab;			/* array of treetop strings */
extern char *outputfile;		/* name of base of output file */
extern fastf_t frame_delta_t;		/* 1.0 / frames_per_second_playback */
extern int benchmark;			/* No random numbers:  benchmark */
extern int curframe;			/* current frame number */
extern int desiredframe;		/* frame to start at */
extern int interactive;			/* human is watching results */
extern int matflag;			/* read matrix from stdin */
extern int nobjs;			/* Number of cmd-line treetops */
extern int pix_end;			/* pixel to end at */
extern int pix_start;			/* pixel to start at */
/***** end variables shared with do.c *****/

/*** do.c ***/
extern void def_tree(struct rt_i *rtip);
extern void do_prep(struct rt_i *rtip);
extern void do_run(int a, int b);
extern void do_ae(double azim, double elev);
extern int old_way(FILE *fp);
extern int do_frame(int framenumber);

/* opt.c */
extern int get_args(int argc, const char *argv[]);

/* view.c */
extern void usage(const char *argv0);
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
