/*
 *			B U _ T C L . C
 *
 *  Tcl interfaces to all the LIBBU Basic BRL-CAD Utility routines.
 *
 *  Remember that in MGED you need to say "set glob_compat_mode 0"
 *  to get [] to work with TCL semantics rather than MGED glob semantics.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char libbu_bu_tcl_RCSid[] = "@(#)$Header$ (ARL)";
#endif


#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

/* defined in libbu/cmdhist_obj.c */
extern int Cho_Init();

/*
 *			B U _ B A D M A G I C _ T C L
 */

void
bu_badmagic_tcl( interp, ptr, magic, str, file, line )
Tcl_Interp	*interp;
CONST long	*ptr;
long		magic;
CONST char	*str;
CONST char	*file;
int		line;
{
	char	buf[256];

	if( !(ptr) )  { 
		sprintf(buf, "ERROR: NULL %s pointer in TCL interface, file %s, line %d\n", 
			str, file, line ); 
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	if( *((long *)(ptr)) != (magic) )  { 
		sprintf(buf, "ERROR: bad pointer in TCL interface x%lx: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n", 
			(long)ptr,
			str, magic,
			bu_identify_magic( *(ptr) ), *(ptr),
			file, line ); 
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	Tcl_AppendResult(interp, "bu_badmagic_tcl() mysterious error condition, ", str, " pointer, ", file, "\n", NULL);
}


/*
 *		B U _ S T R U C T P A R S E _ G E T _ T E R S E _ F O R M
 *
 *  Convert the "form" of a bu_structparse table into a TCL result string,
 *  with parameter-name data-type pairs:
 *
 *	V {%f %f %f} A {%f %f %f}
 *
 *  A different routine should build a more general 'form', along the
 *  lines of {V {%f %f %f} default {help}} {A {%f %f %f} default# {help}}
 */
void
bu_structparse_get_terse_form(interp, sp)
Tcl_Interp *interp;
register struct bu_structparse *sp;
{
	struct bu_vls str;
	int	i;

	bu_vls_init(&str);

	while (sp->sp_name != NULL) {
		Tcl_AppendElement(interp, sp->sp_name);
		bu_vls_trunc(&str, 0);
		if (strcmp(sp->sp_fmt, "%c") == 0 ||
		    strcmp(sp->sp_fmt, "%s") == 0) {
			if (sp->sp_count > 1)
				bu_vls_printf(&str, "%%%ds", sp->sp_count);
			else
				bu_vls_printf(&str, "%%c");
		} else {
			bu_vls_printf(&str, "%s", sp->sp_fmt);
			for (i = 1; i < sp->sp_count; i++)
				bu_vls_printf(&str, " %s", sp->sp_fmt);
		}
		Tcl_AppendElement(interp, bu_vls_addr(&str));
		++sp;
	}
	bu_vls_free(&str);
}

/* Skip the separator(s) */
#define BU_SP_SKIP_SEP(_cp)	\
	{ while( *(_cp) && (*(_cp) == ' ' || *(_cp) == '\n' || \
		*(_cp) == '\t' || *(_cp) == '{' ) )  ++(_cp); }


/*
 *			B U _ S T R U C T P A R S E _ A R G V
 *
 * Support routine for db adjust and db put.  Much like bu_structparse routine,
 * but takes the arguments as lists, a more Tcl-friendly method.
 * Also knows about the Tcl result string, so it can make more informative
 * error messages.
 *
 *  Operates on argv[0] and argv[1], then on argv[2] and argv[3], ...
 */

int
bu_structparse_argv( interp, argc, argv, desc, base )
Tcl_Interp			*interp;
int				 argc;
char			       **argv;
CONST struct bu_structparse	*desc;		/* structure description */
char				*base;		/* base addr of users struct */
{
	register char				*cp, *loc;
	register CONST struct bu_structparse	*sdp;
	register int				 i, j;
	struct bu_vls				 str;

	if( desc == (struct bu_structparse *)NULL ) {
		bu_log( "bu_structparse_argv: NULL desc pointer\n" );
		Tcl_AppendResult( interp, "NULL desc pointer", (char *)NULL );
		return TCL_ERROR;
	}

	/* Run through each of the attributes and their arguments. */

	bu_vls_init( &str );
	while( argc > 0 ) {
		/* Find the attribute which matches this argument. */
		for( sdp = desc; sdp->sp_name != NULL; sdp++ ) {
			if( strcmp(sdp->sp_name, *argv) != 0 )
				continue;

			/* if we get this far, we've got a name match
			 * with a name in the structure description
			 */

#if CRAY && !__STDC__
			loc = (char *)(base+((int)sdp->sp_offset*sizeof(int)));
#else
			loc = (char *)(base+((int)sdp->sp_offset));
#endif
			if( sdp->sp_fmt[0] != '%' ) {
				bu_log( "bu_structparse_argv: unknown format\n" );
				bu_vls_free( &str );
				Tcl_AppendResult( interp, "unknown format",
						  (char *)NULL );
				return TCL_ERROR;
			}

			--argc;
			++argv;

			switch( sdp->sp_fmt[1] )  {
			case 'c':
			case 's':
				/* copy the string, converting escaped
				 * double quotes to just double quotes
				 */
				if( argc < 1 ) {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
			 "not enough values for \"%s\" argument: should be %d",
						       sdp->sp_name,
						       sdp->sp_count );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					bu_vls_free( &str );
					return TCL_ERROR;
				}
				for( i = j = 0;
				     j < sdp->sp_count && argv[0][i] != '\0';
				     loc[j++] = argv[0][i++] )
					;
				if( i < sdp->sp_count )
					loc[i] = '\0';
				if( sdp->sp_count > 1 ) {
					loc[sdp->sp_count-1] = '\0';
					Tcl_AppendResult( interp,
							  sdp->sp_name, " ",
							  loc, " ",
							  (char *)NULL );
				} else {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str, "%s %c ",
						       sdp->sp_name, *loc );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
				}
				--argc;
				++argv;
				break;
			case 'i':
				bu_log(
			 "Error: %%i not implemented. Contact developers.\n" );
				Tcl_AppendResult( interp,
						  "%%i not implemented yet",
						  (char *)NULL );
				bu_vls_free( &str );
				return TCL_ERROR;
			case 'd': {
				register int *ip = (int *)loc;
				register int tmpi;
				register char CONST *cp;

				if( argc < 1 ) {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
      "not enough values for \"%s\" argument: should have %d, only %d given",
						       sdp->sp_name,
						       sdp->sp_count, i );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					bu_vls_free( &str );
					return TCL_ERROR;
				}

				Tcl_AppendResult( interp, sdp->sp_name, " ",
						  (char *)NULL );

				/* Special case:  '=!' toggles a boolean */
				if( argv[0][0] == '!' ) {
					*ip = *ip ? 0 : 1;
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str, "%d ", *ip );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					++argv;
					--argc;
					break;
				}
				/* Normal case: an integer */
				cp = *argv;
				for( i = 0; i < sdp->sp_count; ++i ) {
					if( *cp == '\0' ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str,
		      "not enough values for \"%s\" argument: should have %d",
							       sdp->sp_name,
							       sdp->sp_count );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}

					BU_SP_SKIP_SEP(cp);
					tmpi = atoi( cp );
					if( *cp && (*cp == '+' || *cp == '-') )
						cp++;
					while( *cp && isdigit(*cp) )
						cp++; 
					/* make sure we actually had an
					 * integer out there
					 */

					if( cp == *argv ||
					    (cp == *argv+1 &&
					     (argv[0][0] == '+' ||
					      argv[0][0] == '-')) ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str, 
			       "value \"%s\" to argument %s isn't an integer",
							       argv,
							       sdp->sp_name );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					} else {
						*(ip++) = tmpi;
					}
					BU_SP_SKIP_SEP(cp);
				}
				Tcl_AppendResult( interp,
						  sdp->sp_count > 1 ? "{" : "",
						  argv[0],
						  sdp->sp_count > 1 ? "}" : "",
						  " ", (char *)NULL);
				--argc;
				++argv;
				break; }
			case 'f': {
				int		dot_seen;
				double		tmp_double;
				register double *dp;
				char		*numstart;

				dp = (double *)loc;

				if( argc < 1 ) {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
       "not enough values for \"%s\" argument: should have %d, only %d given",
						       sdp->sp_name,
						       sdp->sp_count, argc );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					bu_vls_free( &str );
					return TCL_ERROR;
				}

				Tcl_AppendResult( interp, sdp->sp_name, " ",
						  (char *)NULL );

				cp = *argv;
				for( i = 0; i < sdp->sp_count; i++ ) {
					if( *cp == '\0' ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str,
       "not enough values for \"%s\" argument: should have %d, only %d given",
							       sdp->sp_name,
							       sdp->sp_count,
							       i );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}
					
					BU_SP_SKIP_SEP(cp);
					numstart = cp;
					if( *cp == '-' || *cp == '+' ) cp++;

					/* skip matissa */
					dot_seen = 0;
					for( ; *cp ; cp++ ) {
						if( *cp == '.' && !dot_seen ) {
							dot_seen = 1;
							continue;
						}
						if( !isdigit(*cp) )
							break;
					}

					/* If no mantissa seen,
					   then there is no float here */
					if( cp == (numstart + dot_seen) ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str, 
	                           "value \"%s\" to argument %s isn't a float",
							       argv[0],
							       sdp->sp_name );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}

					/* there was a mantissa,
					   so we may have an exponent */
					if( *cp == 'E' || *cp == 'e' ) {
						cp++;

						/* skip exponent sign */
						if (*cp == '+' || *cp == '-')
							cp++;
						while( isdigit(*cp) )
							cp++;
					}

					bu_vls_trunc( &str, 0 );
					bu_vls_strcpy( &str, numstart );
					bu_vls_trunc( &str, cp-numstart );
					if( sscanf(bu_vls_addr(&str),
						   "%lf", &tmp_double) != 1 ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str, 
				  "value \"%s\" to argument %s isn't a float",
							       numstart,
							       sdp->sp_name );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}
					
					*dp++ = tmp_double;

					BU_SP_SKIP_SEP(cp);
				}
				Tcl_AppendResult( interp,
						  sdp->sp_count > 1 ? "{" : "",
						  argv[0],
						  sdp->sp_count > 1 ? "}" : "",
						  " ", (char *)NULL );
				--argc;
				++argv;
				break; }
			default:
				Tcl_AppendResult( interp, "unknown format",
						  (char *)NULL );
				return TCL_ERROR;
			}
			break;
		}
		
		if( sdp->sp_name == NULL ) {
			bu_vls_trunc( &str, 0 );
			bu_vls_printf( &str, "invalid attribute %s\n", argv[0] );
			Tcl_AppendResult( interp, bu_vls_addr(&str),
					  (char *)NULL );
			bu_vls_free( &str );
			return TCL_ERROR;
		}
	}
	return TCL_OK;
}

/*
 *			B U _ T C L _ M E M _ B A R R I E R C H E C K
 */
int
bu_tcl_mem_barriercheck(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int	ret;

	ret = bu_mem_barriercheck();
	if( ret < 0 )  {
		Tcl_AppendResult(interp, "bu_mem_barriercheck() failed\n", NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
 *			B U _ T C L _ C K _ M A L L O C _ P T R
 */
int
bu_tcl_ck_malloc_ptr(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if( argc != 3 )  {
		Tcl_AppendResult( interp, "Usage: bu_ck_malloc_ptr ascii-ptr description\n");
		return TCL_ERROR;
	}
	bu_ck_malloc_ptr( (void *)atoi(argv[1]), argv[2] );
	return TCL_OK;
}

/*
 *			B U _ T C L _ M A L L O C _ L E N _ R O U N D U P
 */
int
bu_tcl_malloc_len_roundup(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int	val;

	if( argc != 2 )  {
		Tcl_AppendResult(interp, "Usage: bu_malloc_len_roundup nbytes\n", NULL);
		return TCL_ERROR;
	}
	val = bu_malloc_len_roundup(atoi(argv[1]));
	sprintf(interp->result, "%d", val);
	return TCL_OK;
}

/*
 *			B U _ T C L _ P R M E M
 *
 *  Print map of memory currently in use, to stderr.
 */
int
bu_tcl_prmem(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if( argc != 2 )  {
		Tcl_AppendResult(interp, "Usage: bu_prmem title\n");
		return TCL_ERROR;
	}
	bu_prmem(argv[1]);
	return TCL_OK;
}

/*
 *			B U _ T C L _ P R I N T B
 */
int
bu_tcl_printb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls	str;

	if( argc != 4 )  {
		Tcl_AppendResult(interp, "Usage: bu_printb title integer-to-format bit-format-string\n", NULL);
		return TCL_ERROR;
	}
	bu_vls_init(&str);
	bu_vls_printb( &str, argv[1], atoi(argv[2]), argv[3] );
	Tcl_SetResult( interp, bu_vls_addr(&str), TCL_VOLATILE );
	bu_vls_free(&str);
	return TCL_OK;
}

/*
 *			B U _ G E T _ V A L U E _ B Y _ K E Y W O R D
 *
 *  Given arguments of alternating keywords and values
 *  and a specific keyword ("Iwant"),
 *  return the value associated with that keyword.
 *
 *	example:  bu_get_value_by_keyword Iwant az 35 elev 25 temp 9.6
 *
 *  If only one argument is given after the search keyword, it is interpreted
 *  as a list in the same format.
 *
 *	example:  bu_get_value_by_keyword Iwant {az 35 elev 25 temp 9.6}
 *
 *  Search order is left-to-right, only first match is returned.
 *
 *  Sample use:
 *	 bu_get_value_by_keyword V8 [concat type [.inmem get box.s]]
 */
int
bu_get_value_by_keyword(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int	listc;
	char	**listv;
	register char	*iwant;
	char	**tofree = (char **)NULL;
	int	i;

	if( argc < 3 )  {
		char	buf[32];
		sprintf(buf, "%d", argc);
		Tcl_AppendResult( interp,
			"bu_get_value_by_keyword: wrong # of args (", buf, ").\n",
			"Usage: bu_get_value_by_keyword iwant {list}\n",
			"Usage: bu_get_value_by_keyword iwant key1 val1 key2 val2 ... keyN valN\n",
			(char *)NULL );
		return TCL_ERROR;
	}

	iwant = argv[1];

	if( argc == 3 )  {
		if( Tcl_SplitList( interp, argv[2], &listc, &listv ) != TCL_OK )  {
			Tcl_AppendResult( interp,
				"bu_get_value_by_keyword: iwant='", iwant,
				"', unable to split '",
				argv[2], "'\n", (char *)NULL );
			return TCL_ERROR;
		}
		tofree = listv;
	} else {
		/* Take search list from remaining arguments */
		listc = argc - 2;
		listv = argv + 2;
	}

	if( (listc & 1) != 0 )  {
		char	buf[32];
		sprintf(buf, "%d", listc);
		Tcl_AppendResult( interp,
			"bu_get_value_by_keyword: odd # of items in list (", buf, ").\n",
			(char *)NULL );
		if(tofree) free( (char *)tofree );	/* not bu_free() */
		return TCL_ERROR;
	}

	for( i=0; i < listc; i += 2 )  {
		if( strcmp( iwant, listv[i] ) == 0 )  {
			/* If value is a list, don't nest it in another list */
			if( listv[i+1][0] == '{' )  {
				struct bu_vls	str;
				bu_vls_init( &str );
				/* Skip leading { */
				bu_vls_strcat( &str, &listv[i+1][1] );
				/* Trim trailing } */
				bu_vls_trunc( &str, -1 );
				Tcl_AppendResult( interp,
					bu_vls_addr(&str), (char *)NULL );
				bu_vls_free( &str );
			} else {
				Tcl_AppendResult( interp, listv[i+1], (char *)NULL );
			}
			if(tofree) free( (char *)tofree );	/* not bu_free() */
			return TCL_OK;
		}
	}
	
	/* Not found */
	Tcl_AppendResult( interp, "bu_get_value_by_keyword: keyword '",
		iwant, "' not found in list\n", (char *)NULL );
	if(tofree) free( (char *)tofree );	/* not bu_free() */
	return TCL_ERROR;
}

/*
 *			B U _ G E T _ A L L _ K E Y W O R D _ V A L U E S
 *
 *  Given arguments of alternating keywords and values,
 *  establish local variables named after the keywords, with the
 *  indicated values.
 *
 *	example:  bu_get_all_keyword_values az 35 elev 25 temp 9.6
 *
 *  This is much faster than writing this in raw Tcl 8 as:
 *
 *	foreach {keyword value} $list {
 *		set $keyword $value
 *		lappend retval $keyword
 *	}
 *
 *  If only one argument is given it is interpreted
 *  as a list in the same format.
 *
 *	example:  bu_get_all_keyword_values {az 35 elev 25 temp 9.6}
 *
 *  For security reasons, the name of the local variable assigned to
 *  is that of the input keyword with "key_" prepended.
 *  This prevents a playful user from overriding variables inside
 *  the function, e.g. loop iterator "i", etc.
 *  This could be even worse when called in global context.
 *
 *  Processing order is left-to-right, rightmost value for a repeated
 *  keyword will be the one used.
 *
 *  Sample use:
 *	 bu_get_all_keyword_values [concat type [.inmem get box.s]]
 *
 *  Returns -
 *	List of variable names that were assigned to.
 *	This lets you detect at runtime what assignments
 *	were actually performed.
 */
int
bu_get_all_keyword_values(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls	variable;
	int	listc;
	char	**listv;
	char	**tofree = (char **)NULL;
	int	i;

	if( argc < 2 )  {
		char	buf[32];
		sprintf(buf, "%d", argc);
		Tcl_AppendResult( interp,
			"bu_get_all_keyword_values: wrong # of args (", buf, ").\n",
			"Usage: bu_get_all_keyword_values {list}\n",
			"Usage: bu_get_all_keyword_values key1 val1 key2 val2 ... keyN valN\n",
			(char *)NULL );
		return TCL_ERROR;
	}

	if( argc == 2 )  {
		if( Tcl_SplitList( interp, argv[1], &listc, &listv ) != TCL_OK )  {
			Tcl_AppendResult( interp,
				"bu_get_all_keyword_values: unable to split '",
				argv[1], "'\n", (char *)NULL );
			return TCL_ERROR;
		}
		tofree = listv;
	} else {
		/* Take search list from remaining arguments */
		listc = argc - 1;
		listv = argv + 1;
	}

	if( (listc & 1) != 0 )  {
		char	buf[32];
		sprintf(buf, "%d", listc);
		Tcl_AppendResult( interp,
			"bu_get_all_keyword_values: odd # of items in list (",
			buf, "), aborting.\n",
			(char *)NULL );
		if(tofree) free( (char *)tofree );	/* not bu_free() */
		return TCL_ERROR;
	}


	/* Process all the pairs */
	bu_vls_init( &variable );
	for( i=0; i < listc; i += 2 )  {
		bu_vls_strcpy( &variable, "key_" );
		bu_vls_strcat( &variable, listv[i] );
		/* If value is a list, don't nest it in another list */
		if( listv[i+1][0] == '{' )  {
			struct bu_vls	str;
			bu_vls_init( &str );
			/* Skip leading { */
			bu_vls_strcat( &str, &listv[i+1][1] );
			/* Trim trailing } */
			bu_vls_trunc( &str, -1 );
			Tcl_SetVar( interp, bu_vls_addr(&variable),
				bu_vls_addr(&str), 0);
			bu_vls_free( &str );
		} else {
			Tcl_SetVar( interp, bu_vls_addr(&variable),
				listv[i+1], 0 );
		}
		Tcl_AppendResult( interp, bu_vls_addr(&variable),
			" ", (char *)NULL );
		bu_vls_trunc( &variable, 0 );
	}
	
	/* All done */
	bu_vls_free( &variable );
	if(tofree) free( (char *)tofree );	/* not bu_free() */
	return TCL_OK;
}

/*
 *			B U _ T C L _ R G B _ T O _ H S V
 */
int
bu_tcl_rgb_to_hsv(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int		rgb_int[3];
	unsigned char	rgb[3];
	fastf_t		hsv[3];
	struct bu_vls	result;

	bu_vls_init(&result);
	if( argc != 4 )  {
		Tcl_AppendResult( interp, "Usage: bu_rgb_to_hsv R G B\n",
		    (char *)NULL );
		return TCL_ERROR;
	}
	if (( Tcl_GetInt( interp, argv[1], &rgb_int[0] ) != TCL_OK )
	 || ( Tcl_GetInt( interp, argv[2], &rgb_int[1] ) != TCL_OK )
	 || ( Tcl_GetInt( interp, argv[3], &rgb_int[2] ) != TCL_OK )
	 || ( rgb_int[0] < 0 ) || ( rgb_int[0] > 255 )
	 || ( rgb_int[1] < 0 ) || ( rgb_int[1] > 255 )
	 || ( rgb_int[2] < 0 ) || ( rgb_int[2] > 255 ))
	 {
		bu_vls_printf(&result, "bu_rgb_to_hsv: Bad RGB (%s, %s, %s)\n",
		    argv[1], argv[2], argv[3]);
		Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
		bu_vls_free(&result);
		return TCL_ERROR;
	}
	rgb[0] = rgb_int[0];
	rgb[1] = rgb_int[1];
	rgb[2] = rgb_int[2];

	bu_rgb_to_hsv( rgb, hsv );
	bu_vls_printf(&result, "%g %g %g", V3ARGS(hsv));
	Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
	bu_vls_free(&result);
	return TCL_OK;
	
}

/*
 *			B U _ T C L _ H S V _ T O _ R G B
 */
int
bu_tcl_hsv_to_rgb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	fastf_t		hsv[3];
	unsigned char	rgb[3];
	struct bu_vls	result;

	if( argc != 4 )  {
		Tcl_AppendResult( interp, "Usage: bu_hsv_to_rgb H S V\n",
		    (char *)NULL );
		return TCL_ERROR;
	}
	bu_vls_init(&result);
	if (( Tcl_GetDouble( interp, argv[1], &hsv[0] ) != TCL_OK )
	 || ( Tcl_GetDouble( interp, argv[2], &hsv[1] ) != TCL_OK )
	 || ( Tcl_GetDouble( interp, argv[3], &hsv[2] ) != TCL_OK )
	 || ( bu_hsv_to_rgb( hsv, rgb ) == 0) ) {
		bu_vls_printf(&result, "bu_hsv_to_rgb: Bad HSV (%s, %s, %s)\n",
		    argv[1], argv[2], argv[3]);
		Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
		bu_vls_free(&result);
		return TCL_ERROR;
	}

	bu_vls_printf(&result, "%d %d %d", V3ARGS(rgb));
	Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
	bu_vls_free(&result);
	return TCL_OK;
	
}

int
bu_tcl_key_eq_to_key_val( clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls vls;
	char *next;
	int i=0;

	bu_vls_init( &vls );

	while( ++i < argc )
	{
		if( bu_key_eq_to_key_val( argv[i], &next, &vls ) )
		{
			bu_vls_free( &vls );
			return TCL_ERROR;
		}

		if( i < argc - 1 )
			Tcl_AppendResult(interp, bu_vls_addr( &vls ) , " ", NULL );
		else
			Tcl_AppendResult(interp, bu_vls_addr( &vls ), NULL );

		bu_vls_trunc( &vls, 0 );
	}

	bu_vls_free( &vls );
	return TCL_OK;

}

int
bu_tcl_shader_to_key_val( clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls vls;

	bu_vls_init( &vls );

	if( bu_shader_to_tcl_list( argv[1], &vls ) )
	{
		bu_vls_free( &vls );
		return( TCL_ERROR );
	}

	Tcl_AppendResult(interp, bu_vls_addr( &vls ), NULL );

	bu_vls_free( &vls );

	return TCL_OK;

}

int
bu_tcl_key_val_to_key_eq( clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int i=0;

	for( i=1 ; i<argc ; i += 2 )
	{
		if( i+1 < argc-1 )
			Tcl_AppendResult(interp, argv[i], "=", argv[i+1], " ", NULL );
		else
			Tcl_AppendResult(interp, argv[i], "=", argv[i+1], NULL );

	}
	return TCL_OK;

}

int
bu_tcl_shader_to_key_eq( clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls vls;


	bu_vls_init( &vls );

	if( bu_shader_to_key_eq( argv[1], &vls ) )
	{
		bu_vls_free( &vls );
		return TCL_ERROR;
	}

	Tcl_AppendResult(interp, bu_vls_addr( &vls ), NULL );

	bu_vls_free( &vls );

	return TCL_OK;
}

/*
 *			B U _ T C L _ B R L C A D _ P A T H
 *
 *  Tcl access to library routine bu_brlcad_path(),
 *  which handles BRLCAD_ROOT issues for GUI code.
 */
int
bu_tcl_brlcad_path( clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if( argc != 2 )  {
		Tcl_AppendResult(interp, "Usage: bu_brlcad_path subdir\n",
			(char *)NULL );
		return TCL_ERROR;
	}
	Tcl_AppendResult(interp, bu_brlcad_path( argv[1] ), NULL );
	return TCL_OK;
}

/*
 *			B U _ T C L _ U N I T S _ C O N V E R S I O N
 *
 *	Tcl access to bu_units_comversion()
 */
int
bu_tcl_units_conversion( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	double conv_factor;
	struct bu_vls result;

	if( argc != 2 )  {
		Tcl_AppendResult(interp, "Usage: bu_units_conversion units_string\n",
			(char *)NULL );
		return TCL_ERROR;
	}

	conv_factor = bu_units_conversion( argv[1] );
	if( conv_factor == 0.0 )
	{
		Tcl_AppendResult(interp, "ERROR: bu_units_conversion: Unrecognized units string: ",
			argv[1], "\n", (char *)NULL );
			return TCL_ERROR;
	}

	bu_vls_init( &result );
	bu_vls_printf( &result, "%.12e", conv_factor );
	Tcl_AppendResult(interp, bu_vls_addr( &result ), (char *)NULL );
	bu_vls_free( &result );
	return TCL_OK;
}

/*
 *			B U _ T C L _ S E T U P
 *
 *  Add all the supported Tcl interfaces to LIBBU routines to
 *  the list of commands known by the given interpreter.
 */
void
bu_tcl_setup(interp)
Tcl_Interp *interp;
{
	(void)Tcl_CreateCommand(interp,
		"bu_units_conversion",	bu_tcl_units_conversion,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_brlcad_path",	bu_tcl_brlcad_path,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_mem_barriercheck",	bu_tcl_mem_barriercheck,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_ck_malloc_ptr",	bu_tcl_ck_malloc_ptr,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_malloc_len_roundup",bu_tcl_malloc_len_roundup,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_prmem",		bu_tcl_prmem,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_printb",		bu_tcl_printb,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_get_all_keyword_values", bu_get_all_keyword_values,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_get_value_by_keyword", bu_get_value_by_keyword,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_rgb_to_hsv",	bu_tcl_rgb_to_hsv,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_hsv_to_rgb",	bu_tcl_hsv_to_rgb,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_key_eq_to_key_val",	bu_tcl_key_eq_to_key_val,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_shader_to_tcl_list",	bu_tcl_shader_to_key_val,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_key_val_to_key_eq",	bu_tcl_key_val_to_key_eq,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_shader_to_key_eq",	bu_tcl_shader_to_key_eq,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);

	Tcl_SetVar(interp, "bu_version", (char *)bu_version+5, TCL_GLOBAL_ONLY);	/* from vers.c */
	Tcl_SetVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
	Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_LINK_INT );

	/* initialize command history objects */
	Cho_Init(interp);
}

/*
 *  Allows LIBBU to be dynamically loade to a vanilla tclsh/wish with
 *  "load /usr/brlcad/lib/libbu.so"
 */
int
Bu_Init(interp)
Tcl_Interp *interp;
{
	bu_tcl_setup(interp);
	return TCL_OK;
}
