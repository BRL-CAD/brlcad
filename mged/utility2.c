/*
 *			U T I L I T Y 2 . C
 *
 *
 *	f_tabobj()	tabs objects as they are stored in data file
 *	f_pathsum()	gives various path summaries
 *	f_copyeval()	copy an evaluated solid
 *	trace()		traces hierarchy of objects
 *	f_push()	control routine to push transformations to bottom of paths
 *	push()		pushes all transformations to solids at bottom of paths
 *	identitize()	makes all transformation matrices == identity for an object
 *	f_showmats()	shows matrices along a path
 *
 */

#include "conf.h"

#include <signal.h>
#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "nmg.h"
#include "db.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "./sedit.h"
#include "../librt/debug.h"	/* XXX */

static union record record;

void		identitize();
void		trace();
void		matrix_print();
void		push();

extern struct rt_tol	mged_tol;	/* from ged.c */

BU_EXTERN( struct shell *nmg_dup_shell, ( struct shell *s, long ***trans_tbl ) );
BU_EXTERN( struct rt_i *rt_new_rti, (struct db_i *dbip) );

int
f_shells(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct directory *old_dp,*new_dp;
	struct rt_db_internal old_intern,new_intern;
	struct model *m_tmp,*m;
	struct nmgregion *r_tmp,*r;
	struct shell *s_tmp,*s;
	int shell_count=0;
	char shell_name[NAMESIZE];
	long **trans_tbl;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (old_dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( rt_db_get_internal( &old_intern, old_dp, dbip, rt_identity ) < 0 )
	{
	  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( old_intern.idb_type != ID_NMG )
	{
	  Tcl_AppendResult(interp, "Object is not an NMG!!!\n", (char *)NULL);
	  return TCL_ERROR;
	}

	m = (struct model *)old_intern.idb_ptr;
	NMG_CK_MODEL(m);

	for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
	{
		for( BU_LIST_FOR( s, shell, &r->s_hd ) )
		{
			s_tmp = nmg_dup_shell( s, &trans_tbl );
			bu_free( (char *)trans_tbl, "trans_tbl" );

			m_tmp = nmg_mmr();
			r_tmp = BU_LIST_FIRST( nmgregion, &m_tmp->r_hd );

			BU_LIST_DEQUEUE( &s_tmp->l );
			BU_LIST_APPEND( &r_tmp->s_hd, &s_tmp->l );
			s_tmp->r_p = r_tmp;
			nmg_m_reindex( m_tmp, 0 );
			nmg_m_reindex( m, 0 );

			(void)nmg_model_fuse( m_tmp, &mged_tol );

			sprintf( shell_name, "shell.%d", shell_count );
			while( db_lookup( dbip, shell_name, 0 ) != DIR_NULL )
			{
				shell_count++;
				sprintf( shell_name, "shell.%d", shell_count );
			}

			if( (new_dp=db_diradd( dbip, shell_name, -1, 0, DIR_SOLID)) == DIR_NULL )  {
			  TCL_ALLOC_ERR_return;
			}

			/* make sure the geometry/bounding boxes are up to date */
			nmg_rebound(m_tmp, &mged_tol);


			/* Export NMG as a new solid */
			RT_INIT_DB_INTERNAL(&new_intern);
			new_intern.idb_type = ID_NMG;
			new_intern.idb_ptr = (genptr_t)m_tmp;

			if( rt_db_put_internal( new_dp, dbip, &new_intern ) < 0 )  {
				/* Free memory */
				nmg_km(m_tmp);
				Tcl_AppendResult(interp, "rt_db_put_internal() failure\n", (char *)NULL);
				return TCL_ERROR;
			}
			/* Internal representation has been freed by rt_db_put_internal */
			new_intern.idb_ptr = (genptr_t)NULL;

		}
	}

	return TCL_OK;
}

/*  	F _ T A B O B J :   tabs objects as they appear in data file
 */
int
f_tabobj(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	register struct directory *dp;
	int ngran, nmemb;
	int i, j, k, kk;
	struct bu_vls tmp_vls;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	bu_vls_init(&tmp_vls);
	start_catching_output(&tmp_vls);

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
	else
	  return TCL_OK;

	for(i=1; i<argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL )
			continue;
		if( db_get( dbip, dp, &record, 0, 1) < 0 ) {
		  stop_catching_output(&tmp_vls);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  TCL_READ_ERR_return;
		}
		if(record.u_id == ID_ARS_A) {
			bu_log("%c %d %s ",record.a.a_id,record.a.a_type,record.a.a_name);
			bu_log("%d %d %d %d\n",record.a.a_m,record.a.a_n,
				record.a.a_curlen,record.a.a_totlen);
			/* the b-records */
			ngran = record.a.a_totlen;
			for(j=1; j<=ngran; j++) {
				if( db_get( dbip, dp, &record, j, 1) < 0 ) {
				  stop_catching_output(&tmp_vls);
				  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
				  bu_vls_free(&tmp_vls);
				  TCL_READ_ERR_return;
				}
				bu_log("%c %d %d %d\n",record.b.b_id,record.b.b_type,record.b.b_n,record.b.b_ngranule);
				for(k=0; k<24; k+=6) {
					for(kk=k; kk<k+6; kk++)
						bu_log("%10.4f ",record.b.b_values[kk]*base2local);
					bu_log("\n");
				}
			}
		}

		if(record.u_id == ID_SOLID) {
			bu_log("%c %d %s %d\n", record.s.s_id,
				record.s.s_type,record.s.s_name,
				record.s.s_cgtype);
			for(kk=0;kk<24;kk+=6){
				for(j=kk;j<kk+6;j++)
					bu_log("%10.4f ",record.s.s_values[j]*base2local);
				bu_log("\n");
			}
		}
		if(record.u_id == ID_COMB) {
			bu_log("%c '%c' %s %d %d %d %d %d \n",
			record.c.c_id,record.c.c_flags,
			record.c.c_name,record.c.c_regionid,
			record.c.c_aircode, dp->d_len-1,
			record.c.c_material,record.c.c_los);
			nmemb = dp->d_len-1;
			for(j=1; j<=nmemb; j++) {
				mat_t	xmat;

				if( db_get( dbip, dp, &record, j, 1) < 0 ) {
				  stop_catching_output(&tmp_vls);
				  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
				  bu_vls_free(&tmp_vls);
				  TCL_READ_ERR_return;
				}
				bu_log("%c %c %s\n",
					record.M.m_id,
					record.M.m_relation,
					record.M.m_instname);
				rt_mat_dbmat( xmat, record.M.m_mat );
				matrix_print( xmat );
				bu_log("\n");
			}
		}
		if(record.u_id == ID_P_HEAD) {
			bu_log("POLYGON: not implemented yet\n");
		}

		if(record.u_id == ID_BSOLID) {
			bu_log("SPLINE: not implemented yet\n");
		}
	}

	stop_catching_output(&tmp_vls);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_OK;
}


#define MAX_LEVELS 12
#define CPEVAL		0
#define LISTPATH	1
#define LISTEVAL	2

/* input path */
struct directory *obj[MAX_LEVELS];
int objpos;

/* print flag */
int prflag;

/* path transformation matrix ... calculated in trace() */
mat_t xform;

/*  	F _ P A T H S U M :   does the following
 *		1.  produces path for purposes of matching
 *      	2.  gives all paths matching the input path OR
 *		3.  gives a summary of all paths matching the input path
 *		    including the final parameters of the solids at the bottom
 *		    of the matching paths
 */
int
f_pathsum(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	int i, flag, pos_in;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* pos_in = first member of path entered
	 *
	 *	paths are matched up to last input member
	 *      ANY path the same up to this point is considered as matching
	 */
	prflag = 0;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	/* find out which command was entered */
	if( strcmp( argv[0], "paths" ) == 0 ) {
		/* want to list all matching paths */
		flag = LISTPATH;
	}
	if( strcmp( argv[0], "listeval" ) == 0 ) {
		/* want to list evaluated solid[s] */
		flag = LISTEVAL;
	}

	if( argc < 2 )  {
		/* get the path */
	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   "Enter the path (space is delimiter): ", (char *)NULL);
	  return TCL_ERROR;
	}

	pos_in = 1;
	objpos = argc-1;

	/* build directory pointer array for desired path */
	for(i=0; i<objpos; i++) {
	  if( (obj[i] = db_lookup( dbip, argv[pos_in+i], LOOKUP_NOISY )) == DIR_NULL)
	    return TCL_ERROR;
	}

#if 0
	mat_idn(identity);
#endif
	mat_idn( xform );

	trace(obj[0], 0, identity, flag);

	if(prflag == 0) {
	  /* path not found */
	  Tcl_AppendResult(interp, "PATH:  ", (char *)NULL);
	  for(i=0; i<objpos; i++)
	    Tcl_AppendResult(interp, "/", obj[i]->d_namep, (char *)NULL);

	  Tcl_AppendResult(interp, "  NOT FOUND\n", (char *)NULL);
	}

	return TCL_OK;
}



/*   	F _ C O P Y E V A L : copys an evaluated solid
 */

static union record saverec;

int
f_copyeval(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{

	struct directory *dp;
	struct rt_external external,new_ext;
	struct rt_db_internal internal,new_int;
	mat_t	start_mat;
	int	id;
	int	i;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( argc < 3 )
	{
	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   "Enter new_solid_name and full path to old_solid ",
			   "(seperate path components with spaces not /)\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* check if new solid name already exists in description */
	if( db_lookup( dbip, argv[1], LOOKUP_QUIET) != DIR_NULL )
	{
	  Tcl_AppendResult(interp, argv[1], ": already exists\n", (char *)NULL);
	  return TCL_ERROR;
	}

	mat_idn( start_mat );

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	/* build directory pointer array for desired path */
	for(i=2; i<argc; i++)
	{
	  if( (obj[i-2] = db_lookup( dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL)
	    return TCL_ERROR;
	}

	/* Make sure that final component in path is a solid */
	RT_INIT_EXTERNAL( &external );
	if( db_get_external( &external , obj[argc-3] , dbip ) )
	{
	  db_free_external( &external );
	  TCL_READ_ERR_return;
	}
	RT_CK_EXTERNAL( &external );

	if( (id=rt_id_solid( &external )) == ID_NULL )
	{
	  Tcl_AppendResult(interp, "Final name in full path must be a solid, ",
			   argv[argc-1], " is not a solid\n", (char *)NULL);
	  db_free_external( &external );
	  return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL( &internal );
	if( rt_functab[id].ft_import( &internal, &external, identity ) < 0 )
	{
	  Tcl_AppendResult(interp, "solid import failure on ",
			   argv[argc-1], "\n", (char *)NULL);
	  db_free_external( &external );
	  return TCL_ERROR;
	}

	trace(obj[0], 0, start_mat, CPEVAL);

	if(prflag == 0) {
	  Tcl_AppendResult(interp, "PATH:  ", (char *)NULL);

	  for(i=0; i<objpos; i++)
	    Tcl_AppendResult(interp, "/", obj[i]->d_namep, (char *)NULL);

	  Tcl_AppendResult(interp, "  NOT FOUND\n", (char *)NULL);
	  db_free_external( &external );
	  return TCL_ERROR;
	}

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	/* Have found the desired path - xform is the transformation matrix */
	/* xform matrix calculated in trace() */

	/* create the new solid */
	RT_INIT_DB_INTERNAL( &new_int );
	if( rt_generic_xform( &new_int, xform , &internal , 0 ) )
	{
	  db_free_external( &external );
	  Tcl_AppendResult(interp, "f_copyeval: rt_generic_xform failed\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( rt_functab[id].ft_export( &new_ext , &new_int , 1.0 ) )
	{
	  db_free_external( &new_ext );
	  db_free_external( &external );
	  Tcl_AppendResult(interp, "f_copyeval: export failure for new solid\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( (dp=db_diradd( dbip, argv[1], -1, obj[argc-3]->d_len, obj[argc-3]->d_flags)) == DIR_NULL ||
	    db_alloc( dbip, dp, obj[argc-3]->d_len ) < 0 )
	{
	  db_free_external( &new_ext );
	  db_free_external( &external );
	  TCL_ALLOC_ERR_return;
	}

	if (db_put_external( &new_ext, dp, dbip ) < 0 )
	{
	  db_free_external( &new_ext );
	  db_free_external( &external );
	  TCL_WRITE_ERR_return;
	}
	db_free_external( &external );
	db_free_external( &new_ext );

	return TCL_OK;
}



/*	trace()		traces heirarchy of paths
 */

/* current path being traced */
extern struct directory *path[MAX_LEVELS];

void
trace( dp, pathpos, old_xlate, flag)
register struct directory *dp;
int pathpos;
mat_t old_xlate;
int flag;
{

	struct directory *nextdp;
	mat_t new_xlate;
	int nparts, i, k;
	struct bu_vls str;

	bu_vls_init( &str );

	if( pathpos >= MAX_LEVELS ) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "nesting exceeds %d levels\n",MAX_LEVELS);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);

	  for(i=0; i<MAX_LEVELS; i++)
	    Tcl_AppendResult(interp, "/", path[i]->d_namep, (char *)NULL);

	  Tcl_AppendResult(interp, "\n", (char *)NULL);
	  return;
	}

	if( db_get( dbip, dp, &record, 0, 1) < 0 )  READ_ERR_return;

	if( record.u_id == ID_COMB ) {
		nparts = dp->d_len-1;
		for(i=1; i<=nparts; i++) {
			mat_t	xmat;

			if( db_get( dbip, dp, &record, i, 1) < 0 )  READ_ERR_return;
			path[pathpos] = dp;
			if( (nextdp = db_lookup( dbip, record.M.m_instname, LOOKUP_NOISY)) == DIR_NULL )
				continue;

			rt_mat_dbmat( xmat, record.M.m_mat );
			mat_mul(new_xlate, old_xlate, xmat);

			/* Recursive call */
			trace(nextdp, pathpos+1, new_xlate, flag);

		}
		return;
	}

	/* not a combination  -  should have a solid */

	/* last (bottom) position */
	path[pathpos] = dp;

	/* check for desired path */
	for(k=0; k<objpos; k++) {
		if(path[k]->d_addr != obj[k]->d_addr) {
			/* not the desired path */
			return;
		}
	}

	/* have the desired path up to objpos */
	mat_copy(xform, old_xlate);
	prflag = 1;

	if(flag == CPEVAL) { 
		/* save this record */
		if( db_get( dbip, dp, &saverec, 0, 1) < 0 )  READ_ERR_return;
		return;
	}

	/* print the path */
	for(k=0; k<pathpos; k++)
	  Tcl_AppendResult(interp, "/", path[k]->d_namep, (char *)NULL);

	if(flag == LISTPATH) {
	  Tcl_AppendResult(interp, "/", record.s.s_name, "\n", (char *)NULL);
	  return;
	}

	/* NOTE - only reach here if flag == LISTEVAL */
	/* do_list will print actual solid name */
	Tcl_AppendResult(interp, "/", (char *)NULL);
	do_list( &str, dp, 1 );
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
}

/*
 *			M A T R I X _ P R I N T
 *
 * Print out the 4x4 matrix addressed by "m".
 */
void
matrix_print( m )
register matp_t m;
{
  register int i;
  struct bu_vls tmp_vls;

  bu_vls_init(&tmp_vls);

  for(i=0; i<16; i++) {
    if( (i+1)%4 )
      bu_vls_printf(&tmp_vls, "%f\t",m[i]);
    else if(i == 15)
      bu_vls_printf(&tmp_vls, "%f\n",m[i]);
    else
      bu_vls_printf(&tmp_vls, "%f\n",m[i]*base2local);
  }

  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
  bu_vls_free(&tmp_vls);
}


/* structure to hold all solids that have been pushed. */
struct push_id {
	long	magic;
	struct push_id *forw, *back;
	struct directory *pi_dir;
	mat_t	pi_mat;
};
#define MAGIC_PUSH_ID	0x50495323
struct push_id	pi_head;
#define FOR_ALL_PUSH_SOLIDS(p) \
	for( p=pi_head.forw; p!=&pi_head; p=p->forw)

static int push_error;

/*
 *		P U S H _ L E A F
 *
 * This routine must be prepared to run in parallel.
 *
 * This routine is called once for eas leaf (solid) that is to
 * be pushed.  All it does is build at push_id linked list.  The 
 * linked list could be handled by bu_list macros but it is simple
 * enough to do hear with out them.
 */
HIDDEN union tree *push_leaf( tsp, pathp, ep, id)
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	union tree	*curtree;
	struct directory *dp;
	register struct push_id *pip;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);

	dp = pathp->fp_names[pathp->fp_len-1];

	if (rt_g.debug&DEBUG_TREEWALK) {
	  char *sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(interp, "push_leaf(", rt_functab[id].ft_name,
			   ") path='", sofar, "'\n", (char *)NULL);
	  bu_free(sofar, "path string");
	}
/*
 * XXX - This will work but is not the best method.  dp->d_uses tells us
 * if this solid (leaf) has been seen before.  If it hasn't just add
 * it to the list.  If it has, search the list to see if the matricies
 * match and do the "right" thing.
 *
 * (There is a question as to whether dp->d_uses is reset to zero
 *  for each tree walk.  If it is not, then d_uses is NOT a safe
 *  way to check and this method will always work.)
 */
	RES_ACQUIRE(&rt_g.res_worker);
	FOR_ALL_PUSH_SOLIDS(pip) {
	  if (pip->pi_dir == dp ) {
	    if (!rt_mat_is_equal(pip->pi_mat,
				 tsp->ts_mat, tsp->ts_tol)) {
	      char *sofar = db_path_to_string(pathp);

	      Tcl_AppendResult(interp, "push_leaf: matrix mismatch between '", sofar,
			       "' and prior reference.\n", (char *)NULL);
	      bu_free(sofar, "path string");
	      push_error = 1;
	    }

	    RES_RELEASE(&rt_g.res_worker);
	    BU_GETUNION(curtree, tree);
	    curtree->magic = RT_TREE_MAGIC;
	    curtree->tr_op = OP_NOP;
	    return curtree;
	  }
	}
/*
 * This is the first time we have seen this solid.
 */
	pip = (struct push_id *) bu_malloc(sizeof(struct push_id),
	    "Push ident");
	pip->magic = MAGIC_PUSH_ID;
	pip->pi_dir = dp;
	mat_copy(pip->pi_mat, tsp->ts_mat);
	pip->back = pi_head.back;
	pi_head.back = pip;
	pip->forw = &pi_head;
	pip->back->forw = pip;
	RES_RELEASE(&rt_g.res_worker);
	BU_GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return curtree;
}
/*
 * A null routine that does nothing.
 */
HIDDEN union tree *push_region_end( tsp, pathp, curtree)
register struct db_tree_state *tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	return curtree;
}
/*
 * The tree walker neds to have an initial state.  We could
 * steal it from doview.c but there is no real reason.
 */

static struct db_tree_state push_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0,0,0,			/* region, air, gmater */
	100,			/* GIFT los */
#if __STDC__
	{
#endif
		/* struct mater_info ts_mater */
		1.0, 0.0, 0.0,	/* color, RGB */
		0,		/* override */
		DB_INH_LOWER,	/* color inherit */
		DB_INH_LOWER,	/* mater inherit */
		"",		/* material name */
		""		/* material params */
#if __STDC__
	}
#endif
	,
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};

/*			F _ P U S H
 *
 * The push command is used to move matricies from combinations 
 * down to the solids. At some point, it is worth while thinking
 * about adding a limit to have the push go only N levels down.
 *
 * the -d flag turns on the treewalker debugging output.
 * the -P flag allows for multi-processor tree walking (not useful)
 * the -l flag is there to select levels even if it does not currently work.
 */
int
f_push(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int	ncpu;
	int	c;
	int	old_debug;
#if 0
	int	levels;	/* XXX levels option on push command not yet implemented */
#endif
	extern 	int optind;
	extern	char *optarg;
	extern	struct rt_tol	mged_tol;	/* from ged.c */
	extern	struct rt_tess_tol mged_ttol;
	int	i;
	int	id;
	struct push_id *pip;
	struct rt_external	es_ext;
	struct rt_db_internal	es_int;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	RT_CHECK_DBI(dbip);

	old_debug = rt_g.debug;
	pi_head.magic = MAGIC_PUSH_ID;
	pi_head.forw = pi_head.back = &pi_head;
	pi_head.pi_dir = (struct directory *) 0;

	/* Initial values for options, must be reset each time */
	ncpu = 1;

	/* Parse options */
	optind = 1;	/* re-init getopt() */
	while ( (c=getopt(argc, argv, "l:P:d")) != EOF) {
		switch(c) {
		case 'l':
#if 0
			levels=atoi(optarg);
#endif
			break;
		case 'P':
			ncpu = atoi(optarg);
			if (ncpu<1) ncpu = 1;
			break;
		case 'd':
			rt_g.debug |= DEBUG_TREEWALK;
			break;
		case '?':
		default:
		  Tcl_AppendResult(interp, "push: usage push [-l levels] [-P processors] [-d] root [root2 ...]\n", (char *)NULL);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	push_error = 0;

	push_initial_tree_state.ts_ttol = &mged_ttol;
	push_initial_tree_state.ts_tol  = &mged_tol;
	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	/*
	 * build a linked list of solids with the correct
	 * matrix to apply to each solid.  This will also
	 * check to make sure that a solid is not pushed in two
	 * different directions at the same time.
	 */
	i = db_walk_tree( dbip, argc, (CONST char **)argv,
	    ncpu,
	    &push_initial_tree_state,
	    0,				/* take all regions */
	    push_region_end,
	    push_leaf);

	/*
	 * If there was any error, then just free up the solid
	 * list we just built.
	 */
	if ( i < 0 || push_error ) {
		while (pi_head.forw != &pi_head) {
			pip = pi_head.forw;
			pip->forw->back = pip->back;
			pip->back->forw = pip->forw;
			bu_free((char *)pip, "Push ident");
		}
		rt_g.debug = old_debug;
		Tcl_AppendResult(interp, "push:\tdb_walk_tree failed or there was a solid moving\n\tin two or more directions\n", (char *)NULL);
		return TCL_ERROR;
	}
/*
 * We've built the push solid list, now all we need to do is apply
 * the matrix we've stored for each solid.
 */
	FOR_ALL_PUSH_SOLIDS(pip) {
		RT_INIT_EXTERNAL(&es_ext);
		RT_INIT_DB_INTERNAL(&es_int);
		if (db_get_external( &es_ext, pip->pi_dir, dbip) < 0) {
		  Tcl_AppendResult(interp, "f_push: Read error fetching '",
				   pip->pi_dir->d_namep, "'\n", (char *)NULL);
		  push_error = -1;
		  continue;
		}
		id = rt_id_solid( &es_ext);
		if (rt_functab[id].ft_import(&es_int, &es_ext, pip->pi_mat) < 0 ) {
		  Tcl_AppendResult(interp, "push(", pip->pi_dir->d_namep,
				   "): solid import failure\n", (char *)NULL);
		  if (es_int.idb_ptr) rt_functab[id].ft_ifree( &es_int);
		  db_free_external( &es_ext);
		  continue;
		}
		RT_CK_DB_INTERNAL( &es_int);
		if ( rt_functab[id].ft_export( &es_ext, &es_int, 1.0) < 0 ) {
		  Tcl_AppendResult(interp, "push(", pip->pi_dir->d_namep,
				   "): solid export failure\n", (char *)NULL);
		} else {
			db_put_external(&es_ext, pip->pi_dir, dbip);
		}
		if (es_int.idb_ptr) rt_functab[id].ft_ifree(&es_int);
		db_free_external(&es_ext);
	}

	/*
	 * Now use the identitize() tree walker to turn all the
	 * matricies in a combination to the identity matrix.
	 * It would be nice to use db_tree_walker() but the tree
	 * walker does not give us all combinations, just regions.
	 * This would work if we just processed all matricies backwards
	 * from the leaf (solid) towards the root, but all in all it
	 * seems that this is a better method.
	 */

	while (argc > 0) {
		struct directory *db;
		db = db_lookup(dbip, *argv++, 0);
		if (db) identitize(db);
		--argc;
	}

	/*
	 * Free up the solid table we built.
	 */
	while (pi_head.forw != &pi_head) {
		pip = pi_head.forw;
		pip->forw->back = pip->back;
		pip->back->forw = pip->forw;
		bu_free((char *)pip, "Push ident");
	}

	rt_g.debug = old_debug;
	return push_error ? TCL_ERROR : TCL_OK;
}

/*
 *			I D E N T I T I Z E ( ) 
 *
 *	Traverses an objects paths, setting all member matrices == identity
 *
 */
void
identitize( dp )
struct directory *dp;
{

	struct directory *nextdp;
	int nparts, i;
#if 0
	mat_t	identity;

	mat_idn( identity );
#endif
	if( db_get( dbip, dp, &record, 0, 1) < 0 )  READ_ERR_return;
	if( record.u_id == ID_COMB ) {
		nparts = dp->d_len-1;
		for(i=1; i<=nparts; i++) {
			if( db_get( dbip, dp, &record, i, 1) < 0 )  READ_ERR_return;

			rt_dbmat_mat( record.M.m_mat, identity );
			if( db_put( dbip, dp, &record, i, 1 ) < 0 )  WRITE_ERR_return;

			if( (nextdp = db_lookup( dbip, record.M.m_instname, LOOKUP_NOISY)) == DIR_NULL )
				continue;
			/* Recursive call */
			identitize( nextdp );
		}
		return;
	}
	/* bottom position */
	return;
}

static void
zero_dp_counts( db_ip, dp )
struct db_i *db_ip;
struct directory *dp;
{
  RT_CK_DIR( dp );

  dp->d_nref = 0;
  dp->d_uses = 0;

  if( BU_LIST_NON_EMPTY( &dp->d_use_hd ) )
    Tcl_AppendResult(interp, "List for ", dp->d_namep, " is not empty\n", (char *)NULL);
}

static void
zero_nrefs( db_ip, dp )
struct db_i *db_ip;
struct directory *dp;
{
	RT_CK_DIR( dp );

	dp->d_nref = 0;
}

static void
increment_uses( db_ip, dp )
struct db_i *db_ip;
struct directory *dp;
{
	RT_CK_DIR( dp );

	dp->d_uses++;
}

static void
increment_nrefs( db_ip, dp )
struct db_i *db_ip;
struct directory *dp;
{
	RT_CK_DIR( dp );

	dp->d_nref++;
}

struct object_use
{
	struct bu_list	l;
	struct directory *dp;
	mat_t xform;
	int used;
};

static void
Free_uses( db_ip, dp )
struct db_i *db_ip;
struct directory *dp;
{
	struct object_use *use;
	struct rt_external sol_ext;
	struct rt_db_internal sol_int;
	int id;

	RT_CK_DIR( dp );

	while( BU_LIST_NON_EMPTY( &dp->d_use_hd ) )
	{
		use = BU_LIST_FIRST( object_use, &dp->d_use_hd );
		if( !use->used )
		{
			/* never used, so delete directory entry.
			 * This could actually delete the original, buts that's O.K.
			 */
			db_dirdelete( dbip, use->dp );
		}

		BU_LIST_DEQUEUE( &use->l );
		bu_free( (char *)use, "Free_uses: use" );
	}
}

static void
Make_new_name( db_ip, dp )
struct db_i *db_ip;
struct directory *dp;
{
	struct object_use *use;
	int use_no;
	int digits;
	int suffix_start;
	int name_length;
	int j;
	char format[25];
	char name[NAMESIZE];

	/* only one use and not referenced elsewhere, nothing to do */
	if( dp->d_uses < 2 && dp->d_uses == dp->d_nref )
		return;

	/* check if already done */
	if( BU_LIST_NON_EMPTY( &dp->d_use_hd ) )
		return;

	digits = log10( (double)dp->d_uses ) + 2.0;
	sprintf( format, "_%%0%dd", digits );

	name_length = strlen( dp->d_namep );
	if( name_length + digits + 1 > NAMESIZE - 1 )
		suffix_start = NAMESIZE - digits - 2;
	else
		suffix_start = name_length;

	j = 0;
	for( use_no=0 ; use_no<dp->d_uses ; use_no++ )
	{
		j++;
		use = (struct object_use *)bu_malloc( sizeof( struct object_use ), "Make_new_name: use" );

		/* set xform for this object_use to all zeros */
		mat_zero( use->xform );
		use->used = 0;
		NAMEMOVE( dp->d_namep, name );

		/* Add an entry for the original at the end of the list
		 * This insures that the original will be last to be modified
		 * If original were modified earlier, copies would be screwed-up
		 */
		if( use_no == dp->d_uses-1 )
			use->dp = dp;
		else
		{
			sprintf( &name[suffix_start], format, j );

			/* Insure that new name is unique */
			while( db_lookup( dbip, name, 0 ) != DIR_NULL )
			{
				j++;
				sprintf( &name[suffix_start], format, j );
			}

			/* Add new name to directory */
			if( (use->dp = db_diradd( dbip, name, -1, 0, dp->d_flags )) == DIR_NULL )
			{
				ALLOC_ERR;
				return;
			}
		}

		/* Add new directory pointer to use list for this object */
		BU_LIST_INSERT( &dp->d_use_hd, &use->l );
	}
}

static struct directory *
Copy_solid( dp, xform )
struct directory *dp;
mat_t xform;
{
	struct directory *found;
	struct rt_external sol_ext;
	struct rt_db_internal sol_int;
	struct object_use *use;
	int id;

	RT_CK_DIR( dp );

	if( !(dp->d_flags & DIR_SOLID) )
	{
	  Tcl_AppendResult(interp, "Copy_solid: ", dp->d_namep,
			   " is not a solid!!!!\n", (char *)NULL);
	  return( DIR_NULL );
	}

	/* Look for a copy that already has this transform matrix */
	for( BU_LIST_FOR( use, object_use, &dp->d_use_hd ) )
	{
		if( rt_mat_is_equal( xform, use->xform, &mged_tol ) )
		{
			/* found a match, no need to make another copy */
			use->used = 1;
			return( use->dp );
		}
	}

	found = DIR_NULL;
	/* get a fresh use */
	for( BU_LIST_FOR( use, object_use, &dp->d_use_hd ) )
	{
		if( use->used )
			continue;

		found = use->dp;
		use->used = 1;
		mat_copy( use->xform, xform );
		break;
	}

	if( found == DIR_NULL && dp->d_nref == 1 && dp->d_uses == 1 )
	{
		/* only one use, take it */
		found = dp;
	}

	if( found == DIR_NULL )
	{
	  Tcl_AppendResult(interp, "Ran out of uses for solid ",
			   dp->d_namep, "\n", (char *)NULL);
	  return( DIR_NULL );
	}

	RT_INIT_EXTERNAL( &sol_ext );

	if( db_get_external( &sol_ext, dp, dbip ) < 0 )
	{
	  Tcl_AppendResult(interp, "Cannot get external form of ",
			   dp->d_namep, "\n", (char *)NULL);
	  return( DIR_NULL );
	}

	RT_INIT_DB_INTERNAL( &sol_int );

	id = rt_id_solid( &sol_ext );
	if( rt_functab[id].ft_import( &sol_int, &sol_ext, xform ) < 0 )
	{
	  Tcl_AppendResult(interp, "Cannot import solid ",
			   dp->d_namep, "\n", (char *)NULL);
	  return( DIR_NULL );
	}

	RT_CK_DB_INTERNAL( &sol_int );

	if( rt_db_put_internal( found, dbip, &sol_int ) < 0 )
	{
	  Tcl_AppendResult(interp, "Cannot write copy solid (", found->d_namep,
			   ") to database\n", (char *)NULL);
	  return( DIR_NULL );
	}

	return( found );
}

static struct directory *Copy_object();

static struct directory *
Copy_comb( dp, xform )
struct directory *dp;
mat_t xform;
{
	struct object_use *use;
	struct directory *found;
	union record *rp;
	mat_t new_xform;
	int i;

	RT_CK_DIR( dp );

	/* Look for a copy that already has this transform matrix */
	for( BU_LIST_FOR( use, object_use, &dp->d_use_hd ) )
	{
		if( rt_mat_is_equal( xform, use->xform, &mged_tol ) )
		{
			/* found a match, no need to make another copy */
			use->used = 1;
			return( use->dp );
		}
	}

	/* if we can't get records for this combination, just leave it alone */
	if( (rp=db_getmrec( dbip , dp )) == (union record *)0 )
		return( dp );

	/* copy members */
	for( i=1 ; i<dp->d_len ; i++ )
	{
		mat_t arc_mat;
		struct directory *dp2;
		struct directory *dp_new;

		/* ignore members that don't exist */
		if( (dp2=db_lookup( dbip, rp[i].M.m_instname, 0 )) == DIR_NULL )
			continue;

		/* apply transform matrix for this arc */
		rt_mat_dbmat( arc_mat, rp[i].M.m_mat );
		mat_mul( new_xform, xform, arc_mat );

		/* Copy member with current tranform matrix */
		if( (dp_new=Copy_object( dp2, new_xform )) == DIR_NULL )
		{
		  Tcl_AppendResult(interp, "Failed to copy object ",
				   dp2->d_namep, "\n", (char *)NULL);
		  return( DIR_NULL );
		}

		/* replace member name with new copy */
		NAMEMOVE( dp_new->d_namep, rp[i].M.m_instname );

		/* make transform for this arc the identity matrix */
		rt_dbmat_mat( rp[i].M.m_mat, rt_identity );
	}

	/* Get a use of this object */
	found = DIR_NULL;
	for( BU_LIST_FOR( use, object_use, &dp->d_use_hd ) )
	{
		/* Get a fresh use of this object */
		if( use->used )
			continue;	/* already used */
		found = use->dp;
		use->used = 1;
		mat_copy( use->xform, xform );
		break;
	}

	if( found == DIR_NULL && dp->d_nref == 1 && dp->d_uses == 1 )
	{
		/* only one use, so take original */
		found = dp;
	}

	if( found == DIR_NULL )
	{
	  Tcl_AppendResult(interp, "Ran out of uses for combination ",
			   dp->d_namep, "\n", (char *)NULL);
	  return( DIR_NULL );
	}

	if( found != dp )
	{
	  if( db_alloc( dbip, found, dp->d_len ) < 0 )
	    {
	      Tcl_AppendResult(interp, "Cannot allocate space for combination ",
			       found->d_namep, "\n", (char *)NULL);
	      return( DIR_NULL );
	    }
	  NAMEMOVE( found->d_namep, rp[0].c.c_name );
	}

	if( db_put( dbip, found, rp, 0, found->d_len ) < 0 )
	{
	  Tcl_AppendResult(interp,  "Failed to write combination ",
			   found->d_namep, " to database\n", (char *)NULL);
	  return( DIR_NULL );
	}

	return( found );
}

static struct directory *
Copy_object( dp, xform )
struct directory *dp;
mat_t xform;
{
	RT_CK_DIR( dp );

	if( dp->d_flags & DIR_SOLID )
		return( Copy_solid( dp, xform ) );
	else
		return( Copy_comb( dp, xform ) );
}

int
f_xpush(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct directory *old_dp;
	struct bu_ptbl tops;
	mat_t xform;
	int i,j;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* get directory pointer for arg */
	if( (old_dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	/* initialize use and reference counts */
	db_functree( dbip, old_dp, zero_dp_counts, zero_dp_counts );

	/* Count uses */
	db_functree( dbip, old_dp, increment_uses, increment_uses );

	/* Get list of tree tops in this model */
	bu_ptbl( &tops, BU_PTBL_INIT, (long *)NULL );
	for( i=0 ; i<RT_DBNHASH ; i++ )
	{
		struct directory *dp;

		for( dp=dbip->dbi_Head[i] ; dp!=DIR_NULL ; dp=dp->d_forw )
		{
			union record *rp;

			if( dp->d_flags & DIR_SOLID )
				continue;

			if( (rp=db_getmrec( dbip , dp )) == (union record *)0 )
			{
			  Tcl_AppendResult(interp, "Cannot get records for ", dp->d_namep,
					   "\n", (char *)NULL);
			  return TCL_ERROR;
			}

			for( j=1 ; j<dp->d_len ; j++ )
			{
				struct directory *dp2;

				dp2 = db_lookup( dbip, rp[j].M.m_instname, 0 );
				if( dp2 == DIR_NULL )
					continue;

				dp2->d_nref++;
			}
			bu_free( (char *)rp, "rp[]" );
		}
	}

	for( i=0 ; i<RT_DBNHASH ; i++ )
	{
		struct directory *dp;

		for( dp=dbip->dbi_Head[i] ; dp!=DIR_NULL ; dp=dp->d_forw )
		{
			if( dp->d_flags & DIR_SOLID )
				continue;

			if( dp->d_nref == 0 )
				bu_ptbl( &tops, BU_PTBL_INS, (long *)dp );
		}
	}

	/* zero nrefs in entire model */
	for( i=0 ; i<BU_PTBL_END( &tops ) ; i++ )
	{
		struct directory *dp;

		dp = (struct directory *)BU_PTBL_GET( &tops, i );
		db_functree( dbip, dp, zero_nrefs, zero_nrefs );
	}

	/* count references in entire model */
	for( i=0 ; i<BU_PTBL_END( &tops ) ; i++ )
	{
		struct directory *dp;

		dp = (struct directory *)BU_PTBL_GET( &tops, i );
		db_functree( dbip, dp, increment_nrefs, increment_nrefs );
	}

	/* Free list of tree-tops */
	bu_ptbl( &tops, BU_PTBL_FREE, (long *)NULL );

	/* Make new names */
	db_functree( dbip, old_dp, Make_new_name, Make_new_name );

	mat_idn( xform );

	/* Make new objects */
	(void) Copy_object( old_dp, xform );

	/* Free use lists and delete unused directory entries */
	db_functree( dbip, old_dp, Free_uses, Free_uses );

	return TCL_OK;
}

int
f_showmats(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char *parent;
	char *child;
	struct directory *dp;
	union record *rp;
	int max_count=1;
	mat_t acc_matrix;
	struct bu_vls tmp_vls;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	bu_vls_init(&tmp_vls);
	MAT_IDN( acc_matrix );

	parent = strtok( argv[1], "/" );
	while( child = strtok( (char *)NULL, "/" ) )
	{
		int j;
		int found;
		int count;

		if( (dp = db_lookup( dbip, parent, LOOKUP_NOISY )) == DIR_NULL)
		  return TCL_ERROR;

		Tcl_AppendResult(interp, parent, "\n", (char *)NULL);

		if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		{
		  TCL_READ_ERR_return;
		}

		found = 0;
		count = 0;
		start_catching_output(&tmp_vls);
		for( j=1 ; j<dp->d_len ; j++ )
		{
			if( !strncmp( rp[j].M.m_instname, child, NAMESIZE ) )
			{
				mat_t matrix;

				count++;
				if( count > 1 )
				  bu_log( "\n\tOccurrence #%d:\n", count );

				rt_mat_dbmat( matrix, rp[j].M.m_mat );
				mat_print( "", matrix );
				if( count == 1 )
				{
					mat_t tmp_mat;
					mat_mul( tmp_mat, acc_matrix, matrix );
					MAT_COPY( acc_matrix, tmp_mat );
				}
				found = 1;
			}
		}
		stop_catching_output(&tmp_vls);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		if( !found )
		{
		  Tcl_AppendResult(interp, child, " is not a member of ",
				   parent, "\n", (char *)NULL);
		  return TCL_ERROR;
		}
		if( count > max_count )
			max_count = count;

		bu_free( (char *)rp, "f_showmats: rp" );
		parent = child;
	}
	Tcl_AppendResult(interp, parent, "\n", (char *)NULL);

	if( max_count > 1 )
	  Tcl_AppendResult(interp, "\nAccumulated matrix (using first occurrence of each object):\n", (char *)NULL);
	else
	  Tcl_AppendResult(interp, "\nAccumulated matrix:\n", (char *)NULL);

	start_catching_output(&tmp_vls);
	mat_print( "", acc_matrix );
	stop_catching_output(&tmp_vls);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	return TCL_OK;
}

int
f_nmg_simplify(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char *argv[];
{
	struct directory *dp;
	struct rt_db_internal nmg_intern;
	struct rt_db_internal new_intern;
	struct rt_external new_extern;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	int do_all=1;
	int do_arb=0;
	int do_ell=0;
	int do_tgc=0;
	int do_poly=0;
	char *new_name;
	char *nmg_name;
	int success = 0;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	RT_INIT_DB_INTERNAL( &new_intern );

	if( argc == 4 )
	{
		do_all = 0;
		if( !strncmp( argv[1], "arb", 3 ) )
			do_arb = 1;
		else if( !strncmp( argv[1], "ell", 3 ) )
			do_ell = 1;
		else if( !strncmp( argv[1], "tgc", 3 ) )
			do_tgc = 1;
		else if( !strncmp( argv[1], "poly", 4 ) )
			do_poly = 1;
		else
		{
		  Tcl_AppendResult(interp, "Usage: nmg_simplify [arb|ell|tgc|poly] new_solid_name nmg_solid\n", (char *)NULL);
		  return TCL_ERROR;
		}

		new_name = argv[2];
		nmg_name = argv[3];
	}
	else
	{
		new_name = argv[1];
		nmg_name = argv[2];
	}

	if( db_lookup( dbip, new_name, LOOKUP_QUIET) != DIR_NULL )
	{
	  Tcl_AppendResult(interp, new_name, " already exists\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( (dp=db_lookup( dbip, nmg_name, LOOKUP_QUIET)) == DIR_NULL )
	{
	  Tcl_AppendResult(interp, nmg_name, " does not exist\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( rt_db_get_internal( &nmg_intern, dp, dbip, rt_identity ) < 0 )
	{
	  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( nmg_intern.idb_type != ID_NMG )
	{
	  Tcl_AppendResult(interp, nmg_name, " is not an NMG solid\n", (char *)NULL);
	  rt_db_free_internal( &nmg_intern );
	  return TCL_ERROR;
	}

	m = (struct model *)nmg_intern.idb_ptr;
	NMG_CK_MODEL( m );

	if( do_arb || do_all )
	{
		struct rt_arb_internal arb_int;

		if( nmg_to_arb( m, &arb_int ) )
		{
			new_intern.idb_ptr = (genptr_t)(&arb_int);
			new_intern.idb_type = ID_ARB8;
			success = 1;
		}
		else if( do_arb )
		{
		  rt_db_free_internal( &nmg_intern );
		  Tcl_AppendResult(interp, "Failed to construct an ARB equivalent to ",
				   nmg_name, "\n", (char *)NULL);
		  return TCL_OK;
		}
	}

	if( (do_tgc || do_all) && !success )
	{
		struct rt_tgc_internal tgc_int;

		if( nmg_to_tgc( m, &tgc_int, &mged_tol ) )
		{
			new_intern.idb_ptr = (genptr_t)(&tgc_int);
			new_intern.idb_type = ID_TGC;
			success = 1;
		}
		else if( do_tgc )
		{
		  rt_db_free_internal( &nmg_intern );
		  Tcl_AppendResult(interp, "Failed to construct a TGC equivalent to ",
				   nmg_name, "\n", (char *)NULL);
		  return TCL_OK;
		}
	}
	if( (do_poly || do_all) && !success )
	{
		struct rt_pg_internal *poly_int;

		poly_int = (struct rt_pg_internal *)bu_malloc( sizeof( struct rt_pg_internal ), "f_nmg_simplify: poly_int" );

		if( nmg_to_poly( m, poly_int, &mged_tol ) )
		{
			new_intern.idb_ptr = (genptr_t)(poly_int);
			new_intern.idb_type = ID_POLY;
			success = 1;
		}
		else if( do_poly )
		{
		  rt_db_free_internal( &nmg_intern );
		  Tcl_AppendResult(interp, nmg_name, " is not a closed surface, cannot make a polysolid\n", (char *)NULL);
		  return TCL_OK;
		}
	}

	if( success )
	{
		int ngran;

		r = BU_LIST_FIRST( nmgregion, &m->r_hd );
		s = BU_LIST_FIRST( shell, &r->s_hd );

		if( BU_LIST_NON_EMPTY( &s->lu_hd ) )
		  Tcl_AppendResult(interp, "wire loops in ", nmg_name,
				   " have been ignored in conversion\n", (char *)NULL);

		if( BU_LIST_NON_EMPTY( &s->eu_hd ) )
		  Tcl_AppendResult(interp, "wire edges in ", nmg_name,
				   " have been ignored in conversion\n", (char *)NULL);

		if( s->vu_p )
		  Tcl_AppendResult(interp, "Single vertexuse in shell of ", nmg_name,
				   " has been ignored in conversion\n", (char *)NULL);

		rt_db_free_internal( &nmg_intern );

		if( rt_functab[new_intern.idb_type].ft_export( &new_extern, &new_intern, 1.0 ) < 0 )
		{
		  Tcl_AppendResult(interp, "f_nmg_simplify: export failure\n", (char *)NULL);
		  rt_functab[new_intern.idb_type].ft_ifree( &new_intern );
		  return TCL_ERROR;
		}

		/* only the polysolid mallocs anything */
		if( new_intern.idb_type == ID_POLY )
			rt_functab[new_intern.idb_type].ft_ifree( &new_intern );

		ngran = (new_extern.ext_nbytes+sizeof(union record)-1) / sizeof(union record);
		if( (dp = db_diradd( dbip, new_name, -1L, ngran, DIR_SOLID)) == DIR_NULL ||
		    db_alloc( dbip, dp, 1 ) < 0 )
		    {
		      db_free_external( &new_extern );
		      TCL_ALLOC_ERR_return;
		    }

		if (db_put_external( &new_extern, dp, dbip ) < 0 )
		{
		  db_free_external( &new_extern );
		  TCL_WRITE_ERR_return;
		}
		db_free_external( &new_extern );
		return TCL_OK;
	}

	Tcl_AppendResult(interp, "simplification to ", argv[1],
			 " is not yet supported\n", (char *)NULL);
	return TCL_ERROR;
}

/*			F _ M A K E _ B B
 *
 *	Build an RPP bounding box for the list of objects and/or paths passed to this routine
 */

int
f_make_bb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_i		*rtip;
	int			i,j;
	point_t			rpp_min,rpp_max;
	struct db_full_path	path;
	struct directory	*dp;
	struct rt_arb_internal	arb;
	struct rt_db_internal	new_intern;
	struct rt_external	new_extern;
	struct region		*regp;
	int			ngran;
	char			*new_name;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* Since arguments may be paths, make sure first argument isn't */
	if( strchr( argv[1], '/' ) )
	{
	  Tcl_AppendResult(interp, "Do not use '/' in solid names: ", argv[1], "\n", (char *)NULL);
	  return TCL_ERROR;
	}

	new_name = argv[1];
	if( db_lookup( dbip, new_name, LOOKUP_QUIET ) != DIR_NULL )
	{
	  Tcl_AppendResult(interp, new_name, " already exists\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Make a new rt_i instance from the exusting db_i sructure */
	if( (rtip=rt_new_rti( dbip ) ) == RTI_NULL )
	{
	  Tcl_AppendResult(interp, "rt_new_rti failure for ", dbip->dbi_filename,
			   "\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Get trees for list of objects/paths */
	for( i=2 ; i<argc ; i++  )
	{
		int gottree;

		/* Get full_path structure for argument */
		db_full_path_init( &path );
		if( db_string_to_path( &path,  rtip->rti_dbip, argv[i] ) )
		{
			Tcl_AppendResult(interp, "db_string_to_path failed for ",
				argv[i], "\n", (char *)NULL );
			rt_clean( rtip );
			bu_free( (char *)rtip, "f_make_bb: rtip" );
			return TCL_ERROR;
		}

		/* check if we alerady got this tree */
		gottree = 0;
		for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )
		{
			struct db_full_path tmp_path;

			db_full_path_init( &tmp_path );
			if( db_string_to_path( &tmp_path, rtip->rti_dbip, regp->reg_name ) )
			{
				Tcl_AppendResult(interp, "db_string_to_path failed for ",
					regp->reg_name, "\n", (char *)NULL );
				rt_clean( rtip );
				bu_free( (char *)rtip, "f_make_bb: rtip" );
				return TCL_ERROR;
			}
			if( path.fp_names[0] == tmp_path.fp_names[0] )
				gottree = 1;
			db_free_full_path( &tmp_path );
			if( gottree )
				break;
		}

		/* if we don't already have it, get it */
		if( !gottree && rt_gettree( rtip, path.fp_names[0]->d_namep ) )
		{
			Tcl_AppendResult(interp, "rt_gettree failed for ",
				argv[i], "\n", (char *)NULL );
			rt_clean( rtip );
			bu_free( (char *)rtip, "f_make_bb: rtip" );
			return TCL_ERROR;
		}
		db_free_full_path( &path );
	}

	/* prep calculates bounding boxes of solids */
	rt_prep( rtip );

	/* initialize RPP bounds */
	VSETALL( rpp_min, MAX_FASTF );
	VREVERSE( rpp_max, rpp_min );
	for( i=2 ; i<argc ; i++ )
	{
		vect_t reg_min, reg_max;
		struct region *regp;
		CONST char *reg_name;

		/* check if input name is a region */
		for( regp = rtip->HeadRegion; regp != REGION_NULL; regp = regp->reg_forw )
		{
			reg_name = regp->reg_name;
			if( *argv[i] != '/' && *reg_name == '/' )
				reg_name++;

			if( !strcmp( reg_name, argv[i] ) )
				break;
				
		}

		if( regp != REGION_NULL )
		{
			/* input name was a region  */
			if( rt_bound_tree( regp->reg_treetop, reg_min, reg_max ) )
			{
				Tcl_AppendResult(interp, "rt_bound_tree failed for ",
					regp->reg_name, "\n", (char *)NULL );
				rt_clean( rtip );
				bu_free( (char *)rtip, "f_make_bb: rtip" );
				return TCL_ERROR;
			}
			VMINMAX( rpp_min, rpp_max, reg_min );
			VMINMAX( rpp_min, rpp_max, reg_max );
		}
		else
		{
			int name_len;

			/* input name may be a group, need to check all regions under
			 * that group
			 */
			name_len = strlen( argv[i] );
			for( regp = rtip->HeadRegion; regp != REGION_NULL; regp = regp->reg_forw )
			{
				reg_name = regp->reg_name;
				if( *argv[i] != '/' && *reg_name == '/' )
					reg_name++;

				if( strncmp( argv[i], reg_name, name_len ) )
					continue;

				/* This is part of the group */
				if( rt_bound_tree( regp->reg_treetop, reg_min, reg_max ) )
				{
					Tcl_AppendResult(interp, "rt_bound_tree failed for ",
						regp->reg_name, "\n", (char *)NULL );
					rt_clean( rtip );
					bu_free( (char *)rtip, "f_make_bb: rtip" );
					return TCL_ERROR;
				}
				VMINMAX( rpp_min, rpp_max, reg_min );
				VMINMAX( rpp_min, rpp_max, reg_max );
			}
		}
	}

	/* build bounding RPP */
	VMOVE( arb.pt[0], rpp_min );
	VSET( arb.pt[1], rpp_min[X], rpp_min[Y], rpp_max[Z] );
	VSET( arb.pt[2], rpp_min[X], rpp_max[Y], rpp_max[Z] );
	VSET( arb.pt[3], rpp_min[X], rpp_max[Y], rpp_min[Z] );
	VSET( arb.pt[4], rpp_max[X], rpp_min[Y], rpp_min[Z] );
	VSET( arb.pt[5], rpp_max[X], rpp_min[Y], rpp_max[Z] );
	VMOVE( arb.pt[6], rpp_max );
	VSET( arb.pt[7], rpp_max[X], rpp_max[Y], rpp_min[Z] );
	arb.magic = RT_ARB_INTERNAL_MAGIC;

	/* set up internal structure */
	RT_INIT_DB_INTERNAL( &new_intern );
	new_intern.idb_type = ID_ARB8;
	new_intern.idb_ptr = (genptr_t)(&arb);
	RT_INIT_EXTERNAL( &new_extern );

	/* export it */
	if( rt_functab[new_intern.idb_type].ft_export( &new_extern, &new_intern, 1.0 ) < 0 )
	{
	  Tcl_AppendResult(interp, "f_make_bb: export failure\n", (char *)NULL);
	  rt_functab[new_intern.idb_type].ft_ifree( &new_intern );
	  return TCL_ERROR;
	}

	/* Add this new solid to the directory */
	ngran = (new_extern.ext_nbytes+sizeof(union record)-1) / sizeof(union record);
	if( (dp = db_diradd( dbip, new_name, -1L, ngran, DIR_SOLID)) == DIR_NULL ||
		db_alloc( dbip, dp, 1 ) < 0 )
	{
	  db_free_external( &new_extern );
	  TCL_ALLOC_ERR_return;
	}

	/* and finally, write it to disk */
	if (db_put_external( &new_extern, dp, dbip ) < 0 )
	{
	  db_free_external( &new_extern );
	  TCL_WRITE_ERR_return;
	}

	/* clean up */
	db_free_external( &new_extern );

	rt_clean( rtip );
	bu_free( (char *)rtip, "f_make_bb: rtip" );

	/* use "e" command to get new solid displayed */
	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = new_name;
	  av[2] = NULL;

	  return f_edit( clientData, interp, 2, av );
	}
}

int
f_whatid(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct directory *dp;
	union record rec;
	char id[10];

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
		return TCL_ERROR;

	if( (dp=db_lookup( dbip, argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return TCL_ERROR;

	if( !( dp->d_flags & DIR_REGION ) )
	{
		Tcl_AppendResult(interp, argv[1], " is not a region\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( db_get( dbip, dp, &rec, 0, 1 ) )
	{
		Tcl_AppendResult(interp, "Cannot get database record for ",
			argv[1], "\n", (char *)NULL );
		return TCL_ERROR;
	}

	sprintf( id, "%d\n", rec.c.c_regionid );
	Tcl_AppendResult(interp, id, (char *)NULL );

	return TCL_OK;
}

int
f_eac(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int i,j;
	int item;
	struct directory *dp;
	union record rec;
	struct bu_vls v;
	int new_argc;
	int lim;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
		return TCL_ERROR;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	bu_vls_init( &v );

	bu_vls_strcat( &v, "e" );
	lim = 1;

	for( j=1; j<argc; j++)
	{
		item = atoi( argv[j] );

		for( i = 0; i < RT_DBNHASH; i++ )
		{
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )
			{
				if( (dp->d_flags & DIR_COMB|DIR_REGION) !=
				    (DIR_COMB|DIR_REGION) )
					continue;
				if( db_get( dbip, dp, &rec, 0, 1 ) < 0 ) {
				  TCL_READ_ERR_return;
				}
				if( rec.c.c_regionid != 0 ||
					rec.c.c_aircode != item )
						continue;

				bu_vls_strcat( &v, " " );
				bu_vls_strcat( &v, dp->d_namep );
				lim++;
			}
		}
	}

	if( lim > 1 )
	{
		int retval;
		char **new_argv;

		new_argv = (char **)bu_calloc( lim+1, sizeof( char *), "f_eac: new_argv" );
		new_argc = rt_split_cmd( new_argv, lim+1, bu_vls_addr( &v ) );
		retval = f_edit( clientData, interp, new_argc, new_argv );
		bu_vls_free( &v );
		bu_free( (char *)new_argv, "f_eac: new_argv" );
		return retval;
	}
	else
	{
		bu_vls_free( &v );
		return TCL_OK;
	}
}
