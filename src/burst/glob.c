/*                          G L O B . C
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
 *
 */
/** @file glob.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <signal.h>
#include "machine.h"
#include "vmath.h"
#include "fb.h"
#include "raytrace.h"
#include "./burst.h"
#include "./extern.h"

Colors	colorids;	/* ident range to color mappings for plots */
FBIO *fbiop = NULL;	/* frame buffer specific access from libfb */
FILE *burstfp = NULL;	/* input stream for burst point locations */
FILE *gridfp = NULL;	/* grid file output stream (2-d shots) */
FILE *histfp = NULL;	/* histogram output stream (statistics) */
FILE *outfp = NULL;	/* burst point library output stream */
FILE *plotfp = NULL;	/* 3-D UNIX plot stream (debugging) */
FILE *shotfp = NULL;	/* input stream for shot positions */
FILE *shotlnfp = NULL;	/* shotline file output stream */
FILE *tmpfp = NULL;	/* temporary file output stream for logging input */
HmMenu	*mainhmenu;
Ids airids;		/* burst air idents */
Ids armorids;		/* burst armor idents */
Ids critids;		/* critical component idents */
unsigned char *pixgrid;
unsigned char pixaxis[3]  = { 255,   0,   0 }; /* grid axis */
unsigned char pixbhit[3]  = { 200, 255, 200 }; /* burst ray hit non-critical comps */
unsigned char pixbkgr[3]  = { 150, 100, 255 }; /* outside grid */
unsigned char pixblack[3] = {   0,   0,   0 }; /* black */
unsigned char pixcrit[3]  = { 255, 200, 200 }; /* burst ray hit critical component */
unsigned char pixghit[3]  = { 255,   0, 255 }; /* ground burst */
unsigned char pixmiss[3]  = { 200, 200, 200 }; /* shot missed target */
unsigned char pixtarg[3]  = { 255, 255, 255 }; /* shot hit target */
Trie *cmdtrie = NULL;

boolean batchmode = 0;		/* are we processing batch input now */
boolean cantwarhead = 0;	/* pitch or yaw will be applied to warhead */
boolean deflectcone = DFL_DEFLECT;	/* cone axis deflects towards normal */
boolean dithercells = DFL_DITHER;	/* if true, randomize shot within cell */
boolean fatalerror;		/* must abort ray tracing */
boolean groundburst = 0;	/* if true, burst on imaginary ground */
boolean reportoverlaps = DFL_OVERLAPS;
				/* if true, overlaps are reported */
boolean reqburstair = 1;	/* if true, burst air required for shotburst */
boolean shotburst = 0;		/* if true, burst along shotline */
boolean tty = 1;		/* if true, full screen display is used */
boolean userinterrupt;		/* has the ray trace been interrupted */

char airfile[LNBUFSZ]={0};	/* input file name for burst air ids */
char armorfile[LNBUFSZ]={0};	/* input file name for burst armor ids */
char burstfile[LNBUFSZ]={0};	/* input file name for burst points */
char cmdbuf[LNBUFSZ];
char cmdname[LNBUFSZ];
char colorfile[LNBUFSZ]={0};	/* ident range-to-color file name */
char critfile[LNBUFSZ]={0};	/* input file for critical components */
char errfile[LNBUFSZ]={0};	/* errors/diagnostics log file name */
char fbfile[LNBUFSZ]={0};	/* frame buffer image file name */
char gedfile[LNBUFSZ]={0};	/* MGED data base file name */
char gridfile[LNBUFSZ]={0};	/* saved grid (2-d shots) file name */
char histfile[LNBUFSZ]={0};	/* histogram file name (statistics) */
char objects[LNBUFSZ]={0};	/* list of objects from MGED file */
char outfile[LNBUFSZ]={0};	/* burst point library output file name */
char plotfile[LNBUFSZ]={0};	/* 3-D UNIX plot file name (debugging) */
char scrbuf[LNBUFSZ];		/* scratch buffer for temporary use */
char scriptfile[LNBUFSZ]={0};	/* shell script file name */
char shotfile[LNBUFSZ];		/* input file of firing coordinates */
char shotlnfile[LNBUFSZ]={0};	/* shotline output file name */
char title[TITLE_LEN];		/* title of MGED target description */
char timer[TIMER_LEN];		/* CPU usage statistics */
char tmpfname[TIMER_LEN];	/* temporary file for logging input */

char *cmdptr;

fastf_t	bdist = DFL_BDIST;
			/* fusing distance for warhead */
fastf_t	burstpoint[3];	/* explicit burst point coordinates */
fastf_t	cellsz = DFL_CELLSIZE;
			/* shotline separation */
fastf_t	conehfangle = DFL_CONEANGLE;
			/* spall cone half angle */
fastf_t	fire[3];	/* explicit firing coordinates (2-D or 3-D) */
fastf_t griddn;		/* distance in model coordinates from origin to
				bottom border of grid */
fastf_t gridlf;		/* distance to left border */
fastf_t	gridrt;		/* distance to right border */
fastf_t gridup;		/* distance to top border */
fastf_t	gridhor[3];	/* horizontal grid direction cosines */
fastf_t	gridsoff[3];	/* origin of grid translated by stand-off */
fastf_t	gridver[3];	/* vertical grid direction cosines */
fastf_t grndbk = 0.0;	/* distance to back border of ground plane (-X) */
fastf_t grndht = 0.0;	/* distance of ground plane below target origin (-Z) */
fastf_t grndfr = 0.0;	/* distance to front border of ground plane (+X) */
fastf_t grndlf = 0.0;	/* distance to left border of ground plane (+Y) */
fastf_t grndrt = 0.0;	/* distance to right border of ground plane (-Y) */
fastf_t	modlcntr[3];	/* centroid of target's bounding RPP */
fastf_t modldn = 0.0;	/* distance in model coordinates from origin to bottom
				 extent of projection of model in grid plane */
fastf_t modllf = 0.0;	/* distance to left extent */
fastf_t modlrt = 0.0;	/* distance to right extent */
fastf_t modlup = 0.0;	/* distance to top extent */
fastf_t raysolidangle;	/* solid angle per spall sampling ray */
fastf_t	standoff;	/* distance from model origin to grid */
fastf_t	unitconv = 1.0;	/* units conversion factor (mm to "units") */
fastf_t	viewazim = DFL_AZIMUTH;
			/* degrees from X-axis to firing position */
fastf_t	viewelev = DFL_ELEVATION;
			/* degrees from XY-plane to firing position */

/* These are the angles and fusing distance used to specify the path of
	the canted warhead in Bob Wilson's simulation.
 */
fastf_t	pitch = 0.0;	/* elevation above path of main penetrator */
fastf_t	yaw = 0.0;	/* deviation right of path of main penetrator */

/* useful vectors */
fastf_t xaxis[3] = { 1.0, 0.0, 0.0 };
fastf_t zaxis[3] = { 0.0, 0.0, 1.0 };
fastf_t negzaxis[3] = { 0.0, 0.0, -1.0 };

int co;			/* columns of text displayable on video screen */
int devwid;		/* width in pixels of frame buffer window */
int devhgt;		/* height in pixels of frame buffer window */
int firemode = FM_DFLT;	/* mode of specifying shots */
int gridsz = 512;
int gridxfin;
int gridyfin;
int gridxorg;
int gridyorg;
int gridwidth;		/* Grid width in cells. */
int gridheight;		/* Grid height in cells. */
int li;			/* lines of text displayable on video screen */
int nbarriers = DFL_BARRIERS;
			/* no. of barriers allowed to critical comp */
int noverlaps = 0;	/* no. of overlaps encountered in this view */
int nprocessors;	/* no. of processors running concurrently */
int nriplevels = DFL_RIPLEVELS;
			/* no. of levels of ripping (0 = no ripping) */
int nspallrays = DFL_NRAYS;
			/* no. of spall rays at each burst point */
int units = DFL_UNITS;	/* target units (default is millimeters) */
int zoom = 1;		/* magnification factor on frame buffer */

struct rt_i *rtip = RTI_NULL; /* model specific access from librt */

/* signal handlers */
void	(*norml_sig)();	/* active during interactive operation */
void	(*abort_sig)(); /* active during ray tracing only */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
