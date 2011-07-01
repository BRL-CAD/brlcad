/*                      E U C L I D - G . C
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
 *
 */
/** @file euclid-g.c
 *
 *  Program to convert Euclid file into a BRL-CAD NMG object.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include "bio.h"

/* interface headers */
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"


#define BRLCAD_TITLE_LENGTH	72
#define MAX_FACES_PER_REGION	8192
#define MAX_PTS_PER_FACE	8192

#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define inout(_i, _j)	in_out[_i*shell_count + _j]
#define OUTSIDE		1
#define INSIDE		(-1)
#define OUTER_SHELL	0x00000001
#define	INNER_SHELL	0x00000002
#define INVERT_SHELL	0x00000004

struct vlist {
    fastf_t		pt[3*MAX_PTS_PER_FACE];
    struct vertex	*vt[MAX_PTS_PER_FACE];
};

void euclid_to_brlcad(FILE *fpin, struct rt_wdb *fpout);
int find_vert(struct vlist *vert, int nv, fastf_t x, fastf_t y, fastf_t z);
int store_vert(struct vlist *vert, int *nv, fastf_t x, fastf_t y, fastf_t z);
int read_euclid_face(int *lst, int *ni, FILE *fp, struct vlist *vert, int *nv);
int cvt_euclid_region(FILE *fp, struct rt_wdb *fpdb, int reg_id);


struct bu_ptbl groups[11];

static int polysolids;
static int debug;
static const char usage[] = "Usage: %s [-v] [-i euclid_db] [-o brlcad_db] [-d tolerance] [-p] [-xX lvl]\n\t\t(-p indicates write as polysolids)\n ";
static struct bn_tol  tol;

void
Find_loop_crack(struct shell *s)
{
    struct faceuse *fu;

    for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
    {
	struct loopuse *lu;

	for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
	    struct edgeuse *eu;
	    int found=0;

	    if ( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		continue;

	    for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	    {
		struct edgeuse *eu_next;

		eu_next = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
		if ( eu->vu_p->v_p == eu_next->vu_p->v_p )
		{
		    found = 2;
		    break;
		}
		eu_next = BU_LIST_PNEXT_CIRC( edgeuse, &eu_next->l );

		if ( eu->vu_p->v_p == eu_next->vu_p->v_p )
		{
		    found = 1;
		    break;
		}
	    }

	    if ( !found )
		continue;

	    if ( found == 1 )
		bu_log( "Found a crack:\n" );
	    else if ( found ==2 )
		bu_log( "Found a zero length edge:\n" );
	    nmg_pr_fu_briefly( fu, "" );
	}
    }
}

int
main(int argc, char **argv)
{
    char		*bfile, *efile;
    FILE		*fpin;
    struct rt_wdb	*fpout;
    char		title[BRLCAD_TITLE_LENGTH];	/* BRL-CAD database title */
    int	c;
    int i;

    fpin = stdin;
    efile = NULL;
    bfile = "euclid.g";
    polysolids = 1;
    debug = 0;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;


    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "d:vi:o:nx:X:")) != -1) {
	switch (c) {
	    case 'd':
		tol.dist = atof( bu_optarg );
		tol.dist_sq = tol.dist * tol.dist;
		break;
	    case 'v':
		debug = 1;
		break;
	    case 'i':
		efile = bu_optarg;
		if ((fpin = fopen(efile, "rb")) == NULL)
		{
		    perror( argv[0] );
		    bu_exit(1, "%s: cannot open %s for reading\n",
			    argv[0], efile);
		}
		break;
	    case 'o':
		bfile = bu_optarg;
		break;
	    case 'n':
		polysolids = 0;
		break;
	    case 'x':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
		break;
	    case 'X':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
#endif

    /* Output BRL-CAD database header.  No problem if more than one. */
    if ( efile == NULL )
	snprintf( title, BRLCAD_TITLE_LENGTH, "Conversion from EUCLID (tolerance distance = %gmm)", tol.dist );
    else
    {
	char tol_str[BRLCAD_TITLE_LENGTH];
	size_t title_len, tol_len;

	snprintf( title, BRLCAD_TITLE_LENGTH, "%s", efile );
	title_len = strlen( title );

	snprintf( tol_str, BRLCAD_TITLE_LENGTH, " (tolerance distance = %gmm)", tol.dist );
	tol_len =  strlen( tol_str );

	/* add the tolerance only if it'll completely fit */
	if ( title_len + tol_len < BRLCAD_TITLE_LENGTH ) {
	    bu_strlcat(title, tol_str, BRLCAD_TITLE_LENGTH);
	}
    }

    if ((fpout = wdb_fopen(bfile)) == NULL) {
	perror( argv[0] );
	bu_exit(1, "%s: cannot open %s for writing\n",
		argv[0], bfile);
    }

    mk_id( fpout, title );

    for ( i=0; i<11; i++ )
	bu_ptbl_init( &groups[i], 64, " &groups[i] ");

    euclid_to_brlcad(fpin, fpout);

    fclose(fpin);
    wdb_close(fpout);
    return 0;
}

/*
 *	A d d _ N M G _ t o _ D b
 *
 *	Write the nmg to a BRL-CAD style data base.
 */
static void
add_nmg_to_db(struct rt_wdb *fpout, struct model *m, int reg_id)
{
    char	id[80], *rname, *sname;
    int gift_ident;
    int group_id;
    struct nmgregion *r;
    struct shell *s;
    struct wmember head;

    BU_LIST_INIT( &head.l );

    r = BU_LIST_FIRST( nmgregion, &m->r_hd );
    s = BU_LIST_FIRST( shell, &r->s_hd );

    sprintf(id, "%d", reg_id);
    rname = bu_malloc(sizeof(id) + 3, "rname");	/* Region name. */
    sname = bu_malloc(sizeof(id) + 3, "sname");	/* Solid name. */

    snprintf(sname, 80, "%s.s", id);
    if ( polysolids )
	mk_bot_from_nmg( fpout, sname, s );
    else
    {
	int something_left=1;

	while ( BU_LIST_NOT_HEAD( s, &r->s_hd ) )
	{
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT( shell, &s->l );
	    if ( nmg_simplify_shell( s ) )
	    {
		if ( nmg_ks( s ) )
		{
		    /* we killed it all! */
		    something_left = 0;
		    break;
		}
	    }
	    s = next_s;
	}
	if ( something_left )
	    mk_nmg(fpout, sname,  m);		/* Make nmg object. */
    }

    gift_ident = reg_id % 100000;
    group_id = gift_ident/1000;
    if ( group_id > 10 )
	group_id = 10;

    snprintf(rname, 80, "%s.r", id);

    if ( mk_addmember( sname, &head.l, NULL, WMOP_UNION ) == WMEMBER_NULL )
    {
	bu_exit(1, "add_nmg_to_db: mk_addmember failed for solid %s\n", sname);
    }

    if ( mk_lrcomb( fpout, rname, &head, 1, (char *)NULL, (char *)NULL,
		    (unsigned char *)NULL, gift_ident, 0, 0, 100, 0 ) )
    {
	bu_exit(1, "add_nmg_to_db: mk_rlcomb failed for region %s\n", rname);
    }

    bu_ptbl_ins( &groups[group_id], (long *)rname );

    bu_free( (char *)sname, "euclid-g: solid name" );
}

static void
build_groups(struct rt_wdb *fpout)
{
    int i, j;
    struct wmember head;
    struct wmember head_all;
    char group_name[18];

    BU_LIST_INIT( &head.l );
    BU_LIST_INIT( &head_all.l );

    for ( i=0; i<11; i++ )
    {

	if ( BU_PTBL_END( &groups[i] ) < 1 )
	    continue;

	for ( j=0; j<BU_PTBL_END( &groups[i] ); j++ )
	{
	    char *region_name;

	    region_name = (char *)BU_PTBL_GET( &groups[i], j );
	    if ( mk_addmember( region_name, &head.l, NULL, WMOP_UNION ) == WMEMBER_NULL )
	    {
		bu_exit(1, "build_groups: mk_addmember failed for region %s\n", region_name);
	    }
	}

	if ( i < 10 )
	    sprintf( group_name, "%dxxx_series", i );
	else
	    sprintf( group_name, "ids_over_9999" );

	j = mk_lfcomb( fpout, group_name, &head, 0 )
	    if ( j )
	    {
		bu_exit(1, "build_groups: mk_lcomb failed for group %s\n", group_name);
	    }

	if ( mk_addmember( group_name, &head_all.l, NULL, WMOP_UNION ) == WMEMBER_NULL )
	{
	    bu_exit(1, "build_groups: mk_addmember failed for group %s\n", group_name);
	}
    }

    j = mk_lfcomb( fpout, "all", &head_all, 0 )
	if ( j )
	    bu_exit(1, "build_groups: mk_lcomb failed for group 'all'\n");
}

/*
 *	E u c l i d _ t o _ B r l C a d
 *
 *	Convert a Euclid data base into a BRL-CAD data base.  This might or
 *	might not be correct, but a Euclid data base of faceted objects is
 *	assumed to be an ascii file of records of the following form:
 *
 *		RID FT A? B? NPT
 *		I0 X0 Y0 Z0
 *		I1 X1 Y1 Z1
 *	    	...
 *		Inpt Xnpt Ynpt Znpt
 *		Ip A B C D
 *
 *	where RID is an integer closed region id number,
 *
 *		FT is the facet type with the following values:
 *		   0: simple facet (no holes).
 *		   1: this facet is a hole.
 *		   2: this is a surface facet which will be given holes.
 *
 *		A? B? are unknown variables.
 *
 *		NPT is the number of data point coordinates which will follow.
 *
 *		Ij is a data point index number.
 *
 *		Xi Yi Zi are data point coordinates in mm.
 *
 *		A, B, C, D are the facet's plane equation coefficients and
 *		<A B C> is an outward pointing surface normal.
 */
void
euclid_to_brlcad(FILE *fpin, struct rt_wdb *fpout)
{
    char	str[80] = {0};
    int	reg_id = -1;

    /* skip first string in file (what is it??) */
    if ( fscanf( fpin, "%80s", str ) == EOF )
	bu_exit(1, "Failed on first attempt to read input" );

    /* Id of first region. */
    if (fscanf(fpin, "%d", &reg_id) != 1) {
	bu_exit(1, "euclid_to_brlcad: no region id\n");
    }

    /* Convert each region to an individual nmg. */
    do {
	fprintf( stderr, "Converting region %d...\n", reg_id);
	reg_id = cvt_euclid_region(fpin, fpout, reg_id);
    } while (reg_id != -1);

    /* Build groups based on idents */
    build_groups( fpout );
}


/*
 *	R e a d _ E u c l i d _ R e g i o n
 *
 *	Make a list of indices into global vertex coordinate array.
 *	This list represents the face under construction.
 */
int
cvt_euclid_region(FILE *fp, struct rt_wdb *fpdb, int reg_id)
{
    int	cur_id, face, facet_type, i, lst[MAX_PTS_PER_FACE], np, nv;
    int hole_face = -4200;
    struct faceuse	*outfaceuses[MAX_PTS_PER_FACE];
    struct model	*m;	/* Input/output, nmg model. */
    struct nmgregion *r;
    struct shell	*s;
    struct faceuse	*fu;
    struct vertex	*vertlist[MAX_PTS_PER_FACE];
    struct vlist	vert;

    m = nmg_mm();		/* Make nmg model. */
    r = nmg_mrsv(m);	/* Make region, empty shell, vertex. */
    s = BU_LIST_FIRST(shell, &r->s_hd);

    nv = 0;			/* Initially no vertices for this region. */
    face = 0;		/* No faces either. */
    /* Grab all the faces for one region. */
    do {
	/* Get vertices for a single face. */
	facet_type = read_euclid_face(lst, &np, fp, &vert, &nv);

	if ( np > 2 )
	{

	    /* Make face out of vertices in lst. */
	    for (i = 0; i < np; i++)
		vertlist[i] = vert.vt[lst[i]];

	    switch (facet_type) {
		case 0:	/* Simple facet (no holes). */
		    if ( debug )
		    {
			bu_log( "Making simple face:\n" );
			for ( i=0; i<np; i++ )
			    bu_log( "\t( %g %g %g )\n", V3ARGS( &vert.pt[lst[i]*3] ));
		    }
		    outfaceuses[face] = nmg_cface(s, vertlist, np);
		    face++;
		    break;

		case 1:	/* Facet is a hole. */
		    if ( debug )
		    {
			bu_log( "Making a hole:\n" );
			for ( i=0; i<np; i++ )
			    bu_log( "\t( %g %g %g )\n", V3ARGS( &vert.pt[lst[i]*3] ));
		    }
		    nmg_add_loop_to_face(s, outfaceuses[hole_face],
					 vertlist, np, OT_OPPOSITE);
		    break;

		case 2:	/* Facet will be given at least one hole. */
		    if ( debug )
		    {
			bu_log( "Making face which will get a hole:\n" );
			for ( i=0; i<np; i++ )
			    bu_log( "\t( %g %g %g )\n", V3ARGS( &vert.pt[lst[i]*3] ));
		    }
		    outfaceuses[face] = nmg_cface(s, vertlist, np);
		    hole_face = face;
		    face++;
		    break;

		default:
		    fprintf(stderr, "cvt_euclid_face: in region %d, face %d is an unknown facet type\n", reg_id, face);
		    break;
	    }

	    /* Save (possibly) newly created vertex structs. */
	    for (i = 0; i < np; i++)
		vert.vt[lst[i]] = vertlist[i];
	}

	/* Get next face's region id. */
	if (fscanf(fp, "%d", &cur_id) != 1)
	    cur_id = -1;
    } while (reg_id == cur_id);

    if ( RT_G_DEBUG&DEBUG_MEM_FULL )
	bu_prmem( "After building faces:\n" );

    /* Associate the vertex geometry, ccw. */
    if ( debug )
	bu_log( "Associating vertex geometry:\n" );
    for (i = 0; i < nv; i++)
    {
	if (vert.vt[i])
	{
	    if ( debug )
		bu_log( "\t( %g %g %g )\n", V3ARGS( &vert.pt[3*i] ) );
	    nmg_vertex_gv(vert.vt[i], &vert.pt[3*i]);
	}
    }

    if ( debug )
	bu_log( "Calling nmg_model_vertex_fuse()\n" );
    (void)nmg_model_vertex_fuse( m, &tol );

    /* Break edges on vertices */
    if ( debug )
	bu_log( "Calling nmg_model_break_e_on_v()\n" );
    (void)nmg_model_break_e_on_v( m, &tol );

    /* kill zero length edgeuses */
    if ( nmg_kill_zero_length_edgeuses( m ) )
    {
	nmg_km( m );
	m = (struct model *)NULL;
	return cur_id;
    }

    /* kill cracks */
    s = BU_LIST_FIRST( shell, &r->s_hd );
    if ( nmg_kill_cracks( s ) )
    {
	if ( nmg_ks( s ) )
	{
	    nmg_km( m );
	    m = (struct model *)0;
	}
	s = (struct shell *)0;
    }

    if ( !m )
	return cur_id;

    if ( RT_G_DEBUG&DEBUG_MEM_FULL )
	bu_prmem( "Before assoc face geom:\n" );

    /* Associate the face geometry. */
    if ( debug )
	bu_log( "Associating face geometry:\n" );
    for (i = 0; i < face; i++)
    {
	/* skip faceuses that were killed */
	if ( outfaceuses[i]->l.magic != NMG_FACEUSE_MAGIC )
	    continue;

	/* calculate plane for this faceuse */
	if ( nmg_calc_face_g( outfaceuses[i] ) )
	{
	    bu_log( "nmg_calc_face_g failed\n" );
	    nmg_pr_fu_briefly( outfaceuses[i], "" );
	}
    }

    /* Compute "geometry" for model, region, and shell */
    if ( debug )
	bu_log( "Rebound\n" );
    nmg_rebound( m, &tol );

    if ( RT_G_DEBUG&DEBUG_MEM_FULL )
	bu_prmem( "Before glueing faces:\n" );

    /* Glue faceuses together. */
    if ( debug )
	bu_log( "Glueing faces\n" );
    (void)nmg_model_edge_fuse( m, &tol );

    /* Compute "geometry" for model, region, and shell */
    if ( debug )
	bu_log( "Rebound\n" );
    nmg_rebound( m, &tol );

    /* fix the normals */
    if ( debug )
	bu_log( "Fix normals\n" );
    nmg_fix_normals( s, &tol );

    if ( RT_G_DEBUG&DEBUG_MEM_FULL )
	bu_prmem( "After fixing normals:\n" );

    if ( debug )
	bu_log( "nmg_s_join_touchingloops( %x )\n", s );
    nmg_s_join_touchingloops( s, &tol );

    /* kill cracks */
    s = BU_LIST_FIRST( shell, &r->s_hd );
    if ( nmg_kill_cracks( s ) )
    {
	if ( nmg_ks( s ) )
	{
	    nmg_km( m );
	    m = (struct model *)0;
	}
	s = (struct shell *)0;
    }

    if ( !m )
	return cur_id;

    if ( debug )
	bu_log( "nmg_s_split_touchingloops( %x )\n", s );
    nmg_s_split_touchingloops( s, &tol);

    /* kill cracks */
    s = BU_LIST_FIRST( shell, &r->s_hd );
    if ( nmg_kill_cracks( s ) )
    {
	if ( nmg_ks( s ) )
	{
	    nmg_km( m );
	    m = (struct model *)0;
	}
	s = (struct shell *)0;
    }

    if ( !m )
	return cur_id;

    /* verify face plane calculations */
    if ( debug )
    {
	nmg_stash_model_to_file( "before_tri.g", m, "before_tri" );
	bu_log( "Verify plane equations:\n" );
    }

    if ( RT_G_DEBUG&DEBUG_MEM_FULL )
	bu_prmem( "Before nmg_make_faces_within_tol():\n" );

    nmg_make_faces_within_tol( s, &tol );

    if ( RT_G_DEBUG&DEBUG_MEM_FULL )
	bu_prmem( "After nmg_make_faces_within_tol():\n" );

    if ( debug )
    {
	bu_log( "Checking faceuses:\n" );
	for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
	    struct loopuse *lu;
	    struct edgeuse *eu;
	    struct vertexuse *vu;
	    fastf_t dist_to_plane;
	    plane_t pl;

	    NMG_CK_FACEUSE( fu );

	    if ( fu->orientation != OT_SAME )
		continue;

	    NMG_GET_FU_PLANE( pl, fu );

	    if ( debug )
		bu_log( "faceuse x%x ( %g %g %g %g )\n", fu, V4ARGS( pl ) );

	    /* check if all the vertices for this face lie on the plane */
	    for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	    {
		NMG_CK_LOOPUSE( lu );

		if ( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
		{
		    vu = BU_LIST_FIRST( vertexuse, &lu->down_hd );
		    dist_to_plane = DIST_PT_PLANE( vu->v_p->vg_p->coord, pl );
		    if ( dist_to_plane > tol.dist || dist_to_plane < -tol.dist )
			bu_log( "\tvertex x%x ( %g %g %g ) is %g off plane\n",
				vu->v_p, V3ARGS( vu->v_p->vg_p->coord ), dist_to_plane );
		}
		else
		{
		    for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		    {
			NMG_CK_EDGEUSE( eu );
			vu = eu->vu_p;
			dist_to_plane = DIST_PT_PLANE( vu->v_p->vg_p->coord, pl );
			if ( dist_to_plane > tol.dist || dist_to_plane < -tol.dist )
			    bu_log( "\tvertex x%x ( %g %g %g ) is %g off plane\n",
				    vu->v_p, V3ARGS( vu->v_p->vg_p->coord ), dist_to_plane );
		    }
		}
	    }
	}
    }

    if ( debug )
	bu_log( "%d vertices out of tolerance after fixing out of tolerance faces\n", nmg_ck_geometry( m, &tol ) );

    nmg_s_join_touchingloops( s, &tol );
    nmg_s_split_touchingloops( s, &tol);

    if ( debug )
	bu_log( "Writing model to database:\n" );
    add_nmg_to_db( fpdb, m, reg_id );

    nmg_km(m);				/* Safe to kill model now. */

    return cur_id;
}


/*
 * R e a d _ E u c l i d _ F a c e
 *
 * Read in vertices from a Euclid facet file and store them in an
 * array of nmg vertex structures.  Then make a list of indices of
 * these vertices in the vertex array.  This list represents the face
 * under construction.
 *
 * FIXME: Only allows set max of points and assumes no errors during
 * reading...
 */
int
read_euclid_face(int *lst, int *ni, FILE *fp, struct vlist *vert, int *nv)
{
    double	num_points, x, y, z, a, b, c, d;
    int	i, j, k, facet_type;
    int ret;

    /* Description of record. */
    ret = fscanf(fp, "%d %*f %*f %lf", &facet_type, &num_points);
    if (ret < 2) {
	perror("fscanf");
    }

    *ni = (int)num_points;

    if ( debug )
	bu_log( "facet type %d has %d points:\n", facet_type,  *ni );

    /* Read in data points. */
    for (i = 0; i < *ni; i++)
    {
	ret = fscanf(fp, "%*d %lf %lf %lf", &x, &y, &z);
	if (ret < 3) {
	    perror("fscanf");
	}

	if ( debug )
	    bu_log( "\tpoint #%d ( %g %g %g )\n", i+1, x, y, z );

	if ((lst[i] = find_vert(vert, *nv, x, y, z)) == -1)
	{
	    lst[i] = store_vert(vert, nv, x, y, z);
	    if ( debug )
		bu_log( "\t\tStoring vertex ( %g %g %g ) at index %d\n", x, y, z, lst[i] );
	}
	else if ( debug )
	    bu_log( "\t\tFound vertex ( %g %g %g ) at index %d\n", x, y, z, lst[i] );
    }

    /* Read in plane equation. */
    ret = fscanf(fp, "%*d %lf %lf %lf %lf", &a, &b, &c, &d);
    if (ret < 4) {
	perror("fscanf");
    }
    if ( debug )
	bu_log( "plane equation for face is ( %f %f %f %f )\n", a, b, c, d );

    /* Remove duplicate points (FIXME: this should be improved). */
    for (i = 0; i < *ni; i++)
	for (j = i+1; j < *ni; j++)
	    if (lst[i] == lst[j]) {
		int increment;

		if ( debug )
		    bu_log( "\tComparing vertices at indices lst[%d]=%d and lst[%d]=%d\n", i, lst[i], j, lst[j] );

		if ( j == i+1 || (i == 0 && j == (*ni-1))  )
		    increment = 1;
		else if ( j == i+2 )
		{
		    j = i+1;
		    increment = 2;
		}
		else
		{
		    bu_log( "warning: removing distant duplicates\n" );
		    increment = 1;
		}

		for (k = j; k < *ni-increment; k++)
		    lst[k] = lst[k + increment];
		*ni -= increment;
	    }

    return facet_type;
}

/*
 *	F i n d _ V e r t
 *
 *	Try to locate a geometric point in the list of vertices.  If found,
 *	return the index number within the vertex array, otherwise return
 *	a -1.
 */
int
find_vert(struct vlist *vert, int nv, fastf_t x, fastf_t y, fastf_t z)
{
    int	found, i;
    point_t new_pt;

    VSET( new_pt, x, y, z );
    found = 0;
    for (i = 0; i < nv; i++)
    {
	if ( bn_pt3_pt3_equal( &vert->pt[3*i], new_pt, &tol ) )
	{
	    found = 1;
	    break;
	}
    }
    if (!found)
	return -1;
    else
	return i;
}

/*
 *	S t o r e _ V e r t
 *
 *	Store vertex in an array of vertices.
 */
int
store_vert(struct vlist *vert, int *nv, fastf_t x, fastf_t y, fastf_t z)
{
    vert->pt[*nv*3+0] = x;
    vert->pt[*nv*3+1] = y;
    vert->pt[*nv*3+2] = z;
    vert->vt[*nv] = (struct vertex *)NULL;

    ++*nv;

    if (*nv > MAX_PTS_PER_FACE) {
	bu_exit(1, "read_euclid_face: no more vertex room\n");
    }

    return *nv - 1;
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
