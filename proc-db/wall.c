/*	W A L L . C --- build a wall.
 *
 *	Currently builds "wood frame" walls for typical building constructs.
 *	
 */
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "wdb.h"
#include "raytrace.h"

/* declarations to support use of getopt() system call */
char *options = "w:o:n:t:b:u:c:rlhdm:";
extern char *optarg;
extern int optind, opterr, getopt();

int debug = 0;
char *progname = "(noname)";
FILE *fd;
char *obj_name = "wall";
char sol_name[64];
int sol_num = 0;
char *type = "frame";
char *units = "mm";
double unit_conv = 1.0;
char *color;
char def_color[3];

int log_cmds = 0;	/* log sessions to a log file */
/* standard construction brick:
 * 8" by 2 1/4" by 3 3/8"
 */
double brick_height = 8.0 * 25.4;
double brick_width = 2.25 * 25.4;
double brick_depth = 3.25 * 25.4;
double min_mortar = 0.25 * 25.4;
char brick_color[3] = {160, 40, 40};
char mortar_color[3] = {190, 190, 190};
int rand_brick_color = 0;
int make_mortar = 1;

/* real dimensions of a "2 by 4" board */
double bd_thick = 3.25 * 25.4;
double bd_thin = 1.5 * 25.4;
double sr_thick = 0.75 * 25.4;	/* sheetrock thickness */
double stud_spacing = 16.0 * 25.4; /* spacing between vertical studs */
char sheetrock_color[3] = { 200, 200, 200 };
char stud_color[3] = { 250, 178, 108 };
char *stud_properties[] = { "plastic", "sh=10 di=0.7 sp=0.3" };

struct opening {
	struct rt_list	l;
	double		sx;	/* start in X direction */
	double		sz;	/* start in Z direction */
	double		ex;	/* end in X direction */
	double		ez;	/* end in Z direction */
} ol_hd;

#define WALL_WIDTH ol_hd.ex
#define WALL_HEIGHT ol_hd.ez

struct boardseg {
	struct rt_list	l;
	double		s;	/* start */
	double		e;	/* end */
};


/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(s)
char *s;
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s %s\n%s\n%s\n%s\n",
progname,
"[ -u units ] -w(all) width,height [-o(pening) lx,lz,hx,hz ...]",
" [-n name] [ -d(ebug) ] [-t {frame|brick|block|sheetrock} ] [-c R/G/B]",
" [-l(og_commands)]",
" brick sub-options: [-r(and_color)] [-b width,height,depth ] [-m min_mortar]"
);

	exit(1);
}



/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(ac, av)
int ac;
char *av[];
{
	int  c;

#if defined(BSD) && BSD < 43
#define strrchr rindex
#endif
	char *strrchr();
	struct opening *op;
	double dx, dy, width, height;
	int R, G, B;
	int units_lock=0;
	FILE *logfile;

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;


	RT_LIST_INIT(&ol_hd.l);

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'b'	: if (sscanf(optarg, "%lf,%lf,%lf",
				     &width, &height, &dy) == 3) {
					brick_width = width * unit_conv;
					brick_height = height * unit_conv;
					brick_depth = dy * unit_conv;
				     	units_lock = 1;
				  } else
				  	usage("error parsing -b option\n");

				  break;
		case 'c'	: if (sscanf(optarg, "%d %d %d", &R, &G, &B)
				     == 3) {
				     	color = def_color;
					color[0] = R & 0x0ff;
					color[1] = G & 0x0ff;
					color[2] = B & 0x0ff;
				  }
				  break;
		case 'd'	: debug = !debug; break;
		case 'l'	: log_cmds = !log_cmds; break;
		case 'm'	: if ((dx=atof(optarg)) > 0.0) {
					min_mortar = dx * unit_conv;
					units_lock = 1;
					make_mortar = 1;
				} else
					usage("error parsing -m option\n");

				  break;
		case 'n'	: obj_name = optarg; break;
		case 'o'	: if (sscanf(optarg, "%lf,%lf,%lf,%lf",
				     &dx, &dy, &width, &height) == 4) {
					op = (struct opening *)rt_calloc(1, sizeof(struct opening), "calloc opening");
				     	RT_LIST_INSERT(&ol_hd.l, &op->l);
				     	op->sx = dx * unit_conv;
				     	op->sz = dy * unit_conv;
				     	op->ex = width * unit_conv;
				     	op->ez = height * unit_conv;
				     	units_lock = 1;
				} else
					usage("error parsing -o option\n");
				break;
		case 'r'	: rand_brick_color = !rand_brick_color; break;
		case 't'	: type = optarg; break;
		case 'u'	: if (units_lock)
					(void)fprintf(stderr,
					"Warning: attempting to change units in mid-parse\n");
				if ((dx=rt_units_conversion(optarg)) != 0.0) {
					unit_conv = dx;
					units = optarg;
				} else
					usage("error parsing -u (units)\n");
				break;
		case 'w'	: if (sscanf(optarg, "%lf,%lf",
				     &width, &height) == 2) {
					WALL_WIDTH = width * unit_conv;
					WALL_HEIGHT = height * unit_conv;
				     	units_lock = 1;
				  } else
				  	usage("error parsing -w (wall dimensions)\n");
				break;
		case '?'	:
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}


	if (log_cmds) {
		if ((logfile=fopen("wall.log", "a+")) == (FILE *)NULL) {
			perror("wall.log");
			exit(-1);
		}
		for (R=0 ; R < ac ; R++)
			(void)fprintf(logfile, "%s ", av[R]);
		(void)putc('\n', logfile);
		(void)fclose(logfile);
	}

	return(optind);

}


void
v_segs(sz, ez, seglist, sx, ex)
double sz, ez, sx, ex;
struct boardseg *seglist;
{
	struct opening *op;
	struct boardseg *seg, *sp;

	seg = (struct boardseg *)rt_calloc(1, sizeof(struct boardseg), "initial seg");
	seg->s = sz;
	seg->e = ez;

	if (!seg) abort();

	/* trim opening to X bounds of wall */
    	if (seg->s < ol_hd.sz) seg->s = ol_hd.sz;
    	if (seg->e > WALL_HEIGHT) seg->e = WALL_HEIGHT;
	RT_LIST_APPEND(&(seglist->l), &(seg->l));


	for (RT_LIST_FOR(op, opening, &ol_hd.l) ) {
	    if ((sx >= op->sx && sx <= op->ex) ||
	    	    (ex >= op->sx && ex <= op->ex) ||
	    	    (sx <= op->sx && ex >= op->ex) ) {

    	    	/* opening in vertical segment */
	    	for (RT_LIST_FOR(seg, boardseg, &(seglist->l)) ) {


			if (op->sz <= seg->s) {
			    if (op->ez >= seg->e) {
			    	/* opening covers entire segment.
			    	 * segement gets deleted
			    	 */
			    	sp = RT_LIST_PLAST(boardseg, &(seg->l));
			    	RT_LIST_DEQUEUE(&(seg->l));
			    	rt_free((char *)seg, "seg free");
				if (debug)
				 	printf("deleting segment\n");
			    	seg = sp;
			    } else if (op->ez > seg->s) {
			    	/* opening covers begining of segment */
			    	seg->s = op->ez;
			    }
				/* else opening is entirely prior to seg->s */
			} else if (op->ez >= seg->e) {
			    if (op->sz < seg->e) {
			    	/* opening covers end of segment */
			    	seg->e = op->sz;
			    } 
			    /* else opening entirely after segment */
			} else {
				/* there is an opening in the middle of the
				 * segment.  We must divide the segment into
				 * 2 segements
				 */
				 sp = (struct boardseg *)rt_calloc(1, sizeof(struct boardseg), "alloc boardseg");
				 sp->s = seg->s;
				 sp->e = op->sz;
				 seg->s = op->ez;
				 RT_LIST_INSERT(&(seg->l), &(sp->l));
				 if (debug)
				 	printf("splitting segment\n");
			}



	    	    }
	    }
	}
}

void
h_segs(sz, ez, seglist, sx, ex)
double sz, ez, sx, ex;
struct boardseg *seglist;
{
	struct opening *op;
	struct boardseg *seg, *sp;

	seg = (struct boardseg *)rt_calloc(1, sizeof(struct boardseg), "initial seg");
	seg->s = sx;
	seg->e = ex;
	/* trim opening to X bounds of wall */
    	if (seg->s < ol_hd.sx) seg->s = ol_hd.sx;
    	if (seg->e > WALL_WIDTH) seg->e = WALL_WIDTH;
	RT_LIST_APPEND(&(seglist->l), &(seg->l));



	for(RT_LIST_FOR(op, opening, &ol_hd.l) ) {

	    if ((op->sz >= sz && op->sz <= ez) ||
		(op->ez >= sz && op->ez <= ez) ||
		(op->sz <= sz && op->ez >= ez) ) {

		/* opening in horizontal segment */
	    	for (RT_LIST_FOR(seg, boardseg, &(seglist->l)) ) {
			if (op->sx <= seg->s) {
			    if (op->ex >= seg->e) {
			    	/* opening covers entire segment.
			    	 * segement gets deleted
			    	 */
			    	sp = RT_LIST_PLAST(boardseg, &(seg->l));
			    	RT_LIST_DEQUEUE(&(seg->l));
			    	rt_free((char *)seg, "seg free 2");
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
				 sp = (struct boardseg *)rt_calloc(1, sizeof(struct boardseg), "alloc boardseg");
				 sp->s = seg->s;
				 sp->e = op->sx;
				 seg->s = op->ex;
				 RT_LIST_INSERT(&(seg->l), &(sp->l));
			}
		}
	    }
	}

}
mksolid(fd, pts, wm_hd)
FILE *fd;
point_t pts[8];
struct wmember *wm_hd;
{
	sprintf(sol_name, "s.%s.%d", obj_name, sol_num++);

	mk_arb8(fd, sol_name, pts);
	(void)mk_addmember(sol_name, wm_hd, WMOP_UNION);
}

void
frame(fd)
FILE *fd;
{
	struct boardseg *s_hd, *seg;
	struct opening *op, *nop;
	point_t pts[8];
	double pos;
	struct wmember wm_hd;

	if (WALL_WIDTH <= bd_thin*2) {
		(void)fprintf(stderr, "wall width must exceed %g.\n", (bd_thin*2)/unit_conv);
		return;
	}
	if (WALL_HEIGHT <= bd_thin*2) {
		(void)fprintf(stderr, "wall height must exceed %g.\n", (bd_thin*2)/unit_conv);
		return;
	}
	RT_LIST_INIT(&wm_hd.l);

	if (!color) color = stud_color;


	mk_id(fd, "A wall");

	/* add the thickness of the opening frame to the size of the
	 * openings
	 */
	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {
		op->sx -= bd_thin;
		op->ex += bd_thin;
		op->sz -= bd_thin;
		op->ez += bd_thin;
	}

	/* find the segments of the base-board */
	s_hd = (struct boardseg *)rt_calloc(1, sizeof(struct boardseg), "s_hd");
	RT_LIST_INIT(&(s_hd->l));
	
	h_segs(0.0, bd_thin, s_hd, 0.0, WALL_WIDTH);
	
	/* make the base-board segments */
	while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

		if (debug) {
			printf("baseboard seg: %g -> %g\n",
				seg->s/unit_conv, seg->e/unit_conv);
		}

		VSET(pts[0], seg->s, 0.0, 0.0);
		VSET(pts[1], seg->s, bd_thick, 0.0);
		VSET(pts[2], seg->s, bd_thick, bd_thin);
		VSET(pts[3], seg->s, 0.0, bd_thin);
		VSET(pts[4], seg->e, 0.0, 0.0);
		VSET(pts[5], seg->e, bd_thick, 0.0);
		VSET(pts[6], seg->e, bd_thick, bd_thin);
		VSET(pts[7], seg->e, 0.0, bd_thin);

		RT_LIST_DEQUEUE(&(seg->l));
		rt_free( (char *)seg, "seg free 3");

		mksolid(fd, pts, &wm_hd);
	}

	/* now find the segments of the cap board */

	h_segs(WALL_HEIGHT - bd_thin, WALL_HEIGHT, s_hd, 0.0, WALL_WIDTH);

	/* make the cap board segments */
	while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

		if (debug) {
			printf("capboard seg: %g -> %g\n",
				seg->s/unit_conv, seg->e/unit_conv);
		}

		VSET(pts[0], seg->s, 0.0, WALL_HEIGHT);
		VSET(pts[1], seg->s, bd_thick, WALL_HEIGHT);
		VSET(pts[2], seg->s, bd_thick, WALL_HEIGHT-bd_thin);
		VSET(pts[3], seg->s, 0.0, WALL_HEIGHT-bd_thin);
		VSET(pts[4], seg->e, 0.0, WALL_HEIGHT);
		VSET(pts[5], seg->e, bd_thick, WALL_HEIGHT);
		VSET(pts[6], seg->e, bd_thick, WALL_HEIGHT-bd_thin);
		VSET(pts[7], seg->e, 0.0, WALL_HEIGHT-bd_thin);

		RT_LIST_DEQUEUE(&(seg->l));
		rt_free( (char *)seg, "seg_free 4");

		mksolid(fd, pts, &wm_hd);
	}

	/* make the base board for each of the openings */
	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {

	    nop = RT_LIST_NEXT(opening, &op->l);
	    RT_LIST_DEQUEUE(&(op->l));

	    if (op->sz > 0.0) {
		/* build base board segments for the opening */

		h_segs(op->sz, op->sz+bd_thin, s_hd, op->sx, op->ex);

		/* make opening base board(s) */
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

			if (debug) {
				printf("opening base-board seg: %g -> %g (%d)\n",
					seg->s/unit_conv, seg->e/unit_conv, sol_num);
			}

			VSET(pts[0], seg->s, 0.0, op->sz);
			VSET(pts[1], seg->s, bd_thick, op->sz);
			VSET(pts[2], seg->s, bd_thick, op->sz+bd_thin);
			VSET(pts[3], seg->s, 0.0, op->sz+bd_thin);
			VSET(pts[4], seg->e, 0.0, op->sz);
			VSET(pts[5], seg->e, bd_thick, op->sz);
			VSET(pts[6], seg->e, bd_thick, op->sz+bd_thin);
			VSET(pts[7], seg->e, 0.0, op->sz+bd_thin);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg, "seg_free 5");

			mksolid(fd, pts, &wm_hd);
		}
	    }
	    
	    if (op->ez < WALL_HEIGHT) {
		/* build cap board segments for the opening */

		h_segs(op->ez-bd_thin, op->ez, s_hd, op->sx, op->ex);
		
		/* make opening cap board(s) */
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

			if (debug) {
				printf("opening capboard seg: %g -> %g\n",
					seg->s/unit_conv, seg->e/unit_conv);
			}

			VSET(pts[0], seg->s, 0.0, op->ez);
			VSET(pts[1], seg->s, bd_thick, op->ez);
			VSET(pts[2], seg->s, bd_thick, op->ez-bd_thin);
			VSET(pts[3], seg->s, 0.0, op->ez-bd_thin);
			VSET(pts[4], seg->e, 0.0, op->ez);
			VSET(pts[5], seg->e, bd_thick, op->ez);
			VSET(pts[6], seg->e, bd_thick, op->ez-bd_thin);
			VSET(pts[7], seg->e, 0.0, op->ez-bd_thin);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg, "seg_free 6");

			mksolid(fd, pts, &wm_hd);
		}
	    }
	    RT_LIST_INSERT(&(nop->l), &(op->l));
	}

	/* this concludes the horizontal segments.  It's time to build the
	 * vertical studs.
	 */

	for (pos=0.0 ; pos+bd_thin*2.0 <= WALL_WIDTH ; pos += stud_spacing) {
		v_segs(bd_thin, WALL_HEIGHT-bd_thin, s_hd, pos, pos+bd_thin);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("stud %d @ %g Zmin:%g  Zmax:%g\n",
					sol_num, pos/unit_conv,
					seg->s/unit_conv, seg->e/unit_conv);

			
			VSET(pts[0], pos, 	  0.0,	     seg->s);
			VSET(pts[1], pos,	  bd_thick, seg->s);
			VSET(pts[2], pos+bd_thin, bd_thick, seg->s);
			VSET(pts[3], pos+bd_thin, 0.0,	     seg->s);
			VSET(pts[4], pos, 	  0.0,	     seg->e);
			VSET(pts[5], pos,	  bd_thick, seg->e);
			VSET(pts[6], pos+bd_thin, bd_thick, seg->e);
			VSET(pts[7], pos+bd_thin, 0.0,	     seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg, "seg_free 7");

			mksolid(fd, pts, &wm_hd);
		}
	}

	/* make sure the closing stud is in place */
	if (pos - stud_spacing + bd_thin*2.0 < WALL_WIDTH ) {
		pos = WALL_WIDTH - bd_thin;
		v_segs(bd_thin, WALL_HEIGHT-bd_thin, s_hd, pos, pos+bd_thin);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("last stud %d @ %g Zmin:%g  Zmax:%g\n",
					sol_num, pos/unit_conv,
					seg->s/unit_conv, seg->e/unit_conv);

			VSET(pts[0], pos,	  0.0,	     seg->s);
			VSET(pts[1], pos,	  bd_thick, seg->s);
			VSET(pts[2], pos+bd_thin, bd_thick, seg->s);
			VSET(pts[3], pos+bd_thin, 0.0,          seg->s);
			VSET(pts[4], pos,	  0.0,          seg->e);
			VSET(pts[5], pos,	  bd_thick, seg->e);
			VSET(pts[6], pos+bd_thin, bd_thick, seg->e);
			VSET(pts[7], pos+bd_thin, 0.0,	     seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg, "seg_free 8");

			mksolid(fd, pts, &wm_hd);
		}
	}


	/* put the vertical frame pieces in the openings */
	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {

	    nop = RT_LIST_NEXT(opening, &op->l);
	    RT_LIST_DEQUEUE(&(op->l));

	    if (op->sx > 0.0) {
	    	v_segs(op->sz+bd_thin, op->ez-bd_thin, s_hd, op->sx, op->sx+bd_thin);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("opening vl frame @ %g Zmin:%g  Zmax:%g\n",
					op->sx/unit_conv,
					seg->s/unit_conv, seg->e/unit_conv);

			VSET(pts[0], op->sx,	     0.0,		seg->s);
			VSET(pts[1], op->sx,	     bd_thick, seg->s);
			VSET(pts[2], op->sx+bd_thin, bd_thick, seg->s);
			VSET(pts[3], op->sx+bd_thin, 0.0,		seg->s);
			VSET(pts[4], op->sx,	     0.0,		seg->e);
			VSET(pts[5], op->sx,	     bd_thick, seg->e);
			VSET(pts[6], op->sx+bd_thin, bd_thick, seg->e);
			VSET(pts[7], op->sx+bd_thin, 0.0,		seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg, "seg free 9");

			mksolid(fd, pts, &wm_hd);
		}
	    	
	    }
	    if (op->ex < WALL_WIDTH) {
	    	v_segs(op->sz+bd_thin, op->ez-bd_thin, s_hd, op->ex-bd_thin, op->ex);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("opening vr frame @ %g Zmin:%g  Zmax:%g\n",
					op->sx/unit_conv,
					seg->s/unit_conv, seg->e/unit_conv);

			VSET(pts[0], op->ex-bd_thin, 0.0,		seg->s);
			VSET(pts[1], op->ex-bd_thin, bd_thick,	seg->s);
			VSET(pts[2], op->ex, 	     bd_thick, seg->s);
			VSET(pts[3], op->ex,	     0.0,		seg->s);
			VSET(pts[4], op->ex-bd_thin, 0.0,		seg->e);
			VSET(pts[5], op->ex-bd_thin, bd_thick, seg->e);
			VSET(pts[6], op->ex,	     bd_thick, seg->e);
			VSET(pts[7], op->ex,	     0.0,		seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg, "seg_free 10");

			mksolid(fd, pts, &wm_hd);
		}
	    }

	    RT_LIST_INSERT(&(nop->l), &(op->l));

	}


	/* put all the studding in a region */
	sprintf(sol_name, "r.%s.studs", obj_name);
	mk_lcomb(fd, sol_name, &wm_hd, 1,
		stud_properties[0], stud_properties[1], color, 0);

}

void
sheetrock(fd)
FILE *fd;
{
	point_t pts[8];
	struct wmember wm_hd;
	struct opening *op;
	int i=0;
	
	if (!color) color = sheetrock_color;

	RT_LIST_INIT(&wm_hd.l);

	/* now add the sheetrock */
	VSET(pts[0], 0.0, 0.0, 0.0);
	VSET(pts[1], 0.0, sr_thick, 0.0);
	VSET(pts[2], 0.0, sr_thick, WALL_HEIGHT);
	VSET(pts[3], 0.0, 0.0, WALL_HEIGHT);
	VSET(pts[4], WALL_WIDTH, 0.0, 0.0);
	VSET(pts[5], WALL_WIDTH, sr_thick, 0.0);
	VSET(pts[6], WALL_WIDTH, sr_thick, WALL_HEIGHT);
	VSET(pts[7], WALL_WIDTH, 0.0, WALL_HEIGHT);

	sprintf(sol_name, "s.%s.sr1", obj_name);
	mk_arb8(fd, sol_name, pts);
	(void)mk_addmember(sol_name, &wm_hd, WMOP_UNION);

	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {
		VSET(pts[0], op->sx, -0.01,		op->sz);
		VSET(pts[1], op->sx, sr_thick+0.01,	op->sz);
		VSET(pts[2], op->sx, sr_thick+0.01,	op->ez);
		VSET(pts[3], op->sx, -0.01,		op->ez);
		VSET(pts[4], op->ex, -0.01,		op->sz);
		VSET(pts[5], op->ex, sr_thick+0.01,	op->sz);
		VSET(pts[6], op->ex, sr_thick+0.01,	op->ez);
		VSET(pts[7], op->ex, -0.01,		op->ez);

		sprintf(sol_name, "s.%s.o.%d", obj_name, i++);
		mk_arb8(fd, sol_name, pts);
		(void)mk_addmember(sol_name, &wm_hd, WMOP_SUBTRACT);
	}

	sprintf(sol_name, "r.%s.sr1", obj_name);
	mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
		color, 0);

}

void
mortar_brick(fd)
FILE *fd;
{
	int horiz_bricks;
	int vert_bricks;
	double mortar_height;
	double mortar_width;
	int make_mortar = (type[1] == 'm');
	point_t pts[8];
	struct wmember wm_hd;
	
	RT_LIST_INIT(&wm_hd.l);

	fputs("Not Yet Implemented\n", stderr);
	exit(0);

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

	sprintf(sol_name, "s.%s.b", obj_name);
	mk_arb8(fd, sol_name, pts);

	(void)mk_addmember(sol_name, &wm_hd, WMOP_UNION);
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
		
	sprintf(sol_name, "s.%s.vm", obj_name);
	mk_arb8(fd, sol_name, pts);

	(void)mk_addmember(sol_name, &wm_hd, WMOP_UNION);
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

	sprintf(sol_name, "s.%s.vm", obj_name);
	mk_arb8(fd, sol_name, pts);

	(void)mk_addmember(sol_name, &wm_hd, WMOP_UNION);
	*sol_name = 'r';
	mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
		mortar_color, 0);
}


void
brick(fd)
FILE *fd;
{
	int horiz_bricks;
	int vert_bricks;
	double mortar_height;
	double mortar_width;
	int make_mortar = (type[1] == 'm');
	point_t pts[8];
	struct wmember wm_hd;
	char proto_brick[64];
	
	RT_LIST_INIT(&wm_hd.l);

	fputs("Not Yet Implemented\n", stderr);
	exit(0);

	if (!color) color = brick_color;

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

	sprintf(proto_brick, "s.%s.b", obj_name);
	mk_arb8(fd, proto_brick, pts);
	(void)mk_addmember(proto_brick, &wm_hd, WMOP_UNION);
	*proto_brick = 'r';

	mk_lcomb(fd, proto_brick, &wm_hd, 1, (char *)NULL, (char *)NULL,
			(char *)NULL, 0);

	
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
int ac;
char *av[];
{
	int arg_index;
	struct opening *op;

	if ((arg_index=parse_args(ac, av)) < ac)
		usage("excess command line arguments\n");

	if (ac < 2) usage((char *)NULL);

	sprintf(sol_name, "%s.g", obj_name);
	if ((fd = fopen(sol_name, "w")) == (FILE *)NULL) {
		perror(sol_name);
		return(-1);
	}

	if (debug) {
		printf("Wall \"%s\"(%g) %g by %g\n", units, unit_conv,
			WALL_WIDTH/unit_conv, WALL_HEIGHT/unit_conv);
		for (RT_LIST_FOR(op, opening, &ol_hd.l)) {
			printf("opening at %g %g to %g %g\n",
				op->sx/unit_conv, op->sz/unit_conv,
				op->ex/unit_conv, op->ez/unit_conv);
		}
	}

	if (*type == 'f') frame(fd);
	else if (*type == 's') sheetrock(fd);
	else if (*type == 'b') {
		if (type[1] == 'm' ) mortar_brick(fd);
		else brick(fd);
	}

	return(fclose(fd));
}
