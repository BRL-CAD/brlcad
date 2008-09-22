/*                       B O T _ D U M P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @file bot_dump.c
 *
 */

#include "common.h"
#include "bio.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"

#define V3ARGSIN(_a)       (_a)[X]/25.4, (_a)[Y]/25.4, (_a)[Z]/25.4

static char usage[] = "\
Usage: bot-stl [-b] [-i] [-m directory] [-o file] [-t dxf|obj|stl] [-v] geom.g\n";

enum otype {
    OTYPE_DXF = 1,
    OTYPE_OBJ,
    OTYPE_STL
};

static enum otype output_type = OTYPE_STL;
static fastf_t mm2inches = 1.0/25.4;
static int binary = 0;
static int inches = 0;
static int verbose = 0;
static char *output_file = NULL;	/* output filename */
static char *output_directory = NULL;	/* directory name to hold output files */

/* Byte swaps a four byte value */
static void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}

void
write_bot_dxf(struct rt_bot_internal *bot, FILE *fp, char *name)
{
    int num_vertices;
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
    register int i, vi;

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    for (i = 0; i < num_faces; i++) {
	vi = 3*faces[3*i];
	VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+1];
	VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+2];
	VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);

	if (inches) {
	    VSCALE(A, A, mm2inches);
	    VSCALE(B, B, mm2inches);
	    VSCALE(C, C, mm2inches);
	}

	fprintf( fp, "0\n3DFACE\n8\n%s\n62\n7\n", name );
	fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		 10, A[X], 20, A[Y], 30, A[Z] );
	fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		 11, B[X], 21, B[Y], 31, B[Z] );
	fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		 12, C[X], 22, C[Y], 32, C[Z] );
	fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
		 12, C[X], 22, C[Y], 32, C[Z] );
    }

}

void
write_bot_obj(struct rt_bot_internal *bot, FILE *fp, char *name)
{
    int num_vertices;
    fastf_t *vertices;
    int num_faces, *faces;
    register int i;

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    fprintf( fp, "g %s\n", name);

    for (i = 0; i < num_vertices; i++) {
	if (inches)
	    fprintf( fp, "v %f %f %f\n",
		     vertices[3*i]*mm2inches,
		     vertices[3*i+1]*mm2inches,
		     vertices[3*i+2]*mm2inches);
	else
	    fprintf( fp, "v %f %f %f\n",
		     vertices[3*i],
		     vertices[3*i+1],
		     vertices[3*i+2]);
    }

    for (i = 0; i < num_faces; i++) {
	fprintf( fp, "f %d %d %d\n", faces[3*i], faces[3*i+1], faces[3*i+2]);
    }
}

#define DUMP_STL_NORMALS 0

void
write_bot_stl(struct rt_bot_internal *bot, FILE *fp, char *name)
{
    int num_vertices;
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
#if DUMP_STL_NORMALS
    vect_t BmA;
    vect_t CmA;
    vect_t norm;
#endif
    register int i, vi;

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    fprintf( fp, "solid %s\n", name);
    for (i = 0; i < num_faces; i++) {
	vi = 3*faces[3*i];
	VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+1];
	VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+2];
	VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);

	if (inches) {
	    VSCALE(A, A, mm2inches);
	    VSCALE(B, B, mm2inches);
	    VSCALE(C, C, mm2inches);
	}

#if DUMP_STL_NORMALS
	VSUB2(BmA, B, A);
	VSUB2(CmA, C, A);
	if (bot->orientation != RT_BOT_CW) {
	    VCROSS(norm, BmA, CmA);
	} else {
	    VCROSS(norm, CmA, BmA);
	}
	VUNITIZE(norm);

	fprintf( fp, "  facet normal %lf %lf %lf\n", V3ARGS(norm));
#else
	fprintf( fp, "  facet\n");
#endif
	fprintf( fp, "    outer loop\n");
	fprintf( fp, "      vertex %lf %lf %lf\n", V3ARGS(A));
	fprintf( fp, "      vertex %lf %lf %lf\n", V3ARGS(B));
	fprintf( fp, "      vertex %lf %lf %lf\n", V3ARGS(C));
	fprintf( fp, "    endloop\n");
	fprintf( fp, "  endfacet\n");
    }
    fprintf( fp, "endsolid %s\n", name);
}

void
write_bot_stl_binary(struct rt_bot_internal *bot, int fd, char *name)
{
    int num_vertices;
    fastf_t *vertices;
    int num_faces, *faces;
    point_t A;
    point_t B;
    point_t C;
    vect_t BmA;
    vect_t CmA;
    vect_t norm;
    register int i, j, vi;

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;

    for (i = 0; i < num_faces; i++) {
	float flts[12];
	float *flt_ptr;
	unsigned char vert_buffer[50];

	vi = 3*faces[3*i];
	VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+1];
	VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+2];
	VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);

	VSUB2(BmA, B, A);
	VSUB2(CmA, C, A);
	if (bot->orientation != RT_BOT_CW) {
	    VCROSS(norm, BmA, CmA);
	} else {
	    VCROSS(norm, CmA, BmA);
	}
	VUNITIZE(norm);

	if (inches) {
	    VSCALE(A, A, mm2inches);
	    VSCALE(B, B, mm2inches);
	    VSCALE(C, C, mm2inches);
	}

	memset(vert_buffer, 0, sizeof( vert_buffer ));

	flt_ptr = flts;
	VMOVE(flt_ptr, norm);
	flt_ptr += 3;
	VMOVE(flt_ptr, A);
	flt_ptr += 3;
	VMOVE(flt_ptr, B);
	flt_ptr += 3;
	VMOVE(flt_ptr, C);
	flt_ptr += 3;

	htonf(vert_buffer, (const unsigned char *)flts, 12);
	for ( j=0; j<12; j++ ) {
	    lswap((unsigned int *)&vert_buffer[j*4]);
	}
	write(fd, vert_buffer, 50);
    }
}


/*
 *	M A I N
 *
 */
int
main(int argc, char *argv[])
{
    char idbuf[132];		/* First ID record info */
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct rt_i *rtip;
    struct directory *dp;
    struct bu_vls file_name;
    char *file_ext;
    FILE *fp;
    int fd;
    char c;
    mat_t mat;
    int i;
    unsigned int total_faces = 0;

    bu_optind = 1;

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "bio:m:t:v")) != EOF) {
	switch (c) {
	case 'b':		/* Binary output file */
	    binary=1;
	    break;
	case 'i':
	    inches = 1;
	    break;
	case 'm':
	    output_directory = bu_optarg;
	    bu_vls_init( &file_name );
	    break;
	case 'o':		/* Output file name. */
	    output_file = bu_optarg;
	    break;
	case 't':
	    if (!strcmp("dxf", bu_optarg))
		output_type = OTYPE_DXF;
	    else if (!strcmp("obj", bu_optarg))
		output_type = OTYPE_OBJ;
	    else if (!strcmp("stl", bu_optarg))
		output_type = OTYPE_STL;
	    else
		bu_exit(1, usage, argv[0]);
	    break;
	case 'v':
	    verbose = 1;
	    break;
	default:
	    bu_exit(1, usage, argv[0]);
	    break;
	}
    }

    if (bu_optind >= argc) {
	bu_exit(1, usage, argv[0]);
    }

    if ( output_file && output_directory ) {
	fprintf(stderr, "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n" );
	bu_exit(1, usage, argv[0] );
    }

    if ( !output_file && !output_directory ) {
	if ( binary ) {
	    bu_exit(1, "Can't output binary to stdout\n");
	}
	fp = stdout;

	/* Set this to something non-null in order to possibly write eof */
	output_file = "stdout";

	/* output DXF header and start of TABLES section */
	if (output_type == OTYPE_DXF) {
	    fprintf(fp,
		    "0\nSECTION\n2\nHEADER\n999\n%s (All Bots)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
		    argv[argc-1]);
	}
    } else if ( output_file ) {
	if ( binary && output_type == OTYPE_STL ) {
	    char buf[81];	/* need exactly 80 chars for header */

	    /* Open binary output file */
	    if ( (fd=open( output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0 )
	    {
		perror( argv[0] );
		bu_exit(1, "Cannot open binary output file (%s) for writing\n", output_file );
	    }

	    /* Write out STL header if output file is binary */
	    memset(buf, 0, sizeof( buf ));
	    if (inches) {
		bu_strlcpy( buf, "BRL-CAD generated STL FILE (Units=inches)", sizeof(buf));
	    } else {
		bu_strlcpy( buf, "BRL-CAD generated STL FILE (Units=mm)", sizeof(buf));
	    }
	    write(fd, &buf, 80);

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    write(fd, &buf, 4);
	} else {
	    /* Open ASCII output file */
	    if ( (fp=fopen( output_file, "wb+" )) == NULL )
	    {
		perror( argv[0] );
		bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", output_file );
	    }

	    /* output DXF header and start of TABLES section */
	    if (output_type == OTYPE_DXF) {
		fprintf(fp,
			"0\nSECTION\n2\nHEADER\n999\n%s (All Bots)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
			argv[argc-1]);
	    }
	}
    }

    /* Open brl-cad database */
    argc -= bu_optind;
    argv += bu_optind;

    RT_INIT_DB_INTERNAL(&intern);
    if ((rtip=rt_dirbuild(argv[0], idbuf, sizeof(idbuf)))==RTI_NULL) {
	fprintf(stderr, "rtexample: rt_dirbuild failure\n");
	return 2;
    }

    MAT_IDN(mat);

    if (output_directory) {
	switch (output_type) {
	case OTYPE_DXF:
	    file_ext = ".dxf";
	    break;
	case OTYPE_OBJ:
	    file_ext = ".obj";
	    break;
	case OTYPE_STL:
	default:
	    file_ext = ".stl";
	    break;
	}
    }

    /* dump all the bots */
    FOR_ALL_DIRECTORY_START(dp, rtip->rti_dbip)

    /* we only dump BOT primitives, so skip some obvious exceptions */
    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
    if (dp->d_flags & DIR_COMB) continue;

    /* get the internal form */
    i=rt_db_get_internal(&intern, dp, rtip->rti_dbip, mat, &rt_uniresource);

    if (i < 0) {
	fprintf(stderr, "rt_get_internal failure %d on %s\n", i, dp->d_namep);
	continue;
    }

    if (i != ID_BOT) {
	continue;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;

    if (output_directory) {
	char *cp;

	bu_vls_trunc( &file_name, 0 );
	bu_vls_strcpy( &file_name, output_directory );
	bu_vls_putc( &file_name, '/' );
	cp = dp->d_namep;
	cp++;
	while ( *cp != '\0' ) {
	    if ( *cp == '/' ) {
		bu_vls_putc( &file_name, '@' );
	    } else if ( *cp == '.' || isspace( *cp ) ) {
		bu_vls_putc( &file_name, '_' );
	    } else {
		bu_vls_putc( &file_name, *cp );
	    }
	    cp++;
	}
	bu_vls_strcat( &file_name, file_ext);

	if ( binary && output_type == OTYPE_STL ) {
	    char buf[81];	/* need exactly 80 chars for header */
	    unsigned char tot_buffer[4];

	    if ((fd=open(bu_vls_addr(&file_name), O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		perror(bu_vls_addr(&file_name));
		bu_exit(1, "Cannot open binary output file (%s) for writing\n", bu_vls_addr(&file_name));
	    }

	    /* Write out STL header */
	    memset(buf, 0, sizeof( buf ));
	    if (inches) {
		bu_strlcpy( buf, "BRL-CAD generated STL FILE (Units=inches)", sizeof(buf));
	    } else {
		bu_strlcpy( buf, "BRL-CAD generated STL FILE (Units=mm)", sizeof(buf));
	    }
	    write(fd, &buf, 80);

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    write(fd, &buf, 4);

	    write_bot_stl_binary(bot, fd, dp->d_namep);

	    /* Re-position pointer to 80th byte */
	    lseek( fd, 80, SEEK_SET );

	    /* Write out number of triangles */
	    bu_plong( tot_buffer, (unsigned long)bot->num_faces );
	    lswap( (unsigned int *)tot_buffer );
	    write(fd, tot_buffer, 4);

	    close(fd);
	} else {
	    if ((fp=fopen( bu_vls_addr(&file_name), "wb+")) == NULL) {
		perror(bu_vls_addr(&file_name));
		bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(&file_name));
	    }

	    switch (output_type) {
	    case OTYPE_DXF:
		fprintf(fp,
			"0\nSECTION\n2\nHEADER\n999\n%s (BOT from %s)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
			dp->d_namep, argv[argc-1]);
		write_bot_dxf(bot, fp, dp->d_namep);
		fprintf( fp, "0\nENDSEC\n0\nEOF\n" );
		break;
	    case OTYPE_OBJ:
		write_bot_obj(bot, fp, dp->d_namep);
		break;
	    case OTYPE_STL:
	    default:
		write_bot_stl(bot, fp, dp->d_namep);
		break;
	    }

	    fclose(fp);
	}
    } else {
	if ( binary && output_type == OTYPE_STL ) {
	    total_faces += bot->num_faces;
	    write_bot_stl_binary(bot, fd, dp->d_namep);
	} else {
	    switch (output_type) {
	    case OTYPE_DXF:
		write_bot_dxf(bot, fp, dp->d_namep);
		break;
	    case OTYPE_OBJ:
		write_bot_obj(bot, fp, dp->d_namep);
		break;
	    case OTYPE_STL:
	    default:
		write_bot_stl(bot, fp, dp->d_namep);
		break;
	    }
	}
    }


    FOR_ALL_DIRECTORY_END

    if (output_file) {
	if ( binary && output_type == OTYPE_STL ) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek( fd, 80, SEEK_SET );

	    /* Write out number of triangles */
	    bu_plong( tot_buffer, (unsigned long)total_faces );
	    lswap( (unsigned int *)tot_buffer );
	    write(fd, tot_buffer, 4);
	    close(fd);
	} else {
	    /* end of layers section, start of ENTITIES SECTION */
	    if (output_type == OTYPE_DXF)
		fprintf( fp, "0\nENDSEC\n0\nEOF\n" );

	    fclose(fp);
	}
    }

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
