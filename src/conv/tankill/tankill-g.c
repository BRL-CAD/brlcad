/*                     T A N K I L L - G . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
/** @file tankill-g.c
 *
 * Program to convert the UK TANKILL format to BRL-CAD.
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
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


static int keep_1001=0;		/* flag to indicate that components with id 1001 should not be ignored */
static int verbose=0;		/* verbosity flag */

#define START_ARRAY_SIZE	64
#define ARRAY_BLOCK_SIZE	64

#define	NAMESIZE	MAXPATHLEN /* arbitrary, huge */

struct tankill_verts
{
    point_t coord;
    struct vertex *vp;
};

struct comp_idents		/* structure for linked list of components */
{
    int ident;
    int no_of_solids;
    struct comp_idents *next;
} *id_root;


/*	Adds another solid to the list of solids for each component code number.
 *	Returns the number of solids in this component (including the one just added)
 */
static int
Add_solid(int comp_code_num)
{
    struct comp_idents *ptr;

    /* if list is empty, start one */
    if ( id_root == NULL )
    {
	BU_ALLOC(id_root, struct comp_idents);
	id_root->next = (struct comp_idents *)NULL;
	id_root->ident = comp_code_num;
	id_root->no_of_solids = 1;
	return 1;
    }
    else
    {
	/* look for an entry for this component code number */
	ptr = id_root;
	while ( ptr->next != (struct comp_idents *)NULL && ptr->ident != comp_code_num )
	    ptr = ptr->next;

	/* if found one, just increment the number of solids */
	if ( ptr->ident == comp_code_num )
	{
	    ptr->no_of_solids++;
	    return ptr->no_of_solids;
	}
	else
	{
	    /* make a new entry for this component */
	    BU_ALLOC(ptr->next, struct comp_idents);
	    ptr = ptr->next;
	    ptr->next = NULL;
	    ptr->ident = comp_code_num;
	    ptr->no_of_solids = 1;
	    return 1;
	}
    }
}


struct tankill_args {
    int help;
    int verbose;
    int keep_1001;
    int nmg;
    fastf_t tolerance;
    unsigned int rt_debug;
    unsigned int nmg_debug;
    const char *input_file;
    const char *output_file;
};

static const struct bu_arg_option tankill_options[] = {
    BU_ARG_FLAG("h", "help", struct tankill_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct tankill_args, verbose,
	"Report each component, solid, region, and group"),
    BU_ARG_FLAG("k", "keep-1001", struct tankill_args, keep_1001,
	"Keep normally skipped component code 1001"),
    BU_ARG_FLAG("n", "nmg", struct tankill_args, nmg,
	"Write NMG solids instead of the default BoTs"),
    BU_ARG_VALUE_VALIDATE("t", "tolerance", struct tankill_args, tolerance,
	BU_ARG_VALUE_NUMBER, bu_arg_nonnegative_number_validate, "distance",
	"Set vertex merging distance in millimeters (default 0.0005)"),
    BU_ARG_VALUE("x", "rt-debug", struct tankill_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct tankill_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_VALUE("i", "input", struct tankill_args, input_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "input.tankill",
	"Read TANKILL data from this file instead of standard input"),
    BU_ARG_VALUE("o", "output", struct tankill_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.g",
	"Write the BRL-CAD database to this file (default tankill.g)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema tankill_schema = BU_ARG_SCHEMA(
    "tankill-g", "Convert UK TANKILL triangle strips to BRL-CAD",
    tankill_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar tankill_grammar = {
    &tankill_schema, NULL, NULL
};

static int
tankill_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&tankill_schema, "tankill-g");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "tankill-g help");
    return status;
}


int
main(int argc, char **argv)
{
    struct tankill_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i;
    int introspection;
    int vert1, vert2;
    int vert_no;
    int no_of_verts;
    int comp_code;
    int array_size=START_ARRAY_SIZE;		/* size of "tankill_verts" array */
    int surr_code;	/* not useful */
    float x, y, z;
    struct tankill_verts *verts;
    struct vertex **face_verts[3];
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct bu_ptbl faces;
    struct bn_tol tol;
    struct wmember reg_head;
    struct comp_idents *ptr;
    char name[NAMESIZE+1];
    const char *input_file = NULL;
    const char *output_file;
    FILE *in_fp;					/* input file pointer */
    struct rt_wdb *out_fp;				/* output file pointer */
    int polysolids;					/* flag indicating polysolid output */
    int group_len[100];
    int all_len=0;
    struct bu_list *vlfree = &rt_vlfree;

    bu_setprogname(argv[0]);

    /* Set defaults */

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    args.tolerance = tol.dist;
    args.output_file = "tankill.g";
    introspection = bu_arg_introspect(&msg, &tankill_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (bu_arg_schema_parse_complete(&tankill_schema, &args, &msg,
	    argc - 1, (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return tankill_help(stderr, EXIT_FAILURE);
    }
    bu_vls_free(&msg);
    if (args.help)
	return tankill_help(stdout, EXIT_SUCCESS);

    verbose = args.verbose;
    keep_1001 = args.keep_1001;
    polysolids = !args.nmg;
    tol.dist = args.tolerance;
    tol.dist_sq = tol.dist * tol.dist;
    rt_debug = args.rt_debug;
    nmg_debug = args.nmg_debug;
    input_file = args.input_file;
    output_file = args.output_file;

    if (input_file) {
	in_fp = fopen(input_file, "rb");
	if (!in_fp) {
	    fprintf(stderr, "Cannot open %s\n", input_file);
	    perror("tankill-g");
	    return EXIT_FAILURE;
	}
    } else {
	in_fp = stdin;
	setmode(fileno(in_fp), O_BINARY);
    }
    if (args.rt_debug) {
	bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, RT_DEBUG_FORMAT);
	bu_log("\n");
    }
    if (args.nmg_debug) {
	bu_printb("librt nmg_debug", nmg_debug, NMG_DEBUG_FORMAT);
	bu_log("\n");
    }

    id_root = (struct comp_idents *)NULL;
    bu_ptbl_init( &faces, 64, " &faces ");

    if ( (out_fp = wdb_fopen( output_file )) == NULL )
    {
	perror( output_file );
	fprintf( stderr, "tankill-g: Cannot open %s\n", output_file );
	bu_exit( EXIT_FAILURE,  "Cannot open output file\n" );
    }

    /* use the input file name as the title (if available) */
    if (!input_file)
	mk_id( out_fp, "Conversion from TANKILL" );
    else
	mk_id( out_fp, input_file );

    /* make some space to store the vertices */
    verts = (struct tankill_verts *)bu_malloc( array_size*sizeof( struct tankill_verts ), "tankill-g: verts array " );

    /* read the number of vertices to expect */
    while ( fscanf( in_fp, "%d", &no_of_verts ) != EOF )
    {
	/* make a new shell */
	m = nmg_mm();
	r = nmg_mrsv( m );
	s = BU_LIST_FIRST( shell, &r->s_hd );

	/* make sure there is enough room */
	if ( no_of_verts > array_size )
	{
	    while ( array_size < no_of_verts )
		array_size += ARRAY_BLOCK_SIZE;
	    verts = (struct tankill_verts *)bu_realloc( (char *)verts, array_size*sizeof( struct tankill_verts ), "tankill-g: vertex array" );
	}

	/* read the component code number */
	if ( fscanf( in_fp, "%d", &comp_code ) == EOF )
	{
	    fprintf( stderr, "Unexpected EOF\n" );
	    break;
	}

	if ( verbose )
	    bu_log( "Component code %d (%d vertices)\n", comp_code, no_of_verts );

	/* read the surroundings code (I think this is like the space code in GIFT/FASTGEN) */
	if ( fscanf( in_fp, "%d ", &surr_code ) == EOF )
	{
	    fprintf( stderr, "Unexpected EOF\n" );
	    break;
	}

	/* read the vertices into our structure and set the nmg vertex pointer to NULL */
	for ( vert_no=0; vert_no < no_of_verts; vert_no++ )
	{
	    if ( fscanf( in_fp, "%f %f %f", &x, &y, &z ) == EOF )
	    {
		fprintf( stderr, "Unexpected EOF\n" );
		break;
	    }
	    VSET( verts[vert_no].coord, x, y, z );
	    verts[vert_no].vp = (struct vertex *)NULL;
	}

	/* skip component codes of 1001 (these are not real components) */
	if ( comp_code == 1001 && !keep_1001 )
	{
	    if ( verbose )
		bu_log( "Skipping component code %d (%d vertices)\n", comp_code, no_of_verts );
	    continue;
	}

	/* now start making faces, patch-style */
	vert_no = 0;
	vert1 = 0;
	while ( vert_no < no_of_verts - 2 )
	{
	    /* skip combinations that won't make a face */
	    if ( bg_3pnts_collinear( verts[vert_no].coord, verts[vert_no+1].coord, verts[vert_no+2].coord, &tol ) )
		vert_no++;
	    else if ( !bg_3pnts_distinct( verts[vert_no].coord, verts[vert_no+1].coord, verts[vert_no+2].coord, &tol ) )
		vert_no++;
	    else
	    {
		/* put next three vertices in an array for nmg_cface */
		for ( i=0; i<3; i++ )
		    face_verts[i] = &verts[i+vert_no].vp;

		/* make a face */
		(void)nmg_cmface( s, face_verts, 3 );

		/* make sure any duplicate vertices get the same vertex pointer */
		for (; vert1 < vert_no+3; vert1++ )
		{
		    for ( vert2=vert1+1; vert2 < no_of_verts; vert2++ )
		    {
			if ( verts[vert2].vp )
			    continue;
			if ( VEQUAL( verts[vert1].coord, verts[vert2].coord ) )
			    verts[vert2].vp = verts[vert1].vp;
		    }
		}
		vert_no++;
	    }
	}

	/* assign geometry */
	for ( i=0; i<no_of_verts; i++ )
	{
	    if ( (verts[i].vp != (struct vertex *)NULL) &&
		 (verts[i].vp->vg_p == (struct vertex_g *)NULL) )
		nmg_vertex_gv( verts[i].vp, verts[i].coord );
	}

	/* calculate plane equations for faces */
	for (BU_LIST_FOR(s, shell, &r->s_hd))
	{
	    NMG_CK_SHELL( s );
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd))
	    {
		NMG_CK_FACEUSE( fu );
		if ( fu->orientation == OT_SAME )
		{
		    if ( nmg_calc_face_g( fu, vlfree ) )
			bu_log( "Failed to calculate plane eqn\n" );

		    /* save the face in a table */
		    bu_ptbl_ins( &faces, (long *)fu );
		}
	    }
	}

	/* look for edges that need to be broken apart for sharing
	 *
	 * Like so:
	 *    *------->*-------->*--------->*
	 *    *<----------------------------*
	 *
	 * bottom edge needs to be broken into three to share with
	 * upper edge
	 */

	s = BU_LIST_FIRST( shell, &r->s_hd );
	nmg_break_long_edges( s, &tol );

	/* glue all the faces together */
	nmg_gluefaces( (struct faceuse **)BU_PTBL_BASEADDR( &faces), BU_PTBL_LEN( &faces ), vlfree, &tol );

	/* re-initialize the face list */
	bu_ptbl_reset( &faces );

	/* Calculate bounding boxes */
	nmg_region_a( r, &tol );

	/* fix the normals */
	s = BU_LIST_FIRST( shell, &r->s_hd );

	nmg_fix_normals( s, vlfree, &tol );

	/* make a name for this solid */
	sprintf( name, "s.%d.%d", comp_code, Add_solid( comp_code ) );

	/* write the solid to the brlcad database */
	s = BU_LIST_FIRST( shell, &r->s_hd );
	if ( polysolids )
	{
	    if ( verbose )
		bu_log( "\twriting polysolid %s\n", name );
	    mk_bot_from_nmg( out_fp, name, s );
	}
	else
	{
	    /* simplify the structure as much as possible before writing */
/*			nmg_shell_coplanar_face_merge( s, &tol, 1 );
			if ( nmg_simplify_shell( s ) )
			{
			bu_log( "tankill-g: nmg_simplify_shell emptied %s\n", name );
			nmg_km( m );
			continue;
			}
			else */
	    {
		/* write it out */
		if ( verbose )
		    bu_log( "\twriting polysolid %s\n", name );
		mk_nmg( out_fp, name, m );
	    }
	}

	/* kill the nmg model */
	nmg_km( m );

    }

    /* free the memory for the face list */
    bu_ptbl_free( &faces );

    /* Make regions */
    ptr = id_root;
    while ( ptr != NULL )
    {
	BU_LIST_INIT( &reg_head.l );

	/* make linked list of members */
	for ( i=0; i<ptr->no_of_solids; i++ )
	{
	    /* recreate the name of each solid */
	    sprintf( name, "s.%d.%d", ptr->ident, i+1 );
	    (void)mk_addmember( name, &reg_head.l, NULL, WMOP_UNION );
	}

	/* make the region name */
	sprintf( name, "r.%d", ptr->ident );

	/* make the region */
	if ( verbose )
	{
	    if ( ptr->ident == 1000 )
		bu_log( "Creating air region %s\n", name );
	    else
		bu_log( "Creating region %s\n", name );
	}
	if ( ptr->ident == 1000 )
	{
	    if (mk_lrcomb( out_fp, name, &reg_head, 1, (char *)NULL ,
			   (char *)NULL, (unsigned char *)NULL, 0, 1 ,
			   0, 0, 0 ) )
	    {
		bu_exit( EXIT_FAILURE,  "tankill: Error in freeing region memory" );
	    }
	}
	else
	{
	    if (mk_lrcomb( out_fp, name, &reg_head, 1, (char *)NULL ,
			   (char *)NULL, (unsigned char *)NULL, ptr->ident, 0 ,
			   1, 100, 0 ) )
	    {
		bu_exit( EXIT_FAILURE,  "tankill: Error in freeing region memory" );
	    }
	}
	ptr = ptr->next;
    }

    /* Make groups based on ident numbers */
    for ( i=0; i<100; i++ )
    {
	BU_LIST_INIT( &reg_head.l );

	group_len[i] = 0;
	ptr = id_root;
	while ( ptr != NULL )
	{
	    if ( ptr->ident/100 == i )
	    {
		/* make the region name */
		sprintf( name, "r.%d", ptr->ident );

		(void)mk_addmember( name, &reg_head.l, NULL, WMOP_UNION );
		group_len[i]++;
	    }
	    ptr = ptr->next;
	}

	if ( group_len[i] )
	{
	    /* make a group name */
	    sprintf( name, "%02dXX_codes", i );

	    /* make the group */
	    if ( verbose )
		bu_log( "Creating group %s\n", name );
	    if ( mk_lcomb( out_fp, name, &reg_head, 0,
			   (char *)NULL, (char *)NULL,
			   (unsigned char *)NULL, 0 ) )
	    {
		bu_exit( EXIT_FAILURE,  "tankill: Error in freeing region memory" );
	    }
	}
    }

    /* Make next higher level groups */
    for ( i=0; i<10; i++ )
    {
	int do_group;
	int k;

	BU_LIST_INIT( &reg_head.l );

	do_group = 0;
	for ( k=i*10; k<(i+1)*10; k++ )
	{
	    if ( group_len[k] )
	    {
		do_group = 1;

		/* make group name */
		sprintf( name, "%02dXX_codes", k );
		(void)mk_addmember( name, &reg_head.l, NULL, WMOP_UNION );
	    }
	}
	if ( do_group )
	{
	    /* make the group */
	    sprintf( name, "%dXXX_codes", i );
	    if ( verbose )
		bu_log( "Creating group %s\n", name );
	    if ( mk_lcomb( out_fp, name, &reg_head, 0,
			   (char *)NULL, (char *)NULL, (unsigned char *)0, 0 ) )
	    {
		bu_exit( EXIT_FAILURE,  "tankill: Error in freeing region memory" );
	    }
	}
    }

    /* Make top level group "all" */

    BU_LIST_INIT( &reg_head.l );

    for ( i=0; i<10; i++ )
    {
	int do_group;
	int k;

	do_group = 0;
	for ( k=i*10; k<(i+1)*10; k++ )
	{
	    if ( group_len[k] )
	    {
		do_group = 1;
		break;
	    }
	}
	if ( do_group )
	{
	    /* make the group */
	    sprintf( name, "%dXXX_codes", i );

	    if ( mk_addmember( name, &reg_head.l, NULL, WMOP_UNION ) == WMEMBER_NULL )
		bu_log( "mk_addmember failed for %s\n", name );
	    all_len++;
	}
    }
    if ( all_len )
    {
	if ( verbose )
	    bu_log( "Creating top level group 'all'\n" );
	if ( mk_lcomb( out_fp, "all", &reg_head, 0,
		       (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0 ) )
	    bu_exit( EXIT_FAILURE,  "tankill: Error in freeing region memory" );
    }
    db_close( out_fp->dbip );
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
