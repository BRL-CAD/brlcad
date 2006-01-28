/*                         A S C 2 G . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
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
/** @file asc2g.c
 *
 *  This program generates a GED database from an
 *  ASCII GED data file.
 *
 *  Usage:  asc2g file.asc file.g
 *
 *  Authors -
 *  	Charles M Kennedy
 *  	Michael J Muuss
 *	Susanne Muuss, J.D.	 Converted to libwdb, Oct. 1990
 *
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "mater.h"


#define BUFSIZE			(8*1024)	/* input line buffer size */
#define TYPELEN			10
#define NAME_LEN			20

void		identbld(void), polyhbld(void), pipebld(void), particlebld(void);
void		solbld(void), arbnbld(void), clinebld(void), botbld(void), extrbld(void), sktbld(void);
int		combbld(void);
void		membbld(struct bu_list *headp), arsabld(void), arsbbld(void);
void		materbld(void), bsplbld(void), bsurfbld(void), zap_nl(void);
char		*nxt_spc(register char *cp);
void		strsolbld(void), nmgbld(void);

static union record	record;			/* GED database record */
char 		*buf;		/* Record input buffer */
char		name[NAMESIZE + 2];

FILE	*ifp;
struct rt_wdb	*ofp;
static int ars_ncurves=0;
static int ars_ptspercurve=0;
static int ars_curve=0;
static int ars_pt=0;
static char *ars_name;
static fastf_t **ars_curves=NULL;
static char *slave_name = "safe_interp";
static char *db_name = "db";

static char usage[] = "\
Usage: asc2g file.asc file.g\n\
 Convert an ASCII BRL-CAD database to binary form\n\
";

char *aliases[] = {
	"attr",
	"color",
	"put",
	"title",
	"units",
	"find",
	"dbfind",
	"rm",
	(char *)0
};

int
incr_ars_pt(void)
{
	int ret=0;

	ars_pt++;
	if( ars_pt >= ars_ptspercurve )
	{
		ars_curve++;
		ars_pt = 0;
		ret = 1;
	}

	if( ars_curve >= ars_ncurves )
		return( 2 );

	return( ret );
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	char c1[3];
#ifdef _WIN32
	_fmode = _O_BINARY;
#endif

	bu_debug = BU_DEBUG_COREDUMP;

	if( argc != 3 ) {
		bu_log( "%s", usage );
		exit( 1 );
	}

	Tcl_FindExecutable(argv[0]);

	ifp = fopen(argv[1],"r");
	if( !ifp )  perror(argv[1]);

	ofp = wdb_fopen(argv[2]);
	if( !ofp )  perror(argv[2]);
	if (ifp == NULL || ofp == NULL) {
		(void)fprintf(stderr, "asc2g: can't open files.");
		exit(1);
	}

	rt_init_resource( &rt_uniresource, 0, NULL );

	if( fgets( c1, 6, ifp ) == NULL ) {
		bu_bomb( "Unexpected EOF!!!\n" );
	}

	/* new style ascii database */
	if (!strncmp(c1, "title", 5) || !strncmp(c1, "put ", 4) ) {
		Tcl_Interp     *interp;
		Tcl_Interp     *safe_interp;

		/* this is a Tcl script */

#ifdef _WIN32
		fclose(ifp);
#else
		rewind( ifp );
#endif
		BU_LIST_INIT(&rt_g.rtg_headwdb.l);

		interp = Tcl_CreateInterp();
		if (wdb_init_obj(interp, ofp, db_name) != TCL_OK ||
		    wdb_create_cmd(interp, ofp, db_name) != TCL_OK) {
		    bu_bomb( "Failed to initialize wdb_obj!!\n" );
		}

		/* Create the safe interpreter */
		if ((safe_interp = Tcl_CreateSlave(interp, slave_name, 1)) == NULL) {
			bu_log("Failed to create safe interpreter");
			exit(1);
		}

		/* Create aliases */
		{
			int	i;
			int	ac = 1;
			const char	*av[2];

			av[1] = (char *)0;
			for (i = 0; aliases[i] != (char *)0; ++i) {
				av[0] = aliases[i];
				Tcl_CreateAlias(safe_interp, aliases[i], interp, db_name, ac, av);
			}
			/* add "dbfind" separately */
			av[0] = "find";
			Tcl_CreateAlias(safe_interp, "dbfind", interp, db_name, ac, av);
		}

		if( Tcl_EvalFile( safe_interp, argv[1] ) != TCL_OK ) {
			bu_log( "Failed to process input file (%s)!!\n", argv[1] );
			bu_log( "%s\n", Tcl_GetStringResult(safe_interp) );
			exit( 1 );
		}

		exit( 0 );
	} else {
		rewind( ifp );
	}

	/* allocate our input buffer */
	buf = (char *)bu_calloc( sizeof(char), BUFSIZE, "input buffer" );

	/* Read ASCII input file, each record on a line */
	while( ( fgets( buf, BUFSIZE, ifp ) ) != (char *)0 )  {

after_read:
		/* Clear the output record -- vital! */
		(void)bzero( (char *)&record, sizeof(record) );

		/* Check record type */
		switch( buf[0] )  {
		case ID_SOLID:
			solbld();
			continue;

		case ID_COMB:
			if( combbld() > 0 )  goto after_read;
			continue;

		case ID_MEMB:
			bu_log("Warning: unattached Member record, ignored\n");
			continue;

		case ID_ARS_A:
			arsabld();
			continue;

		case ID_ARS_B:
			arsbbld();
			continue;

		case ID_P_HEAD:
			polyhbld();
			continue;

		case ID_P_DATA:
			bu_log("Unattached POLY-solid P_DATA (Q) record, skipping\n");
			continue;

		case ID_IDENT:
			identbld();
			continue;

		case ID_MATERIAL:
			materbld();
			continue;

		case ID_BSOLID:
			bsplbld();
			continue;

		case ID_BSURF:
			bsurfbld();
			continue;

		case DBID_PIPE:
			pipebld();
			continue;

		case DBID_STRSOL:
			strsolbld();
			continue;

		case DBID_NMG:
			nmgbld();
			continue;

		case DBID_PARTICLE:
			particlebld();
			continue;

		case DBID_ARBN:
			arbnbld();
			continue;

		case DBID_CLINE:
			clinebld();
			continue;

		case DBID_BOT:
			botbld();
			continue;

		case DBID_EXTR:
			extrbld();
			continue;

		case DBID_SKETCH:
			sktbld();
			continue;

		default:
			bu_log("asc2g: bad record type '%c' (0%o), skipping\n", buf[0], buf[0]);
			bu_log("%s\n", buf );
			continue;
		}
		memset(buf, 0, sizeof(char) * BUFSIZE);
	}

	bu_free(buf, "input buffer");

	/* Now, at the end of the database, dump out the entire
	 * region-id-based color table.
	 */
	mk_write_color_table( ofp );
	wdb_close(ofp);

	exit(0);
}

/*
 *			S T R S O L B L D
 *
 *  Input format is:
 *	s type name args...\n
 *
 *  Individual processing is needed for each 'type' of solid,
 *  to hand it off to the appropriate LIBWDB routine.
 */
void
strsolbld(void)
{
    char	*type = NULL;
    char	*name = NULL;
    char	*args = NULL;
    struct bu_vls	str;
    char *buf2 = (char *)bu_malloc(sizeof(char) * BUFSIZE, "strsolbld temporary buffer");
    char *bufp = buf2;

    memcpy(buf2, buf, sizeof(char) * BUFSIZE);

    bu_vls_init(&str);

#if defined (HAVE_STRSEP)
    (void)strsep( &buf2, " ");		/* skip stringsolid_id */
    type = strsep( &buf2, " ");
    name = strsep( &buf2, " " );
    args = strsep( &buf2, "\n" );
#else
    (void)strtok( buf, " ");		/* skip stringsolid_id */
    type = strtok( NULL, " ");
    name = strtok( NULL, " " );
    args = strtok( NULL, "\n" );
#endif

    if( strcmp( type, "dsp" ) == 0 )  {
	struct rt_dsp_internal *dsp;

	BU_GETSTRUCT( dsp, rt_dsp_internal );
	bu_vls_init( &dsp->dsp_name );
	bu_vls_strcpy( &str, args );
	if( bu_struct_parse( &str, rt_functab[ID_DSP].ft_parsetab, (char *)dsp ) < 0 )  {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    rt_dsp_ifree( (struct rt_db_internal *)dsp );
	    goto out;
	}
	dsp->magic = RT_DSP_INTERNAL_MAGIC;
	if( wdb_export( ofp, name, (genptr_t)dsp, ID_DSP, mk_conv2mm ) < 0 )  {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'dsp' has already been freed by wdb_export() */
    } else if ( strcmp(type, "ebm") == 0) {
	struct rt_ebm_internal *ebm;

	BU_GETSTRUCT( ebm, rt_ebm_internal );

	MAT_IDN(ebm->mat);

	bu_vls_strcpy( &str, args );
	if (bu_struct_parse( &str, rt_functab[ID_EBM].ft_parsetab, (char *)ebm) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    rt_ebm_ifree( (struct rt_db_internal *)ebm );
	    return;
	}
	ebm->magic = RT_EBM_INTERNAL_MAGIC;
	if (wdb_export(ofp, name, (genptr_t)ebm, ID_EBM, mk_conv2mm) < 0) {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'ebm' has already been freed by wdb_export() */
    } else if ( strcmp(type, "vol") == 0) {
	struct rt_vol_internal *vol;

	BU_GETSTRUCT( vol, rt_vol_internal );
	MAT_IDN(vol->mat);

	bu_vls_strcpy( &str, args );
	if (bu_struct_parse( &str, rt_functab[ID_VOL].ft_parsetab, (char *)vol) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    rt_vol_ifree( (struct rt_db_internal *)vol );
	    return;
	}
	vol->magic = RT_VOL_INTERNAL_MAGIC;
	if (wdb_export(ofp, name, (genptr_t)vol, ID_VOL, mk_conv2mm) < 0) {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'vol' has already been freed by wdb_export() */
    } else {
	bu_log("strsolbld(%s): unable to convert '%s' type solid, skipping\n",
	       name, type);
    }

 out:
    bu_free(bufp, "strsolbld temporary buffer");
    bu_vls_free(&str);
}

#define LSEG 'L'
#define CARC 'A'
#define NURB 'N'

void
sktbld(void)
{
	register char *cp, *ptr;
	int i, j;
	int vert_count, seg_count;
	float fV[3], fu[3], fv[3];
	point_t V;
	vect_t u, v;
	point2d_t *verts;
	char name[NAMESIZE+1];
	struct rt_sketch_internal *skt;
	struct curve *crv;
	struct line_seg *lsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;

	cp = buf;

	cp++;
	cp++;

	(void)sscanf( cp, "%s %f %f %f %f %f %f %f %f %f %d %d",
		name,
		&fV[0], &fV[1], &fV[2],
		&fu[0], &fu[1], &fu[2],
		&fv[0], &fv[1], &fv[2],
		&vert_count, &seg_count );

	VMOVE( V, fV );
	VMOVE( u, fu );
	VMOVE( v, fv );

	verts = (point2d_t *)bu_calloc( vert_count, sizeof( point2d_t ), "verts" );

	if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
	{
		bu_log( "Unexpected EOF while reading sketch (%s) data\n", name );
		exit( -1 );
	}

	verts = (point2d_t *)bu_calloc( vert_count, sizeof( point2d_t ), "verts" );
	cp = buf;
	ptr = strtok( buf, " " );
	if( !ptr )
	{
		bu_log( "ERROR: no vertices for sketch (%s)!!!\n", name );
		exit( 1 );
	}
	for( i=0 ; i<vert_count ; i++ )
	{
		verts[i][0] = atof( ptr );
		ptr = strtok( (char *)NULL, " " );
		if( !ptr )
		{
			bu_log( "ERROR: not enough vertices for sketch (%s)!!!\n", name );
			exit( 1 );
		}
		verts[i][1] = atof( ptr );
		ptr = strtok( (char *)NULL, " " );
		if( !ptr && i < vert_count-1 )
		{
			bu_log( "ERROR: not enough vertices for sketch (%s)!!!\n", name );
			exit( 1 );
		}
	}

	skt = (struct rt_sketch_internal *)bu_calloc( 1, sizeof( struct rt_sketch_internal ), "sketch" );
	skt->magic = RT_SKETCH_INTERNAL_MAGIC;
	VMOVE( skt->V, V );
	VMOVE( skt->u_vec, u );
	VMOVE( skt->v_vec, v );
	skt->vert_count = vert_count;
	skt->verts = verts;
	crv = &skt->skt_curve;
	crv->seg_count = seg_count;

	crv->segments = (genptr_t *)bu_calloc( crv->seg_count, sizeof( genptr_t ), "segments" );
	crv->reverse = (int *)bu_calloc( crv->seg_count, sizeof( int ), "reverse" );
	for( j=0 ; j<crv->seg_count ; j++ )
	{
		double radius;
		int k;

		if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
		{
			bu_log( "Unexpected EOF while reading sketch (%s) data\n", name );
			exit( -1 );
		}

		cp = buf + 2;
		switch( *cp )
		{
			case LSEG:
				lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "line segment" );
				sscanf( cp+1, "%d %d %d", &crv->reverse[j], &lsg->start, &lsg->end );
				lsg->magic = CURVE_LSEG_MAGIC;
				crv->segments[j] = lsg;
				break;
			case CARC:
				csg = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "arc segment" );
				sscanf( cp+1, "%d %d %d %lf %d %d", &crv->reverse[j], &csg->start, &csg->end,
					&radius, &csg->center_is_left, &csg->orientation );
				csg->radius = radius;
				csg->magic = CURVE_CARC_MAGIC;
				crv->segments[j] = csg;
				break;
			case NURB:
				nsg = (struct nurb_seg *)bu_malloc( sizeof( struct nurb_seg ), "nurb segment" );
				sscanf( cp+1, "%d %d %d %d %d", &crv->reverse[j], &nsg->order, &nsg->pt_type,
					&nsg->k.k_size, &nsg->c_size );
				nsg->k.knots = (fastf_t *)bu_calloc( nsg->k.k_size, sizeof( fastf_t ), "knots" );
				nsg->ctl_points = (int *)bu_calloc( nsg->c_size, sizeof( int ), "control points" );
				if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
				{
					bu_log( "Unexpected EOF while reading sketch (%s) data\n", name );
					exit( -1 );
				}
				cp = buf + 3;
				ptr = strtok( cp, " " );
				if( !ptr )
				{
					bu_log( "ERROR: not enough knots for nurb segment in sketch (%s)!!!\n", name );
					exit( 1 );
				}
				for( k=0 ; k<nsg->k.k_size ; k++ )
				{
					nsg->k.knots[k] = atof( ptr );
					ptr = strtok( (char *)NULL, " " );
					if( !ptr && k<nsg->k.k_size-1 )
					{
						bu_log( "ERROR: not enough knots for nurb segment in sketch (%s)!!!\n", name );
						exit( 1 );
					}
				}
				if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
				{
					bu_log( "Unexpected EOF while reading sketch (%s) data\n", name );
					exit( -1 );
				}
				cp = buf + 3;
				ptr = strtok( cp, " " );
				if( !ptr )
				{
					bu_log( "ERROR: not enough control points for nurb segment in sketch (%s)!!!\n", name );
					exit( 1 );
				}
				for( k=0 ; k<nsg->c_size ; k++ )
				{
					nsg->ctl_points[k] = atoi( ptr );
					ptr = strtok( (char *)NULL, " " );
					if( !ptr && k<nsg->c_size-1 )
					{
						bu_log( "ERROR: not enough control points for nurb segment in sketch (%s)!!!\n", name );
						exit( 1 );
					}
				}
				nsg->magic = CURVE_NURB_MAGIC;
				crv->segments[j] = nsg;
				break;
			default:
				bu_log( "Unrecognized segment type (%c) in sketch (%s)!!!\n",
					*cp, name );
				exit( 1 );
		}

	}

	(void)mk_sketch(ofp, name,  skt );
}

void
extrbld(void)
{
	register char *cp;
	char name[NAMESIZE+1];
	char sketch_name[NAMESIZE+1];
	int keypoint;
	float fV[3];
	float fh[3];
	float fu_vec[3], fv_vec[3];
	point_t V;
	vect_t h, u_vec, v_vec;

	cp = buf;

	cp++;

	cp++;
	(void)sscanf( cp, "%s %s %d %f %f %f  %f %f %f %f %f %f %f %f %f",
		name, sketch_name, &keypoint, &fV[0], &fV[1], &fV[2], &fh[0], &fh[1], &fh[2],
		&fu_vec[0], &fu_vec[1], &fu_vec[2], &fv_vec[0], &fv_vec[1], &fv_vec[2] );

	VMOVE( V, fV );
	VMOVE( h, fh );
	VMOVE( u_vec, fu_vec );
	VMOVE( v_vec, fv_vec );
	(void)mk_extrusion( ofp, name, sketch_name, V, h, u_vec, v_vec, keypoint );
}

/*
 *			N M G B L D
 *
 *  For the time being, what we read in from the ascii form is
 *  a hex dump of the on-disk form of NMG.
 *  This is the same between v4 and v5.
 *  Reassemble it in v5 binary form here,
 *  then import it,
 *  then re-export it.
 *  This extra step is necessary because we don't know what version
 *  database the output it, LIBWDB is only interested in writing
 *  in-memory versions.
 */
void
nmgbld(void)
{
	register char *cp;
	int	version;
	char	*name;
	long	granules;
	long	struct_count[26];
	struct bu_external	ext;
	struct rt_db_internal	intern;
	int	j;

	/* First, process the header line */
	cp = strtok( buf, " " );
	/* This is nmg_id, unused here. */
	cp = strtok( NULL, " " );
	version = atoi(cp);
	cp = strtok( NULL, " " );
	name = bu_strdup( cp );
	cp = strtok( NULL, " " );
	granules = atol( cp );

	/* Allocate storage for external v5 form of the body */
	BU_INIT_EXTERNAL(&ext);
	ext.ext_nbytes = SIZEOF_NETWORK_LONG + 26*SIZEOF_NETWORK_LONG + 128 * granules;
	ext.ext_buf = bu_malloc( ext.ext_nbytes, "nmg ext_buf" );
	bu_plong( ext.ext_buf, version );
	BU_ASSERT_LONG( version, ==, 1 );	/* DISK_MODEL_VERSION */

	/* Get next line of input with the 26 counts on it */
	if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )  {
		bu_log( "Unexpected EOF while reading NMG %s data, line 2\n", name );
		exit(-1);
	}

	/* Second, process counts for each kind of structure */
	cp = strtok( buf , " " );
	for( j=0 ; j<26 ; j++ )
	{
		struct_count[j] = atol( cp );
		bu_plong( ((unsigned char *)ext.ext_buf)+
			SIZEOF_NETWORK_LONG*(j+1), struct_count[j] );
		cp = strtok( (char *)NULL , " " );
	}

	/* Remaining lines have 32 bytes per line, in hex */
	/* There are 4 lines to make up one granule */
	cp = ((char *)ext.ext_buf) + (26+1)*SIZEOF_NETWORK_LONG;
	for( j=0; j < granules * 4; j++ )  {
		int k;
		unsigned int cp_i;

		if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
		{
			bu_log( "Unexpected EOF while reading NMG %s data, hex line %d\n", name, j );
			exit( -1 );
		}

		for( k=0 ; k<32 ; k++ )
		{
			sscanf( &buf[k*2] , "%2x" , &cp_i );
			*cp++ = cp_i;
		}
	}

	/* Next, import this disk record into memory */
	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[ID_NMG].ft_import5( &intern, &ext, bn_mat_identity, ofp->dbip, &rt_uniresource, ID_NMG ) < 0 )  {
		bu_log("ft_import5 failed on NMG %s\n", name );
		exit( -1 );
	}
	bu_free_external(&ext);

	/* Now we should have a good NMG in memory */
	nmg_vmodel( (struct model *)intern.idb_ptr );

	/* Finally, squirt it back out through LIBWDB */
	mk_nmg( ofp, name, (struct model *)intern.idb_ptr );
	/* mk_nmg() frees the intern.idp_ptr pointer */
	RT_INIT_DB_INTERNAL(&intern);

	bu_free( name, "name" );
}

/*		S O L B L D
 *
 * This routine parses a solid record and determines which libwdb routine
 * to call to replicate this solid.  Simple primitives are expected.
 */

void
solbld(void)
{
	register char *cp;
	register char *np;
	register int i;

	char	s_type;			/* id for the type of primitive */
	fastf_t	val[24];		/* array of values/parameters for solid */
	point_t	center;			/* center; used by many solids */
	point_t pnts[9];		/* array of points for the arbs */
	point_t	norm;
	vect_t	a, b, c, d, n;		/* various vectors required */
	vect_t	height;			/* height vector for tgc */
	vect_t	breadth;		/* breadth vector for rpc */
	double	dd, rad1, rad2;

	cp = buf;
	cp++;				/* ident */
	cp = nxt_spc( cp );		/* skip the space */
	s_type = atoi(cp);

	cp = nxt_spc( cp );

	np = name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	*np = '\0';

	cp = nxt_spc( cp );
	/* Comgeom solid type */

	for( i = 0; i < 24; i++ )  {
		cp = nxt_spc( cp );
		val[i] = atof( cp );
	}

	/* Switch on the record type to make the solids. */

	switch( s_type ) {

		case GRP:
			VSET(center, val[0], val[1], val[2]);
			VSET(n, val[3], val[4], val[5]);
			(void)mk_grip( ofp, name, center, n, val[6] );
			break;

		case TOR:
			VSET(center, val[0], val[1], val[2]);
			VSET(n, val[3], val[4], val[5]);
			rad1 = MAGNITUDE(&val[6]);
			rad2 = MAGNITUDE(n);
			VUNITIZE(n);

			/* Prevent illegal torii from floating point fuzz */
			if( rad2 > rad1 )  rad2 = rad1;

			mk_tor(ofp, name, center, n, rad1, rad2);
			break;

		case GENTGC:
			VSET(center, val[0], val[1], val[2]);
			VSET(height, val[3], val[4], val[5]);
			VSET(a, val[6], val[7], val[8]);
			VSET(b, val[9], val[10], val[11]);
			VSET(c, val[12], val[13], val[14]);
			VSET(d, val[15], val[16], val[17]);

			mk_tgc(ofp, name, center, height, a, b, c, d);
			break;

		case GENELL:
			VSET(center, val[0], val[1], val[2]);
			VSET(a, val[3], val[4], val[5]);
			VSET(b, val[6], val[7], val[8]);
			VSET(c, val[9], val[10], val[11]);

			mk_ell(ofp, name, center, a, b, c);
			break;

		case GENARB8:
			VSET(pnts[0], val[0], val[1], val[2]);
			VSET(pnts[1], val[3], val[4], val[5]);
			VSET(pnts[2], val[6], val[7], val[8]);
			VSET(pnts[3], val[9], val[10], val[11]);
			VSET(pnts[4], val[12], val[13], val[14]);
			VSET(pnts[5], val[15], val[16], val[17]);
			VSET(pnts[6], val[18], val[19], val[20]);
			VSET(pnts[7], val[21], val[22], val[23]);

			/* Convert from vector notation to absolute points */
			for( i=1; i<8; i++ )  {
				VADD2( pnts[i], pnts[i], pnts[0] );
			}

			mk_arb8(ofp, name, &pnts[0][X]);
			break;

		case HALFSPACE:
			VSET(norm, val[0], val[1], val[2]);
			dd = val[3];

			mk_half(ofp, name, norm, dd);
			break;

		case RPC:
			VSET( center, val[0], val[1], val[2] );
			VSET( height, val[3], val[4], val[5] );
			VSET( breadth, val[6], val[7], val[8] );
			dd = val[9];

			mk_rpc( ofp, name, center, height, breadth, dd );
			break;

		case RHC:
			VSET( center, val[0], val[1], val[2] );
			VSET( height, val[3], val[4], val[5] );
			VSET( breadth, val[6], val[7], val[8] );
			rad1 = val[9];
			dd = val[10];

			mk_rhc( ofp, name, center, height, breadth, rad1, dd );
			break;

		case EPA:
			VSET( center, val[0], val[1], val[2] );
			VSET( height, val[3], val[4], val[5] );
			VSET( a, val[6], val[7], val[8] );
			VUNITIZE( a );
			rad1 = val[9];
			rad2 = val[10];

			mk_epa( ofp, name, center, height, a, rad1, rad2 );
			break;

		case EHY:
			VSET( center, val[0], val[1], val[2] );
			VSET( height, val[3], val[4], val[5] );
			VSET( a, val[6], val[7], val[8] );
			VUNITIZE( a );
			rad1 = val[9];
			rad2 = val[10];
			dd = val[11];

			mk_ehy( ofp, name, center, height, a, rad1, rad2, dd );
			break;

		case ETO:
			VSET( center, val[0], val[1], val[2] );
			VSET( norm, val[3], val[4], val[5] );
			VSET( c, val[6], val[7], val[8] );
			rad1 = val[9];
			rad2 = val[10];

			mk_eto( ofp, name, center, norm, c, rad1, rad2 );
			break;

		default:
			bu_log("asc2g: bad solid %s s_type= %d, skipping\n",
				name, s_type);
	}

}


/*			C O M B B L D
 *
 *  This routine builds combinations.
 *  It does so by processing the "C" combination input line,
 *  (which may be followed by optional material properties lines),
 *  and it then slurps up any following "M" member lines,
 *  building up a linked list of all members.
 *  Reading continues until a non-"M" record is encountered.
 *
 *  Returns -
 *	0	OK
 *	1	OK, another record exists in global input line buffer.
 */
int
combbld(void)
{
	struct bu_list	head;
	register char 	*cp;
	register char 	*np;
	int 		temp_nflag, temp_pflag;

	char		override;
	char		reg_flags;	/* region flag */
	int		is_reg;
	short		regionid;
	short		aircode;
	short		material;	/* GIFT material code */
	short		los;		/* LOS estimate */
	unsigned char	rgb[3];		/* Red, green, blue values */
	char		matname[32];	/* String of material name */
	char		matparm[60];	/* String of material parameters */
	char		inherit;	/* Inheritance property */

	/* Set all flags initially. */
	BU_LIST_INIT( &head );

	override = 0;
	temp_nflag = temp_pflag = 0;	/* indicators for optional fields */

	cp = buf;
	cp++;				/* ID_COMB */
	cp = nxt_spc( cp );		/* skip the space */

	reg_flags = *cp++;		/* Y, N, or new P, F */
	cp = nxt_spc( cp );

	np = name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	*np = '\0';

	cp = nxt_spc( cp );

	regionid = (short)atoi( cp );
	cp = nxt_spc( cp );
	aircode = (short)atoi( cp );
	cp = nxt_spc( cp );
	/* DEPRECTED: number of members expected */
	cp = nxt_spc( cp );
	/* DEPRECATED: Comgeom reference number */
	cp = nxt_spc( cp );
	material = (short)atoi( cp );
	cp = nxt_spc( cp );
	los = (short)atoi( cp );
	cp = nxt_spc( cp );
	override = (char)atoi( cp );
	cp = nxt_spc( cp );

	rgb[0] = (unsigned char)atoi( cp );
	cp = nxt_spc( cp );
	rgb[1] = (unsigned char)atoi( cp );
	cp = nxt_spc( cp );
	rgb[2] = (unsigned char)atoi( cp );
	cp = nxt_spc( cp );

	temp_nflag = atoi( cp );
	cp = nxt_spc( cp );
	temp_pflag = atoi( cp );

	cp = nxt_spc( cp );
	inherit = atoi( cp );

	/* To support FASTGEN, different kinds of regions now exist. */
	switch( reg_flags )  {
	case 'Y':
	case 'R':
		is_reg = DBV4_REGION;
		break;
	case 'P':
		is_reg = DBV4_REGION_FASTGEN_PLATE;
		break;
	case 'V':
		is_reg = DBV4_REGION_FASTGEN_VOLUME;
		break;
	case 'N':
	default:
		is_reg = 0;
	}

	if( temp_nflag )  {
		fgets( buf, BUFSIZE, ifp );
		zap_nl();
		bzero( matname, sizeof(matname) );
		strncpy( matname, buf, sizeof(matname)-1 );
	}
	if( temp_pflag )  {
		fgets( buf, BUFSIZE, ifp );
		zap_nl();
		bzero( matparm, sizeof(matparm) );
		strncpy( matparm, buf, sizeof(matparm)-1 );
	}

	for(;;)  {
		buf[0] = '\0';
		if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
			break;

		if( buf[0] != ID_MEMB )  break;

		/* Process (and accumulate) the members */
		membbld( &head );
	}

	/* Spit them out, all at once.  Use GIFT semantics. */
	if( mk_comb(ofp, name, &head, is_reg,
		temp_nflag ? matname : (char *)0,
		temp_pflag ? matparm : (char *)0,
		override ? (unsigned char *)rgb : (unsigned char *)0,
		regionid, aircode, material, los, inherit, 0, 1) < 0 )  {
			fprintf(stderr,"asc2g: mk_lrcomb fail\n");
			abort();
	}

	if( buf[0] == '\0' )  return(0);
	return(1);
}


/*		M E M B B L D
 *
 *  This routine invokes libwdb to build a member of a combination.
 *  Called only from combbld()
 */
void
membbld(struct bu_list *headp)
{
	register char 	*cp;
	register char 	*np;
	register int 	i;
	char		relation;	/* boolean operation */
	char		inst_name[NAMESIZE+2];
	struct wmember	*memb;

	cp = buf;
	cp++;				/* ident */
	cp = nxt_spc( cp );		/* skip the space */

	relation = *cp++;
	cp = nxt_spc( cp );

	np = inst_name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	*np = '\0';

	cp = nxt_spc( cp );

	memb = mk_addmember( inst_name, headp, NULL, relation );

	for( i = 0; i < 16; i++ )  {
		memb->wm_mat[i] = atof( cp );
		cp = nxt_spc( cp );
	}
}


/*		A R S B L D
 *
 * This routine builds ARS's.
 */

void
arsabld(void)
{
	char *cp;
	char *np;
	int i;

	if( ars_name )
		bu_free( (char *)ars_name, "ars_name" );
	cp = buf;
	cp = nxt_spc( cp );
	cp = nxt_spc( cp );

	np = cp;
	while( *(++cp) != ' ' );
	*cp++ = '\0';
	ars_name = bu_strdup( np );
	ars_ncurves = (short)atoi( cp );
	cp = nxt_spc( cp );
	ars_ptspercurve = (short)atoi( cp );

	ars_curves = (fastf_t **)bu_calloc( (ars_ncurves+1), sizeof(fastf_t *), "ars_curves" );
	for( i=0 ; i<ars_ncurves ; i++ )
	{
		ars_curves[i] = (fastf_t *)bu_calloc( ars_ptspercurve + 1,
			sizeof( fastf_t ) * ELEMENTS_PER_VECT, "ars_curve" );
	}

	ars_pt = 0;
	ars_curve = 0;
}

/*		A R S B L D
 *
 * This is the second half of the ARS-building.  It builds the ARS B record.
 */

void
arsbbld(void)
{
	char *cp;
	int i;
	int incr_ret;

	cp = buf;
	cp = nxt_spc( cp );		/* skip the space */
	cp = nxt_spc( cp );
	cp = nxt_spc( cp );
	for( i = 0; i < 8; i++ )  {
		cp = nxt_spc( cp );
		ars_curves[ars_curve][ars_pt*3] = atof( cp );
		cp = nxt_spc( cp );
		ars_curves[ars_curve][ars_pt*3 + 1] = atof( cp );
		cp = nxt_spc( cp );
		ars_curves[ars_curve][ars_pt*3 + 2] = atof( cp );
		if( ars_curve > 0 || ars_pt > 0 )
			VADD2( &ars_curves[ars_curve][ars_pt*3], &ars_curves[ars_curve][ars_pt*3], &ars_curves[0][0] )

		incr_ret = incr_ars_pt();
		if( incr_ret == 2 )
		{
			/* finished, write out the ARS solid */
			if( mk_ars( ofp, ars_name, ars_ncurves, ars_ptspercurve, ars_curves ) )
			{
				bu_log( "Failed trying to make ARS (%s)\n", ars_name );
				bu_bomb( "Failed trying to make ARS\n" );
			}
			return;
		}
		else if( incr_ret == 1 )
		{
			/* end of curve, ignore remainder of reocrd */
			return;
		}
	}
}


/*		Z A P _ N L
 *
 * This routine removes newline characters from the buffer and substitutes
 * in NULL.
 */

void
zap_nl(void)
{
	register char *bp;

	bp = &buf[0];

	while( *bp != '\0' )  {
		if( *bp == '\n' )
			*bp = '\0';
		bp++;
	}
}


/*		I D E N T B L D
 *
 * This routine makes an ident record.  It calls libwdb to do this.
 */

void
identbld(void)
{
	register char	*cp;
	register char	*np;
	char		units;		/* units code number */
	char		version[6] = {0};
	char		title[255] = {0};
	char		unit_str[8] = {0};
	double		local2mm;

	strncpy(unit_str, "none", 4);

	cp = buf;
	cp++;				/* ident */
	cp = nxt_spc( cp );		/* skip the space */

	units = (char)atoi( cp );
	cp = nxt_spc( cp );

	/* Note that there is no provision for handing libwdb the version.
	 * However, this is automatically provided when needed.
	 */

	np = version;
	while( *cp != '\n' && *cp != '\0' )  {
		*np++ = *cp++;
	}
	*np = '\0';

	if( strcmp( version, ID_VERSION ) != 0 )  {
		bu_log("WARNING:  input file version (%s) is not %s\n",
			version, ID_VERSION);
	}

	(void)fgets( buf, BUFSIZE, ifp);
	zap_nl();
	(void)strncpy( title, buf, sizeof(title)-1 );

/* XXX Should use db_conversions() for this */
	switch(units)  {
	case ID_NO_UNIT:
	  strncpy(unit_str,"mm",4);
	  break;
	case ID_MM_UNIT:
	  strncpy(unit_str,"mm",4);
	  break;
	case ID_UM_UNIT:
	  strncpy(unit_str,"um",4);
	  break;
	case ID_CM_UNIT:
	  strncpy(unit_str,"cm",4);
	  break;
	case ID_M_UNIT:
	  strncpy(unit_str,"m",4);
	  break;
	case ID_KM_UNIT:
	  strncpy(unit_str,"km",4);
	  break;
	case ID_IN_UNIT:
	  strncpy(unit_str,"in",4);
	  break;
	case ID_FT_UNIT:
	  strncpy(unit_str,"ft",4);
	  break;
	case ID_YD_UNIT:
	  strncpy(unit_str,"yard",4);
	  break;
	case ID_MI_UNIT:
	  strncpy(unit_str,"mile",4);
	  break;
	default:
	  fprintf(stderr,"asc2g: unknown v4 units code = %d\n", units);
	  exit(1);
	}
	local2mm = bu_units_conversion(unit_str);
	if( local2mm <= 0 )  {
		fprintf(stderr, "asc2g: unable to convert v4 units string '%s', got local2mm=%g\n",
			unit_str, local2mm);
		exit(3);
	}

	if( mk_id_editunits(ofp, title, local2mm) < 0 )  {
		bu_log("asc2g: unable to write database ID\n");
		exit(2);
	}
}


/*		P O L Y H B L D
 *
 *  Collect up all the information for a POLY-solid.
 *  These are handled as BoT solids in v5, but we still have to read
 *  the data in the old format, and then convert it.
 *
 *  The poly header line is followed by an unknown number of
 *  poly data lines.
 */

void
polyhbld(void)
{
	char	*cp;
	char	*name;
	long	startpos;
	long	nlines;
	struct rt_pg_internal	*pg;
	struct rt_db_internal	intern;
	struct bn_tol	tol;

	(void)strtok( buf, " " );	/* skip the ident character */
	cp = strtok( NULL, " \n" );
	name = bu_strdup(cp);

	/* Count up the number of poly data lines which follow */
	startpos = ftell(ifp);
	for( nlines = 0; ; nlines++ )  {
		if( fgets( buf, BUFSIZE, ifp ) == NULL )  break;
		if( buf[0] != ID_P_DATA )  break;	/* 'Q' */
	}
	BU_ASSERT_LONG( nlines, >, 0 );

	/* Allocate storage for the faces */
	BU_GETSTRUCT( pg, rt_pg_internal );
	pg->magic = RT_PG_INTERNAL_MAGIC;
	pg->npoly = nlines;
	pg->poly = (struct rt_pg_face_internal *)bu_calloc( pg->npoly,
		sizeof(struct rt_pg_face_internal), "poly[]" );
	pg->max_npts = 0;

	/* Return to first 'Q' record */
	fseek( ifp, startpos, 0 );

	for( nlines = 0; nlines < pg->npoly; nlines++ )  {
		register struct rt_pg_face_internal	*fp = &pg->poly[nlines];
		register int	i;

		if( fgets( buf, BUFSIZE, ifp ) == NULL )  break;
		if( buf[0] != ID_P_DATA )  bu_bomb("mis-count of Q records?\n");

		/* Input always has 5 points, even if all aren't significant */
		fp->verts = (fastf_t *)bu_malloc( 5*3*sizeof(fastf_t), "verts[]" );
		fp->norms = (fastf_t *)bu_malloc( 5*3*sizeof(fastf_t), "norms[]" );

		cp = buf;
		cp++;				/* ident */
		cp = nxt_spc( cp );		/* skip the space */

		fp->npts = (char)atoi( cp );
		if( fp->npts > pg->max_npts )  pg->max_npts = fp->npts;

		for( i = 0; i < 5*3; i++ )  {
			cp = nxt_spc( cp );
			fp->verts[i] = atof( cp );
		}

		for( i = 0; i < 5*3; i++ )  {
			cp = nxt_spc( cp );
			fp->norms[i] = atof( cp );
		}
	}

	/* Convert the polysolid to a BoT */
	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_POLY;
	intern.idb_meth = &rt_functab[ID_POLY];
	intern.idb_ptr = pg;

	/* this tolerance structure is only used for converting polysolids to BOT's
	 * use zero distance to avoid losing any polysolid facets
	 */
        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.0;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	if( rt_pg_to_bot( &intern, &tol, &rt_uniresource ) < 0 )
		bu_bomb("rt_pg_to_bot() failed\n");
	/* The polysolid is freed by the converter */

	/*
	 * Since we already have an internal form, this is much simpler than
	 * calling mk_bot().
	 */
	if( wdb_put_internal( ofp, name, &intern, mk_conv2mm ) < 0 )
		bu_bomb("wdb_put_internal() failure on BoT from polysolid\n");
	/* BoT internal has been freed */
}

/*		M A T E R B L D
 *
 *  Add information to the region-id based coloring table.
 */

void
materbld(void)
{
	register char *cp;
	int	low, hi;
	int	r,g,b;

	cp = buf;
	cp++;				/* skip ID_MATERIAL */
	cp = nxt_spc( cp );		/* skip the space */

	/* flags = (char)atoi( cp ); */
	cp = nxt_spc( cp );
	low = (short)atoi( cp );
	cp = nxt_spc( cp );
	hi = (short)atoi( cp );
	cp = nxt_spc( cp );
	r = (unsigned char)atoi( cp);
	cp = nxt_spc( cp );
	g = (unsigned char)atoi( cp);
	cp = nxt_spc( cp );
	b = (unsigned char)atoi( cp);

	/* Put it on a linked list for output later */
	rt_color_addrec( low, hi, r, g, b, -1L );
}

/*		B S P L B L D
 *
 *  This routine builds B-splines using libwdb.
 */

void
bsplbld(void)
{
#if 0
	register char	*cp;
	register char	*np;
	short		nsurf;		/* number of surfaces */
	fastf_t		resolution;	/* resolution of flatness */

	cp = buf;
	cp++;				/* ident */
	cp = nxt_spc( cp );		/* skip the space */

	np = name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	*np = '\0';
	cp = nxt_spc( cp );

	nsurf = (short)atoi( cp );
	cp = nxt_spc( cp );
	resolution = atof( cp );

	mk_bsolid(ofp, name, nsurf, resolution);
#else
	bu_bomb("bsplbld() needs to be upgraded to v5\n");
#endif
}

/* 		B S U R F B L D
 *
 * This routine builds d-spline surface descriptions using libwdb.
 */

void
bsurfbld(void)
{
#if 0

/* HELP! This involves mk_bsurf(filep, bp) where bp is a ptr to struct */

	register char	*cp;
	register int	i;
	register float	*vp;
	int		nbytes, count;
	float		*fp;

	cp = buf;
	record.d.d_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.d.d_order[0] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_order[1] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_kv_size[0] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_kv_size[1] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_ctl_size[0] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_ctl_size[1] = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_geom_type = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_nknots = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.d.d_nctls = (short)atoi( cp );

	record.d.d_nknots =
		ngran( record.d.d_kv_size[0] + record.d.d_kv_size[1] );

	record.d.d_nctls =
		ngran( record.d.d_ctl_size[0] * record.d.d_ctl_size[1]
			* record.d.d_geom_type);

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, ofp );

	/*
	 * The b_surf_head record is followed by
	 * d_nknots granules of knot vectors (first u, then v),
	 * and then by d_nctls granules of control mesh information.
	 * Note that neither of these have an ID field!
	 *
	 * B-spline surface record, followed by
	 *	d_kv_size[0] floats,
	 *	d_kv_size[1] floats,
	 *	padded to d_nknots granules, followed by
	 *	ctl_size[0]*ctl_size[1]*geom_type floats,
	 *	padded to d_nctls granules.
	 *
	 * IMPORTANT NOTE: granule == sizeof(union record)
	 */

	/* Malloc and clear memory for the KNOT DATA and read it */
	nbytes = record.d.d_nknots * sizeof(union record);
	vp = (float *)bu_malloc(nbytes, "vp");
	fp = vp;
	(void)bzero( (char *)vp, nbytes );
	/* Read the knot vector information */
	count = record.d.d_kv_size[0] + record.d.d_kv_size[1];
	for( i = 0; i < count; i++ )  {
		fgets( buf, BUFSIZE, ifp );
		(void)sscanf( buf, "%f", vp++);
	}
	/* Write out the information */
	(void)fwrite( (char *)fp, nbytes, 1, ofp );

	/* Free the knot data memory */
	(void)bu_free( (char *)fp, "knot data" );

	/* Malloc and clear memory for the CONTROL MESH data and read it */
	nbytes = record.d.d_nctls * sizeof(union record);
	vp = (float *)bu_malloc(nbytes, "vp");
	fp = vp;
	(void)bzero( (char *)vp, nbytes );
	/* Read the control mesh information */
	count = record.d.d_ctl_size[0] * record.d.d_ctl_size[1] *
		record.d.d_geom_type;
	for( i = 0; i < count; i++ )  {
		fgets( buf, BUFSIZE, ifp );
		(void)sscanf( buf, "%f", vp++);
	}
	/* Write out the information */
	(void)fwrite( (char *)fp, nbytes, 1, ofp );

	/* Free the control mesh memory */
	(void)bu_free( (char *)fp, "mesh data" );
#else
	bu_bomb("bsrfbld() needs to be upgraded to v5\n");
#endif
}

/*		C L I N E B L D
 *
 */
void
clinebld(void)
{
	char			my_name[NAME_LEN];
	fastf_t			thickness;
	fastf_t			radius;
	point_t			V;
	vect_t			height;
	register char		*cp;
	register char		*np;

	cp = buf;
	cp++;
	cp = nxt_spc( cp );

	np = my_name;
	while( *cp != ' ' && *cp != '\n' )
		*np++ = *cp++;
	*np = '\0';

	cp = nxt_spc( cp );

	V[0] = atof( cp );

	cp = nxt_spc( cp );

	V[1] = atof( cp );

	cp = nxt_spc( cp );

	V[2] = atof( cp );

	cp = nxt_spc( cp );

	height[0] = atof( cp );

	cp = nxt_spc( cp );

	height[1] = atof( cp );

	cp = nxt_spc( cp );

	height[2] = atof( cp );

	cp = nxt_spc( cp );

	radius = atof( cp );

	cp = nxt_spc( cp );

	thickness = atof( cp );

	mk_cline( ofp, my_name, V, height, radius, thickness );
}

/*		B O T B L D
 *
 */
void
botbld(void)
{
	char			my_name[NAME_LEN];
	char			type;
	int			mode, orientation, error_mode, num_vertices, num_faces;
	int			i,j;
	double			a[3];
	fastf_t			*vertices;
	fastf_t			*thick=NULL;
	int			*faces;
	struct bu_bitv		*facemode=NULL;

	sscanf( buf, "%c %s %d %d %d %d %d", &type, my_name, &mode, &orientation,
		&error_mode, &num_vertices, &num_faces );

	/* get vertices */
	vertices = (fastf_t *)bu_calloc( num_vertices * 3, sizeof( fastf_t ), "botbld: vertices" );
	for( i=0 ; i<num_vertices ; i++ )
	{
		fgets( buf, BUFSIZE, ifp);
		sscanf( buf, "%d: %le %le %le", &j, &a[0], &a[1], &a[2] );
		if( i != j )
		{
			bu_log( "Vertices out of order in solid %s (expecting %d, found %d)\n",
				my_name, i, j );
			bu_free( (char *)vertices, "botbld: vertices" );
			bu_log( "Skipping this solid!!!\n" );
			while( buf[0] == '\t' )
				fgets( buf, BUFSIZE, ifp);
			return;
		}
		VMOVE( &vertices[i*3], a );
	}

	/* get faces (and possibly thicknesses */
	faces = (int *)bu_calloc( num_faces * 3, sizeof( int ), "botbld: faces" );
	if( mode == RT_BOT_PLATE )
		thick = (fastf_t *)bu_calloc( num_faces, sizeof( fastf_t ), "botbld thick" );
	for( i=0 ; i<num_faces ; i++ )
	{
		fgets( buf, BUFSIZE, ifp);
		if( mode == RT_BOT_PLATE )
			sscanf( buf, "%d: %d %d %d %le", &j, &faces[i*3], &faces[i*3+1], &faces[i*3+2], &a[0] );
		else
			sscanf( buf, "%d: %d %d %d", &j, &faces[i*3], &faces[i*3+1], &faces[i*3+2] );

		if( i != j )
		{
			bu_log( "Faces out of order in solid %s (expecting %d, found %d)\n",
				my_name, i, j );
			bu_free( (char *)vertices, "botbld: vertices" );
			bu_free( (char *)faces, "botbld: faces" );
			if( mode == RT_BOT_PLATE )
				bu_free( (char *)thick, "botbld thick" );
			bu_log( "Skipping this solid!!!\n" );
			while( buf[0] == '\t' )
				fgets( buf, BUFSIZE, ifp);
			return;
		}

		if( mode == RT_BOT_PLATE )
			thick[i] = a[0];
	}

	if( mode == RT_BOT_PLATE )
	{
		/* get bit vector */
		fgets( buf, BUFSIZE, ifp);
		facemode = bu_hex_to_bitv( &buf[1] );
	}

	mk_bot( ofp, my_name, mode, orientation, 0, num_vertices, num_faces,
		vertices, faces, thick, facemode );

	bu_free( (char *)vertices, "botbld: vertices" );
	bu_free( (char *)faces, "botbld: faces" );
	if( mode == RT_BOT_PLATE )
	{
		bu_free( (char *)thick, "botbld thick" );
		bu_free( (char *)facemode, "botbld facemode" );
	}

}

/*		P I P E B L D
 *
 *  This routine reads pipe data from standard in, constructs a doublely
 *  linked list of pipe points, and sends this list to mk_pipe().
 */

void
pipebld(void)
{

	char			name[NAME_LEN];
	register char		*cp;
	register char		*np;
	struct wdb_pipept	*sp;
	struct bu_list		head;

	/* Process the first buffer */

	cp = buf;
	cp++;				/* ident, not used later */
	cp = nxt_spc( cp );		/* skip spaces */

	np = name;
	while( *cp != '\n' )  {
		*np++ = *cp++;
	}
	*np = '\0';			/* null terminate the string */


	/* Read data lines and process */

	BU_LIST_INIT( &head );
	fgets( buf, BUFSIZE, ifp);
	while( strncmp (buf , "END_PIPE", 8 ) )
	{
		double id,od,x,y,z,bendradius;

		sp = (struct wdb_pipept *)bu_malloc(sizeof(struct wdb_pipept), "pipe");

		(void)sscanf( buf, "%le %le %le %le %le %le",
				&id, &od,
				&bendradius, &x, &y, &z );

		sp->l.magic = WDB_PIPESEG_MAGIC;

		sp->pp_id = id;
		sp->pp_od = od;
		sp->pp_bendradius = bendradius;
		VSET( sp->pp_coord, x, y, z );

		BU_LIST_INSERT( &head, &sp->l);
		fgets( buf, BUFSIZE, ifp);
	}

	mk_pipe(ofp, name, &head);
	mk_pipe_free( &head );
}

/*			P A R T I C L E B L D
 *
 * This routine reads particle data from standard in, and constructs the
 * parameters required by mk_particle.
 */

void
particlebld(void)
{

	char		name[NAME_LEN];
	char		ident;
	point_t		vertex;
	vect_t		height;
	double		vrad;
	double		hrad;


	/* Read all the information out of the existing buffer.  Note that
	 * particles fit into one granule.
	 */

	(void)sscanf(buf, "%c %s %le %le %le %le %le %le %le %le",
		&ident, name,
		&vertex[0],
		&vertex[1],
		&vertex[2],
		&height[0],
		&height[1],
		&height[2],
		&vrad, &hrad);

	mk_particle( ofp, name, vertex, height, vrad, hrad);
}


/*			A R B N B L D
 *
 *  This routine reads arbn data from standard in and sendss it to
 *  mk_arbn().
 */

void
arbnbld(void)
{

	char		name[NAME_LEN];
	char		type[TYPELEN];
	int		i;
	int		neqn;			/* number of eqn expected */
	plane_t		*eqn;			/* pointer to plane equations for faces */
	register char	*cp;
	register char	*np;

	/* Process the first buffer */

	cp = buf;
	cp++;					/* ident */
	cp = nxt_spc(cp);			/* skip spaces */

	np = name;
	while( *cp != ' ')  {
		*np++ = *cp++;
	}
	*np = '\0';				/* null terminate the string */

	cp = nxt_spc(cp);

	neqn = atoi(cp);			/* find number of eqns */
/*bu_log("neqn = %d\n", neqn);
 */
	/* Check to make sure plane equations actually came in. */
	if( neqn <= 0 )  {
		bu_log("asc2g: warning: %d equations counted for arbn %s\n", neqn, name);
	}

/*bu_log("mallocing space for eqns\n");
 */
	/* Malloc space for the in-coming plane equations */
	eqn = (plane_t *)bu_malloc( sizeof( plane_t ) * neqn, "eqn" );

	/* Now, read the plane equations and put in appropriate place */

/*bu_log("starting to dump eqns\n");
 */
	for( i = 0; i < neqn; i++ )  {
		fgets( buf, BUFSIZE, ifp);
		(void)sscanf( buf, "%s %le %le %le %le", type,
			&eqn[i][X], &eqn[i][Y], &eqn[i][Z], &eqn[i][3]);
	}

/*bu_log("sending info to mk_arbn\n");
 */
	mk_arbn( ofp, name, neqn, eqn);
}

char *
nxt_spc(register char *cp)
{
	while( *cp != ' ' && *cp != '\t' && *cp !='\0' )  {
		cp++;
	}
	if( *cp != '\0' )  {
		cp++;
	}
	return( cp );
}

int
ngran(int nfloat)
{
	register int gran;
	/* Round up */
	gran = nfloat + ((sizeof(union record)-1) / sizeof(float) );
	gran = (gran * sizeof(float)) / sizeof(union record);
	return(gran);
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
