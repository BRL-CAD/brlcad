/*
 *			J A C K - G . C
 *
 *  Program to convert JACK Psurf file into a BRL-CAD NMG object.
 *
 *  Author -
 *	Michael John Markowski
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlist.h"
#include "../librt/debug.h"

struct vlist {
	fastf_t		pt[3*1024];
	struct vertex	*vt[1024];
};

static char	usage[] = "Usage: %s [-r region] [-g group] [jack_db] [brlcad_db]\n";

RT_EXTERN( fastf_t nmg_loop_plane_area, (CONST struct loopuse *lu, plane_t pl ) );

main(argc, argv)
int	argc;
char	*argv[];
{
	char		*base, *bfile, *grp_name, *jfile, *reg_name;
	FILE		*fpin, *fpout;
	int		doti;
	register int	c;

	grp_name = reg_name = NULL;

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "g:r:")) != EOF) {
		switch (c) {
		case 'g':
			grp_name = optarg;
			/* BRL-CAD group to add psurf to. */
			break;
		case 'r':
			/* BRL-CAD region name for psurf. */
			reg_name = optarg;
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
			break;
		}
	}

	/* Get Jack psurf input file name. */
	if (optind >= argc) {
		jfile = "-";
		fpin = stdin;
	} else {
		jfile = argv[optind];
		if ((fpin = fopen(jfile, "r")) == NULL) {
			fprintf(stderr,
				"%s: cannot open %s for reading\n",
				argv[0], jfile);
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
		if ((fpout = fopen(bfile, "r")) == NULL) {
			if ((fpout = fopen(bfile, "w")) == NULL) {
				fprintf(stderr,
					"%s: cannot open %s for writing\n",
					argv[0], bfile);
				exit(1);
			}
		} else {
			fclose(fpout);
			if ((fpout = fopen(bfile, "a")) == NULL) {
				fprintf(stderr,
					"%s: cannot open %s for appending\n",
					argv[0], bfile);
				exit(1);
			}
		}
	}

	/* Output BRL-CAD database header.  No problem if more than one. */
	mk_id(fpout, jfile);

	/* Make default region name if none given. */
	if (!reg_name) {
		/* Ignore leading path info. */
		base = strrchr(argv[1], '/');
		if (!base)
			base = argv[1];
		else
			base++;
		reg_name = malloc(sizeof(base)+1);
		strcpy(reg_name, base);
		/* Ignore .pss extension if it's there. */
		doti = strlen(reg_name) - 4;
		if (doti > 0 && !strcmp(".pss", reg_name+doti))
		reg_name[doti] = '\0';
	}

	jack_to_brlcad(fpin, fpout, reg_name, grp_name, jfile, bfile);
	fclose(fpin);
	fclose(fpout);
}

/*
 *	J A C K _ T O _ B R L C A D
 *
 *	Convert a UPenn Jack data base into a BRL-CAD data base.
 */
jack_to_brlcad(fpin, fpout, reg_name, grp_name, jfile, bfile)
FILE	*fpin, *fpout;
char	*reg_name, *grp_name, *jfile, *bfile;
{
	struct model	*m;

	m = nmg_mm();			/* Make nmg model. */
	psurf_to_nmg(m, fpin, jfile);	/* Convert psurf model to nmg. */
	create_brlcad_db(fpout, m, reg_name, grp_name);	/* Put in db. */
	nmg_km(m);			/* Destroy the nmg model. */
}

/*
 *	R E A D _ P S U R F _ V E R T I C E S
 *
 *	Read in vertices from a psurf file and store them in an
 *	array of nmg vertex structures.
 *
 *	Fix this!  Only allows set max of points and assumes
 *	no errors during reading...
 */
int
read_psurf_vertices(fp, vert)
FILE		*fp;	/* Psurf file pointer. */
struct vlist	*vert;	/* Array of read in vertices. */
{
	fastf_t	x, y, z;
	int	i;

	/* Read vertices. */
	for (i = 0; fscanf(fp, "%lf %lf %lf", &x, &y, &z) == 3; i++) {
		vert->pt[3*i+0] = x * 10.;	/* Convert cm to mm. */
		vert->pt[3*i+1] = y * 10.;
		vert->pt[3*i+2] = z * 10.;
		vert->vt[i] = (struct vertex *)0;
		fscanf(fp, "%*[^\n]");
	}
	fscanf(fp, ";;");
	return(i);
}

/*
 *	R E A D _ P S U R F _ F A C E
 *
 *	Read in the vertexes describing a face of a psurf.
 */
int
read_psurf_face(fp, lst)
FILE	*fp;
int	*lst;
{
	int	i, n;

	for (i = 0; fscanf(fp, "%d", &n) == 1; i++)
		lst[i] = n;
	fscanf(fp, "%*[^\n]");
	return(i);
}

/*
 *	P S U R F _ T O _ N M G
 *
 */
int
psurf_to_nmg(m, fp, jfile)
struct model	*m;	/* Input/output, nmg model. */
FILE		*fp;	/* Input, pointer to psurf data file. */
char		*jfile;	/* Name of Jack data base file. */
{
	int		face, fail, i, lst[256], nf, nv;
	struct faceuse	*outfaceuses[1024];
	struct nmgregion *r;
	struct rt_tol	tol;
	struct shell	*s;
	struct vertex	*vertlist[1024];
	struct vlist	vert;

	/* Copied from proc-db/nmgmodel.c */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.01;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 0.001;
	tol.para = 0.999;

	face = 0;
	r = nmg_mrsv(m);	/* Make region, empty shell, vertex. */
	s = RT_LIST_FIRST(shell, &r->s_hd);

	while (nv = read_psurf_vertices(fp, &vert)) {
		while (nf = read_psurf_face(fp, lst)) {

			/* Make face out of vertices in lst (ccw ordered). */
			for (i = 0; i < nf; i++)
				vertlist[i] = vert.vt[lst[i]-1];
			outfaceuses[face] = nmg_cface(s, vertlist, nf);
			face++;

			/* Save (possibly) newly created vertex structs. */
			for (i = 0; i < nf; i++)
				vert.vt[lst[i]-1] = vertlist[i];
		}
		fscanf(fp, ";;");

		/* Associate the vertex geometry, ccw. */
		for (i = 0; i < nv; i++)
			if (vert.vt[i])
				nmg_vertex_gv(vert.vt[i], &vert.pt[3*i]);
			else
				fprintf(stderr, "%s, vertex %d is unused\n",
					jfile, i+1);
	}

	/* Associate the face geometry. */
	for (i = 0, fail = 0; i < face; i++)
	{
		struct loopuse *lu;
		plane_t pl;

		lu = RT_LIST_FIRST( loopuse , &outfaceuses[i]->lu_hd );
		if( nmg_loop_plane_area( lu , pl ) < 0.0 )
			fail = 1;
		else
			nmg_face_g( outfaceuses[i] , pl );
	}
	if (fail)
		return(-1);

	/* Glue edges of outward pointing face uses together. */
	nmg_gluefaces(outfaceuses, face);

	/* Compute "geometry" for region and shell */
	nmg_region_a(r, &tol);

	return(0);
}

/*
 *	C R E A T E _ B R L C A D _ D B
 *
 *	Write the nmg to a brl-cad style data base.
 */
int
create_brlcad_db(fpout, m, reg_name, grp_name)
FILE		*fpout;
char		*grp_name, *reg_name;
struct model	*m;
{
	char	*rname, *sname;

	rname = malloc(sizeof(reg_name) + 3);	/* Region name. */
	sname = malloc(sizeof(reg_name) + 3);	/* Solid name. */

	sprintf(sname, "s.%s", reg_name);
	mk_nmg(fpout, sname,  m);		/* Make nmg object. */
	sprintf(rname, "r.%s", reg_name);
	mk_comb1(fpout, rname, sname, 1);	/* Put object in a region. */
	if (grp_name) {
		mk_comb1(fpout, grp_name, rname, 1);	/* Region in group. */
	}
	return 0;
}
