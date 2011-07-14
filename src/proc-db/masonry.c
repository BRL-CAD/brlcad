/*                       M A S O N R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file proc-db/masonry.c
 *
 * build a wall out of various materials.
 *
 * Currently builds "wood frame" walls for typical building constructs.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"


/* declarations to support use of bu_getopt() system call */
char *options = "w:o:n:t:b:u:c:rlhdm:T:R:";

int debug = 0;
char *progname = "masonry";
char *obj_name = "wall";
char sol_name[64];
int sol_num = 0;
char *type = "frame";
char *units = "mm";
double unit_conv = 1.0;
matp_t trans_matrix = (matp_t)NULL;
const double degtorad =  0.01745329251994329573;

int log_cmds = 0;	/* log sessions to a log file */
/* standard construction brick:
 * 8" by 2 1/4" by 3 3/8"
 */
double brick_height = 8.0 * 25.4;
double brick_width = 2.25 * 25.4;
double brick_depth = 3.25 * 25.4;
double min_mortar = 0.25 * 25.4;

unsigned char *color;
unsigned char def_color[3];
unsigned char brick_color[3] = {160, 40, 40};
unsigned char mortar_color[3] = {190, 190, 190};

int rand_brick_color = 0;
int make_mortar = 1;

/* real dimensions of a "2 by 4" board */
double bd_thick = 3.25 * 25.4;
double bd_thin = 1.5 * 25.4;
double beam_height = 5.5 * 25.4;
double sr_thick = 0.75 * 25.4;	/* sheetrock thickness */
double stud_spacing = 16.0 * 25.4; /* spacing between vertical studs */

unsigned char sheetrock_color[3] = { 200, 200, 200 };
unsigned char stud_color[3] = { 250, 178, 108 };

char *stud_properties[] = { "plastic", "sh=10 di=0.7 sp=0.3" };

struct opening {
    struct bu_list l;
    double sx;	/* start in X direction */
    double sz;	/* start in Z direction */
    double ex;	/* end in X direction */
    double ez;	/* end in Z direction */
} ol_hd;

#define WALL_WIDTH ol_hd.ex
#define WALL_HEIGHT ol_hd.ez

struct boardseg {
    struct bu_list l;
    double s;	/* start */
    double e;	/* end */
};


/**
 * U S A G E
 *
 * tell user how to invoke this program, then exit
 */
void usage(char *s)
{
    if (s)
	bu_log("%s\n", s);

    bu_exit(1, "Usage: %s %s\n%s\n%s\n%s\n",
	    progname,
	    "[ -u units ] -w(all) width, height [-o(pening) lx, lz, hx, hz ...]",
	    " [-n name] [ -d(ebug) ] [-t {frame|brick|block|sheetrock} ] [-c R/G/B]",
	    " [-l(og_commands)] [-R(otate) rx/ry/rz] [-T(ranslate) dx/dy/dz]",
	    " brick sub-options: [-r(and_color)] [-b width, height, depth ] [-m min_mortar]"
	);
}


void
set_translate(char *s)
{
    double dx, dy, dz;

    if (sscanf(s, "%lf/%lf/%lf", &dx, &dy, &dz) != 3)
	usage("translation option problem\n");

    if (!trans_matrix) {
	trans_matrix = (matp_t)bu_calloc(sizeof(mat_t), 1, "transformation matrix");
	MAT_IDN(trans_matrix);
    }

    MAT_DELTAS(trans_matrix, dx*unit_conv, dy*unit_conv, dz*unit_conv);
}

/*
 * B U I L D H R O T
 *
 * This routine builds a Homogeneous rotation matrix, given
 * alpha, beta, and gamma as angles of rotation.
 *
 * NOTE:  Only initialize the rotation 3x3 parts of the 4x4
 * There is important information in dx, dy, dz, s .
 */
void
buildHrot(matp_t mat, double alpha, double beta, double ggamma)
{
    static fastf_t calpha, cbeta, cgamma;
    static fastf_t salpha, sbeta, sgamma;

    calpha = cos(alpha);
    cbeta = cos(beta);
    cgamma = cos(ggamma);

    salpha = sin(alpha);
    sbeta = sin(beta);
    sgamma = sin(ggamma);

    /*
     * compute the new rotation to apply to the previous
     * viewing rotation.
     * Alpha is angle of rotation about the X axis, and is done third.
     * Beta is angle of rotation about the Y axis, and is done second.
     * Gamma is angle of rotation about Z axis, and is done first.
     */
#ifdef m_RZ_RY_RX
    /* view = model * RZ * RY * RX (Neuman+Sproul, premultiply) */
    mat[0] = cbeta * cgamma;
    mat[1] = -cbeta * sgamma;
    mat[2] = -sbeta;

    mat[4] = -salpha * sbeta * cgamma + calpha * sgamma;
    mat[5] = salpha * sbeta * sgamma + calpha * cgamma;
    mat[6] = -salpha * cbeta;

    mat[8] = calpha * sbeta * cgamma + salpha * sgamma;
    mat[9] = -calpha * sbeta * sgamma + salpha * cgamma;
    mat[10] = calpha * cbeta;
#endif
    /* This is the correct form for this version of GED */
    /* view = RX * RY * RZ * model (Rodgers, postmultiply) */
    /* Point thumb along axis of rotation.  +Angle as hand closes */
    mat[0] = cbeta * cgamma;
    mat[1] = -cbeta * sgamma;
    mat[2] = sbeta;

    mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
    mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
    mat[6] = -salpha * cbeta;

    mat[8] = -calpha * sbeta * cgamma + salpha * sgamma;
    mat[9] = calpha * sbeta * sgamma + salpha * cgamma;
    mat[10] = calpha * cbeta;
}

void
set_rotate(char *s)
{
    double rx, ry, rz;

    if (sscanf(s, "%lf/%lf/%lf", &rx, &ry, &rz) != 3)
	usage("rotation option problem\n");

    if (!trans_matrix) {
	trans_matrix = (matp_t)bu_calloc(sizeof(mat_t), 1,
					 "rotation matrix");
	MAT_IDN(trans_matrix);
    }
    buildHrot(trans_matrix, rx*degtorad, ry*degtorad, rz*degtorad);
}


/*
 * P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char **av)
{
    int c;
    struct opening *op;
    double dx, dy, width, height;
    int R, G, B;
    int units_lock=0;
    FILE *logfile;

    if (! (progname=strrchr(*av, '/')))
	progname = *av;
    else
	++progname;


    BU_LIST_INIT(&ol_hd.l);

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	switch (c) {
	    case 'T':
		set_translate(bu_optarg);
		units_lock = 1;
		break;
	    case 'R':
		set_rotate(bu_optarg);
		break;
	    case 'b':
		if (sscanf(bu_optarg, "%lf, %lf, %lf", &width, &height, &dy) == 3) {
		    brick_width = width * unit_conv;
		    brick_height = height * unit_conv;
		    brick_depth = dy * unit_conv;
		    units_lock = 1;
		} else {
		    usage("error parsing -b option\n");
		}

		break;
	    case 'c':
		if (sscanf(bu_optarg, "%d %d %d", &R, &G, &B) == 3) {
		    color = def_color;
		    color[0] = R & 0x0ff;
		    color[1] = G & 0x0ff;
		    color[2] = B & 0x0ff;
		}
		break;
	    case 'd':
		debug = !debug;
		break;
	    case 'l':
		log_cmds = !log_cmds;
		break;
	    case 'm':
		if ((dx=atof(bu_optarg)) > 0.0) {
		    min_mortar = dx * unit_conv;
		    units_lock = 1;
		    make_mortar = 1;
		} else {
		    usage("error parsing -m option\n");
		}

		break;
	    case 'n':
		obj_name = bu_optarg;
		break;
	    case 'o':
		if (ZERO(ol_hd.ex)) {
		    usage("set wall dim before openings\n");
		} else if (sscanf(bu_optarg, "%lf, %lf, %lf, %lf", &dx, &dy, &width, &height) == 4) {
		    op = (struct opening *)bu_calloc(1, sizeof(struct opening), "calloc opening");
		    BU_LIST_INSERT(&ol_hd.l, &op->l);
		    op->sx = dx * unit_conv;
		    op->sz = dy * unit_conv;
		    op->ex = width * unit_conv;
		    op->ez = height * unit_conv;
		    
		    /* do bounds checking */
		    if (op->sx < 0.0) op->sx = 0.0;
		    if (op->sz < 0.0) op->sz = 0.0;
		    if (op->ex > WALL_WIDTH)
			op->ex = WALL_WIDTH;
		    if (op->ez > WALL_HEIGHT)
			op->ez = WALL_HEIGHT;
		    
		    units_lock = 1;
		} else {
		    usage("error parsing -o option\n");
		}
		break;
	    case 'r':
		rand_brick_color = !rand_brick_color;
		break;
	    case 't':
		type = bu_optarg;
		break;
	    case 'u':
		if (units_lock)
		    bu_log("Warning: attempting to change units in mid-parse\n");

		dx=bu_units_conversion(bu_optarg);
		if (!ZERO(dx)) {
		    unit_conv = dx;
		    units = bu_optarg;
		} else {
		    usage("error parsing -u (units)\n");
		}
		break;
	    case 'w':
		if (sscanf(bu_optarg, "%lf, %lf", &width, &height) == 2) {
		    WALL_WIDTH = width * unit_conv;
		    WALL_HEIGHT = height * unit_conv;
		    units_lock = 1;
		} else {
		    usage("error parsing -w (wall dimensions)\n");
		}
		break;
	    case '?':
	    case 'h':
	    default:
		usage("Bad or help flag specified\n");
		break;
	}
    }


    if (log_cmds) {
	if ((logfile=fopen("wall.log", "a+")) == (FILE *)NULL) {
	    perror("wall.log");
	    bu_exit(-1, NULL);
	}
	for (R=0; R < ac; R++)
	    (void)fprintf(logfile, "%s ", av[R]);
	(void)putc('\n', logfile);
	(void)fclose(logfile);
    }

    return bu_optind;

}


void
h_segs(double sz, double ez, struct boardseg *seglist, double sx, double ex)
{
    struct opening *op;
    struct boardseg *seg, *sp;

    seg = (struct boardseg *)bu_calloc(1, sizeof(struct boardseg), "initial seg");
    seg->s = sx;
    seg->e = ex;
    /* trim opening to X bounds of wall */
    if (seg->s < ol_hd.sx) seg->s = ol_hd.sx;
    if (seg->e > WALL_WIDTH) seg->e = WALL_WIDTH;
    BU_LIST_APPEND(&(seglist->l), &(seg->l));

    for (BU_LIST_FOR(op, opening, &ol_hd.l)) {

	if ((op->sz >= sz && op->sz <= ez) ||
	    (op->ez >= sz && op->ez <= ez) ||
	    (op->sz <= sz && op->ez >= ez)) {

	    /* opening in horizontal segment */
	    for (BU_LIST_FOR(seg, boardseg, &(seglist->l))) {
		if (op->sx <= seg->s) {
		    if (op->ex >= seg->e) {
			/* opening covers entire segment.
			 * segement gets deleted
			 */
			sp = BU_LIST_PLAST(boardseg, &(seg->l));
			BU_LIST_DEQUEUE(&(seg->l));
			bu_free((char *)seg, "seg free 2");
			seg = sp;

		    } else if (op->ex > seg->s) {
			/* opening covers begining of segment */
			seg->s = op->ex;
		    }
		    /* else opening is entirely prior to seg->s */
		} else if (op->ex >= seg->e) {
		    if (op->sx < seg->e) {
			/* opening covers end of segment */
			seg->e = op->sx;
		    }
		    /* else opening entirely after segment */
		} else {
		    /* there is an opening in the middle of the
		     * segment.  We must divide the segment into
		     * 2 segements
		     */
		    sp = (struct boardseg *)bu_calloc(1, sizeof(struct boardseg), "alloc boardseg");
		    sp->s = seg->s;
		    sp->e = op->sx;
		    seg->s = op->ex;
		    BU_LIST_INSERT(&(seg->l), &(sp->l));
		}
	    }
	}
    }
}

void
mksolid(struct rt_wdb *fd, point_t (*pts), struct wmember *wm_hd)
{
    struct wmember *wm;
    snprintf(sol_name, 64, "s.%s.%d", obj_name, sol_num++);

    mk_arb8(fd, sol_name, &pts[0][X]);
    wm = mk_addmember(sol_name, &(wm_hd->l), NULL, WMOP_UNION);

    if (trans_matrix)
	MAT_COPY(wm->wm_mat, trans_matrix);
}

void
mk_h_rpp(struct rt_wdb *fd, struct wmember *wm_hd, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax)
{
    point_t pts[8];

    VSET(pts[0], xmin, ymin, zmin);
    VSET(pts[1], xmin, ymin, zmax);
    VSET(pts[2], xmin, ymax, zmax);
    VSET(pts[3], xmin, ymax, zmin);
    VSET(pts[4], xmax, ymin, zmin);
    VSET(pts[5], xmax, ymin, zmax);
    VSET(pts[6], xmax, ymax, zmax);
    VSET(pts[7], xmax, ymax, zmin);

    mksolid(fd, pts, wm_hd);
}

void
mk_v_rpp(struct rt_wdb *fd, struct wmember *wm_hd, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax)
{
    point_t pts[8];

    VSET(pts[0], xmin, ymin, zmin);
    VSET(pts[1], xmax, ymin, zmin);
    VSET(pts[2], xmax, ymax, zmin);
    VSET(pts[3], xmin, ymax, zmin);
    VSET(pts[4], xmin, ymin, zmax);
    VSET(pts[5], xmax, ymin, zmax);
    VSET(pts[6], xmax, ymax, zmax);
    VSET(pts[7], xmin, ymax, zmax);

    mksolid(fd, pts, wm_hd);
}

/*
 * put the sides on a frame opening
 */
void
frame_o_sides(struct rt_wdb *fd, struct wmember *wm_hd, struct opening *op, double h)
{
    double sx, ex;

    /* put in the side(s) of the window */
    if (op->sx-bd_thin >= 0.0) {
	/* put in a closing board on the side */
	sx = op->sx-bd_thin;

	if (sx < bd_thin)
	    sx = 0.0;
	mk_v_rpp(fd, wm_hd,
		 sx,	 op->sx,
		 0.0,	 bd_thick,
		 bd_thin, h);


	if (op->sx-bd_thin*2.0 >= 0.0) {
	    /* put in reinforcing board on side */

	    if ((sx=op->sx-bd_thin*2.0) < bd_thin)
		sx = 0.0;

	    mk_v_rpp(fd, wm_hd,
		     sx,	op->sx-bd_thin,
		     0.0, 	bd_thick,
		     bd_thin, WALL_HEIGHT-bd_thin);
	}
    }

    /* close off the end of the opening */
    if (op->ex+bd_thin <= WALL_WIDTH) {

	ex = op->ex+bd_thin;

	if (ex > WALL_WIDTH-bd_thin)
	    ex = WALL_WIDTH;

	mk_v_rpp(fd, wm_hd,
		 op->ex,	 ex,
		 0.0,	 bd_thick,
		 bd_thin, h);

	if (ex+bd_thin <= WALL_WIDTH) {

	    if ((ex += bd_thin) > WALL_WIDTH-bd_thin)
		ex = WALL_WIDTH;

	    mk_v_rpp(fd, wm_hd,
		     ex-bd_thin,	ex,
		     0.0,		bd_thick,
		     bd_thin,	WALL_HEIGHT-bd_thin);
	}
    }

}


/*
 * Make the frame opening (top & bottom, call frame_o_sides for sides)
 */
void
frame_opening(struct rt_wdb *fd, struct wmember *wm_hd, struct opening *op)
{
    double pos;
    int studs;
    double dx, span;

    /* 2 vertical studs @ op->sx-bd_thin*2.0 && op->ex+bd_thin*2.0 */


    /* build the bottom of the opening */
    if (op->sz > bd_thin) {
	/* put in bottom board of opening */

	if (op->sz-bd_thin >= bd_thin)
	    pos = op->sz - bd_thin;
	else
	    pos = bd_thin;

	mk_h_rpp(fd, wm_hd, op->sx, op->ex, 0.0, bd_thick,
		 pos, op->sz);

	if (op->sz > bd_thin*2.0) {
	    /* put in support for bottom board */
	    if (op->ex - op->sx < bd_thin*2) {
		/* one wide board to support */
		mk_v_rpp(fd, wm_hd,
			 op->sx, op->ex,
			 0.0,	bd_thick,
			 bd_thin, op->sz-bd_thin);
	    } else {

		/* multiple support boards */
		mk_v_rpp(fd, wm_hd,
			 op->sx, op->sx+bd_thin,
			 0.0, bd_thick,
			 bd_thin, op->sz-bd_thin);

		mk_v_rpp(fd, wm_hd,
			 op->ex-bd_thin, op->ex,
			 0.0,	bd_thick,
			 bd_thin, op->sz-bd_thin);

		/* do we need some in the span? */
		span = op->ex - op->sx;
		span -= bd_thin*2.0;

		studs = (int) (span/stud_spacing);

		dx = span / ((double)studs+1.0);

		if (debug)
		    bu_log("making %d xtra studs, spacing %g on span %g\n",
			   studs, dx / unit_conv,
			   span / unit_conv);

		for (pos=op->sx+dx; studs; pos+=dx, studs--) {
		    if (debug)
			bu_log("making xtra stud @ %g\n",
			       pos / unit_conv);

		    mk_v_rpp(fd,	wm_hd,
			     pos,	pos+bd_thin,
			     0.0,	bd_thick,
			     bd_thin,
			     op->sz-bd_thin);
		}
	    }
	}
    }

    /* build the top of the opening */
    if (op->ez < WALL_HEIGHT-bd_thin*2.0) {
	/* put in board in top of opening */

	if (op->ez+bd_thin+beam_height < WALL_HEIGHT-bd_thin) {
	    /* there's room to separate the beam from the
	     * board at the top of the opening.  First, we
	     * put in the board for the top of the opening.
	     */
	    mk_h_rpp(fd, wm_hd,
		     op->sx, op->ex,
		     0.0, bd_thick,
		     op->ez, op->ez+bd_thin);

	    /* put the beam in */
	    mk_h_rpp(fd, wm_hd,
		     FMAX(0.0, op->sx-bd_thin),
		     FMIN(WALL_WIDTH, op->ex+bd_thin),
		     0.0, bd_thick,
		     WALL_HEIGHT-bd_thin-beam_height,
		     WALL_HEIGHT-bd_thin);

	    /* put in the offset boards */
	    mk_v_rpp(fd, wm_hd,
		     op->sx, op->sx+bd_thin,
		     0.0, bd_thick,
		     op->ez+bd_thin,
		     WALL_HEIGHT-bd_thin-beam_height);

	    mk_v_rpp(fd, wm_hd,
		     op->ex-bd_thin, op->ex,
		     0.0, bd_thick,
		     op->ez+bd_thin,
		     WALL_HEIGHT-bd_thin-beam_height);

	    span = op->ex - op->sx;
	    span -= bd_thin*2.0;

	    studs = (int) (span/stud_spacing);
	    dx = span / ((double)studs+1.0);

	    for (pos=op->sx+dx; studs--; pos += dx) {
		mk_v_rpp(fd, wm_hd,
			 pos, pos+bd_thin,
			 0.0, bd_thick,
			 op->ez+bd_thin,
			 WALL_HEIGHT-bd_thin-beam_height);
	    }

	    frame_o_sides(fd, wm_hd, op,
			  WALL_HEIGHT-bd_thin-beam_height);

	} else {
	    /* Make the beam on the top of the window the top
	     * of the window.
	     */

	    mk_h_rpp(fd, wm_hd,
		     FMAX(0.0, op->sx-bd_thin),
		     FMIN(WALL_WIDTH, op->ex+bd_thin),
		     0.0, bd_thick,
		     op->ez, WALL_HEIGHT-bd_thin);

	    /* put in the sides of the opening */
	    frame_o_sides(fd, wm_hd, op,
			  op->ez);
	}
    } else {
	/* There is no top board capping the opening
	 * (with the possible exception of the top rail of the wall)
	 */
	frame_o_sides(fd, wm_hd, op, WALL_HEIGHT-bd_thin);
    }
}


void
frame(struct rt_wdb *fd)
{
    struct boardseg *s_hd, *seg;
    struct opening *op;
    double pos;
    struct wmember wm_hd;

    if (WALL_WIDTH <= bd_thin*2) {
	bu_log("wall width must exceed %g.\n", (bd_thin*2)/unit_conv);
	return;
    }
    if (WALL_HEIGHT <= bd_thin*2) {
	bu_log("wall height must exceed %g.\n", (bd_thin*2)/unit_conv);
	return;
    }
    BU_LIST_INIT(&wm_hd.l);

    if (!color)
	color = stud_color;

    mk_id(fd, "A wall");

    /* find the segments of the base-board */
    s_hd = (struct boardseg *)bu_calloc(1, sizeof(struct boardseg), "s_hd");
    BU_LIST_INIT(&(s_hd->l));

    h_segs(0.0, bd_thin, s_hd, 0.0, WALL_WIDTH);

    /* make the base-board segments */
    while (BU_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

	if (debug) {
	    bu_log("baseboard seg: %g -> %g\n",
		   seg->s/unit_conv, seg->e/unit_conv);
	}

	mk_h_rpp(fd, &wm_hd,
		 seg->s, seg->e,
		 0.0, bd_thick,
		 0.0, bd_thin);

	BU_LIST_DEQUEUE(&(seg->l));
	bu_free((char *)seg, "seg free 3");
    }

    /* now find the segments of the cap board */

    h_segs(WALL_HEIGHT - bd_thin, WALL_HEIGHT, s_hd, 0.0, WALL_WIDTH);

    /* make the cap board segments */
    while (BU_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

	if (debug) {
	    bu_log("capboard seg: %g -> %g\n",
		   seg->s/unit_conv, seg->e/unit_conv);
	}

	mk_h_rpp(fd, &wm_hd,
		 seg->s, seg->e,
		 0.0, bd_thick,
		 WALL_HEIGHT-bd_thin, WALL_HEIGHT);

	BU_LIST_DEQUEUE(&(seg->l));
	bu_free((char *)seg, "seg_free 4");
    }

    /* put in the vertical stud boards that are not a part of an
     * opening for a window or a door.
     */
    for (pos = 0.0; pos <= WALL_WIDTH-bd_thin; pos += stud_spacing) {
	int mk_stud_flag;

	if (pos > WALL_WIDTH-bd_thin*2.0)
	    pos = WALL_WIDTH-bd_thin;

	mk_stud_flag = 1;
	/* make sure stud doesn't overlap an opening */
	for (BU_LIST_FOR(op, opening, &ol_hd.l)) {
	    if ((pos > op->sx-bd_thin*2.0 &&
		 pos < op->ex+bd_thin*2.0) ||
		(pos+bd_thin > op->sx-bd_thin*2.0 &&
		 pos+bd_thin < op->ex+bd_thin*2.0)
		) {
		if (debug)
		    bu_log("not making stud @ %g\n", pos / unit_conv);

		mk_stud_flag = 0;
		break;
	    }
	}
	if (mk_stud_flag) {
	    /* put in the vertical stud */
	    if (debug)
		bu_log("Making stud @ %g\n", pos / unit_conv);

	    mk_v_rpp(fd, &wm_hd,
		     pos, pos+bd_thin,
		     0.0, bd_thick,
		     bd_thin, WALL_HEIGHT-bd_thin);
	}

	if (pos < WALL_WIDTH-bd_thin &&
	    pos+stud_spacing > WALL_WIDTH-bd_thin)
	    pos = WALL_WIDTH - bd_thin - stud_spacing;
    }

    for (BU_LIST_FOR(op, opening, &ol_hd.l))
	frame_opening(fd, &wm_hd, op);

    /* put all the studding in a region */
    snprintf(sol_name, 64, "r.%s.studs", obj_name);
    mk_lcomb(fd, sol_name, &wm_hd, 1,
	     stud_properties[0], stud_properties[1], color, 0);
}

void
sheetrock(struct rt_wdb *fd)
{
    point_t pts[8];
    struct wmember wm_hd;
    struct opening *op;
    int i=0;

    if (!color)
	color = sheetrock_color;

    BU_LIST_INIT(&wm_hd.l);

    /* now add the sheetrock */
    VSET(pts[0], 0.0, 0.0, 0.0);
    VSET(pts[1], 0.0, sr_thick, 0.0);
    VSET(pts[2], 0.0, sr_thick, WALL_HEIGHT);
    VSET(pts[3], 0.0, 0.0, WALL_HEIGHT);
    VSET(pts[4], WALL_WIDTH, 0.0, 0.0);
    VSET(pts[5], WALL_WIDTH, sr_thick, 0.0);
    VSET(pts[6], WALL_WIDTH, sr_thick, WALL_HEIGHT);
    VSET(pts[7], WALL_WIDTH, 0.0, WALL_HEIGHT);

    snprintf(sol_name, 64, "s.%s.sr1", obj_name);
    mk_arb8(fd, sol_name, &pts[0][X]);
    (void)mk_addmember(sol_name, &wm_hd.l, NULL, WMOP_UNION);

    for (BU_LIST_FOR(op, opening, &ol_hd.l)) {
	VSET(pts[0], op->sx, -0.01,		op->sz);
	VSET(pts[1], op->sx, sr_thick+0.01,	op->sz);
	VSET(pts[2], op->sx, sr_thick+0.01,	op->ez);
	VSET(pts[3], op->sx, -0.01,		op->ez);
	VSET(pts[4], op->ex, -0.01,		op->sz);
	VSET(pts[5], op->ex, sr_thick+0.01,	op->sz);
	VSET(pts[6], op->ex, sr_thick+0.01,	op->ez);
	VSET(pts[7], op->ex, -0.01,		op->ez);

	snprintf(sol_name, 64, "s.%s.o.%d", obj_name, i++);
	mk_arb8(fd, sol_name, &pts[0][X]);
	(void)mk_addmember(sol_name, &wm_hd.l, NULL, WMOP_SUBTRACT);
    }

    snprintf(sol_name, 64, "r.%s.sr1", obj_name);
    mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
	     color, 0);
}

void
mortar_brick(struct rt_wdb *UNUSED(fd))
{
    struct wmember wm_hd;
#if 0
    int horiz_bricks;
    int vert_bricks;
    double mortar_height;
    double mortar_width;
    point_t pts[8];

    horiz_bricks = (WALL_WIDTH-brick_depth) / (brick_width + min_mortar);

    /* compute excess distance to be used in mortar */
    mortar_width = WALL_WIDTH -
	(horiz_bricks * (brick_width + min_mortar) +
	 brick_depth);

    mortar_width = min_mortar + mortar_width / (double)horiz_bricks;

    vert_bricks = WALL_HEIGHT / (brick_height+min_mortar);
    mortar_height = WALL_HEIGHT - vert_bricks * (brick_height+min_mortar);
    mortar_height = min_mortar + mortar_height/vert_bricks;


    /* make prototype brick */

    VSET(pts[0], 0.0,	  0.0,		mortar_height);
    VSET(pts[1], 0.0,	  brick_depth,	mortar_height);
    VSET(pts[2], brick_width, brick_depth,	mortar_height);
    VSET(pts[3], brick_width, 0.0,		mortar_height);

    VSET(pts[4], 0.0,	  0.0,		mortar_height+brick_height);
    VSET(pts[5], 0.0,	  brick_depth,	mortar_height+brick_height);
    VSET(pts[6], brick_width, brick_depth,	mortar_height+brick_height);
    VSET(pts[7], brick_width, 0.0,		mortar_height+brick_height);

    snprintf(sol_name, 64, "s.%s.b", obj_name);
    mk_arb8(fd, sol_name, &pts[0][X]);

    (void)mk_addmember(sol_name, &wm_hd.l, NULL, WMOP_UNION);
    *sol_name = 'r';

    if (rand_brick_color)
	mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
		 (char *)NULL, 0);
    else
	mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
		 brick_color, 0);


    /* make prototype mortar upon which brick will sit */
    VSET(pts[0], 0.0,	  0.0,		0.0);
    VSET(pts[1], 0.0,	  brick_depth,	0.0);
    VSET(pts[2], brick_width, brick_depth,	0.0);
    VSET(pts[3], brick_width, 0.0,		0.0);

    VSET(pts[4], 0.0,	  0.0,		mortar_height);
    VSET(pts[5], 0.0,	  brick_depth,	mortar_height);
    VSET(pts[6], brick_width, brick_depth,	mortar_height);
    VSET(pts[7], brick_width, 0.0,		mortar_height);

    snprintf(sol_name, 64, "s.%s.vm", obj_name);
    mk_arb8(fd, sol_name, &pts[0][X]);

    (void)mk_addmember(sol_name, &wm_hd.l, NULL, WMOP_UNION);
    *sol_name = 'r';
    mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
	     mortar_color, 0);


    /* make the mortar that goes between
     * horizontally adjacent bricks
     */
    VSET(pts[0], 0.0,	  0.0,		0.0);
    VSET(pts[1], 0.0,	  brick_depth,	0.0);
    VSET(pts[2], 0.0,	  brick_depth,	mortar_height+brick_height);
    VSET(pts[3], 0.0,	  0.0,		mortar_height+brick_height);

    VSET(pts[4], -mortar_width, 0.0,	 0.0);
    VSET(pts[5], -mortar_width, brick_depth, 0.0);
    VSET(pts[6], -mortar_width, brick_depth, mortar_height+brick_height);
    VSET(pts[7], -mortar_width, 0.0,	 mortar_height+brick_height);

    snprintf(sol_name, 64, "s.%s.vm", obj_name);
    mk_arb8(fd, sol_name, &pts[0][X]);

    (void)mk_addmember(sol_name, &wm_hd.l, NULL, WMOP_UNION);
    *sol_name = 'r';
    mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
	     mortar_color, 0);
#else
    BU_LIST_INIT(&wm_hd.l);

    bu_exit(0, "Not Yet Implemented\n");

#endif
}


void
brick(struct rt_wdb *UNUSED(fd))
{
    struct wmember wm_hd;
#if 0
    int horiz_bricks;
    int vert_bricks;
    double mortar_height;
    double mortar_width;
    point_t pts[8];
    char proto_brick[64];

    if (!color)
	color = brick_color;

    horiz_bricks = (WALL_WIDTH-brick_depth) / brick_width;
    mortar_width = WALL_WIDTH - horiz_bricks * brick_width;
    mortar_width /= horiz_bricks;

    vert_bricks = WALL_HEIGHT / brick_height;
    mortar_height = 0.0;


    /* make prototype brick */

    VSET(pts[0], 0.0,	  0.0,		mortar_height);
    VSET(pts[1], 0.0,	  brick_depth,	mortar_height);
    VSET(pts[2], brick_width, brick_depth,	mortar_height);
    VSET(pts[3], brick_width, 0.0,		mortar_height);

    VSET(pts[4], 0.0,	  0.0,		mortar_height+brick_height);
    VSET(pts[5], 0.0,	  brick_depth,	mortar_height+brick_height);
    VSET(pts[6], brick_width, brick_depth,	mortar_height+brick_height);
    VSET(pts[7], brick_width, 0.0,		mortar_height+brick_height);

    snprintf(proto_brick, 64, "s.%s.b", obj_name);
    mk_arb8(fd, proto_brick, &pts[0][X]);
    (void)mk_addmember(proto_brick, &wm_hd.l, NULL, WMOP_UNION);
    *proto_brick = 'r';

    mk_lcomb(fd, proto_brick, &wm_hd, 1, (char *)NULL, (char *)NULL,
	     (char *)NULL, 0);
#else
    BU_LIST_INIT(&wm_hd.l);

    bu_exit(0, "Not Yet Implemented\n");

#endif
}


/**
 * M A I N
 *
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char **av)
{
    struct opening *op;
    struct rt_wdb *db_fd;

    ol_hd.ex = ol_hd.ez = 0.0;

    if ((parse_args(ac, av)) < ac)
	usage("excess command line arguments\n");

    if (ac < 2)
	usage((char *)NULL);

    snprintf(sol_name, 64, "%s.g", progname);
    if ((db_fd = wdb_fopen(sol_name)) == (struct rt_wdb *)NULL) {
	perror(sol_name);
	return -1;
    }

    if (debug) {
	bu_log("Wall \"%s\"(%g) %g by %g\n", units, unit_conv,
	       WALL_WIDTH/unit_conv, WALL_HEIGHT/unit_conv);
	for (BU_LIST_FOR(op, opening, &ol_hd.l)) {
	    bu_log("opening at %g %g to %g %g\n",
		   op->sx/unit_conv, op->sz/unit_conv,
		   op->ex/unit_conv, op->ez/unit_conv);
	}
    }

    if (*type == 'f') {
	frame(db_fd);
    } else if (*type == 's') {
	sheetrock(db_fd);
    } else if (*type == 'b') {
	if (type[1] == 'm')
	    mortar_brick(db_fd);
	else
	    brick(db_fd);
    }

    wdb_close(db_fd);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
