/*
 *				P R O C _ R E G . C
 *
 * This module deals with building an edge description of a region.
 *
 * Functions -
 *	f_evedit	Evaluated edit something (add to visible display)
 *	Edrawtree	Call drawHobj to draw a tree
 *	EdrawHobj	Call Edrawsolid for all solids in an object
 *	EdrawHsolid	Manage the drawing of a COMGEOM solid
 *	proc_region	processes each member (solid) of a region
 *
 * Authors -
 *	Keith Applin
 *	Gary Kuehl
 *  
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

extern void color_soltab();

int		drawreg;	/* if > 0, process and draw regions */
int		regmemb;	/* # of members left to process in a region */
int		reg_pathpos;	/* pathpos of a processed region */
int		reg_error;	/* error encountered in region processing */
char		memb_oper;	/* operation for present member of processed region */

extern int	no_memory;	/* flag indicating memory for drawing is used up */
extern long	nvectors;	/* from dodraw.c */
extern struct rt_tol		mged_tol;		/* from ged.c */

void		Edrawtree();
void		EdrawHobj();
int		EdrawHsolid();

static void	center(), dwreg(), ellin(), move(), points(),
		regin(), solin(), solpl(), tgcin(), tplane(),
		vectors();
static int	arb(), cgarbs(), gap(), redoarb(), 
		region();

static union record input;		/* Holds an object file record */

/* following variables are used in processing regions
 *  for producing edge representations
 */
#define NMEMB	250	/* max number of members in a region */
static int	nmemb = 0;		/* actual number of members */
static int	m_type[NMEMB];		/* member solid types */
static char	m_op[NMEMB];		/* member operations */
static fastf_t	m_param[NMEMB*24];	/* member parameters */
static int	memb_count = 0;		/* running count of members */
static int	param_count = 0; 	/* location in m_param[] array */

/*
 *			F _ E V E D I T
 *
 *  The "Big E" command.
 *  Evaluated Edit something (add to visible display)
 *  Usage: E object(s)
 */
int
f_evedit(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int	i;
	time_t		stime, etime;	/* start & end times */
	static int	first_time = 1;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	drawreg = 1;
	regmemb = -1;

	/* Innards from old eedit() */
	nvectors = 0;
	(void)time( &stime );
	for( i=1; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) == DIR_NULL )
			continue;

		if( dmp->dmr_displaylist )  {
			/*
			 * Delete any portion of object
			 * remaining from previous draw.
			 */
			eraseobj( dp );
			dmaflag++;
			refresh();
			dmaflag++;
		}

		/*
		 * Draw this object as a ROOT object, level 0
		 * on the path, with no displacement, and
		 * unit scale.
		 */
		if( no_memory )  {
		  Tcl_AppendResult(interp, "No memory left so cannot draw ",
				   dp->d_namep, "\n", (char *)NULL);
		  drawreg = 0;
		  regmemb = -1;
		  continue;
		}

		Edrawtree( dp );
		regmemb = -1;
	}
	(void)time( &etime );
	if(first_time && BU_LIST_NON_EMPTY(&HeadSolid.l)){
		first_time = 0;

		size_reset();
		new_mats();
	}

	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "E: %ld vectors in %ld sec\n", nvectors, etime - stime );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}
	dmp->dmr_colorchange(dmp);
	dmaflag = 1;

	return TCL_OK;
}

struct directory	*cur_path[MAX_PATH];	/* Record of current path */

static struct mater_info mged_no_mater = {
	/* RT default is white.  This is red, to stay clear of illuminate mode */
	1.0, 0, 0,		/* */
	0,			/* override */
	DB_INH_LOWER,		/* color inherit */
	DB_INH_LOWER		/* mater inherit */
};

/*
 *			D R A W T R E E
 *
 *  This routine is the analog of rt_gettree().
 */
void
Edrawtree( dp )
struct directory	*dp;
{
	mat_t		root;
	struct mater_info	root_mater;

	root_mater = mged_no_mater;	/* struct copy */

	mat_idn( root );
	/* Could apply root animations here ? */

	EdrawHobj( dp, ROOT, 0, root, 0, &root_mater );
}

/*
 *			D R A W H O B J
 *
 * This routine is used to get an object drawn.
 * The actual drawing of solids is performed by Edrawsolid(),
 * but all transformations down the path are done here.
 */
void
EdrawHobj( dp, flag, pathpos, old_xlate, regionid, materp )
register struct directory *dp;
int		flag;
int		pathpos;
matp_t		old_xlate;
int		regionid;
struct mater_info *materp;
{
	struct rt_external	ext;
	union record	*rp;
	auto mat_t	new_xlate;	/* Accumulated translation matrix */
	auto int	i;
	struct mater_info curmater;

	if( pathpos >= MAX_PATH )  {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "nesting exceeds %d levels\n", MAX_PATH );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);

	  for(i=0; i<MAX_PATH; i++)
	    Tcl_AppendResult(interp, "/", cur_path[i]->d_namep, (char *)NULL);

	  Tcl_AppendResult(interp, "\n", (char *)NULL);
	  return;			/* ERROR */
	}

	/*
	 * Load the record into local record buffer
	 */
	if( db_get_external( &ext, dp, dbip ) < 0 )
		return;

	rp = (union record *)ext.ext_buf;
	if( rp[0].u_id != ID_COMB )  {
		register struct solid *sp;

		if( rt_id_solid( &ext ) == ID_NULL )  {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "Edrawobj(%s):  defective database record, type='%c' (0%o) addr=x%x\n",
				dp->d_namep, rp[0].u_id, rp[0].u_id, dp->d_addr );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);

		  goto out;		/* ERROR */
		}
		/*
		 * Enter new solid (or processed region) into displaylist.
		 */
		cur_path[pathpos] = dp;

		GET_SOLID(sp, &FreeSolid.l);
		if( sp == SOLID_NULL )
			return;		/* ERROR */
		if( EdrawHsolid( sp, flag, pathpos, old_xlate, &ext, regionid, materp ) != 1 ) {
			FREE_SOLID(sp, &FreeSolid.l);
		}
		goto out;
	}

	/*
	 *  At this point, u_id == ID_COMB.
	 *  Process a Combination (directory) node
	 */
	if( dp->d_len <= 1 )  {
	  Tcl_AppendResult(interp, "Warning: combination with zero members \"",
			   dp->d_namep, "\".\n", (char *)NULL);
	  goto out;			/* non-fatal ERROR */
	}

	/*
	 *  Handle inheritance of material property.
	 *  Color and the material property have separate
	 *  inheritance interlocks.
	 */
	curmater = *materp;	/* struct copy */
	if( rp[0].c.c_override == 1 )  {
		if( regionid != 0 )  {
		  Tcl_AppendResult(interp, "Edrawobj: ERROR: color override in combination within region ", dp->d_namep, "\n", (char *)NULL);
		} else {
			if( curmater.ma_cinherit == DB_INH_LOWER )  {
				curmater.ma_override = 1;
				curmater.ma_color[0] =
					((double)(rp[0].c.c_rgb[0]))*rt_inv255;
				curmater.ma_color[1] =
					((double)(rp[0].c.c_rgb[1]))*rt_inv255;
				curmater.ma_color[2] =
					((double)(rp[0].c.c_rgb[2]))*rt_inv255;
				curmater.ma_cinherit = rp[0].c.c_inherit;
			}
		}
	}
	if( rp[0].c.c_matname[0] != '\0' )  {
		if( regionid != 0 )  {
		  Tcl_AppendResult(interp, "Edrawobj: ERROR: material property spec in combination within region ", dp->d_namep, "\n", (char *)NULL);
		} else {
			if( curmater.ma_minherit == DB_INH_LOWER )  {
				strncpy( curmater.ma_matname, rp[0].c.c_matname, sizeof(rp[0].c.c_matname) );
				strncpy( curmater.ma_matparm, rp[0].c.c_matparm, sizeof(rp[0].c.c_matparm) );
				curmater.ma_minherit = rp[0].c.c_inherit;
			}
		}
	}

	/* Handle combinations which are the top of a "region" */
	if( rp[0].c.c_flags == 'R' )  {
	  if( regionid != 0 ){
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "regionid %d overriden by %d\n",
			  regionid, rp[0].c.c_regionid );
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	  }

	  regionid = rp[0].c.c_regionid;
	}

	/*
	 *  This node is a combination (eg, a directory).
	 *  Process all the arcs (eg, directory members).
	 */
	if( drawreg && rp[0].c.c_flags == 'R' && dp->d_len > 1 ) {
	  if( regmemb >= 0  ) {
	    Tcl_AppendResult(interp, "ERROR: region (", dp->d_namep,
			     ") is member of region (", cur_path[reg_pathpos]->d_namep,
			     ")\n", (char *)NULL);
	    goto out;	/* ERROR */
	  }
	  /* Well, we are processing regions and this is a region */
	  /* if region has only 1 member, don't process as a region */
	  if( dp->d_len > 2) {
	    regmemb = dp->d_len-1;
	    reg_pathpos = pathpos;
	  }
	}

	/* Process all the member records */
	for( i=1; i < dp->d_len; i++ )  {
		register struct member	*mp;
		register struct directory *nextdp;
		static mat_t		xmat;	/* temporary fastf_t matrix */

		mp = &(rp[i].M);
		if( mp->m_id != ID_MEMB )  {
		  Tcl_AppendResult(interp, "EdrawHobj:  ", dp->d_namep,
				   " bad member rec\n", (char *)NULL);
		  goto out;			/* ERROR */
		}
		cur_path[pathpos] = dp;
		if( regmemb > 0  ) { 
			regmemb--;
			memb_oper = mp->m_relation;
		}
		if( (nextdp = db_lookup( dbip,  mp->m_instname, LOOKUP_NOISY )) == DIR_NULL )
			continue;

		/* s' = M3 . M2 . M1 . s
		 * Here, we start at M3 and descend the tree.
		 * convert matrix to fastf_t from disk format.
		 */
		rt_mat_dbmat( xmat, mp->m_mat );
		/* Check here for animation to apply */
		mat_mul(new_xlate, old_xlate, xmat);

		/* Recursive call */
		EdrawHobj(
			nextdp,
			(mp->m_relation != SUBTRACT) ? ROOT : INNER,
			pathpos + 1,
			new_xlate,
			regionid,
			&curmater
		);
	}
out:
	db_free_external( &ext );
}

/*
 *			D R A W H S O L I D
 *
 * Returns -
 *	-1	on error
 *	 0	if NO OP
 *	 1	if solid was drawn
 */
int
EdrawHsolid( sp, flag, pathpos, xform, ep, regionid, materp )
register struct solid	*sp;
int			flag;
int			pathpos;
mat_t			xform;
struct rt_external	*ep;
int			regionid;
struct mater_info	*materp;
{
	register int i;
	int dashflag;		/* draw with dashed lines */
	struct bu_list	vhead;
	struct rt_tess_tol	ttol;

	BU_LIST_INIT( &vhead );
	if( regmemb >= 0 ) {
		/* processing a member of a processed region */
		/* regmemb  =>  number of members left */
		/* regmemb == 0  =>  last member */
		/* reg_error > 0  =>  error condition  no more processing */
		if(reg_error) { 
			if(regmemb == 0) {
				reg_error = 0;
				regmemb = -1;
			}
			return(-1);		/* ERROR */
		}
		if(memb_oper == UNION)
			flag = 999;

		/* The hard part */
		i = proc_region( (union record *)ep->ext_buf, xform, flag );

		if( i < 0 )  {
		  /* error somwhere */
		  Tcl_AppendResult(interp, "Error in converting solid ",
				   cur_path[reg_pathpos]->d_namep, " to ARBN\n", (char *)NULL);
		  reg_error = 1;
		  if(regmemb == 0) {
		    regmemb = -1;
		    reg_error = 0;
		  }
		  return(-1);		/* ERROR */
		}
		reg_error = 0;		/* reset error flag */

		/* if more member solids to be processed, no drawing was done
		 */
		if( regmemb > 0 )
		  return(0);		/* NOP -- more to come */

		i = finish_region( &vhead );
		if( i < 0 )  {
		  Tcl_AppendResult(interp, "error in finish_region()\n", (char *)NULL);
		  return(-1);		/* ERROR */
		}
		dashflag = 0;
	}  else  {
		/* Doing a normal solid */
		struct rt_db_internal	intern;
		int id;

		dashflag = (flag != ROOT);

		id = rt_id_solid( ep );
		if( id <= 0 || id >= rt_nfunctab )  {
		  Tcl_AppendResult(interp, "EdrawHsolid(", cur_path[pathpos]->d_namep,
				   "):  unknown database object\n", (char *)NULL);
		  return(-1);			/* ERROR */
		}

	    	RT_INIT_DB_INTERNAL(&intern);
		if( rt_functab[id].ft_import( &intern, ep, xform ) < 0 )  {
		  Tcl_AppendResult(interp, cur_path[pathpos]->d_namep,
				   ":  solid import failure\n", (char *)NULL);
		  if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		  return(-1);			/* FAIL */
		}
		RT_CK_DB_INTERNAL( &intern );

		ttol.magic = RT_TESS_TOL_MAGIC;
		ttol.abs = mged_abs_tol;
		ttol.rel = mged_rel_tol;
		ttol.norm = mged_nrm_tol;

		if( rt_functab[id].ft_plot( &vhead,
					    &intern,
					    &ttol, &mged_tol ) < 0 )  {
		  Tcl_AppendResult(interp, cur_path[pathpos]->d_namep,
				   ": vector conversion failure\n", (char *)NULL);
		}
		rt_functab[id].ft_ifree( &intern );
	}

	/* Take note of the base color */
	if( materp )  {
		sp->s_basecolor[0] = materp->ma_color[0] * 255.;
		sp->s_basecolor[1] = materp->ma_color[1] * 255.;
		sp->s_basecolor[2] = materp->ma_color[2] * 255.;
	}

	/*
	 * Compute the min, max, and center points.
	 */
	BU_LIST_APPEND_LIST( &(sp->s_vlist), &vhead );

	mged_bound_solid( sp );
	nvectors += sp->s_vlen;

	/*
	 * If this solid is not illuminated, fill in it's information.
	 * A solid might be illuminated yet vectorized again by redraw().
	 */
	if( sp != illump )  {
		sp->s_iflag = DOWN;
		sp->s_soldash = dashflag;

		if(regmemb == 0) {
			/* done processing a region */
			regmemb = -1;
			sp->s_last = reg_pathpos;
			sp->s_Eflag = 1;	/* This is processed region */
		}  else  {
			sp->s_Eflag = 0;	/* This is a solid */
			sp->s_last = pathpos;
		}
		/* Copy path information */
		for( i=0; i<=sp->s_last; i++ )
			sp->s_path[i] = cur_path[i];
	}
	sp->s_regionid = regionid;
	sp->s_addr = 0;
	sp->s_bytes = 0;

	/* Cvt to displaylist, determine displaylist memory requirement. */
	if( !no_memory && (sp->s_bytes = dmp->dmr_cvtvecs( dmp, sp )) != 0 )  {
		/* Allocate displaylist storage for object */
		sp->s_addr = rt_memalloc( &(dmp->dmr_map), sp->s_bytes );
		if( sp->s_addr == 0 )  {
		  no_memory = 1;
		  Tcl_AppendResult(interp, "Edraw: out of Displaylist\n", (char *)NULL);
		  sp->s_bytes = 0;	/* not drawn */
		} else {
			sp->s_bytes = dmp->dmr_load(dmp, sp->s_addr, sp->s_bytes );
		}
	}

	/* Solid is successfully drawn */
	if( sp != illump )  {
		/* Add to linked list of solid structs */
	  BU_LIST_APPEND(HeadSolid.l.back, &sp->l);
	  dmp->dmr_viewchange( dmp, DM_CHGV_ADD, sp );
	} else {
	  /* replacing illuminated solid -- struct already linked in */
	  sp->s_iflag = UP;
	  dmp->dmr_viewchange( dmp, DM_CHGV_REPL, sp );
	}

	return(1);		/* OK */
}

/*
 * This routine processes each member(solid) of a region.
 *
 * When the last member is processed, dwreg() is executed.
 *
 * Returns -
 *	-1	error
 *	 0	region drawn
 *	 1	more solids follow, please re-invoke w/next solid.
 */
proc_region( recordp, xform, flag )
register union record *recordp;
register mat_t xform;
int flag;
{
	register int i;
	register dbfloat_t *op;	/* Used for scanning vectors */
	static vect_t	work;		/* Vector addition work area */
	static int length, type;
	static int uvec[8], svec[11];
	static int cgtype;

	input = *recordp;		/* struct copy */

	type = recordp->s.s_type;
	cgtype = type;

	if(type == GENARB8) {
		/* check for arb8, arb7, arb6, arb5, arb4 */
		points();
		if( (i = cgarbs(uvec, svec)) == 0 ) {
			nmemb = param_count = memb_count = 0;
			return(-1);	/* ERROR */
		}
		if(redoarb(uvec, svec, i) == 0) {
			nmemb = param_count = memb_count = 0;
			return(-1);	/* ERROR */
		}
		vectors();
/* XXX */
		cgtype = input.s.s_cgtype;
	}
	if(cgtype == RPP || cgtype == BOX || cgtype == GENARB8)
		cgtype = ARB8;

	if(flag == ROOT)
		m_op[memb_count] = '+';
	else
	if(flag == 999)
		m_op[memb_count] = 'u';
	else
		m_op[memb_count] = '-';

	m_type[memb_count++] = cgtype;
	if(memb_count > NMEMB) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "proc_reg: region has more than %d members\n", NMEMB);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  nmemb = param_count = memb_count = 0;
	  return(-1);	/* ERROR */
	}

	switch( cgtype )  {

	case ARB8:
		length = 8;
arbcom:		/* common area for arbs */
		MAT4X3PNT( work, xform, &input.s.s_values[0] );
		VMOVE( &input.s.s_values[0], work );
		VMOVE(&m_param[param_count], &input.s.s_values[0]);
		param_count += 3;
		op = &input.s.s_values[1*3];
		for( i=1; i<length; i++ )  {
			MAT4X3VEC( work, xform, op );
			VADD2(op, input.s.s_values, work);
			VMOVE(&m_param[param_count], op);
			param_count += 3;
			op += 3;
		}
		break;

	case ARB7:
		length = 7;
		goto arbcom;

	case ARB6:
		VMOVE(&input.s.s_values[15], &input.s.s_values[18]);
		length = 6;
		goto arbcom;

	case ARB5:
		length = 5;
		goto arbcom;

	case ARB4:
		length = 4;
		VMOVE(&input.s.s_values[9], &input.s.s_values[12]);
		goto arbcom;

	case GENTGC:
		op = &recordp->s.s_values[0*3];
		MAT4X3PNT( work, xform, op );
		VMOVE( op, work );
		VMOVE(&m_param[param_count], op);
		param_count += 3;
		op += 3;

		for( i=1; i<6; i++ )  {
			MAT4X3VEC( work, xform, op );
			VMOVE( op, work );
			VMOVE(&m_param[param_count], op);
			param_count += 3;
			op += 3;
		}
		break;

	case GENELL:
		op = &recordp->s.s_values[0*3];
		MAT4X3PNT( work, xform, op );
		VMOVE( op, work );
		VMOVE(&m_param[param_count], op);
		param_count += 3;
		op += 3;

		for( i=1; i<4; i++ )  {
			MAT4X3VEC( work, xform, op );
			VMOVE( op, work );
			VMOVE(&m_param[param_count], op);
			param_count += 3;
			op += 3;
		}
		break;

	default:
	  {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "proc_reg:  Cannot draw solid type %d (%s)\n",
			  type, type == TOR ? "TOR":
			  type == ARS ? "ARS" : "UNKNOWN TYPE" );
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	  }

	  nmemb = param_count = memb_count = 0;
	  return(-1);	/* ERROR */
	}
	return(0);		/* OK */
}

int
finish_region(vhead)
struct bu_list	*vhead;
{
	/* last member solid has been seen, now draw the region */
	nmemb = memb_count;
	param_count = memb_count = 0;
	if(nmemb == 0) {
		nmemb = param_count = memb_count = 0;
		return(-1);	/* ERROR */
	}
	dwreg(vhead);
	return(0);		/* OK, region was drawn */
}

#define NO	0
#define YES	1

/* C G A R B S :   determines COMGEOM arb types from GED general arbs
 */
static int
cgarbs( uvec, svec )
register int *uvec;	/* array of unique points */
register int *svec;	/* array of like points */
{
	register int i,j;
	static int numuvec, unique, done;
	static int si;

	done = NO;		/* done checking for like vectors */

	svec[0] = svec[1] = 0;
	si = 2;

	for(i=0; i<7; i++) {
		unique = YES;
		if(done == NO)
			svec[si] = i;
		for(j=i+1; j<8; j++) {
			vect_t	diff;
			VSUB2( diff, &input.s.s_values[i*3], &input.s.s_values[j*3] );
			if( VNEAR_ZERO( diff, 0.0001 ) )  {
				/* Points are the same */
				if( done == NO )
					svec[++si] = j;
				unique = NO;
			}
		}
		if( unique == NO ) {  	/* point i not unique */
			if( si > 2 && si < 6 ) {
				svec[0] = si - 1;
				if(si == 5 && svec[5] >= 6)
					done = YES;
				si = 6;
			}
			if( si > 6 ) {
				svec[1] = si - 5;
				done = YES;
			}
		}
	}
	if( si > 2 && si < 6 ) 
		svec[0] = si - 1;
	if( si > 6 )
		svec[1] = si - 5;
	for(i=1; i<=svec[1]; i++)
		svec[svec[0]+1+i] = svec[5+i];
	for(i=svec[0]+svec[1]+2; i<11; i++)
		svec[i] = -1;
	/* find the unique points */
	numuvec = 0;
	for(j=0; j<8; j++) {
		unique = YES;
		for(i=2; i<svec[0]+svec[1]+2; i++) {
			if( j == svec[i] ) {
				unique = NO;
				break;
			}
		}
		if( unique == YES )
			uvec[numuvec++] = j;
	}

	/* put comgeom solid typpe into s_cgtype */
	switch( numuvec ) {

	case 8:
		input.s.s_cgtype = ARB8;  /* ARB8 */
		break;

	case 6:
		input.s.s_cgtype = ARB7;	/* ARB7 */
		break;

	case 4:
		if(svec[0] == 2)
			input.s.s_cgtype = ARB6;	/* ARB6 */
		else
			input.s.s_cgtype = ARB5;	/* ARB5 */
		break;

	case 2:
		input.s.s_cgtype = ARB4;	/* ARB4 */
		break;

	default:
	  {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "solid: %s  bad number of unique vectors (%d)\n",
			  input.s.s_name, numuvec);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	  }

	  return(0);
	}
	return( numuvec );
}

/*  R E D O A R B :   rearranges arbs to be GIFT compatible
 */
static int
redoarb( uvec, svec, numvec )
register int *uvec, *svec;
int numvec;
{
	register int i, j;
	static int prod;

	switch( input.s.s_cgtype ) {

	case ARB8:
		/* do nothing */
		break;

	case ARB7:
		/* arb7 vectors: 0 1 2 3 4 5 6 4 */
		switch( svec[2] ) {
			case 0:
				/* 0 = 1, 3, or 4 */
				if(svec[3] == 1)
					move(4,7,6,5,1,4,3,1);
				if(svec[3] == 3)
					move(4,5,6,7,0,1,2,0);
				if(svec[3] == 4)
					move(1,2,6,5,0,3,7,0);
				break;
			case 1:
				/* 1 = 2 or 5 */
				if(svec[3] == 2)
					move(0,4,7,3,1,5,6,1);
				if(svec[3] == 5)
					move(0,3,7,4,1,2,6,1);
				break;
			case 2:
				/* 2 = 3 or 6 */
				if(svec[3] == 3)
					move(6,5,4,7,2,1,0,2);
				if(svec[3] == 6)
					move(3,0,4,7,2,1,5,2);
				break;
			case 3:
				/* 3 = 7 */
				move(2,1,5,6,3,0,4,3);
				break;
			case 4:
				/* 4 = 5 */
				/* if 4 = 7  do nothing */
				if(svec[3] == 5)
					move(1,2,3,0,5,6,7,5);
				break;
			case 5:
				/* 5 = 6 */
				move(2,3,0,1,6,7,4,6);
				break;
			case 6:
				/* 6 = 7 */
				move(3,0,1,2,7,4,5,7);
				break;
			default:
			  Tcl_AppendResult(interp, "redoarb: ", input.s.s_name,
					   " - bad arb7\n", (char *)NULL);
			  return( 0 );
			}
			break;    	/* end of ARB7 case */

		case ARB6:
			/* arb6 vectors:  0 1 2 3 4 4 6 6 */

			prod = 1;
			for(i=0; i<numvec; i++)
				prod = prod * (uvec[i] + 1);
			switch( prod ) {
			case 24:
				/* 0123 unique */
				/* 4=7 and 5=6  OR  4=5 and 6=7 */
				if(svec[3] == 7)
					move(3,0,1,2,4,4,5,5);
				else
					move(0,1,2,3,4,4,6,6);
				break;
			case 1680:
				/* 4567 unique */
				/* 0=3 and 1=2  OR  0=1 and 2=3 */
				if(svec[3] == 3)
					move(7,4,5,6,0,0,1,1);
				else
					move(4,5,6,7,0,0,2,2);
				break;
			case 160:
				/* 0473 unique */
				/* 1=2 and 5=6  OR  1=5 and 2=6 */
				if(svec[3] == 2)
					move(0,3,7,4,1,1,5,5);
				else
					move(4,0,3,7,1,1,2,2);
				break;
			case 672:
				/* 3267 unique */
				/* 0=1 and 4=5  OR  0=4 and 1=5 */
				if(svec[3] == 1)
					move(3,2,6,7,0,0,4,4);
				else
					move(7,3,2,6,0,0,1,1);
				break;
			case 252:
				/* 1256 unique */
				/* 0=3 and 4=7  OR 0=4 and 3=7 */
				if(svec[3] == 3)
					move(1,2,6,5,0,0,4,4);
				else
					move(5,1,2,6,0,0,3,3);
				break;
			case 60:
				/* 0154 unique */
				/* 2=3 and 6=7  OR  2=6 and 3=7 */
				if(svec[3] == 3)
					move(0,1,5,4,2,2,6,6);
				else
					move(5,1,0,4,2,2,3,3);
				break;
			default:
			  Tcl_AppendResult(interp, "redoarb: ", input.s.s_name,
					   " - bad arb6\n", (char *)NULL);
			  return( 0 );
			}
			break; 		/* end of ARB6 case */

		case ARB5:
			/* arb5 vectors:  0 1 2 3 4 4 4 4 */
			prod = 1;
			for(i=2; i<6; i++)
				prod = prod * (svec[i] + 1);
			switch( prod ) {
			case 24:
				/* 0=1=2=3 */
				move(4,5,6,7,0,0,0,0);
				break;
			case 1680:
				/* 4=5=6=7 */
				/* do nothing */
				break;
			case 160:
				/* 0=3=4=7 */
				move(1,2,6,5,0,0,0,0);
				break;
			case 672:
				/* 2=3=7=6 */
				move(0,1,5,4,2,2,2,2);
				break;
			case 252:
				/* 1=2=5=6 */
				move(0,3,7,4,1,1,1,1);
				break;
			case 60:
				/* 0=1=5=4 */
				move(3,2,6,7,0,0,0,0);
				break;
			default:
			  Tcl_AppendResult(interp, "redoarb: ", input.s.s_name,
					   " - bad arb5\n", (char *)NULL);
			  return( 0 );
			}
			break;		/* end of ARB5 case */

		case ARB4:
			/* arb4 vectors:  0 1 2 0 4 4 4 4 */
			j = svec[6];
			if( svec[0] == 2 )
				j = svec[4];
			move(uvec[0],uvec[1],svec[2],uvec[0],j,j,j,j);
			break;

		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "redoarb %s: unknown arb type (%d)\n",
				  input.s.s_name,input.s.s_cgtype);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }

		  return( 0 );
	}

	return( 1 );
}


static void
move( p0, p1, p2, p3, p4, p5, p6, p7 )
int p0, p1, p2, p3, p4, p5, p6, p7;
{
	register int i, j;
	static int pts[8];
	static dbfloat_t copy[24];

	pts[0] = p0 * 3;
	pts[1] = p1 * 3;
	pts[2] = p2 * 3;
	pts[3] = p3 * 3;
	pts[4] = p4 * 3;
	pts[5] = p5 * 3;
	pts[6] = p6 * 3;
	pts[7] = p7 * 3;

	/* copy of the record */
	for(i=0; i<=21; i+=3) {
		VMOVE(&copy[i], &input.s.s_values[i]);
	}

	for(i=0; i<8; i++) {
		j = pts[i];
		VMOVE(&input.s.s_values[i*3], &copy[j]);
	}
}


static void
vectors()
{
	register int i;

	for(i=3; i<=21; i+=3) {
		VSUB2(&input.s.s_values[i],&input.s.s_values[i],&input.s.s_values[0]);
	}
}



static void
points()
{
	register int i;

	for(i=3; i<=21; i+=3) {
		VADD2(&input.s.s_values[i],&input.s.s_values[i],&input.s.s_values[0]);
	}
}




/*
 *      		D W R E G
 *
 *	Draws an "edge approximation" of a region
 *
 *	All solids are converted to planar approximations.
 *	Logical operations are then applied to the solids to
 *	produce the edge representation of the region.
 *
 * 	Gary Kuehl 	2Feb83
 *
 * 	Routines used by dwreg():
 *	  1. gap()   	puts gaps in the edges
 *	  2. region()	finds intersection of ray with a region
 *	  5. center()	finds center point of an arb
 *	  6. tplane()	tests if plane is inside enclosing rpp
 *	  7. arb()	finds intersection of ray with an arb
 *	  8. tgcin()	converts tgc to arbn
 *	  9. ellin()	converts ellg to arbn
 *	 10. regin()	process region to planes
 *	 11. solin()	finds enclosing rpp for solids
 *	 12. solpl()	process solids to arbn's
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	11/02/83  CMK	Modified to use g3.c module (device independence).
 */


#define NPLANES	500
static fastf_t	peq[NPLANES*4];		/* plane equations for EACH region */
static fastf_t	solrpp[NMEMB*6];	/* enclosing RPPs for each member of the region */
static fastf_t	regi[NMEMB], rego[NMEMB];	/* distances along ray where ray enters and leaves the region */
static fastf_t	pcenter[3];		/* center (interior) point of a solid */
static fastf_t	reg_min[3], reg_max[3];		/* min,max's for the region */
static fastf_t	sol_min[3], sol_max[3];		/* min,max's for a solid */
static fastf_t	xb[3];			/* starting point of ray */
static fastf_t	wb[3];			/* direction cosines of ray */
static fastf_t	rin, rout;		/* location where ray enters and leaves a solid */
static fastf_t	ri, ro;			/* location where ray enters and leaves a region */
static fastf_t	tol;			/* tolerance */
static fastf_t	*sp, *savesp;		/* pointers to the solid parameter array m_param[] */
static int	mlc[NMEMB];		/* location of last plane for each member */
static int	la, lb, lc, id, jd, ngaps;
static fastf_t	pinf = 1000000.0;
static int	negpos;
static char	oper;

static void
dwreg(vhead)
struct bu_list	*vhead;
{
	register int i,j;
	static int k,l;
	static int n,m;
	static int itemp;
	static int lmemb, umemb;/* lower and upper limit of members of region
				 * from one OR to the next OR */

	/* calculate center and scale for COMPLETE REGION since may have ORs */
	lmemb = umemb = 0;
	savesp = &m_param[0];
	while( 1 ) {
		/* Perhaps this can be eliminated?  Side effects? */
		for(umemb = lmemb+1; (umemb < nmemb && m_op[umemb] != 'u'); umemb++)
			;
		lc = 0;
		regin(1, lmemb, umemb);
		lmemb = umemb;
		if(umemb >= nmemb)
			break;
	}

	lmemb = 0;
	savesp = &m_param[0];

orregion:	/* sent here if region has or's */
	for(umemb = lmemb+1; (umemb < nmemb && m_op[umemb] != 'u'); umemb++)
		;

	lc = 0;
	regin(0, lmemb, umemb);

	l=0;

	/* id loop processes all member to the next OR */
	for(id=lmemb; id<umemb; id++) {
		if(mlc[id] < l)
			goto skipid;

		/* plane i is associated with member id */
		for(i=l; (i<(lc-1) && i<=mlc[id]); i++){
			m = i + 1;
			itemp = id;
			if(i == mlc[id])
				itemp = id + 1;
			for(jd=itemp; jd<umemb; jd++) {

				negpos = 0;
				if( (m_op[id] == '-' && m_op[jd] != '-') ||
				    (m_op[id] != '-' && m_op[jd] == '-') )
					negpos = 1;

				/* plane j is associated with member jd */
				for(j=m; j<=mlc[jd]; j++){
					if(id!=jd && m_op[id]=='-' && m_op[jd]=='-') { 
						for(k=0; k<3; k++) {
							if(solrpp[6*id+k] > solrpp[6*jd+k+3] || 
							   solrpp[6*id+k+3] < solrpp[6*jd+k] )
								goto sksolid;
						}

						for(k=mlc[jd-1]+1; k<=mlc[jd]; k++) {
							if(fabs(peq[i*4+3] - peq[k*4+3]) < tol && 
							    VDOT(&peq[i*4], &peq[k*4]) > .9999) {
								for(k=l; k<=mlc[id]; k++) {
									if(fabs(peq[j*4+3] - peq[k*4+3]) < tol && 
									    VDOT(&peq[j*4], &peq[k*4]) > .9999)
										goto noskip;
								}
								goto sksolid;
							}
						}
						for(k=l; k<= mlc[id]; k++) {
							if(fabs(peq[j*4+3] - peq[k*4+3]) < tol && 
							    VDOT(&peq[j*4], &peq[k*4]) > .9999)
								goto skplane;
						}
					}
noskip:
					if( rt_isect_2planes( xb, wb,
					    &peq[i*4], &peq[j*4], reg_min, &mged_tol ) < 0 )
						continue;

					/* check if ray intersects region */
					if( (k=region(lmemb,umemb))<=0)
						continue;

					/* ray intersects region */
					/* plot this ray  including gaps */
					for(n=0; n<k; n++){
						static vect_t	pi, po;

						VJOIN1( pi, xb, regi[n], wb );
						VJOIN1( po, xb, rego[n], wb );
						RT_ADD_VLIST( vhead, pi, RT_VLIST_LINE_MOVE );
						RT_ADD_VLIST( vhead, po, RT_VLIST_LINE_DRAW );
					}
skplane:				 ;
				}
sksolid:
				m = mlc[jd] + 1;
			}
		}
skipid:
		l=mlc[id]+1;
	}


	lmemb = umemb;
	if(umemb < nmemb)
		goto orregion;

	nmemb = 0;
	/* The finishing touches are done by EdrawHsolid() */
}


/* put gaps into region line */
static int
gap(si,so)
fastf_t si,so;
{
	register int igap,lgap,i;

	if(si<=ri+tol) goto front;
	if(so>=ro-tol) goto back;
	if(ngaps>0){
		for(igap=0;igap<ngaps;igap++) {
			/* locate position of gap along ray */
			if(si<=(regi[igap+1]+tol)) 
				goto insert;
		}
	}
	if((++ngaps)==25) return(-1);

	/* last gap along ray */
	rego[ngaps-1]=si;
	regi[ngaps]=so;
	return(1);

insert:		/* insert gap in ray between existing gaps */
	if(so < (rego[igap]-tol)) goto novlp;

	/* have overlapping gaps - sort out */
	V_MIN(rego[igap],si);
	if(regi[igap+1]>=so) return(1);
	regi[igap+1]=so;
	for(lgap=igap;(lgap<ngaps&&so<(rego[lgap]-tol));lgap++)
		;
	if(lgap==igap) return(1);
	if(so>regi[lgap+1]) {
		lgap++;
		igap++;
	}
	while(lgap<ngaps){
		rego[igap]=rego[lgap];
		regi[igap+1]=regi[lgap+1];
		lgap++;
		igap++;
	}
	ngaps=ngaps-lgap+igap;
	return(1);

novlp:		/* no overlapping gaps */
	if((++ngaps)>=25) return(-1);
	for(lgap=igap+1;lgap<ngaps;lgap++){
		rego[lgap]=rego[lgap-1];
		regi[lgap+1]=regi[lgap];
	}
	rego[igap]=si;
	regi[igap+1]=so;
	return(1);

front:		/* gap starts before beginning of ray */
	if((so+tol)>ro)
		return(0);
	V_MAX(ri,so);
	if(ngaps<1) return(1);
	for(igap=0; ((igap<ngaps) && ((ri+tol)>rego[igap])); igap++)
		;
	if(igap<1) return(1);
	V_MAX(ri, regi[igap]);
	lgap=ngaps;
	ngaps=0;
	if(igap>=lgap) return(1);
	for(i=igap;i<lgap;i++){
		rego[ngaps++]=rego[i];
		regi[ngaps]=regi[i+1];
	}
	return(1);

back:		/* gap ends after end of ray */
	V_MIN(ro,si);
	if(ngaps<1) return(1);
	for(igap=ngaps; (igap>0 && (ro<(regi[igap]+tol))); igap--)
		;
	if(igap < ngaps) {
		V_MIN(ro, rego[igap]);
	}
	ngaps=igap;
	return(1);
}


/* computes intersection of ray with region 
 *   	returns ngaps+1	if intersection
 *	        0	if no intersection
 */
static int
region(lmemb,umemb)
{
	register int	i, kd;
	static vect_t	s1, s2;
	static fastf_t a1, a2;

	ngaps=0;
	ri = -1.0 * pinf;
	ro=pinf;

	/* does ray intersect region rpp */
	VSUB2( s1, reg_min, xb );
	VSUB2( s2, reg_max, xb );

	/* find start & end point of ray with enclosing rpp */
	for(i=0;i<3;i++){
		if(fabs(wb[i])>.001){
			a1=s1[i]/wb[i];
			a2=s2[i]/wb[i];
			if(wb[i] <= 0.){
				if(a1<tol) return(0);
				V_MAX(ri,a2);
				V_MIN(ro,a1);
			} else {
				if(a2<tol) return(0);
				V_MAX(ri,a1);
				V_MIN(ro,a2);
			}
			if((ri+tol)>=ro) return(0);
		} else {
			if(s1[i]>tol || s2[i]<(-1.0*tol)) 
				return(0);
		}
	}

	/* ray intersects region - find intersection with each member */
	la=0;
	for(kd=lmemb;kd<umemb;kd++){
		oper=m_op[kd];
		lb=mlc[kd];

		/* if la > lb then no planes to check for this member */
		if(la > lb)
			continue;

		if(kd==id || kd==jd) oper='+';
		if(oper!='-'){
			/* positive solid  recalculate end points of ray */
			if( arb() == 0 )
				return(0);
			if(ngaps==0){
				V_MAX(ri,rin);
				V_MIN(ro,rout);
			} else{
				if(gap(-pinf, rin) <= 0)
					return(0);
				if(gap(rout, pinf) <= 0)
					return(0);
			}
		}
		if(oper == '-') {
			/* negative solid  look for gaps in ray */
			if(arb() != 0) {
				if(gap(rin, rout) <= 0)
					return(0);
			}
		}
		if(ri+tol>=ro) return(0);
		la=lb+1;
	}

	/*end of region - set number of intersections*/
	regi[0]=ri;
	rego[ngaps]=ro;
	return(ngaps+1);
}


/* find center point */
static void
center()
{
	register fastf_t ppc;
	register int i,j,k;

	for(i=0;i<3;i++){
		k=i;
		ppc=0.0;
		for(j=0; j<m_type[id]; j++) {
			ppc += *(sp+k);
			k+=3;
		}
		pcenter[i]=ppc/(fastf_t)m_type[id];
	}
}


/*
 *			T P L A N E
 *
 *  Register a plane which contains the 4 specified points,
 *  unless they are degenerate, or lie outside the region RPP.
 */
static void
tplane(p,q,r,s)
fastf_t *p, *q, *r, *s;
{
	register fastf_t *pp,*pf;
	register int i;

	/* If all 4 pts have coord outside region RPP,
	 * discard this plane, as the polygon is definitely outside */
	for(i=0;i<3;i++){
		FAST fastf_t t;
		t=reg_min[i]-tol;
		if(*(p+i)<t && *(q+i)<t && *(r+i)<t && *(s+i)<t)
			return;
		t=reg_max[i]+tol;
		if(*(p+i)>t && *(q+i)>t && *(r+i)>t  && *(s+i)>t)
			return;
	}

	/* Do these three points form a valid plane? */
	/* WARNING!!  Fourth point is never even looked at!! */
	pp = &peq[lc*4];
	/* Dist tol here used to be 1e-6 */
	if( rt_mk_plane_3pts( pp, p, q, r, &mged_tol ) < 0 )  return;

	if((pp[3]-VDOT(pp,pcenter)) > 0.)  {
		VREVERSE( pp, pp );
		pp[3] = -pp[3];
	}

	/* See if this plane already exists */
	for(pf = &peq[0];pf<pp;pf+=4) {
		 if(VDOT(pp,pf)>0.9999 && fabs(*(pp+3)-*(pf+3))<tol) 
			return;
	}

	/* increment plane counter */
	if(lc >= NPLANES) {
	  struct bu_vls tmp_vls;
	  
	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "tplane: More than %d planes for a region - ABORTING\n",
			NPLANES);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  return;
	}
	lc++;		/* Save plane eqn */
}

/* finds intersection of ray with arbitrary convex polyhedron */
static int
arb()
{
	static fastf_t s,*pp,dxbdn,wbdn,te;

	rin = ri;
	rout = ro;

	te = tol;
	if(oper == '-' && negpos)
		te = -1.0 * tol;

	for(pp = &peq[la*4];pp <= &peq[lb*4];pp+=4){
		dxbdn = *(pp+3)-VDOT(xb,pp);
		wbdn=VDOT(wb,pp);
		if(fabs(wbdn)>.001){
			s=dxbdn/wbdn;
			if(wbdn > 0.0) {
				V_MAX(rin, s);
			}
			else {
				V_MIN(rout,s);
			}
		}
		else{
			if(dxbdn>te) return(0);
		}
		if((rin+tol)>=rout || rout<=tol) return(0);
	}
	/* ray starts inside */
	V_MAX(rin,0);
	return(1);
}



/* convert tgc to an arbn */
static void
tgcin()
{
	static fastf_t vt[3], p[3], q[3], r[3], t[3];
	static fastf_t s,sa,c,ca,sd=.38268343,cd=.92387953;
	register int i,j,lk;

	lk = lc;

	for(i=0;i<3;i++){
		vt[i] = *(sp+i) + *(sp+i+3);
		pcenter[i]=( *(sp+i) + vt[i])*.5;
		p[i] = *(sp+i) + *(sp+i+6);
		t[i] = vt[i] + *(sp+i+12);
	}
	s=0.;
	c=1.;
	for(i=0;i<16;i++){
		sa=s*cd+c*sd;
		ca=c*cd-s*sd;
		for(j=0;j<3;j++){
			q[j] = *(sp+j) + ca * *(sp+j+6) + sa * *(sp+j+9);
			r[j]=vt[j]+ ca * *(sp+j+12) + sa* *(sp+j+15);
		}
		tplane( p, q, r, t );
		s=sa;
		c=ca;
		for(j=0; j<3; j++) {
			p[j] = q[j];
			t[j] = r[j];
		}
	}
	if(lc == lk)	return;

	/* top plane */
	for(i=0; i<3; i++) {
		p[i]=vt[i] + *(sp+i+12) + *(sp+i+15);
		q[i]=vt[i] + *(sp+i+15) - *(sp+i+12);
		r[i]=vt[i] - *(sp+i+12) - *(sp+i+15);
		t[i]=vt[i] + *(sp+i+12) - *(sp+i+15);
	}
	tplane( p, q, r, t );

	/* bottom plane */
	for(i=0;i<3;i++){
		p[i] = *(sp+i) + *(sp+i+6) + *(sp+i+9);
		q[i] = *(sp+i) + *(sp+i+9) - *(sp+i+6);
		r[i] = *(sp+i) - *(sp+i+6) - *(sp+i+9);
		t[i] = *(sp+i) + *(sp+i+6) - *(sp+i+9);
	}
	tplane( p, q, r, t );
}


/* convert ellg to an arbn */
static void
ellin()
{
	static fastf_t p[3], q[3], r[3], t[3];
	static fastf_t s1,s2,sa,c1,c2,ca,sd=.5,cd=.8660254,sgn;
	static fastf_t se, ce;
	register int i,j,ih,ie;

	sgn = -1.;
	for(i=0;i<3;i++) pcenter[i] = *(sp+i);
	for(ih=0;ih<2;ih++){
		c2=1.;
		s2=0.;

		/* find first point */
		for(ie=0;ie<3;ie++){
			s1=0.;
			c1=1.;
			se=s2*cd+c2*sd;
			ce=c2*cd-s2*sd;
			for(i=0;i<3;i++) {
				p[i] = *(sp+i) + (c2 * (*(sp+i+3)))
				       + (sgn * s2 * (*(sp+i+9)));
				t[i] = *(sp+i) + (ce * (*(sp+i+3)))
				       + (sgn * se * (*(sp+i+9)));
			}
			for(i=0;i<12;i++){
				sa=s1*cd+c1*sd;
				ca=c1*cd-s1*sd;
				for(j=0;j<3;j++){
					q[j] = *(sp+j) + (ca * c2 * (*(sp+j+3))) 
						+ (sa * c2 * (*(sp+j+6))) 
						+ (s2 * sgn * (*(sp+j+9)));
					r[j] = *(sp+j) + (c1 * ce* (*(sp+j+3))) 
						+ (s1 * ce * (*(sp+j+6))) 
						+ (se * sgn * (*(sp+j+9)));
				}
				tplane( p, q, r, t );
				s1=sa;
				c1=ca;
				for(j=0; j<3; j++) {
					p[j] = q[j];
					t[j] = r[j];
				}
			}
			c2=ce;
			s2=se;
		}
		sgn = -sgn;
	}
}


/* process region into planes */
static void
regin(flag, lmemb, umemb)
int flag;	/* 1 if only calculating min,maxs   NO PLANE EQUATIONS */
{
	register int i,j;

	tol=reg_min[0]=reg_min[1]=reg_min[2] = -pinf;
	reg_max[0]=reg_max[1]=reg_max[2]=pinf;
	sp = savesp;

	/* find min max's */
	for(i=lmemb;i<umemb;i++){
		solin(i);
		if(m_op[i] != '-') {
			for(j=0;j<3;j++){
				V_MAX(reg_min[j],sol_min[j]);
				V_MIN(reg_max[j],sol_max[j]);
			}
		}
	}
	for(i=0;i<3;i++){
		V_MAX(tol,fabs(reg_min[i]));
		V_MAX(tol,fabs(reg_max[i]));
	}
	tol=tol*0.00001;

	if(flag == 0 ) {
		/* find planes for each solid */
		sp = savesp;
		solpl(lmemb,umemb);
	}

	/* save the parameter pointer in case region has ORs */
	savesp = sp;
}


/* finds enclosing RPP for a solid using the solid's parameters */
static void
solin(num)
int num;
{
	static int amt[19]={0,0,0,12,15,18,21,24,0,0,0,0,0,0,0,0,0,18,12};
	register int *ity,i,j;
	static fastf_t a,b,c,d,v1,v2,vt,vb;

	ity = &m_type[num];
	if(*ity==20) *ity=8;
	if(*ity>19 || amt[*ity-1]==0){
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "solin: Type %d Solid not known\n",*ity);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  return;
	}
	sol_min[0]=sol_min[1]=sol_min[2]=pinf;
	sol_max[0]=sol_max[1]=sol_max[2] = -pinf;

	/* ARB4,5,6,7,8 */
	if(*ity<18){
		for(i=0;i<*ity;i++){
			for(j=0;j<3;j++){
				V_MIN(sol_min[j],*sp);
				V_MAX(sol_max[j],*sp);
				sp++;
			}
		}
	}

	/* TGC */
	if(*ity==18){
		for(i=0;i<3;i++,sp++){
			vt = *sp + *(sp+3);
			a = *(sp+6);
			b = *(sp+9);
			c = *(sp+12);
			d = *(sp+15);
			v1=sqrt(a*a+b*b);
			v2=sqrt(c*c+d*d);
			V_MIN(sol_min[i],*(sp)-v1);
			V_MIN(sol_min[i],vt-v2);
			V_MAX(sol_max[i],*(sp)+v1);
			V_MAX(sol_max[i],vt+v2);			
		}
		sp+=15;
	}

	if(*ity > 18) {
		/* ELLG */
		for(i=0;i<3;i++,sp++){
			vb = *sp - *(sp+3);
			vt = *sp + *(sp+3);
			a = *(sp+6);
			b = *(sp+9);
			v1=sqrt(a*a+b*b);
			v2=sqrt(c*c+d*d);
			V_MIN(sol_min[i],vb-v1);
			V_MIN(sol_min[i],vt-v2);
			V_MAX(sol_max[i],vb+v1);
			V_MAX(sol_max[i],vt+v2);
		}
		sp+=9;
	}

	/* save min,maxs for this solid */
	for(i=0; i<3; i++) {
		solrpp[num*6+i] = sol_min[i];
		solrpp[num*6+i+3] = sol_max[i];
	}
}


/* converts all solids to ARBNs */
/* Called only from regin() */
static void
solpl(lmemb,umemb)
{
	static fastf_t tt;
	static int ls, n4;
	static fastf_t *pp,*p1,*p2,*p3,*p4;
	register int i,j;
	static int nfc[5]={4,5,5,6,6};
	static int iv[5*24]={
		 0,1,2,0, 3,0,1,0, 3,1,2,1, 3,2,0,0, 0,0,0,0, 0,0,0,0,
		 0,1,2,3, 4,0,1,0, 4,1,2,1, 4,2,3,2, 4,3,0,0, 0,0,0,0,
		 0,1,2,3, 1,2,5,4, 0,4,5,3, 4,0,1,0, 5,2,3,2, 0,0,0,0,
		 0,1,2,3, 4,5,6,4, 0,3,4,0, 1,2,6,5, 0,1,5,4, 3,2,6,4,
		 0,1,2,3, 4,5,6,7, 0,4,7,3, 1,2,6,5, 0,1,5,4, 3,2,6,7};

	lc=0;
	tt=tol*10.;
	for(id=lmemb;id<umemb;id++){
		ls=lc;
		if(m_type[id]<18){

			/* ARB 4,5,6,7,8 */
			n4=m_type[id]-4;
			center();
			j=n4*24;
			for(i=0;i<nfc[n4];i++){
				p1=sp+iv[j++]*3;
				p2=sp+iv[j++]*3;
				p3=sp+iv[j++]*3;
				p4=sp+iv[j++]*3;
				tplane(p1,p2,p3,p4);
			}
			sp+=m_type[id]*3;
		}
		if(m_type[id]==18){
			tgcin();
			sp+=18;
		}
		if(m_type[id]==19){
			ellin();
			sp+=12;
		}

		if(m_op[id]=='-'){
			pp = &peq[4*ls]+3;
			while(ls++ < lc) {
				*pp-=tt;
				pp+=4;
			}
		}

		mlc[id]=lc-1;
	}
}
