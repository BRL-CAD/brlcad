/*
 *		A S C 2 G . C
 *  
 *  This program generates a GED database from an
 *  ASCII GED data file.
 *
 *  Usage:  asc2g < file.asc > file.g
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"


#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "externs.h"


#define BUFSIZE			(8*1024)	/* input line buffer size */
#define TYPELEN			10
#define NAME_LEN			20

void		identbld(), polyhbld(), polydbld(), pipebld(), particlebld();
void		solbld(), arbnbld(), clinebld(), botbld(), extrbld(), sktbld();
int		combbld();
void		membbld(), arsabld(), arsbbld();
void		materbld(), bsplbld(), bsurfbld(), zap_nl();
char		*nxt_spc();
void		strsolbld(), nmgbld();

static union record	record;			/* GED database record */
static char 		buf[BUFSIZE];		/* Record input buffer */
char			name[NAMESIZE + 2];
int 			debug;

FILE	*ifp;
FILE	*ofp;

static char usage[] = "\
Usage: asc2g < file.asc > file.g\n\
   or  asc2g file.asc file.g\n\
 Convert an ASCII BRL-CAD database to binary form\n\
";

main(argc, argv)
int argc;
char **argv;
{
	ifp = stdin;
	ofp = stdout;

#if 0
(void)fprintf(stderr, "About to call bu_log\n");
bu_log("Hello cold cruel world!\n");
(void)fprintf(stderr, "About to begin\n");
#endif

	if( argc == 2 || argc == 4 )
		debug = 1;

	if( argc >= 3 ) {
		ifp = fopen(argv[1],"r");
		if( !ifp )  perror(argv[1]);
		ofp = fopen(argv[2],"w");
		if( !ofp )  perror(argv[2]);
		if (ifp == NULL || ofp == NULL) {
			(void)fprintf(stderr, "asc2g: can't open files.");
			exit(1);
		}
	}
	if (isatty(fileno(ofp))) {
		(void)fprintf(stderr, usage);
		exit(1);
	}

	/* Read ASCII input file, each record on a line */
	while( ( fgets( buf, BUFSIZE, ifp ) ) != (char *)0 )  {

after_read:
		/* Clear the output record -- vital! */
		(void)bzero( (char *)&record, sizeof(record) );

		/* Check record type */
		if( debug )
			bu_log("rec %c\n", buf[0] );
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
			polydbld();
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
	}
	exit(0);
}

/*
 *			S T R S O L B L D
 */
void
strsolbld()
{
	register char *cp;
	register char *np;
	char keyword[10];
	char name[NAME_LEN+1];

	cp = buf;

	if( *cp != DBID_STRSOL )
	{
		bu_log( "asc2g: expecting STRSOL, found '%c' (0%o) (skipping)\n" , buf[0], buf[0] );
		bu_log( "%s\n" , buf );
		return;
	}

	cp = nxt_spc( cp );
	np = keyword;
	while( *cp != ' ' )
		*np++ = *cp++;
	*np = '\0';

	cp = nxt_spc( cp );
	np = name;
	while( *cp != ' ' )
		*np++ = *cp++;
	*np = '\0';

	cp = nxt_spc( cp );

	/* Zap the trailing newline */
	cp[strlen(cp)-1] = '\0';

	if( mk_strsol( ofp, name, keyword, cp ) )  {
		bu_log("asc2g(%s) couldn't convert %s type solid\n",
			name, keyword );
	}
}

#define LSEG 'L'
#define CARC 'A'
#define NURB 'N'

void
sktbld()
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
extrbld()
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

void
nmgbld()
{
	register char *cp;
	int cp_i;
	char *ptr;
	char nmg_id;
	int version;
	char name[NAMESIZE+1];
	long granules,struct_count[26];
	int i;
	long j;

	if( sizeof( union record )%32 )
	{
		bu_log( "asc2g: nmgbld() will only work with union records with size multipe of 32\n" );
		exit( -1 );
	}

	cp = buf;
	sscanf( buf , "%c %d %s %ld" , &nmg_id, &version, name, &granules );

	if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
	{
		bu_log( "Unexpected EOF while reading NMG data\n" );
		exit( -1 );
	}

	ptr = strtok( buf , " " );
	for( i=0 ; i<26 ; i++ )
	{
		struct_count[i] = atof( ptr );
		ptr = strtok( (char *)NULL , " " );
	}

	record.nmg.N_id = nmg_id;
	record.nmg.N_version = version;
	strncpy( record.nmg.N_name , name , NAMESIZE );
	bu_plong( record.nmg.N_count , granules );
	for( i=0 ; i<26 ; i++ )
		bu_plong( &record.nmg.N_structs[i*4] , struct_count[i] );

	/* write out first record */
	(void)fwrite( (char *)&record , sizeof( union record ) , 1 , ofp );

	/* read and write the remaining granules */
	for( j=0 ; j<granules ; j++ )
	{
		cp = (char *)&record;
		for( i=0 ; i<sizeof( union record )/32 ; i++ )
		{
			int k;

			if( fgets( buf, BUFSIZE, ifp ) == (char *)0 )
			{
				bu_log( "Unexpected EOF while reading NMG data\n" );
				exit( -1 );
			}

			for( k=0 ; k<32 ; k++ )
			{
				sscanf( &buf[k*2] , "%2x" , &cp_i );
				*cp++ = cp_i;
			}
		}
		(void)fwrite( (char *)&record , sizeof( union record ) , 1 , ofp );
	}
}

/*		S O L B L D
 *
 * This routine parses a solid record and determines which libwdb routine
 * to call to replicate this solid.  Simple primitives are expected.
 */

void
solbld()
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
combbld()
{
	struct wmember	head;
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
	BU_LIST_INIT( &head.l );

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

	/* Spit them out, all at once */
	if( mk_lrcomb(ofp, name, &head, is_reg,
		temp_nflag ? matname : (char *)0,
		temp_pflag ? matparm : (char *)0,
		override ? (unsigned char *)rgb : (unsigned char *)0,
		regionid, aircode, material, los, inherit) < 0 )  {
			fprintf(stderr,"asc2g: mk_rcomb fail\n");
			exit(1);
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
membbld( headp )
struct wmember	*headp;
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

	memb = mk_addmember( inst_name, headp, relation );

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
arsabld()
{
	register char *cp;
	register char *np;

	cp = buf;
	record.a.a_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.a.a_type = (char)atoi( cp );
	cp = nxt_spc( cp );

	np = record.a.a_name;
	while( *cp != ' ' )  {
		*np++ = *cp++;
	}
	cp = nxt_spc( cp );

	record.a.a_m = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.a.a_n = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.a.a_curlen = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.a.a_totlen = (short)atoi( cp );
	cp = nxt_spc( cp );

	record.a.a_xmax = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_xmin = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_ymax = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_ymin = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_zmax = atof( cp );
	cp = nxt_spc( cp );
	record.a.a_zmin = atof( cp );

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, ofp );

}

/*		A R S B L D
 *
 * This is the second half of the ARS-building.  It builds the ARS B record.
 */

void
arsbbld()
{
	register char *cp;
	register int i;

	cp = buf;
	record.b.b_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.b.b_type = (char)atoi( cp );
	cp = nxt_spc( cp );
	record.b.b_n = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.b.b_ngranule = (short)atoi( cp );

	for( i = 0; i < 24; i++ )  {
		cp = nxt_spc( cp );
		record.b.b_values[i] = atof( cp );
	}

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, ofp );

}


/*		Z A P _ N L
 *
 * This routine removes newline characters from the buffer and substitutes
 * in NULL.
 */

void
zap_nl()
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
identbld()
{
	register char	*cp;
	register char	*np;
	char		units;		/* units code number */
	char		version[6];
	char		title[72];
	char		*unit_str = "none";

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

	switch(units)  {
	case ID_NO_UNIT:
		unit_str = "none";
		break;
	case ID_MM_UNIT:
		unit_str = "mm";
		break;
	case ID_UM_UNIT:
		unit_str = "um";
		break;
	case ID_CM_UNIT:
		unit_str = "cm";
		break;
	case ID_M_UNIT:
		unit_str = "m";
		break;
	case ID_KM_UNIT:
		unit_str = "km";
		break;
	case ID_IN_UNIT:
		unit_str = "in";
		break;
	case ID_FT_UNIT:
		unit_str = "ft";
		break;
	case ID_YD_UNIT:
		unit_str = "yard";
		break;
	case ID_MI_UNIT:
		unit_str = "mile";
		break;
	default:
		fprintf(stderr,"asc2g: unknown units = %d\n", units);
		exit(1);
	}

	if( mk_id_units(ofp, title, unit_str) < 0 )  {
		bu_log("asc2g: unable to write database ID\n");
		exit(2);
	}
}


/*		P O L Y H B L D
 *
 *  This routine builds the record headder for a polysolid.
 */

void
polyhbld()
{

	/* Headder for polysolid */

	register char	*cp;
	register char	*np;

	cp = buf;
	cp++;				/* ident */
	cp = nxt_spc( cp );		/* skip the space */

	np = name;
	while( *cp != '\n' && *cp != '\0' )  {
		*np++ = *cp++;
	}
	*np = '\0';

	mk_polysolid(ofp, name);
}

/*		P O L Y D B L D
 *
 * This routine builds a polydata record using libwdb.
 */

void
polydbld()
{
	register char	*cp;
	register int	i, j;
	char		count;		/* number of vertices */
	fastf_t		verts[5][3];	/* vertices for the polygon */
	fastf_t		norms[5][3];	/* normals at each vertex */

	cp = buf;
	cp++;				/* ident */
	cp = nxt_spc( cp );		/* skip the space */

	count = (char)atoi( cp );

	for( i = 0; i < 5; i++ )  {
		for( j = 0; j < 3; j++ )  {
			cp = nxt_spc( cp );
			verts[i][j] = atof( cp );
		}
	}

	for( i = 0; i < 5; i++ )  {
		for( j = 0; j < 3; j++ )  {
			cp = nxt_spc( cp );
			norms[i][j] = atof( cp );
		}
	}

	mk_poly(ofp, count, verts, norms);
}


/*		M A T E R B L D
 *
 * The need for this is being phased out. Leave alone.
 */

void
materbld()
{

	register char *cp;

	cp = buf;
	record.md.md_id = *cp++;
	cp = nxt_spc( cp );		/* skip the space */

	record.md.md_flags = (char)atoi( cp );
	cp = nxt_spc( cp );
	record.md.md_low = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.md.md_hi = (short)atoi( cp );
	cp = nxt_spc( cp );
	record.md.md_r = (unsigned char)atoi( cp);
	cp = nxt_spc( cp );
	record.md.md_g = (unsigned char)atoi( cp);
	cp = nxt_spc( cp );
	record.md.md_b = (unsigned char)atoi( cp);

	/* Write out the record */
	(void)fwrite( (char *)&record, sizeof record, 1, ofp );
}

/*		B S P L B L D
 *
 *  This routine builds B-splines using libwdb.
 */

void
bsplbld()
{
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
}

/* 		B S U R F B L D
 *
 * This routine builds d-spline surface descriptions using libwdb.
 */

void
bsurfbld()
{

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
	if( (vp = (float *)malloc(nbytes))  == (float *)0 )  {
		bu_log( "asc2g: spline knot malloc error\n");
		exit(1);
	}
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
	(void)free( (char *)fp );

	/* Malloc and clear memory for the CONTROL MESH data and read it */
	nbytes = record.d.d_nctls * sizeof(union record);
	if( (vp = (float *)malloc(nbytes))  == (float *)0 )  {
		bu_log( "asc2g: control mesh malloc error\n");
		exit(1);
	}
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
	(void)free( (char *)fp );
}

/*		C L I N E B L D
 *
 */
void
clinebld()
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
botbld()
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

	mk_bot( ofp, my_name, mode, orientation, error_mode, num_vertices, num_faces,
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
pipebld()
{

	char			name[NAME_LEN];
	register char		*cp;
	register char		*np;
	struct wdb_pipept	*sp;
	struct wdb_pipept	head;

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

	BU_LIST_INIT( &head.l );
	fgets( buf, BUFSIZE, ifp);
	while( strncmp (buf , "END_PIPE", 8 ) )
	{
		double id,od,x,y,z,bendradius;

		if( (sp = (struct wdb_pipept *)malloc(sizeof(struct wdb_pipept)) ) == NULL)
		{
				fprintf(stderr,"asc2g: malloc failure for pipe\n");
				exit(-1);
		}

		(void)sscanf( buf, "%le %le %le %le %le %le",
				&id, &od,
				&bendradius, &x, &y, &z );

		sp->l.magic = WDB_PIPESEG_MAGIC;

		sp->pp_id = id;
		sp->pp_od = od;
		sp->pp_bendradius = bendradius;
		VSET( sp->pp_coord, x, y, z );

		BU_LIST_INSERT( &head.l, &sp->l);
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
particlebld()
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
arbnbld()
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
	if( (eqn = (plane_t *)malloc( sizeof( plane_t ) * neqn ) ) == NULL)  {
		bu_log("asc2g: malloc failure for arbn\n");
		exit(-1);
	}

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
nxt_spc( cp)
register char *cp;
{
	while( *cp != ' ' && *cp != '\t' && *cp !='\0' )  {
		cp++;
	}
	if( *cp != '\0' )  {
		cp++;
	}
	return( cp );
}

ngran( nfloat )
{
	register int gran;
	/* Round up */
	gran = nfloat + ((sizeof(union record)-1) / sizeof(float) );
	gran = (gran * sizeof(float)) / sizeof(union record);
	return(gran);
}
