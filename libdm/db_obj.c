/*
 *				D B _ O B J . C
 *
 * A database object contains the attributes and
 * methods for controlling a BRLCAD database. Much
 * of this code was borrowed from MGED.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 */

#include <fcntl.h>
#include <math.h>
#include <sys/errno.h>
#include "conf.h"
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "solid.h"
#include "cmd.h"

#include "../librt/debug.h"	/* XXX */

/* This must be defined by the application */
extern Tcl_Interp *interp;

/* from librt/tcl.c */
extern int rt_db();

struct db_obj {
  struct bu_list	l;
  struct bu_vls		dbo_name;	/* database name */
  struct db_i		*dbo_ip;	/* database instance pointer */
  struct rt_wdb		*dbo_wp;
  struct solid		dbo_headSolid;	/* head of solid list */
};

HIDDEN int dbo_cmd();
HIDDEN int dbo_db_tcl();
HIDDEN int dbo_dbip_tcl();
HIDDEN int dbo_ls_tcl();
HIDDEN int dbo_draw_tcl();
HIDDEN int dbo_headSolid_tcl();
HIDDEN int dbo_close_tcl();
#if 0
HIDDEN int dbo_slist_tcl();
HIDDEN int dbo_vlist_tcl();
HIDDEN int dbo_clist_tcl();
#endif

HIDDEN int dbo_cmpdirname();
HIDDEN void dbo_vls_col_pr4v();
HIDDEN void dbo_vls_line_dpp();
HIDDEN struct directory ** dbo_getspace();
HIDDEN union tree *dbo_wireframe_region_end();
HIDDEN union tree *dbo_wireframe_leaf();
HIDDEN void dbo_bound_solid();
HIDDEN void dbo_drawH_part2();

HIDDEN struct db_tree_state	dbo_initial_tree_state;
HIDDEN struct rt_tess_tol	dbo_ttol;
HIDDEN struct bn_tol		dbo_tol;

HIDDEN struct db_obj HeadDBObj;	/* head of BRLCAD database object list */
HIDDEN struct db_i *curr_dbip;		/* current database instance pointer */
HIDDEN struct solid *curr_hsp;		/* current head solid pointer */

HIDDEN struct cmdtab dbo_cmds[] = {
       "db",			dbo_db_tcl,
       "dbip",			dbo_dbip_tcl,
       "ls",			dbo_ls_tcl,
       "draw",			dbo_draw_tcl,
       "headSolid",		dbo_headSolid_tcl,
       "close",			dbo_close_tcl,
       (char *)0,		(int (*)())0
};

/*
 *			D M _ C M D
 *
 * Generic interface for database commands.
 * Usage:
 *        procname cmd ?args?
 *
 * Returns: result of dbo command.
 */
HIDDEN int
dbo_cmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  return do_cmd(clientData, interp, argc, argv, dbo_cmds, 1);
}

int
Dbo_Init(interp)
Tcl_Interp *interp;
{
  BU_LIST_INIT(&HeadDBObj.l);

  /* initilize tolerance structures */
  dbo_ttol.magic = RT_TESS_TOL_MAGIC;
  dbo_ttol.abs = 0.0;		/* disabled */
  dbo_ttol.rel = 0.01;
  dbo_ttol.norm = 0.0;		/* disabled */

  dbo_tol.magic = BN_TOL_MAGIC;
  dbo_tol.dist = 0.005;
  dbo_tol.dist_sq = dbo_tol.dist * dbo_tol.dist;
  dbo_tol.perp = 1e-6;
  dbo_tol.para = 1 - dbo_tol.perp;

  /* initialize tree state */
  dbo_initial_tree_state = rt_initial_tree_state;  /* struct copy */
  dbo_initial_tree_state.ts_ttol = &dbo_ttol;
  dbo_initial_tree_state.ts_tol = &dbo_tol;

  return TCL_OK;
}

/*
 * Called by Tcl when the object is destroyed.
 */
HIDDEN void
dbo_deleteProc(clientData)
     ClientData clientData;
{
	struct db_obj *dbop = (struct db_obj *)clientData;

	bu_log("dbo_deleteProc\n");

	bu_vls_free(&dbop->dbo_name);

	RT_CK_WDB(dbop->dbo_wp);
	wdb_close(dbop->dbo_wp);

	RT_CK_DBI(dbop->dbo_ip);
	db_close(dbop->dbo_ip);

	BU_LIST_DEQUEUE(&dbop->l);
	bu_free((genptr_t)dbop, "dbo_deleteProc: dbop");
}

/*
 * Close a BRLCAD database object.
 *
 * USAGE:
 *	  dbo_close name
 *	  procname close
 */
HIDDEN int
dbo_close_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	struct db_obj *dbop = (struct db_obj *)clientData;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dbo_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

#if 0
	/* check to see if database object exists */
	for (BU_LIST_FOR(dbop, db_obj, &HeadDBObj.l)) {
		/* found it */
		if (strcmp(argv[1],bu_vls_addr(&dbop->dbo_name)) == 0) {
			Tcl_DeleteCommand(interp, bu_vls_addr(&dbop->dbo_name));
			dbo_deleteProc(clientData);
			return TCL_OK;
		}
	}

	Tcl_AppendResult(interp, "dbo_close: ", argv[1],
			 " does not exist.\n", (char *)NULL);
	return TCL_ERROR;
#else
	/* Among other things, this will call dbo_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&dbop->dbo_name));

	return TCL_OK;
#endif
}

/*
 * Open/create a BRLCAD database object.
 *
 * USAGE:
 *	  dbo_open [name dbname] 
 */
HIDDEN int
dbo_open_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct db_obj *dbop;
  struct db_i *dbip;
  struct rt_wdb *wdbp;
  struct bu_vls vls;
  static int count = 0;

  if (argc == 1) {
    /* get list of database objects */
    for (BU_LIST_FOR(dbop, db_obj, &HeadDBObj.l))
      Tcl_AppendResult(interp, bu_vls_addr(&dbop->dbo_name), " ", (char *)NULL);

    return TCL_OK;
  }

  if (argc != 3) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib db_open");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /*XXX Leaving this commented out allows us to redefine an object. */
#if 0
  /* check to see if database object exists */
  for (BU_LIST_FOR(dbop, db_obj, &HeadDBObj.l)) {
    if (strcmp(argv[1],bu_vls_addr(&dbop->dbo_name)) == 0) {
      Tcl_AppendResult(interp, "dbo_open: ", argv[1],
		       " exists.\n", (char *)NULL);
      return TCL_ERROR;
    }
  }
#endif

  /* open database */
  if (((dbip = db_open(argv[2], "r+w")) == DBI_NULL ) &&
      ((dbip = db_open(argv[2], "r"  )) == DBI_NULL )) {
    char line[128];

    /*
     * Check to see if we can access the database
     */
    if (access(argv[2], R_OK|W_OK) != 0 && errno != ENOENT) {
      perror(argv[2]);
      return TCL_ERROR;
    }

    if ((dbip = db_create(argv[2])) == DBI_NULL) {
      Tcl_AppendResult(interp, "dbo_open: failed to create ", argv[2], "\n",\
		       (char *)NULL);
      if (dbip == DBI_NULL)
	Tcl_AppendResult(interp, "opendb: no database is currently opened!", \
			 (char *)NULL);

      return TCL_ERROR;
    }
  }

  /* --- Scan geometry database and build in-memory directory --- */
  db_scan(dbip, (int (*)())db_diradd, 1);

  wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);

  if (wdbp == RT_WDB_NULL) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "db_open: wdb_dbopen %s failed\n", argv[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* acquire db_obj struct */
  BU_GETSTRUCT(dbop,db_obj);

  /* initialize db_obj */
  bu_vls_init(&dbop->dbo_name);
  bu_vls_strcpy(&dbop->dbo_name,argv[1]);
  dbop->dbo_ip = dbip;
  dbop->dbo_wp = wdbp;
  BU_LIST_INIT(&dbop->dbo_headSolid.l);

  /* append to list of db_obj's */
  BU_LIST_APPEND(&HeadDBObj.l,&dbop->l);

  (void)Tcl_CreateCommand(interp,
			  bu_vls_addr(&dbop->dbo_name),
			  dbo_cmd,
			  (ClientData)dbop,
			  dbo_deleteProc);

  /* Return new function name as result */
  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, bu_vls_addr(&dbop->dbo_name), (char *)NULL);
  return TCL_OK;
}

/****************** Database Object Methods ********************/
/*
 *
 * Usage:
 *        procname db ?args?
 *
 * Returns: result of db command.
 */
HIDDEN int
dbo_db_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct db_obj *dbop = (struct db_obj *)clientData;

  return rt_db(dbop->dbo_wp, interp, argc-1, argv+1);
}

/*
 *
 * Usage:
 *        procname dbip
 *
 * Returns: database objects dbip.
 */
HIDDEN int
dbo_dbip_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct db_obj *dbop = (struct db_obj *)clientData;
  struct bu_vls vls;

  bu_vls_init(&vls);

  if (argc != 2) {
    bu_vls_printf(&vls, "helplib dbo_dbip");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_printf(&vls, "%lu", (unsigned long)dbop->dbo_ip);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *
 * Usage:
 *        procname headSolid
 *
 * Returns: database objects headSolid.
 */
HIDDEN int
dbo_headSolid_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct db_obj *dbop = (struct db_obj *)clientData;
  struct bu_vls vls;

  bu_vls_init(&vls);

  if (argc != 2) {
    bu_vls_printf(&vls, "helplib dbo_headSolid");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_printf(&vls, "%lu", (unsigned long)&dbop->dbo_headSolid);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *
 * Usage:
 *        procname ls [args]
 *
 * Returns: list objects in this database object.
 * The guts of this routine were lifted from mged/dir.c.
 */
HIDDEN int
dbo_ls_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct db_obj *dbop = (struct db_obj *)clientData;
  struct bu_vls vls;
  register struct directory *dp;
  register int i;
  int c;
  int aflag = 0;		/* print all objects without formatting */
  int cflag = 0;		/* print combinations */
  int rflag = 0;		/* print regions */
  int sflag = 0;		/* print solids */
  struct directory **dirp;
  struct directory **dirp0 = (struct directory **)NULL;

  bu_vls_init(&vls);

  /* skip past procname */
  --argc;
  ++argv;

  if(argc < 1 || MAXARGS < argc){
    bu_vls_printf(&vls, "helplib dbo_ls");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_optind = 1;	/* re-init bu_getopt() */
  while ((c = bu_getopt(argc, argv, "acgrs")) != EOF) {
    switch (c) {
    case 'a':
      aflag = 1;
      break;
    case 'c':
      cflag = 1;
      break;
    case 'r':
      rflag = 1;
      break;
    case 's':
      sflag = 1;
      break;
    default:
      bu_vls_printf(&vls, "Unrecognized option - %c\n", c);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);
      return TCL_ERROR;
    }
  }
  argc -= (bu_optind - 1);
  argv += (bu_optind - 1);

  /* create list of objects in database */
  if (argc > 1) {
    /* Just list specified names */
    dirp = dbo_getspace(dbop->dbo_ip, argc-1);
    dirp0 = dirp;
    /*
     * Verify the names, and add pointers to them to the array.
     */
    for (i = 1; i < argc; i++) {
      if ((dp = db_lookup(dbop->dbo_ip, argv[i], LOOKUP_NOISY)) ==
	  DIR_NULL)
	continue;
      *dirp++ = dp;
    }
  } else {
    /* Full table of contents */
    dirp = dbo_getspace(dbop->dbo_ip, 0);	/* Enough for all */
    dirp0 = dirp;
    /*
     * Walk the directory list adding pointers (to the directory
     * entries) to the array.
     */
    for (i = 0; i < RT_DBNHASH; i++)
      for (dp = dbop->dbo_ip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
	*dirp++ = dp;
  }

  if (aflag || cflag || rflag || sflag)
    dbo_vls_line_dpp(&vls, dirp0, (int)(dirp - dirp0),
		 aflag, cflag, rflag, sflag);
  else
    dbo_vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0));

  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  bu_free( (genptr_t)dirp0, "dbo_getspace dp[]" );

  return TCL_OK;
}

/*
 * Prepare database objects for drawing.
 *
 * Usage:
 *        procname draw [args]
 *
 */
HIDDEN int
dbo_draw_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct db_obj *dbop = (struct db_obj *)clientData;
  struct bu_vls vls;
  int ret;

  bu_vls_init(&vls);

  if (argc < 2) {
    bu_vls_printf(&vls, "helplib dbo_draw");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* return list of database objects that have been prepped for drawing */
  if (argc == 2) {
    /* XXX implement me */
    Tcl_AppendResult(interp, "not ready yet!\n", (char *)NULL);
    return TCL_OK;
  }

  curr_dbip = dbop->dbo_ip;
  curr_hsp = &dbop->dbo_headSolid;

  argc -= 2;
  argv += 2;
  /* Wireframes */
  ret = db_walk_tree(dbop->dbo_ip, argc, (CONST char **)argv,
		     1,			/* # of cpu's */
		     &dbo_initial_tree_state,
		     0,			/* take all regions */
		     dbo_wireframe_region_end,
		     dbo_wireframe_leaf );
  if (ret < 0) {
    /* XXX free resources */

    Tcl_AppendResult(interp, "dbo_draw: db_walk_tree failed\n", (char *)NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/****************** utility routines ********************/
/*
 *			D B O _ C M P D I R N A M E
 *
 * Given two pointers to pointers to directory entries, do a string compare
 * on the respective names and return that value.
 *  This routine was lifted from mged/columns.c.
 */
int
dbo_cmpdirname(a, b)
CONST genptr_t a;
CONST genptr_t b;
{
	register struct directory **dp1, **dp2;

	dp1 = (struct directory **)a;
	dp2 = (struct directory **)b;
	return( strcmp( (*dp1)->d_namep, (*dp2)->d_namep));
}

/*
 *			D B O _ V L S _ C O L _ P R 4 V
 *
 *  Given a pointer to a list of pointers to names and the number of names
 *  in that list, sort and print that list in column order over four columns.
 *  This routine was lifted from mged/columns.c.
 */
HIDDEN void
dbo_vls_col_pr4v(vls, list_of_names, num_in_list)
struct bu_vls *vls;
struct directory **list_of_names;
int num_in_list;
{
  int lines, i, j, namelen, this_one;

  qsort( (genptr_t)list_of_names,
	 (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	 (int (*)())dbo_cmpdirname);

  /*
   * For the number of (full and partial) lines that will be needed,
   * print in vertical format.
   */
  lines = (num_in_list + 3) / 4;
  for( i=0; i < lines; i++) {
    for( j=0; j < 4; j++) {
      this_one = j * lines + i;
      /* Restrict the print to 16 chars per spec. */
      bu_vls_printf(vls,  "%.16s", list_of_names[this_one]->d_namep);
      namelen = strlen( list_of_names[this_one]->d_namep);
      if( namelen > 16)
	namelen = 16;
      /*
       * Region and ident checks here....  Since the code
       * has been modified to push and sort on pointers,
       * the printing of the region and ident flags must
       * be delayed until now.  There is no way to make the
       * decision on where to place them before now.
       */
      if(list_of_names[this_one]->d_flags & DIR_COMB) {
	bu_vls_putc(vls, '/');
	namelen++;
      }
      if(list_of_names[this_one]->d_flags & DIR_REGION) {
	bu_vls_putc(vls, 'R');
	namelen++;
      }
      /*
       * Size check (partial lines), and line termination.
       * Note that this will catch the end of the lines
       * that are full too.
       */
      if( this_one + lines >= num_in_list) {
	bu_vls_putc(vls, '\n');
	break;
      } else {
	/*
	 * Pad to next boundary as there will be
	 * another entry to the right of this one. 
	 */
	while( namelen++ < 20)
	  bu_vls_putc(vls, ' ');
      }
    }
  }
}

/*
 *			D B O _ V L S _ L I N E _ D P P
 *
 *  Given a pointer to a list of pointers to names and the number of names
 *  in that list, sort and print that list on the same line.
 *  This routine was lifted from mged/columns.c.
 */
HIDDEN void
dbo_vls_line_dpp(vls, list_of_names, num_in_list, aflag, cflag, rflag, sflag)
struct bu_vls *vls;
struct directory **list_of_names;
int num_in_list;
int aflag;	/* print all objects */
int cflag;	/* print combinations */
int rflag;	/* print regions */
int sflag;	/* print solids */
{
  int i;
  int isComb, isRegion;
  int isSolid;

  qsort( (genptr_t)list_of_names,
	 (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	 (int (*)())dbo_cmpdirname);

  /*
   * i - tracks the list item
   */
  for (i=0; i < num_in_list; ++i) {
    if (list_of_names[i]->d_flags & DIR_COMB) {
      isComb = 1;
      isSolid = 0;

      if (list_of_names[i]->d_flags & DIR_REGION)
	isRegion = 1;
      else
	isRegion = 0;
    } else {
      isComb = isRegion = 0;
      isSolid = 1;
    }

    /* print list item i */
    if (aflag ||
	!cflag && !rflag && !sflag ||
	cflag && isComb ||
	rflag && isRegion ||
	sflag && isSolid) {
      bu_vls_printf(vls,  "%s ", list_of_names[i]->d_namep);
    }
  }
}

/*
 *			D B O _ G E T S P A C E
 *
 * This routine walks through the directory entry list and mallocs enough
 * space for pointers to hold:
 *  a) all of the entries if called with an argument of 0, or
 *  b) the number of entries specified by the argument if > 0.
 *  This routine was lifted from mged/dir.c.
 */
HIDDEN struct directory **
dbo_getspace(dbip, num_entries)
struct db_i *dbip;
register int num_entries;
{
  register struct directory *dp;
  register int i;
  register struct directory **dir_basep;

  if (num_entries < 0) {
    bu_log("dbo_getspace: was passed %d, used 0\n",
	   num_entries);
    num_entries = 0;
  }

  if (num_entries == 0) {
    /* Set num_entries to the number of entries */
    for (i = 0; i < RT_DBNHASH; i++)
      for (dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
	num_entries++;
  }

  /* Allocate and cast num_entries worth of pointers */
  dir_basep = (struct directory **) bu_malloc(
	      (num_entries+1) * sizeof(struct directory *),
	      "dbo_getspace *dir[]" );
  return(dir_basep);
}

HIDDEN union tree *
dbo_wireframe_region_end(tsp, pathp, curtree)
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
  return (curtree);
}

/*
 *			D M O _ W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
dbo_wireframe_leaf(tsp, pathp, ep, id)
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct bu_external	*ep;
int			id;
{
  struct rt_db_internal	intern;
  union tree	*curtree;
  int		dashflag;		/* draw with dashed lines */
  struct bu_list	vhead;

  RT_CK_TESS_TOL(tsp->ts_ttol);
  BN_CK_TOL(tsp->ts_tol);

  BU_LIST_INIT(&vhead);

  if (rt_g.debug&DEBUG_TREEWALK) {
    char	*sofar = db_path_to_string(pathp);

    Tcl_AppendResult(interp, "dbo_wireframe_leaf(", rt_functab[id].ft_name,
		     ") path='", sofar, "'\n", (char *)NULL);
    bu_free((genptr_t)sofar, "path string");
  }

  /* solid lines */
  dashflag = 0;
#if 0
  dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER));
#endif

  RT_INIT_DB_INTERNAL(&intern);
  if (rt_functab[id].ft_import(&intern, ep, tsp->ts_mat, curr_dbip) < 0) {
    Tcl_AppendResult(interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
		     ":  solid import failure\n", (char *)NULL);

    if (intern.idb_ptr)
      rt_functab[id].ft_ifree( &intern );
    return (TREE_NULL);		/* ERROR */
  }
  RT_CK_DB_INTERNAL(&intern);

  if (rt_functab[id].ft_plot(&vhead,
			     &intern,
			     tsp->ts_ttol,
			     tsp->ts_tol) < 0) {
    Tcl_AppendResult(interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
		     ": plot failure\n", (char *)NULL);
    rt_functab[id].ft_ifree(&intern);
    return (TREE_NULL);		/* ERROR */
  }

  /*
   * XXX HACK CTJ - dbo_drawH_part2 sets the default color of a
   * solid by looking in tps->ts_mater.ma_color, for pseudo
   * solids, this needs to be something different and drawH
   * has no idea or need to know what type of solid this is.
   */
  if (intern.idb_type == ID_GRIP) {
    int r,g,b;
    r= tsp->ts_mater.ma_color[0];
    g= tsp->ts_mater.ma_color[1];
    b= tsp->ts_mater.ma_color[2];
    tsp->ts_mater.ma_color[0] = 0;
    tsp->ts_mater.ma_color[1] = 128;
    tsp->ts_mater.ma_color[2] = 128;
    dbo_drawH_part2(dashflag, &vhead, pathp, tsp);
    tsp->ts_mater.ma_color[0] = r;
    tsp->ts_mater.ma_color[1] = g;
    tsp->ts_mater.ma_color[2] = b;
  } else {
    dbo_drawH_part2(dashflag, &vhead, pathp, tsp);
  }
  rt_functab[id].ft_ifree(&intern);

  /* Indicate success by returning something other than TREE_NULL */
  BU_GETUNION(curtree, tree);
  curtree->magic = RT_TREE_MAGIC;
  curtree->tr_op = OP_NOP;

  return (curtree);
}

/*
 *  Compute the min, max, and center points of the solid.
 *  Also finds s_vlen;
 * XXX Should split out a separate bn_vlist_rpp() routine, for librt/vlist.c
 */
HIDDEN void
dbo_bound_solid(sp)
register struct solid *sp;
{
  register struct bn_vlist	*vp;
  register double			xmax, ymax, zmax;
  register double			xmin, ymin, zmin;

  xmax = ymax = zmax = -INFINITY;
  xmin = ymin = zmin =  INFINITY;
  sp->s_vlen = 0;
  for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
    register int	j;
    register int	nused = vp->nused;
    register int	*cmd = vp->cmd;
    register point_t *pt = vp->pt;
    for (j = 0; j < nused; j++,cmd++,pt++) {
      switch (*cmd) {
      case BN_VLIST_POLY_START:
      case BN_VLIST_POLY_VERTNORM:
	/* Has normal vector, not location */
	break;
      case BN_VLIST_LINE_MOVE:
      case BN_VLIST_LINE_DRAW:
      case BN_VLIST_POLY_MOVE:
      case BN_VLIST_POLY_DRAW:
      case BN_VLIST_POLY_END:
	V_MIN(xmin, (*pt)[X]);
	V_MAX(xmax, (*pt)[X]);
	V_MIN(ymin, (*pt)[Y]);
	V_MAX(ymax, (*pt)[Y]);
	V_MIN(zmin, (*pt)[Z]);
	V_MAX(zmax, (*pt)[Z]);
	break;
      default:
	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "unknown vlist op %d\n", *cmd);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}
      }
    }
    sp->s_vlen += nused;
  }

  sp->s_center[X] = (xmin + xmax) * 0.5;
  sp->s_center[Y] = (ymin + ymax) * 0.5;
  sp->s_center[Z] = (zmin + zmax) * 0.5;

  sp->s_size = xmax - xmin;
  V_MAX( sp->s_size, ymax - ymin );
  V_MAX( sp->s_size, zmax - zmin );
}

/*
 *			D M O _ D R A W h _ P A R T 2
 *
 *  Once the vlist has been created, perform the common tasks
 *  in handling the drawn solid.
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN void
dbo_drawH_part2(dashflag, vhead, pathp, tsp)
int			dashflag;
struct bu_list		*vhead;
struct db_full_path	*pathp;
struct db_tree_state	*tsp;
{
  register struct solid *sp;
  register int	i;
  register struct dm_list *dmlp;
  register struct dm_list *save_dmlp;

  if (pathp->fp_len > MAX_PATH) {
    char *cp = db_path_to_string(pathp);

    Tcl_AppendResult(interp, "dbo_drawH_part2: path too long, solid ignored.\n\t",
		     cp, "\n", (char *)NULL);
    bu_free((genptr_t)cp, "Path string");
    return;
  }

  BU_GETSTRUCT(sp,solid);
  BU_LIST_INIT(&sp->s_vlist);

  /*
   * Compute the min, max, and center points.
   */
  BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
  dbo_bound_solid(sp);

  sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.;
  sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.;
  sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.;
  sp->s_color[0] = sp->s_basecolor[0];
  sp->s_color[1] = sp->s_basecolor[1];
  sp->s_color[2] = sp->s_basecolor[2];

  sp->s_cflag = 0;
  sp->s_iflag = DOWN;
  sp->s_soldash = dashflag;
  sp->s_Eflag = 0; /* This is a solid */
  sp->s_last = pathp->fp_len-1;

  /* Copy path information */
  for( i=0; i<=sp->s_last; i++ ) {
    sp->s_path[i] = pathp->fp_names[i];
  }

  sp->s_regionid = tsp->ts_regionid;

  /* Solid is successfully drawn */
  /* Add to linked list of solid structs */
  bu_semaphore_acquire( RT_SEM_MODEL );
  BU_LIST_APPEND(curr_hsp->l.back, &sp->l);
  bu_semaphore_release( RT_SEM_MODEL );
}
