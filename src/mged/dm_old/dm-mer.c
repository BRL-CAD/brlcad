/*                        D M - M E R . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file dm-mer.c
 *
 *  MGED display manager for the Megatek Merlin (9200)
 *  display system.  This version only works over the serial
 *  interface.
 *  Based on dm-ps.c
 *
 *  Display structure -
 *	ROOT LIST = { wrld }
 *	wrld = { face, Mvie, Medi }
 *	face = { faceplate stuff, 2d lines and text }
 *	Mvie = { model2view matrix, view }
 *	view = { (all non-edit solids) }
 *	Medi = { model2objview matrix, edit }
 *	edit = { (all edit solids) }
 *
 *  Authors -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <sgtty.h>

#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

typedef unsigned char u_char;

/* Display Manager package interface */

#define MERBOUND	4095.9	/* Max magnification in Rot matrix */
int	Mer_open();
void	Mer_close();
int	Mer_input();
void	Mer_prolog(), Mer_epilog();
void	Mer_normal(), Mer_newrot();
void	Mer_update();
void	Mer_puts(), Mer_2d_line(), Mer_light();
int	Mer_object();
unsigned Mer_cvtvecs(), Mer_load();
void	Mer_statechange(), Mer_viewchange(), Mer_colorchange();
void	Mer_window(), Mer_debug();

struct dm dm_Mer = {
	Mer_open, Mer_close,
	Mer_input,
	Mer_prolog, Mer_epilog,
	Mer_normal, Mer_newrot,
	Mer_update,
	Mer_puts, Mer_2d_line,
	Mer_light,
	Mer_object,
	Mer_cvtvecs, Mer_load,
	Mer_statechange,
	Mer_viewchange,
	Mer_colorchange,
	Mer_window, Mer_debug,
	0,			/* no "displaylist", per. se. */
	MERBOUND,
	"mer", "Megatek MERLIN 9200",
	0,
	0
};

extern struct device_values dm_values;	/* values read from devices */

static int	mer_debug;		/* 2 for basic, 3 for full */

/* Map +/-2048 GED space into +/-16k MERLIN space */
#define GED2MERLIN(x)	((x)*6)
#define MERLIN2GED(x)	((x)/6)

/**** Local stuff to run the Merlin ****/

FILE *mer_fp;
int input_fd;
struct sgttyb Mer_oldstty, Mer_newstty;
struct tchars Mer_oldtc, Mer_newtc;	/* intr/quit/start/stop/eof/nl */
int Mer_oldloc, Mer_newloc;		/* local terminal modes */

int ck_input();
u_char *eat_chars(), *Dlink_Receive(), *Message_Receive();

#define HDR_OFFSET	10	/* 5 for network header, 5 for msg header */

#define ENTER_DATAGRAM	0x1D
#define ECHO_DATAGRAM	0x1E
#define END_DATAGRAM	0x1F

#define NET_STATE_CLOSED	0
#define NET_STATE_OPENING	1
#define NET_STATE_OPEN		2
#define NET_STATE_CLOSING	3
int Network_state = NET_STATE_CLOSED;
int net_remote = 99;	/* net conn, remote addr (from Conn Confirm NPDU) */
int net_local = 1;	/* net conn, local address */

char *Net_DRreasons[] = {
	"End of Session",
	"Resources Not Available",
	"Connection Does Not Exist",
	"Protocol Error",
	"Configuration Not Supported",
	"Crash",
	"Soft Reset",
	"I/O Error",
	"Duplicate Node ID/Address",
	"--zot--"
};

/* Database element types */
#define TY_STRING	0x03	/* char string definition */
#define TY_POINT	0x05
#define TY_LINE		0x06
#define TY_BOUNDEDA	0x07	/* polygon areas bounded by points */

/* Geometric type mask for coloring */
#define MASK_BACKGROUND	(1<<0)
#define MASK_CHAR	(1<<1)
#define MASK_POINT	(1<<2)
#define MASK_LINES	(1<<3)	/* And rows */
#define MASK_COLS	(1<<4)
#define MASK_CCW_SURF	(1<<8)
#define MASK_CW_SURF	(1<<9)
#define MASK_VIEWPORT	(1<<11)
#define MASK_GEOM	(MASK_CHAR|MASK_POINT|MASK_LINES|MASK_COLS| \
			MASK_CCW_SURF|MASK_CW_SURF)

#define DB_SIZE		4096
#define DB_START	(&db_buf[HDR_OFFSET])	/* start of non-header */
u_char db_buf[DB_SIZE+HDR_OFFSET+32];
u_char *db_next = DB_START;
int insert_mode = 0;


u_char crap[8196];	/* temp, for uploading into */

/*
 *			M E R _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
int
Mer_open()
{
	register int i, j;
	register int count;
	char *fullpath;
	char line[64], line2[64];

	/* Protect ourselves. */
	sync();

	(void)printf("Output MERLIN ? ");
	(void)gets( line );		/* Null terminated */
	if( feof(stdin) )  return(1);		/* BAD */
	if( line[0] != '\0' )  {
		if( (mer_fp = fopen(line,"r+w")) == NULL )  {
			(void)sprintf( line2, "/dev/tty%s%c", line, '\0' );
			if( (mer_fp = fopen(line2,"r+w")) == NULL )  {
				perror(line);
				return(1);		/* BAD */
			}
		}
		/* From here down is Dlink_Connect */
		input_fd = fileno(mer_fp);
		if( ioctl( input_fd, TIOCGETP, &Mer_oldstty ) < 0 )  {
			perror("TIOCGETP");
			fclose(mer_fp);
			return(1);			/* BAD */
		}
		if( ioctl( input_fd, TIOCGETC, &Mer_oldtc ) < 0 )  {
			perror("TIOCGETC");
			fclose(mer_fp);
			return(1);			/* BAD */
		}
		if( ioctl( input_fd, TIOCLGET, &Mer_oldloc ) < 0 )  {
			perror("TIOCLGET");
			fclose(mer_fp);
			return(1);			/* BAD */
		}
		Mer_newstty = Mer_oldstty;		/* struct copy */
		Mer_newtc = Mer_oldtc;			/* struct copy */
		Mer_newloc = Mer_oldloc;
		/*
		 * 8 bit input would need RAW, but terminal needs ^S/^Q.
		 * We use LITOUT to get 8-bit binary output, but use
		 * cooked input mode, simply setting the extra break
		 * character to the END_DATAGRAM character.
		 */
		Mer_newstty.sg_flags &= ~(ECHO|CRMOD|XTABS|LCASE);
		Mer_newstty.sg_flags |= ANYP;
		Mer_newloc |= LLITOUT;
		if( ioctl( input_fd, TIOCSETP, &Mer_newstty ) < 0 )  {
			perror("TIOCSETP");
			fclose(mer_fp);
			return(1);			/* BAD */
		}
		Mer_newtc.t_startc = 0x11;		/* ^Q */
		Mer_newtc.t_stopc = 0x13;		/* ^S */
		Mer_newtc.t_brkc = END_DATAGRAM;
		if( ioctl( input_fd, TIOCSETC, &Mer_newtc ) < 0 )  {
			perror("TIOCSETC");
			fclose(mer_fp);
			return(1);			/* BAD */
		}
		if( ioctl( input_fd, TIOCLSET, &Mer_newloc ) < 0 )  {
			perror("TIOCLSET");
			fclose(mer_fp);
			return(1);			/* BAD */
		}
	} else {
		return(1);				/* BAD */
	}

	Network_Connect();		/* network level connection */
	if( Network_state != NET_STATE_OPEN )  {
		(void)printf("Unable to connect, aborting Mer_open\n");
		Mer_close();
		return(1);		/* BAD */
	}
	/* Note:  On serial line, no need to open message connection! */

	/* Wipe Merlin database, and clear screen */
	EdInit();
	EdClear();		/* Clear frame buffer */

	EdOpen( 'face', 0, 1000, 0 );	/* a null object, for starters */
	EdClose();
	EdOpen( 'colr', 0, 8000, 0 );
	EdClose();

	Mer_colorchange();	/* to load color map into 'colr' */

	EdOpen( 'wrld', 8, 600, 0 );	/* make a root object */
	EdInsert();
	DbFrameBufferErase();
	DbRefer( 'colr' );
	DbRefer( 'face' );
	DbFrameBufferCopy();
	EdClose();
	edflush();

	return(0);			/* OK */
}

/*
 *  			M E R _ C L O S E
 *
 *  Gracefully release the display.
 */
void
Mer_close()
{
	/* This routine is aka Dlink_Disconnect */
	if( Network_state == NET_STATE_OPEN )
		Network_Disconnect( 5 );	/* "Crash", ahem */
	(void)ioctl( input_fd, TIOCSETP, &Mer_oldstty );
	(void)ioctl( input_fd, TIOCSETC, &Mer_oldtc );
	(void)ioctl( input_fd, TIOCLSET, &Mer_oldloc );
	(void)close( input_fd );
	input_fd = -1;
}

dumpEntity( name )
long name;
{
	register u_char *cp = crap;
	int len;

	printf("Dumping %c%c%c%c:\n", name>>24, (name>>16)&0xFF,
		(name>>8)&0xFF, name&0xFF );
	len = EdUploadNamedEntity( name, crap );
	while( len > 0 )  {
		printf(" %2.2x", *cp++ );
		printf(" %2.2x", *cp++ );
		printf(" %2.2x", *cp++ );
		printf(" %2.2x", *cp++ );
		printf("\t%2.2x", *cp++ );
		printf(" %2.2x", *cp++ );
		printf(" %2.2x", *cp++ );
		printf(" %2.2x", *cp++ );
		printf("\n");
		len -= 8;
	}
}

/*
 *			M E R _ P R O L O G
 *
 * Define the world, and include in it instances of all the
 * important things.  Most important of all is the object "faceplate",
 * which is built between dmr_normal() and dmr_epilog()
 * by dmr_puts and dmr_2d_line calls from adcursor() and dotitles().
 */
void
Mer_prolog()
{

	if( !dmaflag )
		return;

	EdOpen( 'Mvie', 0, 100, 0 );
	EdDeleteOpenEntity();
	EdInsert();
	DbReplaceMatrix();
	DbCorrectAspectRatio(0);	/* shrink picture to fit */
	Db4x4Matrix( model2view );
DbCaptureTransformationState( 'sav1');
	DbRefer( 'view' );
	EdClose();

	if( state == ST_VIEW )
		return;

	EdOpen( 'Medi', 0, 100, 0 );
	EdDeleteOpenEntity();
	EdInsert();
	DbReplaceMatrix();
	DbCorrectAspectRatio(0);	/* shrink picture to fit */
	Db4x4Matrix( model2objview );
	DbRefer( 'edit' );
	EdClose();
}
mer_cvt_mat( m )
register mat_t *m;
{
	/* TODO */
}

/*
 *			M E R _ E P I L O G
 */
void
Mer_epilog()
{
	/*
	 * A Point, in the Center of the Screen.
	 * This is drawn last, to always come out on top.
	 */
	Mer_2d_line( 0, 0, 0, 0, 0 );
	/* End of faceplate */
	EdClose();			/* close 'face' */
}

/*
 *  			M E R _ N E W R O T
 *  Stub.
 */
void
Mer_newrot(mat)
mat_t mat;
{
}

/*
 *  			M E R _ O B J E C T
 *
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.  Mat is model2view.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */
int
Mer_object( sp, mat, ratio, white )
register struct solid *sp;
mat_t mat;
double ratio;
{
	return(1);	/* OK */
}

/*
 *			M E R _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 * Also, start the definition of the faceplate.
 * This BEGIN_STRUCTURE is matched by an END_STRUCTURE
 * produced by dmr_epilog().
 */
void
Mer_normal()
{
	EdOpen( 'face', 0, 1000, 0 );
	EdDeleteOpenEntity();
	EdInsert();
#ifdef never
	PWindow("", -1.,1., -1.,1., 1.0E-15,1.0E15, "");
#endif
}

/*
 *			M E R _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
Mer_update()
{
	if( !dmaflag )
		return;

	/* Get changes onto the screen */
	EdDisplayRootList();		/* traverse tree */
	edflush();		/* flush output buffers */
}

/*
 *			M E R _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
void
Mer_puts( str, x, y, size, color )
register u_char *str;
{
	DbPos2A( GED2MERLIN(x), GED2MERLIN(y) );
	DbMaskedHueSelect( color, MASK_GEOM );
	/* Size?  */
	DbBegin( TY_STRING );
	DbBlock( strlen(str), 0x71 );	/* system stroke char opcode */
	while( *str )
		db1( *str++ );
	DbEnd();
}

/*
 *			M E R _ 2 D _ L I N E
 *
 */
void
Mer_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	/* Dashed? */
	DbMaskedHueSelect( DM_YELLOW, MASK_LINES );
	DbBegin( TY_LINE );
	DbPos2A( GED2MERLIN(x1), GED2MERLIN(y1) );
	DbPos2A( GED2MERLIN(x2), GED2MERLIN(y2) );
	DbEnd();
}

/*
 *			M E R L I M I T
 *
 * Because the dials are so sensitive, setting them exactly to
 * zero is very difficult.  This function can be used to extend the
 * location of "zero" into "the dead zone".
 */
static float
merlimit(i)
double i;
{
#define NOISE	0.1

	if( i > NOISE )
		return( i-NOISE );
	if( i < -NOISE )
		return( i+NOISE );
	return(0.0);
}

/*
 *			M E R _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream, or a device event has
 * occured, unless "noblock" is set.
 *
 * The GED "generic input" structure is filled in.
 *
 * Returns:
 *	0 if no command waiting to be read,
 *	1 if command is waiting to be read.
 */
Mer_input( cmd_fd, noblock )
{
	register int i, j;
	int readfds;

	edflush();		/* Flush any pending output */
	/*
	 * Check for input on the keyboard or on the polled registers.
	 *
	 * Suspend execution until either
	 *  1)  User types a full line
	 *  2)  A change in peripheral status occurs
	 *  3)  The timelimit on SELECT has expired
	 *
	 * If a RATE operation is in progress (zoom, rotate, slew)
	 * in which the peripherals (rate setting) may not be changed,
	 * but we still have to update the display,
	 * do not suspend execution.
	 */
	if( noblock )  {
		readfds = bsdselect( 1<<cmd_fd, 0, 300000 );
	}  else  {
		readfds = bsdselect( 1<<cmd_fd, 1000, 0 );
	}

	/*
	 * Set device interface structure for GED to "rest" state.
	 * First, process any messages that came in.
	 */
	dm_values.dv_buttonpress = 0;
	dm_values.dv_flagadc = 0;
	dm_values.dv_penpress = 0;

	if( readfds & (1<<input_fd) )  {
		(void)eat_chars();
	}

	if( readfds & (1<<cmd_fd) )
		return(1);		/* command awaits */
	else
		return(0);		/* just peripheral stuff */
}

/*
 *			M E R _ L I G H T
 *
 * This function must keep both the light hardware, and the software
 * copy of the lights up to date.  Note that requests for light changes
 * may not actually cause the lights< to be changed, depending on
 * whether the buttons are being used for "view" or "edit" functions
 * (although this is not done in the present code).
 */
void
Mer_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
}

/*
 *			M E R _ C V T V E C S
 *
 * Convert model space vector list to Merlin Database commands, and
 * calculate displaylist memory requirement.
 * The co-ordinate system extends from +full scale to
 * -full scale, as the object extends from xmin to xmax.
 * Hence the total co-ordinate distance is 2*full scale.
 *
 * Note that these macros draw the object FULL SCALE.
 * (Ie to occupy the full screen).  dozoom() depends on this.
 *
 *	delta * full scale / (scale/2)
 * gives delta * full scale * 2 / scale
 *
 */
unsigned
Mer_cvtvecs( sp )
register struct solid *sp;
{
	register struct vlist *vp;

	EdOpen( sp, 0, 1000, 0 );	/* need better size est */
	EdDeleteOpenEntity();
	EdInsert();
	DbBegin( TY_LINE );

	for( vp = sp->s_vlist; vp != VL_NULL; vp = vp->vl_forw )  {
		if( vp->vl_draw == 0 )  {
			/* Move, not draw */
			DbEnd();
/* Need to interact with INSERT command here! */
/***			dbflush();	/* conservative */
			DbBegin( TY_LINE );
		}  else  {
			/* draw */
		}
		/* Z sign may be wrong */
		homog( vp );		/* Does DbPos4A() */
		/* Worry about overflow of 1000 byte buffer here */
	}
	DbEnd();			/* End line definition */
	EdClose();			/* End EdInsert */

#ifdef never
	if( sp->s_soldash )
		PPatWith( oname, "dotdashed" );
#endif
	return( 0 );	/* No "displaylist" consumed */
}

homog( vp )
register struct vlist *vp;
{
	static double big, temp;
	static double mul;
	static int x, y, z, w;
	static int exp;

#define abs(a)	(((a)<0)?(-(a)):(a))
	big = abs( vp->vl_pnt[0] );
	temp = abs( vp->vl_pnt[1] );
	if( temp > big )  big = temp;
	temp = abs( vp->vl_pnt[2] );
	if( temp > big )  big = temp;

	big = abs( big );
	if( big < 1.0 )  {
		w = 16384;
		mul = 16384.0;
	} else {
		temp = frexp( big, &exp );	/* big = temp * 2^exp */
		if( exp > 14 )  {
			printf("dm-mer/homog: big %f, dropped\n", big);
			return;
		}
		w = 1<<(14-exp);
		mul = 16384.0/(1<<exp);
		if( big * mul > 16384.0 )  {
			printf(" big=%f * mul=%f > 1.0\n", big, mul, big*mul);
			return;
		}
	}
#define SCALE_GED2MER	0.75	/* implication of GED2MERLIN() */
	x = SCALE_GED2MER * vp->vl_pnt[0] * mul + 0.999;
	y = SCALE_GED2MER * vp->vl_pnt[1] * mul + 0.999;
	z = SCALE_GED2MER * vp->vl_pnt[2] * mul + 0.999;
#ifdef never
printf("X: %f ?= %f\n", vp->vl_pnt[0], ((double)x)/w );
printf("Y: %f ?= %f\n", vp->vl_pnt[1], ((double)y)/w );
printf("Z: %f ?= %f\n", vp->vl_pnt[2], ((double)z)/w );
#endif

	DbPos4A( x, y, z, w );
}

/*
 * Loads displaylist from storage[]
 */
unsigned
Mer_load( addr, count )
unsigned addr, count;
{
	return( 0 );		/* FLAG:  error */
}

void
Mer_statechange( a, b )
{
#ifdef never
	static P_PatternType pattern = { 1, 1, 3, 1 }; /* dot dashed */
	double interval;
	char str[32];
	register struct solid *sp;

	/* Define "dotdashed" pattern */
	interval = Viewscale/100.0;
	if( interval < 5.0 || interval > 1000.0 )  interval = 50.0;
	PDefPatt( "dotdashed", 4, pattern, 1, 1, interval );

	PInst( "model", "" );
	PXfMatrx("model_mat","model");
	FOR_ALL_SOLIDS( sp )  {
		if( sp->s_iflag == UP )
			continue;
		sprintf( str, "O%x%c", sp, '\0' );
		PIncl( str, "model" );
	}
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting, and transmitting the separate
	 *  "stuff" display group (for solid & object editing).
	 */
	switch( b )  {
	case ST_VIEW:
		PSndBool(0, 1, "T_track");	/* constant tracking OFF */
		break;

	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
		PSndBool(1, 1, "T_track");	/* constant tracking ON */
		goto com;
	case ST_O_EDIT:
	case ST_S_EDIT:
		PSndBool(0, 1, "T_track");	/* constant tracking OFF */
com:
		PInst( "stuff", "" );
		PXfMatrx("stuff_mat","stuff");
		FOR_ALL_SOLIDS( sp )  {
			/* Ignore all objects not being rotated */
			if( sp->s_iflag != UP )
				continue;
			sprintf( str, "O%x%c", sp, '\0' );
			PIncl( str, "stuff" );
			if( sp == illump )  {
				/* Intensify by doing twice */
				PIncl( str, "stuff" );
			}
		}
		break;
	default:
		(void)printf("Mer_statechange: unknown state %s\n", state_str[b]);
		break;
	}
#endif
}

void
Mer_viewchange( cmd, sp )
register int cmd;
register struct solid *sp;
{
	printf("Mer_viewchange(%d,x%x)\n", cmd, sp);
	switch( cmd )  {
	case DM_CHGV_ADD:
		EdOpen( 'view', 0, 10000, 0 );
		EdInsert();
		if( sp->s_materp != (char *)0 )  {
			DbMaskedHueSelect(
				((struct mater *)sp->s_materp)->mt_dm_int,
				MASK_GEOM );
		}
		/* else it just re-uses last color */
		DbRefer( sp );
		EdClose();
		edflush();
		break;
	case DM_CHGV_REDO:
	case DM_CHGV_DEL:
		/* Need to erase and rebuild 'view' and 'edit' */
		EdOpen( 'view', 0, 10000, 0 );
		EdDeleteOpenEntity();
		EdInsert();
		FOR_ALL_SOLIDS( sp )  {
			/* Ignore all objects being rotated */
			if( sp->s_iflag == UP )
				continue;
			if( sp->s_materp != (char *)0 )  {
				DbMaskedHueSelect(
				    ((struct mater *)sp->s_materp)->mt_dm_int,
				    MASK_GEOM );
			}
			DbRefer( sp );
		}
		EdClose();
		EdOpen( 'edit', 0, 10000, 0 );
		EdDeleteOpenEntity();
		EdInsert();
		FOR_ALL_SOLIDS( sp )  {
			/* Ignore all objects not being rotated */
			if( sp->s_iflag != UP )
				continue;
			if( sp->s_materp != (char *)0 )  {
				DbMaskedHueSelect(
				    ((struct mater *)sp->s_materp)->mt_dm_int,
				    MASK_GEOM );
			}
			DbRefer( sp );
		}
		EdClose();
		break;
	case DM_CHGV_REPL:
		return;
	case DM_CHGV_ILLUM:
		break;
	}
	EdOpen( 'wrld', 8, 600, 0 );	/* the root object */
	EdDeleteOpenEntity();
	EdInsert();
	DbFrameBufferErase();
	DbCorrectAspectRatio(0);	/* shrink picture to fit */
	DbMaskedVisibilityEnable( MASK_GEOM|MASK_VIEWPORT );
	DbShadingMix( 0 );		/* 7bits hue, 5 bits intensity */
	DbShadingType( 1 );		/* 1=depth cue, 2=constant inten */
	DbRefer( 'colr' );
	DbRefer( 'face' );
	DbRefer( 'Mvie' );		/* Matrix_view intermediate */
	if( state != ST_VIEW )
		DbRefer( 'Medi' );	/* Matrix_edit intermediate */
	DbFrameBufferCopy();
	EdClose();
	edflush();
#ifdef never
	/*
	 *  This is the top-level node in the PS300 display tree
	 *  (whatever E&S calls it).  Note that the matrixes
	 *  "mmat" and "smat" are replaced by the prolog() routine.
	 *  There are 2 or 3 inferior elements:  faceplate, model, and stuff.
	 */
	PBeginS("world");
	PInst( "", "faceplate" );
	PViewP( "", -1.0,1.0, -1.0,1.0,	1.0,1.0, "" );
	PWindow("mmat", -1.0,1.0, -1.0,1.0,
		1.0E-7,1.0E7, "");
	PSetInt( "depth_cue", 1, 0.75, 1.0, "model" );
	if( b != ST_VIEW )  {
		PWindow("smat", -1.0,1.0, -1.0,1.0,
			1.0E-7,1.0E7, "");
		PSetInt( "sdepth_cue", 1, 1.0, 1.0, "stuff" );
	}
	PEndS();
#endif
}

void
Mer_debug(lvl)
{
	mer_debug = lvl;
}

void
Mer_window(w)
int w[];
{
}


/*
 * Color Map table
 */
#define NSLOTS		128	/* shading mix 0: 7hue, 5inten */
static struct rgbtab {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
} mer_rgbtab[NSLOTS];

/*
 *  			M E R _ C O L O R C H A N G E
 *
 *  Go through the mater table, and allocate color map slots.
 *  With 4096, we shouldn't often run out.
 */
static int slotsused;
void
Mer_colorchange()
{
	register struct mater *mp;

	printf("Mer_colorchange\n");
	mer_rgbtab[0].r=0; mer_rgbtab[0].g=0; mer_rgbtab[0].b=0;/* Black */
	mer_rgbtab[1].r=255; mer_rgbtab[1].g=0; mer_rgbtab[1].b=0;/* Red */
	mer_rgbtab[2].r=0; mer_rgbtab[2].g=0; mer_rgbtab[2].b=255;/* Blue */
	mer_rgbtab[3].r=255; mer_rgbtab[3].g=255;mer_rgbtab[3].b=0;/*Yellow */
	mer_rgbtab[4].r = mer_rgbtab[4].g = mer_rgbtab[4].b = 255; /* White */
	slotsused = 5;
	for( mp = MaterHead; mp != MATER_NULL; mp = mp->mt_forw )
		mer_colorit( mp );

	color_soltab();		/* apply colors to the solid table */

	EdOpen( 'colr', 0, 8000, 0 );
	EdDeleteOpenEntity();
	DbColorData( mer_rgbtab, slotsused );
	EdClose();
}

int
mer_colorit( mp )
struct mater *mp;
{
	register struct rgbtab *rgb;
	register int i;
	register int r,g,b;

	r = mp->mt_r;
	g = mp->mt_g;
	b = mp->mt_b;
	if( (r == 255 && g == 255 && b == 255) ||
	    (r == 0 && g == 0 && b == 0) )  {
		mp->mt_dm_int = DM_WHITE;
		return;
	}

	/* First, see if this matches an existing color map entry */
	rgb = mer_rgbtab;
	for( i = 0; i < slotsused; i++, rgb++ )  {
		if( rgb->r == r && rgb->g == g && rgb->b == b )  {
			 mp->mt_dm_int = i;
			 return;
		}
	}

	/* If slots left, create a new color map entry, first-come basis */
	if( slotsused < NSLOTS )  {
		rgb = &mer_rgbtab[i=(slotsused++)];
		rgb->r = r;
		rgb->g = g;
		rgb->b = b;
		mp->mt_dm_int = i;
		return;
	}
	mp->mt_dm_int = DM_YELLOW;	/* Default color */
}

/****************************************+=+=+=+=+=+=+=+=+=********** */

#ifdef Not_Serial
/* Not used for serial line version */
Message_Connect( link )
{
	u_char buf[128];

	if(mer_debug>=2) (void)printf("Message_Connect(%d)\n", link);
	if( Network_state != NET_STATE_OPEN )
		Network_Connect();

	buf[HDR_OFFSET+1] = 0xE0;		/* ConnReq, credit=0 */
	buf[HDR_OFFSET+2] = 0;			/* dst ref */
	buf[HDR_OFFSET+3] = 0;			/* dst ref */
	buf[HDR_OFFSET+4] = link&0xFF;		/* src-ref of msg svc conn */
	buf[HDR_OFFSET+5] = (link>>8)&0xFF;	/* src-ref, upper byte */
	buf[HDR_OFFSET+6] = 0x20;		/* class 2 */

	buf[HDR_OFFSET+0] = 6;			/* header len */
	Network_Send( net_remote, &buf[HDR_OFFSET], 7 );
	/* Need to see an answer here */
	if( ck_input(1) )  (void)eat_chars();
}

Message_Disconnect()
{
	if(mer_debug>=2) (void)printf("Message_Disconnect\n");
}
#endif Not_Serial

/*
 *  			M E S S A G E _ S E N D
 *
 *  This routine sends a higher-level message to a process in the Merlin.
 *  Note that at this level, "dest" is a MERLIN "well-known" logical unit.
 *  See Appendix A of the Communications Reference Manual.
 *	1	Process_1 (host?)
 *  	4	Configuration_Services
 *  	5	Error_Reporting_Services
 *  	9	Editor
 *  	10	LTL_Task_0
 *  	18	LTL_Task_8
 *
 *  The caller is responsible for having HDR_OFFSET space
 *  free BEFORE the area pointed at by 'ptr'!
 */
Message_Send( dest, ptr, cnt )
register int dest;
register u_char *ptr;
{
	if(mer_debug>=2) (void)printf("Message_Send %d, dest=%d \n", cnt, dest);
	ptr[-5] = 4;		/* header len */
	ptr[-4] = 0xF0;		/* data transfer */
	ptr[-3] = dest & 0xFF;	/* dst-ref */
	ptr[-2] = (dest>>8) & 0xFF;	/* dst-ref, upper byte */
	ptr[-1] = 0x80;		/* EOM */
	/* Need to make sure that data area is < MPDUsize-5 (1000-5) */
	Network_Send( net_remote, ptr-5, cnt+5 );
}

int Message_data_len;	/* length of message returned by eat_chars() */

/*
 *  			M E S S A G E _ R E C E I V E
 *
 *  Called by Dlink_Receive() whenever a Data NPDU packet is read.
 *  Returns 0 if there is no application data involved, else returns
 *  a pointer to the first byte of the actual application data.
 */
u_char *
Message_Receive( ptr, count )
register u_char *ptr;
int count;
{
	register int hlen;

	if(mer_debug>=2) (void)printf("Message_Receive %d\n", count);
	/* Warning -- there may be multiple messages in the buffer we get */
	/* If so, this code never sees the rest */
	hlen = ptr[0];		/* Header length */
	switch( ptr[1]>>4 )  {
	case 0xF:
		/* Data */
		/* Might also need to check EOM bit here? */
		Message_data_len = count - 1 - hlen;	/* extern, sorry */
		return( ptr+1+hlen );		/* hope he checks how much! */
	case 0xE:
		/* Connect Request */
	case 0xD:
		/* Connect Confirm */
	case 0x8:
		/* Disconnect Request */
	case 0xC:
		/* Disconnect Confirm */
	case 0x6:
		/* Data Acknowledge */
	case 0x7:
		/* Error */
	default:
		(void)printf("Unexpected MPDU, type x%x\n", ptr[2] );
		break;
	}
	return( (u_char *)0 );
}

/*
 *  			N E T W O R K _ C O N N E C T
 *
 *  Attempt to open a network peer-to-peer connection to
 *  carry all message level connections.
 *  Try several times.
 */
Network_Connect()
{
	register int i;
	u_char buf[128];
	register u_char *cp;

	cp = buf;
	*cp++ = 0xE0;			/* Connect request:  CR_NPDU */
	*cp++ = net_local&0xFF;		/* src-ref, rhs*/
	*cp++ = (net_local>>8)&0xFF;	/* src-ref, lhs */
	*cp++ = (0<<4) |		/* initial request */
		0x01;			/* minimum- point-to-point */
	*cp++ = 0;			/* len of Var part, filled later */

	/*
	 *  The Variable Portion, long and involved
	 */
	*cp++ = 0x06;		/* Version */
	*cp++ = 3;		/* len */
	*cp++ = 0;		/* version */
	*cp++ = 0;		/* revision */
	*cp++ = 0;		/* "reserved" */

	*cp++ = 0x07;		/* Source Node Identifier */
	*cp++ = 6;
	*cp++ = 'V';
	*cp++ = 'A';
	*cp++ = 'X';
	*cp++ = '0';
	*cp++ = '0';
	*cp++ = '1';

	*cp++ = 0x01;		/* Datagram size from node (send) */
	*cp++ = 2;
	*cp++ = 0x00;		/* 0x0800, 2048. bytes in Link packet */
	*cp++ = 0x08;

	*cp++ = 0x04;		/* Datagram Echo from node (send) */
	*cp++ = 1;
	*cp++ = 0;		/* OFF */

	*cp++ = 0x03;		/* Packed Data Mode from node (send) */
	*cp++ = 1;
	*cp++ = 0;		/* OFF */

	*cp++ = 0x11;		/* Datagram Size to Node (rcv) */
	*cp++ = 2;
	*cp++ = 0xF0;		/* 0x00F0, 248. bytes in Link packet */
	*cp++ = 0x00;		/* Because TTYHOG = 255. */

	*cp++ = 0x14;		/* Datagram Echo to node (rcv) */
	*cp++ = 1;
	*cp++ = 0;		/* OFF */

	*cp++ = 0x13;		/* Packed Data mode to node (rcv) */
	*cp++ = 1;
	*cp++ = 1;		/* ON */

	*cp++ = 0x12;		/* Packed Data Length to node (rcv) */
	*cp++ = 1;
	*cp++ = 4;		/* 4-in-8 packing (HEX) */

	*cp++ = 0x15;		/* Packed Data Lookup Table to node (rcv) */
	*cp++ = 1;
	*cp++ = 1;		/* ASCII */

	buf[4] = (cp-buf)-5;	/* variable part length */

	for( i=0; i < 5; i++ )  {
		if( Network_state == NET_STATE_OPEN )  {
/***			Network_Suspend();	/* VMS does this */
			return;		/* OK */
		}
		Network_state = NET_STATE_OPENING;
		if(mer_debug>=3) (void)printf("Network_Connect\n");
		Dlink_Send( buf, cp-buf );
		/* await Connection Confirm */
		if( ck_input(1) )  (void)eat_chars();
	}
	return;			/* FAIL */
}

Network_Disconnect(reason)
int reason;
{
	u_char buf[128];
	register u_char *cp;
	register int i;

	cp = buf;
	*cp++ = 0x80;			/* Disconnect request:  DR_NPDU */
	*cp++ = net_remote & 0xFF;	/* dest-ref */
	*cp++ = (net_remote>>8)&0xFF;
	*cp++ = net_local & 0xFF;		/* src-ref */
	*cp++ = (net_local>>8)&0xFF;
	*cp++ = (u_char)reason;

	for( i=0; i < 3; i++ )  {
		if( Network_state == NET_STATE_CLOSED )
			return;		/* OK */
		Network_state = NET_STATE_CLOSING;
		if(mer_debug>=3) (void)printf("Network_Disconnect(%s)\n", Net_DRreasons[reason]);
		Dlink_Send( buf, 6 );
		/* await Disconnect Confirm */
		if( ck_input(1) )  (void)eat_chars();
	}
}

Network_Discovery()
{
	u_char buf[128];
	u_char *cp;

	cp = buf;
	*cp++ = 0x0F;		/* Discovery code */
	*cp++ = 1;		/* Discovery request */
	*cp++ = 0x26;		/* ISO Msg, Megatek NCP, RS-232 */
	*cp++ = 0;		/* unused */
	*cp++ = 0;		/* Variable part */

	if(mer_debug>=2) (void)printf("Network_Discovery\n");
	Dlink_Send( buf, cp-buf );
	/* await reply */
	if( ck_input(1) )  (void)eat_chars();
}

/*
 *  		N E T W O R K _ S U S P E N D
 *
 *  Prevent terminal from sending packets, and allow characters
 *  typed at the terminal keyboard to be sent through.  Ie, disable
 *  the MCF (Message Service Communications Facility), and enable
 *  the DTF (Data Terminal Facility).
 *  This should be called when prompting the user from the Merlin
 *  terminal keyboard.
 */
Network_Suspend()
{
	u_char buf[16];

	if(mer_debug>=2) (void)printf("Network_Suspend\n");
	buf[0] = 0x0E;		/* Suspend request SR_NPDU */
	buf[1] = net_remote&0xFF;
	buf[2] = (net_remote>>8)&0xFF;
	buf[3] = net_local&0xFF;
	buf[4] = (net_local>>8)&0xFF;
	Dlink_Send( buf, 5 );
	/* await reply */
	if( ck_input(1) )  (void)eat_chars();
}

/*
 *  			N E T W O R K _ R E S U M E
 *
 *  Permit terminal to send packets again, disabling the DTF
 *  (Data Terminal Facility) and re-enabling the MCF (Message
 *  Service Communications Facility).
 */
Network_Resume()
{
	u_char buf[16];

	if(mer_debug>=2) (void)printf("Network_Resume\n");
	buf[0] = 0x0C;		/* Resume request SR_NPDU */
	buf[1] = net_remote&0xFF;
	buf[2] = (net_remote>>8)&0xFF;
	buf[3] = net_local&0xFF;
	buf[4] = (net_local>>8)&0xFF;
	Dlink_Send( buf, 5 );
	/* should be no reply, but... */
	if( ck_input(0) )  (void)eat_chars();
}

/*
 *  			N E T W O R K _ S E N D
 *
 *  Place a network data transfer header into the area in FRONT
 *  of the buffer pointed to by 'ptr', and send it to the data link.
 */
Network_Send( dest, ptr, len )
register int dest;
register u_char *ptr;
register int len;
{
	ptr[-5] = 0xF0;			/* DT NPDU, send data */
	ptr[-4] = dest & 0xFF;		/* dest ref, rhs */
	ptr[-3] = (dest>>8) & 0xFF;	/* dest ref, lhs */
	ptr[-2] = len & 0xFF;		/* data len, rhs */
	ptr[-1] = (len>>8) & 0xFF;	/* data len, lhs */
	Dlink_Send( ptr-5, len+5 );
}

/*
 *  			D L I N K _ S E N D
 *
 *  The default state of the line in packet mode is
 *  "packed data" mode, with 4 bits of data encoded as HEX
 *  sent per output byte.  For simplicity, we will keep it
 *  this way for now.
 */
Dlink_Send( ptr, count )
register u_char *ptr;
register int count;
{
	if(mer_debug>=3) (void)printf("Dlink_Send %d: ", count);
	(void)putc( ENTER_DATAGRAM, mer_fp );
	while( count-- > 0 )  {
		if(mer_debug>=3) (void)printf("%2.2x ", *ptr );
		(void)putc( *ptr++, mer_fp );	/* straight binary */
	}
	(void)putc( END_DATAGRAM, mer_fp );
	/* VMS sends 20 pad nulls each time.  As we are in CBREAK mode
	 * and honoring flow control, we won't bother.
	 */
	if(mer_debug>=3) (void)printf("\n");
	fflush( mer_fp );
}

/*
 *  			C K _ I N P U T
 *
 *  This routine is called to explicitly check for input from the Merlin,
 *  waiting at most "tim" seconds for it to arrive.
 *  However, no input is actually read.  The return is 0 if there is nothing
 *  to read, and !0 if something is ready to read.
 *
 *  The ususal usage is:
 *	if( ck_input(tim) )  cp = eat_chars();
 */
ck_input(tim)
{
	struct timeval tv;
	long ibits;
	register int i;

	tv.tv_sec = tim;
	tv.tv_usec = 0;

	ibits = 1<<input_fd;
	i = select( 32, &ibits, (char *)0, (char *)0, &tv );

	if( i < 0 ) {
		perror("select");
		return( 0 );
	}
	if( !ibits )
		return( 0 );

	return( 1 );
}

int in_packet = 0;
u_char *packetp;
u_char packetbuf[4096];

/*
 *  			E A T _ C H A R S
 *
 *  This routine is called whenever select() has indicated that there
 *  are characters from the Merlin waiting to be read.
 *  The return is the return of Dlink_Receive (indicating that
 *  there is application data to be read), else return is 0.
 *
 *  Note that the length of the data availible is stashed in
 *  Message_data_len by Message_Receive().
 */
u_char *
eat_chars()
{
	u_char buf[4096];
	u_char *ret;		/* temp storage for return code */
	register u_char *cp;
	register int i;

	i = read( input_fd, buf, sizeof(buf) );
	if( i <= 0 )  {
		(void)printf("EOF on Merlin?\n");
		return( (u_char *)0 );
	}
	ret = (u_char *)0;
	for( cp=buf; cp < &buf[i]; cp++ )  {
		if( in_packet )  {
			/* Collecting packet */
			if( *cp == END_DATAGRAM )  {
				/* End of packet */
				in_packet = 0;
				*packetp = '\0';
				/* Handoff to input routine */
				ret = Dlink_Receive( packetbuf, packetp-packetbuf );
				continue;
			}
			if( *cp < '0' || *cp > 'F' || (*cp > '9' && *cp < 'A') )  {
				(void)printf("noise in packet: x%x\n", *cp );
				in_packet = 0;
				continue;
			}
			if( *cp > '9' )
				*cp -= 'A' - 10;
			else
				*cp -= '0';
			/* Hex digits presented lhs, rhs */
			if( in_packet == 1 )  {
				*packetp = *cp<<4;	/* lhs */
				in_packet = 2;
			} else {
				*packetp++ |= *cp;	/* rhs */
				in_packet = 1;
			}
			continue;
		}
		switch( *cp )  {
		case ENTER_DATAGRAM:
			/* Start of Packet */
			in_packet = 1;
			packetp = packetbuf;
			continue;
		case 0x11:	/* ^Q */
		case 0x13:	/* ^S */
			(void)printf("?flow control?\n");
			continue;
		default:
			(void)printf("Terminal char '%c'\n", *cp );
		}
	}
	return( ret );
}

/*
 *  			D L I N K _ R E C E I V E
 *
 *  Called bye eat_chars() whenever a full link-level packet has been
 *  read.  All link level encapsulation has been stripped before this
 *  routine is called.
 *
 *  Returns the return of Message_Receive(), otherwise returns 0,
 *  meaning no application data contained in this packet.
 */
u_char *
Dlink_Receive( ptr, count )
u_char *ptr;
int count;
{
	register int i;
	register u_char *cp;

	if(mer_debug>=2) (void)printf("Dlink_Receive %d: ", count);
	for( cp=ptr, i=0; i < count; i++ )  {
		if(mer_debug>=2) (void)printf("%2.2x ", *cp++ );
	}
	if(mer_debug>=2) (void)printf("\n");

	switch( *ptr )  {
	case 0xE0:
		/* Connection Request */
		(void)printf("Got Connection Request?\n");
		break;
	case 0xD0:
		/* Connection Confirm */
		if( Network_state == NET_STATE_OPENING )  {
			/* Could check dest_ref = net_local */
			net_remote = (ptr[4]<<8)|ptr[3];	/* src */
			Network_state = NET_STATE_OPEN;
			if(mer_debug>=3) (void)printf("Got Connection Confirm, net_remote=%d\n",net_remote);
		} else {
			(void)printf("Unexpected Connection Confirm\n");
			Network_Error();
		}
		break;
	case 0x80:
		/* Disconnect Request */
		(void)printf("Got Disconnect Request, reason=x%x (%s)\n",
			ptr[5], Net_DRreasons[ptr[5]] );
		Network_Confirm_Disconnect(ptr[3],ptr[4],ptr[1],ptr[2]);
		Network_state = NET_STATE_CLOSED;
		break;
	case 0xC0:
		/* Disconnect Confirm */
		if( Network_state == NET_STATE_CLOSING )  {
			Network_state = NET_STATE_CLOSED;
			if(mer_debug>=3) (void)printf("Got Disconnnect Confirm\n");
		} else {
			(void)printf("Unexpected Disconnect Confirm\n");
			Network_Error();
		}
		break;
	case 0xF0:
		/* Data */
		/* Ignore dest_ref */
		return( Message_Receive( ptr+5, (ptr[4]<<8)|ptr[3] ) );
	case 0x0F:
		/* Discovery */
		(void)printf("Discovery type=x%x, protocol=x%x\n", ptr[1], ptr[2]);
		break;
	case 0x0E:
		/* Suspend */
		(void)printf("??Suspend Request??\n");
		break;
	case 0x0D:
		/* Suspend Confirm */
		if(mer_debug>=3) (void)printf("Suspend Confirm\n");
		break;
	case 0x0C:
		/* Resume */
		(void)printf("??Resume Request??\n");
		Network_Error();
		break;
	default:
		(void)printf("Unrecognized NPDU of x%x\n", *ptr );
		Network_Error();
		break;
	}
	return( (u_char *)0 );
}

Network_Error()
{
	/* Send Disconnect Request, reason='Protocol Error' */
	Network_Disconnect( 3 );
	/* Wait for Disconnect Confirm */
}

Network_Confirm_Disconnect(d0,d1,s0,s1)
{
	u_char buf[12];

	if(mer_debug>=3) (void)printf("Network_Confirm_Disconnect\n");
	buf[0] = 0xC0;		/* Disconnect Confirm DC_NPDU */
	buf[1] = d0;
	buf[2] = d1;
	buf[3] = s0;
	buf[4] = s1;
	Dlink_Send( buf, 5 );
}

/*********** Database Editor Interfaces ******** */
#define GET_LONG(p)	(((p)[3]<<24)|((p)[2]<<16)|((p)[1]<<8)|*(p))
#define GET_SHORT(p)	(((p)[1]<<8)|*(p))

/* 4.2 General Control Commands */
EdInit()  {
	op1(0xAF);
}

EdClear()  {
	op1(0xFE);
}

EdPort(p)  {
	op1(0x81);
	op1( p & 0x0F );
}

EdOpen( name, cntrl, len, misc )
long name;
{
	op1( 0x02 );
	op4( name );
	op1( cntrl );
	op2( len );
	op2( misc );
}

end_insert_mode()  {
	register int len = db_next - DB_START - 3;
	if( len <= 0 )  {
		/* Nothing to insert */
		db_next = DB_START;	/* reset to front */
	} else {
		if( len > DB_SIZE )
			(void)printf("EdClose: buffer overflow\n");
		if( DB_START[0] != 0x21 )
			(void)printf("EdClose: EdInsert botch\n");
		DB_START[1] = len & 0xFF;
		DB_START[2] = (len>>8) & 0xFF;
	}
	insert_mode = 0;
}
EdClose()
{
	if( insert_mode )
		end_insert_mode();
	op1( 0x11 );		/* Close Named Entity */
}

/* 4.3 Insertion Commands */
EdInsert()  {
	edflush();	/* flush out all accumulated Ed* commands */

	/* Start Insert mode collection, ALWAYS AT FRONT OF BUFFER! */
	if( insert_mode )
		(void)printf("EdInsert: Already Inserting!\n");
	insert_mode = 1;
	op1( 0x21 );
	op2( 1 );	/* LENGTH, filled in later by EdClose() */
}

/* 4.4 Traversal Control Commands */
EdDisplay( name )
long name;
{
	op1( 0xA1 );
	op4( name );
}
EdDisplayRootList()  {
	op1( 0xA3 );
}

/* 4.5 Editor Command Prefixes */
EdRepeat( cnt )  {
	op1( 0x82 );
	op2( cnt );
}

EdUnconditional()  {
	op1( 0x84 );
}
/* 4.6 finds */
/* 4.7 deletes.  Names are intentionally made hard to type */
EdDeleteOpenEntity()  {
	op1( 0x45 );
}
EdDeleteAtomicEntity()  {
	op1( 0x44 );
}
EdDeleteGroupEntity()  {
	op1( 0x46 );
}
EdDeleteNamedEntity(name)
long name;
{
	op1( 0x48 );
	op4( name );
}
EdDeleteAllNamedEntities()  {
	op1( 0x41 );
}
EdDeleteIsolatedNamedEntities()  {
	op1( 0x42 );
	/* The book shows a name parameter here?  EH? */
}
EdDeleteAllUnreferencedAndSubtree()  {
	op1( 0x47 );
}
EdDeleteNamedEntityAndSubtree(name)
long name;
{
	op1( 0x49 );
	op4( name );
}

/* 4.8 cut and paste commands */
/* 4.9 Modify database and current edit position */

/* 4.10 List and Upload commands */
long
EdUploadNamedEntity(iname, buf)
long iname;
register u_char *buf;
{
	register u_char *cp;
	register long left;
	long name, len;

	op1( 0xBA );
	op4( iname );
	op1( 1 );		/* format */
	edflush();
	/* The name and length come back in the first message. */
	/* The actual data is sent along in subsequent messages. */
	if( !ck_input(1) || (cp = eat_chars()) == (u_char *)0 )  {
		printf("EdUploadNamedEntity:  no header\n");
		return(0L);
	}
	name = GET_LONG(cp);
	len = GET_LONG(cp+4);
	printf("name x%x got x%x, len=x%x\n", iname, name, len );
	left = len;
	while( left > 0 )  {
		if( !ck_input(1) || (cp = eat_chars()) == (u_char *)0 )  {
			printf("EdUploadNamedEntity:  short reply\n");
			return(0L);
		}
		bcopy( cp, buf, Message_data_len );
		buf += Message_data_len;
		left -= Message_data_len;
	}
	return(len);
}

/* 4.11 Miscellaneous Commands */
EdNoop()  {
	op1( 0x00 );
}

long
EdEnquireMem()  {
	register u_char *cp;
	long total, unalloc;
	int percent;

	op1( 0xD1 );
	edflush();
	if( ck_input(1) )  {
		cp = eat_chars();
		if( cp == (u_char *)0 )
			return( 0L );		/* Assume none */
		total = GET_LONG(cp);
		unalloc = GET_LONG(cp+4);
		percent = GET_SHORT(cp+8);
		(void)printf("Memory status:  %x/%x free (%d%%)\n", unalloc, total, percent);
		return(unalloc);
	}
	return( 0L );
}

/********* Database Elements ******* */
/* Variable Data Type */
#define VDT_NOOP	0x00
#define VDT_DPI		0x01	/* Double Precision Integer, 32-bits */
#define VDT_SPFP	0x02	/* Single Precision Float, 32 bits */
#define VDT_DPFP	0x03	/* Double Precision Float, 64 bits */

/* Data Element and Structure Commands */
DbBlock( count, opcode )  {
	db1( 0x0E );
	db2( count );
	db1( opcode );
}

/* Database structure commands */
DbBegin( type ) {
	db1( 0x0C );
	db1( type );
}

DbEnd()  {
	db1( 0x0D );
}
DbRefer(name)
long name;
{
	db1( 0x09 );
	db4( name );
}

/* Graphics Primitives Commands */
DbSeparator()  {
	db1( 0x28 );
}

DbClosure()  {
	db1( 0x29 );
}

/* Geometeric Data (integer only, for now ) */
DbPos2A( x, y )
{
	db1( 0x30 );
	db2( x );
	db2( y );
}
DbPos4A( x, y, z, w )
{
	db1( 0x38 );
	db2( x );
	db2( y );
	db2( z );
	db2( w );
}
/* Graphic Transformations */
DbReplaceMatrix()  {
	db1( 0x64 );
}
DbXRot( a )  {
	db1( 0x5C );
	db2( a );
}
DbYRot( a )  {
	db1( 0x5D );
	db2( a );
}
DbZRot( a )  {
	db1( 0x5E );
	db2( a );
}
DbScale( s, w )  {
	db1( 0x60 );
	db2( s );
	db2( w );
}

Db4x4Matrix( m )
register mat_t m;
{
	op1( 0x65 );
/* 256.0 */
#define CV(x)	((int)(m[x] * 128.0))
	op2( CV(0) );
	op2( CV(4) );
	op2( CV(8) );
	op2( CV(12) );

	op2( CV(1) );
	op2( CV(5) );
	op2( CV(9) );
	op2( CV(13) );

	op2( CV(2) );
	op2( CV(6) );
	op2( CV(10) );
	op2( CV(14) );

	op2( CV(3) );
	op2( CV(7) );
	op2( CV(11) );
	op2( CV(15) );
#undef CV
}
DbCorrectAspectRatio(mode) {
	if( !mode )  db1( 0x58 );	/* shrink picture to fit */
	else  db1( 0x59 );		/* enlarge picture */
}
DbCaptureTransformationState( name )
long name;
{
	db1( 0x6F );
	db4( name );
}

/* Section 6: Graphic Attributes */
DbVisibilityEnableAll()  {
	db1( 0x93 );
}
DbMaskedVisibilityDisable(mask)  {
	db1( 0x94 );
	db2( mask );
}
DbMaskedVisibilityEnable(mask)  {
	db1( 0x95 );
	db2( mask );
}
DbColorSelectAll( c )  {
	db1( 0x96 );
	db2( c );
}
DbHueSelectAll( h )  {
	db1

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
