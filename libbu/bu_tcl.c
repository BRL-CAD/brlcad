/*
 *			B U _ T C L . C
 *
 *  Tcl interfaces to all the LIBBU Basic BRL-CAD Utility routines.
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
static char RCSid[] = "@(#)$Header$ (ARL)";
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
		sprintf(buf, "ERROR: NULL %s pointer, file %s, line %d\n", 
			str, file, line ); 
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	if( *((long *)(ptr)) != (magic) )  { 
		sprintf(buf, "ERROR: bad pointer x%x: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n", 
			ptr,
			str, magic,
			bu_identify_magic( *(ptr) ), *(ptr),
			file, line ); 
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	Tcl_AppendResult(interp, "bu_badmagic_tcl() mysterious error condition, ", str, " pointer, ", file, "\n", NULL);
}



/*
 *			B U _ S T R U C T P A R S E _ A R G V
 *
 * Support routine for db adjust and db put.  Much like bu_structparse routine,
 * but takes the arguments as lists, a more Tcl-friendly method.
 * Also knows about the Tcl result string, so it can make more informative
 * error messages.
 * XXX move to libbu/bu_tcl.c
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

					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp )
						++cp;
			
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
					/* Skip the separator(s) */
					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp ) 
						++cp;
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
					
					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp )
						++cp;

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

					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp )
						++cp;
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
			bu_vls_printf( &str, "invalid attribute %s", argv[0] );
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

	Tcl_SetVar(interp, "bu_version", (char *)bu_version+5, TCL_GLOBAL_ONLY);	/* from vers.c */
	Tcl_SetVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
	Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_LINK_INT );
}
