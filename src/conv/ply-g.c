/*                         P L Y - G . C
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
 *
 */
/** @file ply-g.c
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  ifdef HAVE_SYS_UNISTD_H
#    include <sys/unistd.h>
#  endif
#endif
#include <ctype.h>

/* interface headers */
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


static FILE *ply_fp=NULL;
static struct rt_wdb *out_fp=NULL;
static char *ply_file;
static char *brlcad_file;
static fastf_t scale_factor=1000.0;	/* default units are meters */
static int ply_file_type=0;
static int verbose=0;
static int endianness;

static char *usage="Usage:\n\tply-g [-s scale_factor] [-d] [-v] input_file.ply output_file.g";

#define LINELEN		512

static char line[LINELEN];

#define PLY_ASCII		1
#define PLY_BIN_BIG_ENDIAN	2
#define PLY_BIN_LITTLE_ENDIAN	3

#define TYPE_CHAR	0
#define TYPE_UCHAR	1
#define TYPE_SHORT	2
#define TYPE_USHORT	3
#define TYPE_INT	4
#define TYPE_UINT	5
#define TYPE_FLOAT	6
#define TYPE_DOUBLE	7

static char *types[]={
	"char",
	"uchar",
	"short",
	"ushort",
	"int",
	"uint",
	"float",
	"double",
	NULL };

static char *types2[]={
	"int8",
	"uint8",
	"int16",
	"uint16",
	"int32",
	"uint32",
	"float32",
	"float64",
	NULL };


static int length[]={
	1,
	1,
	2,
	2,
	4,
	4,
	4,
	8,
	0 };

struct prop {
	int type;
	int index_type;		/* only used for lists */
	int list_type;		/* only used for lists */
	char *name;
	struct prop *next;
};
#define PROP_LIST_TYPE	-1

struct element {
	int type;
	int count;
	struct prop *props;
	struct element *next;
};


#define	ELEMENT_VERTEX	1
#define ELEMENT_FACE	2
#define	ELEMENT_OTHER	3

static struct element *root=NULL;

static int cur_vertex=-1;
static int cur_face=-1;

void
log_elements()
{
	struct element *elemp;
	struct prop *p;

	elemp = root;

	while( elemp ) {
		bu_log( "element:\n" );
		switch( elemp->type ) {
		case ELEMENT_VERTEX:
			bu_log( "\t%d vertices\n", elemp->count );
			break;
		case ELEMENT_FACE:
			bu_log( "\t%d faces\n", elemp->count );
			break;
		case ELEMENT_OTHER:
			bu_log( "\t%d others\n", elemp->count );
			break;
		}
		p = elemp->props;
		while( p ) {
			if( p->type == PROP_LIST_TYPE ) {
				bu_log( "\t\tproperty (%s) is a list\n", p->name );
			} else {
				bu_log( "\t\tproperty (%s) is a %s\n", p->name, types[p->type] );
			}
			p = p->next;
		}
		elemp = elemp->next;
	}
}


int
get_endianness()
{
	int i = 0x87654321;
	unsigned char* a = (unsigned char*) &i;

	if(*a != 0x21) {
		/* big Endian */
		return PLY_BIN_BIG_ENDIAN;
	} else {
		return PLY_BIN_LITTLE_ENDIAN;
	}
}

/* Byte swaps a 4-byte val */
void lswap4(unsigned int *in, unsigned int *out)
{
	unsigned int r;

	r = *in;
	*out = ((r & 0xff) << 24) |
	       ((r & 0xff00) << 8) |
	       ((r & 0xff0000) >> 8) |
	       ((r & 0xff000000) >> 24);
}

/* Byte swaps an 8-byte val */
void lswap8(unsigned long long *in, unsigned long long *out )
{
	long long r;

	r = *in;
	*out = ((r & 0xffLL) << 56) |
	       ((r & 0xff00LL) << 40) |
	       ((r & 0xff0000LL) << 24) |
	       ((r & 0xff000000LL) << 8) |
	       ((r & 0xff00000000LL) >> 8) |
	       ((r & 0xff0000000000LL) >> 24) |
	       ((r & 0xff000000000000LL) >> 40) |
	       ((r & 0xff00000000000000LL) >> 56);
}

void
skip( int type )
{
	char buf[16];
	double val_double;

	if( ply_file_type != PLY_ASCII ) {
		if( !fread( buf, 1, length[type], ply_fp ) ) {
			bu_log( "Unexpected EOF while reading data\n" );
			exit( 1 );
		}
	} else {
		if( fscanf( ply_fp, "%lf", &val_double ) != 1 ) {
			bu_log( "ERROR parsing data\n" );
			exit( 1 );
		}
	}
}

double
get_double( int type )
{
	unsigned char buf1[16], buf2[16];
	char val_char;
	unsigned char val_uchar;
	short val_short;
	unsigned short val_ushort;
	int val_int;
	unsigned int val_uint;
	float val_float;
	double val_double;
	double val;

	if( ply_file_type == PLY_ASCII ) {
		switch( type ) {
		case TYPE_CHAR:
			if( fscanf( ply_fp, " %c", &val_char ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_char;
			break;
		case TYPE_UCHAR:
			if( fscanf( ply_fp, " %c", &val_uchar ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_uchar;
			break;
		case TYPE_SHORT:
			if( fscanf( ply_fp, "%hd", &val_short ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_short;
			break;
		case TYPE_USHORT:
			if( fscanf( ply_fp, "%hu", &val_ushort ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_ushort;
			break;
		case TYPE_INT:
			if( fscanf( ply_fp, "%d", &val_int ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_int;
			break;
		case TYPE_UINT:
			if( fscanf( ply_fp, "%u", &val_uint ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_uint;
			break;
		case TYPE_FLOAT:
			if( fscanf( ply_fp, "%f", &val_float ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_float;
			break;
		case TYPE_DOUBLE:
			if( fscanf( ply_fp, "%lf", &val_double ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_double;
			break;
		}
	} else {
		if( !fread( buf1, 1, length[type], ply_fp ) ) {
			bu_log( "Unexpected EOF while reading data\n" );
			exit( 1 );
		}
		if( ply_file_type != endianness ) {
			/* need to swap bytes */
			switch( length[type] ) {
			case 1:
				/* just copy to buf2 */
				buf2[0] = buf1[0];
				break;
			case 2:
				swab( buf1, buf2, 2 );
				break;
			case 4:
				lswap4( (unsigned int *)buf1, (unsigned int *)buf2 );
				break;
			case 8:
				lswap8( (unsigned long long *)buf1, (unsigned long long *)buf2 );
				break;
			default:
				bu_log( "Property has illegal length (%d)!!!\n", length[type] );
				exit( 1 );
				break;
			}
		} else {
			/* just copy to buf2 */
			bcopy( buf1, buf2, length[type] );
		}
		switch( type ) {
		case TYPE_CHAR:
			val = *((char *)buf2);
			break;
		case TYPE_UCHAR:
			val = *((unsigned char *)buf2);
			break;
		case TYPE_SHORT:
			val = *((short *)buf2);
			break;
		case TYPE_USHORT:
			val = *((unsigned short *)buf2);
			break;
		case TYPE_INT:
			val = *((int *)buf2 );
			break;
		case TYPE_UINT:
			val = *((unsigned int *)buf2);
			break;
		case TYPE_FLOAT:
			val = *((float *)buf2);
			break;
		case TYPE_DOUBLE:
			val = *((double *)buf2);
			break;
		}
	}

	return( val*scale_factor );
}

int
get_int( int type )
{
	unsigned char buf1[16], buf2[16];
	int val_int;
	unsigned int val_uint;
	double val_double;
	int val;


	if( ply_file_type == PLY_ASCII ) {
		switch( type ) {
		case TYPE_CHAR:
		case TYPE_UCHAR:
		case TYPE_SHORT:
		case TYPE_USHORT:
		case TYPE_INT:
			if( fscanf( ply_fp, "%d", &val_int ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_int;
			break;
		case TYPE_UINT:
			if( fscanf( ply_fp, "%u", &val_uint ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_uint;
			break;
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
			if( fscanf( ply_fp, "%lf", &val_double ) != 1 ) {
				bu_log( "ERROR parsing data\n" );
				exit( 1 );
			}
			val = val_double;
			break;
		}
	} else {
		if( !fread( buf1, 1, length[type], ply_fp ) ) {
			bu_log( "Unexpected EOF while reading data\n" );
			exit( 1 );
		}
		if( ply_file_type != endianness ) {
			/* need to swap bytes */
			switch( length[type] ) {
			case 1:
				/* just copy to buf2 */
				buf2[0] = buf1[0];
				break;
			case 2:
				swab( buf1, buf2, 2 );
				break;
			case 4:
				lswap4( (unsigned int *)buf1, (unsigned int *)buf2 );
				break;
			case 8:
				lswap8( (unsigned long long *)buf1, (unsigned long long *)buf2 );
				break;
			default:
				bu_log( "Property has illegal length (%d)!!!\n", length[type] );
				exit( 1 );
				break;
			}
		} else {
			/* just copy to buf2 */
			bcopy( buf1, buf2, length[type] );
		}
		switch( type ) {
		case TYPE_CHAR:
			val = *((char *)buf2);
			break;
		case TYPE_UCHAR:
			val = *((unsigned char *)buf2);
			break;
		case TYPE_SHORT:
			val = *((short *)buf2);
			break;
		case TYPE_USHORT:
			val = *((unsigned short *)buf2);
			break;
		case TYPE_INT:
			val = *((int *)buf2 );
			break;
		case TYPE_UINT:
			val = *((unsigned int *)buf2);
			break;
		case TYPE_FLOAT:
			val = *((float *)buf2);
			break;
		case TYPE_DOUBLE:
			val = *((double *)buf2);
			break;
		}
	}

	return( val );
}

struct element *
new_element(char *str)
{
	struct element *ptr;
	char *c;

	if( verbose ) {
		bu_log( "Creating a new element structure\n" );
	}

	ptr = (struct element *)bu_calloc( 1, sizeof( struct element ), "element" );

	if( root ) {
		struct element *ptr2;

		ptr2 = root;
		while( ptr2->next != NULL ) {
			ptr2 = ptr2->next;
		}

		ptr2->next = ptr;
	} else {
		if( verbose ) {
			bu_log( "This element will be the root\n" );
		}
		root = ptr;
	}

	c = strtok( str, " \t" );
	if( !c ) {
		bu_log( "Error parsing line %s\n", line );
		bu_free( (char *)ptr, "element" );
		return( (struct element *)NULL );
	}

	if( !strncmp( c, "vertex", 6 ) ) {
		/* this is a vertex element */
		ptr->type = ELEMENT_VERTEX;
	} else if( !strncmp( c, "face", 4 ) ) {
		/* this ia a face element */
		ptr->type = ELEMENT_FACE;
	} else {
		ptr->type = ELEMENT_OTHER;
	}

	c = strtok( (char *)NULL, " \t" );
	if( !c ) {
		bu_log( "Error parsing line %s\n", line );
		bu_free( (char *)ptr, "element" );
		return( (struct element *)NULL );
	}

	ptr->count = atoi( c );

	return( ptr );
}

int
get_property( struct element *ptr )
{
	char *c;
	char *tmp_buf;
	struct prop *p;
	int i;

	if( !ptr->props ) {
		ptr->props = (struct prop *)bu_calloc( 1, sizeof( struct prop ), "property" );
		p = ptr->props;
	} else {
		p = ptr->props;
		while( p->next ) {
			p = p->next;
		}
		p->next = (struct prop *)bu_calloc( 1, sizeof( struct prop ), "property" );
		p = p->next;
	}

	tmp_buf = bu_strdup( line );

	c = strtok( tmp_buf, " \t" );
	if( c ) {
		if( strcmp( c, "property" ) ) {
			bu_log( "get_property called for non-property, line = %s\n", line );
			exit( 1 );
		}
	} else {
		bu_log( "Unexpected EOL while parsing property, line = %s\n", line );
		exit( 1 );
	}

	c = strtok( (char *)NULL, " \t" );
	if( !c ) {
		bu_log( "Unexpected EOL while parsing property, line = %s\n", line );
		exit( 1 );
	}

	if( !strcmp( c, "list" ) ) {
		if( verbose ) {
			bu_log( "\tfound a list\n" );
		}

		p->type = PROP_LIST_TYPE;

		c = strtok( (char *)NULL, " \t" );
		if( !c ) {
			bu_log( "Unexpected EOL while parsing property, line = %s\n", line );
			exit( 1 );
		}
		i = 0;
		while( types[i] ) {
			if( !strcmp( c, types[i] ) || !strcmp( c, types2[i] ) ) {
				p->index_type = i;
				break;
			}
			i++;
		}

		if( !types[i] ) {
			bu_log( "Cannot find property type for line %s\n", line );
			exit( 1 );
		}


		c = strtok( (char *)NULL, " \t" );
		if( !c ) {
			bu_log( "Unexpected EOL while parsing property, line = %s\n", line );
			exit( 1 );
		}
		i = 0;
		while( types[i] ) {
			if( !strcmp( c, types[i] ) || !strcmp( c, types2[i] ) ) {
				p->list_type = i;
				break;
			}
			i++;
		}

		if( !types[i] ) {
			bu_log( "Cannot find property type for line %s\n", line );
			bu_log( "type = %s\n", c );
			exit( 1 );
		}


	} else {
		i = 0;
		while( types[i] ) {
			if( !strcmp( c, types[i] ) || !strcmp( c, types2[i] ) ) {
				p->type = i;
				break;
			}
			i++;
		}

		if( !types[i] ) {
			bu_log( "Cannot find property type for line %s\n", line );
			bu_log( "type = %s\n", c );
			exit( 1 );
		}
	}

	c = strtok( (char *)NULL, " \t" );
	if( !c ) {
		bu_log( "Unexpected EOL while parsing property, line = %s\n", line );
		exit( 1 );
	}

	p->name = bu_strdup( c );

	bu_free( tmp_buf, "copy of line" );
	return( 0 );
}

int
read_ply_header()
{

	if( verbose ) {
		bu_log( "Reading header...\n" );
	}
	if( fgets( line, LINELEN, ply_fp ) == NULL ) {
		bu_log( "Unexpected EOF in input file!!!\n" );
		return( 1 );
	}
	if( strncmp( line, "ply", 3 ) ) {
		bu_log( "Input file does not appear to be a PLY file!!!\n" );
		return( 1 );
	}
	while( fgets( line, LINELEN, ply_fp ) ) {
		struct element *elem_ptr;
		int len;

		len = strlen( line );
		len--;
		while( len && isspace( line[len] ) ) {
			line[len] = '\0';
			len--;
		}

		if( verbose ) {
			bu_log( "Processing line:%s\n", line );
		}

		if( !strncmp( line, "end_header", 10 ) ) {
			if( verbose ) {
				bu_log( "Found end of header\n" );
			}
			break;
		}

		if( !strncmp( line, "comment", 7 ) ) {
			/* comment */
			bu_log( "%s\n", line );
			continue;
		}

		if( !strncmp( line, "format", 6 ) ) {
			/* format specification */
			if( !strncmp( &line[7], "ascii", 5 ) ) {
				ply_file_type = PLY_ASCII;
			} else if( !strncmp( &line[7], "binary_big_endian", 17 ) ) {
				ply_file_type = PLY_BIN_BIG_ENDIAN;
			} else if( !strncmp( &line[7], "binary_little_endian", 20 ) ) {
				ply_file_type = PLY_BIN_LITTLE_ENDIAN;
			} else {
				bu_log( "Unrecognized PLY format:%s\n", line );
				return( 1 );
			}

			if( verbose ) {
				switch( ply_file_type ) {
				case PLY_ASCII:
					bu_log( "This is an ASCII PLY file\n" );
					break;
				case PLY_BIN_BIG_ENDIAN:
					bu_log( "This is a binary big endian PLY file\n" );
					break;
				case PLY_BIN_LITTLE_ENDIAN:
					bu_log( "This is a binary little endian PLY file\n" );
					break;
				}
			}
		} else if( !strncmp( line, "element", 7 ) ) {
			/* found an element description */
			if( verbose ) {
				bu_log( "Found an element\n" );
			}
			elem_ptr = new_element( &line[8] );
		} else if( !strncmp( line, "property", 8 ) ) {
			if( !elem_ptr ) {
				bu_log( "Encountered \"property\" before \"element\"\n" );
				return( 1 );
			}
			if( get_property( elem_ptr ) ) {
				return( 1 );
			}
		}
	}

	return 0;
}

int
read_ply_data( struct rt_bot_internal *bot )
{
	struct element *elem_ptr;
	struct prop *p;
	int i;

	elem_ptr = root;
	while( elem_ptr ) {
		for( i=0 ; i<elem_ptr->count ; i++ ) {
			switch( elem_ptr->type ) {
			case ELEMENT_VERTEX:
				cur_vertex++;
				p = elem_ptr->props;
				while( p ) {
					if( !strcmp( p->name, "x" ) ) {
						bot->vertices[cur_vertex*3] = get_double(p->type );
					} else if( !strcmp( p->name, "y" ) ) {
						bot->vertices[cur_vertex*3+1] = get_double(p->type );
					} else if( !strcmp( p->name, "z" ) ) {
						bot->vertices[cur_vertex*3+2] = get_double(p->type );
					} else {
						skip( p->type );
					}
					p = p->next;
				}
				break;
			case ELEMENT_FACE:
				cur_face++;
				p = elem_ptr->props;
				while( p ) {
					if( p->type == PROP_LIST_TYPE ) {
						int vcount;
						int index;
						int v[4];

						vcount = get_int( p->index_type );

						if( vcount < 3 || vcount > 4) {
							bu_log( "ignoring face with %d vertices\n", vcount );
							for( index=0 ; index < vcount ; index++ ) {
								skip( p->list_type );
							}
							continue;
						}

						for( index=0 ; index < vcount ; index++ ) {
							v[index] = get_int( p->list_type );
							bot->faces[cur_face*3+index] = v[index];
						}

						if( vcount == 4 ) {
							/* need to break this into two BOT faces */
							bot->num_faces++;
							bot->faces = (int *)bu_realloc( bot->faces,
											bot->num_faces * 3 * sizeof( int ),
											"bot_faces" );
							cur_face++;
							/* bot->faces[cur_face*3] was already set above when index == 4 */
							bot->faces[cur_face*3+1] = v[0];
							bot->faces[cur_face*3+2] = v[2];
						}

					}
					p = p->next;
				}
				break;
			}

		}
		elem_ptr = elem_ptr->next;
	}

	return( 0 );
}

int
main( int argc, char *argv[] )
{
	struct rt_bot_internal *bot;
	struct element *elem_ptr;
	int c;

	/* get command line arguments */
	while ((c = getopt(argc, argv, "dvs:")) != EOF)
	{
		switch( c )
		{
		        case 's':	/* scale factor */
				scale_factor = atof( optarg );
				if( scale_factor < SQRT_SMALL_FASTF ) {
					bu_log( "scale factor too small\n" );
					bu_log( "%s\n", usage );
					exit( 1 );
				}
				break;
		        case 'd':	/* debug */
				bu_debug = BU_DEBUG_COREDUMP;
				break;
			case 'v':	/* verbose */
				verbose = 1;
				break;
			default:
				bu_log( "%s\n", usage );
				break;
		}
	}

	if( argc - optind < 2 ) {
		bu_log( "%s\n",usage );
		exit( 1 );
	}

	ply_file = argv[optind++];
	brlcad_file = argv[optind];

	if( (out_fp = wdb_fopen( brlcad_file )) == NULL ) {
		bu_log( "Cannot open output file (%s)\n", brlcad_file );
		perror( brlcad_file );
		bu_bomb( "Cannot open output file\n" );
	}

	if( (ply_fp=fopen( ply_file, "r")) == NULL ) {
		bu_log( "Cannot open PLY file (%s)\n", ply_file );
		perror( ply_file );
		bu_bomb( "Cannot open PLY file\n" );
	}

	endianness = get_endianness();
	if( verbose ) {
		if( endianness == PLY_BIN_BIG_ENDIAN ) {
			bu_log( "This machine is BigEndian\n" );
		} else {
			bu_log( "This machine is LittleEndian\n" );
		}
	}

	/* read header */
	if( read_ply_header() ) {
		exit( 1 );
	}

	if( verbose ) {
		log_elements();
	}

	/* malloc BOT storage */
	bot = (struct rt_bot_internal *)bu_calloc( 1, sizeof( struct rt_bot_internal ), "BOT" );
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SURFACE;
	bot->orientation = RT_BOT_UNORIENTED;
	bot->num_vertices = 0;
	bot->num_faces = 0;

	elem_ptr = root;
	while( elem_ptr ) {
		switch( elem_ptr->type ) {
		case ELEMENT_VERTEX:
			bot->num_vertices += elem_ptr->count;
			break;
		case ELEMENT_FACE:
			bot->num_faces += elem_ptr->count;
			break;
		}
		elem_ptr = elem_ptr->next;
	}

	if( bot->num_faces < 1 || bot->num_vertices < 1 ) {
		bu_log( "This PLY file appears to contain no geometry!!!\n" );
		return( 0 );
	}

	bot->faces = (int *)bu_calloc( bot->num_faces * 3, sizeof( int ), "bot faces" );
	bot->vertices = (fastf_t *)bu_calloc( bot->num_vertices * 3, sizeof( fastf_t ), "bot vertices" );

	read_ply_data( bot );

	wdb_export( out_fp, "ply_bot", (genptr_t)bot, ID_BOT, 1.0 );

	return( 0 );
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
