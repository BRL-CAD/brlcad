/*
 *			U T I L I T Y 1 . C
 *
 *  Functions -
 *	f_tables()	control routine for building ascii tables
 *	tables()	builds ascii summary tables
 *	f_edcodes()	control routine for editing region ident codes
 *	edcodes()	allows for easy editing of region ident codes
 *	f_which_id()	lists all regions with given ident number
 *
 *  Author -
 *	Keith A. Applin
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <pwd.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./sedit.h"

extern struct bn_tol    mged_tol;       /* from ged.c */

int readcodes(), writecodes();
int loadcodes(), printcodes();
void		tables(), edcodes(), changes(), prfield();

#define LINELEN 256
#define MAX_LEVELS 12
struct directory *path[MAX_LEVELS];

/* structure to distinguish new solids from existing (old) solids */
struct identt {
	int	i_index;
	char	i_name[NAMESIZE+1];
	mat_t	i_mat;
};
struct identt identt, idbuf;

#define ABORTED		-99
#define OLDSOLID	0
#define NEWSOLID	1
#define SOL_TABLE	1
#define REG_TABLE	2
#define ID_TABLE	3

/*
 *
 *	F _ T A B L E S :	control routine to build ascii tables
 *
 *
 */

char operate;
int regflag, numreg, lastmemb, numsol, old_or_new, oper_ok;
int discr[MAXARGS], idfd, rd_idfd;
int flag;	/* which type of table to make */
FILE	*tabptr;

char ctemp[7];
/*
 *
 *	F _ E D C O D E S ( )
 *
 *		control routine for editing region ident codes
 *
 *
 */
int
f_edcodes(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
  int i;
  int status;
  char *tmpfil = "/tmp/GED.aXXXXX";
  char **av;

  if(dbip == DBI_NULL)
    return TCL_OK;

  CHECK_READ_ONLY;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help edcodes");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  (void)mktemp(tmpfil);
  i=creat(tmpfil, 0600);
  if( i < 0 ){
    perror(tmpfil);
    return TCL_ERROR;
  }

  (void)close(i);

  av = (char **)bu_malloc(sizeof(char *)*(argc + 2), "f_edcodes: av");
  av[0] = "wcodes";
  av[1] = tmpfil;
  for(i = 2; i < argc + 1; ++i)
    av[i] = argv[i-1];

  av[i] = NULL;

  if( f_wcodes(clientData, interp, argc + 1, av) == TCL_ERROR ){
    (void)unlink(tmpfil);
    bu_free((genptr_t)av, "f_edcodes: av");
    return TCL_ERROR;
  }

	if( regflag == ABORTED )
	{
		Tcl_AppendResult(interp, "f_edcodes: nesting is too deep\n", (char *)NULL );
		(void)unlink(tmpfil);
		return TCL_ERROR;
	}

  if( editit(tmpfil) ){
    regflag = lastmemb = 0;
    av[0] = "rcodes";
    av[2] = NULL;
    status = f_rcodes(clientData, interp, 2, av);
  }else
    status = TCL_ERROR;

  (void)unlink(tmpfil);
  bu_free((genptr_t)av, "f_edcodes: av");
  return status;
}


/* write codes to a file */
int
f_wcodes(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
  register int i;
  int status;
  FILE *fp;
  register struct directory *dp;

  if(dbip == DBI_NULL)
    return TCL_OK;

  if(argc < 3 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help wcodes");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if((fp = fopen(argv[1], "w")) == NULL){
    Tcl_AppendResult(interp, "f_wcodes: Failed to open file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  regflag = lastmemb = 0;
  for(i = 2; i < argc; ++i){
    if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) != DIR_NULL ){
      status = printcodes(fp, dp, 0);

      if(status == TCL_ERROR){
	(void)fclose(fp);
	return TCL_ERROR;
      }
    }
  }

  (void)fclose(fp);
  return TCL_OK;
}

/* read codes from a file and load them into the database */
int
f_rcodes(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int		 argc;
char		*argv[];
{
  int item, air, mat, los;
  char name[MAX_LEVELS * NAMESIZE];
  char line[LINELEN];
  char *cp;
  FILE *fp;
  register struct directory *dp;
  struct rt_db_internal intern;
  struct rt_comb_internal *comb;

  if(dbip == DBI_NULL)
    return TCL_OK;

  CHECK_READ_ONLY;

  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help rcodes");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if((fp = fopen(argv[1], "r")) == NULL){
    Tcl_AppendResult(interp, "f_rcodes: Failed to read file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  while(fgets( line , LINELEN, fp ) != NULL){
    if(sscanf(line, "%d%d%d%d%s", &item, &air, &mat, &los, name) != 5)
      continue; /* not useful */

    /* skip over the path */
    if((cp = strrchr(name, (int)'/')) == NULL)
      cp = name;
    else
      ++cp;

    if(*cp == '\0')
      continue;

    if((dp = db_lookup( dbip, cp, LOOKUP_NOISY )) == DIR_NULL){
      Tcl_AppendResult(interp, "f_rcodes: Warning - ", cp, " not found in database.\n",
		       (char *)NULL);
      continue;
    }

  	if( !(dp->d_flags & DIR_REGION) )
  	{
  		Tcl_AppendResult(interp, "f_rcodes: Warning ", cp, " not a region\n", (char *)NULL );
  		continue;
  	}

  	if( rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL ) != ID_COMBINATION )
  	{
  		Tcl_AppendResult(interp, "f_rcodes: Warning ", cp, " not a region\n", (char *)NULL );
  		continue;
  	}

  	comb = (struct rt_comb_internal *)intern.idb_ptr;

  	/* make the changes */
  	comb->region_id = item;
  	comb->aircode = air;
  	comb->GIFTmater = mat;
  	comb->los = los;

  	/* write out all changes */
  	if( rt_db_put_internal( dp, dbip, &intern ) )
  	{
  		Tcl_AppendResult(interp, "Database write error, aborting.\n", (char *)NULL );
  		TCL_ERROR_RECOVERY_SUGGESTION;
  		rt_comb_ifree( comb );
  		return TCL_ERROR;
  	}

  }

  return TCL_OK;
}

HIDDEN void
Do_printnode( dbip, comb, comb_leaf, user_ptr1, user_ptr2, user_ptr3 )
struct db_i		*dbip;
struct rt_comb_internal *comb;
union tree		*comb_leaf;
genptr_t		user_ptr1, user_ptr2, user_ptr3;
{
	FILE *fp;
	int *pathpos;
	struct directory *nextdp;

	RT_CK_DBI( dbip );
	RT_CK_TREE( comb_leaf );

	if( (nextdp=db_lookup( dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
		return;

	fp = (FILE *)user_ptr1;
	pathpos = (int *)user_ptr2;

	/* recurse on combinations */
	if( nextdp->d_flags & DIR_COMB )
		(void)printcodes( fp, nextdp, (*pathpos)+1 );
}

int
printcodes(fp, dp, pathpos)
FILE *fp;
struct directory *dp;
int pathpos;
{
	int i;
	int status;
	int nparts;
	struct directory *nextdp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	int id;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	if(pathpos >= MAX_LEVELS)
	{
		regflag = ABORTED;
		return TCL_ERROR;
	}

	if( !(dp->d_flags & DIR_COMB) )
		return( 0 );

	if( (id=rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL ) ) < 0 )
	{
		Tcl_AppendResult(interp, "printcodes: Cannot get records for ",
			dp->d_namep, "\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( id != ID_COMBINATION )
		return TCL_OK;

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	if( comb->region_flag )
	{
		fprintf(fp, "%-6d%-3d%-3d%-4d  ",
			comb->region_id,
			comb->aircode,
			comb->GIFTmater,
			comb->los );
		for(i=0; i < pathpos; i++)
			fprintf(fp, "/%s",path[i]->d_namep);
		fprintf(fp, "/%s\n", dp->d_namep );
		rt_comb_ifree( &intern );
		return TCL_OK;
	}

	if( comb->tree )
	{
		path[pathpos] = dp;
		db_tree_funcleaf( dbip, comb, comb->tree, Do_printnode,
			(genptr_t)fp, (genptr_t)&pathpos, (genptr_t)NULL );
	}

	rt_comb_ifree( &intern );
	return TCL_OK;
}

/*    C H E C K      -     compares solids       returns 1 if they match
							 0 otherwise
 */

check( a, b )
register char *a, *b;
{

	register int	c= sizeof( struct identt );

	while( c-- )	if( *a++ != *b++ ) return( 0 );	/* no match */
	return( 1 );	/* match */

}

/*
 *	F _ W H I C H _ I D ( ) :	finds all regions with given idents
 */
int
f_which_id(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register int	i,j;
	register struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	int		item;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	if(argc < 2 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help whichid");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	for( j=1; j<argc; j++) {
		item = atoi( argv[j] );
		Tcl_AppendResult(interp, "Region[s] with ident ", argv[j],
				 ":\n", (char *)NULL);

		/* Examine all COMB nodes */
		for( i = 0; i < RT_DBNHASH; i++ )  {
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
				if( !(dp->d_flags & DIR_REGION) )
					continue;
				if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
				{
					(void)signal( SIGINT, SIG_IGN );
					TCL_READ_ERR_return;
				}
				comb = (struct rt_comb_internal *)intern.idb_ptr;
				if( comb->region_id != item )
					continue;

				Tcl_AppendResult(interp, "   ", dp->d_namep,
						 "\n", (char *)NULL);

				rt_comb_ifree( &intern );
			}
		}
	}

	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
}
/*		F _ W H I C H _ S H A D E R
 *
 *	Finds all combinations using the given shaders
 */
int
f_which_shader(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register int	i,j;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int item;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	if(argc < 2 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help which_shader");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	for( j=1; j<argc; j++) {
		item = atoi( argv[j] );
		Tcl_AppendResult(interp, "Combination[s] with shader ", argv[j],
				 ":\n", (char *)NULL);

		/* Examine all COMB nodes */
		for( i = 0; i < RT_DBNHASH; i++ )  {
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
				if( !(dp->d_flags & DIR_COMB) )
					continue;

				if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )  {
					(void)signal( SIGINT, SIG_IGN );
					TCL_READ_ERR_return;
				}
				comb = (struct rt_comb_internal *)intern.idb_ptr;

				if( !strstr( bu_vls_addr( &comb->shader ), argv[j] ) )
					continue;

				Tcl_AppendResult(interp, "   ", dp->d_namep,
						 "\n", (char *)NULL);
				rt_comb_ifree( &intern );
			}
		}
	}

	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
}

/*
 *	F _ W H I C H _ A I R ( ) :	finds all regions with given air codes
 */
int
f_which_air(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register int	i,j;
	register struct directory *dp;
	int		item;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	if(argc < 2 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help whichair");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	for( j=1; j<argc; j++) {
		item = atoi( argv[j] );
		Tcl_AppendResult(interp, "Region[s] with air code ", argv[j],
				 ":\n", (char *)NULL);

		/* Examine all COMB nodes */
		for( i = 0; i < RT_DBNHASH; i++ )  {
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
				if( !(dp->d_flags & DIR_REGION) )
					continue;
				if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
				{
					(void)signal( SIGINT, SIG_IGN );
					TCL_READ_ERR_return;
				}
				comb = (struct rt_comb_internal *)intern.idb_ptr;
				if( comb->region_id != 0 || comb->aircode != item )
					continue;

				Tcl_AppendResult(interp, "   ", dp->d_namep,
						 "\n", (char *)NULL);
			}
		}
	}

	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
}

/*		F _ D E C O M P O S E
 *
 *	decompose an NMG object into shells,
 *	making a new NMG object for each shell.
 *	This is not just copying each shell from the NMG object into a new
 *	object. The NMG object is actually disassembled and each face
 *	is placed into an appropriate shell so that the end product is a
 *	group of shell(s) that can be described as exterior shells and interior
 *	void shells.
 */

int
f_decompose(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	int count=0;
	char solid_name[NAMESIZE];
	char *nmg_solid_name;
	char *prefix;
	char *def_prefix="sh";
	struct model *m;
	struct nmgregion *r;
	struct model *new_m;
	struct nmgregion *tmp_r;
	struct shell *kill_s;
	struct directory *dp;
	struct rt_db_internal nmg_intern;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	CHECK_READ_ONLY;

	if(argc < 2 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help decompose");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	nmg_solid_name = argv[1];

	if( argc > 2 )
	{
		prefix = argv[2];
		if( strlen( prefix ) >= (NAMESIZE-3) )
		{
			Tcl_AppendResult(interp, "Prefix ", prefix, " is too long", (char *)NULL );
			return TCL_ERROR;
		}
	}
	else
		prefix = def_prefix;;

	if( (dp=db_lookup( dbip, nmg_solid_name, LOOKUP_NOISY ) ) == DIR_NULL )
		return TCL_ERROR;

	if( rt_db_get_internal( &nmg_intern, dp, dbip, bn_mat_identity ) < 0 )
	{
		Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
		return TCL_ERROR;
	}

	if( nmg_intern.idb_type != ID_NMG )
	{
		Tcl_AppendResult(interp, nmg_solid_name, " is not an NMG solid!", (char *)NULL );
		return TCL_ERROR;
	}

	m = (struct model *)nmg_intern.idb_ptr;
	NMG_CK_MODEL(m);

	/* create temp region to hold duplicate shell */
	tmp_r = nmg_mrsv( m );	/* temp nmgregion to hold dup shells */
	kill_s = BU_LIST_FIRST( shell, &tmp_r->s_hd );
	(void)nmg_ks( kill_s );

	for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
	{
		struct shell *s;

		if( r == tmp_r )
			continue;

		for( BU_LIST_FOR( s, shell, &r->s_hd ) )
		{
			struct shell *tmp_s;
			struct shell *decomp_s;
			long *trans_tbl;

			/* duplicate shell */
			tmp_s = (struct shell *)nmg_dup_shell( s, &trans_tbl, &mged_tol );
			bu_free( (char *)trans_tbl, "trans_tbl" );

			 /* move duplicate to temp region */
			(void) nmg_mv_shell_to_region( tmp_s, tmp_r );

			/* decompose this shell */
			(void) nmg_decompose_shell( tmp_s, &mged_tol );

			/* move each decomposed shell to yet another region */
			decomp_s = BU_LIST_FIRST( shell, &tmp_r->s_hd );
			while( BU_LIST_NOT_HEAD( &decomp_s->l, &tmp_r->s_hd ) )
			{
				struct shell *next_s;
				struct shell *new_s;
				struct rt_db_internal new_intern;
				struct directory *new_dp;
				struct nmgregion *decomp_r;
				char shell_no[32];
				int end_prefix;

				next_s = BU_LIST_NEXT( shell, &decomp_s->l );

				decomp_r = nmg_mrsv( m );
				kill_s = BU_LIST_FIRST( shell, &decomp_r->s_hd );
				(void)nmg_ks( kill_s );
				nmg_shell_a( decomp_s, &mged_tol );
				new_s = (struct shell *)nmg_dup_shell( decomp_s, &trans_tbl, &mged_tol );
				(void)nmg_mv_shell_to_region( new_s, decomp_r );

				/* move this region to a different model */
				new_m = (struct model *)nmg_mk_model_from_region( decomp_r, 1 );
				(void)nmg_rebound( new_m, &mged_tol );

				/* create name for this shell */
				count++;
				strcpy( solid_name, prefix );
				sprintf( shell_no, "_%d", count );
				end_prefix = strlen( prefix );
				if( end_prefix + strlen( shell_no ) >= NAMESIZE )
					end_prefix = NAMESIZE - strlen( shell_no );
				solid_name[end_prefix] = '\0';
				strncat( solid_name, shell_no, NAMESIZE-strlen(solid_name)-1 );

				if( db_lookup( dbip, solid_name, LOOKUP_QUIET ) != DIR_NULL )
				{
					Tcl_AppendResult(interp, "decompose: cannot create unique solid name (", solid_name, ")", (char *)NULL );
					Tcl_AppendResult(interp, "decompose: failed" );
					return TCL_ERROR;
				}

				/* write this model as a seperate nmg solid */
				if( (new_dp=db_diradd( dbip, solid_name, -1, 0, DIR_SOLID)) == DIR_NULL )
				{
					TCL_ALLOC_ERR;
					return TCL_ERROR;;
				}

				RT_INIT_DB_INTERNAL( &new_intern );
				new_intern.idb_type = ID_NMG;
				new_intern.idb_ptr = (genptr_t)new_m;

				if( rt_db_put_internal( new_dp, dbip, &new_intern ) < 0 )
				{
					(void)nmg_km( new_m );
					Tcl_AppendResult(interp, "rt_db_put_internal() failure\n", (char *)NULL);
					return TCL_ERROR;
				}

				(void)nmg_ks( decomp_s );
				decomp_s = next_s;
			}
		}
	}

	rt_db_free_internal( &nmg_intern );

	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
}

HIDDEN int
sol_number( matrix, name, old )
matp_t matrix;
char *name;
int *old;
{
	int ret_sol_no=0;
	int i;
	struct identt idbuf1, idbuf2;

	bzero( &idbuf1, sizeof( struct identt ) );
	(void)strncpy(idbuf1.i_name, name, NAMESIZE);
	bn_mat_copy(idbuf1.i_mat, matrix);

	for( i=0 ; i<numsol ; i++ )
	{
		(void)lseek(rd_idfd, i*(long)sizeof identt, 0);
		(void)read(rd_idfd, &idbuf2, sizeof identt);

		idbuf1.i_index = i + 1;

		if( check( (char *)&idbuf1, (char *)&idbuf2 ) == 1 )
		{
			*old = 1;
			return( idbuf2.i_index );
		}
	}
	numsol++;
	idbuf1.i_index = numsol;

	(void)lseek(idfd, (off_t)0L, 2);
	(void)write(idfd, &idbuf1, sizeof identt);

	*old = 0;
	return( idbuf1.i_index );
}

HIDDEN void
new_tables( dp, cur_path, old_mat, flag)
struct directory *dp;
struct bu_ptbl *cur_path;
mat_t old_mat;
int flag;
{
	struct rt_db_internal intern;	
	struct rt_comb_internal *comb;
	struct rt_tree_array *tree_list;
	int node_count;
	int actual_count;
	int i,k;

	if(dbip == DBI_NULL)
	  return;

	RT_CK_DIR( dp );
	BU_CK_PTBL( cur_path );

	if( dp->d_flags & DIR_SOLID )
		return;

	if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
		READ_ERR_return;

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 )
	{
		db_non_union_push( comb->tree );
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
		{
			Tcl_AppendResult(interp, "Cannot flatten tree for editing\n", (char *)NULL );
			rt_comb_ifree( comb );
			return;
		}
	}

	if( !comb->tree )
	{
		/* empty combination */
		rt_comb_ifree( &intern );
		return;
	}

	node_count = db_tree_nleaves( comb->tree );
	tree_list = (struct rt_tree_array *)bu_calloc( node_count,
		sizeof( struct rt_tree_array ), "tree list" );

	/* flatten tree */
	actual_count = (struct rt_tree_array *)db_flatten_tree( tree_list, comb->tree, OP_UNION ) - tree_list;
	if( actual_count > node_count )  bu_bomb("combadd() array overflow!");
	if( actual_count < node_count )  bu_log("WARNING combadd() array underflow! %d", actual_count, node_count);

	if( dp->d_flags & DIR_REGION )
	{
		numreg++;
		(void)fprintf( tabptr, " %-4d %4d %4d %4d %4d  ",
			numreg, comb->region_id, comb->aircode, comb->GIFTmater,
			comb->los );
		for( k=0 ; k<BU_PTBL_END( cur_path ) ; k++ )
		{
			struct directory *path_dp;

			path_dp = (struct directory *)BU_PTBL_GET( cur_path, k );
			RT_CK_DIR( path_dp );
			(void)fprintf( tabptr, "/%s", path_dp->d_namep );
		}
		(void)fprintf( tabptr, "/%s:\n", dp->d_namep );

		if( flag == ID_TABLE )
			goto out;

		for( i=0 ; i<actual_count ; i++ )
		{
			char op;
			int nsoltemp=0;
			struct rt_db_internal sol_intern;
			struct directory *sol_dp;
			mat_t temp_mat;
			struct bu_vls tmp_vls;
			int old;

			switch( tree_list[i].tl_op )
			{
				case OP_UNION:
					op = 'u';
					break;
				case OP_SUBTRACT:
					op = '-';
					break;
				case OP_INTERSECT:
					op = '+';
					break;
				default:
					bu_log( "unrecognized operation in region %s\n", dp->d_namep );
					op = '?';
					break;
			}

			if( (sol_dp=db_lookup( dbip, tree_list[i].tl_tree->tr_l.tl_name, LOOKUP_QUIET )) != DIR_NULL )
			{
				if( sol_dp->d_flags & DIR_COMB )
				{
					(void)fprintf(tabptr,"   RG %c %s\n",
						op, sol_dp->d_namep);
					continue;
				}
				else
				{
					bn_mat_mul( temp_mat, old_mat,
						tree_list[i].tl_tree->tr_l.tl_mat );
					if( rt_db_get_internal( &sol_intern, sol_dp, dbip, temp_mat ) < 0 )
					{
						bu_log( "Could not import %s\n", tree_list[i].tl_tree->tr_l.tl_name );
						nsoltemp = 0;
					}
					nsoltemp = sol_number( temp_mat, tree_list[i].tl_tree->tr_l.tl_name, &old );
					(void)fprintf(tabptr,"   %c [%d] ", op, nsoltemp );
				}
			}
			else
			{
				nsoltemp = sol_number( old_mat, tree_list[i].tl_tree->tr_l.tl_name, &old );
				(void)fprintf(tabptr,"   %c [%d] ", op, nsoltemp );
				continue;
			}

			if( flag == REG_TABLE || old )
			{
				(void) fprintf( tabptr, "%s\n", tree_list[i].tl_tree->tr_l.tl_name );
				continue;
			}
			else
				(void) fprintf( tabptr, "%s:  ", tree_list[i].tl_tree->tr_l.tl_name );

			if( !old )
			{
				/* if we get here, we must be looking for a solid table */
				bu_vls_init_if_uninit( &tmp_vls );
				if( rt_functab[sol_intern.idb_type].ft_describe( &tmp_vls, &sol_intern, 1, base2local ) < 0 )
				{
					Tcl_AppendResult(interp, tree_list[i].tl_tree->tr_l.tl_name,
						"describe error\n" , (char *)NULL );
				}
				(void)fprintf( tabptr, bu_vls_addr(&tmp_vls));
				bu_vls_free( &tmp_vls );
			}
			if( nsoltemp )
				rt_db_free_internal( &sol_intern );
		}
	}
	else if( dp->d_flags & DIR_COMB )
	{
		bu_ptbl_ins( cur_path, (long *)dp );

		for( i=0 ; i<actual_count ; i++ )
		{
			struct directory *nextdp;
			mat_t new_mat;

			if( (nextdp=db_lookup( dbip, tree_list[i].tl_tree->tr_l.tl_name,
				 LOOKUP_NOISY )) == DIR_NULL )
			{
				Tcl_AppendResult(interp, "\tskipping this object\n", (char *)NULL );
				continue;
			}

			/* recurse */
			bn_mat_mul( new_mat, old_mat, tree_list[i].tl_tree->tr_l.tl_mat );
			new_tables( nextdp, cur_path, new_mat, flag );
		}
	}
	else
	{
		Tcl_AppendResult(interp, "Illegal flags for ", dp->d_namep,
			"skipping\n", (char *)NULL );
		return;
	}

out:
	bu_free( (char *)tree_list, "new_tables: tree_list" );
	rt_comb_ifree( &intern );
	return;
}

int
f_tables(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	static CONST char sortcmd[] = "sort -n +1 -2 -o /tmp/ord_id ";
	static CONST char catcmd[] = "cat /tmp/ord_id >> ";
	struct bu_vls tmp_vls;
	struct bu_vls	cmd;
	struct bu_ptbl	cur_path;
	int status = TCL_OK;
	char *timep;
	time_t now;
	int i;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	if(argc < 3 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help %s", argv[0]);
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	bu_vls_init(&tmp_vls);
	bu_vls_init( &cmd );
	bu_ptbl_init( &cur_path, 8, "f_tables: cur_path" );
	numreg = 0;
	numsol = 0;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
	else{
	  bu_vls_free( &cmd );
	  bu_vls_free(&tmp_vls);
	  bu_ptbl_free( &cur_path );
	  return TCL_OK;
	}

	/* find out which ascii table is desired */
	if( strcmp(argv[0], "solids") == 0 ) {
		/* complete summary - down to solids/paremeters */
		flag = SOL_TABLE;
	}
	else if( strcmp(argv[0], "regions") == 0 ) {
		/* summary down to solids as members of regions */
		flag = REG_TABLE;
	}
	else if( strcmp(argv[0], "idents") == 0 ) {
		/* summary down to regions */
		flag = ID_TABLE;
	}
	else {
		/* should never reach here */
	  Tcl_AppendResult(interp, "tables:  input error\n", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}

	/* open the file */
	if( (tabptr=fopen(argv[1], "w+")) == NULL ) {
	  Tcl_AppendResult(interp, "Can't open ", argv[1], "\n", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}

	if( flag == SOL_TABLE || flag == REG_TABLE ) {
		/* temp file for discrimination of solids */
		if( (idfd = creat("/tmp/mged_discr", 0600)) < 0 ) {
			perror( "/tmp/mged_discr" );
			status = TCL_ERROR;
			goto end;
		}
		rd_idfd = open( "/tmp/mged_discr", 2 );
	}

	(void)time( &now );
	timep = ctime( &now );
	timep[24] = '\0';
	(void)fprintf(tabptr,"1 -8    Summary Table {%s}  (written: %s)\n",argv[0],timep);
	(void)fprintf(tabptr,"2 -7         file name    : %s\n",dbip->dbi_filename);    
	(void)fprintf(tabptr,"3 -6         \n");
	(void)fprintf(tabptr,"4 -5         \n");
	(void)fprintf(tabptr,"5 -4         user         : %s\n",getpwuid(getuid())->pw_gecos);
	(void)fprintf(tabptr,"6 -3         target title : %s\n",cur_title);
	(void)fprintf(tabptr,"7 -2         target units : %s\n",
		rt_units_string(dbip->dbi_local2base) );
	(void)fprintf(tabptr,"8 -1         objects      :");
	for(i=2; i<argc; i++) {
		if( (i%8) == 0 )
			(void)fprintf(tabptr,"\n                           ");
		(void)fprintf(tabptr," %s",argv[i]);
	}
	(void)fprintf(tabptr,"\n\n");

	/* make the tables */
	for( i=2 ; i<argc ; i++ )
	{
		struct directory *dp;

		bu_ptbl_reset( &cur_path );
		if( (dp = db_lookup( dbip, argv[i],LOOKUP_NOISY)) != DIR_NULL )
			new_tables( dp, &cur_path, identity, flag);
		else
			Tcl_AppendResult(interp, " skip this object\n", (char *)NULL);
	}

	Tcl_AppendResult(interp, "Summary written in: ", argv[1], "\n", (char *)NULL);

	if( flag == SOL_TABLE || flag == REG_TABLE ) {
		(void)unlink( "/tmp/mged_discr\0" );
		(void)fprintf(tabptr,"\n\nNumber Solids = %d  Number Regions = %d\n",
				numsol,numreg);

		bu_vls_printf(&tmp_vls, "Processed %d Solids and %d Regions\n",
			      numsol,numreg);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);

		(void)fclose( tabptr );
	}

	else {
		(void)fprintf(tabptr,"* 9999999\n* 9999999\n* 9999999\n* 9999999\n* 9999999\n");
		(void)fclose( tabptr );

		bu_vls_printf(&tmp_vls, "Processed %d Regions\n",numreg);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);

		/* make ordered idents */
		bu_vls_strcpy( &cmd, sortcmd );
		bu_vls_strcat( &cmd, argv[1] );
		Tcl_AppendResult(interp, bu_vls_addr(&cmd), "\n", (char *)NULL);
		(void)system( bu_vls_addr(&cmd) );

		bu_vls_trunc( &cmd, 0 );
		bu_vls_strcpy( &cmd, catcmd );
		bu_vls_strcat( &cmd, argv[1] );
		Tcl_AppendResult(interp, bu_vls_addr(&cmd), "\n", (char *)NULL);
		(void)system( bu_vls_addr(&cmd) );

		(void)unlink( "/tmp/ord_id\0" );
	}

end:
	bu_vls_free( &cmd );
	bu_vls_free(&tmp_vls);
	bu_ptbl_free( &cur_path );
	(void)signal( SIGINT, SIG_IGN );
	return status;
}
