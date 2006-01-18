/*                       A S C - N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
 *
 */
/** @file asc-nmg.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

static int ascii_to_brlcad(FILE *fpin, struct rt_wdb *fpout, char *reg_name, char *grp_name);
static void descr_to_nmg(struct shell *s, FILE *fp, fastf_t *Ext);

char		usage[] = "Usage: %s [file]\n";
extern char	*optarg;
extern int	optind;

/*
 *	M a i n
 *
 *	Get ascii input file and output file names.
 */
int
main(int argc, char **argv)
{
	char		*afile, *bfile = "nmg.g";
	FILE		*fpin;
	struct rt_wdb	*fpout;

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
		bfile = "nmg.g";
	} else {
		bfile = argv[optind];
	}
	if ((fpout = wdb_fopen(bfile)) == NULL) {
		fprintf(stderr, "%s: cannot open %s for writing\n",
			argv[0], bfile);
		exit(1);
	}

	ascii_to_brlcad(fpin, fpout, "nmg", NULL);
	fclose(fpin);
	wdb_close(fpout);
	return 0;
}

/*
 *	C r e a t e _ B r l c a d _ D b
 *
 *	Write the nmg to a BRL-CAD style data base.
 */
void
create_brlcad_db(struct rt_wdb *fpout, struct model *m, char *reg_name, char *grp_name)
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
static int
ascii_to_brlcad(FILE *fpin, struct rt_wdb *fpout, char *reg_name, char *grp_name)
{
	struct model	*m;
	struct nmgregion	*r;
	struct bn_tol	tol;
	struct shell	*s;
	vect_t		Ext;
	struct faceuse *fu;
	plane_t		pl;

	VSETALL(Ext, 0.);

	m = nmg_mm();		/* Make nmg model. */
	r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	descr_to_nmg(s, fpin, Ext);	/* Convert ascii description to nmg. */

        /* Copied from proc-db/nmgmodel.c */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.01;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 0.001;
	tol.para = 0.999;

	/* Associate the face geometry. */
	fu = BU_LIST_FIRST( faceuse , &s->fu_hd );
	if (nmg_loop_plane_area(BU_LIST_FIRST(loopuse, &fu->lu_hd), pl) < 0.0)
		return(-1);
	else
		nmg_face_g( fu , pl );

	if (!NEAR_ZERO(MAGNITUDE(Ext), 0.001))
		nmg_extrude_face(BU_LIST_FIRST(faceuse, &s->fu_hd), Ext, &tol);

	nmg_region_a(r, &tol);	/* Calculate geometry for region and shell. */

	nmg_fix_normals( s , &tol ); /* insure that faces have outward pointing normals */

	create_brlcad_db(fpout, m, reg_name, grp_name);

	nmg_km(m);		/* Destroy the nmg model. */

	return( 0 );
}

/*
 *	D e s c r _ t o _ N M G
 *
 *	Convert an ascii description of an nmg to an actual nmg.
 *	(This should be done with lex and yacc.)
 */
static void
descr_to_nmg(struct shell *s, FILE *fp, fastf_t *Ext)
            	   	/* NMG shell to add loops to. */
    		    	/* File pointer for ascii nmg file. */
      		    	/* Extrusion vector. */
{
#define MAXV	1024

	char	token[80];	/* Token read from ascii nmg file. */
	fastf_t	x, y, z;	/* Coordinates of a vertex. */
	int	dir = OT_NONE;	/* Direction of face. */
	int	i,
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
			if (token[1] == '\0')
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
			bu_log("descr_to_nmg: unexpected token \"%s\"\n", token);
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
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
