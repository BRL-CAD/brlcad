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
 *	f_rt		ray-trace
 *	f_saveview	save the current view parameters
 *	f_attach	attach display device
 *	f_release	release display device
 *	eraseobj	Drop an object from the visible list
 *	pr_solids	Print info about visible list
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

#include	<math.h>
#include	<signal.h>
#include	<stdio.h>
#include "ged_types.h"
#include "../h/db.h"
#include "sedit.h"
#include "ged.h"
#include "objdir.h"
#include "solid.h"
#include "dm.h"
#include "../h/vmath.h"

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();

extern char	*filename;	/* Name of database file */
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
	float f;
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

/* Evaluated Edit something (add to visible display) */
/* E object */
void
f_evedit()
{
	drawreg = 1;
	regmemb = -1;
	eedit();
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
	register int i;
	static long stime, etime;	/* start & end times */

	(void)time( &stime );
	for( i=1; i < numargs; i++ )  {
		if( (dp = lookup( cmd_args[i], LOOKUP_NOISY )) == DIR_NULL )
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

		drawHobj( dp, ROOT, 0, identity );

		drawreg = 0;
		regmemb = -1;
	}
	(void)time( &etime );
	if( Viewscale == .125 )  {	/* also in ged.c */
		Viewscale = maxview;
		new_mats();
	}

	(void)printf("vectorized in %ld sec\n", etime - stime );
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
		if( (dp = lookup( cmd_args[i], LOOKUP_NOISY )) != DIR_NULL )
			eraseobj( dp );
	}
	dmaflag = 1;
}

void
f_debug()
{
	pr_solids( &HeadSolid );
}

void
f_regdebug()
{
	if( numargs <= 1 )
		regdebug = !regdebug;	/* toggle */
	else
		regdebug = atoi( cmd_args[1] );
	(void)printf("regdebug=%d\n", regdebug);
}

/* List object information */
/* Format: l object	*/
void
f_list()
{
	register struct directory *dp;
	register int i;
	union record record;
	
	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	db_getrec( dp, &record, 0 );

	if( record.u_id == ID_SOLID )  {
		(void)printf("%s:  %s\n",
			dp->d_namep,record.s.s_type==GENARB8 ? "GENARB8" :
			record.s.s_type==GENTGC ? "GENTGC" :
			record.s.s_type==GENELL ? "GENELL": "TOR" );

		pr_solid( &record.s );

		for( i=0; i < es_nlines; i++ )
			(void)printf("%s\n",&es_display[ES_LINELEN*i]);

		/* If in solid edit, re-compute solid params */
		if(state == ST_S_EDIT)
			pr_solid(&es_rec.s);

		return;
	}

	if( record.u_id == ID_ARS_A )  {
		(void)printf("%s:  ARS\n", dp->d_namep );
		db_getrec( dp, &record, 0 );
		(void)printf(" num curves  %d\n", record.a.a_m );
		(void)printf(" pts/curve   %d\n", record.a.a_n );
		db_getrec( dp, &record, 1 );
		/* convert vertex from the base unit to the local unit */
		(void)printf(" vertex      %.4f %.4f %.4f\n",
			record.b.b_values[0]*base2local,
			record.b.b_values[1]*base2local,
			record.b.b_values[2]*base2local );
		return;
	}
	if( record.u_id == ID_B_SPL_HEAD ) {
		(void)printf("%s:  SPLINE\n", dp->d_namep );
		db_getrec( dp, &record, 0 );
		(void)printf(" order %d %d\n", record.d.d_order[0],
			record.d.d_order[1]);
		(void)printf(" num Control points %d %d\n",
			record.d.d_ctl_size[0], record.d.d_ctl_size[1]);
		return;
	}
	if( record.u_id == ID_P_HEAD )  {
		(void)printf("%s:  %d granules of polygon data\n",
			dp->d_namep, dp->d_len-1 );
		return;
	}
	if( record.u_id != ID_COMB )  {
		(void)printf("%s: unknown record type!\n", dp->d_namep );
		return;
	}

	/* Combination */
	(void)printf("%s (len %d) ", dp->d_namep, dp->d_len-1 );
	if( record.c.c_flags == 'R' )
		(void)printf("REGION item=%d, air=%d, mat=%d, los=%d ",
			record.c.c_regionid, record.c.c_aircode,
			record.c.c_material, record.c.c_los );
	(void)printf("--\n", dp->d_len-1 );

	for( i=1; i < dp->d_len; i++ )  {
		db_getrec( dp, &record, i );
		(void)printf("  %c %s",
			record.M.m_relation, record.M.m_instname );

		if( record.M.m_brname[0] != '\0' )
			(void)printf(" br name=%s", record.M.m_brname );
#define Mat record.M.m_mat
		if( Mat[0] != 1.0  || Mat[5] != 1.0 || Mat[10] != 1.0 )
			(void)printf(" (Rotated)");
		if( Mat[MDX] != 0.0 || Mat[MDY] != 0.0 || Mat[MDZ] != 0.0 )
			(void)printf(" [%f,%f,%f]",
				Mat[MDX]*base2local,
				Mat[MDY]*base2local,
				Mat[MDZ]*base2local);
		if( Mat[12] != 0.0 || Mat[13] != 0.0 || Mat[14] != 0.0 )
			(void)printf(" ??Perspective=[%f,%f,%f]??",
				Mat[12], Mat[13], Mat[14] );
		(void)putchar('\n');
	}
#undef Mat
}

/* ZAP the display -- everything dropped */
/* Format: Z	*/
void
f_zap()
{
	register struct solid *sp;
	register struct solid *nsp;

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
	(void)printf("maxview=%f, ", maxview*base2local);
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

/* Fix the display processor after a hardware error, as best we can */
void
f_fix()
{
	dmp->dmr_restart();
	dmaflag = 1;		/* causes refresh() */
}

void
f_refresh()
{
	dmaflag = 1;		/* causes refresh() */
}

#define LEN 32
void
f_rt()
{
	register char **vp;
	register struct solid *sp;
	register int i;
	int pid, rpid;
	int retcode;
	int o_pipe[2];
	char *vec[LEN];
	FILE *fp;

	if( not_state( ST_VIEW, "Ray-trace of current view" ) )
		return;

	vp = &vec[0];
	*vp++ = "rt";
	*vp++ = "-f";
	*vp++ = "-M";
	for( i=1; i < numargs; i++ )
		*vp++ = cmd_args[i];
	*vp++ = filename;

	/* Find all unique top-level entrys.
	 *  Mark ones already done with s_iflag == UP
	 */
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
	FOR_ALL_SOLIDS( sp )  {
		register struct solid *forw;	/* XXX */

		if( sp->s_iflag == UP )
			continue;
		if( vp < &vec[LEN] )
			*vp++ = sp->s_path[0]->d_namep;
		else
			(void)printf("ran out of vec for %s\n",
				sp->s_path[0]->d_namep );
		sp->s_iflag = UP;
		for( forw=sp->s_forw; forw != &HeadSolid; forw=forw->s_forw) {
			if( forw->s_path[0] == sp->s_path[0] )
				forw->s_iflag = UP;
		}
	}
	*vp = (char *)0;

	vp = &vec[0];
	while( *vp )
		(void)printf("%s ", *vp++ );
	(void)printf("\n");

	(void)pipe( o_pipe );
	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		(void)close(0);
		(void)dup( o_pipe[0] );
		for( i=3; i < 20; i++ )
			(void)close(i);

		(void)signal( SIGINT, SIG_DFL );
		(void)execvp( "rt", vec );
		perror( "rt" );
		exit(42);
	}
	/* Connect up to pipe */
	(void)close( o_pipe[0] );
	fp = fdopen( o_pipe[1], "w" );

	/* Send out model2view matrix */
	for( i=0; i < 16; i++ )
		(void)fprintf( fp, "%.9e ", model2view[i] );
	(void)fclose( fp );
	
	/* Wait for rt to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */
	(void)signal(SIGINT, cur_sigint);

	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
}

void
f_saveview()
{
	register struct solid *sp;
	register int i;
	register FILE *fp;

	if( (fp = fopen( cmd_args[1], "a")) == NULL )  {
		perror(cmd_args[1]);
		return;
	}
	(void)fprintf(fp, "#!/bin/sh\nrt -M ");
	for( i=2; i < numargs; i++ )
		(void)fprintf(fp,"%s ", cmd_args[i]);
	(void)fprintf(fp,"-o %s.pix ", filename);
	(void)fprintf(fp,"%s ", filename);

	/* Find all unique top-level entrys.
	 *  Mark ones already done with s_iflag == UP
	 */
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
	FOR_ALL_SOLIDS( sp )  {
		register struct solid *forw;	/* XXX */

		if( sp->s_iflag == UP )
			continue;
		(void)fprintf(fp, "%s ", sp->s_path[0]->d_namep);
		sp->s_iflag = UP;
		for( forw=sp->s_forw; forw != &HeadSolid; forw=forw->s_forw) {
			if( forw->s_path[0] == sp->s_path[0] )
				forw->s_iflag = UP;
		}
	}
	(void)fprintf(fp," 2> %s.log", filename);
	(void)fprintf(fp," <<EOF");

	/* Send out model2view matrix */
	for( i=0; i < 16; i++ ) {
		if( (i%4) == 0 )  (void)fprintf(fp, "\n");
		(void)fprintf( fp, "%.9e ", model2view[i] );
	}
	(void)fprintf(fp,"\nEOF\n");
	(void)fclose( fp );
	
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
}

void
f_attach()
{
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
				dmp->dmr_viewchange( 2, sp );	/* DEL sol */
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
 *			P R _ S O L I D S
 *
 *  Given a pointer to a member of the circularly linked list of solids
 *  (typically the head), chase the list and print out the information
 *  about each solid structure.
 */
void
pr_solids( startp )
struct solid *startp;
{
	register struct solid *sp;
	register int i;

	sp = startp->s_forw;
	while( sp != startp )  {
		for( i=0; i <= sp->s_last; i++ )
			(void)printf("/%s", sp->s_path[i]->d_namep);
		(void)printf("  %s", sp->s_flag == UP ? "VIEW":"-NO-" );
		if( sp->s_iflag == UP )
			(void)printf(" ILL");
		/* convert to the local unit for printing */
		(void)printf(" [%f,%f,%f] size %f",
			sp->s_center[X]*base2local, sp->s_center[Y]*base2local, 
			sp->s_center[Z]*base2local,sp->s_size*base2local);
		(void)putchar('\n');
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

	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
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
	if( lastfound->s_flag != UP )  {
		(void)printf("%s not visible\n", cmd_args[1]);
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
	float f;

	f = atof(cmd_args[2]);
	if( f < -1.0 )
		f = -1.0;
	else if( f > 1.0 )
		f = 1.0;
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
	case 'Z':
		dm_values.dv_zoom = f;
		break;
	case 'X':
		dm_values.dv_xslew = f;
		break;
	case 'Y':
		dm_values.dv_yslew = f;
		break;
	default:
		(void)printf("x,y,z for joystick, Z for zoom, X,Y for slew\n");
		return;
	}
}
