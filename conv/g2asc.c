/*
 *		G 2 A S C . C
 *  
 *  This program generates an ASCII data file which contains
 *  a GED database.
 *
 *  Usage:  g2asc < file.g > file.asc
 *  
 *  Author -
 *  	Charles M Kennedy
 *  	Michael J Muuss
 *	Susanne Muuss, J.D.
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
 
#include <stdio.h>
#include <ctype.h>
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "db.h"
#include "wdb.h"
#include "raytrace.h"
#include "rtlist.h"

#define RT_PARTICLE_TYPE_SPHERE		1
#define RT_PARTICLE_TYPE_CYLINDER	2
#define RT_PARTICLE_TYPE_CONE		3

struct	pipe_internal {
	int			pipe_count;
	struct wdb_pipeseg	pipe_segs;
};

struct	part_internal {
	point_t		part_V;
	vect_t		part_H;
	fastf_t		part_vrad;
	fastf_t		part_hrad;
	int		part_type;
};

struct	arbn_internal {
	int		neqn;
	plane_t		*eqn;
};


mat_t	id_mat = {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0};	/* identity matrix for pipes */

char *name();
char *strchop();
#define CH(x)	strchop(x,sizeof(x))

int	_rt_pipe_import(), _rt_part_import(), _rt_arbn_import();
void	combdump();
void	idendump(), polyhead(), polydata();
void	soldump();
void	membdump(), arsadump(), arsbdump();
void	materdump(), bspldump(), bsurfdump();
void	pipe_dump(), particle_dump(), dump_pipe_segs();
void	arbn_dump();

union record	record;		/* GED database record */

main(argc, argv)
char **argv;
{
	/* Read database file */
	while( fread( (char *)&record, sizeof record, 1, stdin ) == 1  &&
	    !feof(stdin) )  {
	    	if( argc > 1 )
			(void)fprintf(stderr,"0%o (%c)\n", record.u_id, record.u_id);
		/* Check record type and skip deleted records */
	    	switch( record.u_id )  {
	    	case ID_FREE:
			continue;
	    	case ID_SOLID:
			soldump();
			continue;
	    	case ID_COMB:
			combdump();
			continue;
	    	case ID_MEMB:
	    		/* Just convert them as they are found.  Assume DB is good */
	    		membdump();
	    		continue;
	    	case ID_ARS_A:
			arsadump();
	    		continue;
	    	case ID_P_HEAD:
			polyhead();
	    		continue;
	    	case ID_P_DATA:
			polydata();
	    		continue;
	    	case ID_IDENT:
			idendump();
	    		continue;
	    	case ID_MATERIAL:
			materdump();
	    		continue;
	    	case DBID_PIPE:
	    		pipe_dump();
	    		continue;
	    	case DBID_PARTICLE:
	    		particle_dump();
	    		continue;
	    	case DBID_ARBN:
	    		arbn_dump();
	    		continue;
	    	case ID_BSOLID:
			bspldump();
	    		continue;
	    	case ID_BSURF:
			bsurfdump();
	    		continue;
	    	default:
			(void)fprintf(stderr,
				"g2asc: unable to convert record type '%c' (0%o), skipping\n",
				record.u_id, record.u_id);
	    		continue;
		}
	}
	exit(0);
}

void
idendump()	/* Print out Ident record information */
{
	(void)printf( "%c %d %.6s\n",
		record.i.i_id,			/* I */
		record.i.i_units,		/* units */
		CH(record.i.i_version)		/* version */
	);
	(void)printf( "%.72s\n",
		CH(record.i.i_title)	/* title or description */
	);

	/* Print a warning message on stderr if versions differ */
	if( strcmp( record.i.i_version, ID_VERSION ) != 0 )  {
		(void)fprintf(stderr,
			"g2asc: File is version (%s), Program is version (%s)\n",
			record.i.i_version, ID_VERSION );
	}
}

void
polyhead()	/* Print out Polyhead record information */
{
	(void)printf("%c ", record.p.p_id );		/* P */
	(void)printf("%.16s", name(record.p.p_name) );	/* unique name */
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
polydata()	/* Print out Polydata record information */
{
	register int i, j;

	(void)printf("%c ", record.q.q_id );		/* Q */
	(void)printf("%d ", record.q.q_count );		/* # of vertices <= 5 */
	for( i = 0; i < 5; i++ )  {			/* [5][3] vertices */
		for( j = 0; j < 3; j++ ) {
			(void)printf("%.12e ", record.q.q_verts[i][j] );
		}
	}
	for( i = 0; i < 5; i++ )  {			/* [5][3] normals */
		for( j = 0; j < 3; j++ ) {
			(void)printf("%.12e ", record.q.q_norms[i][j] );
		}
	}
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
soldump()	/* Print out Solid record information */
{
	register int i;

	(void)printf("%c ", record.s.s_id );	/* S */
	(void)printf("%d ", record.s.s_type );	/* GED primitive type */
	(void)printf("%.16s ", name(record.s.s_name) );	/* unique name */
	(void)printf("%d ", record.s.s_cgtype );/* COMGEOM solid type */
	for( i = 0; i < 24; i++ )
		(void)printf("%.12e ", record.s.s_values[i] ); /* parameters */
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
pipe_dump()	/* Print out Pipe record information */
{

	int			ngranules;	/* number of granules, total */
	int			count;
	int			ret;
	char			*name;
	char			id;
	union record		*rp;		/* pointer to an array of granules */
	struct pipe_internal	pipe;		/* want a struct for the head, not a ptr. */
	struct wdb_pipeseg	head;		/* actual head, not a ptr. */

	ngranules = record.pw.pw_count;
	name = record.pw.pw_name;
	id = record.pw.pw_id;

	/* malloc enough space for ngranules */
	if( (rp = (union record *)malloc(ngranules * sizeof(union record)) ) == 0)  {
		fprintf(stderr, "g2asc: malloc failure\n");
		exit(-1);
	}

	/* copy the freebee record into the array */
	bcopy( (char *)&record, (char *)rp, sizeof(union record) );

	/* copy ngranules-1 more records into the array */

	if( (count = fread( (char *)&rp[1], sizeof(union record), ngranules - 1, stdin) ) != ngranules - 1)  {
		fprintf(stderr, "g2asc: pipe read failure\n");
		exit(-1);
	}

	/* Send this off to _rt_pipe_import() for conversion into machine
	 * dependent format and making of a doubly linked list.  rt_pipe_internal()
	 * fills in the "pipe_internal" structure.
	 */

	if( (ret = (_rt_pipe_import(&pipe, rp, id_mat) ) !=0 ) )   {
		fprintf(stderr, "g2asc: pipe_import failure\n" );
		exit(-1);
	}

	/* send the doubly linked list off to dump_pipe_segs(), which
	 * will print all the information.
	 */

	dump_pipe_segs(id, name, &pipe.pipe_segs);
	mk_pipe_free( &pipe.pipe_segs );	/* give back memory */
	free( (char *)rp );
}

void
dump_pipe_segs(id, name, headp)
char			id;
char			*name;
struct wdb_pipeseg	*headp;
{

	struct wdb_pipeseg	*sp;

	printf("%c %.16s\n", id, name);

	/* print parameters for each segment: one segment per line */

	for( RT_LIST( sp, wdb_pipeseg, &(headp->l) ) )  {
		switch(sp->ps_type)  {
		case WDB_PIPESEG_TYPE_END:
			printf("end %26.20e %26.20e %26.20e %26.20e %26.20e\n",
				sp->ps_id, sp->ps_od,
				sp->ps_start[X],
				sp->ps_start[Y],
				sp->ps_start[Z] );
			break;
		case WDB_PIPESEG_TYPE_LINEAR:
			printf("linear %26.20e %26.20e %26.20e %26.20e %26.20e\n",
				sp->ps_id, sp->ps_od,
				sp->ps_start[X],
				sp->ps_start[Y],
				sp->ps_start[Z] );
			break;
		case WDB_PIPESEG_TYPE_BEND:
			printf("bend %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e\n",
				sp->ps_id, sp->ps_od,
				sp->ps_start[X],
				sp->ps_start[Y],
				sp->ps_start[Z],
				sp->ps_bendcenter[X],
				sp->ps_bendcenter[Y],
				sp->ps_bendcenter[Z]);
			break;
		default:
			fprintf(stderr, "g2asc: unknown pipe type %d\n",
				sp->ps_type);
			break;
		}
	}
}

void
particle_dump()	/* Print out Particle record information */
{

	/* Note that particles fit into one granule only. */

	int			ret;
	char			*type;
	struct part_internal 	part;	/* head for the structure */
	
	if( (ret = (_rt_part_import(&part, &record, id_mat)) ) != 0)  {
		fprintf(stderr, "g2asc: particle import failure\n");
		exit(-1);
	}

	/* Particle type is picked up on here merely to ensure receiving
	 * valid data.  The type is not used any further.
	 */

	switch( part.part_type )  {
	case RT_PARTICLE_TYPE_SPHERE:
		type = "sphere";
		break;
	case RT_PARTICLE_TYPE_CYLINDER:
		type = "cylinder";
		break;
	case RT_PARTICLE_TYPE_CONE:
		type = "cone";
		break;
	default:
		fprintf(stderr, "g2asc: no particle type %s\n", type);
		exit(-1);
	}

	printf("%c %.16s %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e\n",
		record.part.p_id, record.part.p_name,
		part.part_V[X],
		part.part_V[Y],
		part.part_V[Z],
		part.part_H[X],
		part.part_H[Y],
		part.part_H[Z],
		part.part_vrad, part.part_hrad);
}


/*			A R B N _ D U M P
 *
 *  Print out arbn information.
 *
 */
void
arbn_dump()
{

	int		ngranules;	/* number of granules to be read */
	int		count;
	int		neqn;		/* number of plane equations */
	int		ret;		/* return code catcher */
	int		i;		/* a counter */
	char		*name;
	char		id;
	union record	*rp;
	struct arbn_internal	arbn;

	ngranules = record.n.n_grans;
	name = record.n.n_name;
	id = record.n.n_id;
	neqn = record.n.n_neqn;

	/* malloc space for ngranules + 1 */
	if( (rp = (union record *) malloc( (ngranules + 1) * sizeof(union record)) ) == 0)  {
		fprintf( stderr, "g2asc: malloc failure\n");
		exit(-1);
	}

	/* Copy the freebie (first) record into the array of records.  Then
	 * copy ngranules more. 
	 */
	bcopy( (char *)&record, (char *)rp, sizeof(union record) );
	if( (count = fread( (char *)&rp[1], sizeof(union record), ngranules, stdin) ) != ngranules )  {
		fprintf(stderr, "g2asc: arbn read failure\n");
	}

	/* Hand off the rt's arbn_import() routine */
	if( ret = (_rt_arbn_import(&arbn, rp, id_mat) ) != 0)  {
		fprintf(stderr, "g2asc: arbn import failure\n");
		exit(-1);
	}

	fprintf(stdout, "%c %.16s %d\n", id, name, neqn);
	for( i = 0; i < arbn.neqn; i++ )  {
		printf("n %26.20e %20.26e %26.20e %26.20e\n", arbn.eqn[i][X], arbn.eqn[i][Y],
			arbn.eqn[i][Z], arbn.eqn[i][3]);
	}

	free( (char *)rp );
}

	
/*
 *			C O M B D U M P
 *
 */
void
combdump()	/* Print out Combination record information */
{
	register int i;
	register int length;	/* Keep track of number of members */
	int	m1, m2;		/* material property flags */

	(void)printf("%c ", record.c.c_id );		/* C */
	if( record.c.c_flags == 'R' )			/* set region flag */
		(void)printf("Y ");			/* Y if `R' */
	else
		(void)printf("N ");			/* N if ` ' */
	(void)printf("%.16s ", name(record.c.c_name) );	/* unique name */
	(void)printf("%d ", record.c.c_regionid );	/* region ID code */
	(void)printf("%d ", record.c.c_aircode );	/* air space code */
#if 1
	(void)printf("%d ", 0 );			/* DEPRECATED: # of members */
	(void)printf("%d ", 0 );			/* DEPRECATED: COMGEOM region # */
#else
	(void)printf("%d ", record.c.c_length );        /* DEPRECATED: # of members */
	(void)printf("%d ", record.c.c_num );           /* DEPRECATED: COMGEOM region # */
#endif
	(void)printf("%d ", record.c.c_material );	/* material code */
	(void)printf("%d ", record.c.c_los );		/* equiv. LOS est. */
	(void)printf("%d %d %d %d ",
		record.c.c_override ? 1 : 0,
		record.c.c_rgb[0],
		record.c.c_rgb[1],
		record.c.c_rgb[2] );
	m1 = m2 = 0;
	if( isascii(record.c.c_matname[0]) && isprint(record.c.c_matname[0]) )  {
		m1 = 1;
		if( record.c.c_matparm[0] )
			m2 = 1;
	}
	printf("%d %d ", m1, m2 );
	switch( record.c.c_inherit )  {
	case DB_INH_HIGHER:
		printf("%d ", DB_INH_HIGHER );
		break;
	default:
	case DB_INH_LOWER:
		printf("%d ", DB_INH_LOWER );
		break;
	}
	(void)printf("\n");			/* Terminate w/ a newline */

	if( m1 )
		(void)printf("%.32s\n", CH(record.c.c_matname) );
	if( m2 )
		(void)printf("%.60s\n", CH(record.c.c_matparm) );
}

void
membdump()	/* Print out Member record information */
{
	register int i;

	(void)printf("%c ", record.M.m_id );		/* M */
	(void)printf("%c ", record.M.m_relation );	/* Boolean oper. */
	(void)printf("%.16s ", name(record.M.m_instname) );	/* referred-to obj. */
	for( i = 0; i < 16; i++ )			/* homogeneous transform matrix */
		(void)printf("%.12e ", record.M.m_mat[i] );
	(void)printf("%d ", 0 );			/* was COMGEOM solid # */
	(void)printf("\n");				/* Terminate w/ nl */
}

void
arsadump()	/* Print out ARS record information */
{
	register int i;
	register int length;	/* Keep track of number of ARS B records */

	(void)printf("%c ", record.a.a_id );	/* A */
	(void)printf("%d ", record.a.a_type );	/* primitive type */
	(void)printf("%.16s ", name(record.a.a_name) );	/* unique name */
	(void)printf("%d ", record.a.a_m );	/* # of curves */
	(void)printf("%d ", record.a.a_n );	/* # of points per curve */
	(void)printf("%d ", record.a.a_curlen );/* # of granules per curve */
	(void)printf("%d ", record.a.a_totlen );/* # of granules for ARS */
	(void)printf("%.12e ", record.a.a_xmax );	/* max x coordinate */
	(void)printf("%.12e ", record.a.a_xmin );	/* min x coordinate */
	(void)printf("%.12e ", record.a.a_ymax );	/* max y coordinate */
	(void)printf("%.12e ", record.a.a_ymin );	/* min y coordinate */
	(void)printf("%.12e ", record.a.a_zmax );	/* max z coordinate */
	(void)printf("%.12e ", record.a.a_zmin );	/* min z coordinate */
	(void)printf("\n");			/* Terminate w/ a newline */
			
	length = (int)record.a.a_totlen;	/* Get # of ARS B records */

	for( i = 0; i < length; i++ )  {
		arsbdump();
	}
}

void
arsbdump()	/* Print out ARS B record information */
{
	register int i;
	
	/* Read in a member record for processing */
	(void)fread( (char *)&record, sizeof record, 1, stdin );
	(void)printf("%c ", record.b.b_id );		/* B */
	(void)printf("%d ", record.b.b_type );		/* primitive type */
	(void)printf("%d ", record.b.b_n );		/* current curve # */
	(void)printf("%d ", record.b.b_ngranule );	/* current granule */
	for( i = 0; i < 24; i++ )  {			/* [8*3] vectors */
		(void)printf("%.12e ", record.b.b_values[i] );
	}
	(void)printf("\n");			/* Terminate w/ a newline */
}

void
materdump()	/* Print out material description record information */
{
	(void)printf( "%c %d %d %d %d %d %d\n",
		record.md.md_id,			/* m */
		record.md.md_flags,			/* UNUSED */
		record.md.md_low,	/* low end of region IDs affected */
		record.md.md_hi,	/* high end of region IDs affected */
		record.md.md_r,
		record.md.md_g,		/* color of regions: 0..255 */
		record.md.md_b );
}

void
bspldump()	/* Print out B-spline solid description record information */
{
	(void)printf( "%c %.16s %d %.12e\n",
		record.B.B_id,		/* b */
		name(record.B.B_name),	/* unique name */
		record.B.B_nsurf,	/* # of surfaces in this solid */
		record.B.B_resolution );	/* resolution of flatness */
}

void
bsurfdump()	/* Print d-spline surface description record information */
{
	register int i;
	register float *vp;
	int nbytes, count;
	float *fp;

	(void)printf( "%c %d %d %d %d %d %d %d %d %d\n",
		record.d.d_id,		/* D */
		record.d.d_order[0],	/* order of u and v directions */
		record.d.d_order[1],	/* order of u and v directions */
		record.d.d_kv_size[0],	/* knot vector size (u and v) */
		record.d.d_kv_size[1],	/* knot vector size (u and v) */
		record.d.d_ctl_size[0],	/* control mesh size (u and v) */
		record.d.d_ctl_size[1],	/* control mesh size (u and v) */
		record.d.d_geom_type,	/* geom type 3 or 4 */
		record.d.d_nknots,	/* # granules of knots */
		record.d.d_nctls );	/* # granules of ctls */
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
		(void)fprintf(stderr, "g2asc: spline knot malloc error\n");
		exit(1);
	}
	fp = vp;
	(void)bzero( (char *)fp, nbytes );
	count = fread( (char *)fp, 1, nbytes, stdin );
	if( count != nbytes )  {
		(void)fprintf(stderr, "g2asc: spline knot read failure\n");
		exit(1);
	}
	/* Print the knot vector information */
	count = record.d.d_kv_size[0] + record.d.d_kv_size[1];
	for( i = 0; i < count; i++ )  {
		(void)printf("%.12e\n", *vp++);
	}
	/* Free the knot data memory */
	(void)free( (char *)fp );

	/* Malloc and clear memory for the CONTROL MESH data and read it */
	nbytes = record.d.d_nctls * sizeof(union record);
	if( (vp = (float *)malloc(nbytes))  == (float *)0 )  {
		(void)fprintf(stderr, "g2asc: control mesh malloc error\n");
		exit(1);
	}
	fp = vp;
	(void)bzero( (char *)fp, nbytes );
	count = fread( (char *)fp, 1, nbytes, stdin );
	if( count != nbytes )  {
		(void)fprintf(stderr, "g2asc: control mesh read failure\n");
		exit(1);
	}
	/* Print the control mesh information */
	count = record.d.d_ctl_size[0] * record.d.d_ctl_size[1] *
		record.d.d_geom_type;
	for( i = 0; i < count; i++ )  {
		(void)printf("%.12e\n", *vp++);
	}
	/* Free the control mesh memory */
	(void)free( (char *)fp );
}

/*
 *			N A M E
 *
 *  Take a database name and null-terminate it,
 *  converting unprintable characters to something printable.
 *  Here we deal with NAMESIZE long names not being null-terminated.
 */
char *name( str )
char *str;
{
	static char buf[NAMESIZE+2];
	register char *ip = str;
	register char *op = buf;
	register int warn = 0;

	while( op < &buf[NAMESIZE] )  {
		if( *ip == '\0' )  break;
		if( isascii(*ip) && isprint(*ip) && !isspace(*ip) )  {
			*op++ = *ip++;
		}  else  {
			*op++ = '@';
			ip++;
			warn = 1;
		}
	}
	*op = '\0';
	if(warn)  {
		(void)fprintf(stderr,
			"g2asc: Illegal char in object name, converted to '%s'\n",
			buf );
	}
	if( op == buf )  {
		/* Null input name */
		(void)fprintf(stderr,
			"g2asc:  NULL object name converted to -=NULL=-\n");
		return("-=NULL=-");
	}
	return(buf);
}

/*
 *			S T R C H O P
 *
 *  Take a string and a length, and null terminate,
 *  converting unprintable characters to something printable.
 */
char *strchop( str, len )
char *str;
{
	static char buf[1024];
	register char *ip = str;
	register char *op = buf;
	register int warn = 0;
	char *ep;

	if( len > sizeof(buf)-2 )  len=sizeof(buf)-2;
	ep = &buf[len-1];		/* Leave room for null */
	while( op < ep )  {
		if( *ip == '\0' )  break;
		if( isascii(*ip) && (isprint(*ip) || isspace(*ip)) )  {
			*op++ = *ip++;
		}  else  {
			*op++ = '@';
			ip++;
			warn = 1;
		}
	}
	*op = '\0';
	if(warn)  {
		(void)fprintf(stderr,
			"g2asc: Illegal char in string, converted to '%s'\n",
			buf );
	}
	if( op == buf )  {
		/* Null input name */
		(void)fprintf(stderr,
			"g2asc:  NULL string converted to -=STRING=-\n");
		return("-=STRING=-");
	}
	return(buf);
}

/*** XXX temporary copy, until formal import library exists ***/
/*
 *			R T _ P I P E _ I M P O R T
 */
int
_rt_pipe_import( pipe, rp, mat )
struct pipe_internal	*pipe;
union record		*rp;
register mat_t		mat;
{
	register struct exported_pipeseg *ep;
	register struct wdb_pipeseg	*psp;
	struct wdb_pipeseg		tmp;

	/* Check record type */
	if( rp->u_id != DBID_PIPE )  {
		fprintf(stderr,"_rt_pipe_import: defective record\n");
		return(-1);
	}

	/* Count number of segments */
	pipe->pipe_count = 0;
	for( ep = &rp->pw.pw_data[0]; ; ep++ )  {
		pipe->pipe_count++;
		switch( (int)(ep->eps_type[0]) )  {
		case WDB_PIPESEG_TYPE_END:
			goto done;
		case WDB_PIPESEG_TYPE_LINEAR:
		case WDB_PIPESEG_TYPE_BEND:
			break;
		default:
			return(-2);	/* unknown segment type */
		}
	}
done:	;
	if( pipe->pipe_count <= 1 )
		return(-3);		/* Not enough for 1 pipe! */

	/*
	 *  Walk the array of segments in reverse order,
	 *  allocating a linked list of segments in internal format,
	 *  using exactly the same structures as libwdb.
	 */
	RT_LIST_INIT( &pipe->pipe_segs.l );
	for( ep = &rp->pw.pw_data[pipe->pipe_count-1]; ep >= &rp->pw.pw_data[0]; ep-- )  {
		tmp.ps_type = (int)ep->eps_type[0];
		ntohd( tmp.ps_start, ep->eps_start, 3 );
		ntohd( &tmp.ps_id, ep->eps_id, 1 );
		ntohd( &tmp.ps_od, ep->eps_od, 1 );

		/* Apply modeling transformations */
		psp = (struct wdb_pipeseg *)calloc( 1, sizeof(struct wdb_pipeseg) );
		psp->ps_type = tmp.ps_type;
		MAT4X3PNT( psp->ps_start, mat, tmp.ps_start );
		if( psp->ps_type == WDB_PIPESEG_TYPE_BEND )  {
			ntohd( tmp.ps_bendcenter, ep->eps_bendcenter, 3 );
			MAT4X3PNT( psp->ps_bendcenter, mat, tmp.ps_bendcenter );
		} else {
			VSETALL( psp->ps_bendcenter, 0 );
		}
		psp->ps_id = tmp.ps_id / mat[15];
		psp->ps_od = tmp.ps_od / mat[15];
		RT_LIST_APPEND( &pipe->pipe_segs.l, &psp->l );
	}

	return(0);			/* OK */
}

/*		R T _ P A R T_ I M P O R T
 *
 */
int
_rt_part_import( part, rp, mat )
struct part_internal	  *part;
union record		  *rp;
register mat_t		  mat;
{
	point_t		  v;
	vect_t		  h;
	double		  vrad;
	double		  hrad;
	fastf_t		  maxrad, minrad;

	/* Check record type */
	if( rp->u_id != DBID_PARTICLE )  {
		fprintf(stderr,"_rt_part_import: defective record\n");
		return(-1);
	}

	/* Convert from database to internal format */
	ntohd( v, rp->part.p_v, 3 );
	ntohd( h, rp->part.p_h, 3 );
	ntohd( &vrad, rp->part.p_vrad, 1 );
	ntohd( &hrad, rp->part.p_hrad, 1 );

	/* Apply modeling transformations */
	MAT4X3PNT( part->part_V, mat, v );
	MAT4X3PNT( part->part_H, mat, h );
	if( (part->part_vrad = vrad / mat[15]) < 0 )
		return(-2);
	if( (part->part_hrad = hrad / mat[15]) < 0 )
		return(-2);

	if( part->part_vrad > part->part_hrad )  {
		maxrad = part->part_vrad;
		minrad = part->part_hrad;
	} else {
		maxrad = part->part_hrad;
		minrad = part->part_vrad;
	}
	if( maxrad <= 0 )
		return(-4);

	if( MAGSQ( part->part_H ) * 1000000 < maxrad * maxrad)  {
		/* Height vector is insignificant, particle is a sphere */
		part->part_vrad = part->part_hrad = maxrad;
		VSETALL( part->part_H, 0 );		 /* sanity */
		part->part_type = RT_PARTICLE_TYPE_SPHERE;
		return(0);		 /* OK */
	}

	if( (maxrad - minrad) / maxrad < 0.001 )  {
		/* radii are nearly equal, particle is a cylinder (lozenge) */
		part->part_vrad = part->part_hrad = maxrad;
		part->part_type = RT_PARTICLE_TYPE_CYLINDER;
		return(0);		 /* OK */
	}

	part->part_type = RT_PARTICLE_TYPE_CONE;
	return(0);		 /* OK */

}

/*		R T _ A R B N _ I M P O R T
 *
 * Cannabalized from the rt library.  For temporary use only, pending
 * the arrival of a formal import library.
 *
 */

int
_rt_arbn_import( aip, rp, mat )
struct arbn_internal	*aip;
union record		*rp;
register mat_t		mat;
{

	register int	i;

	if( rp->u_id != DBID_ARBN )  {
		rt_log("_rt_arbn_import: defective record, id=x%x\n", rp->u_id);
		return(-1);
	}

	aip->neqn = rp->n.n_neqn;
	if( aip->neqn <= 0 )
		return( -1 );
	aip->eqn = (plane_t *)rt_malloc( aip->neqn * sizeof(plane_t), "_rt_arbn_import() planes");
	

	ntohd( (char *)aip->eqn, (char *)(&rp[1]), aip->neqn*4 );

	/* Transform by the matrix */
#	include "noalias.h"
	for( i = 0; i < aip->neqn; i++ )  {
		point_t	orig_pt;
		point_t	pt;
		vect_t	norm;


		/* Pick a point on the original halfspace */
		VSCALE( orig_pt, aip->eqn[i], aip->eqn[i][3] );

		/* Transform the point and the normal */
		MAT4X3VEC( norm, mat, aip->eqn[i] );
		MAT4X3PNT( pt, mat, orig_pt );

		/* Measure new distance from origin to new point */
		VMOVE( aip->eqn[i], norm );
		aip->eqn[i][3] = VDOT( pt, norm );

	}

	return(0);
}
