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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

#include "../librt/debug.h"	/* XXX */

extern int	atoi();
extern long	time();

extern long	nvectors;	/* from dodraw.c */

int		drawreg;	/* if > 0, process and draw regions */
extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

static void	eedit();
void	f_zap();

/* DEBUG -- force view center */
/* Format: C x y z	*/
void
f_center()
{
	/* must convert from the local unit to the base unit */
	toViewcenter[MDX] = -atof( cmd_args[1] ) * local2base;
	toViewcenter[MDY] = -atof( cmd_args[2] ) * local2base;
	toViewcenter[MDZ] = -atof( cmd_args[3] ) * local2base;
	new_mats();
	dmaflag++;
}

void
f_vrot()
{
	/* Actually, it would be nice if this worked all the time */
	/* usejoy isn't quite the right thing */
	if( not_state( ST_VIEW, "View Rotate") )
		return;

	usejoy(	atof(cmd_args[1]) * degtorad,
		atof(cmd_args[2]) * degtorad,
		atof(cmd_args[3]) * degtorad );
}

/* DEBUG -- force viewsize */
/* Format: view size	*/
void
f_view()
{
	fastf_t f;
	f = atof( cmd_args[1] );
	if( f < 0.0001 ) f = 0.0001;
	Viewscale = f * 0.5 * local2base;
	new_mats();
	dmaflag++;
}

/* ZAP the display -- then edit anew */
/* Format: B object	*/
void
f_blast()
{

	f_zap();

	if( dmp->dmr_displaylist )  {
		/*
		 * Force out the control list with NO solids being drawn,
		 * then the display processor will not mind when we start
		 * writing new subroutines out there...
		 */
		refresh();
	}

	drawreg = 0;
	regmemb = -1;
	eedit();
}

/* Edit something (add to visible display) */
/* Format: e object	*/
void
f_edit()
{
	drawreg = 0;
	regmemb = -1;
	eedit();
}

/*
 *			F _ E V E D I T
 *
 *  The "Big E" command.
 *  Evaluated Edit something (add to visible display)
 *  Usage: E object(s)
 */
void
f_evedit()
{
	drawreg = 1;
	regmemb = -1;
	eedit();
}

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
		VSETALL( radial, 1 );
	} else {
		VADD2SCALE( center, max, min, 0.5 );
		VSUB2( radial, max, center );
	}
	mat_idn( toViewcenter );
	MAT_DELTAS( toViewcenter, -center[X], -center[Y], -center[Z] );
	Viewscale = radial[X];
	MAX( Viewscale, radial[Y] );
	MAX( Viewscale, radial[Z] );
}

/*
 *			E E D I T
 *
 * B, e, and E commands uses this area as common
 */
static void
eedit()
{
	register struct directory *dp;
	register int	i;
	long		stime, etime;	/* start & end times */
	static int	first_time = 1;

	nvectors = 0;
	(void)time( &stime );
	for( i=1; i < numargs; i++ )  {
		if( (dp = db_lookup( dbip,  cmd_args[i], LOOKUP_NOISY )) == DIR_NULL )
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
			(void)printf("No memory left so cannot draw %s\n",
				dp->d_namep);
			drawreg = 0;
			regmemb = -1;
			continue;
		}

		drawtree( dp );
		regmemb = -1;
	}
	(void)time( &etime );
	if( first_time && HeadSolid.s_forw != &HeadSolid)  {
		first_time = 0;
		size_reset();
		new_mats();
	}

	(void)printf("%ld vectors in %ld sec\n", nvectors, etime - stime );
	dmp->dmr_colorchange();
	dmaflag = 1;
}

/* Delete an object or several objects from the display */
/* Format: d object1 object2 .... objectn */
void
f_delobj()
{
	register struct directory *dp;
	register int i;

	for( i = 1; i < numargs; i++ )  {
		if( (dp = db_lookup( dbip,  cmd_args[i], LOOKUP_NOISY )) != DIR_NULL )
			eraseobj( dp );
	}
	no_memory = 0;
	dmaflag = 1;
}

void
f_debug()
{
	(void)signal( SIGINT, sig2 );	/* allow interupts */
	pr_schain( &HeadSolid );
}

void
f_regdebug()
{
	static int regdebug = 0;

	if( numargs <= 1 )
		regdebug = !regdebug;	/* toggle */
	else
		regdebug = atoi( cmd_args[1] );
	(void)printf("regdebug=%d\n", regdebug);
	dmp->dmr_debug(regdebug);
}

void
f_debuglib()
{
	if( numargs >= 2 )  {
		sscanf( cmd_args[1], "%x", &rt_g.debug );
	}
	rt_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
	rt_log("\n");
}

void
do_list( dp )
register struct directory *dp;
{
	register int	i;
	register union record	*rp;

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		return;

	if( rp[0].u_id == ID_SOLID )  {
		switch( rp[0].s.s_type )  {
		case GENARB8:
			dbpr_arb( &rp[0].s, dp );
			break;
		case GENTGC:
			dbpr_tgc( &rp[0].s, dp );
			break;
		case GENELL:
			dbpr_ell( &rp[0].s, dp );
			break;
		case HALFSPACE:
			dbpr_half( &rp[0].s, dp );
			break;
		case TOR:
			dbpr_torus( &rp[0].s, dp );
			break;
		default:
			printf("bad solid type %d\n", rp[0].s.s_type );
			break;
		}

		/* This stuff ought to get pushed into the dbpr_xx code */
		pr_solid( &rp[0].s );

		for( i=0; i < es_nlines; i++ )
			(void)printf("%s\n",&es_display[ES_LINELEN*i]);

		/* If in solid edit, re-compute solid params */
		if(state == ST_S_EDIT)
			pr_solid(&es_rec.s);

		goto out;
	}

	if( rp[0].u_id == ID_ARS_A )  {
		(void)printf("%s:  ARS\n", dp->d_namep );
		(void)printf(" num curves  %d\n", rp[0].a.a_m );
		(void)printf(" pts/curve   %d\n", rp[0].a.a_n );
		/* convert vertex from base unit to the local unit */
		(void)printf(" vertex      %.4f %.4f %.4f\n",
			rp[1].b.b_values[0]*base2local,
			rp[1].b.b_values[1]*base2local,
			rp[1].b.b_values[2]*base2local );
		goto out;
	}
	if( rp[0].u_id == ID_BSOLID ) {
		dbpr_spline( dp );
		goto out;
	}
	if( rp[0].u_id == ID_P_HEAD )  {
		(void)printf("%s:  %d granules of polygon data\n",
			dp->d_namep, dp->d_len-1 );
		goto out;
	}
	if( rp[0].u_id != ID_COMB )  {
		(void)printf("%s: unknown record type!\n",
			dp->d_namep );
		goto out;
	}

	/* Combination */
	(void)printf("%s (len %d) ", dp->d_namep, dp->d_len-1 );
	if( rp[0].c.c_flags == 'R' )
		(void)printf("REGION id=%d  (air=%d, los=%d, GIFTmater=%d) ",
			rp[0].c.c_regionid,
			rp[0].c.c_aircode,
			rp[0].c.c_los,
			rp[0].c.c_material );
	(void)printf("--\n");
	if( rp[0].c.c_matname[0] )
		(void)printf("Material '%s' '%s'\n",
			rp[0].c.c_matname,
			rp[0].c.c_matparm);
	if( rp[0].c.c_override == 1 )
		(void)printf("Color %d %d %d\n",
			rp[0].c.c_rgb[0],
			rp[0].c.c_rgb[1],
			rp[0].c.c_rgb[2]);
	if( rp[0].c.c_matname[0] || rp[0].c.c_override )  {
		if( rp[0].c.c_inherit == DB_INH_HIGHER )
			(void)printf("(These material properties override all lower ones in the tree)\n");
	}

	for( i=1; i < dp->d_len; i++ )  {
		mat_t	xmat;

		rt_mat_dbmat( xmat, rp[i].M.m_mat );

		(void)printf("  %c %s",
			rp[i].M.m_relation, rp[i].M.m_instname );

		if( xmat[0] != 1.0 || xmat[5] != 1.0 || xmat[10] != 1.0 )
			(void)printf(" (Rotated)");
		if( xmat[MDX] != 0.0 ||
		    xmat[MDY] != 0.0 ||
		    xmat[MDZ] != 0.0 )
			(void)printf(" [%f,%f,%f]",
				xmat[MDX]*base2local,
				xmat[MDY]*base2local,
				xmat[MDZ]*base2local);
		if( xmat[12] != 0.0 ||
		    xmat[13] != 0.0 ||
		    xmat[14] != 0.0 )
			(void)printf(" ??Perspective=[%f,%f,%f]??",
				xmat[12], xmat[13], xmat[14] );
		(void)putchar('\n');
	}
out:
	rt_free( (char *)rp, "do_list records");
}

/* List object information */
/* Format: l object	*/
void
f_list()
{
	register struct directory *dp;
	register int arg;
	
	(void)signal( SIGINT, sig2 );	/* allow interupts */
	for( arg = 1; arg < numargs; arg++ )  {
		if( (dp = db_lookup( dbip,  cmd_args[arg], LOOKUP_NOISY )) == DIR_NULL )
			continue;

		do_list( dp );
	}
}

/* ZAP the display -- everything dropped */
/* Format: Z	*/
void
f_zap()
{
	register struct solid *sp;
	register struct solid *nsp;

	no_memory = 0;

	/* FIRST, reject any editing in progress */
	if( state != ST_VIEW )
		button( BE_REJECT );

	sp=HeadSolid.s_forw;
	while( sp != &HeadSolid )  {
		memfree( &(dmp->dmr_map), sp->s_bytes, (unsigned long)sp->s_addr );
		sp->s_addr = sp->s_bytes = 0;
		nsp = sp->s_forw;
		DEQUEUE_SOLID( sp );
		FREE_SOLID( sp );
		sp = nsp;
	}
	(void)chg_state( state, state, "zap" );
	dmaflag = 1;
}

void
f_status()
{
	(void)printf("STATE=%s, ", state_str[state] );
	(void)printf("Viewscale=%f (%f mm)\n", Viewscale*base2local, Viewscale);
	(void)printf("base2local=%f\n", base2local);
	mat_print("toViewcenter", toViewcenter);
	mat_print("Viewrot", Viewrot);
	mat_print("model2view", model2view);
	mat_print("view2model", view2model);
	if( state != ST_VIEW )  {
		mat_print("model2objview", model2objview);
		mat_print("objview2model", objview2model);
	}
}

/* Fix the display processor after a hardware error by re-attaching */
void
f_fix()
{
	attach( dmp->dmr_name );	/* reattach */
	dmaflag = 1;		/* causes refresh() */
}

void
f_refresh()
{
	dmaflag = 1;		/* causes refresh() */
}

/* set view using azimuth and elevation angles */
void
f_aeview()
{
	setview( 270 + atoi(cmd_args[2]), 0, 270 - atoi(cmd_args[1]) );
}

void
f_attach()
{
	if (numargs == 1)
		get_attached();
	else
		attach( cmd_args[1] );
}

void
f_release()
{
	release();
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

	sp=HeadSolid.s_forw;
	while( sp != &HeadSolid )  {
		nsp = sp->s_forw;
		for( i=0; i<=sp->s_last; i++ )  {
			if( sp->s_path[i] == dp )  {
				if( state != ST_VIEW && illump == sp )
					button( BE_REJECT );
				dmp->dmr_viewchange( DM_CHGV_DEL, sp );
				memfree( &(dmp->dmr_map), sp->s_bytes, (unsigned long)sp->s_addr );
				DEQUEUE_SOLID( sp );
				FREE_SOLID( sp );
				break;
			}
		}
		sp = nsp;
	}
}

/*
 *			P R _ S C H A I N
 *
 *  Given a pointer to a member of the circularly linked list of solids
 *  (typically the head), chase the list and print out the information
 *  about each solid structure.
 */
void
pr_schain( startp )
struct solid *startp;
{
	register struct solid *sp;
	register int i;

	sp = startp->s_forw;
	while( sp != startp )  {
		(void)printf( sp->s_flag == UP ? "VIEW ":"-no- " );
		for( i=0; i <= sp->s_last; i++ )
			(void)printf("/%s", sp->s_path[i]->d_namep);
		if( sp->s_iflag == UP )
			(void)printf(" ILLUM");
		(void)printf("\n");
		/* convert to the local unit for printing */
		(void)printf("  (%.3f,%.3f,%.3f) sz=%g ",
			sp->s_center[X]*base2local,
			sp->s_center[Y]*base2local, 
			sp->s_center[Z]*base2local,
			sp->s_size*base2local );
		(void)printf("reg=%d",sp->s_regionid );
		(void)printf(" (%d,%d,%d) %d,%d,%d i=%d\n",
			sp->s_basecolor[0],
			sp->s_basecolor[1],
			sp->s_basecolor[2],
			sp->s_color[0],
			sp->s_color[1],
			sp->s_color[2],
			sp->s_dmindex );
		sp = sp->s_forw;
	}
}

/* Illuminate the named object */
/* TODO:  allow path specification on cmd line */
void
f_ill()
{
	register struct directory *dp;
	register struct solid *sp;
	struct solid *lastfound;
	register int i;
	int nmatch;

	if( (dp = db_lookup( dbip,  cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;
	if( state != ST_O_PICK && state != ST_S_PICK )  {
		state_err("keyboard illuminate pick");
		return;
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
		(void)printf("%s not being displayed\n", cmd_args[1]);
		return;
	}
	if( nmatch > 1 )  {
		(void)printf("%s multiply referenced\n", cmd_args[1]);
		return;
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
}

/* Simulate pressing "Solid Edit" and doing an ILLuminate command */
void
f_sed()
{
	if( not_state( ST_VIEW, "keyboard solid edit start") )
		return;

	button(BE_S_ILLUMINATE);	/* To ST_S_PICK */
	f_ill();		/* Illuminate named solid --> ST_S_EDIT */
}

/* Simulate a knob twist.  "knob id val" */
void
f_knob()
{
	fastf_t f;

	if(numargs == 2)
		f = 0;
	else {
		f = atof(cmd_args[2]);
		if( f < -1.0 )
			f = -1.0;
		else if( f > 1.0 )
			f = 1.0;
	}
	switch( cmd_args[1][0] )  {
	case 'x':
		dm_values.dv_xjoy = f;
		break;
	case 'y':
		dm_values.dv_yjoy = f;
		break;
	case 'z':
		dm_values.dv_zjoy = f;
		break;
	case 'X':
		dm_values.dv_xslew = f;
		break;
	case 'Y':
		dm_values.dv_yslew = f;
		break;
	case 'Z':
		dm_values.dv_zslew = f;
		break;
	case 'S':
		dm_values.dv_zoom = f;
		break;
	default:
		(void)printf("x,y,z for rotation, S for scale, X,Y,Z for slew\n");
		return;
	}
}
