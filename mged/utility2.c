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
 *
 */

#include <signal.h>
#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include "../librt/debug.h"	/* XXX */

static union record record;

void		identitize();
void		trace();
void		matrix_print();
void		push();

/*  	F _ T A B O B J :   tabs objects as they appear in data file
 */
int
f_tabobj(argc, argv)
int argc;
char **argv;
{
	register struct directory *dp;
	int ngran, nmemb;
	int i, j, k, kk;

	/* interupts */
	(void)signal( SIGINT, sig2 );

	for(i=1; i<argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL )
			continue;
		if( db_get( dbip, dp, &record, 0, 1) < 0 ) {
			READ_ERR;
			return CMD_BAD;
		}
		if(record.u_id == ID_ARS_A) {
			(void)printf("%c %d %s ",record.a.a_id,record.a.a_type,record.a.a_name);
			(void)printf("%d %d %d %d\n",record.a.a_m,record.a.a_n,
				record.a.a_curlen,record.a.a_totlen);
			/* the b-records */
			ngran = record.a.a_totlen;
			for(j=1; j<=ngran; j++) {
				if( db_get( dbip, dp, &record, j, 1) < 0 ) {
					READ_ERR;
					return CMD_BAD;
				}
				(void)printf("%c %d %d %d\n",record.b.b_id,record.b.b_type,record.b.b_n,record.b.b_ngranule);
				for(k=0; k<24; k+=6) {
					for(kk=k; kk<k+6; kk++)
						(void)printf("%10.4f ",record.b.b_values[kk]*base2local);
					(void)printf("\n");
				}
			}
		}

		if(record.u_id == ID_SOLID) {
			(void)printf("%c %d %s %d\n", record.s.s_id,
				record.s.s_type,record.s.s_name,
				record.s.s_cgtype);
			for(kk=0;kk<24;kk+=6){
				for(j=kk;j<kk+6;j++)
					(void)printf("%10.4f ",record.s.s_values[j]*base2local);
				(void)printf("\n");
			}
		}
		if(record.u_id == ID_COMB) {
			(void)printf("%c '%c' %s %d %d %d %d %d \n",
			record.c.c_id,record.c.c_flags,
			record.c.c_name,record.c.c_regionid,
			record.c.c_aircode, dp->d_len-1,
			record.c.c_material,record.c.c_los);
			nmemb = dp->d_len-1;
			for(j=1; j<=nmemb; j++) {
				mat_t	xmat;

				if( db_get( dbip, dp, &record, j, 1) < 0 ) {
					READ_ERR;
					return CMD_BAD;
				}
				(void)printf("%c %c %s\n",
					record.M.m_id,
					record.M.m_relation,
					record.M.m_instname);
				rt_mat_dbmat( xmat, record.M.m_mat );
				matrix_print( xmat );
				(void)putchar('\n');
			}
		}
		if(record.u_id == ID_P_HEAD) {
			(void)printf("POLYGON: not implemented yet\n");
		}

		if(record.u_id == ID_BSOLID) {
			(void)printf("SPLINE: not implemented yet\n");
		}
	}
	return CMD_OK;
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
f_pathsum(argc, argv)
int	argc;
char	**argv;
{
	int i, flag, pos_in;

	/* pos_in = first member of path entered
	 *
	 *	paths are matched up to last input member
	 *      ANY path the same up to this point is considered as matching
	 */
	prflag = 0;

	/* interupts */
	(void)signal( SIGINT, sig2 );

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
		(void)printf("Enter the path (space is delimiter): ");
		return CMD_MORE;
	}

	pos_in = 1;
	objpos = argc-1;

	/* build directory pointer array for desired path */
	for(i=0; i<objpos; i++) {
		if( (obj[i] = db_lookup( dbip, argv[pos_in+i], LOOKUP_NOISY )) == DIR_NULL)
			return CMD_BAD;
	}

	mat_idn(identity);
	mat_idn( xform );

	trace(obj[0], 0, identity, flag);

	if(prflag == 0) {
		/* path not found */
		(void)printf("PATH:  ");
		for(i=0; i<objpos; i++)
			(void)printf("/%s",obj[i]->d_namep);
		(void)printf("  NOT FOUND\n");
	}

	return CMD_OK;
}



/*   	F _ C O P Y E V A L : copys an evaluated solid
 */

static union record saverec;

int
f_copyeval(argc, argv)
int argc;
char **argv;
{

	register struct directory *dp;
	int	i;
	int	j;
	int	k;
	int	kk = 1;
	int	ngran;
	int	pos_in;
	vect_t	vec;

	prflag = 0;
	pos_in = argc;

	printf("The copyeval command is currently being reconstructed.\n\
Sorry for the inconvenience.\n");

#if 0
	argcnt = 0;

	/* interupts */
	(void)signal( SIGINT, sig2 );

	/* get the path - ignore any input so far */
	(void)printf("Enter the complete path: ");
	argcnt = getcmd(args);
	args += argcnt;
	objpos = argcnt;

	/* build directory pointer array for desired path */
	for(i=0; i<objpos; i++) {
		if( (obj[i] = db_lookup( dbip, cmd_args[pos_in+i], LOOKUP_NOISY)) == DIR_NULL)
			return CMD_BAD;
	}

	/* check if last path member is a solid */
	if( db_get( dbip,  obj[objpos-1], &record, 0, 1) < 0 ) {
		READ_ERR;
		return;
	}
	if(record.u_id != ID_SOLID && record.u_id != ID_ARS_A &&
		record.u_id != ID_BSOLID && record.u_id != ID_P_HEAD) {
		(void)printf("Bottom of path is not a solid\n");
		return CMD_BAD;
	}

	/* get the new solid name */
	(void)printf("Enter the new solid name: ");
	argcnt = getcmd(args);

	/* check if new solid name already exists in description */
	if( db_lookup( dbip, cmd_args[args], LOOKUP_QUIET) != DIR_NULL ) {
		(void)printf("%s: already exists\n",cmd_args[args]);
		return CMD_BAD;
	}

	mat_idn( identity );
	mat_idn( xform );

	trace(obj[0], 0, identity, CPEVAL);

	if(prflag == 0) {
		(void)printf("PATH:  ");
		for(i=0; i<objpos; i++)
			(void)printf("/%s",obj[i]->d_namep);
		(void)printf("  NOT FOUND\n");
		return CMD_BAD;
	}

	/* No interupts */
	(void)signal( SIGINT, SIG_IGN );

	/* Have found the desired path - xform is the transformation matrix */
	/* xform matrix calculated in trace() */

	/* create the new solid */
	if(saverec.u_id == ID_ARS_A) {
		NAMEMOVE(cmd_args[args], saverec.a.a_name);
		ngran = saverec.a.a_totlen;
		if( (dp = db_diradd( dbip, saverec.a.a_name, -1, ngran+1, DIR_SOLID)) == DIR_NULL ||
		    db_alloc( dbip, dp, ngran+1 ) < 0 )  {
		    	ALLOC_ERR;
			return CMD_BAD;
		}
		if( db_put( dbip, dp, &saverec, 0, 1 ) < 0 ) {
			WRITE_ERR;
			return CMD_BAD;
		}

		/* apply transformation to the b-records */
		for(i=1; i<=ngran; i++) {
			if( db_get( dbip, obj[objpos-1], &record, i , 1) < 0 ) {
				READ_ERR;
				return CMD_BAD;
			}
			if(i == 1) {
				/* vertex */
				MAT4X3PNT( vec, xform,
						&record.b.b_values[0] );
				VMOVE(&record.b.b_values[0], vec);
				kk = 1;
			}


			/* rest of the vectors */
			for(k=kk; k<8; k++) {
				MAT4X3VEC( vec, xform,
						&record.b.b_values[k*3] );
				VMOVE(&record.b.b_values[k*3], vec);
			}
			kk = 0;

			/* write this b-record */
			if( db_put( dbip, dp, &record, i, 1) < 0 ) {
				WRITE_ERR;
				return CMD_BAD;
			}
		}
		return CMD_OK;
	}

	if(saverec.u_id == ID_BSOLID) {
		(void)printf("B-SPLINEs not implemented\n");
		return CMD_BAD;
	}

	if(saverec.u_id == ID_P_HEAD) {
		(void)printf("POLYGONs not implemented\n");
		return CMD_BAD;
	}

	if(saverec.u_id == ID_SOLID) {
		NAMEMOVE(cmd_args[args], saverec.s.s_name);
		if( (dp = db_diradd( dbip, saverec.s.s_name, -1, 1, DIR_SOLID)) == DIR_NULL ||
		    db_alloc( dbip, dp, 1 ) < 0 )  {
			ALLOC_ERR;
			return CMD_BAD;
		}
		MAT4X3PNT( vec, xform, &saverec.s.s_values[0] );
		VMOVE(&saverec.s.s_values[0], vec);
		for(i=3; i<=21; i+=3) {
			MAT4X3VEC( vec, xform, &saverec.s.s_values[i] );
			VMOVE(&saverec.s.s_values[i], vec);
		}
		if( db_put( dbip, dp, &saverec, 0, 1 ) < 0 ){
			WRITE_ERR;
			return CMD_BAD;
		}
		return CMD_OK;
	}
#endif

	return CMD_BAD;
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
	int nparts, i, k, j;
	int arslen, npt, n;
	int	kk = 0;
	vect_t	vertex;
	vect_t	vec;

	if( pathpos >= MAX_LEVELS ) {
		(void)printf("nesting exceeds %d levels\n",MAX_LEVELS);
		for(i=0; i<MAX_LEVELS; i++)
			(void)printf("/%s", path[i]->d_namep);
		(void)printf("\n");
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
		(void)printf("/%s",path[k]->d_namep);
	if(flag == LISTPATH) {
		(void)printf("/%s\n",record.s.s_name);
		return;
	}

	/* NOTE - only reach here if flag == LISTEVAL */
	/* do_list will print actual solid name */
	(void)printf("/");
	do_list( stdout, dp, 1 );
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

	for(i=0; i<16; i++) {
		if( (i+1)%4 )
			(void)printf("%f\t",m[i]);
		else if(i == 15)
			(void)printf("%f\t",m[i]);
		else
			(void)printf("%f\n",m[i]*base2local);
	}
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
 * linked list could be handled by rt_list macros but it is simple
 * enough to do hear with out them.
 */
HIDDEN union tree *push_leaf( tsp, pathp, ep, id)
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	struct rt_db_internal intern;
	union tree	*curtree;
	struct directory *dp;
	register struct push_id *pip;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);

	dp = pathp->fp_names[pathp->fp_len-1];

	if (rt_g.debug&DEBUG_TREEWALK) {
		char *sofar = db_path_to_string(pathp);
		rt_log("push_leaf(%s) path='%s'\n",
		    rt_functab[id].ft_name, sofar);
		rt_free(sofar, "path string");
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
				rt_log("push_leaf: matrix mismatch between '%s' and prior reference.\n",
				    sofar, dp->d_namep);
				rt_free(sofar, "path string");
				push_error = 1;
			}
			RES_RELEASE(&rt_g.res_worker);
			GETUNION(curtree, tree);
			curtree->magic = RT_TREE_MAGIC;
			curtree->tr_op = OP_NOP;
			return curtree;
		}
	}
/*
 * This is the first time we have seen this solid.
 */
	pip = (struct push_id *) rt_malloc(sizeof(struct push_id),
	    "Push ident");
	pip->magic = MAGIC_PUSH_ID;
	pip->pi_dir = dp;
	mat_copy(pip->pi_mat, tsp->ts_mat);
	pip->back = pi_head.back;
	pi_head.back = pip;
	pip->forw = &pi_head;
	pip->back->forw = pip;
	RES_RELEASE(&rt_g.res_worker);
	GETUNION(curtree, tree);
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
f_push(argc, argv)
int argc;
char **argv;
{
	int	ncpu;
	int	c;
	int	old_debug;
	int	levels;
	extern 	int optind;
	extern	char *optarg;
	extern	struct rt_tol	mged_tol;	/* from ged.c */
	extern	struct rt_tess_tol mged_ttol;
	int	i;
	int	id;
	struct push_id *pip;
	struct rt_external	es_ext;
	struct rt_db_internal	es_int;

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
			levels=atoi(optarg);
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
			printf("push: usage push [-l levels] [-P processors] [-d] root [root2 ...]\n");
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
			rt_free((char *)pip, "Push ident");
		}
		rt_g.debug = old_debug;
		rt_log("push:\tdb_walk_tree failed or there was a solid moving\n\tin two or more directions\n");
		return CMD_BAD;
	}
/*
 * We've built the push solid list, now all we need to do is apply
 * the matrix we've stored for each solid.
 */
	FOR_ALL_PUSH_SOLIDS(pip) {
		RT_INIT_EXTERNAL(&es_ext);
		RT_INIT_DB_INTERNAL(&es_int);
		if (db_get_external( &es_ext, pip->pi_dir, dbip) < 0) {
			rt_log("f_push: Read error fetching '%s'\n",
			    pip->pi_dir->d_namep);
			push_error = -1;
			continue;
		}
		id = rt_id_solid( &es_ext);
		if (rt_functab[id].ft_import(&es_int, &es_ext, pip->pi_mat) < 0 ) {
			rt_log("push(%s): solid import failure\n",
			    pip->pi_dir->d_namep);
			if (es_int.idb_ptr) rt_functab[id].ft_ifree( &es_int);
			db_free_external( &es_ext);
			continue;
		}
		RT_CK_DB_INTERNAL( &es_int);
		if ( rt_functab[id].ft_export( &es_ext, &es_int, 1.0) < 0 ) {
			rt_log("push(%s): solid export failure\n", pip->pi_dir->d_namep);
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
		rt_free((char *)pip, "Push ident");
	}

	rt_g.debug = old_debug;
	return push_error ? CMD_BAD : CMD_OK;
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
	mat_t	identity;

	mat_idn( identity );
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
