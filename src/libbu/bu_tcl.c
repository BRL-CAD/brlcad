/*                        B U _ T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup butcl */
/** @{ */
/** @file bu_tcl.c
 *
 * @brief
 *	Tcl interfaces to all the LIBBU Basic BRL-CAD Utility routines.
 *
 *	Remember that in MGED you need to say "set glob_compat_mode 0"
 *	to get [] to work with TCL semantics rather than MGED glob semantics.
 *
 * @author Michael John Muuss
 *
 * @par Source -
 *	The U. S. Army Research Laboratory			@n
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char libbu_bu_tcl_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <ctype.h>

#include "tcl.h"
#include "machine.h"
#include "cmd.h"		/* this includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "bu.h"


/* defined in libbu/cmdhist_obj.c */
extern int Cho_Init(Tcl_Interp *interp);

static struct bu_cmdtab bu_cmds[] = {
	{"bu_units_conversion",		bu_tcl_units_conversion},
	{"bu_brlcad_data",		bu_tcl_brlcad_data},
	{"bu_brlcad_path",		bu_tcl_brlcad_path},
	{"bu_brlcad_root",		bu_tcl_brlcad_root},
	{"bu_mem_barriercheck",		bu_tcl_mem_barriercheck},
	{"bu_ck_malloc_ptr",		bu_tcl_ck_malloc_ptr},
	{"bu_malloc_len_roundup",	bu_tcl_malloc_len_roundup},
	{"bu_prmem",			bu_tcl_prmem},
	{"bu_printb",			bu_tcl_printb,},
	{"bu_get_all_keyword_values",	bu_get_all_keyword_values},
	{"bu_get_value_by_keyword",	bu_get_value_by_keyword},
	{"bu_rgb_to_hsv",		bu_tcl_rgb_to_hsv},
	{"bu_hsv_to_rgb",		bu_tcl_hsv_to_rgb},
	{"bu_key_eq_to_key_val",	bu_tcl_key_eq_to_key_val},
	{"bu_shader_to_tcl_list",	bu_tcl_shader_to_key_val},
	{"bu_key_val_to_key_eq",	bu_tcl_key_val_to_key_eq},
	{"bu_shader_to_key_eq",		bu_tcl_shader_to_key_eq},
	{(char *)NULL,	(int (*)())0 }
};


/**
 *	b u _ b a d m a g i c _ t c l
 *
 * 	Support routine for BU_CKMAG_TCL macro. As used by
 *	BU_CKMAG_TCL, this routine is not called unless there
 *	is trouble with the pointer. When called, an appropriate
 *	message is added to interp indicating the problem.
 *
 *	@param interp	- tcl interpreter where result is stored
 *	@param ptr	- pointer to a data structure
 *	@param magic	- the correct/desired magic number
 *	@param str	- usually indicates the data structure name
 *	@param file	- file where this routine was called
 *	@param line	- line number in the above file
 *
 * 	@return
 *		void
 */

void
bu_badmagic_tcl(Tcl_Interp	*interp,
		const long	*ptr,
		unsigned long	magic,
		const char	*str,
		const char	*file,
		int		line)
{
	char	buf[256];

	if (!(ptr)) {
		sprintf(buf, "ERROR: NULL %s pointer in TCL interface, file %s, line %d\n",
			str, file, line);
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	if (*((long *)(ptr)) != (magic)) {
		sprintf(buf, "ERROR: bad pointer in TCL interface x%lx: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n",
			(long)ptr,
			str, magic,
			bu_identify_magic( *(ptr) ), *(ptr),
			file, line);
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	Tcl_AppendResult(interp, "bu_badmagic_tcl() mysterious error condition, ", str, " pointer, ", file, "\n", NULL);
}


/**
 *	bu_structparse_get_terse_form
 *
 *	Convert the "form" of a bu_structparse table into a TCL result string,
 *	with parameter-name data-type pairs:
 *
 *		V {%f %f %f} A {%f %f %f}
 *
 *	A different routine should build a more general 'form', along the
 *	lines of {V {%f %f %f} default {help}} {A {%f %f %f} default# {help}}
 *
 *	@param interp	- tcl interpreter
 *	@param sp	- structparse table
 *
 * 	@return
 *		void
 */
void
bu_structparse_get_terse_form(Tcl_Interp			*interp,
			      const struct bu_structparse	*sp)
{
	struct bu_vls	str;
	int		i;

	bu_vls_init(&str);

	while (sp->sp_name != NULL) {
		Tcl_AppendElement(interp, sp->sp_name);
		bu_vls_trunc(&str, 0);
		/* These types are specified by lengths, e.g. %80s */
		if (strcmp(sp->sp_fmt, "%c") == 0 ||
		    strcmp(sp->sp_fmt, "%s") == 0 ||
		    strcmp(sp->sp_fmt, "%S") == 0) {
			if (sp->sp_count > 1) {
				/* Make them all look like %###s */
				bu_vls_printf(&str, "%%%lds", sp->sp_count);
			} else {
				/* Singletons are specified by their actual character */
				bu_vls_printf(&str, "%%c");
			}
		} else {
			/* Vectors are specified by repetition, e.g. {%f %f %f} */
			bu_vls_printf(&str, "%s", sp->sp_fmt);
			for (i = 1; i < sp->sp_count; i++)
				bu_vls_printf(&str, " %s", sp->sp_fmt);
		}
		Tcl_AppendElement(interp, bu_vls_addr(&str));
		++sp;
	}
	bu_vls_free(&str);
}

/**
 *	BU_SP_SKIP_SEP
 *
 *	Skip the separator(s) (i.e. whitespace and open-braces)
 *
 *	@param _cp	- character pointer
 */
#define BU_SP_SKIP_SEP(_cp)	\
	{ while( *(_cp) && (*(_cp) == ' ' || *(_cp) == '\n' || \
		*(_cp) == '\t' || *(_cp) == '{' ) )  ++(_cp); }


/**
 *	bu_structparse_argv
 *
 *	Support routine for db adjust and db put.  Much like the bu_struct_parse routine
 *	which takes its input as a bu_vls. This routine, however, takes the arguments
 *	as lists, a more Tcl-friendly method. Also knows about the Tcl result string,
 *	so it can make more informative error messages.
 *
 *	Operates on argv[0] and argv[1], then on argv[2] and argv[3], ...
 *
 *
 *	@param interp	- tcl interpreter
 *	@param argc	- number of elements in argv
 *	@param argv	- contains the keyword-value pairs
 *	@param desc	- structure description
 *	@param base	- base addr of users struct
 *
 * 	@retval TCL_OK if successful,
 *	@retval TCL_ERROR on failure
 */
int
bu_structparse_argv(Tcl_Interp			*interp,
		    int				argc,
		    char			**argv,
		    const struct bu_structparse	*desc,
		    char			*base)
{
	register char				*cp, *loc;
	register const struct bu_structparse	*sdp;
	register int				 j;
	register int				ii;
	struct bu_vls				 str;

	if (desc == (struct bu_structparse *)NULL) {
		bu_log("bu_structparse_argv: NULL desc pointer\n");
		Tcl_AppendResult(interp, "NULL desc pointer", (char *)NULL);
		return TCL_ERROR;
	}

	/* Run through each of the attributes and their arguments. */

	bu_vls_init(&str);
	while (argc > 0) {
		/* Find the attribute which matches this argument. */
		for (sdp = desc; sdp->sp_name != NULL; sdp++) {
			if (strcmp(sdp->sp_name, *argv) != 0)
				continue;

			/* if we get this far, we've got a name match
			 * with a name in the structure description
			 */

#if CRAY && !__STDC__
			loc = (char *)(base+((int)sdp->sp_offset*sizeof(int)));
#else
			loc = (char *)(base+((int)sdp->sp_offset));
#endif
			if (sdp->sp_fmt[0] != '%') {
				bu_log("bu_structparse_argv: unknown format\n");
				bu_vls_free(&str);
				Tcl_AppendResult(interp, "unknown format",
						 (char *)NULL);
				return TCL_ERROR;
			}

			--argc;
			++argv;

			switch (sdp->sp_fmt[1]) {
			case 'c':
			case 's':
				/* copy the string, converting escaped
				 * double quotes to just double quotes
				 */
				if (argc < 1) {
					bu_vls_trunc(&str, 0);
					bu_vls_printf(&str,
						      "not enough values for \"%s\" argument: should be %ld",
						      sdp->sp_name,
						      sdp->sp_count);
					Tcl_AppendResult(interp,
							 bu_vls_addr(&str),
							 (char *)NULL);
					bu_vls_free(&str);
					return TCL_ERROR;
				}
				for (ii = j = 0;
				     j < sdp->sp_count && argv[0][ii] != '\0';
				     loc[j++] = argv[0][ii++])
					;
				if (ii < sdp->sp_count)
					loc[ii] = '\0';
				if (sdp->sp_count > 1) {
					loc[sdp->sp_count-1] = '\0';
					Tcl_AppendResult(interp,
							 sdp->sp_name, " ",
							 loc, " ",
							 (char *)NULL);
				} else {
					bu_vls_trunc(&str, 0);
					bu_vls_printf(&str, "%s %c ",
						      sdp->sp_name, *loc);
					Tcl_AppendResult(interp,
							 bu_vls_addr(&str),
							 (char *)NULL);
				}
				break;
			case 'S': {
				struct bu_vls *vls = (struct bu_vls *)loc;
				bu_vls_init_if_uninit( vls );
				bu_vls_strcpy(vls, *argv);
				break;
			}
			case 'i':
#if 0
				bu_log(
			 "Error: %%i not implemented. Contact developers.\n" );
				Tcl_AppendResult( interp,
						  "%%i not implemented yet",
						  (char *)NULL );
				bu_vls_free( &str );
				return TCL_ERROR;
#else
				{
				register short *sh = (short *)loc;
				register int tmpi;
				register char const *cp;

				if( argc < 1 ) { /* XXX - when was ii defined */
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
      "not enough values for \"%s\" argument: should have %ld",
						       sdp->sp_name,
						       sdp->sp_count);
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
					*sh = *sh ? 0 : 1;
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str, "%hd ", *sh );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					break;
				}
				/* Normal case: an integer */
				cp = *argv;
				for( ii = 0; ii < sdp->sp_count; ++ii ) {
					if( *cp == '\0' ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str,
		      "not enough values for \"%s\" argument: should have %ld",
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
							       argv[0],
							       sdp->sp_name );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					} else {
						*(sh++) = tmpi;
					}
					BU_SP_SKIP_SEP(cp);
				}
				Tcl_AppendResult( interp,
						  sdp->sp_count > 1 ? "{" : "",
						  argv[0],
						  sdp->sp_count > 1 ? "}" : "",
						  " ", (char *)NULL);
				break; }

#endif
			case 'd': {
				register int *ip = (int *)loc;
				register int tmpi;
				register char const *cp;

				if( argc < 1 ) { /* XXX - when was ii defined */
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
      "not enough values for \"%s\" argument: should have %ld",
						       sdp->sp_name,
						       sdp->sp_count);
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
					break;
				}
				/* Normal case: an integer */
				cp = *argv;
				for( ii = 0; ii < sdp->sp_count; ++ii ) {
					if( *cp == '\0' ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str,
		      "not enough values for \"%s\" argument: should have %ld",
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
							       argv[0],
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
       "not enough values for \"%s\" argument: should have %ld, only %d given",
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
				for( ii = 0; ii < sdp->sp_count; ii++ ) {
					if( *cp == '\0' ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str,
       "not enough values for \"%s\" argument: should have %ld, only %d given",
							       sdp->sp_name,
							       sdp->sp_count,
							       ii );
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
				break; }
			default: {
				struct bu_vls vls;

				bu_vls_init(&vls);
				bu_vls_printf(&vls,
				"%s line:%d Parse error, unknown format: '%s' for element \"%s\"",
				__FILE__, __LINE__, sdp->sp_fmt,
				sdp->sp_name);

				Tcl_AppendResult( interp, bu_vls_addr(&vls),
					(char *)NULL );

				bu_vls_free( &vls );
				return TCL_ERROR;
				}
			}

			if( sdp->sp_hook )  {
				sdp->sp_hook( sdp, sdp->sp_name, base, *argv);

			}
			--argc;
			++argv;


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


/**
 *	bu_tcl_mem_barriercheck
 *
 *	A tcl wrapper for bu_mem_barriercheck.
 *
 * @param clientData	- associated data/state
 * @param interp		- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_mem_barriercheck(ClientData	clientData,
			Tcl_Interp	*interp,
			int		argc,
			char		**argv)
{
	int	ret;

	ret = bu_mem_barriercheck();
	if (ret < 0) {
		Tcl_AppendResult(interp, "bu_mem_barriercheck() failed\n", NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}


/**
 *	bu_tcl_ck_malloc_ptr
 *
 *	A tcl wrapper for bu_ck_malloc_ptr.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_ck_malloc_ptr(ClientData		clientData,
		     Tcl_Interp		*interp,
		     int		argc,
		     char		**argv)
{
	if( argc != 3 )  {
		Tcl_AppendResult( interp, "Usage: bu_ck_malloc_ptr ascii-ptr description\n");
		return TCL_ERROR;
	}
	bu_ck_malloc_ptr( (genptr_t)atol(argv[1]), argv[2] );
	return TCL_OK;
}


/**
 * 	bu_tcl_malloc_len_roundup
 *
 *	A tcl wrapper for bu_malloc_len_roundup.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@Return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_malloc_len_roundup(ClientData	clientData,
			  Tcl_Interp	*interp,
			  int		argc,
			  char		**argv)
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


/**
 *	bu_tcl_prmem
 *
 *	A tcl wrapper for bu_prmem. Prints map of
 *	memory currently in use, to stderr.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_prmem(ClientData	clientData,
	     Tcl_Interp	*interp,
	     int	argc,
	     char	**argv)
{
	if (argc != 2) {
		Tcl_AppendResult(interp, "Usage: bu_prmem title\n");
		return TCL_ERROR;
	}
	bu_prmem(argv[1]);
	return TCL_OK;
}


/**
 *	bu_tcl_printb
 *
 *	A tcl wrapper for bu_vls_printb.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_printb(ClientData	clientData,
	      Tcl_Interp	*interp,
	      int		argc,
	      char		**argv)
{
	struct bu_vls	str;

	if (argc != 4) {
		Tcl_AppendResult(interp, "Usage: bu_printb title integer-to-format bit-format-string\n", NULL);
		return TCL_ERROR;
	}
	bu_vls_init(&str);
	bu_vls_printb(&str, argv[1], atoi(argv[2]), argv[3]);
	Tcl_SetResult(interp, bu_vls_addr(&str), TCL_VOLATILE);
	bu_vls_free(&str);
	return TCL_OK;
}


/**
 *	bu_get_value_by_keyword
 *
 *	Given arguments of alternating keywords and values
 *	and a specific keyword ("Iwant"),
 *	return the value associated with that keyword.
 *
 *	example:  bu_get_value_by_keyword Iwant az 35 elev 25 temp 9.6
 *
 *	If only one argument is given after the search keyword, it is interpreted
 *	as a list in the same format.
 *
 *	example:  bu_get_value_by_keyword Iwant {az 35 elev 25 temp 9.6}
 *
 *	Search order is left-to-right, only first match is returned.
 *
 *	Sample use:
 *		bu_get_value_by_keyword V8 [concat type [.inmem get box.s]]
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_get_value_by_keyword(ClientData	clientData,
			Tcl_Interp	*interp,
			int		argc,
			char		**argv)
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
		if( Tcl_SplitList( interp, argv[2], &listc, (const char ***)&listv ) != TCL_OK )  {
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


/**
 *	bu_get_all_keyword_values
 *
 *	Given arguments of alternating keywords and values,
 *	establish local variables named after the keywords, with the
 *	indicated values. Returns in interp a list of the variable
 *	names that were assigned to. This lets you detect at runtime
 *	what assignments were actually performed.
 *
 *	example:  bu_get_all_keyword_values az 35 elev 25 temp 9.6
 *
 *	This is much faster than writing this in raw Tcl 8 as:
 *
 *	foreach {keyword value} $list {
 *		set $keyword $value
 *		lappend retval $keyword
 *	}
 *
 *	If only one argument is given it is interpreted
 *	as a list in the same format.
 *
 *	example:  bu_get_all_keyword_values {az 35 elev 25 temp 9.6}
 *
 *	For security reasons, the name of the local variable assigned to
 *	is that of the input keyword with "key_" prepended.
 *	This prevents a playful user from overriding variables inside
 *	the function, e.g. loop iterator "i", etc.
 *	This could be even worse when called in global context.
 *
 *	Processing order is left-to-right, rightmost value for a repeated
 *	keyword will be the one used.
 *
 *	Sample use:
 *		bu_get_all_keyword_values [concat type [.inmem get box.s]]
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_get_all_keyword_values(ClientData	clientData,
			  Tcl_Interp	*interp,
			  int		argc,
			  char		**argv)
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
		if( Tcl_SplitList( interp, argv[1], &listc, (const char ***)&listv ) != TCL_OK )  {
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


/**
 *	bu_tcl_rgb_to_hsv
 *
 *	A tcl wrapper for bu_rgb_to_hsv.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_rgb_to_hsv(ClientData	clientData,
		  Tcl_Interp	*interp,
		  int		argc,
		  char		**argv)
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
	    || ( rgb_int[2] < 0 ) || ( rgb_int[2] > 255 )) {
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


/**
 *	bu_tcl_hsv_to_rgb
 *
 *	A tcl wrapper for bu_hsv_to_rgb.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_hsv_to_rgb(ClientData	clientData,
		  Tcl_Interp	*interp,
		  int		argc,
		  char		**argv)
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


/**
 *	bu_tcl_key_eq_to_key_val
 *
 *	Converts key=val to "key val" pairs.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_key_eq_to_key_val(ClientData	clientData,
			 Tcl_Interp	*interp,
			 int		argc,
			 char		**argv)
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


/**
 *	bu_tcl_shader_to_key_val
 *
 *	Converts a shader string to a tcl list.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_shader_to_key_val(ClientData	clientData,
			 Tcl_Interp	*interp,
			 int		argc,
			 char		**argv)
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


/**
 *	bu_tcl_key_val_to_key_eq
 *
 *	Converts "key value" pairs to key=value.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_key_val_to_key_eq(ClientData	clientData,
			 Tcl_Interp	*interp,
			 int		argc,
			 char		**argv)
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


/**
 *	bu_tcl_shader_to_key_eq
 *
 *	Converts a shader tcl list into a shader string.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_shader_to_key_eq(ClientData	clientData,
			Tcl_Interp	*interp,
			int		argc,
			char		**argv)
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


/**
 *	bu_tcl_brlcad_root
 *
 *	A tcl wrapper for bu_brlcad_root.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_brlcad_root(ClientData	clientData,
		   Tcl_Interp	*interp,
		   int		 argc,
		   char		**argv)
{
	if (argc != 2) {
		Tcl_AppendResult(interp, "Usage: bu_brlcad_root subdir\n",
				 (char *)NULL);
		return TCL_ERROR;
	}
	Tcl_AppendResult(interp, bu_brlcad_root(argv[1], 0), NULL);
	return TCL_OK;
}


/**
 *	bu_tcl_brlcad_data
 *
 *	A tcl wrapper for bu_brlcad_data.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_brlcad_data(ClientData	clientData,
		   Tcl_Interp	*interp,
		   int		 argc,
		   char		**argv)
{
	if (argc != 2) {
		Tcl_AppendResult(interp, "Usage: bu_brlcad_data subdir\n",
				 (char *)NULL);
		return TCL_ERROR;
	}
	Tcl_AppendResult(interp, bu_brlcad_data(argv[1], 0), NULL);
	return TCL_OK;
}


/**
 *	bu_tcl_brlcad_path
 *
 *	A tcl wrapper for bu_brlcad_path.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_brlcad_path(ClientData	clientData,
		   Tcl_Interp	*interp,
		   int		 argc,
		   char		**argv)
{
	if (argc != 2) {
		Tcl_AppendResult(interp, "Usage: bu_brlcad_path subdir\n",
				 (char *)NULL);
		return TCL_ERROR;
	}
	Tcl_AppendResult(interp, bu_brlcad_path(argv[1], 0), NULL);
	return TCL_OK;
}


/**
 *	bu_tcl_units_conversion
 *
 *	A tcl wrapper for bu_units_conversion.
 *
 *	@param clientData	- associated data/state
 *	@param interp		- tcl interpreter in which this command was registered.
 *	@param argc		- number of elements in argv
 *	@param argv		- command name and arguments
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_tcl_units_conversion(ClientData	clientData,
			Tcl_Interp	*interp,
			int		argc,
			char		**argv)
{
	double conv_factor;
	struct bu_vls result;

	if (argc != 2) {
		Tcl_AppendResult(interp, "Usage: bu_units_conversion units_string\n",
				 (char *)NULL);
		return TCL_ERROR;
	}

	conv_factor = bu_units_conversion(argv[1]);
	if (conv_factor == 0.0) {
		Tcl_AppendResult(interp, "ERROR: bu_units_conversion: Unrecognized units string: ",
				 argv[1], "\n", (char *)NULL);
		return TCL_ERROR;
	}

	bu_vls_init(&result);
	bu_vls_printf(&result, "%.12e", conv_factor);
	Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
	bu_vls_free(&result);
	return TCL_OK;
}

/**
 *	bu_tcl_setup
 *
 *	Add all the supported Tcl interfaces to LIBBU routines to
 *	the list of commands known by the given interpreter.
 *
 *	@param interp		- tcl interpreter in which this command was registered.
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
void
bu_tcl_setup(Tcl_Interp *interp)
{
	bu_register_cmds(interp, bu_cmds);

	Tcl_SetVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
	Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_LINK_INT );

	/* initialize command history objects */
	Cho_Init(interp);
}

/**
 *	Bu_Init
 *
 *	Allows LIBBU to be dynamically loaded to a vanilla tclsh/wish with
 *	"load /usr/brlcad/lib/libbu.so"
 *
 *	@param interp		- tcl interpreter in which this command was registered.
 *
 *	@return TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
#ifdef BRLCAD_DEBUG
Bu_d_Init(Tcl_Interp *interp)
#else
Bu_Init(Tcl_Interp *interp)
#endif
{
	bu_tcl_setup(interp);
#if 0
	bu_hook_list_init(&bu_log_hook_list);
	bu_hook_list_init(&bu_bomb_hook_list);
#endif
	return TCL_OK;
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
