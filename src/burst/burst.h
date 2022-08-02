/*                         B U R S T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file burst/burst.h
 *
 */

#ifndef BURST_BURST_H
#define BURST_BURST_H

#include "common.h"
#include <stdio.h> /* for FILE */

#include "bu/ptbl.h"
#include "bu/vls.h"
#include "dm.h"

#define LNBUFSZ         1330    /* buffer for one-line messages */
#define TITLE_LEN       72
#define TIMER_LEN       72

#define MSG_LOG 0
#define MSG_OUT 1

#define MAXDEVWID	10000	/* maximum width of frame buffer */

/* default parameters */
#define DFL_AZIMUTH     0.0
#define DFL_BARRIERS    100
#define DFL_BDIST       0.0
#define DFL_CELLSIZE    101.6
#define DFL_CONEANGLE   (45.0/RAD2DEG)
#define DFL_DEFLECT     0
#define DFL_DITHER      0
#define DFL_ELEVATION   0.0
#define DFL_NRAYS       200
#define DFL_OVERLAPS    1
#define DFL_RIPLEVELS   1

/* firing mode bit definitions */
#define FM_GRID  0      /* generate grid of shotlines */
#define FM_DFLT  FM_GRID
#define FM_PART  (1)    /* bit 0: ON = partial envelope, OFF = full */
#define FM_SHOT  (1<<1) /* bit 1: ON = discrete shots, OFF = gridding */
#define FM_FILE  (1<<2) /* bit 2: ON = file input, OFF = direct input */
#define FM_3DIM  (1<<3) /* bit 3: ON = 3-D coords., OFF = 2-D coords */
#define FM_BURST (1<<4) /* bit 4: ON = discrete burst points, OFF = shots */

struct ids {
    short i_lower;
    short i_upper;
};

struct colors {
    short c_lower;
    short c_upper;
    unsigned char c_rgb[3];
};

struct burst_state {
    int quit;                  /* 0 = continue, 1 = quit */
    struct bu_ptbl colorids;   /* ident range to color mappings for plots */
    fb *fbiop;                 /* frame buffer specific access from libfb */
    FILE *burstfp;             /* input stream for burst point locations */
    FILE *gridfp;              /* grid file output stream (2-d shots) */
    FILE *histfp;              /* histogram output stream (statistics) */
    FILE *outfp;               /* burst point library output stream */
    FILE *plotfp;              /* 3-D UNIX plot stream (debugging) */
    FILE *shotfp;              /* input stream for shot positions */
    FILE *shotlnfp;            /* shotline file output stream */
    struct bu_vls cmdhist;     /* interactive input logging (used to generated burst cmd files)*/
    struct bu_ptbl airids;     /* burst air idents */
    struct bu_ptbl armorids;   /* burst armor idents */
    struct bu_ptbl critids;    /* critical component idents */
    unsigned char *pixgrid;    /* */
    unsigned char pixaxis[3];  /* grid axis */
    unsigned char pixbhit[3];  /* burst ray hit non-critical comps */
    unsigned char pixbkgr[3];  /* outside grid */
    unsigned char pixblack[3]; /* black */
    unsigned char pixcrit[3];  /* burst ray hit critical component */
    unsigned char pixghit[3];  /* ground burst */
    unsigned char pixmiss[3];  /* shot missed target */
    unsigned char pixtarg[3];  /* shot hit target */

    int plotline;              /* boolean for plot lines (otherwise plots points) */
    int batchmode;             /* are we processing batch input now */
    int cantwarhead;           /* pitch or yaw will be applied to warhead */
    int deflectcone;           /* cone axis deflects towards normal */
    int dithercells;           /* if true, randomize shot within cell */
    int fatalerror;            /* must abort ray tracing */
    int groundburst;           /* if true, burst on imaginary ground */
    int reportoverlaps;        /* if true, overlaps are reported */
    int reqburstair;           /* if true, burst air required for shotburst */
    int shotburst;    	       /* if true, burst along shotline */
    int userinterrupt;         /* has the ray trace been interrupted */

    struct bu_vls airfile;     /* input file name for burst air ids */
    struct bu_vls armorfile;   /* input file name for burst armor ids */
    struct bu_vls burstfile;   /* input file name for burst points */
    struct bu_vls cmdbuf;      /* */
    struct bu_vls cmdname;     /* */
    struct bu_vls colorfile;   /* ident range-to-color file name */
    struct bu_vls critfile;    /* input file for critical components */
    FILE *errfile;             /* errors/diagnostics log file */
    struct bu_vls fbfile;      /* frame buffer image file name */
    struct bu_vls gedfile;     /* MGED data base file name */
    struct bu_vls gridfile;    /* saved grid (2-d shots) file name */
    struct bu_vls histfile;    /* histogram file name (statistics) */
    struct bu_vls objects;     /* list of objects from MGED file */
    struct bu_vls outfile;     /* burst point library output file name */
    struct bu_vls plotfile;    /* 3-D UNIX plot file name (debugging) */
    struct bu_vls shotfile;    /* input file of firing coordinates */
    struct bu_vls shotlnfile;  /* shotline output file name */

    char timer[TIMER_LEN];     /* CPU usage statistics */

    fastf_t bdist;             /* fusing distance for warhead */
    fastf_t burstpoint[3];     /* explicit burst point coordinates */
    fastf_t cellsz;            /* shotline separation */
    fastf_t conehfangle;       /* spall cone half angle */
    fastf_t fire[3];           /* explicit firing coordinates (2-D or 3-D) */
    fastf_t griddn;            /* distance in model coordinates from origin to bottom border of grid */
    fastf_t gridlf;            /* distance to left border */
    fastf_t gridrt;            /* distance to right border */
    fastf_t gridup;            /* distance to top border */
    fastf_t gridhor[3];        /* horizontal grid direction cosines */
    fastf_t gridsoff[3];       /* origin of grid translated by stand-off */
    fastf_t gridver[3];        /* vertical grid direction cosines */
    fastf_t grndbk;            /* distance to back border of ground plane (-X) */
    fastf_t grndht;            /* distance of ground plane below target origin (-Z) */
    fastf_t grndfr;            /* distance to front border of ground plane (+X) */
    fastf_t grndlf;            /* distance to left border of ground plane (+Y) */
    fastf_t grndrt;            /* distance to right border of ground plane (-Y) */
    fastf_t modlcntr[3];       /* centroid of target's bounding RPP */
    fastf_t modldn;            /* distance in model coordinates from origin to bottom extent of projection of model in grid plane */
    fastf_t modllf;            /* distance to left extent */
    fastf_t modlrt;            /* distance to right extent */
    fastf_t modlup;            /* distance to top extent */
    fastf_t raysolidangle;     /* solid angle per spall sampling ray */
    fastf_t standoff;          /* distance from model origin to grid */
    fastf_t unitconv;          /* units conversion factor (mm to "units") */
    fastf_t viewazim;          /* degrees from X-axis to firing position */
    fastf_t viewelev;          /* degrees from XY-plane to firing position */

    /* These are the angles and fusing distance used to specify the path of
       the canted warhead in Bob Wilson's simulation.
       */
    fastf_t pitch;	       /* elevation above path of main penetrator */
    fastf_t yaw;	       /* deviation right of path of main penetrator */

    /* useful vectors */
    fastf_t xaxis[3];
    fastf_t zaxis[3];
    fastf_t negzaxis[3];

    int co;    		       /* columns of text displayable on video screen */
    int devwid;    	       /* width in pixels of frame buffer window */
    int devhgt;    	       /* height in pixels of frame buffer window */
    int firemode;              /* mode of specifying shots */
    int gridsz;
    int gridxfin;
    int gridyfin;
    int gridxorg;
    int gridyorg;
    int gridwidth;    	       /* Grid width in cells. */
    int gridheight;    	       /* Grid height in cells. */
    int li;    		       /* lines of text displayable on video screen */
    int nbarriers;             /* no. of barriers allowed to critical comp */
    int noverlaps;             /* no. of overlaps encountered in this view */
    int nprocessors;           /* no. of processors running concurrently */
    int nriplevels;            /* no. of levels of ripping (0 = no ripping) */
    int nspallrays;            /* no. of spall rays at each burst point */
    int zoom;                  /* magnification factor on frame buffer */

    struct rt_i *rtip;         /* model specific access from librt */

    /* signal handlers */
    void (*norml_sig)();       /* active during interactive operation */
    void (*abort_sig)();       /* active during ray tracing only */

    /* command table for help printing */
    const struct bu_cmdtab *cmds;
};



extern void burst_state_init(struct burst_state *s);

extern int execute_run(struct burst_state *s);

extern int findIdents(int ident, struct bu_ptbl *idpl);
extern int readIdents(struct bu_ptbl *idlist, const char *fname);
extern int readColors(struct bu_ptbl *idlist, const char *fname);
extern struct colors *findColors(int ident, struct bu_ptbl *colp);

extern void gridModel(struct burst_state *s);
extern void gridInit(struct burst_state *s);
extern void spallInit(struct burst_state *s);

extern void paintCellFb(struct application *ap, unsigned char *pixpaint, unsigned char *pixexpendable);
extern void paintSpallFb(struct application *ap);
extern void paintGridFb(struct burst_state *s);
extern int imageInit(struct burst_state *s);

/* as far as I can tell the original burst code didn't actually use errfile,
 * but since it is a defined command we will set up to support it */
extern void brst_log(struct burst_state *s, int TYPE, const char *, ...);

#endif  /* BURST_BURST_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
