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
static const char RCSid[] = "@(#)$Header$ (ARL)";
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
extern int optind, opterr, getopt(int, char *const *, const char *);

char *progname = "(noname)";
int triangulate = 0;

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(char *s)
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [-t] file.g nmg_solid [ nmg_solid ... ]\n",
			progname);
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char **av)
{
	int  c;
	char *strrchr(const char *, int);

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
lu_to_rib(struct loopuse *lu, fastf_t *fu_normal, struct bu_vls *norms, struct bu_vls *points)
{
	struct edgeuse *eu;
	struct vertexuse *vu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC) {
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
			bu_vls_printf(points, "%g %g %g  ",
				V3ARGS(eu->vu_p->v_p->vg_p->coord));

			if (eu->vu_p->a.magic_p && *eu->vu_p->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC)
				bu_vls_printf(norms, "%g %g %g  ",
					V3ARGS(eu->vu_p->a.plane_p->N));
			else
				bu_vls_printf(norms, "%g %g %g  ",
					V3ARGS(fu_normal));
		}
	} else if (BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
		vu = BU_LIST_FIRST(vertexuse,  &lu->down_hd );
		bu_vls_printf(points, "%g %g %g", V3ARGS(vu->v_p->vg_p->coord));
		if (*vu->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC)
			bu_vls_printf(norms, "%g %g %g  ",
				V3ARGS(vu->a.plane_p->N));
		else
			bu_vls_printf(norms, "%g %g %g  ",
				V3ARGS(fu_normal));
	} else {
		rt_bomb("bad child of loopuse\n");
	}
}

void
nmg_to_rib(struct model *m)
{
	struct bn_tol tol;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct bu_vls points;
	struct bu_vls norms;
	vect_t fu_normal;

	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.05;
	tol.dist_sq = 0.0025;
	tol.perp = 0.00001;
	tol.para = 0.99999;


	if (triangulate)
		nmg_triangulate_model(m, &tol);

	bu_vls_init(&norms);
	bu_vls_init(&points);

	for (BU_LIST_FOR(r, nmgregion, &m->r_hd))
	    for (BU_LIST_FOR(s, shell, &r->s_hd))
		for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		    if (fu->orientation != OT_SAME)
		    	continue;

		    NMG_GET_FU_NORMAL(fu_normal, fu);

		    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    	bu_vls_strcpy(&norms, "");
		    	bu_vls_strcpy(&points, "");
			lu_to_rib(lu, fu_normal, &norms, &points);
		    	printf("Polygon \"P\" [ %s ] \"N\" [ %s ]\n",
		    		bu_vls_addr(&points), bu_vls_addr(&norms));
		    }
		}
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char **av)
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

	rt_init_resource( &rt_uniresource, 0, NULL );

	/* open the database */
	if ((dbip = db_open(av[arg_index], "r")) == DBI_NULL) {
		perror(av[arg_index]);
		exit(-1);
	}

	if (++arg_index >= ac) usage("No NMG specified\n");

	db_dirbuild( dbip );

	/* process each remaining argument */
	for ( ; arg_index < ac ; arg_index++ ) {

		if ( ! (dp = db_lookup(dbip, av[arg_index], 1)) ) {
			fprintf(stderr, "%s: db_lookup failed\n", progname);
			exit(-1);
		}
		
		MAT_IDN( my_mat );
		if ((rt_db_get_internal( &ip, dp, dbip, my_mat, &rt_uniresource ))<0) {
			fprintf(stderr, "%s: rt_db_get_internal() failed\n", progname );
			exit(-1);
		}

		if (ip.idb_type != ID_NMG) {
			fprintf(stderr, "%s: solid type (%d) is NOT NMG!\n",
				progname, ip.idb_type);
			exit(-1);
		}
		nmg_to_rib((struct model *)ip.idb_ptr );
	}
	return 0;
}
