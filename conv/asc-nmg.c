/*
 *			A S C - N M G . C
 *
 *  Program to convert an ascii description of an NMG into a BRL-CAD
 *  NMG model.
 *
 *  Authors -
 *	Michael Markowski
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlist.h"

struct faceuse *nmg_add_loop_to_face();

char		usage[] = "Usage: %s [file]\n";
extern char	*optarg;
extern int	optind;

/*
 *	M a i n
 *
 *	Get ascii input file and output file names.
 */
main(argc, argv)
int	argc;
char	*argv[];
{
	char		*afile, *bfile;
	FILE		*fpin, *fpout;

	/* Get ascii NMG input file name. */
	if (optind >= argc) {
		afile = "-";
		fpin = stdin;
	} else {
		afile = argv[optind];
		if ((fpin = fopen(afile, "r")) == NULL) {
			fprintf(stderr,
				"%s: cannot open %s for reading\n",
				argv[0], afile);
			exit(1);
		}
	}

	/* Get BRL-CAD output data base name. */
	optind++;
	if (optind >= argc) {
		bfile = "-";
		fpout = stdout;
	} else {
		bfile = argv[optind];
		if ((fpout = fopen(bfile, "w")) == NULL) {
			fprintf(stderr, "%s: cannot open %s for writing\n",
				argv[0], bfile);
			exit(1);
		}
	}

	ascii_to_brlcad(fpin, fpout, "nmg", NULL);
	fclose(fpin);
	fclose(fpout);
}

/*
 *	C r e a t e _ B r l c a d _ D b
 *
 *	Write the nmg to a brl-cad style data base.
 */
void
create_brlcad_db(fpout, m, reg_name, grp_name)
FILE		*fpout;
char		*grp_name, *reg_name;
struct model	*m;
{
	char	*rname, *sname;

	mk_id(fpout, "Ascii NMG");

	rname = malloc(sizeof(reg_name) + 3);	/* Region name. */
	sname = malloc(sizeof(reg_name) + 3);	/* Solid name. */

	sprintf(sname, "s.%s", reg_name);
	mk_nmg(fpout, sname,  m);		/* Make nmg object. */
	sprintf(rname, "r.%s", reg_name);
	mk_comb1(fpout, rname, sname, 1);	/* Put object in a region. */
	if (grp_name) {
		mk_comb1(fpout, grp_name, rname, 1);	/* Region in group. */
	}
}

/*
 *	A s c i i _ t o _ B r l c a d
 *
 *	Convert an ascii nmg description into a BRL-CAD data base.
 */
ascii_to_brlcad(fpin, fpout, reg_name, grp_name)
FILE	*fpin, *fpout;
char	*reg_name, *grp_name;
{
	struct model	*m;
	struct nmgregion	*r;
	struct rt_tol	tol;
	struct shell	*s;
	vect_t		Ext;

	VSETALL(Ext, 0.);

	m = nmg_mm();		/* Make nmg model. */
	r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
	s = RT_LIST_FIRST(shell, &r->s_hd);
	descr_to_nmg(s, fpin, Ext);	/* Convert ascii description to nmg. */

        /* Copied from proc-db/nmgmodel.c */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.01;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 0.001;
	tol.para = 0.999;
	/* Associate the face geometry. */
	if (nmg_fu_planeeqn(RT_LIST_FIRST(faceuse, &s->fu_hd), &tol) < 0)
		return(-1);

	nmg_region_a(r, &tol);	/* Calculate geometry for region and shell. */
	if (!NEAR_ZERO(MAGNITUDE(Ext), 0.001))
		extrude_nmg_face(RT_LIST_FIRST(faceuse, &s->fu_hd), Ext, &tol);
	create_brlcad_db(fpout, m, reg_name, grp_name);
	nmg_km(m);		/* Destroy the nmg model. */
}

/*
 *	D e s c r _ t o _ N M G
 *
 *	Convert an ascii description of an nmg to an actual nmg.
 *	(This should be done with lex and yacc.)
 */
descr_to_nmg(s, fp, Ext)
struct shell	*s;	/* NMG shell to add loops to. */
FILE		*fp;	/* File pointer for ascii nmg file. */
vect_t		Ext;	/* Extrusion vector. */
{
#define MAXV	1024

	char	token[80];	/* Token read from ascii nmg file. */
	fastf_t	x, y, z;	/* Coordinates of a vertex. */
	int	dir,		/* Direction of face. */
		i,
		lu_verts[MAXV],	/* Vertex names making up loop. */
		n,		/* Number of vertices so far in loop. */
		stat,		/* Set to EOF when finished ascii file. */
		vert_num;	/* Current vertex in ascii file. */
	fastf_t	pts[3*MAXV];	/* Points in current loop. */
	struct faceuse *fu;	/* Face created. */
	struct vertex	*cur_loop[MAXV],/* Vertices in current loop. */
			*verts[MAXV];	/* Vertices in all loops. */

	n = 0;			/* No vertices read in yet. */
	fu = NULL;		/* Face to be created elsewhere. */
	for (i = 0; i < MAXV; i++)
		verts[i] = NULL;
	stat = fscanf(fp, "%s", token);	/* Get 1st token. */
	do {
		switch (token[0]) {
		case 'e':		/* Extrude face. */
			stat = fscanf(fp, "%s", token);
			switch (token[0]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
			case '+':
			case '-':
				/* Get x value of vector. */
				x = atof(token);
				if (fscanf(fp, "%lf%lf", &y, &z) != 2)
					rt_bomb("descr_to_nmg: messed up vector\n");
				VSET(Ext, x, y, z);

				/* Get token for next trip through loop. */
				stat = fscanf(fp, "%s", token);
				break;
			}
			break;
		case 'l':		/* Start new loop. */
			/* Make a loop with vertices previous to this 'l'. */
			if (n) {
				for (i = 0; i < n; i++)
					if (lu_verts[i] >= 0)
						cur_loop[i] = verts[lu_verts[i]];
					else /* Reuse of a vertex. */
						cur_loop[i] = NULL;
				fu = nmg_add_loop_to_face(s, fu, cur_loop, n,
					dir);
				/* Associate geometry with vertices. */
				for (i = 0; i < n; i++) {
					if (lu_verts[i] >= 0 && !verts[lu_verts[i]]) {
						nmg_vertex_gv( cur_loop[i],
							&pts[3*lu_verts[i]]);
						verts[lu_verts[i]] =
							cur_loop[i];
					}
				}
				/* Take care of reused vertices. */
				for (i = 0; i < n; i++)
					if (lu_verts[i] < 0)
						nmg_jv(verts[-lu_verts[i]], cur_loop[i]);
				n = 0;
			}
			stat = fscanf(fp, "%s", token);

			switch (token[0]) {
			case 'h':	/* Is it cw or ccw? */
				if (!strcmp(token, "hole"))
					dir = OT_OPPOSITE;
				else
					rt_bomb("descr_to_nmg: expected \"hole\"\n");
				/* Get token for next trip through loop. */
				stat = fscanf(fp, "%s", token);
				break;

			default:
				dir = OT_SAME;
				break;
			}
			break;

		case 'v':		/* Vertex in current loop. */
			if (token[1] == NULL)
				rt_bomb("descr_to_nmg: vertices must be numbered.\n");
			vert_num = atoi(token+1);
			stat = fscanf(fp, "%s", token);
			switch (token[0]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
			case '+':
			case '-':
				/* Get coordinates of vertex. */
				x = atof(token);
				if (fscanf(fp, "%lf%lf", &y, &z) != 2)
					rt_bomb("descr_to_nmg: messed up vertex\n");
				/* Save vertex with others in current loop. */
				pts[3*vert_num] = x;
				pts[3*vert_num+1] = y;
				pts[3*vert_num+2] = z;
				/* Save vertex number. */
				lu_verts[n] = vert_num;
				if (++n > MAXV)
					rt_bomb("descr_to_nmg: too many points in loop\n");
				/* Get token for next trip through loop. */
				stat = fscanf(fp, "%s", token);
				break;

			default:
				/* Use negative vert number to mark vertex as being reused. */
				lu_verts[n] = -vert_num;
				if (++n > MAXV)
					rt_bomb("descr_to_nmg: too many points in loop\n");
				break;
			}
			break;

		default:
			rt_log("descr_to_nmg: unexpected token \"%s\"\n", token);
			rt_bomb("");
			break;
		}
	} while (stat != EOF);

	/* Make a loop with vertices previous to this 'l'. */
	if (n) {
		for (i = 0; i < n; i++)
			if (lu_verts[i] >= 0)
				cur_loop[i] = verts[lu_verts[i]];
			else /* Reuse of a vertex. */
				cur_loop[i] = NULL;
		fu = nmg_add_loop_to_face(s, fu, cur_loop, n,
			dir);
		/* Associate geometry with vertices. */
		for (i = 0; i < n; i++) {
			if (lu_verts[i] >= 0 && !verts[lu_verts[i]]) {
				nmg_vertex_gv( cur_loop[i],
					&pts[3*lu_verts[i]]);
				verts[lu_verts[i]] =
					cur_loop[i];
			}
		}
		/* Take care of reused vertices. */
		for (i = 0; i < n; i++)
			if (lu_verts[i] < 0)
				nmg_jv(verts[-lu_verts[i]], cur_loop[i]);
		n = 0;
	}
}


/*
 *	FROM HERE DOWN ARE ROUTINES FOR FACE/SURFACE EXTRUSION
 */

/*
 *	E x t r u d e _ N M G _ F a c e
 *
 *	Duplicate a given NMG face, move it by specified vector,
 *	and create a solid bounded by these faces.
 */
extrude_nmg_face(fu, Vec, tol)
struct faceuse	*fu;	/* Face to extrude. */
vect_t		Vec;	/* Magnitude and direction of extrusion. */
struct rt_tol	*tol;	/* NMG tolerances. */
{
	fastf_t		cosang;
	int		cnt, i, j, nfaces;
	struct edgeuse	*eu;
	struct faceuse	*back, *front, *fu2, *nmg_dup_face(), **outfaceuses;
	struct loopuse	*lu, *lu2;
	struct vertex	*vertlist[4], **verts, **verts2;
	plane_t		N;

#define MIKE_TOL 0.0001

	j = 0;

	/* Duplicate face. */
	fu2 = nmg_dup_face(fu, fu->s_p);

	/* Figure out which face to flip. */
	NMG_GET_FU_NORMAL(N, fu);
	cosang = VDOT(Vec, N);
	front = fu;
	back = fu2;
	if (NEAR_ZERO(cosang, MIKE_TOL)) {
		rt_bomb("extrude_nmg_face: extrusion cannot be parallel to face\n");
	} else if (cosang > 0.) {
		flip_nmg_face(back, tol);
		translate_nmg_face(front, Vec, tol);
	} else if (cosang < 0.) {
		flip_nmg_face(back, tol);
		translate_nmg_face(back, Vec, tol);
	}

	lu = (struct loopuse *)((&front->lu_hd)->forw);
	lu2 = (struct loopuse *)((&back->lu_hd)->forw);
	nfaces = verts_in_nmg_face(front);
	outfaceuses = (struct faceuse **)
		rt_malloc((nfaces+2) * sizeof(struct faceuse *), "faces");

	do {
		cnt = verts_in_nmg_loop(lu);
		if (cnt < 3)
			rt_bomb("extrude_nmg_face: need at least 3 points\n");
		verts = (struct vertex **)
			rt_malloc((cnt+1)*sizeof(struct vertex *), "verts");
		verts2 = (struct vertex **)
			rt_malloc((cnt+1)*sizeof(struct vertex *), "verts");

		/* Collect vertex structures from 1st face. */
		i = 0;
		NMG_CK_LOOPUSE(lu);
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				verts[i++] = eu->vu_p->v_p;
			}
		} else
			rt_bomb("extrude_nmg_face: bad loopuse (1)\n");

		/* Collect vertex structures from 2nd face. */
		i = 0;
		NMG_CK_LOOPUSE(lu2);
		if (RT_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu2->down_hd)) {
				verts2[cnt-i-1] = eu->vu_p->v_p;
				i++;
			}
		} else
			rt_bomb("extrude_nmg_face: bad loopuse (2)\n");

		verts[cnt] = verts[0];
		verts2[cnt] = verts2[0];

		for (i = 0; i < cnt; i++) {
			/* Generate connecting faces. */
			vertlist[0] = verts[i];
			vertlist[1] = verts2[i];
			vertlist[2] = verts2[i+1];
			vertlist[3] = verts[i+1];
			outfaceuses[2+i+j] = nmg_cface(fu->s_p, vertlist, 4);
		}
		j += cnt;

		/* Free memory. */
		rt_free((char *)verts, "verts");
		rt_free((char *)verts2, "verts");

		/* On to next loopuse. */
		lu = (struct loopuse *)((struct rt_list *)(lu))->forw;
		lu2 = (struct loopuse *)((struct rt_list *)(lu2))->forw;

	} while (lu != (struct loopuse *)(&fu->lu_hd));

	outfaceuses[0] = fu;
	outfaceuses[1] = fu2;

	/* Associate the face geometry. */
	for (i = 0; i < nfaces+2; i++) {
		if (nmg_fu_planeeqn(outfaceuses[i], tol) < 0)
			return(-1);	/* FAIL */
	}

	/* Glue the edges of different outward pointing face uses together. */
	nmg_gluefaces(outfaceuses, nfaces+2);

	/* Compute geometry for region and shell. */
	nmg_region_a(fu->s_p->r_p, tol);

	/* Free memory. */
	rt_free((char *)outfaceuses, "faces");
}

/*
 *	F l i p _ N M G _ F a c e
 *
 *	Given a pointer to a faceuse, flip the face by reversing the
 *	order of vertex pointers in each loopuse.
 */
flip_nmg_face(fu, tol)
struct faceuse	*fu;
struct rt_tol	*tol;
{
	int		cnt,		/* Number of vertices in face. */
			i;
	struct vertex	**verts;	/* List of verts in face. */
	struct edgeuse	*eu;
	struct loopuse	*lu;
	struct vertex	*v;

	/* Go through each loop and flip it. */
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		cnt = verts_in_nmg_loop(lu);	/* # of vertices in loop. */
		verts = (struct vertex **)
			rt_malloc(cnt * sizeof(struct vertex *), "verts");

		/* Collect vertex structure pointers from current loop. */
		i = 0;
		NMG_CK_LOOPUSE(lu);
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				verts[i++] = eu->vu_p->v_p;
			}
		} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_VERTEXUSE_MAGIC) {
			v = RT_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
			verts[i++] = v;
		} else
			rt_bomb("extrude_nmg_face: loopuse mess up!\n");

		/* Reverse order of vertex structures in current loop. */
		i = 0;
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				eu->vu_p->v_p = verts[cnt-i-1];
				i++;
			}
		} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_VERTEXUSE_MAGIC) {
			RT_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p
				= verts[cnt-i-1];
			i++;
		} else
			rt_bomb("extrude_nmg_face: loopuse mess up!\n");

		rt_free((char *)verts, "verts");
	}

	nmg_fu_planeeqn(fu, tol);
}

