/*                      N M G M O D E L . C
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
 */
/** @file nmgmodel.c
 *
 *      build a really hairy nmg solid in a database
 *
 *	Options
 *	h	help
 */
#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

static struct model *m;
static struct nmgregion *r;
static struct shell *s;
static struct faceuse *fu;
static struct faceuse *tc_fu;	/* top center */
static struct faceuse *fl_fu;	/* front left */
static struct faceuse *bl_fu;	/* back left */
static struct faceuse *ul_fu;	/* underside, left */
static struct faceuse *fr_fu;	/* front right */
static struct loopuse *lu;
static struct vertex *vertl[256];
static struct vertex **f_vertl[256];


/* declarations to support use of getopt() system call */
char *options = "h3210";
extern char *optarg;
extern int optind, opterr, getopt(int, char *const *, const char *);

char *progname = "(noname)";
char plotfilename[1024];
char mfilename[1024];

int manifold[4] = { 1, 1, 1, 1 };

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(char *s)
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [ -0123 ] \n%s\"%s\"\n%s\"%s\"\n",
		progname,
		"\tCreate NMG to mged database ",
		mfilename,
		"\tand plot file ",
		plotfilename);
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char **av)
{
	int  c;

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;

	strcpy(plotfilename, progname);
	strcat(plotfilename, ".pl");

	strcpy(mfilename, progname);
	strcat(mfilename, ".g");

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case '3'	: manifold[3] = 0; break;
		case '2'	: manifold[2] = 0; break;
		case '1'	: manifold[1] = 0; break;
		case '0'	: manifold[0] = 0; break;
		case '?'	:
		case 'h'	:
		default		: usage((char *)NULL); break;
		}

	return(optind);
}

void
make_3manifold_bits(struct bn_tol *tol)
{
	plane_t plane;
	struct faceuse *fu_end;

	bzero((char *)vertl, sizeof(vertl));
	bzero((char *)f_vertl, sizeof(f_vertl));

	/* front right */
	f_vertl[0] = &vertl[0];
	f_vertl[1] = &vertl[1];
	f_vertl[2] = &vertl[2];
	f_vertl[3] = &vertl[3];
	fr_fu = fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[0],  100.0, 100.0, 100.0);
	nmg_vertex_g(vertl[1],   50.0, 100.0, 100.0);
	nmg_vertex_g(vertl[2],   50.0,   0.0, 100.0);
	nmg_vertex_g(vertl[3],  100.0,   0.0, 100.0);
	(void)nmg_fu_planeeqn(fu, tol);

	/* make top right */
	f_vertl[0] = &vertl[0];
	f_vertl[1] = &vertl[4];
	f_vertl[2] = &vertl[5];
	f_vertl[3] = &vertl[1];
	fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[4],  100.0, 100.0,   0.0);
	nmg_vertex_g(vertl[5],   50.0, 100.0,   0.0);
	(void)nmg_fu_planeeqn(fu, tol);


	/* back right */
	f_vertl[0] = &vertl[5];
	f_vertl[1] = &vertl[4];
	f_vertl[2] = &vertl[6];
	f_vertl[3] = &vertl[7];
	fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[6],  100.0,   0.0,   0.0);
	nmg_vertex_g(vertl[7],   50.0,   0.0,   0.0);
	(void)nmg_fu_planeeqn(fu, tol);

	/* bottom right */
	f_vertl[0] = &vertl[7];
	f_vertl[1] = &vertl[6];
	f_vertl[2] = &vertl[3];
	f_vertl[3] = &vertl[2];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);

	/* right end */
	f_vertl[0] = &vertl[3];
	f_vertl[1] = &vertl[6];
	f_vertl[2] = &vertl[4];
	f_vertl[3] = &vertl[0];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);


	/* make split top */
	f_vertl[0] = &vertl[1];
	f_vertl[1] = &vertl[5];
	f_vertl[2] = &vertl[8];
	f_vertl[3] = &vertl[9];
	tc_fu = fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[8],   25.0, 100.0,   0.0);
	nmg_vertex_g(vertl[9],   25.0, 100.0, 100.0);
	(void)nmg_fu_planeeqn(fu, tol);

	f_vertl[0] = &vertl[9];
	f_vertl[1] = &vertl[8];
	f_vertl[2] = &vertl[12];
	f_vertl[3] = &vertl[13];
	fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[12],   0.0, 100.0,   0.0);
	nmg_vertex_g(vertl[13],   0.0, 100.0, 100.0);
	(void)nmg_fu_planeeqn(fu, tol);


	/* make split & funky front side
	 * we make the convex face first, make the second (non-convex) portion
	 * after the face normal has been computed.
	 */
	f_vertl[0] = &vertl[14];
	f_vertl[1] = &vertl[18];
	f_vertl[2] = &vertl[15];
	f_vertl[3] = &vertl[16];
	fl_fu = fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[14],   0.0,  25.0, 100.0);
	nmg_vertex_g(vertl[15],  25.0,   0.0, 100.0);
	nmg_vertex_g(vertl[16],  25.0,  25.0, 100.0);
	nmg_vertex_g(vertl[18],   0.0,   0.0, 100.0);
	(void)nmg_fu_planeeqn(fu, tol);

	f_vertl[0] = &vertl[1];
	f_vertl[1] = &vertl[9];
	f_vertl[2] = &vertl[10];
	f_vertl[3] = &vertl[9];
	f_vertl[4] = &vertl[13];
	f_vertl[5] = &vertl[14];
	f_vertl[6] = &vertl[16];
	f_vertl[7] = &vertl[15];
	f_vertl[8] = &vertl[2];
	nmg_jf(fu, nmg_cmface(s, f_vertl, 9));
	nmg_vertex_g(vertl[10],  25.0,  75.0, 100.0);


	/* make split back side */
	f_vertl[0] = &vertl[5];
	f_vertl[1] = &vertl[7];
	f_vertl[2] = &vertl[17];
	f_vertl[3] = &vertl[12];
	f_vertl[4] = &vertl[8];
	f_vertl[5] = &vertl[11];
	f_vertl[6] = &vertl[8];
	bl_fu = fu = nmg_cmface(s, f_vertl, 7);
	nmg_vertex_g(vertl[11],  25.0,  75.0,   0.0);
	nmg_vertex_g(vertl[17],   0.0,   0.0,   0.0);

	/* this face isn't strictly convex, so we have to make the plane
	 * equation the old-fashioned way.
	 */
	bn_mk_plane_3pts(plane,
			vertl[7]->vg_p->coord,
			vertl[17]->vg_p->coord,
			vertl[12]->vg_p->coord,
			tol);
	nmg_face_g(fu, plane);


	/* make funky end */
	f_vertl[0] = &vertl[14];
	f_vertl[1] = &vertl[20];
	f_vertl[2] = &vertl[19];
	f_vertl[3] = &vertl[18];
	fu_end = fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[19],   0.0,   0.0,  75.0);
	nmg_vertex_g(vertl[20],   0.0,  25.0,  75.0);
	(void)nmg_fu_planeeqn(fu, tol);

	f_vertl[0] = &vertl[12];
	f_vertl[1] = &vertl[17];
	f_vertl[2] = &vertl[19];
	f_vertl[3] = &vertl[20];
	f_vertl[4] = &vertl[14];
	f_vertl[5] = &vertl[13];
	nmg_jf(fu, nmg_cmface(s, f_vertl, 6));


	/* make funky bottom */
	f_vertl[0] = &vertl[15];
	f_vertl[1] = &vertl[18];
	f_vertl[2] = &vertl[19];
	f_vertl[3] = &vertl[21];
	ul_fu = fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[21],  25.0,   0.0,  75.0);
	(void)nmg_fu_planeeqn(fu, tol);

	f_vertl[0] = &vertl[7];
	f_vertl[1] = &vertl[2];
	f_vertl[2] = &vertl[15];
	f_vertl[3] = &vertl[21];
	f_vertl[4] = &vertl[19];
	f_vertl[5] = &vertl[17];
	nmg_jf(fu, nmg_cmface(s, f_vertl, 6));


	/* now create the (3manifold) hole through the object */

	/* make the holes in the end face */
	f_vertl[0] = &vertl[29];
	f_vertl[1] = &vertl[28];
	f_vertl[2] = &vertl[27];
	f_vertl[3] = &vertl[26];
	fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[26],  0.0,   60.0, 10.0);
	nmg_vertex_g(vertl[27],  0.0,   60.0, 25.0);
	nmg_vertex_g(vertl[28],  0.0,   40.0, 25.0);
	nmg_vertex_g(vertl[29],  0.0,   40.0, 10.0);

	/* GROSS HACK XXX
	 * we reverse the orientation of the faceuses and loopuses
	 * so that we can make the hole as a face, and transfer the hole
	 * to an existing face
	 */

	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	lu->orientation = OT_OPPOSITE;
	lu->lumate_p->orientation = OT_OPPOSITE;

	fu->orientation = OT_OPPOSITE;
	fu->fumate_p->orientation = OT_SAME;

	nmg_jf(fu_end, fu->fumate_p);


	f_vertl[0] = &vertl[22];
	f_vertl[1] = &vertl[23];
	f_vertl[2] = &vertl[24];
	f_vertl[3] = &vertl[25];
	fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[22],  10.0, 60.0,  0.0);
	nmg_vertex_g(vertl[23],  25.0,  60.0,  0.0);
	nmg_vertex_g(vertl[24],  25.0,  40.0,  0.0);
	nmg_vertex_g(vertl[25],  10.0, 40.0,  0.0);

	/* GROSS HACK XXX
	 * we reverse the orientation of the faceuses and loopuses
	 * so that we can make the hole as a face, and transfer the hole
	 * to an existing face.
	 */

	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	lu->orientation = OT_OPPOSITE;
	lu->lumate_p->orientation = OT_OPPOSITE;

	fu->orientation = OT_OPPOSITE;
	fu->fumate_p->orientation = OT_SAME;

	nmg_jf(bl_fu, fu->fumate_p);


	/* make the top of the hole */
	f_vertl[0] = &vertl[22];
	f_vertl[1] = &vertl[23];
	f_vertl[2] = &vertl[27];
	f_vertl[3] = &vertl[26];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);


	/* make the bottom of the hole */
	f_vertl[0] = &vertl[24];
	f_vertl[1] = &vertl[25];
	f_vertl[2] = &vertl[29];
	f_vertl[3] = &vertl[28];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);


	/* make origin-ward side of the hole */
	f_vertl[0] = &vertl[22];
	f_vertl[1] = &vertl[26];
	f_vertl[2] = &vertl[29];
	f_vertl[3] = &vertl[25];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);

	/* make last side of hole */
	f_vertl[0] = &vertl[23];
	f_vertl[1] = &vertl[24];
	f_vertl[2] = &vertl[28];
	f_vertl[3] = &vertl[27];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);



	/* now make the void in the center of the solid */

	/* void bottom */
	f_vertl[0] = &vertl[41];
	f_vertl[1] = &vertl[40];
	f_vertl[2] = &vertl[39];
	f_vertl[3] = &vertl[38];
	fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[38],  85.0, 40.0, 60.0);
	nmg_vertex_g(vertl[39],  65.0, 40.0, 60.0);
	nmg_vertex_g(vertl[40],  65.0, 40.0, 40.0);
	nmg_vertex_g(vertl[41],  85.0, 40.0, 40.0);
	(void)nmg_fu_planeeqn(fu, tol);


	/* void top */
	f_vertl[0] = &vertl[42];
	f_vertl[1] = &vertl[43];
	f_vertl[2] = &vertl[44];
	f_vertl[3] = &vertl[45];
	fu = nmg_cmface(s, f_vertl, 4);
	nmg_vertex_g(vertl[42],  85.0, 60.0, 40.0);
	nmg_vertex_g(vertl[43],  85.0, 60.0, 60.0);
	nmg_vertex_g(vertl[44],  65.0, 60.0, 60.0);
	nmg_vertex_g(vertl[45],  65.0, 60.0, 40.0);
	(void)nmg_fu_planeeqn(fu, tol);


	/* void front */
	f_vertl[0] = &vertl[44];
	f_vertl[1] = &vertl[43];
	f_vertl[2] = &vertl[38];
	f_vertl[3] = &vertl[39];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);

	/* void back */
	f_vertl[0] = &vertl[42];
	f_vertl[1] = &vertl[45];
	f_vertl[2] = &vertl[40];
	f_vertl[3] = &vertl[41];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);


	/* void left */
	f_vertl[0] = &vertl[45];
	f_vertl[1] = &vertl[44];
	f_vertl[2] = &vertl[39];
	f_vertl[3] = &vertl[40];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);

	/* void right */
	f_vertl[0] = &vertl[42];
	f_vertl[1] = &vertl[41];
	f_vertl[2] = &vertl[38];
	f_vertl[3] = &vertl[43];
	fu = nmg_cmface(s, f_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);


}

void
make_2manifold_bits(struct bn_tol *tol)
{
	struct vertex *f2_vertl[8];


	/* make a non-dangling internal face */
	f2_vertl[0] = vertl[1];
	f2_vertl[1] = vertl[2];
	f2_vertl[2] = vertl[7];
	f2_vertl[3] = vertl[5];
	fu = nmg_cface(s, f2_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);

	/*
	 * we need to make the 2-manifolds share edge topology
	 */
	nmg_mesh_faces(tc_fu, fu, tol);
	nmg_mesh_faces(fl_fu, fu, tol);
	nmg_mesh_faces(bl_fu, fu, tol);
	nmg_mesh_faces(ul_fu, fu, tol);

	/* make a dangling internal face */
	f2_vertl[0] = vertl[9];
	f2_vertl[1] = vertl[10];
	f2_vertl[2] = vertl[11];
	f2_vertl[3] = vertl[8];
	fu = nmg_cface(s, f2_vertl, 4);
	(void)nmg_fu_planeeqn(fu, tol);

	/* make faces share edge topology */
	nmg_mesh_faces(tc_fu, fu, tol);
	nmg_mesh_faces(fl_fu, fu, tol);
	nmg_mesh_faces(bl_fu, fu, tol);


	/* make an exterior, connected dangling face */
	f2_vertl[0] = vertl[0];
	f2_vertl[1] = vertl[3];
	f2_vertl[2] = vertl[31];
	f2_vertl[3] = vertl[30];
	fu = nmg_cface(s, f2_vertl, 4);
	vertl[30] = f2_vertl[3];
	vertl[31] = f2_vertl[2];
	nmg_vertex_g(vertl[30],  150.0, 100.0, 150.0);
	nmg_vertex_g(vertl[31],  150.0,   0.0, 150.0);
	(void)nmg_fu_planeeqn(fu, tol);
	nmg_mesh_faces(fr_fu, fu, tol);



}

void
make_1manifold_bits(struct bn_tol *tol)
{
	struct edgeuse *eu;

	/* make an internal wire */
	(void)nmg_me(vertl[11], vertl[1], s);


	/* make an external wire */
	eu = nmg_me((struct vertex *)NULL, vertl[4], s);
	vertl[46] = eu->vu_p->v_p;
	nmg_vertex_g(vertl[46],  100.0, 150.0, -50.0);

}

void
make_0manifold_bits(struct bn_tol *tol)
{
#if 0
	struct nmgregion *lrp;
#endif
	struct shell *sp;

	/* make a shell of a single vertex in the same region
	 * as all the other stuff
	 */
	sp = nmg_msv(r);
	vertl[48] = sp->vu_p->v_p;
	nmg_vertex_g(vertl[48],  -10.0, 40.0, -10.0);


#if 0
	/* make a region &shell of a single vertex */
	lrp = nmg_mrsv(m);
	sp = BU_LIST_FIRST(shell, &lrp->s_hd);
	vertl[47] = sp->vu_p->v_p;
	nmg_vertex_g(vertl[47],  -10.0, 50.0, -10.0);
#endif
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char **av)
{
	struct bn_tol tol;
	FILE *fdplot;
	struct rt_wdb *fdmodel;

	if (parse_args(ac, av) < ac) usage((char *)NULL);
	if (!manifold[0] && !manifold[1] && !manifold[2] && !manifold[3])
		usage("No manifolds selected\n");


	m = nmg_mm();
	r = nmg_mrsv(m);
	s = BU_LIST_FIRST(shell, &r->s_hd);

	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.01;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 0.001;
	tol.para = 0.999;

	if (manifold[3]) make_3manifold_bits(&tol);
	if (manifold[2]) make_2manifold_bits(&tol);
	if (manifold[1]) make_1manifold_bits(&tol);
	if (manifold[0]) make_0manifold_bits(&tol);

	NMG_CK_MODEL(m);


	/* write the database */
	if ((fdmodel = wdb_fopen(mfilename)) == NULL)
		perror(mfilename);
	else {
		mk_id(fdmodel, "hairy NMG");
		mk_nmg(fdmodel, "s.NMG",  m);

		/* build a database region mentioning the solid */
		mk_comb1( fdmodel, "r.NMG", "s.NMG", 1 );

		wdb_close(fdmodel);
	}


	/* write a plot file */
	if ((fdplot = fopen(plotfilename, "w")) == (FILE *)NULL)
		perror(plotfilename);
	else {
		nmg_pl_m(fdplot, m);
		fclose(fdplot);
	}


	nmg_km(m);	/* destroy the NMG model */

	return(0);
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
