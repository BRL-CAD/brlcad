/*
 *	Options
 *	h	help
 */
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "wdb.h"


struct opening {
	struct rt_list	l;
	double		sx;	/* start in X direction */
	double		sz;	/* start in Z direction */
	double		ex;	/* end in X direction */
	double		ez;	/* end in Z direction */
} ol_hd;

struct boardseg {
	struct rt_list	l;
	double		s;	/* start */
	double		e;	/* end */
};

/* declarations to support use of getopt() system call */
char *options = "w:o:hn:d";
extern char *optarg;
extern int optind, opterr, getopt();

int debug =0;
char *progname = "(noname)";
FILE *fd;
char *name = "wall";
char sol_name[64];
int sol_num = 0;
char sheetrock_color[3] = { '\200', '\200', '\200' };
char stud_color[3] = { '\200', '\200', '\150' };
/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(s)
char *s;
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr,
	"Usage: %s -w(all) width,height [-o(pening) lx,lz,hx,hz ...] model.g\n",
			progname);
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
	char *strrchr();
	struct opening *op;
	double dx, dy, width, height;

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
		case 'd'	: debug = !debug; break;
		case 'w'	: if (sscanf(optarg, "%lf,%lf",
					&width, &height)
				      == 2 && width > 3.0 && height > 3.0) {
					ol_hd.ex = width;
					ol_hd.ez = height;
				      }
				break;
		case 'o'	: if (sscanf(optarg, "%lf,%lf,%lf,%lf",
				     &dx, &dy, &width, &height) == 4) {
					op = (struct opening *)rt_calloc(1, sizeof(struct opening), "calloc opening");
				     	RT_LIST_INSERT(&ol_hd.l, &op->l);
				     	op->sx = dx;
				     	op->sz = dy;
				     	op->ex = width;
				     	op->ez = height;
				}
				break;
		case 'n'	: name = optarg; break;
		case '?'	:
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}

	if (ol_hd.ex <= 3.0) usage("wall width must exceed 3.0 in.\n");
	if (ol_hd.ez <= 3.0) usage("wall height must exceed 3.0 in.\n");

	return(optind);
}

void
fix_units(pts)
point_t pts[8];
{
	double unit_conv=25.4;
	int i;

	for (i=0 ; i < 8 ; ++i) {
		(pts[i])[0] *= unit_conv;
		(pts[i])[1] *= unit_conv;
		(pts[i])[2] *= unit_conv;
	}
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
	/* trim opening to X bounds of wall */
    	if (seg->s < ol_hd.sz) seg->s = ol_hd.sz;
    	if (seg->e > ol_hd.ez) seg->e = ol_hd.ez;
	RT_LIST_APPEND(&(seglist->l), &(seg->l));

	for (RT_LIST_FOR(op, opening, &ol_hd.l) ) {
	    for (RT_LIST_FOR(seg, boardseg, &(seglist->l)) ) {
	    	if ((op->sx >= sx && op->sx <= ex) ||
	    	    (op->ex >= sx && op->ez <= ex) ||
	    	    (op->sx <= sx && op->ex >= ex) ) {
	    	    	/* opening in vertical segment */

			if (op->sz <= seg->s) {
			    if (op->ez >= seg->e) {
			    	/* opening covers entire segment.
			    	 * segement gets deleted
			    	 */
			    	RT_LIST_DEQUEUE(&(seg->l));
			    	rt_free((char *)seg);
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
    	if (seg->e < ol_hd.ex) seg->e = ol_hd.ex;
	RT_LIST_APPEND(&(seglist->l), &(seg->l));



	for(RT_LIST_FOR(op, opening, &ol_hd.l) ) {
	    for (RT_LIST_FOR(seg, boardseg, &(seglist->l)) ) {

		if ((op->sz >= sz && op->sz <= ez) ||
		    (op->ez >= sz && op->ez <= ez) ||
		    (op->sz <= sz && op->ez >= ez) ) {
			/* opening in horizontal segment */
			if (op->sx <= seg->s) {
			    if (op->ex >= seg->e) {
			    	/* opening covers entire segment.
			    	 * segement gets deleted
			    	 */
			    	RT_LIST_DEQUEUE(&(seg->l));
			    	rt_free((char *)seg);
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
	struct wmember *wm;
	
	sprintf(sol_name, "s.%s.%d", name, sol_num++);

	fix_units(pts);
	mk_arb8(fd, sol_name, pts);


	wm = mk_addmember(sol_name, wm_hd, WMOP_UNION);
}

void
frame(fd)
FILE *fd;
{
	struct boardseg *s_hd, *seg;
	struct opening *op, *nop;
	point_t pts[8];
	double pos;
	int i=0;
	struct wmember wm_hd, wm_hd2, *wm;

	RT_LIST_INIT(&wm_hd.l);
	RT_LIST_INIT(&wm_hd2.l);

	mk_id(fd, "A wall");

	/* add the thickness of the opening frame to the size of the
	 * openings
	 */
	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {
		op->sx -= 1.5;
		op->ex += 1.5;
		op->sz -= 1.5;
		op->ez += 1.5;
	}

	/* find the segments of the base-board */
	s_hd = (struct boardseg *)rt_calloc(1, sizeof(struct boardseg), "s_hd");
	RT_LIST_INIT(&(s_hd->l));
	
	h_segs(0.0, 1.5, s_hd, 0.0, ol_hd.ex);
	
	/* make the base-board segments */
	while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

		if (debug) {
			printf("baseboard seg: %g -> %g\n", seg->s, seg->e);
		}

		VSET(pts[0], seg->s, 0.75, 0.0);
		VSET(pts[1], seg->s, 4.075, 0.0);
		VSET(pts[2], seg->s, 4.075, 1.5);
		VSET(pts[3], seg->s, 0.75, 1.5);
		VSET(pts[4], seg->e, 0.75, 0.0);
		VSET(pts[5], seg->e, 4.075, 0.0);
		VSET(pts[6], seg->e, 4.075, 1.5);
		VSET(pts[7], seg->e, 0.75, 1.5);

		RT_LIST_DEQUEUE(&(seg->l));
		rt_free( (char *)seg);

		mksolid(fd, pts, &wm_hd);
	}

	/* now find the segments of the cap board */

	h_segs(ol_hd.ez - 1.5, ol_hd.ez, s_hd, 0.0, ol_hd.ex);

	/* make the cap board segments */
	while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

		if (debug) {
			printf("capboard seg: %g -> %g\n", seg->s, seg->e);
		}

		VSET(pts[0], seg->s, 0.75, ol_hd.ez);
		VSET(pts[1], seg->s, 4.075, ol_hd.ez);
		VSET(pts[2], seg->s, 4.075, ol_hd.ez-1.5);
		VSET(pts[3], seg->s, 0.75, ol_hd.ez-1.5);
		VSET(pts[4], seg->e, 0.75, ol_hd.ez);
		VSET(pts[5], seg->e, 4.075, ol_hd.ez);
		VSET(pts[6], seg->e, 4.075, ol_hd.ez-1.5);
		VSET(pts[7], seg->e, 0.75, ol_hd.ez-1.5);

		RT_LIST_DEQUEUE(&(seg->l));
		rt_free( (char *)seg);

		mksolid(fd, pts, &wm_hd);
	}

	/* make the base board for each of the openings */
	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {

	    nop = RT_LIST_NEXT(opening, &op->l);
	    RT_LIST_DEQUEUE(&(op->l));

	    if (op->sz > 0.0) {
		/* build base board segments for the opening */

		h_segs(op->sz, op->sz+1.5, s_hd, op->sx, op->ex);
		
		/* make opening base board(s) */
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

			if (debug) {
				printf("opening base-board seg: %g -> %g\n", seg->s, seg->e);
			}


			VSET(pts[0], op->sx, 0.75, op->sz);
			VSET(pts[1], op->sx, 4.075, op->sz);
			VSET(pts[2], op->sx, 4.075, op->sz+1.5);
			VSET(pts[3], op->sx, 0.75, op->sz+1.5);
			VSET(pts[4], op->ex, 0.75, op->sz);
			VSET(pts[5], op->ex, 4.075, op->sz);
			VSET(pts[6], op->ex, 4.075, op->sz+1.5);
			VSET(pts[7], op->ex, 0.75, op->sz+1.5);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg);

			mksolid(fd, pts, &wm_hd);
		}
	    }
	    
	    if (op->ez < ol_hd.ez) {
		/* build cap board segments for the opening */

		h_segs(op->ez-1.5, op->ez, s_hd, op->sx, op->ex);
		
		/* make opening cap board(s) */
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {

			if (debug) {
				printf("opening capboard seg: %g -> %g\n",
					seg->s, seg->e);
			}

			VSET(pts[0], op->sx, 0.75, op->ez);
			VSET(pts[1], op->sx, 4.075, op->ez);
			VSET(pts[2], op->sx, 4.075, op->ez-1.5);
			VSET(pts[3], op->sx, 0.75, op->ez-1.5);
			VSET(pts[4], op->ex, 0.75, op->ez);
			VSET(pts[5], op->ex, 4.075, op->ez);
			VSET(pts[6], op->ex, 4.075, op->ez-1.5);
			VSET(pts[7], op->ex, 0.75, op->ez-1.5);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg);

			mksolid(fd, pts, &wm_hd);
		}
	    }
	    RT_LIST_INSERT(&(nop->l), &(op->l));
	}

	/* this concludes the horizontal segments.  It's time to build the
	 * vertical studs.
	 */

	for (pos = 0.0 ; pos <= ol_hd.ex-1.5 ; pos += 16.0) {
		v_segs(1.5, ol_hd.ez-1.5, s_hd, pos, pos+1.5);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("stud @ %g Zmin:%g  Zmax:%g\n",
					pos, seg->s, seg->e);

			
			VSET(pts[0], pos, 0.75, seg->s);
			VSET(pts[1], pos, 4.075, seg->s);
			VSET(pts[2], pos+1.5, 4.075, seg->s);
			VSET(pts[3], pos+1.5, 4.075, seg->s);
			VSET(pts[4], pos, 0.75, seg->e);
			VSET(pts[5], pos, 4.075, seg->e);
			VSET(pts[6], pos+1.5, 4.075, seg->e);
			VSET(pts[7], pos+1.5, 4.075, seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg);

			mksolid(fd, pts, &wm_hd);
		}
	}

	/* make sure the closing stud is in place */
	if (pos-16.0 < ol_hd.ex-3.0) {
		pos = ol_hd.ex - 1.5;
		v_segs(1.5, ol_hd.ez-1.5, s_hd, pos, pos+1.5);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("stud @ %g Zmin:%g  Zmax:%g\n",
					pos, seg->s, seg->e);

			VSET(pts[0], pos, 0.75, seg->s);
			VSET(pts[1], pos, 4.075, seg->s);
			VSET(pts[2], pos+1.5, 4.075, seg->s);
			VSET(pts[3], pos+1.5, 4.075, seg->s);
			VSET(pts[4], pos, 0.75, seg->e);
			VSET(pts[5], pos, 4.075, seg->e);
			VSET(pts[6], pos+1.5, 4.075, seg->e);
			VSET(pts[7], pos+1.5, 4.075, seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg);

			mksolid(fd, pts, &wm_hd);
		}
	}


	/* put the vertical frame pieces in the openings */
	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {

	    nop = RT_LIST_NEXT(opening, &op->l);
	    RT_LIST_DEQUEUE(&(op->l));

	    if (op->sx > 0.0) {
	    	v_segs(op->sz+1.5, op->ez-1.5, s_hd, op->sx, op->sx+1.5);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("opening vl frame @ %g Zmin:%g  Zmax:%g\n",
					op->sx, seg->s, seg->e);

			VSET(pts[0], op->sx, 0.75, seg->s);
			VSET(pts[1], op->sx, 4.075, seg->s);
			VSET(pts[2], op->sx+1.5, 4.075, seg->s);
			VSET(pts[3], op->sx+1.5, 0.75, seg->s);
			VSET(pts[4], op->sx, 0.75, seg->e);
			VSET(pts[5], op->sx, 4.075, seg->e);
			VSET(pts[6], op->sx+1.5, 4.075, seg->e);
			VSET(pts[7], op->sx+1.5, 0.75, seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg);

			mksolid(fd, pts, &wm_hd);
		}
	    	
	    }
	    if (op->ex < ol_hd.ex) {
	    	v_segs(op->sz+1.5, op->ez-1.5, s_hd, op->ex-1.5, op->ex);
		while (RT_LIST_WHILE(seg, boardseg, &(s_hd->l))) {
			if (debug)
				printf("opening vl frame @ %g Zmin:%g  Zmax:%g\n",
					op->sx, seg->s, seg->e);

			VSET(pts[0], op->ex-1.5, 0.75, seg->s);
			VSET(pts[1], op->ex-1.5, 4.075, seg->s);
			VSET(pts[2], op->ex, 4.075, seg->s);
			VSET(pts[3], op->ex, 0.75, seg->s);
			VSET(pts[4], op->ex-1.5, 0.75, seg->e);
			VSET(pts[5], op->ex-1.5, 4.075, seg->e);
			VSET(pts[6], op->ex, 4.075, seg->e);
			VSET(pts[7], op->ex, 0.75, seg->e);

			RT_LIST_DEQUEUE(&(seg->l));
			rt_free( (char *)seg);

			mksolid(fd, pts, &wm_hd);
		}
	    }

	    RT_LIST_INSERT(&(nop->l), &(op->l));

	}

	/* put all the studding in a region */
	sprintf(sol_name, "r.%s.studs", name);
	mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
		stud_color, 0);

	/* add some sheet-rock */
	VSET(pts[0], 0.0, 0.0, 0.0);
	VSET(pts[1], 0.0, 0.75, 0.0);
	VSET(pts[2], 0.0, 0.75, ol_hd.ez);
	VSET(pts[3], 0.0, 0.0, ol_hd.ez);
	VSET(pts[4], ol_hd.ex, 0.0, 0.0);
	VSET(pts[5], ol_hd.ex, 0.75, 0.0);
	VSET(pts[6], ol_hd.ex, 0.75, ol_hd.ez);
	VSET(pts[7], ol_hd.ex, 0.0, ol_hd.ez);

	sprintf(sol_name, "s.%s.sr1", name);
	fix_units(pts);
	mk_arb8(fd, sol_name, pts);
	wm = mk_addmember(sol_name, &wm_hd, WMOP_UNION);


	VSET(pts[0], 0.0, 4.075, 0.0);
	VSET(pts[1], 0.0, 4.825, 0.0);
	VSET(pts[2], 0.0, 4.825, ol_hd.ez);
	VSET(pts[3], 0.0, 4.075, ol_hd.ez);
	VSET(pts[4], ol_hd.ex, 4.075, 0.0);
	VSET(pts[5], ol_hd.ex, 4.825, 0.0);
	VSET(pts[6], ol_hd.ex, 4.825, ol_hd.ez);
	VSET(pts[7], ol_hd.ex, 4.075, ol_hd.ez);

	sprintf(sol_name, "s.%s.sr2", name);
	fix_units(pts);
	mk_arb8(fd, sol_name, pts);
	wm = mk_addmember(sol_name, &wm_hd2, WMOP_UNION);


	for (RT_LIST_FOR(op, opening, &ol_hd.l)) {
		VSET(pts[0], op->sx, -0.01, op->sz);
		VSET(pts[1], op->sx, 4.826, op->sz);
		VSET(pts[2], op->sx, 4.826, op->ez);
		VSET(pts[3], op->sx, -0.01, op->ez);
		VSET(pts[4], op->ex, -0.01, op->sz);
		VSET(pts[5], op->ex, 4.826, op->sz);
		VSET(pts[6], op->ex, 4.826, op->ez);
		VSET(pts[7], op->ex, -0.01, op->ez);

		sprintf(sol_name, "s.%s.o.%d", name, i++);
		fix_units(pts);
		mk_arb8(fd, sol_name, pts);
		wm = mk_addmember(sol_name, &wm_hd, WMOP_SUBTRACT);
		wm = mk_addmember(sol_name, &wm_hd2, WMOP_SUBTRACT);
	}

	sprintf(sol_name, "r.%s.sr1", name);
	mk_lcomb(fd, sol_name, &wm_hd, 1, (char *)NULL, (char *)NULL,
		sheetrock_color, 0);

	sprintf(sol_name, "r.%s.sr2", name);
	mk_lcomb(fd, sol_name, &wm_hd2, 1, (char *)NULL, (char *)NULL,
		sheetrock_color, 0);
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

	if ((arg_index=parse_args(ac, av)) >= ac)
		usage("no database file specified\n");


	if ((fd = fopen(av[arg_index], "w")) == (FILE *)NULL) {
		perror(av[arg_index]);
		return(-1);
	}

	if (debug) {
		printf("Wall %g by %g\n", ol_hd.ex, ol_hd.ez);
		for (RT_LIST_FOR(op, opening, &ol_hd.l)) {
			printf("opening at %g %g to %g %g\n",
				op->sx, op->sz, op->ex, op->ez);
		}
	}

	frame(fd);

	return(fclose(fd));
}
