/*
 *			C H G V I E W . C
 *
 * Functions -
 *	f_center	(DEBUG) force view center
 *	f_vrot		(DEBUG) force view rotation
 *	f_view		(DEBUG) force view size
 *	f_blast		zap the display, then edit anew
 *	f_edit		edit something (add to visible display)
 *	f_evedit	Evaluated edit something (add to visible display)
 *	f_delobj	delete an object or several from the display
 *	f_debug		(DEBUG) print solid info?
 *	f_regdebug	toggle debugging state
 *	f_list		list object information
 *	f_zap		zap the display -- everything dropped
 *	f_status	print view info
 *	f_fix		fix display processor after hardware error
 *	f_refresh	request display refresh
 *	f_attach	attach display device
 *	f_release	release display device
 *	eraseobj	Drop an object from the visible list
 *	pr_schain	Print info about visible list
 *	f_ill		illuminate the named object
 *	f_sed		simulate pressing "solid edit" then illuminate
 *	f_knob		simulate knob twist
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "rtstring.h"
#include "raytrace.h"
#include "nmg.h"
#include "externs.h"
#include "./sedit.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

#include "../librt/debug.h"	/* XXX */

extern long	nvectors;	/* from dodraw.c */

double		mged_abs_tol;
double		mged_rel_tol = 0.01;		/* 1%, by default */
double		mged_nrm_tol;			/* normal ang tol, radians */

int		rateflag_slew;
vect_t		rate_slew;
int		rateflag_rotate;
vect_t		rate_rotate;
vect_t		absolute_rotate;	/* selected by mged_variables.rateknobs */
int		rateflag_zoom;
fastf_t		rate_zoom;

RT_EXTERN(int	edit_com, (int argc, char **argv, int kind, int catch_sigint));
int		f_zap();



/* Delete an object or several objects from the display */
/* Format: d object1 object2 .... objectn */
int
f_delobj(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;

	for( i = 1; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) != DIR_NULL )
			eraseobj( dp );
	}
	no_memory = 0;
	dmaflag = 1;

	return CMD_OK;
}

/* DEBUG -- force view center */
/* Format: C x y z	*/
int
f_center(argc, argv)
int	argc;
char	**argv;
{
	/* must convert from the local unit to the base unit */
	toViewcenter[MDX] = -atof( argv[1] ) * local2base;
	toViewcenter[MDY] = -atof( argv[2] ) * local2base;
	toViewcenter[MDZ] = -atof( argv[3] ) * local2base;
	new_mats();
	dmaflag++;

	return CMD_OK;
}

int
f_vrot(argc, argv)
int	argc;
char	**argv;
{
	/* Actually, it would be nice if this worked all the time */
	/* usejoy isn't quite the right thing */
	if( not_state( ST_VIEW, "View Rotate") )
		return CMD_BAD;

	usejoy(	atof(argv[1]) * degtorad,
		atof(argv[2]) * degtorad,
		atof(argv[3]) * degtorad );

	return CMD_OK;
}

/* DEBUG -- force viewsize */
/* Format: view size	*/
int
f_view(argc, argv)
int	argc;
char	**argv;
{
	fastf_t f;
	f = atof( argv[1] );
	if( f < 0.0001 ) f = 0.0001;
	Viewscale = f * 0.5 * local2base;
	new_mats();
	dmaflag++;

	return CMD_OK;
}

/* ZAP the display -- then edit anew */
/* Format: B object	*/
int
f_blast(argc, argv)
int	argc;
char	**argv;
{

	if (f_zap(argc, argv) == CMD_BAD) return CMD_BAD;

	if( dmp->dmr_displaylist )  {
		/*
		 * Force out the control list with NO solids being drawn,
		 * then the display processor will not mind when we start
		 * writing new subroutines out there...
		 */
		refresh();
	}

	return edit_com( argc, argv, 1, 1 );
}

/* Edit something (add to visible display) */
/* Format: e object	*/
int
f_edit(argc, argv)
int	argc;
char	**argv;
{
	return edit_com( argc, argv, 1, 1 );
}

/* Format: ev objects	*/
int
f_ev(argc, argv)
int	argc;
char	**argv;
{
	return edit_com( argc, argv, 3, 1 );
}

#if 0
/* XXX until NMG support is complete,
 * XXX the old Big-E command is retained in all its glory in
 * XXX the file proc_reg.c, including the command processing.
 */
/*
 *			F _ E V E D I T
 *
 *  The "Big E" command.
 *  Evaluated Edit something (add to visible display)
 *  Usage: E object(s)
 */
int
f_evedit(argc, argv)
int	argc;
char	**argv;
{
	return edit_com( argc, argv, 2, 1 );
}
#endif

/*
 *			S I Z E _ R E S E T
 *
 *  Reset view size and view center so that all solids in the solid table
 *  are in view.
 *  Caller is responsible for calling new_mats().
 */
void
size_reset()
{
	register struct solid	*sp;
	vect_t		min, max;
	vect_t		minus, plus;
	vect_t		center;
	vect_t		radial;

	VSETALL( min,  INFINITY );
	VSETALL( max, -INFINITY );

	FOR_ALL_SOLIDS( sp )  {
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN( min, minus );
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX( max, plus );
	}

	if( HeadSolid.s_forw == &HeadSolid )  {
		/* Nothing is in view */
		VSETALL( center, 0 );
		VSETALL( radial, 1000 );	/* 1 meter */
	} else {
		VADD2SCALE( center, max, min, 0.5 );
		VSUB2( radial, max, center );
	}

	if( VNEAR_ZERO( radial , SQRT_SMALL_FASTF ) )
		VSETALL( radial , 1.0 );

	mat_idn( toViewcenter );
	MAT_DELTAS( toViewcenter, -center[X], -center[Y], -center[Z] );
	Viewscale = radial[X];
	V_MAX( Viewscale, radial[Y] );
	V_MAX( Viewscale, radial[Z] );
}

/*
 *			E D I T _ C O M
 *
 * B, e, and E commands uses this area as common
 */
int
edit_com(argc, argv, kind, catch_sigint)
int	argc;
char	**argv;
int	kind;
int	catch_sigint;
{
	register struct directory *dp;
	register int	i;
	double		elapsed_time;
	int		initial_blank_screen;

	initial_blank_screen = (HeadSolid.s_forw == &HeadSolid);

	/*  First, delete any mention of these objects.
	 *  Silently skip any leading options (which start with minus signs).
	 */
	for( i = 1; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], LOOKUP_QUIET )) != DIR_NULL )  {
			eraseobj( dp );
			no_memory = 0;
		}
	}

	dmaflag = 1;
	if( dmp->dmr_displaylist )  {
		/* Force displaylist update before starting new drawing */
		dmaflag = 1;
		refresh();
	}

	if( catch_sigint )
		(void)signal( SIGINT, sig2 );	/* allow interupts after here */

	nvectors = 0;
	rt_prep_timer();
	drawtrees( argc, argv, kind );
	(void)rt_get_timer( (struct rt_vls *)0, &elapsed_time );
	rt_log("%ld vectors in %g sec\n", nvectors, elapsed_time );
	
	/* If we went from blank screen to non-blank, resize */
	if (mged_variables.autosize  && initial_blank_screen &&
	    HeadSolid.s_forw != &HeadSolid)  {
		size_reset();
		new_mats();
	}

	dmp->dmr_colorchange();
	dmaflag = 1;

	return CMD_OK;
}

int
f_debug( argc, argv )
int	argc;
char	**argv;
{
	int	lvl = 0;

	if( argc > 1 )  lvl = atoi(argv[1]);

	rt_log("ndrawn=%d\n", ndrawn);
	(void)signal( SIGINT, sig2 );	/* allow interupts */
	pr_schain( &HeadSolid, lvl );

	return CMD_OK;
}

int
f_regdebug(argc, argv)
int	argc;
char	**argv;
{
	static int regdebug = 0;

	if( argc <= 1 )
		regdebug = !regdebug;	/* toggle */
	else
		regdebug = atoi( argv[1] );
	rt_log("regdebug=%d\n", regdebug);
	dmp->dmr_debug(regdebug);

	return CMD_OK;
}

int
f_debuglib(argc, argv)
int	argc;
char	**argv;
{
	if( argc >= 2 )  {
		sscanf( argv[1], "%x", &rt_g.debug );
	} else {
		rt_printb( "Possible flags", 0xffffffffL, DEBUG_FORMAT );
		rt_log("\n");
	}
	rt_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
	rt_log("\n");

	return CMD_OK;
}

int
f_debugmem( argc, argv )
int	argc;
char	**argv;
{
	(void)signal( SIGINT, sig2 );	/* allow interupts */
	rt_prmem("Invoked via MGED command");
	return CMD_OK;
}

int
f_debugnmg(argc, argv)
int	argc;
char	**argv;
{
	if( argc >= 2 )  {
		sscanf( argv[1], "%x", &rt_g.NMG_debug );
	} else {
		rt_printb( "possible flags", 0xffffffffL, NMG_DEBUG_FORMAT );
		rt_log("\n");
	}
	rt_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
	rt_log("\n");

	return CMD_OK;
}

/*
 *			D O _ L I S T
 */
void
do_list( outstrp, dp, verbose )
struct rt_vls	*outstrp;
register struct directory *dp;
int	verbose;
{
	register int	i;
	register union record	*rp;
	int			id;
	struct rt_external	ext;
	struct rt_db_internal	intern;
	mat_t			ident;
	struct rt_vls		str;
	rt_vls_init( &str );

	rt_vls_printf( outstrp, "%s:  ", dp->d_namep );
	RT_INIT_EXTERNAL(&ext);
	if( db_get_external( &ext, dp, dbip ) < 0 )  {
		rt_log("db_get_external(%s) failure\n", dp->d_namep);
		return;
	}
	rp = (union record *)ext.ext_buf;

	/* XXX This should be converted to _import and _describe routines! */
	if( rp[0].u_id == ID_COMB )  {
		/* Combination */
		rt_vls_printf( outstrp, "%s (len %d) ", dp->d_namep, 
			       dp->d_len-1 );
		if( rp[0].c.c_flags == 'R' ) {
			rt_vls_printf( outstrp,
			       "REGION id=%d  (air=%d, los=%d, GIFTmater=%d) ",
				rp[0].c.c_regionid,
				rp[0].c.c_aircode,
				rp[0].c.c_los,
				rp[0].c.c_material );
		}
		rt_vls_strcat( outstrp, "--\n" );
		if( rp[0].c.c_matname[0] ) {
			rt_vls_printf( outstrp,
				"Material '%s' '%s'\n",
				rp[0].c.c_matname,
				rp[0].c.c_matparm);
		}
		if( rp[0].c.c_override == 1 ) {
			rt_vls_printf( outstrp,
				"Color %d %d %d\n",
				rp[0].c.c_rgb[0],
				rp[0].c.c_rgb[1],
				rp[0].c.c_rgb[2]);
		}
		if( rp[0].c.c_matname[0] || rp[0].c.c_override )  {
			if( rp[0].c.c_inherit == DB_INH_HIGHER ) {
				rt_vls_strcat( outstrp, 
	"(These material properties override all lower ones in the tree)\n");
			}
		}

		for( i=1; i < dp->d_len; i++ )  {
			mat_t	xmat;
			int	status;

			status = 0;
#define STAT_ROT	1
#define STAT_XLATE	2
#define STAT_PERSP	4
#define STAT_SCALE	8

			/* See if this matrix does anything */
			rt_mat_dbmat( xmat, rp[i].M.m_mat );

			if( xmat[0] != 1.0 || xmat[5] != 1.0 
						|| xmat[10] != 1.0 )
				status |= STAT_ROT;

			if( xmat[MDX] != 0.0 ||
			    xmat[MDY] != 0.0 ||
			    xmat[MDZ] != 0.0 )
				status |= STAT_XLATE;

			if( xmat[12] != 0.0 ||
			    xmat[13] != 0.0 ||
			    xmat[14] != 0.0 )
				status |= STAT_PERSP;

			if( xmat[15] != 1.0 )  status |= STAT_SCALE;

			if( verbose )  {
				rt_vls_printf( outstrp, "  %c %s",
					rp[i].M.m_relation, 
					rp[i].M.m_instname );
				if( status & STAT_ROT ) {
					fastf_t	az, el;
					ae_vec( &az, &el, xmat );
					rt_vls_printf( outstrp, 
						" az=%g, el=%g, ",
						az, el );
				}
				if( status & STAT_XLATE ) {
					rt_vls_printf( outstrp, " [%g,%g,%g]",
						xmat[MDX]*base2local,
						xmat[MDY]*base2local,
						xmat[MDZ]*base2local);
				}
				if( status & STAT_SCALE ) {
					rt_vls_printf( outstrp, " scale %g",
						1.0/xmat[15] );
				}
				if( status & STAT_PERSP ) {
					rt_vls_printf( outstrp, 
						" ??Perspective=[%g,%g,%g]??",
						xmat[12], xmat[13], xmat[14] );
				}
				rt_vls_printf( outstrp, "\n" );
			} else {
				register char	*cp;

				rt_vls_trunc( &str, 0 );
				rt_vls_printf( &str, "%c %s",
					rp[i].M.m_relation, 
					rp[i].M.m_instname );

				cp = rt_vls_addr( &str );
				if( status )  {
					cp += strlen(cp);
					*cp++ = '/';
					if( status & STAT_ROT )  *cp++ = 'R';
					if( status & STAT_XLATE) *cp++ = 'T';
					if( status & STAT_SCALE) *cp++ = 'S';
					if( status & STAT_PERSP) *cp++ = 'P';
					*cp = '\0';
				}
				col_item( rt_vls_addr(&str) );
				rt_vls_trunc( &str, 0 );
			}
		}
		if( !verbose )  col_eol();
		goto out;
	}

	id = rt_id_solid( &ext );
	mat_idn( ident );
	if( rt_functab[id].ft_import( &intern, &ext, ident ) < 0 )  {
		rt_log("%s: database import error\n", dp->d_namep);
		goto out;
	}

	if( rt_functab[id].ft_describe( &str, &intern,
	    verbose, base2local ) < 0 )
		rt_log("%s: describe error\n", dp->d_namep);
	rt_functab[id].ft_ifree( &intern );
	rt_vls_vlscat( outstrp, &str );

out:
	db_free_external( &ext );

	rt_vls_free( &str );
}

/* List object information, verbose */
/* Format: l object	*/
int
f_list(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int arg;
	struct rt_vls str;


	rt_vls_init( &str );
	
	(void)signal( SIGINT, sig2 );	/* allow interupts */
	for( arg = 1; arg < argc; arg++ )  {
		if( (dp = db_lookup( dbip, argv[arg], LOOKUP_NOISY )) == DIR_NULL )
			continue;

		rt_vls_trunc( &str, 0 );
		do_list( &str, dp, 99 );	/* very verbose */
		rt_log( rt_vls_addr(&str) );
	}

	rt_vls_free( &str );

	return CMD_OK;
}

/* List object information, briefly */
/* Format: cat object	*/
int
f_cat( argc, argv )
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int arg;
	struct rt_vls str;

	rt_vls_init( &str );
	
	(void)signal( SIGINT, sig2 );	/* allow interupts */
	for( arg = 1; arg < argc; arg++ )  {
		if( (dp = db_lookup( dbip, argv[arg], LOOKUP_NOISY )) == DIR_NULL )
			continue;

		rt_vls_trunc( &str, 0 );
		do_list( &str, dp, 0 );	/* non-verbose */
		rt_log( rt_vls_addr(&str) );
	}

	rt_vls_free( &str );

	return CMD_OK;
}

/*
 *  To return all the free "struct rt_vlist" and "struct solid" items
 *  lurking on their respective freelists, back to rt_malloc().
 *  Primarily as an aid to tracking memory leaks.
 *  WARNING:  This depends on knowledge of the macro GET_SOLID in mged/solid.h
 *  and RT_GET_VLIST in h/raytrace.h.
 */
void
mged_freemem()
{
	register struct solid		*sp;
	register struct rt_vlist	*vp;

	while( (sp = FreeSolid) != SOLID_NULL )  {
		FreeSolid = sp->s_forw;
		rt_free( (char *)sp, "mged_freemem: struct solid" );
	}
	while( RT_LIST_NON_EMPTY( &rt_g.rtg_vlfree ) )  {
		vp = RT_LIST_FIRST( rt_vlist, &rt_g.rtg_vlfree );
		RT_LIST_DEQUEUE( &(vp->l) );
		rt_free( (char *)vp, "mged_freemem: struct rt_vlist" );
	}
}

/* ZAP the display -- everything dropped */
/* Format: Z	*/
int
f_zap(argc, argv)
int	argc;
char	**argv;
{
	register struct solid *sp;
	register struct solid *nsp;
	struct directory	*dp;

	no_memory = 0;

	/* FIRST, reject any editing in progress */
	if( state != ST_VIEW )
		button( BE_REJECT );

	sp=HeadSolid.s_forw;
	while( sp != &HeadSolid )  {
		rt_memfree( &(dmp->dmr_map), sp->s_bytes, (unsigned long)sp->s_addr );
		dp = sp->s_path[0];
		RT_CK_DIR(dp);
		if( dp->d_addr == RT_DIR_PHONY_ADDR )  {
			if( db_dirdelete( dbip, dp ) < 0 )  {
				rt_log("f_zap: db_dirdelete failed\n");
			}
		}
		sp->s_addr = sp->s_bytes = 0;
		nsp = sp->s_forw;
		DEQUEUE_SOLID( sp );
		FREE_SOLID( sp );
		sp = nsp;
	}

	/* Keeping freelists improves performance.  When debugging, give mem back */
	if( rt_g.debug )  mged_freemem();

	(void)chg_state( state, state, "zap" );
	dmaflag = 1;

	return CMD_OK;
}

int
f_status(argc, argv)
int	argc;
char	**argv;
{
	rt_log("STATE=%s, ", state_str[state] );
	rt_log("Viewscale=%f (%f mm)\n", Viewscale*base2local, Viewscale);
	rt_log("base2local=%f\n", base2local);
	mat_print("toViewcenter", toViewcenter);
	mat_print("Viewrot", Viewrot);
	mat_print("model2view", model2view);
	mat_print("view2model", view2model);
	if( state != ST_VIEW )  {
		mat_print("model2objview", model2objview);
		mat_print("objview2model", objview2model);
	}

	return CMD_OK;
}

/* Fix the display processor after a hardware error by re-attaching */
int
f_fix(argc, argv)
int	argc;
char	**argv;
{
	attach( dmp->dmr_name );	/* reattach */
	dmaflag = 1;		/* causes refresh() */
	return CMD_OK;
}

int
f_refresh(argc, argv)
int	argc;
char	**argv;
{
	dmaflag = 1;		/* causes refresh() */
	return CMD_OK;
}

/* set view using azimuth and elevation angles */
int
f_aeview(argc, argv)
int	argc;
char	**argv;
{
	setview( 270.0 + atof(argv[2]), 0.0, 270.0 - atof(argv[1]) );
	return CMD_OK;
}

int
f_attach(argc, argv)
int	argc;
char	**argv;
{
	if (argc == 1)
		get_attached();
	else
		attach( argv[1] );
	return CMD_OK;
}

int
f_release(argc, argv)
int	argc;
char	**argv;
{
	release();
	return CMD_OK;
}

/*
 *			E R A S E O B J
 *
 * This routine goes through the solid table and deletes all displays
 * which contain the specified object in their 'path'
 */
void
eraseobj( dp )
register struct directory *dp;
{
	register struct solid *sp;
	static struct solid *nsp;
	register int i;

	RT_CK_DIR(dp);
	sp=HeadSolid.s_forw;
	while( sp != &HeadSolid )  {
		nsp = sp->s_forw;
		for( i=0; i<=sp->s_last; i++ )  {
			if( sp->s_path[i] != dp )  continue;

			if( state != ST_VIEW && illump == sp )
				button( BE_REJECT );
			dmp->dmr_viewchange( DM_CHGV_DEL, sp );
			rt_memfree( &(dmp->dmr_map), sp->s_bytes, (unsigned long)sp->s_addr );
			DEQUEUE_SOLID( sp );
			FREE_SOLID( sp );
			dmaflag = 1;
			break;
		}
		sp = nsp;
	}
	if( dp->d_addr == RT_DIR_PHONY_ADDR )  {
		if( db_dirdelete( dbip, dp ) < 0 )  {
			rt_log("eraseobj: db_dirdelete failed\n");
		}
	}
}

static char *mged_vl_draw_message[] = {
	"line move ",
	"line draw ",
	"poly start",
	"poly move ",
	"poly draw ",
	"poly end  ",
	"**unknown*"
};

/*
 *			P R _ S C H A I N
 *
 *  Given a pointer to a member of the circularly linked list of solids
 *  (typically the head), chase the list and print out the information
 *  about each solid structure.
 */
void
pr_schain( startp, lvl )
struct solid *startp;
int		lvl;			/* debug level */
{
	register struct solid	*sp;
	register int		i;
	register struct rt_vlist	*vp;
	int			nvlist;
	int			npts;

	for( sp = startp->s_forw; sp != startp; sp = sp->s_forw )  {
		rt_log( sp->s_flag == UP ? "VIEW ":"-no- " );
		for( i=0; i <= sp->s_last; i++ )
			rt_log("/%s", sp->s_path[i]->d_namep);
		if( sp->s_iflag == UP )
			rt_log(" ILLUM");
		rt_log("\n");

		if( lvl <= 0 )  continue;

		/* convert to the local unit for printing */
		rt_log("  cent=(%.3f,%.3f,%.3f) sz=%g ",
			sp->s_center[X]*base2local,
			sp->s_center[Y]*base2local, 
			sp->s_center[Z]*base2local,
			sp->s_size*base2local );
		rt_log("reg=%d\n",sp->s_regionid );
		rt_log("  color=(%d,%d,%d) %d,%d,%d i=%d\n",
			sp->s_basecolor[0],
			sp->s_basecolor[1],
			sp->s_basecolor[2],
			sp->s_color[0],
			sp->s_color[1],
			sp->s_color[2],
			sp->s_dmindex );

		if( lvl <= 1 )  continue;

		/* Print the actual vector list */
		nvlist = 0;
		npts = 0;
		for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
			register int	i;
			register int	nused = vp->nused;
			register int	*cmd = vp->cmd;
			register point_t *pt = vp->pt;

			nvlist++;
			npts += nused;
			if( lvl <= 2 )  continue;

			for( i = 0; i < nused; i++,cmd++,pt++ )  {
				rt_log("  %s (%g, %g, %g)\n",
					mged_vl_draw_message[*cmd],
					V3ARGS( *pt ) );
			}
		}
		rt_log("  %d vlist structures, %d pts\n", nvlist, npts );
	}
}

/* Illuminate the named object */
/* TODO:  allow path specification on cmd line */
int
f_ill(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register struct solid *sp;
	struct solid *lastfound = SOLID_NULL;
	register int i;
	int nmatch;

	if( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
		return CMD_BAD;
	if( state != ST_O_PICK && state != ST_S_PICK )  {
		state_err("keyboard illuminate pick");
		return CMD_BAD;
	}
	nmatch = 0;
	FOR_ALL_SOLIDS( sp )  {
		for( i=0; i<=sp->s_last; i++ )  {
			if( sp->s_path[i] == dp )  {
				lastfound = sp;
				nmatch++;
				break;
			}
		}
		sp->s_iflag = DOWN;
	}
	if( nmatch <= 0 )  {
		rt_log("%s not being displayed\n", argv[1]);
		return CMD_BAD;
	}
	if( nmatch > 1 )  {
		rt_log("%s multiply referenced\n", argv[1]);
		return CMD_BAD;
	}
	/* Make the specified solid the illuminated solid */
	illump = lastfound;
	illump->s_iflag = UP;
	if( state == ST_O_PICK )  {
		ipathpos = 0;
		(void)chg_state( ST_O_PICK, ST_O_PATH, "Keyboard illuminate");
	} else {
		/* Check details, Init menu, set state=ST_S_EDIT */
		init_sedit();
	}
	dmaflag = 1;
	return CMD_OK;
}

/* Simulate pressing "Solid Edit" and doing an ILLuminate command */
int
f_sed(argc, argv)
int	argc;
char	**argv;
{
	if( not_state( ST_VIEW, "keyboard solid edit start") )
		return CMD_BAD;

	button(BE_S_ILLUMINATE);	/* To ST_S_PICK */
	return f_ill(argc, argv);	/* Illuminate named solid --> ST_S_EDIT */
}

void
check_nonzero_rates()
{
	if( rate_rotate[X] != 0.0 ||
	    rate_rotate[Y] != 0.0 ||
	    rate_rotate[Z] != 0.0 )  {
	    	rateflag_rotate = 1;
	} else {
		rateflag_rotate = 0;
	}
	if( rate_slew[X] != 0.0 ||
	    rate_slew[Y] != 0.0 ||
	    rate_slew[Z] != 0.0 )  {
	    	rateflag_slew = 1;
	} else {
		rateflag_slew = 0;
	}
	if( rate_zoom != 0.0 )  {
		rateflag_zoom = 1;

	} else {
		rateflag_zoom = 0;
	}
	dmaflag = 1;	/* values changed so update faceplate */
}

/* Main processing of a knob twist.  "knob id val" */
int
f_knob(argc, argv)
int	argc;
char	**argv;
{
	int	i;
	fastf_t	f;
	char	*cmd = argv[1];

	if(argc == 2)  {
		i = 0;
		f = 0;
	} else {
		i = atoi(argv[2]);
		f = atof(argv[2]);
		if( f < -1.0 )
			f = -1.0;
		else if( f > 1.0 )
			f = 1.0;
	}
	if( cmd[1] == '\0' )  {
		switch( cmd[0] )  {
		case 'x':
			if( mged_variables.rateknobs )  {
				rate_rotate[X] = f;
			} else {
				VSETALL(rate_rotate, 0);
				absolute_rotate[X] = f;
				absview_v( absolute_rotate );
			}
			break;
		case 'y':
			if( mged_variables.rateknobs )  {
				rate_rotate[Y] = f;
			} else {
				VSETALL(rate_rotate, 0);
				absolute_rotate[Y] = f;
				absview_v( absolute_rotate );
			}
			break;
		case 'z':
			if( mged_variables.rateknobs )  {
				rate_rotate[Z] = f;
			} else {
				VSETALL(rate_rotate, 0);
				absolute_rotate[Z] = f;
				absview_v( absolute_rotate );
			}
			break;
		case 'X':
			if( mged_variables.rateknobs )
				rate_slew[X] = f;
			break;
		case 'Y':
			if( mged_variables.rateknobs )
				rate_slew[Y] = f;
			break;
		case 'Z':
			if( mged_variables.rateknobs )
				rate_slew[Z] = f;
			break;
		case 'S':
			if( mged_variables.rateknobs )
				rate_zoom = f;
			break;
		default:
			goto usage;
		}
	} else if( strcmp( cmd, "xadc" ) == 0 )  {
		dm_values.dv_xadc = i;
		dm_values.dv_flagadc = 1;
	} else if( strcmp( cmd, "yadc" ) == 0 )  {
		dm_values.dv_yadc = i;
		dm_values.dv_flagadc = 1;
	} else if( strcmp( cmd, "ang1" ) == 0 )  {
		dm_values.dv_1adc = i;
		dm_values.dv_flagadc = 1;
	} else if( strcmp( cmd, "ang2" ) == 0 )  {
		dm_values.dv_2adc = i;
		dm_values.dv_flagadc = 1;
	} else if( strcmp( cmd, "distadc" ) == 0 )  {
		dm_values.dv_distadc = i;
		dm_values.dv_flagadc = 1;
	} else if( strcmp( cmd, "zap" ) == 0 || strcmp( cmd, "zero" ) == 0 )  {
		char	*av[3];

		VSETALL( rate_rotate, 0 );
		VSETALL( rate_slew, 0 );
		rate_zoom = 0;

		av[0] = "adc";
		av[1] = "reset";
		av[2] = (char *)NULL;
		(void)f_adc( 2, av );
	} else {
usage:
		rt_log("knob: x,y,z for rotation, S for scale, X,Y,Z for slew (rates, range -1..+1)\n");
		rt_log("knob: xadc, yadc, zadc, ang1, ang2, distadc (values, range -2048..+2047)\n");
		rt_log("knob: zero (cancel motion)\n");
	}

	check_nonzero_rates();
	return CMD_OK;
}

/*
 *			F _ T O L
 *
 *  "tol"	displays current settings
 *  "tol abs #"	sets absolute tolerance.  # > 0.0
 *  "tol rel #"	sets relative tolerance.  0.0 < # < 1.0
 *  "tol norm #" sets normal tolerance, in degrees.
 */
int
f_tol( argc, argv )
int	argc;
char	**argv;
{
	double	f;

	if( argc < 3 )  {
		rt_log("Current tolerance settings are:\n");
		if( mged_abs_tol > 0.0 )  {
			rt_log("\tabs %g %s\n",
				mged_abs_tol * base2local,
				rt_units_string(dbip->dbi_local2base) );
		} else {
			rt_log("\tabs None\n");
		}
		if( mged_rel_tol > 0.0 )  {
			rt_log("\trel %g (%g%%)\n",
				mged_rel_tol, mged_rel_tol * 100.0 );
		} else {
			rt_log("\trel None\n");
		}
		if( mged_nrm_tol > 0.0 )  {
			int	deg, min;
			double	sec;

			sec = mged_nrm_tol * rt_radtodeg;
			deg = (int)(sec);
			sec = (sec - (double)deg) * 60;
			min = (int)(sec);
			sec = (sec - (double)min) * 60;

			rt_log("\tnorm %g degrees (%d deg %d min %g sec)\n",
				mged_nrm_tol * rt_radtodeg,
				deg, min, sec );
		} else {
			rt_log("\tnorm None\n");
		}
		return CMD_OK;
	}

	f = atof(argv[2]);
	if( argv[1][0] == 'a' )  {
		/* Absolute tol */
		if( f <= 0.0 )  {
			mged_abs_tol = 0.0;	/* None */
			return CMD_OK;
		}
		mged_abs_tol = f * local2base;
		return CMD_OK;
	}
	if( argv[1][0] == 'r' )  {
		/* Relative */
		if( f < 0.0 || f >= 1.0 )  {
			rt_log("relative tolerance must be between 0 and 1, not changed\n");
			return CMD_BAD;
		}
		/* Note that a value of 0.0 will disable relative tolerance */
		mged_rel_tol = f;
		return CMD_OK;
	}
	if( argv[1][0] == 'n' )  {
		/* Normal tolerance, in degrees */
		if( f < 0.0 || f > 90.0 )  {
			rt_log("Normal tolerance must be in positive degrees, < 90.0\n");
			return CMD_BAD;
		}
		/* Note that a value of 0.0 or 360.0 will disable this tol */
		mged_nrm_tol = f * rt_degtorad;
		return CMD_OK;
	}
	rt_log("Error, tolerance '%s' unknown\n", argv[1] );
	return CMD_BAD;
}

/*
 *			F _ Z O O M
 *
 *  A scale factor of 2 will increase the view size by a factor of 2,
 *  (i.e., a zoom out) which is accomplished by reducing Viewscale in half.
 */
int
f_zoom( argc, argv )
int	argc;
char	**argv;
{
	double	val;

	val = atof(argv[1]);
	if( val < SMALL_FASTF || val > INFINITY )  {
		rt_log("zoom: scale factor out of range\n");
		return CMD_BAD;
	}
	if( Viewscale < SMALL_FASTF || Viewscale > INFINITY )
		return CMD_BAD;

	Viewscale /= val;
	new_mats();
	return CMD_OK;
}

/*
 *			F _ O R I E N T A T I O N
 *
 *  Set current view direction from a quaternion,
 *  such as might be found in a "saveview" script.
 */
int
f_orientation( argc, argv )
int	argc;
char	**argv;
{
	register int	i;
	quat_t		quat;

	for( i=0; i<4; i++ )
		quat[i] = atof( argv[i+1] );
	quat_quat2mat( Viewrot, quat );
	new_mats();
	return CMD_OK;
}

/*
 *			F _ Q V R O T
 *
 *  Set view from direction vector and twist angle
 */
int
f_qvrot( argc, argv )
int	argc;
char	**argv;
{
    double	dx, dy, dz;
    double	az;
    double	el;
    double	theta;

    dx = atof(argv[1]);
    dy = atof(argv[2]);
    dz = atof(argv[3]);

    if (NEAR_ZERO(dy, 0.00001) && NEAR_ZERO(dx, 0.00001))
    {
	if (NEAR_ZERO(dz, 0.00001))
	{
	    rt_log("f_qvrot: (dx, dy, dz) may not be the zero vector\n");
	    return CMD_BAD;
	}
	az = 0.0;
    }
    else
	az = atan2(dy, dx);
    
    el = atan2(dz, sqrt(dx * dx + dy * dy));

    setview( 270.0 + el * radtodeg, 0.0, 270.0 - az * radtodeg );
    theta = atof(argv[4]) * degtorad;
    usejoy(0.0, 0.0, theta);

    return CMD_OK;
}
