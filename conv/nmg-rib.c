/*	N M G - R I B
 *
 *	Convert a polygonal model from NMG's to RIB format polygons.
 *
 *	Options
 *	h	help
 *
 *  Author -
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
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/* declarations to support use of getopt() system call */
char *options = "ht";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";
int triangulate = 0;

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(s)
char *s;
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [-t] file.g nmg_solid [ nmg_solid ... ]\n",
			progname, options);
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

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;
	
	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 't'	: triangulate = !triangulate; break;
		case '?'	:
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}

	return(optind);
}

static void
lu_to_rib(lu, fu_normal, norms, points)
struct loopuse *lu;
vect_t fu_normal;
struct rt_vls *norms;
struct rt_vls *points;
{
	struct edgeuse *eu;
	struct vertexuse *vu;

	NMG_CK_LOOPUSE(lu);

	if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC) {
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
			rt_vls_printf(points, "%g %g %g  ",
				V3ARGS(eu->vu_p->v_p->vg_p->coord));

			if (eu->vu_p->a.magic_p && *eu->vu_p->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC)
				rt_vls_printf(norms, "%g %g %g  ",
					V3ARGS(eu->vu_p->a.plane_p->N));
			else
				rt_vls_printf(norms, "%g %g %g  ",
					V3ARGS(fu_normal));
		}
	} else if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
		vu = RT_LIST_FIRST(vertexuse,  &lu->down_hd );
		rt_vls_printf(points, "%g %g %g", V3ARGS(vu->v_p->vg_p->coord));
		if (*vu->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC)
			rt_vls_printf(norms, "%g %g %g  ",
				V3ARGS(vu->a.plane_p->N));
		else
			rt_vls_printf(norms, "%g %g %g  ",
				V3ARGS(fu_normal));
	} else {
		rt_bomb("bad child of loopuse\n");
	}
}

void
nmg_to_rib(m)
struct model *m;
{
	struct rt_tol tol;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct rt_vls points;
	struct rt_vls norms;
	vect_t fu_normal;

	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.05;
	tol.dist_sq = 0.0025;
	tol.perp = 0.00001;
	tol.para = 0.99999;


	if (triangulate)
		nmg_triangulate_model(m, &tol);

	rt_vls_init(&norms);
	rt_vls_init(&points);

	for (RT_LIST_FOR(r, nmgregion, &m->r_hd))
	    for (RT_LIST_FOR(s, shell, &r->s_hd))
		for (RT_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		    if (fu->orientation != OT_SAME)
		    	continue;

		    NMG_GET_FU_NORMAL(fu_normal, fu);

		    for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    	rt_vls_strcpy(&norms, "");
		    	rt_vls_strcpy(&points, "");
			lu_to_rib(lu, fu_normal, &norms, &points);
		    	printf("Polygon \"P\" [ %s ] \"N\" [ %s ]\n",
		    		rt_vls_addr(&points), rt_vls_addr(&norms));
		    }
		}
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
	struct rt_db_internal ip;
	struct directory *dp;
	struct db_i *dbip;
	mat_t my_mat;

	/* parse command flags, and make sure there are arguments
	 * left over for processing.
	 */
	if ((arg_index = parse_args(ac, av)) >= ac) usage("No extra args specified\n");


	/* open the database */
	if ((dbip = db_open(av[arg_index], "r")) == DBI_NULL) {
		perror(av[arg_index]);
		exit(-1);
	}

	if (++arg_index >= ac) usage("No NMG specified\n");

	db_scan(dbip, (int (*)())db_diradd, 1);

	/* process each remaining argument */
	for ( ; arg_index < ac ; arg_index++ ) {
		int id;

		if ( ! (dp = db_lookup(dbip, av[arg_index], 1)) ) {
			fprintf(stderr, "%s: db_lookup failed\n", progname);
			exit(-1);
		}
		
		mat_idn( my_mat );
		if ((id=rt_db_get_internal( &ip, dp, dbip, my_mat ))<0) {
			fprintf(stderr, "%s: rt_db_get_internal() failed\n", progname);
			exit(-1);
		}

		if (ip.idb_type != ID_NMG) {
			fprintf(stderr, "%s: solid type (%d) is NOT NMG!\n",
				progname, ip.idb_type);
			exit(-1);
		}
		nmg_to_rib((struct model *)ip.idb_ptr );
	}
}
