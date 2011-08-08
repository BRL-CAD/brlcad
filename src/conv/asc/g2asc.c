/*                         G 2 A S C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file g2asc.c
 *
 *  This program generates an ASCII data file which contains
 *  a GED database.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"
#include "bin.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"
#include "tcl.h"

const mat_t id_mat = MAT_INIT_IDN; /* identity matrix for pipes */

char *strchop(char *str, size_t len);
#define CH(x)	strchop(x, sizeof(x))

int	combdump(void);
void	idendump(void), polyhead(void), polydata(void);
void	soldump(void), extrdump(void), sketchdump(void);
void	membdump(union record *rp), arsadump(void), arsbdump(void);
void	materdump(void), bspldump(void), bsurfdump(void);
void	pipe_dump(void), particle_dump(void), dump_pipe_segs(char *, struct bu_list *);
void	arbn_dump(void), cline_dump(void), bot_dump(void);
void	nmg_dump(void);
void	strsol_dump(void);

union record	record;		/* GED database record */

static const char usage[] = "\
Usage: g2asc file.g file.asc\n\
 Convert a binary BRL-CAD database to machine-independent ASCII form\n\
";

FILE	*ifp;
FILE	*ofp;
char	*iname = "-";

static char *tclified_name=NULL;
static size_t tclified_name_buffer_len=0;


/*	This routine escapes the '{' and '}' characters in any string and returns a static buffer containing the
 *	resulting string. Used for names and db title on output.
 *
 *	NOTE: RETURN OF STATIC BUFFER
 */
char *
tclify_name( const char *name )
{
    const char *src=name;
    char *dest;

    size_t max_len=2*strlen( name ) + 1;

    if ( max_len < 2 ) {
	return (char *)NULL;
    }

    if ( max_len > tclified_name_buffer_len ) {
	tclified_name_buffer_len = max_len;
	tclified_name = bu_realloc( tclified_name, tclified_name_buffer_len, "tclified_name buffer" );
    }

    dest = tclified_name;

    while ( *src ) {
	if ( *src == '{' || *src == '}' ) {
	    *dest++ = '\\';
	}
	*dest++ = *src++;
    }
    *dest = '\0';

    return tclified_name;
}

int
main(int argc, char **argv)
{
    unsigned i;

    if (argc != 3) {
	bu_exit(1, "%s", usage);
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);
#endif

    iname = "-";
    ifp = stdin;
    ofp = stdout;

    bu_debug = BU_DEBUG_COREDUMP;

    if ( argc >= 3 ) {
	iname = argv[1];
	if ( BU_STR_EQUAL(iname, "-") )  {
	    ifp = stdin;
	} else {
	    ifp = fopen(iname, "rb");
	}
	if ( !ifp )  perror(iname);
	if ( BU_STR_EQUAL(argv[2], "-") )  {
	    ofp = stdout;
	} else {
	    ofp = fopen(argv[2], "wb");
	}
	if ( !ofp )  perror(argv[2]);
	if (ifp == NULL || ofp == NULL) {
	    bu_exit(1, "g2asc: can't open files.");
	}
    }
    if (isatty(fileno(ifp))) {
	bu_exit(1, "%s", usage);
    }

    Tcl_FindExecutable(argv[0]);

    rt_init_resource( &rt_uniresource, 0, NULL );

    /* First, determine what version database this is */
    if ( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )  {
	bu_exit(2, "g2asc(%s) ERROR, file too short to be BRL-CAD database\n",
		iname);
    }

    if ( db5_header_is_valid( (unsigned char *)&record ) )  {
	Tcl_Interp	*interp;
	struct db_i	*dbip;
	struct directory *dp;
	const char *u;

	if ( ifp == stdin || ofp == stdout ) {
	    bu_log( "Cannot use stdin or stdout for Release 6 or later databases\n");
	    bu_exit(1, "Please use the \"g2asc input.g output.g\" form\n" );
	}

	bu_log("Exporting Release 6 database\n" );
	bu_log("  Note that the Release 6 binary format is machine independent.\n");
	bu_log("  Converting to ASCII to move database to a different\n");
	bu_log("  computer architecture is no longer necessary.\n");
	interp = Tcl_CreateInterp();
	/* This runs the init.tcl script */
	if ( Tcl_Init(interp) == TCL_ERROR )
	    bu_log("Tcl_Init error %s\n", Tcl_GetStringResult(interp));

	if ( (dbip = db_open( iname, "rb" )) == NULL )  {
	    bu_exit(4, "Unable to db_open() file '%s', aborting\n", iname );
	}
	RT_CK_DBI(dbip);
	if ( db_dirbuild( dbip ) ) {
	    bu_exit(1, "db_dirbuild failed\n" );
	}

	/* write out the title and units special */
	if ( dbip->dbi_title[0] ) {
	    fprintf( ofp, "title {%s}\n", tclify_name( dbip->dbi_title ) );
	} else {
	    fprintf( ofp, "title {Untitled BRL-CAD Database}\n" );
	}
	u = bu_units_string( dbip->dbi_local2base );
	if (u) {
	    fprintf( ofp, "units %s\n", u );
	}
	FOR_ALL_DIRECTORY_START(dp, dbip)  {
	    struct rt_db_internal	intern;
	    struct bu_attribute_value_set *avs=NULL;

	    /* Process the _GLOBAL object */
	    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
		const char *value;
		Tcl_Obj	*list, *obj;
		size_t list_len;
		struct bu_attribute_value_set g_avs;

		/* get _GLOBAL attributes */
		if ( db5_get_attributes( dbip, &g_avs, dp ) ) {
		    bu_log( "Failed to find any attributes on _GLOBAL\n" );
		    continue;
		}

		/* save the associated attributes of
		 * _GLOBAL (except for title and units
		 * which were already written out) and
                 * regionid_colortable (which is written out below)
		 */
		if (g_avs.count) {
                    int printedHeader = 0;
		    for (i=0; i < g_avs.count; i++) {
			if (strncmp(g_avs.avp[i].name, "title", 6) == 0) {
			    continue;
			} else if (strncmp(g_avs.avp[i].name, "units", 6) == 0) {
			    continue;
                        } else if (strncmp(g_avs.avp[i].name, "regionid_colortable", 19) == 0) {
                            continue;
			} else if (strlen(g_avs.avp[i].name) <= 0) {
			    continue;
			}
                        if (printedHeader == 0) {
                            fprintf(ofp, "attr set {_GLOBAL}");
                            printedHeader = 1;
                        }
			fprintf(ofp, " {%s} {%s}", g_avs.avp[i].name, g_avs.avp[i].value);
		    }
                    if (printedHeader == 1)
                        fprintf(ofp, "\n");
		}

		value = bu_avs_get( &g_avs, "regionid_colortable" );
		if ( !value )
		    continue;
		list = Tcl_NewStringObj( value, -1);
		{
		    int llen;
		    if ( Tcl_ListObjLength( interp, list, &llen ) != TCL_OK ) {
			bu_log( "Failed to get length of region color table!!\n" );
			continue;
		    }
		    list_len = (size_t)llen;
		}
		for ( i=0; i<list_len; i++ ) {
		    if ( Tcl_ListObjIndex( interp, list, i, &obj ) != TCL_OK ) {
			bu_log( "Cannot get entry %d from the color table!!\n",
				i );
			continue;
		    }
		    fprintf( ofp, "color %s\n",
			     Tcl_GetStringFromObj(obj, NULL) );
		}
		bu_avs_free( &g_avs );
		continue;
	    }

	    if ( rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource ) < 0 )  {
		bu_log("Unable to read '%s', skipping\n", dp->d_namep);
		continue;
	    }
	    if (!intern.idb_meth->ft_get) {
		bu_log("Unable to get '%s' (unimplemented), skipping\n", dp->d_namep);
		continue;
	    }

	    if ( dp->d_flags & RT_DIR_COMB ) {
		struct bu_vls logstr;

		bu_vls_init(&logstr);
		if (intern.idb_meth->ft_get(&logstr, &intern, "tree") != TCL_OK) {
		    rt_db_free_internal(&intern);
		    bu_log("Unable to export '%s', skipping\n", dp->d_namep);
		    Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
		    bu_vls_free(&logstr);
		    continue;
		}
		Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
		bu_vls_free(&logstr);
		if ( dp->d_flags & RT_DIR_REGION ) {
		    fprintf( ofp, "put {%s} comb region yes tree {%s}\n",
			     tclify_name( dp->d_namep ),
			     Tcl_GetStringResult(interp) );
		} else {
		    fprintf( ofp, "put {%s} comb region no tree {%s}\n",
			     tclify_name( dp->d_namep ),
			     Tcl_GetStringResult(interp) );
		}
	    } else {
		struct bu_vls logstr;

		bu_vls_init(&logstr);
		if ( (dp->d_minor_type != ID_CONSTRAINT) && (intern.idb_meth->ft_get( &logstr, &intern, NULL ) != TCL_OK) )  {
		    rt_db_free_internal(&intern);
		    bu_log("Unable to export '%s', skipping\n", dp->d_namep );
		    Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
		    bu_vls_free(&logstr);
		    continue;
		}
		Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
		bu_vls_free(&logstr);
		fprintf( ofp, "put {%s} %s\n",
			 tclify_name( dp->d_namep ),
			 Tcl_GetStringResult(interp) );
	    }
	    avs = &intern.idb_avs;
	    if ( avs->magic == BU_AVS_MAGIC && avs->count > 0 ) {
		fprintf( ofp, "attr set {%s}", tclify_name( dp->d_namep ) );
		for ( i=0; i<avs->count; i++ ) {
		    if (strlen(avs->avp[i].name) <= 0) {
			continue;
		    }
		    fprintf( ofp, " {%s}", avs->avp[i].name );
		    fprintf( ofp, " {%s}", avs->avp[i].value );
		}
		fprintf( ofp, "\n" );
	    }
	    Tcl_ResetResult( interp );
	    rt_db_free_internal(&intern);
	} FOR_ALL_DIRECTORY_END;
	return 0;
    } else {
	/* A record is already in the input buffer */
	goto top;
    }

    /* Read database file */
 top:
    do {
	/* Check record type and skip deleted records */
	switch ( record.u_id )  {
	    case ID_FREE:
		continue;
	    case ID_SOLID:
		soldump();
		continue;
	    case ID_COMB:
		if ( combdump() > 0 )  goto top;
		continue;
	    case ID_MEMB:
		(void)fprintf(stderr, "g2asc: stray MEMB record, skipped\n");
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
	    case DBID_STRSOL:
		strsol_dump();
		continue;
	    case DBID_NMG:
		nmg_dump();
		continue;
	    case DBID_PARTICLE:
		particle_dump();
		continue;
	    case DBID_ARBN:
		arbn_dump();
		continue;
	    case DBID_CLINE:
		cline_dump();
		continue;
	    case DBID_BOT:
		bot_dump();
		continue;
	    case ID_BSOLID:
		bspldump();
		continue;
	    case ID_BSURF:
		bsurfdump();
		continue;
	    case DBID_SKETCH:
		sketchdump();
		continue;
	    case DBID_EXTR:
		extrdump();
		continue;
	    default:
		(void)fprintf(stderr,
			      "g2asc: unable to convert record type '%c' (0%o), skipping\n",
			      record.u_id, record.u_id);
		continue;
	}
    }  while ( fread( (char *)&record, sizeof record, 1, ifp ) == 1  &&
	       !feof(ifp) );

    return 0;
}


/*
 *			N A M E
 *
 *  Take a database name and null-terminate it,
 *  converting unprintable characters to something printable.
 *  Here we deal with names not being null-terminated.
 */
char *encode_name(char *str)
{
    static char buf[NAMESIZE+1];
    char *ip = str;
    char *op = buf;
    int warn = 0;

    while ( op < &buf[NAMESIZE] )  {
	if ( *ip == '\0' )  break;
	if ( isascii(*ip) && isprint(*ip) && !isspace(*ip) )  {
	    *op++ = *ip++;
	}  else  {
	    *op++ = '@';
	    ip++;
	    warn = 1;
	}
    }
    *op = '\0';
    if (warn)  {
	(void)fprintf(stderr,
		      "g2asc: Illegal char in object name, converted to '%s'\n",
		      buf );
    }
    if ( op == buf )  {
	/* Null input name */
	(void)fprintf(stderr,
		      "g2asc:  NULL object name converted to -=NULL=-\n");
	return "-=NULL=-";
    }
    return buf;
}


/*
 *			G E T _ E X T
 *
 *  Take "ngran" granueles, and put them in memory.
 *  The first granule comes from the global extern "record",
 *  the remainder are read from ifp.
 */
void
get_ext(struct bu_external *ep, size_t ngran)
{
    size_t count;

    BU_EXTERNAL_INIT(ep);

    ep->ext_nbytes = ngran * sizeof(union record);
    ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "get_ext ext_buf" );

    /* Copy the freebie (first) record into the array of records.  */
    memcpy((char *)ep->ext_buf, (char *)&record, sizeof(union record));
    if ( ngran <= 1 )  return;

    count = fread( ((char *)ep->ext_buf)+sizeof(union record),
		   sizeof(union record), ngran-1, ifp);
    if ( count != (size_t)ngran-1 )  {
	fprintf(stderr,
		"g2asc: get_ext:  wanted to read %lu granules, got %lu\n",
		(unsigned long)ngran-1, (unsigned long)count);
	bu_exit(1, NULL);
    }
}

void
nmg_dump(void)
{
    union record rec;
    long struct_count[26];
    size_t i, granules;
    size_t j, k;

    /* just in case someone changes the record size */
    if ( sizeof( union record )%32 )
    {
	fprintf( stderr, "g2asc: nmg_dump cannot work with records not multiple of 32\n" );
	bu_exit( -1, NULL );
    }

    /* get number of granules needed for this NMG */
    granules = ntohl(*(uint32_t *)record.nmg.N_count);

    /* get the array of structure counts */
    for ( j=0; j<26; j++ )
	struct_count[j] = ntohl(*(uint32_t *)&record.nmg.N_structs[j*4]);

    /* output some header info */
    (void)fprintf(ofp,  "%c %d %.16s %lu\n",
		  record.nmg.N_id,	/* N */
		  record.nmg.N_version,	/* NMG version */
		  record.nmg.N_name,	/* solid name */
		  (unsigned long)granules );		/* number of additional granules */

    /* output the structure counts */
    for ( j=0; j<26; j++ )
	(void)fprintf(ofp,  " %ld", struct_count[j] );
    (void)fputc( '\n', ofp );

    /* dump the reminder in hex format */
    for ( i=0; i<granules; i++ )
    {
	char *cp;
	/* Read the record */
	if ( !fread( (char *)&rec, sizeof record, 1, ifp ) )
	{
	    (void)fprintf(stderr, "Error reading nmg granules\n" );
	    bu_exit( -1, NULL );
	}
	cp = (char *)&rec;

	/* 32 bytes per line */
	for ( k=0; k<sizeof( union record)/32; k++ )
	{
	    for ( j=0; j<32; j++ )
		fprintf(ofp,  "%02x", (0xff & (*cp++)) );	 /* two hex digits per byte */
	    fputc( '\n', ofp );
	}
    }
}

void
strsol_dump(void)	/* print out strsol solid info */
{
    union record rec[DB_SS_NGRAN];
    char *cp;

    /* get all the strsol granules */
    rec[0] = record;	/* struct copy the current record */

    /* read the rest from ifp */
    if ( !fread( (char *)&rec[1], sizeof record, DB_SS_NGRAN-1, ifp ) )
    {
	(void)fprintf(stderr, "Error reading strsol granules\n" );
	bu_exit( -1, NULL );
    }

    /* make sure that at least the last byte is null */
    cp = (char *)&rec[DB_SS_NGRAN-1];
    cp += (sizeof( union record ) - 1);
    *cp = '\0';

    (void)fprintf(ofp,  "%c %.16s %.16s %s\n",
		  rec[0].ss.ss_id,	/* s */
		  rec[0].ss.ss_keyword,	/* "ebm", "vol", or ??? */
		  rec[0].ss.ss_name,	/* solid name */
		  rec[0].ss.ss_args );	/* everything else */

}

void
idendump(void)	/* Print out Ident record information */
{
    (void)fprintf(ofp,  "%c %d %.6s\n",
		  record.i.i_id,			/* I */
		  record.i.i_units,		/* units */
		  CH(record.i.i_version)		/* version */
	);
    (void)fprintf(ofp,  "%.72s\n",
		  CH(record.i.i_title)	/* title or description */
	);

    /* Print a warning message on stderr if versions differ */
    if ( !BU_STR_EQUAL( record.i.i_version, ID_VERSION ) )  {
	(void)fprintf(stderr,
		      "g2asc: File is version (%s), Program is version (%s)\n",
		      record.i.i_version, ID_VERSION );
    }
}

void
polyhead(void)	/* Print out Polyhead record information */
{
    (void)fprintf(ofp, "%c ", record.p.p_id );		/* P */
    (void)fprintf(ofp, "%.16s", encode_name(record.p.p_name) );	/* unique name */
    (void)fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

void
polydata(void)	/* Print out Polydata record information */
{
    int i, j;

    (void)fprintf(ofp, "%c ", record.q.q_id );		/* Q */
    (void)fprintf(ofp, "%d", record.q.q_count );		/* # of vertices <= 5 */
    for ( i = 0; i < 5; i++ )  {
	/* [5][3] vertices */
	for ( j = 0; j < 3; j++ ) {
	    (void)fprintf(ofp, " %.12e", record.q.q_verts[i][j] );
	}
    }
    for ( i = 0; i < 5; i++ )  {
	/* [5][3] normals */
	for ( j = 0; j < 3; j++ ) {
	    (void)fprintf(ofp, " %.12e", record.q.q_norms[i][j] );
	}
    }
    (void)fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

void
soldump(void)	/* Print out Solid record information */
{
    int i;

    (void)fprintf(ofp, "%c ", record.s.s_id );	/* S */
    (void)fprintf(ofp, "%d ", record.s.s_type );	/* GED primitive type */
    (void)fprintf(ofp, "%.16s ", encode_name(record.s.s_name) );	/* unique name */
    (void)fprintf(ofp, "%d", record.s.s_cgtype );/* COMGEOM solid type */
    for ( i = 0; i < 24; i++ )
	(void)fprintf(ofp, " %.12e", record.s.s_values[i] ); /* parameters */
    (void)fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

void
cline_dump(void)
{
    size_t ngranules;	/* number of granules, total */
    char *name;
    struct rt_cline_internal *cli;
    struct bu_external ext;
    struct rt_db_internal intern;

    name = record.cli.cli_name;

    ngranules = 1;
    get_ext( &ext, ngranules );

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ( (rt_functab[ID_CLINE].ft_import4( &intern, &ext, id_mat, DBI_NULL, &rt_uniresource )) != 0 )  {
	fprintf(stderr, "g2asc: cline import failure\n");
	bu_exit(-1, NULL);
    }

    cli = (struct rt_cline_internal *)intern.idb_ptr;
    RT_CLINE_CK_MAGIC(cli);

    (void)fprintf(ofp, "%c ", DBID_CLINE );	/* c */
    (void)fprintf(ofp, "%.16s ", name );	/* unique name */
    (void)fprintf(ofp, "%26.20e %26.20e %26.20e ", V3ARGS( cli->v ) );
    (void)fprintf(ofp, "%26.20e %26.20e %26.20e ", V3ARGS( cli->h ) );
    (void)fprintf(ofp, "%26.20e %26.20e", cli->radius, cli->thickness );
    (void)fprintf(ofp, "\n");			/* Terminate w/ a newline */

    rt_db_free_internal(&intern);
    bu_free_external( &ext );
}

void
bot_dump(void)
{
    size_t ngranules;
    char *name;
    struct rt_bot_internal *bot;
    struct bu_external ext;
    struct rt_db_internal intern;
    size_t i;

    name = record.bot.bot_name;
    ngranules = ntohl(*(uint32_t *)record.bot.bot_nrec) + 1;
    get_ext( &ext, ngranules );

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ( (rt_functab[ID_BOT].ft_import4( &intern, &ext, id_mat, DBI_NULL, &rt_uniresource )) != 0 )  {
	fprintf(stderr, "g2asc: bot import failure\n");
	bu_exit(-1, NULL);
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    (void)fprintf(ofp, "%c ", DBID_BOT );	/* t */
    (void)fprintf(ofp, "%.16s ", name );	/* unique name */
    (void)fprintf(ofp, "%d ", bot->mode );
    (void)fprintf(ofp, "%d ", bot->orientation );
    (void)fprintf(ofp, "%d ", 0 );	/* was error_mode */
    (void)fprintf(ofp, "%lu ", (unsigned long)bot->num_vertices );
    (void)fprintf(ofp, "%lu", (unsigned long)bot->num_faces );
    (void)fprintf(ofp, "\n");

    for ( i=0; i<bot->num_vertices; i++ )
	fprintf(ofp,  "	%lu: %26.20e %26.20e %26.20e\n", (unsigned long)i, V3ARGS( &bot->vertices[i*3] ) );
    if ( bot->mode == RT_BOT_PLATE )
    {
	struct bu_vls vls;

	for ( i=0; i<bot->num_faces; i++ )
	    fprintf(ofp,  "	%lu: %d %d %d %26.20e\n", (unsigned long)i, V3ARGS( &bot->faces[i*3] ), bot->thickness[i] );
	bu_vls_init( &vls );
	bu_bitv_to_hex( &vls, bot->face_mode );
	fprintf(ofp,  "	%s\n", bu_vls_addr( &vls ) );
	bu_vls_free( &vls );
    }
    else
    {
	for ( i=0; i<bot->num_faces; i++ )
	    fprintf(ofp,  "	%lu: %d %d %d\n", (unsigned long)i, V3ARGS( &bot->faces[i*3] ) );
    }

    rt_db_free_internal(&intern);
    bu_free_external( &ext );
}

void
pipe_dump(void)	/* Print out Pipe record information */
{

    size_t ngranules;	/* number of granules, total */
    char *name;
    struct rt_pipe_internal *pipeip;		/* want a struct for the head, not a ptr. */
    struct bu_external ext;
    struct rt_db_internal intern;

    ngranules = ntohl(*(uint32_t *)record.pwr.pwr_count) + 1;
    name = record.pwr.pwr_name;

    get_ext( &ext, ngranules );

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ( (rt_functab[ID_PIPE].ft_import4( &intern, &ext, id_mat, NULL, &rt_uniresource )) != 0 )  {
	fprintf(stderr, "g2asc: pipe import failure\n");
	bu_exit(-1, NULL);
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    RT_PIPE_CK_MAGIC(pipeip);

    /* send the doubly linked list off to dump_pipe_segs(), which
     * will print all the information.
     */

    dump_pipe_segs(name, &pipeip->pipe_segs_head);

    rt_db_free_internal(&intern);
    bu_free_external( &ext );
}

void
dump_pipe_segs(char *name, struct bu_list *headp)
{

    struct wdb_pipept *sp;

    fprintf(ofp, "%c %.16s\n", DBID_PIPE, name);

    /* print parameters for each point: one point per line */

    for ( BU_LIST_FOR( sp, wdb_pipept, headp ) )  {
	fprintf(ofp,  "%26.20e %26.20e %26.20e %26.20e %26.20e %26.20e\n",
		sp->pp_id, sp->pp_od, sp->pp_bendradius, V3ARGS( sp->pp_coord ) );
    }
    fprintf(ofp,  "END_PIPE %s\n", name );
}

/*
 * Print out Particle record information.
 * Note that particles fit into one granule only.
 */
void
particle_dump(void)
{
    struct rt_part_internal 	*part;	/* head for the structure */
    struct bu_external	ext;
    struct rt_db_internal	intern;

    get_ext( &ext, 1 );

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ( (rt_functab[ID_PARTICLE].ft_import4( &intern, &ext, id_mat, NULL, &rt_uniresource )) != 0 )  {
	fprintf(stderr, "g2asc: particle import failure\n");
	bu_exit(-1, NULL);
    }

    part = (struct rt_part_internal *)intern.idb_ptr;
    RT_PART_CK_MAGIC(part);

    /* Particle type is picked up on here merely to ensure receiving
     * valid data.  The type is not used any further.
     */

    switch ( part->part_type )  {
	case RT_PARTICLE_TYPE_SPHERE:
	    break;
	case RT_PARTICLE_TYPE_CYLINDER:
	    break;
	case RT_PARTICLE_TYPE_CONE:
	    break;
	default:
	    fprintf(stderr, "g2asc: no particle type %d\n", part->part_type);
	    bu_exit(-1, NULL);
    }

    fprintf(ofp, "%c %.16s %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e\n",
	    record.part.p_id, record.part.p_name,
	    part->part_V[X],
	    part->part_V[Y],
	    part->part_V[Z],
	    part->part_H[X],
	    part->part_H[Y],
	    part->part_H[Z],
	    part->part_vrad, part->part_hrad);
}


/*			A R B N _ D U M P
 *
 *  Print out arbn information.
 *
 */
void
arbn_dump(void)
{
    size_t ngranules;	/* number of granules to be read */
    size_t i;		/* a counter */
    char *name;
    struct rt_arbn_internal *arbn;
    struct bu_external ext;
    struct rt_db_internal intern;

    ngranules = ntohl(*(uint32_t *)record.n.n_grans) + 1;
    name = record.n.n_name;

    get_ext( &ext, ngranules );

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ( (rt_functab[ID_ARBN].ft_import4( &intern, &ext, id_mat, NULL, &rt_uniresource )) != 0 )  {
	fprintf(stderr, "g2asc: arbn import failure\n");
	bu_exit(-1, NULL);
    }

    arbn = (struct rt_arbn_internal *)intern.idb_ptr;
    RT_ARBN_CK_MAGIC(arbn);

    fprintf(ofp, "%c %.16s %lu\n", 'n', name, (unsigned long)arbn->neqn);
    for ( i = 0; i < arbn->neqn; i++ )  {
	fprintf(ofp, "n %26.20e %20.26e %26.20e %26.20e\n",
		arbn->eqn[i][X], arbn->eqn[i][Y],
		arbn->eqn[i][Z], arbn->eqn[i][3]);
    }

    rt_db_free_internal(&intern);
    bu_free_external( &ext );
}


/*
 *			C O M B D U M P
 *
 *  Note that for compatability with programs such as FRED that
 *  (inappropriately) read .asc files, the member count has to be
 *  recalculated here.
 *
 *  Returns -
 *	0	converted OK
 *	1	converted OK, left next record in global "record" for reuse.
 */
int
combdump(void)	/* Print out Combination record information */
{
    int	m1, m2;		/* material property flags */
    struct bu_list	head;
    struct mchain {
	struct bu_list	l;
	union record	r;
    };
    struct mchain	*mp;
    struct mchain	*ret_mp = (struct mchain *)0;
    int		mcount;

    /*
     *  Gobble up all subsequent member records, so that
     *  an accurate count of them can be output.
     */
    BU_LIST_INIT( &head );
    mcount = 0;
    while (1)  {
	BU_GETSTRUCT( mp, mchain );
	if ( fread( (char *)&mp->r, sizeof(mp->r), 1, ifp ) != 1
	     || feof( ifp ) )
	    break;
	if ( mp->r.u_id != ID_MEMB )  {
	    ret_mp = mp;	/* Handle it later */
	    break;
	}
	BU_LIST_INSERT( &head, &(mp->l) );
	mcount++;
    }

    /*
     *  Output the combination
     */
    (void)fprintf(ofp, "%c ", record.c.c_id );		/* C */
    switch ( record.c.c_flags )  {
	case DBV4_REGION:
	    (void)fprintf(ofp, "Y ");			/* Y if `R' */
	    break;
	case DBV4_NON_REGION_NULL:
	case DBV4_NON_REGION:
	    (void)fprintf(ofp, "N ");			/* N if ` ' or '\0' */
	    break;
	case DBV4_REGION_FASTGEN_PLATE:
	    (void)fprintf(ofp, "P ");
	    break;
	case DBV4_REGION_FASTGEN_VOLUME:
	    (void)fprintf(ofp, "V ");
	    break;
    }
    (void)fprintf(ofp, "%.16s ", encode_name(record.c.c_name) );	/* unique name */
    (void)fprintf(ofp, "%d ", record.c.c_regionid );	/* region ID code */
    (void)fprintf(ofp, "%d ", record.c.c_aircode );	/* air space code */
    (void)fprintf(ofp, "%d ", mcount );       		/* DEPRECATED: # of members */
#if 1
    (void)fprintf(ofp, "%d ", 0 );			/* DEPRECATED: COMGEOM region # */
#else
    (void)fprintf(ofp, "%d ", record.c.c_num );           /* DEPRECATED: COMGEOM region # */
#endif
    (void)fprintf(ofp, "%d ", record.c.c_material );	/* material code */
    (void)fprintf(ofp, "%d ", record.c.c_los );		/* equiv. LOS est. */
    (void)fprintf(ofp, "%d %d %d %d ",
		  record.c.c_override ? 1 : 0,
		  record.c.c_rgb[0],
		  record.c.c_rgb[1],
		  record.c.c_rgb[2] );
    m1 = m2 = 0;
    if ( isascii(record.c.c_matname[0]) && isprint(record.c.c_matname[0]) )  {
	m1 = 1;
	if ( record.c.c_matparm[0] )
	    m2 = 1;
    }
    fprintf(ofp, "%d %d", m1, m2 );
    switch ( record.c.c_inherit )  {
	case DB_INH_HIGHER:
	    fprintf(ofp, " %d", DB_INH_HIGHER );
	    break;
	default:
	case DB_INH_LOWER:
	    fprintf(ofp, " %d", DB_INH_LOWER );
	    break;
    }
    (void)fprintf(ofp, "\n");			/* Terminate w/ a newline */

    if ( m1 )
	(void)fprintf(ofp, "%.32s\n", CH(record.c.c_matname) );
    if ( m2 )
	(void)fprintf(ofp, "%.60s\n", CH(record.c.c_matparm) );

    /*
     *  Output the member records now
     */
    while ( BU_LIST_WHILE( mp, mchain, &head ) )  {
	membdump( &mp->r );
	BU_LIST_DEQUEUE( &mp->l );
	bu_free( (char *)mp, "mchain");
    }

    if ( ret_mp )  {
	memcpy((char *)&record, (char *)&ret_mp->r, sizeof(record));
	bu_free( (char *)ret_mp, "mchain");
	return 1;
    }
    return 0;
}

/*
 *			M E M B D U M P
 *
 *  Print out Member record information.
 *  Intented to be called by combdump only.
 */
void
membdump(union record *rp)
{
    int i;

    (void)fprintf(ofp, "%c ", rp->M.m_id );		/* M */
    (void)fprintf(ofp, "%c ", rp->M.m_relation );	/* Boolean oper. */
    (void)fprintf(ofp, "%.16s ", encode_name(rp->M.m_instname) );	/* referred-to obj. */
    for ( i = 0; i < 16; i++ )			/* homogeneous transform matrix */
	(void)fprintf(ofp, "%.12e ", rp->M.m_mat[i] );
    (void)fprintf(ofp, "%d", 0 );			/* was COMGEOM solid # */
    (void)fprintf(ofp, "\n");				/* Terminate w/ nl */
}

void
arsadump(void)	/* Print out ARS record information */
{
    int i;
    int length;	/* Keep track of number of ARS B records */

    (void)fprintf(ofp, "%c ", record.a.a_id );	/* A */
    (void)fprintf(ofp, "%d ", record.a.a_type );	/* primitive type */
    (void)fprintf(ofp, "%.16s ", encode_name(record.a.a_name) );	/* unique name */
    (void)fprintf(ofp, "%d ", record.a.a_m );	/* # of curves */
    (void)fprintf(ofp, "%d ", record.a.a_n );	/* # of points per curve */
    (void)fprintf(ofp, "%d ", record.a.a_curlen );/* # of granules per curve */
    (void)fprintf(ofp, "%d ", record.a.a_totlen );/* # of granules for ARS */
    (void)fprintf(ofp, "%.12e ", record.a.a_xmax );	/* max x coordinate */
    (void)fprintf(ofp, "%.12e ", record.a.a_xmin );	/* min x coordinate */
    (void)fprintf(ofp, "%.12e ", record.a.a_ymax );	/* max y coordinate */
    (void)fprintf(ofp, "%.12e ", record.a.a_ymin );	/* min y coordinate */
    (void)fprintf(ofp, "%.12e ", record.a.a_zmax );	/* max z coordinate */
    (void)fprintf(ofp, "%.12e", record.a.a_zmin );	/* min z coordinate */
    (void)fprintf(ofp, "\n");			/* Terminate w/ a newline */

    length = (int)record.a.a_totlen;	/* Get # of ARS B records */

    for ( i = 0; i < length; i++ )  {
	arsbdump();
    }
}

void
arsbdump(void)	/* Print out ARS B record information */
{
    int i;
    size_t ret;

    /* Read in a member record for processing */
    ret = fread( (char *)&record, sizeof record, 1, ifp );
    if (ret != 1)
	perror("fread");
    (void)fprintf(ofp, "%c ", record.b.b_id );		/* B */
    (void)fprintf(ofp, "%d ", record.b.b_type );		/* primitive type */
    (void)fprintf(ofp, "%d ", record.b.b_n );		/* current curve # */
    (void)fprintf(ofp, "%d", record.b.b_ngranule );	/* current granule */
    for ( i = 0; i < 24; i++ )  {
	/* [8*3] vectors */
	(void)fprintf(ofp, " %.12e", record.b.b_values[i] );
    }
    (void)fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

void
materdump(void)	/* Print out material description record information */
{
    (void)fprintf(ofp,  "%c %d %d %d %d %d %d\n",
		  record.md.md_id,			/* m */
		  record.md.md_flags,			/* UNUSED */
		  record.md.md_low,	/* low end of region IDs affected */
		  record.md.md_hi,	/* high end of region IDs affected */
		  record.md.md_r,
		  record.md.md_g,		/* color of regions: 0..255 */
		  record.md.md_b );
}

void
bspldump(void)	/* Print out B-spline solid description record information */
{
    (void)fprintf(ofp,  "%c %.16s %d\n",
		  record.B.B_id,		/* b */
		  encode_name(record.B.B_name),	/* unique name */
		  record.B.B_nsurf);	/* # of surfaces in this solid */
		/*record.B.B_unused );	UNUSED (was resolution of flatness) */
}

void
bsurfdump(void)	/* Print d-spline surface description record information */
{
    size_t i;
    float *vp;
    size_t nbytes, count;
    float *fp;

    (void)fprintf(ofp,  "%c %d %d %d %d %d %d %d %d %d\n",
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
    vp = (float *)bu_calloc((unsigned int)nbytes, 1, "KNOT DATA");
    fp = vp;
    count = fread( (char *)fp, 1, nbytes, ifp );
    if ( count != nbytes )  {
	(void)fprintf(stderr, "g2asc: spline knot read failure\n");
	bu_exit(1, NULL);
    }
    /* Print the knot vector information */
    count = record.d.d_kv_size[0] + record.d.d_kv_size[1];
    for ( i = 0; i < count; i++ )  {
	(void)fprintf(ofp, "%.12e\n", *vp++);
    }
    /* Free the knot data memory */
    (void)bu_free( (char *)fp, "KNOT DATA" );

    /* Malloc and clear memory for the CONTROL MESH data and read it */
    nbytes = record.d.d_nctls * sizeof(union record);
    vp = (float *)bu_calloc((unsigned int)nbytes, 1, "CONTROL MESH");
    fp = vp;
    count = fread( (char *)fp, 1, nbytes, ifp );
    if ( count != nbytes )  {
	(void)fprintf(stderr, "g2asc: control mesh read failure\n");
	bu_exit(1, NULL);
    }
    /* Print the control mesh information */
    count = record.d.d_ctl_size[0] * record.d.d_ctl_size[1] *
	record.d.d_geom_type;
    for ( i = 0; i < count; i++ )  {
	(void)fprintf(ofp, "%.12e\n", *vp++);
    }
    /* Free the control mesh memory */
    (void)bu_free( (char *)fp, "CONTROL MESH" );
}

/*
 *			S T R C H O P
 *
 *  Take a string and a length, and null terminate,
 *  converting unprintable characters to something printable.
 */
char *strchop(char *str, size_t len)
{
    static char buf[10000] = {0};
    char *ip = str;
    char *op = buf;
    int warn = 0;
    char *ep;

    if ( len > sizeof(buf)-2 )
	len=sizeof(buf)-2;

    ep = &buf[len-1];		/* Leave room for null */
    while ( op < ep )  {
	if ( *ip == '\0' )  break;
	if ( isascii(*ip) && (isprint(*ip) || isspace(*ip)) )  {
	    *op++ = *ip++;
	}  else  {
	    *op++ = '@';
	    ip++;
	    warn = 1;
	}
    }
    *op = '\0';
    if (warn)  {
	(void)fprintf(stderr,
		      "g2asc: Illegal char in string, converted to '%s'\n",
		      buf );
    }
    if ( op == buf )  {
	/* Null input name */
	(void)fprintf(stderr,
		      "g2asc:  NULL string converted to -=STRING=-\n");
	return "-=STRING=-";
    }
    return buf;
}

void
extrdump(void)
{
    struct rt_extrude_internal	*extr;
    int				ngranules;
    char				*myname;
    struct bu_external		ext;
    struct rt_db_internal		intern;

    myname = record.extr.ex_name;
    ngranules = ntohl(*(uint32_t *)record.extr.ex_count) + 1;
    get_ext( &ext, ngranules );

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ( (rt_functab[ID_EXTRUDE].ft_import4( &intern, &ext, id_mat, DBI_NULL, &rt_uniresource )) != 0 )  {
	fprintf(stderr, "g2asc: extrusion import failure\n");
	bu_exit(-1, NULL);
    }

    extr = (struct rt_extrude_internal *)intern.idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extr);

    (void)fprintf(ofp, "%c ", DBID_EXTR );	/* e */
    (void)fprintf(ofp, "%.16s ", encode_name( myname ) );	/* unique name */
    (void)fprintf(ofp, "%.16s ", encode_name( extr->sketch_name ) );
    (void)fprintf(ofp, "%d ", extr->keypoint );
    (void)fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS( extr->V ) );
    (void)fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS( extr->h ) );
    (void)fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS( extr->u_vec ) );
    (void)fprintf(ofp, "%.12e %.12e %.12e\n", V3ARGS( extr->v_vec ) );
}

void
sketchdump(void)
{
    struct rt_sketch_internal *skt;
    size_t ngranules;
    char *myname;
    struct bu_external ext;
    struct rt_db_internal intern;
    size_t i, j;
    struct rt_curve *crv;

    myname = record.skt.skt_name;
    ngranules = ntohl(*(uint32_t *)record.skt.skt_count) + 1;
    get_ext( &ext, ngranules );

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ( (rt_functab[ID_SKETCH].ft_import4( &intern, &ext, id_mat, DBI_NULL, &rt_uniresource )) != 0 )  {
	fprintf(stderr, "g2asc: sketch import failure\n");
	bu_exit( -1, NULL );
    }

    skt = (struct rt_sketch_internal *)intern.idb_ptr;
    RT_SKETCH_CK_MAGIC( skt );
    crv = &skt->curve;
    (void)fprintf(ofp, "%c ", DBID_SKETCH ); /* d */
    (void)fprintf(ofp, "%.16s ", encode_name( myname ) );  /* unique name */
    (void)fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS( skt->V ) );
    (void)fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS( skt->u_vec ) );
    (void)fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS( skt->v_vec ) );
    (void)fprintf(ofp, "%lu %lu\n", (unsigned long)skt->vert_count, (unsigned long)crv->count );
    for ( i=0; i<skt->vert_count; i++ )
	(void)fprintf(ofp, " %.12e %.12e", V2ARGS( skt->verts[i] ) );
    (void)fprintf(ofp, "\n" );

    for ( j=0; j<crv->count; j++ )
    {
	long *lng;
	struct line_seg *lsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	int k;

	lng = (long *)crv->segment[j];
	switch ( *lng )
	{
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		(void)fprintf(ofp, "  L %d %d %d\n", crv->reverse[j], lsg->start, lsg->end );
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		(void)fprintf(ofp, "  A %d %d %d %.12e %d %d\n", crv->reverse[j], csg->start, csg->end,
			      csg->radius, csg->center_is_left, csg->orientation );
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		(void)fprintf(ofp, "  N %d %d %d %d %d\n   ", crv->reverse[j], nsg->order, nsg->pt_type,
			      nsg->k.k_size, nsg->c_size );
		for ( k=0; k<nsg->k.k_size; k++ )
		    (void)fprintf(ofp, " %.12e", nsg->k.knots[k] );
		(void)fprintf(ofp, "\n   " );
		for ( k=0; k<nsg->c_size; k++ )
		    (void)fprintf(ofp, " %d", nsg->ctl_points[k] );
		(void)fprintf(ofp, "\n" );
		break;
	}
    }
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
