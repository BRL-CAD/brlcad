/*
 *			T C L . C
 *
 *  Tcl interfaces to LIBRT routines.
 *
 *  LIBRT routines are not for casual command-line use;
 *  as a result, the Tcl name for the function should be exactly
 *  the same as the C name for the underlying function.
 *  Typing "rt_" is no hardship when writing Tcl procs, and
 *  clarifies the origin of the routine.
 *
 *  Authors -
 *	Michael John Muuss
 *      Glenn Durfee
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
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
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
#include "rtgeom.h"
#include "raytrace.h"
#include "externs.h"

#define RT_CK_DBI_TCL(_p)	BU_CKMAG_TCL(interp,_p,(long)DBI_MAGIC,"struct db_i")
#define RT_CK_RTI_TCL(_p)	BU_CKMAG_TCL(interp,_p,(long)RTI_MAGIC,"struct rt_i")
#define RT_CK_WDB_TCL(_p)	BU_CKMAG_TCL(interp,_p,(long)RT_WDB_MAGIC,"struct rt_wdb")

extern struct bu_structparse rt_tor_parse[];
extern struct bu_structparse rt_tgc_parse[];
extern struct bu_structparse rt_ell_parse[];
extern struct bu_structparse rt_arb8_parse[];
/* ARS -- not supported yet */
extern struct bu_structparse rt_half_parse[];
/* REC -- subsumed by TGC */
/* POLY -- not supported yet */
/* BSPLINE -- not supported yet */
/* SPH -- not supported yet */
/* NMG -- not supported yet */
extern struct bu_structparse rt_ebm_parse[];
extern struct bu_structparse rt_vol_parse[];
/* ARBN -- not supported yet */
/* PIPE -- not supported yet */
extern struct bu_structparse rt_part_parse[];
extern struct bu_structparse rt_rpc_parse[];
extern struct bu_structparse rt_rhc_parse[];
extern struct bu_structparse rt_epa_parse[];
extern struct bu_structparse rt_ehy_parse[];
extern struct bu_structparse rt_eto_parse[];
/* GRIP -- not supported yet */
/* JOINT -- not supported yet */
extern struct bu_structparse rt_hf_parse[];
extern struct bu_structparse rt_dsp_parse[];
/* COMB -- supported below */

struct rt_solid_type_lookup {
	char			id;
	size_t			db_internal_size;
	long			magic;
	char		       *label;
	struct bu_structparse  *parsetab;
} rt_solid_type_lookup[] = {
	{ ID_TOR,     sizeof(struct rt_tor_internal), (long)RT_TOR_INTERNAL_MAGIC, "tor", rt_tor_parse },
	{ ID_TGC,     sizeof(struct rt_tgc_internal), (long)RT_TGC_INTERNAL_MAGIC, "tgc", rt_tgc_parse },
	{ ID_ELL,     sizeof(struct rt_ell_internal), (long)RT_ELL_INTERNAL_MAGIC, "ell", rt_ell_parse },
	{ ID_ARB8,    sizeof(struct rt_arb_internal), (long)RT_ARB_INTERNAL_MAGIC, "arb8",rt_arb8_parse },
	{ ID_HALF,    sizeof(struct rt_half_internal),(long)RT_HALF_INTERNAL_MAGIC,"half",rt_half_parse },
	{ ID_REC,     sizeof(struct rt_tgc_internal), (long)RT_TGC_INTERNAL_MAGIC, "rec", rt_tgc_parse },
	{ ID_SPH,     sizeof(struct rt_ell_internal), (long)RT_ELL_INTERNAL_MAGIC, "sph", rt_ell_parse },
	{ ID_EBM,     sizeof(struct rt_ebm_internal), (long)RT_EBM_INTERNAL_MAGIC, "ebm", rt_ebm_parse },
	{ ID_VOL,     sizeof(struct rt_vol_internal), (long)RT_VOL_INTERNAL_MAGIC, "vol", rt_vol_parse },
	{ ID_PARTICLE,sizeof(struct rt_part_internal),(long)RT_PART_INTERNAL_MAGIC,"part",rt_part_parse },
	{ ID_RPC,     sizeof(struct rt_rpc_internal), (long)RT_RPC_INTERNAL_MAGIC, "rpc", rt_rpc_parse },
	{ ID_RHC,     sizeof(struct rt_rhc_internal), (long)RT_RHC_INTERNAL_MAGIC, "rhc", rt_rhc_parse },
	{ ID_EPA,     sizeof(struct rt_epa_internal), (long)RT_EPA_INTERNAL_MAGIC, "epa", rt_epa_parse },
	{ ID_EHY,     sizeof(struct rt_ehy_internal), (long)RT_EHY_INTERNAL_MAGIC, "ehy", rt_ehy_parse },
	{ ID_ETO,     sizeof(struct rt_eto_internal), (long)RT_ETO_INTERNAL_MAGIC, "eto", rt_eto_parse },
	{ ID_HF,      sizeof(struct rt_hf_internal),  (long)RT_HF_INTERNAL_MAGIC,  "hf",  rt_hf_parse },
	{ ID_DSP,     sizeof(struct rt_dsp_internal), (long)RT_DSP_INTERNAL_MAGIC, "dsp", rt_dsp_parse },
	{ 0, 0, 0, 0 }
};

/*
 *		R T _ G E T _ P A R S E T A B _ B Y _ I D
 */

struct rt_solid_type_lookup *
rt_get_parsetab_by_id( s_id )
int s_id;
{
	register struct rt_solid_type_lookup   *stlp;
	static struct rt_solid_type_lookup	solid_type;

	for( stlp = rt_solid_type_lookup; stlp->id != 0; stlp++ )
		if( stlp->id == s_id )
			return stlp;

	return NULL;
}

/*
 *		R T _ G E T _ P A R S E T A B _ B Y _ N A M E
 */

struct rt_solid_type_lookup *
rt_get_parsetab_by_name( s_type )
char *s_type;
{
	register int				i;
	register struct rt_solid_type_lookup   *stlp;

	for( stlp = rt_solid_type_lookup; stlp->id != 0; stlp++ )
		if( strcmp(s_type, stlp->label) == 0 )
			return stlp;

	return NULL;
}

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
 *		D B _ T C L _ T R E E _ D E S C R I B E
 *
 * Fills a Tcl_DString with a representation of the given tree appropriate
 * for processing by Tcl scripts.  The reason we use Tcl_DStrings instead
 * of bu_vlses is that Tcl_DStrings provide "start/end sublist" commands
 * and automatic escaping of Tcl-special characters.
 *
 * A tree 't' is represented in the following manner:
 *
 *	t := { l dbobjname mat }
 *	   | { u t1 t2 }
 * 	   | { n t1 t2 }
 *	   | { - t1 t2 }
 *	   | { ^ t1 t2 }
 *         | { ! t1 }
 *	   | { G t1 }
 *	   | { X t1 }
 *	   | { N }
 *	   | {}
 *
 * where 'dbobjname' is a string containing the name of a database object,
 *       'mat'       is the matrix preceeding a leaf,
 *       't1', 't2'  are trees (recursively defined).
 *
 * Notice that in most cases, this tree will be grossly unbalanced.
 */

void
db_tcl_tree_describe( dsp, tp )
Tcl_DString		*dsp;
union tree		*tp;
{
	if( !tp ) return;

	RT_CK_TREE(tp);
	switch( tp->tr_op ) {
	case OP_DB_LEAF: {
		struct bu_vls vls;
		Tcl_DStringAppendElement( dsp, "l" );
		Tcl_DStringAppendElement( dsp, tp->tr_l.tl_name );
		bu_vls_init( &vls );
		bn_encode_mat( &vls, tp->tr_l.tl_mat );
		Tcl_DStringAppendElement( dsp, bu_vls_addr(&vls) );
		bu_vls_free( &vls );
		break; }

		/* This node is known to be a binary op */
	case OP_UNION:
		Tcl_DStringAppendElement( dsp, "u" );
		goto bin;
	case OP_INTERSECT:
		Tcl_DStringAppendElement( dsp, "n" );
		goto bin;
	case OP_SUBTRACT:
		Tcl_DStringAppendElement( dsp, "-" );
		goto bin;
	case OP_XOR:
		Tcl_DStringAppendElement( dsp, "^" );
	bin:
		Tcl_DStringStartSublist( dsp );
		db_tcl_tree_describe( dsp, tp->tr_b.tb_left );
		Tcl_DStringEndSublist( dsp );

		Tcl_DStringStartSublist( dsp );
		db_tcl_tree_describe( dsp, tp->tr_b.tb_right );
		Tcl_DStringEndSublist( dsp );

		break;

		/* This node is known to be a unary op */
	case OP_NOT:
		Tcl_DStringAppendElement( dsp, "!" );
		goto unary;
	case OP_GUARD:
		Tcl_DStringAppendElement( dsp, "G" );
		goto unary;
	case OP_XNOP:
		Tcl_DStringAppendElement( dsp, "X" );
	unary:
		Tcl_DStringStartSublist( dsp );
		db_tcl_tree_describe( dsp, tp->tr_b.tb_left );
		Tcl_DStringEndSublist( dsp );
		break;
			
	case OP_NOP:
		Tcl_DStringAppendElement( dsp, "N" );
		break;

	default:
		bu_log("db_tcl_tree_describe: bad op %d\n", tp->tr_op);
		bu_bomb("db_tcl_tree_describe\n");
	}
}

/*
 *		D B _ T C L _ C O M B _ D E S C R I B E
 *
 * Sets the interp->result string to a description of the given combination.
 */

int
db_tcl_comb_describe( interp, comb, item )
Tcl_Interp		*interp;
struct rt_comb_internal *comb;
CONST char		*item;
{
	char buf[128];
	Tcl_DString	ds;
	
	if( !comb ) {
		Tcl_AppendResult( interp, "error: null combination",
				  (char *)NULL );
		return TCL_ERROR;
	}

	if( item==0 ) {
		/* Print out the whole combination. */
		Tcl_DStringInit( &ds );

		Tcl_DStringAppendElement( &ds, "comb" );
		Tcl_DStringAppendElement( &ds, "region" );
		if( comb->region_flag ) {
			Tcl_DStringAppendElement( &ds, "yes" );
			Tcl_DStringAppendElement( &ds, "id" );
			sprintf( buf, "%d", comb->region_id );
			Tcl_DStringAppendElement( &ds, buf );
			Tcl_DStringAppendElement( &ds, "air" );
			sprintf( buf, "%d", comb->aircode );
			Tcl_DStringAppendElement( &ds, buf );
			Tcl_DStringAppendElement( &ds, "los" );
			sprintf( buf, "%d", comb->los );
			Tcl_DStringAppendElement( &ds, buf );
			Tcl_DStringAppendElement( &ds, "GIFTmater" );
			sprintf( buf, "%d", comb->GIFTmater );
			Tcl_DStringAppendElement( &ds, buf );
		} else {
			Tcl_DStringAppendElement( &ds, "no" );
		}
		
		Tcl_DStringAppendElement( &ds, "rgb" );
		if( comb->rgb_valid ) {
			sprintf( buf, "%d %d %d", V3ARGS(comb->rgb) );
			Tcl_DStringAppendElement( &ds, buf );
		} else
			Tcl_DStringAppendElement( &ds, "invalid" );

		Tcl_DStringAppendElement( &ds, "shader" );
		Tcl_DStringAppendElement( &ds, bu_vls_addr(&comb->shader) );
		Tcl_DStringAppendElement( &ds, "material" );
		Tcl_DStringAppendElement( &ds, bu_vls_addr(&comb->material) );

		if( comb->inherit )
			Tcl_DStringAppendElement( &ds, "inherit" );

		Tcl_DStringAppendElement( &ds, "tree" );
		Tcl_DStringStartSublist( &ds );
		db_tcl_tree_describe( &ds, comb->tree );
		Tcl_DStringEndSublist( &ds );

		Tcl_DStringResult( interp, &ds );
		Tcl_DStringFree( &ds );

		return TCL_OK;
	} else {
		/* Print out only the requested item. */
		register int i;
		char itemlwr[128];

		for( i = 0; i < 128 && item[i]; i++ ) {
			itemlwr[i] = (isupper(item[i]) ? tolower(item[i]) :
				      item[i]);
		}
		itemlwr[i] = 0;

		if( strcmp(itemlwr, "region")==0 ) {
			strcpy( buf, comb->region_flag ? "yes" : "no" );
		} else if( strcmp(itemlwr, "id")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->region_id );
		} else if( strcmp(itemlwr, "air")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->aircode );
		} else if( strcmp(itemlwr, "los")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->los );
		} else if( strcmp(itemlwr, "giftmater")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->GIFTmater );
		} else if( strcmp(itemlwr, "rgb")==0 ) {
			if( comb->rgb_valid )
				sprintf( buf, "%d %d %d", V3ARGS(comb->rgb) );
			else
				strcpy( buf, "invalid" );
		} else if( strcmp(itemlwr, "shader")==0 ) {
			Tcl_AppendResult( interp, bu_vls_addr(&comb->shader),
					  (char *)NULL );
			return TCL_OK;
		} else if( strcmp(itemlwr, "material")==0 ) {
			Tcl_AppendResult( interp, bu_vls_addr(&comb->material),
					  (char *)NULL );
			return TCL_OK;
		} else if( strcmp(itemlwr, "inherit")==0 ) {
			strcpy( buf, comb->inherit ? "yes" : "no" );
		} else if( strcmp(itemlwr, "tree")==0 ) {
			Tcl_DStringInit( &ds );
			db_tcl_tree_describe( &ds, comb->tree );
			Tcl_DStringResult( interp, &ds );
			Tcl_DStringFree( &ds );
			return TCL_OK;
		} else {
			Tcl_AppendResult( interp, "no such attribute",
					  (char *)NULL );
			return TCL_ERROR;
		}

		Tcl_AppendResult( interp, buf, (char *)NULL );
		return TCL_OK;

	not_region:
		Tcl_AppendResult( interp, "item not valid for non-region",
				  (char *)NULL );
		return TCL_ERROR;
	}
}


/*
 *			D B _ T C L _ C O M B _ A D J U S T
 */

int
db_tcl_comb_adjust( comb, interp, argc, argv )
struct rt_comb_internal	       *comb;
Tcl_Interp		       *interp;
int				argc;
char			      **argv;
{
	char	buf[128];
	int	i;
	
	RT_CK_COMB( comb );
	

	for( i=0; i<128 && argv[0]!=0; i++ )
		buf[i] = isupper(argv[0][i])?tolower(argv[0][i]):argv[0][i];
	buf[i] = 0;

	while( argc >= 2 ) {
		if( strcmp(buf, "region")==0 ) {
			if( Tcl_GetBoolean( interp, argv[1], &i )!= TCL_OK )
				return TCL_ERROR;
			comb->region_flag = (char)i;
		} else if( strcmp(buf, "id")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK )
				return TCL_ERROR;
			comb->region_id = (short)i;
		} else if( strcmp(buf, "air")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK)
				return TCL_ERROR;
			comb->aircode = (short)i;
		} else if( strcmp(buf, "los")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK )
				return TCL_ERROR;
			comb->los = (short)i;
		} else if( strcmp(buf, "giftmater")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK )
				return TCL_ERROR;
			comb->GIFTmater = (short)i;
		} else if( strcmp(buf, "rgb")==0 ) {
			if( strcmp(argv[1], "invalid")==0 ) {
				comb->rgb[0] = comb->rgb[1] =
					comb->rgb[2] = 0;
				comb->rgb_valid = 0;
			} else {
				unsigned int r, g, b;
				sscanf( argv[1], "%u %u %u",
					&r, &g, &b );
				comb->rgb[0] = (unsigned char)r;
				comb->rgb[1] = (unsigned char)g;
				comb->rgb[2] = (unsigned char)b;
				comb->rgb_valid = 1;
			}
		} else if( strcmp(buf, "shader" )==0 ) {
			bu_vls_trunc( &comb->shader, 0 );
			bu_vls_strcat( &comb->shader, argv[1] );
			/* Leading spaces boggle the combination exporter */
			bu_vls_trimspace( &comb->shader );
		} else if( strcmp(buf, "material" )==0 ) {
			bu_vls_trunc( &comb->material, 0 );
			bu_vls_strcat( &comb->material, argv[1] );
			bu_vls_trimspace( &comb->material );
		} else if( strcmp(buf, "inherit" )==0 ) {
			if( Tcl_GetBoolean( interp, argv[1], &i ) != TCL_OK )
				return TCL_ERROR;
			comb->inherit = (char)i;
		} else if( strcmp(buf, "tree" )==0 ) {
			;
		} else {
			Tcl_AppendResult( interp, argv[0],
					  ": no such attribute",
					  (char *)NULL );
			return TCL_ERROR;
		}
		argc -= 2;
		argv += 2;
	}

	return TCL_OK;

 not_region:
	Tcl_AppendResult( interp, "attribute not valid for non-region",
			  (char *)NULL );
	return TCL_ERROR;
}

/*
 *			R T _ D B _ M A T C H
 *
 * Returns (in interp->result) a list (possibly empty) of all matches to
 * the (possibly wildcard-containing) arguments given.
 * Does *NOT* return tokens that do not match anything, unlike the
 * "expand" command.
 */

int
rt_db_match( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_wdb  *wdb = (struct rt_wdb *)clientData;
	struct bu_vls	matches;

	RT_CK_WDB_TCL(wdb);
	
	/* Verify that this wdb supports lookup operations
	   (non-null dbip) */
	if( wdb->dbip == 0 ) {
		Tcl_AppendResult( interp, "this database does not support lookup operations" );
		return TCL_ERROR;
	}

	bu_vls_init( &matches );
	for( ++argv; *argv != NULL; ++argv ) {
		if( db_regexp_match_all( &matches, wdb->dbip, *argv ) > 0 )
			bu_vls_strcat( &matches, " " );
	}
	bu_vls_trimspace( &matches );
	Tcl_AppendResult( interp, bu_vls_addr(&matches), (char *)NULL );
	bu_vls_free( &matches );
	return TCL_OK;
}

/*
 *			R T _ D B _ G E T
 *
 **
 ** For use with Tcl, this routine accepts as its first argument the name
 ** of an object in the database.  If only one argument is given, this routine
 ** then fills the result string with the (minimal) attributes of the item.
 ** If a second, optional, argument is provided, this function looks up the
 ** property with that name of the item given, and returns it as the result
 ** string.
 **/

int
rt_db_get( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	register struct directory	       *dp;
	register struct bu_structparse	       *sp = NULL;
	register struct rt_solid_type_lookup   *stlp;
	int			id, status;
	struct rt_db_internal	intern;
	char			objecttype;
	char		       *objname;
	struct rt_wdb	       *wdb = (struct rt_wdb *)clientData;
	Tcl_DString		ds;
	struct bu_vls		str;

	if( argc < 2 || argc > 3) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"", argv[0],
				  " objName ?attr?\"", (char *)NULL );
		return TCL_ERROR;
	}

	/* Verify that this wdb supports lookup operations
	   (non-null dbip) */
	if( wdb->dbip==0 ) {
		Tcl_AppendResult( interp,
				  "db does not support lookup operations",
				  (char *)NULL );
		return TCL_ERROR;
	}

	dp = db_lookup( wdb->dbip, argv[1], LOOKUP_QUIET );
	if( dp == NULL ) {
		Tcl_AppendResult( interp, argv[1], ": not found\n",
				  (char *)NULL );
		return TCL_ERROR;
	}

	status = rt_db_get_internal( &intern, dp, wdb->dbip, (matp_t)NULL );
	if( status < 0 ) {
		Tcl_AppendResult( interp, "rt_db_get_internal failure: ",
				  argv[1], (char *)NULL );
		return TCL_ERROR;
	}
	RT_CK_DB_INTERNAL( &intern );

       /* Find out what type of object we are dealing with and report on it. */
	id = intern.idb_type;
	switch( id ) {
	case ID_COMBINATION:
		status = db_tcl_comb_describe( interp,
			       (struct rt_comb_internal *)intern.idb_ptr,
					       argv[2] );
		break;
	default:
		if( (stlp=rt_get_parsetab_by_id(id)) == NULL ) {
			Tcl_AppendResult( interp,
 "invalid {an output routine for this data type has not yet been implemented}",
				  (char *)NULL );
			return TCL_OK;
		}

		bu_vls_init( &str );
		Tcl_DStringInit( &ds );

		sp = stlp->parsetab;
		if( argc == 2 ) {
			/* Print out solid type and all attributes */
			Tcl_DStringAppendElement( &ds, stlp->label );
			while( sp->sp_name != NULL ) {
				Tcl_DStringAppendElement( &ds, sp->sp_name );
				bu_vls_trunc( &str, 0 );
				bu_vls_struct_item( &str, sp,
						 (char *)intern.idb_ptr, ' ' );
				Tcl_DStringAppendElement( &ds,
							  bu_vls_addr(&str) );
				++sp;
			}
			status = TCL_OK;
		} else {
			if( bu_vls_struct_item_named( &str, sp, argv[2],
					   (char *)intern.idb_ptr, ' ') < 0 ) {
				Tcl_DStringAppend( &ds, "no such attribute",
						   17 );
				status = TCL_ERROR;
			} else {
				Tcl_DStringAppendElement( &ds,
							  bu_vls_addr(&str) );
				status = TCL_OK;
			}
		}

		Tcl_DStringResult( interp, &ds );
		Tcl_DStringFree( &ds );
		bu_vls_free( &str );
	}

	rt_functab[id].ft_ifree( &intern );
	return status;
}

/*
 *			R T _ D B _ P U T
 **
 ** Creates an object and stuffs it into the databse.
 ** All arguments must be specified.  Object cannot already exist.
 **/

int
rt_db_put(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_db_internal			intern;
	register struct rt_solid_type_lookup   *stlp;
	register struct directory	       *dp;
	int					status, ngran, id, i;
	char				       *name;
	struct rt_wdb			  *wdb = (struct rt_wdb *)clientData;
	char				        type[16];

    
	if( argc < 2 ) {
		Tcl_AppendResult( interp,
 	               "wrong # args: should be db put objName objType attrs",
				  (char *)NULL );
		return TCL_ERROR;
	}

	name = argv[1];
    
	/* XXX
	   Verify that this wdb supports lookup operations (non-null dbip) */
	/* If not, just skip the lookup test and write the object */

	if( db_lookup(wdb->dbip, argv[1], LOOKUP_QUIET) != DIR_NULL ) {
		Tcl_AppendResult(interp, argv[1], " already exists",
				 (char *)NULL);
		return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL(&intern);

	for( i = 0; argv[2][i] != 0 && i < 16; i++ ) {
		type[i] = isupper(argv[2][i]) ? tolower(argv[2][i]) :
			  argv[2][i];
	}
	type[i] = 0;

	if( strcmp(type, "comb")==0 ) {
		struct rt_comb_internal *comb;
		id = intern.idb_type = ID_COMBINATION;
		intern.idb_ptr = bu_malloc( sizeof(struct rt_comb_internal),
					    "rt_db_put" );
		/* Create combination with appropriate values */
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		comb->magic = (long)RT_COMB_MAGIC;
		comb->tree = (union tree *)0;
		comb->region_flag = 1;
		comb->region_id = 0;
		comb->aircode = 0;
		comb->GIFTmater = 0;
		comb->los = 0;
		comb->rgb_valid = 0;
		comb->rgb[0] = comb->rgb[1] = comb->rgb[2] = 0;
		bu_vls_init( &comb->shader );
		bu_vls_init( &comb->material );
		comb->inherit = 0;

		if( db_tcl_comb_adjust( comb, interp, argc-3,
					argv+3 ) == TCL_ERROR ) {
			rt_db_free_internal( &intern );
			return TCL_ERROR;
		}
	} else {
		stlp = rt_get_parsetab_by_name( type );
		if( stlp == NULL ) {
			Tcl_AppendResult( interp,
					  "unknown object type",
					  (char *)NULL );
			return TCL_ERROR;
		}

		id = intern.idb_type = stlp->id;
		intern.idb_ptr = bu_malloc( stlp->db_internal_size,
					    "rt_db_put" );
		*((long *)intern.idb_ptr) = stlp->magic;
		if( bu_structparse_argv(interp, argc-3, argv+3, stlp->parsetab,
					(char *)intern.idb_ptr )==TCL_ERROR ) {
			rt_db_free_internal(&intern);
			return TCL_ERROR;
		}
	}

	if( wdb_export( wdb, name, intern.idb_ptr, intern.idb_type,
			1.0 ) < 0 )  {
		Tcl_AppendResult( interp, "wdb_export(", argv[1],
				  ") failure\n", (char *)NULL );
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}

	rt_db_free_internal( &intern );
	return TCL_OK;
}

/*
 *			R T _ D B _ A D J U S T
 *
 **
 ** For use with Tcl, this routine accepts as its first argument an item in
 ** the database; as its remaining arguments it takes the properties that
 ** need to be changed and their values.
 **/

int
rt_db_adjust( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	register struct directory	*dp;
	register struct bu_structparse	*sp = NULL;
	int				 id, status, i;
	char			        *name = argv[1];
	struct rt_db_internal		 intern;
	mat_t				 idn;
	char				 objecttype;
	struct rt_wdb		        *wdb = (struct rt_wdb *)clientData;

	if( argc < 4 ) {
		Tcl_AppendResult( interp,
		"wrong # args: should be \"db adjust objName attr value ?attr? ?value?...\"",
				  (char *)NULL );
		return TCL_ERROR;
	}

	/* XXX
	   Verify that this wdb supports lookup operations (non-null dbip) */
	RT_CK_DBI_TCL(wdb->dbip);

	dp = db_lookup( wdb->dbip, argv[1], LOOKUP_QUIET );
	if( dp == DIR_NULL ) {
		Tcl_AppendResult( interp, argv[1], ": not found\n",
				  (char *)NULL );
		return TCL_ERROR;
	}

	status = rt_db_get_internal( &intern, dp, wdb->dbip, (matp_t)NULL );
	if( status < 0 ) {
		Tcl_AppendResult( interp, "rt_db_get_internal(", argv[1],
				  ") failure\n", (char *)NULL );
		return TCL_ERROR;
	}
	RT_CK_DB_INTERNAL( &intern );

	/* Find out what type of object we are dealing with and tweak it. */
	id = intern.idb_type;

	if( id == ID_COMBINATION ) {
		struct rt_comb_internal *comb =
			(struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);

		status = db_tcl_comb_adjust( comb, interp, argc+2, argv-2 );
	} else if( rt_get_parsetab_by_id(id) == NULL ||
		   (sp=rt_get_parsetab_by_id(id)->parsetab) == NULL ) {
		Tcl_AppendResult( interp,
           "manipulation routines for this type have not yet been implemented",
				  (char *)NULL );
		status = TCL_ERROR;
	} else {
		/* If we were able to find an entry in on the "cheat sheet",
		   just use the handy parse functions to return the object. */

		status = bu_structparse_argv( interp, argc-2, argv+2, sp,
					      (char *)intern.idb_ptr );
	}

	if( status == TCL_OK && wdb_export( wdb, name, intern.idb_ptr,
					    intern.idb_type, 1.0 ) < 0 )  {
		Tcl_AppendResult( interp, "wdb_export(", name,
				  ") failure\n", (char *)NULL );
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}

	rt_db_free_internal( &intern );
	return status;
}

/*
 *			R T _ D B _ F O R M
 */
int
rt_db_form( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	register struct bu_structparse *sp = NULL;
	register struct rt_solid_type_lookup *stlp;
	register int i;
	struct bu_vls str;
	register char *cp;

	if (argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"db form objType\"",
				 (char *)NULL);
		return TCL_ERROR;
	}

	bu_vls_init(&str);
	sp = rt_get_parsetab_by_name(argv[1]) == NULL ? NULL :
		rt_get_parsetab_by_name(argv[1])->parsetab;
    
	if (sp != NULL)
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
	else {
		Tcl_AppendResult(interp, "a form routine for this data type has not \
yet been implemented", (char *)NULL);
		goto error;
	}

	bu_vls_free(&str);
	return TCL_OK;

error:
	bu_vls_free(&str);
	return TCL_ERROR;
}

/*
 *			R T _ D B _ C L O S E
 */
int
rt_db_close( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_wdb		        *wdbp = (struct rt_wdb *)clientData;

	RT_CK_WDB_TCL(wdbp);

	wdb_close(wdbp);		/* frees memory */
	wdbp = NULL;

	/* De-register Tcl command */
bu_log("De-registering Tcl command '%s'\n", argv[-1] );
	Tcl_DeleteCommand( interp, argv[-1] );
	return TCL_OK;
}


/*
 *			R T _ D B _ T O P S
 */

int
rt_db_tops( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_wdb *wdp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int i;

	RT_CK_WDB_TCL( wdp );
	RT_CK_DBI_TCL( wdp->dbip );

	/* Can this be executed only sometimes?
	   Perhaps a "dirty bit" on the database? */
	db_update_nref( wdp->dbip );
	
	for( i = 0; i < RT_DBNHASH; i++ )
		for( dp = wdp->dbip->dbi_Head[i];
		     dp != DIR_NULL;
		     dp = dp->d_forw )
			if( dp->d_nref == 0 )
				Tcl_AppendElement( interp, dp->d_namep );
	return TCL_OK;
}

/*
 *			R T _ D B
 *
 * Generic interface for the database_class manipulation routines.
 * Usage:
 *        widget_command dbCmdName ?args?
 * Returns: result of cmdName database command.
 */

int
rt_db( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct dbcmdstruct	*dbcmd;
	struct rt_wdb		*wdb = (struct rt_wdb *)clientData;
	struct dbcmdstruct {
		char *cmdname;
		int (*cmdfunc)();
	} rt_db_cmds[] = {
		"match",	rt_db_match,
		"get",		rt_db_get,
		"put",		rt_db_put,
		"adjust",	rt_db_adjust,
		"form",		rt_db_form,
		"tops",		rt_db_tops,
		"close",	rt_db_close,
		(char *)0,	(int (*)())0
	};

	if( argc < 2 ) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"", argv[0],
				  " command [args...]\"",
				  (char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_CK_WDB_TCL(wdb);

	Tcl_AppendResult( interp, "unknown database command; must be one of:",
			  (char *)NULL );
	for( dbcmd = rt_db_cmds; dbcmd->cmdname != NULL; dbcmd++ ) {
		if( strcmp(dbcmd->cmdname, argv[1]) == 0 ) {
			/* hack: dispose of error msg if OK */
			Tcl_ResetResult( interp );
			return (*dbcmd->cmdfunc)( clientData, interp,
						  argc-1, argv+1 );
		}
		Tcl_AppendResult( interp, " ", dbcmd->cmdname, (char *)NULL );
	}

	return TCL_ERROR;
}

/*
 *			W D B _ O P E N
 *
 *  A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 *  Implicit return -
 *	Creates a new TCL proc which responds to get/put/etc. arguments
 *	when invoked.  clientData of that proc will be wdb pointer
 *	for this instance of the database.
 *	Easily allows keeping track of multiple databases.
 *
 *  Explicit return -
 *	wdb pointer, for more traditional C-style interfacing.
 *
 *  Example -
 *	set wdbp [wdb_open .inmem inmem $dbip]
 *	.inmem get box.s
 *	.inmem close
 */
int
wdb_open( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
	struct rt_wdb	*wdb;
	char		buf[32];

	if( argc != 4 )  {
		Tcl_AppendResult(interp, "\
Usage: wdb_open widget_command file filename\n\
       wdb_open widget_command disk $dbip\n\
       wdb_open widget_command disk_append $dbip\n\
       wdb_open widget_command inmem $dbip\n\
       wdb_open widget_command inmem_append $dbip\n",
		NULL);
		return TCL_ERROR;
	}

	if( strcmp( argv[2], "file" ) == 0 )  {
		wdb = wdb_fopen( argv[3] );
	} else {
		struct db_i	*dbip;

		dbip = (struct db_i *)atol( argv[3] );
		/* Could core dump */
		RT_CK_DBI_TCL(dbip);

		if( strcmp( argv[2], "disk" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK );
		} else if( strcmp( argv[2], "disk_append" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY );
		} else if( strcmp( argv[2], "inmem" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_INMEM );
		} else if( strcmp( argv[2], "inmem_append" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY );
		} else {
			Tcl_AppendResult(interp, "wdb_open ", argv[2],
				" target type not recognized\n", NULL);
			return TCL_ERROR;
		}
	}
	if( wdb == RT_WDB_NULL )  {
		Tcl_AppendResult(interp, "wdb_open ", argv[1], " failed\n", NULL);
		return TCL_ERROR;
	}

	/* Instantiate the widget_command, with clientData of wdb */
	/* XXX should we see if it exists first? default=overwrite */
	/* XXX Should provide delete proc to free up wdb */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand( interp, argv[1], rt_db,
				 (ClientData)wdb, (Tcl_CmdDeleteProc *)NULL );

	/* Return new function name as result */
	Tcl_AppendResult( interp, argv[1], (char *)NULL );
	
	return TCL_OK;
}

/*
 *			R T _ T C L _ S E T U P
 *
 *  Add all the supported Tcl interfaces to LIBRT routines to
 *  the list of commands known by the given interpreter.
 */
void
rt_tcl_setup(interp)
Tcl_Interp *interp;
{
	/* XXX All these are temporary hacks, don't get dependent on
	 * XXX them just yet.  -Mike.
	 */

	(void)Tcl_CreateCommand(interp, "wdb_open", wdb_open,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_SetVar(interp, "rt_version", (char *)rt_version+5, TCL_GLOBAL_ONLY);
}
