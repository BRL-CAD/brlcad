/*
 *			D B _ P A T H . C
 *
 *  Routines to manipulate "db_full_path" structures
 *
 * Functions -
 *
 *
 *  Authors -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#include "./debug.h"

/*
 *			D B _ F U L L _ P A T H _ I N I T
 */
void
db_full_path_init( struct db_full_path *pathp )
{
	pathp->fp_len = 0;
	pathp->fp_maxlen = 0;
	pathp->fp_names = (struct directory **)NULL;
	pathp->magic = DB_FULL_PATH_MAGIC;
}

/*
 *			D B _ A D D _ N O D E _ T O _ F U L L _ P A T H
 */
void
db_add_node_to_full_path( struct db_full_path *pp, struct directory *dp )
{
	RT_CK_FULL_PATH( pp );

	if( pp->fp_maxlen <= 0 )  {
		pp->fp_maxlen = 32;
		pp->fp_names = (struct directory **)bu_malloc(
			pp->fp_maxlen * sizeof(struct directory *),
			"db_full_path array");
	} else if( pp->fp_len >= pp->fp_maxlen )  {
		pp->fp_maxlen *= 4;
		pp->fp_names = (struct directory **)rt_realloc(
			(char *)pp->fp_names,
			pp->fp_maxlen * sizeof(struct directory *),
			"enlarged db_full_path array");
	}
	pp->fp_names[pp->fp_len++] = dp;
}

/*
 *			D B _ D U P _ F U L L _ P A T H
 */
void
db_dup_full_path( newp, oldp )
register struct db_full_path		*newp;
register const struct db_full_path	*oldp;
{
	RT_CK_FULL_PATH(newp);
	RT_CK_FULL_PATH(oldp);

	newp->fp_maxlen = oldp->fp_maxlen;
	newp->fp_len = oldp->fp_len;
	if( oldp->fp_len <= 0 )  {
		newp->fp_names = (struct directory **)0;
		return;
	}
	newp->fp_names = (struct directory **)bu_malloc(
		newp->fp_maxlen * sizeof(struct directory *),
		"db_full_path array (duplicate)" );
	bcopy( (char *)oldp->fp_names, (char *)newp->fp_names,
		newp->fp_len * sizeof(struct directory *) );
}

/*
 *			D B _ E X T E N D _ F U L L _ P A T H
 *
 *  Extend "pathp" so that it can grow from current fp_len by incr more names.
 *
 *  This is intended primarily as an internal method.
 */
void
db_extend_full_path( struct db_full_path *pathp, int incr )
{
	int		newlen;

	RT_CK_FULL_PATH(pathp);

	if( pathp->fp_maxlen <= 0 )  {
		pathp->fp_len = 0;
		pathp->fp_maxlen = incr;
		pathp->fp_names = (struct directory **)bu_malloc(
			pathp->fp_maxlen * sizeof(struct directory *),
			"empty fp_names extension" );
		return;
	}

	newlen = pathp->fp_len + incr;
	if( pathp->fp_maxlen < newlen )  {
		pathp->fp_maxlen = newlen+1;
		pathp->fp_names = (struct directory **)rt_realloc(
			(char *)pathp->fp_names,
			pathp->fp_maxlen * sizeof(struct directory *),
			"fp_names extension" );
	}
}

/*
 *			D B _ A P P E N D _ F U L L _ P A T H
 */
void
db_append_full_path( struct db_full_path *dest, const struct db_full_path *src )
{
	RT_CK_FULL_PATH(dest);
	RT_CK_FULL_PATH(src);

	db_extend_full_path( dest, src->fp_len );
	bcopy( (char *)&src->fp_names[0],
		(char *)&dest->fp_names[dest->fp_len],
		src->fp_len * sizeof(struct directory *) );
	dest->fp_len += src->fp_len;
}

/*
 *			D B _ D U P _ P A T H _ T A I L
 *
 *  Dup old path from starting index to end.
 */
void
db_dup_path_tail( newp, oldp, start )
register struct db_full_path		*newp;
register const struct db_full_path	*oldp;
int					start;
{
	RT_CK_FULL_PATH(newp);
	RT_CK_FULL_PATH(oldp);

	if( start < 0 || start > oldp->fp_len-1 )  rt_bomb("db_dup_path_tail: start offset out of range\n");

	newp->fp_maxlen = newp->fp_len = oldp->fp_len - start;
	if( newp->fp_len <= 0 )  {
		newp->fp_names = (struct directory **)0;
		return;
	}
	newp->fp_names = (struct directory **)bu_malloc(
		newp->fp_maxlen * sizeof(struct directory *),
		"db_full_path array (duplicate)" );
	bcopy( (char *)&oldp->fp_names[start], (char *)newp->fp_names,
		newp->fp_len * sizeof(struct directory *) );
}

/*
 *			D B _ P A T H _ T O _ S T R I N G
 *
 *  Unlike rt_path_str(), this version can be used in parallel.
 *  Caller is responsible for freeing the returned buffer.
 */
char *
db_path_to_string( const struct db_full_path *pp )
{
	register char	*cp;
	char	*buf;
	int	len;
	int	i;

	RT_CK_FULL_PATH( pp );

	len = 3;	/* leading slash, trailing null, spare */
	for( i=pp->fp_len-1; i >= 0; i-- )  {
		if( pp->fp_names[i] )
			len += strlen( pp->fp_names[i]->d_namep ) + 1;
		else
			len += 16;
	}

	buf = bu_malloc( len, "pathname string" );
	cp = buf;

	for( i=0; i < pp->fp_len; i++ )  {
		*cp++ = '/';
		if( pp->fp_names[i] )
			strcpy( cp, pp->fp_names[i]->d_namep );
		else
			strcpy( cp, "**NULL**" );
		cp += strlen( cp );
	}
	*cp++ = '\0';
	return buf;
}

/*
 *			D B _ P A T H _ T O _ V L S
 *
 *  Append a string representation of the path onto the vls.
 *  Must have exactly the same formattting conventions as
 *  db_path_to_string().
 */
void
db_path_to_vls( struct bu_vls *str, const struct db_full_path *pp )
{
	register int i;

	BU_CK_VLS(str);
	RT_CK_FULL_PATH( pp );
	
	for( i=0; i < pp->fp_len; i++ )  {
		bu_vls_putc( str, '/' );
		if( pp->fp_names[i] )
			bu_vls_strcat( str, pp->fp_names[i]->d_namep );
		else
			bu_vls_strcat( str, "**NULL**" );
	}
}

/*
 *			D B _ P R _ F U L L _ P A T H
 */
void
db_pr_full_path( const char *msg, const struct db_full_path *pathp )
{
	char	*sofar = db_path_to_string(pathp);

	bu_log("%s %s\n", msg, sofar );
	bu_free(sofar, "path string");
}

/*
 *			D B _ S T R I N G _ T O _ P A T H
 *
 *  Reverse the effects of db_path_to_string().
 *
 *  The db_full_path structure will be initialized.  If it was already in use,
 *  user should call db_free_full_path() first.
 *
 *  Returns -
 *	-1	One or more components of path did not exist in the directory.
 *	 0	OK
 */
int
db_string_to_path(struct db_full_path *pp, const struct db_i *dbip, const char *str)
{
	register char		*cp;
	register char		*slashp;
	struct directory	*dp;
	char			*copy;
	int			nslash = 0;
	int			ret = 0;
	int			len;

	RT_CK_DBI(dbip);

	/* Count slashes */
	while( *str == '/' )  str++;	/* strip off leading slashes */
	if( *str == '\0' )  {
		/* Path of a lone slash */
		db_full_path_init( pp );
		return 0;
	}

	copy = bu_strdup( str );

	/* eliminate a a trailing slash */
	len = strlen( copy );
	if( copy[len-1] == '/' )
		copy[len-1] = '\0';

	cp = copy;
	while( *cp )  {
		if( (slashp = strchr( cp, '/' )) == NULL )  break;
		nslash++;
		cp = slashp+1;
	}

	/* Make a path structure just big enough */
	pp->magic = DB_FULL_PATH_MAGIC;
	pp->fp_maxlen = pp->fp_len = nslash+1;
	pp->fp_names = (struct directory **)bu_malloc(
		pp->fp_maxlen * sizeof(struct directory *),
		"db_string_to_path path array" );


	/* Build up path array */
	cp = copy;
	nslash = 0;
	while( *cp )  {
		if( (slashp = strchr( cp, '/' )) == NULL )  {
			/* Last element of string, has no trailing slash */
			slashp = cp + strlen(cp) - 1;
		} else {
			*slashp = '\0';
		}
		if( (dp = db_lookup( dbip, cp, LOOKUP_NOISY )) == DIR_NULL )  {
			bu_log("db_string_to_path() of '%s' failed on '%s'\n",
				str, cp );
			ret = -1;	/* FAILED */
			/* Fall through, storing null dp in this location */
		}
		pp->fp_names[nslash++] = dp;
		cp = slashp+1;
	}
	BU_ASSERT_LONG( nslash, ==, pp->fp_len );
	bu_free( copy, "db_string_to_path() duplicate string");
	return ret;
}

/*
 *			D B _ A R G V _ T O _ P A T H
 *
 *  Treat elements from argv[0] to argv[argc-1] as a path specification.
 *
 *  The path structure will be fully initialized.  If it was already in use,
 *  user should call db_free_full_path() first.
 *
 *  Returns -
 *	-1	One or more components of path did not exist in the directory.
 *	 0	OK
 */
int
db_argv_to_path( pp, dbip, argc, argv )
register struct db_full_path	*pp;
struct db_i			*dbip;
int				argc;
const char			*const*argv;
{
	struct directory	*dp;
	int			ret = 0;
	int			i;

	RT_CK_DBI(dbip);

	/* Make a path structure just big enough */
	pp->magic = DB_FULL_PATH_MAGIC;
	pp->fp_maxlen = pp->fp_len = argc;
	pp->fp_names = (struct directory **)bu_malloc(
		pp->fp_maxlen * sizeof(struct directory *),
		"db_argv_to_path path array" );

	for( i=0; i<argc; i++ )  {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL )  {
			bu_log("db_argv_to_path() failed on element %d='%s'\n",
				i, argv[i] );
			ret = -1;	/* FAILED */
			/* Fall through, storing null dp in this location */
		}
		pp->fp_names[i] = dp;
	}
	return ret;
}

/*
 *			D B _ F R E E _ F U L L _ P A T H
 *
 *  Free the contents of the db_full_path structure, but not the structure
 *  itself, which might be automatic.
 */
void
db_free_full_path( pp )
register struct db_full_path	*pp;
{
	RT_CK_FULL_PATH( pp );

	if( pp->fp_maxlen > 0 )  {
		bu_free( (char *)pp->fp_names, "db_full_path array" );
		pp->fp_maxlen = pp->fp_len = 0;
		pp->fp_names = (struct directory **)0;
	}
}

/*
 *			D B _ I D E N T I C A L _ F U L L _ P A T H S
 *
 *  Returns -
 *	1	match
 *	0	different
 */
int
db_identical_full_paths(
	const struct db_full_path *a,
	const struct db_full_path *b )
{
	register int i;

	RT_CK_FULL_PATH(a);
	RT_CK_FULL_PATH(b);

	if( a->fp_len != b->fp_len )  return 0;

	for( i = a->fp_len-1; i >= 0; i-- )  {
		if( a->fp_names[i] != b->fp_names[i] )  return 0;
	}
	return 1;
}

/*
 *			D B _ F U L L _ P A T H _ S U B S E T
 *
 *  Returns -
 *	1	if 'b' is a proper subset of 'a'
 *	0	if not.
 */
int
db_full_path_subset(
	const struct db_full_path *a,
	const struct db_full_path *b )
{
	register int i;

	RT_CK_FULL_PATH(a);
	RT_CK_FULL_PATH(b);

	if( b->fp_len > a->fp_len )  return 0;

	for( i=0; i < a->fp_len; i++ )  {
		register int j;

		if( a->fp_names[i] != b->fp_names[0] )  continue;

		/* First element matches, check remaining length */
		if( b->fp_len > a->fp_len - i )  return 0;

		/* Check remainder of 'b' */
		for( j=1; j < b->fp_len; j++ )  {
			if( a->fp_names[i+j] != b->fp_names[j] )  goto step;
		}
		/* 'b' is a proper subset */
		return 1;

step:		;
	}
	return 0;
}

/*
 *			D B _ F U L L _ P A T H _ S E A R C H
 *
 *  Returns -
 *	1	'dp' is found on this path
 *	0	not found
 */
int
db_full_path_search( const struct db_full_path *a, const struct directory *dp )
{
	register int i;

	RT_CK_FULL_PATH(a);
	RT_CK_DIR(dp);

	for( i = a->fp_len-1; i >= 0; i-- )  {
		if( a->fp_names[i] == dp )  return 1;
	}
	return 0;
}
