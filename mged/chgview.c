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
 *	f_rrt		ray-trace using any program
 *	f_saveview	save the current view parameters
 *	f_attach	attach display device
 *	f_release	release display device
 *	eraseobj	Drop an object from the visible list
 *	pr_schain	Print info about visible list
 *	f_ill		illuminate the named object
 *	f_sed		simulate pressing "solid edit" then illuminate
 *	f_knob		simulate knob twist
 *	f_rmats		load views from a file
 *	f_savekey	save keyframe in file
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
#include "./ged.h"
#include "./objdir.h"
#include "./solid.h"
#include "./dm.h"

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

		drawHobj( dp, ROOT, 0, identity, 0 );
		regmemb = -1;
	}
	(void)time( &etime );
	if( Viewscale == .125 )  {	/* also in ged.c */
		Viewscale = maxview;
		new_mats();
	}

	(void)printf("vectorized in %ld sec\n", etime - stime );
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
		if( (dp = lookup( cmd_args[i], LOOKUP_NOISY )) != DIR_NULL )
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
do_list( dp )
register struct directory *dp;
{
	register int i;
	union record record;

	db_getrec( dp, &record, 0 );

	if( record.u_id == ID_SOLID )  {
		switch( record.s.s_type )  {
		case GENARB8:
			dbpr_arb( &record.s, dp );
			break;
		case GENTGC:
			dbpr_tgc( &record.s, dp );
			break;
		case GENELL:
			dbpr_ell( &record.s, dp );
			break;
		case HALFSPACE:
			dbpr_half( &record.s, dp );
			break;
		case TOR:
			dbpr_torus( &record.s, dp );
			break;
		default:
			printf("bad solid type %d\n", record.s.s_type );
			break;
		}

		/* This stuff ought to get pushed into the dbpr_xx code */
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
		/* convert vertex from base unit to the local unit */
		(void)printf(" vertex      %.4f %.4f %.4f\n",
			record.b.b_values[0]*base2local,
			record.b.b_values[1]*base2local,
			record.b.b_values[2]*base2local );
		return;
	}
	if( record.u_id == ID_BSOLID ) {
		dbpr_spline( dp );
		return;
	}
	if( record.u_id == ID_P_HEAD )  {
		(void)printf("%s:  %d granules of polygon data\n",
			dp->d_namep, dp->d_len-1 );
		return;
	}
	if( record.u_id != ID_COMB )  {
		(void)printf("%s: unknown record type!\n",
			dp->d_namep );
		return;
	}

	/* Combination */
	(void)printf("%s (len %d) ", dp->d_namep, dp->d_len-1 );
	if( record.c.c_flags == 'R' )
		(void)printf("REGION id=%d  (air=%d, los=%d, GIFTmater=%d) ",
			record.c.c_regionid,
			record.c.c_aircode,
			record.c.c_los,
			record.c.c_material );
	(void)printf("--\n");
	if( record.c.c_matname[0] )
		(void)printf("Material '%s' '%s'\n",
			record.c.c_matname,
			record.c.c_matparm);
	if( record.c.c_override == 1)
		(void)printf("Color %d %d %d\n",
			record.c.c_rgb[0],
			record.c.c_rgb[1],
			record.c.c_rgb[2]);
	if( record.c.c_matname[0] || record.c.c_override )  {
		if( record.c.c_inherit == DB_INH_HIGHER )
			(void)printf("(These material properties override all lower ones in the tree)\n");
	}

	for( i=1; i < dp->d_len; i++ )  {
		mat_t	xmat;

		db_getrec( dp, &record, i );
		rt_mat_dbmat( xmat, record.M.m_mat );

		(void)printf("  %c %s",
			record.M.m_relation, record.M.m_instname );

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
		if( (dp = lookup( cmd_args[arg], LOOKUP_NOISY )) == DIR_NULL )
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

/*
 *  			R T _ O L D W R I T E
 *  
 *  Write out the information that RT's -M option needs to show current view.
 *  Note that the model-space location of the eye is a parameter,
 *  as it can be computed in different ways.
 *  The is the OLD format, needed only when sending to RT on a pipe,
 *  due to some oddball hackery in RT to determine old -vs- new format.
 */
HIDDEN void
rt_oldwrite(fp, eye_model)
FILE *fp;
vect_t eye_model;
{
	register int i;

	(void)fprintf(fp, "%.9e\n", VIEWSIZE );
	(void)fprintf(fp, "%.9e %.9e %.9e\n",
		eye_model[X], eye_model[Y], eye_model[Z] );
	for( i=0; i < 16; i++ )  {
		(void)fprintf( fp, "%.9e ", Viewrot[i] );
		if( (i%4) == 3 )
			(void)fprintf(fp, "\n");
	}
	(void)fprintf(fp, "\n");
}

/*
 *  			R T _ W R I T E
 *  
 *  Write out the information that RT's -M option needs to show current view.
 *  Note that the model-space location of the eye is a parameter,
 *  as it can be computed in different ways.
 */
HIDDEN void
rt_write(fp, eye_model)
FILE *fp;
vect_t eye_model;
{
	register int i;

	(void)fprintf(fp, "viewsize %.9e;\n", VIEWSIZE );
	(void)fprintf(fp, "eye_pt %.9e %.9e %.9e;\n",
		eye_model[X], eye_model[Y], eye_model[Z] );
	(void)fprintf(fp, "viewrot ");
	for( i=0; i < 16; i++ )  {
		(void)fprintf( fp, "%.9e ", Viewrot[i] );
		if( (i%4) == 3 )
			(void)fprintf(fp, "\n");
	}
	(void)fprintf(fp, ";\n");
	(void)fprintf(fp, "start 0;\nend;\n");
}

/*
 *  			R T _ R E A D
 *  
 *  Read in one view in RT format.
 */
HIDDEN int
rt_read(fp, scale, eye, mat)
FILE	*fp;
fastf_t	*scale;
vect_t	eye;
mat_t	mat;
{
	register int i;
	double d;

	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	*scale = d*0.5;
	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	eye[X] = d;
	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	eye[Y] = d;
	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	eye[Z] = d;
	for( i=0; i < 16; i++ )  {
		if( fscanf( fp, "%lf", &d ) != 1 )
			return(-1);
		mat[i] = d;
	}
	return(0);
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
	char *dm;
	FILE *fp;

	if( not_state( ST_VIEW, "Ray-trace of current view" ) )
		return;

	/*
	 * This may be a workstation where RT and MGED have to share the
	 * display, so let display go.  We will try to reattach at the end.
	 */
	dm = dmp->dmr_name;
	release();

	vp = &vec[0];
	*vp++ = "rt";
	*vp++ = "-s50";
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
	{
		vect_t temp;
		vect_t eye_model;

		VSET( temp, 0, 0, 1 );
		MAT4X3PNT( eye_model, view2model, temp );
		rt_oldwrite(fp, eye_model );
	}
	(void)fclose( fp );
	
	/* Wait for rt to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */
	if( retcode != 0 )
		(void)printf("Abnormal exit status x%x\n", retcode);
	(void)signal(SIGINT, cur_sigint);

	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;

	if( retcode == 0 )  {
		/* Wait for a return, then reattach display */
		printf("Press RETURN to reattach\007\n");
		while( getchar() != '\n' )
			/* NIL */  ;
	}
	attach( dm );
}

/*
 *			F _ R R T
 *
 *  Invoke any program with the current view & stuff, just like
 *  an "rt" command (above).
 *  Typically used to invoke a remote RT (hence the name).
 */
void
f_rrt()
{
	register char **vp;
	register struct solid *sp;
	register int i;
	int pid, rpid;
	int retcode;
	int o_pipe[2];
	char *vec[LEN];
	char *dm;
	FILE *fp;

	if( not_state( ST_VIEW, "Ray-trace of current view" ) )
		return;

	/*
	 * This may be a workstation where RT and MGED have to share the
	 * display, so let display go.  We will try to reattach at the end.
	 */
	dm = dmp->dmr_name;
	release();

	vp = &vec[0];
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
		(void)execvp( cmd_args[1], vec );
		perror( cmd_args[1] );
		exit(42);
	}
	/* Connect up to pipe */
	(void)close( o_pipe[0] );
	fp = fdopen( o_pipe[1], "w" );
	{
		vect_t temp;
		vect_t eye_model;

		VSET( temp, 0, 0, 1 );
		MAT4X3PNT( eye_model, view2model, temp );
		rt_oldwrite(fp, eye_model );
	}
	(void)fclose( fp );
	
	/* Wait for rt to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */
	if( retcode != 0 )
		(void)printf("Abnormal exit status x%x\n", retcode);
	(void)signal(SIGINT, cur_sigint);

	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;

	if( retcode == 0 )  {
		/* Wait for a return, then reattach display */
		printf("Press RETURN to reattach\007\n");
		while( getchar() != '\n' )
			/* NIL */  ;
	}
	attach( dm );
}

/*
 *  				B A S E N A M E
 *  
 *  Return basename of path, removing leading slashes and trailing suffix.
 */
static char *
basename( p1, suff )
register char *p1, *suff;
{
	register char *p2, *p3;
	static char buf[128];

	p2 = p1;
	while (*p1) {
		if (*p1++ == '/')
			p2 = p1;
	}
	for(p3=suff; *p3; p3++) 
		;
	while(p1>p2 && p3>suff)
		if(*--p3 != *--p1)
			return(p2);
	strncpy( buf, p2, p1-p2 );
	return(buf);
}

void
f_saveview()
{
	register struct solid *sp;
	register int i;
	register FILE *fp;
	char *base;

	if( (fp = fopen( cmd_args[1], "a")) == NULL )  {
		perror(cmd_args[1]);
		return;
	}
	base = basename( cmd_args[1], ".sh" );
	(void)chmod( cmd_args[1], 0755 );	/* executable */
	(void)fprintf(fp, "#!/bin/sh\nrt -M ");
	for( i=2; i < numargs; i++ )
		(void)fprintf(fp,"%s ", cmd_args[i]);
	(void)fprintf(fp,"$*\\\n -o %s.pix\\\n", base);
	(void)fprintf(fp," %s\\\n ", filename);

	/* Find all unique top-level entrys.
	 *  Mark ones already done with s_iflag == UP
	 */
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
	FOR_ALL_SOLIDS( sp )  {
		register struct solid *forw;	/* XXX */

		if( sp->s_iflag == UP )
			continue;
		(void)fprintf(fp, "'%s' ", sp->s_path[0]->d_namep);
		sp->s_iflag = UP;
		for( forw=sp->s_forw; forw != &HeadSolid; forw=forw->s_forw) {
			if( forw->s_path[0] == sp->s_path[0] )
				forw->s_iflag = UP;
		}
	}
	(void)fprintf(fp,"\\\n 2> %s.log\\\n", base);
	(void)fprintf(fp," <<EOF\n");
	{
		vect_t temp;
		vect_t eye_model;

		VSET( temp, 0, 0, 1 );
		MAT4X3PNT( eye_model, view2model, temp );
		rt_write(fp, eye_model);
	}
	(void)fprintf(fp,"\nEOF\n");
	(void)fclose( fp );
	
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
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
		(void)printf("    (%.3f, %.3f, %.3f) sz=%.4f ",
			sp->s_center[X]*base2local,
			sp->s_center[Y]*base2local, 
			sp->s_center[Z]*base2local,
			sp->s_size*base2local );
		(void)printf("reg=%d",sp->s_regionid );
		if( sp->s_materp )  {
			register struct mater *mp;
			if( (mp = (struct mater *)sp->s_materp) != MATER_NULL)
				(void)printf(" dm%d",
					mp->mt_dm_int );
		}
		(void)printf("\n");
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
		(void)printf("x,y,z for joystick, Z for zoom, X,Y for slew\n");
		return;
	}
}

/*
 *			F _ R M A T S
 *
 * Load view matrixes from a file.  rmats filename [mode]
 *
 * Modes:
 *	-1	put eye in viewcenter (default)
 *	0	put eye in viewcenter, don't rotate.
 *	1	leave view alone, animate solid named "EYE"
 */
void
f_rmats()
{
	register FILE *fp;
	register struct directory *dp;
	register struct solid *sp;
	union record	rec;
	vect_t	eye_model;
	vect_t	xlate;
	vect_t	sav_center;
	vect_t	sav_start;
	int	mode;
	fastf_t	scale;
	mat_t	rot;
	register struct veclist *vp;
	register int nvec;

	if( not_state( ST_VIEW, "animate from matrix file") )
		return;

	if( (fp = fopen(cmd_args[1], "r")) == NULL )  {
		perror(cmd_args[1]);
		return;
	}
	mode = -1;
	if( numargs > 2 )
		mode = atoi(cmd_args[2]);
	switch(mode)  {
	case 1:
		if( (dp=lookup("EYE",LOOKUP_NOISY)) == DIR_NULL )  {
			mode = -1;
			break;
		}
		db_getrec( dp, &rec, 0 );
		FOR_ALL_SOLIDS(sp)  {
			if( sp->s_path[sp->s_last] == dp )  {
				VMOVE( sav_start, sp->s_vlist->vl_pnt );
				VMOVE( sav_center, sp->s_center );
				printf("animating EYE solid\n");
				goto work;
			}
		}
		/* Fall through */
	default:
	case -1:
		mode = -1;
		printf("default mode:  eyepoint at (0,0,1) viewspace\n");
		break;
	case 0:
		printf("rotation supressed, center is eyepoint\n");
		break;
	}
work:
	/* If user hits ^C, this will stop, but will leave hanging filedes */
	(void)signal(SIGINT, cur_sigint);
	while( !feof( fp ) &&
	    rt_read( fp, &scale, eye_model, rot ) >= 0 )  {
	    	switch(mode)  {
	    	case -1:
	    		/* First step:  put eye in center */
		       	Viewscale = scale;
		       	mat_copy( Viewrot, rot );
			MAT_DELTAS( toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
	    		new_mats();
	    		/* Second step:  put eye in front */
	    		VSET( xlate, 0, 0, -1 );	/* correction factor */
	    		MAT4X3PNT( eye_model, view2model, xlate );
			MAT_DELTAS( toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
	    		new_mats();
	    		break;
	    	case 0:
		       	Viewscale = scale;
			mat_idn(Viewrot);	/* top view */
			MAT_DELTAS( toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
			new_mats();
	    		break;
	    	case 1:
	    		/* Adjust center for displaylist devices */
	    		VMOVE( sp->s_center, eye_model );

	    		/* Adjust vector list for non-dl devices */
	    		VSUB2( xlate, eye_model, sp->s_vlist->vl_pnt );
			nvec = sp->s_vlen;
			for( vp = sp->s_vlist; nvec-- > 0; vp++ )  {
				VADD2( vp->vl_pnt, vp->vl_pnt, xlate );
			}
	    		break;
	    	}
		dmaflag = 1;
		refresh();	/* Draw new display */
	}
	if( mode == 1 )  {
    		VMOVE( sp->s_center, sav_center );
    		VSUB2( xlate, sav_start, sp->s_vlist->vl_pnt );
		nvec = sp->s_vlen;
		for( vp = sp->s_vlist; nvec-- > 0; vp++ )  {
			VADD2( vp->vl_pnt, vp->vl_pnt, xlate );
		}
	}
	dmaflag = 1;
	fclose(fp);
}

/* Save a keyframe to a file */
void
f_savekey()
{
	register int i;
	register FILE *fp;
	fastf_t	time;
	vect_t	eye_model;
	vect_t temp;

	if( (fp = fopen( cmd_args[1], "a")) == NULL )  {
		perror(cmd_args[1]);
		return;
	}
	if( numargs > 2 ) {
		time = atof( cmd_args[2] );
		(void)fprintf(fp,"%f\n", time);
	}
	/*
	 *  Eye is in conventional place.
	 */
	VSET( temp, 0, 0, 1 );
	MAT4X3PNT( eye_model, view2model, temp );
	rt_oldwrite(fp, eye_model);
	(void)fclose( fp );
}
