/*                            U I . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 *
 */
/** @file ui.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#include "./Sc.h"
#include "./Mm.h"
#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"


#define DEBUG_UI	0

static char promptbuf[LNBUFSZ];
static char *bannerp = "BURST (%s)";
static char *pgmverp = "2.2";

#define AddCmd( nm, f )\
	{	Trie	*p;\
	if( (p = addTrie( nm, &cmdtrie )) == TRIE_NULL )\
		prntScr( "BUG: addTrie(%s) returned NULL.", nm );\
	else	p->l.t_func = f;\
	}

#define GetBool( var, ptr ) \
	if( getInput( ptr ) ) \
		{ \
		if( ptr->buffer[0] == 'y' ) \
			var = true; \
		else \
		if( ptr->buffer[0] == 'n' ) \
			var = false; \
		else \
			{ \
			(void) sprintf( scrbuf, \
					"Illegal input \"%s\".", \
					ptr->buffer ); \
			warning( scrbuf ); \
			return; \
			} \
		(ptr)++; \
		}

#define GetVar( var, ptr, conv )\
	{\
	if( ! batchmode )\
		{\
		(void) sprintf( (ptr)->buffer, (ptr)->fmt, var*conv );\
		(void) getInput( ptr );\
		if( (sscanf( (ptr)->buffer, (ptr)->fmt, &(var) )) != 1 )\
			{\
			(void) strcpy( (ptr)->buffer, "" );\
			return;\
			}\
		(ptr)++;\
		}\
	else\
		{	char *tokptr = strtok( cmdptr, WHITESPACE );\
		if(	tokptr == NULL\
		    ||	sscanf( tokptr, (ptr)->fmt, &(var) ) != 1 )\
			{\
			brst_log( "ERROR -- command syntax:\n" );\
			brst_log( "\t%s\n", cmdbuf );\
			brst_log( "\tcommand (%s): argument (%s) is of wrong type, %s expected.\n",\
				cmdptr, tokptr == NULL ? "(null)" : tokptr,\
				(ptr)->fmt );\
			}\
		cmdptr = NULL;\
		}\
	}

typedef struct
	{
	char *prompt;
	char buffer[LNBUFSZ];
	char *fmt;
	char *range;
	}
Input;

/* local menu functions, names all start with M */
STATIC void MattackDir();
STATIC void MautoBurst();
STATIC void MburstArmor();
STATIC void MburstAir();
STATIC void MburstDist();
STATIC void MburstFile();
STATIC void McellSize();
STATIC void McolorFile();
STATIC void Mcomment();
STATIC void MconeHalfAngle();
STATIC void McritComp();
STATIC void MdeflectSpallCone();
STATIC void Mdither();
STATIC void MenclosePortion();
STATIC void MencloseTarget();
STATIC void MerrorFile();
STATIC void Mexecute();
STATIC void MfbFile();
STATIC void MgedFile();
STATIC void MgridFile();
STATIC void MgroundPlane();
STATIC void MhistFile();
STATIC void Minput2dShot();
STATIC void Minput3dShot();
STATIC void MinputBurst();
STATIC void MmaxBarriers();
STATIC void MmaxSpallRays();
STATIC void Mnop();
STATIC void Mobjects();
STATIC void Moverlaps();
STATIC void MplotFile();
STATIC void Mread2dShotFile();
STATIC void Mread3dShotFile();
STATIC void MreadBurstFile();
STATIC void MreadCmdFile();
STATIC void MshotlineFile();
STATIC void Munits();
STATIC void MwriteCmdFile();

/* local utility functions */
STATIC HmMenu *addMenu();
STATIC int getInput();
STATIC int unitStrToInt();
STATIC void addItem();
STATIC void banner();

typedef struct ftable Ftable;
struct ftable
	{
	char *name;
	char *help;
	Ftable *next;
	Func *func;
	};

Ftable	shot2dmenu[] =
	{
	{ "read-2d-shot-file",
		"input shotline coordinates from file",
		0, Mread2dShotFile },
	{ "input-2d-shot",
		"type in shotline coordinates",
		0, Minput2dShot },
	{ "execute", "begin ray tracing", 0, Mexecute },
	{ 0 },
	};

Ftable	shot3dmenu[] =
	{
	{ "read-3d-shot-file",
		"input shotline coordinates from file",
		0, Mread3dShotFile },
	{ "input-3d-shot",
		"type in shotline coordinates",
		0, Minput3dShot },
	{ "execute", "begin ray tracing", 0, Mexecute },
	{ 0 },
	};

Ftable	shotcoordmenu[] =
	{
	{ "target coordinate system",
		"specify shotline location in model coordinates (3-d)",
		shot3dmenu, 0 },
	{ "shotline coordinate system",
		"specify shotline location in attack coordinates (2-d)",
		shot2dmenu, 0 },
	{ 0 }
	};

Ftable	gridmenu[] =
	{
	{ "enclose-target",
		"generate a grid which covers the entire target",
		0, MencloseTarget },
	{ "enclose-portion",
		"generate a grid which covers a portion of the target",
		0, MenclosePortion },
	{ "execute", "begin ray tracing", 0, Mexecute },
	{ 0 }
	};

Ftable	locoptmenu[] =
	{
	{ "envelope",
		"generate a grid of shotlines", gridmenu, 0 },
	{ "discrete shots",
		"specify each shotline by coordinates", shotcoordmenu, 0 },
	{ 0 }
	};

Ftable	burstcoordmenu[] =
	{
	{ "read-burst-file",
		"input burst coordinates from file",
		0, MreadBurstFile },
	{ "burst-coordinates",
		"specify each burst point in target coordinates (3-d)",
		0, MinputBurst },
	{ "execute", "begin ray tracing", 0, Mexecute },
	{ 0 },
	};

Ftable	burstoptmenu[] =
	{
	{ "burst-distance",
		"fuzing distance to burst point from impact",
		0, MburstDist },
	{ "cone-half-angle",
		"degrees from spall cone axis to limit burst rays",
		0, MconeHalfAngle },
	{ "deflect-spall-cone",
		"spall cone axis perturbed halfway to normal direction",
		0, MdeflectSpallCone },
	{ "max-spall-rays",
		"maximum rays generated per burst point (ray density)",
		0, MmaxSpallRays },
	{ "max-barriers",
		"maximum number of shielding components along spall ray",
		0, MmaxBarriers },
	{ 0 }
	};

Ftable	burstlocmenu[] =
	{
	{ "burst point coordinates",
		"input explicit burst points in 3-d target coordinates",
		burstcoordmenu, 0 },
	{ "ground-plane",
		"burst on impact with ground plane",
		0, MgroundPlane },
	{ "shotline-burst",
		"burst along shotline on impact with critical components",
		0, MautoBurst },
	{ 0 }
	};

Ftable	burstmenu[] =
	{
	{ "bursting method",
		"choose method of creating burst points",
		burstlocmenu, 0 },
	{ "bursting parameters",
		"configure spall cone generation options",
		burstoptmenu, 0 },
	{ 0 }
	};

Ftable	shotlnmenu[] =
	{
	{ "attack-direction",
		"shotline orientation WRT target", 0, MattackDir },
	{ "cell-size",
		"shotline separation or coverage (1-D)", 0, McellSize },
	{ "dither-cells",
		"randomize location of shotline within grid cell",
		0, Mdither },
	{ "shotline location",
		"positioning of shotlines", locoptmenu, 0 },
	{ 0 }
	};

Ftable	targetmenu[] =
	{
	{ "target-file",
		"MGED data base file name", 0, MgedFile },
	{ "target-objects",
		"objects to read from MGED file", 0, Mobjects },
	{ "burst-air-file",
		"file containing space codes for triggering burst points",
		0, MburstAir },
	{ "burst-armor-file",
		"file containing armor codes for triggering burst points",
		0, MburstArmor },
	{ "critical-comp-file", "file containing critical component codes",
		0, McritComp },
	{ "color-file", "file containing component ident to color mappings",
		0, McolorFile },
	{ 0 }
	};

Ftable	prefmenu[] =
	{
	{ "report-overlaps",
		"enable or disable the reporting of overlaps",
		0, Moverlaps },
	{ 0 }
	};

Ftable	filemenu[] =
	{
	{ "read-input-file",
		"read commands from a file", 0, MreadCmdFile },
	{ "shotline-file",
		"name shotline output file", 0, MshotlineFile },
	{ "burst-file",
		"name burst point output file", 0, MburstFile },
	{ "error-file",
		"redirect error diagnostics to file", 0, MerrorFile },
	{ "histogram-file",
		"name file for graphing hits on critical components",
		0, MhistFile },
	{ "grid-file",
		"name file for storing grid points",
		0, MgridFile },
	{ "image-file",
		"name frame buffer device", 0, MfbFile },
	{ "plot-file",
		"name UNIX plot output file", 0, MplotFile },
	{ "write-input-file",
		"save input up to this point in a session file",
		0, MwriteCmdFile },
	{ 0 }
	};

Ftable	mainmenu[] =
	{
	{ "units",
		"units for input and output interpretation", 0, Munits },
	{ "project files",
		"set up input/output files for this analysis",
		filemenu, 0 },
	{ "target files",
		"identify target-specific input files", targetmenu, 0 },
	{ "shotlines",
		"shotline generation (grid specification)", shotlnmenu, 0 },
	{ "burst points",
		"burst point generation", burstmenu, 0 },
	{ CMD_COMMENT, "add a comment to the session file", 0, Mcomment },
	{ "execute", "begin ray tracing", 0, Mexecute },
	{ "preferences",
		"options for tailoring behavior of user interface",
		prefmenu, 0 },
	{ 0 }
	};

STATIC void
addItem( tp, itemp )
Ftable *tp;
HmItem *itemp;
	{
	itemp->text = tp->name;
	itemp->help = tp->help;
	itemp->next = addMenu( tp->next );
	itemp->dfn = 0;
	itemp->bfn = 0;
	itemp->hfn = tp->func;
	return;
	}

STATIC HmMenu *
addMenu( tp )
Ftable *tp;
	{	register HmMenu	*menup;
		register HmItem *itemp;
		register Ftable	*ftp = tp;
		register int cnt;
		register bool done = false;
	if( ftp == NULL )
		return	NULL;
	for( cnt = 0; ftp->name != NULL; ftp++ )
		cnt++;
	cnt++; /* Must include space for NULL entry. */
	menup = MmAllo( HmMenu );
	menup->item = MmVAllo( cnt, HmItem );
	menup->generator = 0;
	menup->prevtop = 0;
	menup->prevhit = 0;
	menup->sticky = true;
	/* menup->item should now be as long as tp. */
	for(	ftp = tp, itemp = menup->item;
		! done;
		ftp++, itemp++
		)
		{
		addItem( ftp, itemp );
		if( ftp->name == NULL ) /* Must include NULL entry. */
			done = true;
		}
	return	menup;
	}

/*
        void banner( void )

        Display program name and version on one line with BORDER_CHRs
	to border the top of the scrolling region.
 */
STATIC void
banner()
        {
        (void) sprintf(	scrbuf,	bannerp, pgmverp );
	HmBanner( scrbuf, BORDER_CHR );
        return;
        }

void
closeUi()
	{
	ScMvCursor( 1, ScLI );
	return;
	}

STATIC int
getInput( ip )
Input *ip;
	{
	if( ! batchmode )
		{	register int c;
			register char *p;
			char *defaultp = ip->buffer;
		if( *defaultp == NUL )
			defaultp = "no default";
		if( ip->range != NULL )
			(void) sprintf( promptbuf, "%s ? (%s)[%s] ",
					ip->prompt, ip->range, defaultp );
		else
			(void) sprintf( promptbuf, "%s ? [%s] ",
					ip->prompt, defaultp );
		prompt( promptbuf );
		for( p = ip->buffer; (c = HmGetchar()) != '\n'; )
			if( p - ip->buffer < LNBUFSZ-1 )
				*p++ = c;
		/* In case user hit CR only, do not disturb buffer. */
		if( p != ip->buffer )
			*p = '\0';
		prompt( (char *) NULL );
		}
	else
		{	char *str = strtok( cmdptr, WHITESPACE );
		if( str == NULL )
			return	false;
		(void) strcpy( ip->buffer, str );
		cmdptr = NULL;
		}
	return  true;
	}

/*
	void initCmds( void )

	Initialize the keyword commands.
 */
STATIC void
initCmds( tp )
register Ftable *tp;
	{
	for( ; tp->name != NULL; tp++ )
		{
		if( tp->next != NULL )
			initCmds( tp->next );
		else
			AddCmd( tp->name, tp->func );
		}
	return;
	}

/*
	void initMenus( void )

	Initialize the hierarchical menus.
 */
STATIC void
initMenus( tp )
register Ftable	*tp;
	{
	mainhmenu = addMenu( tp );
	return;
	}

bool
initUi()
	{
	if( tty )
		{
		if( ! ScInit( stdout ) )
			return	false;
 		if( ScSetScrlReg( SCROLL_TOP, SCROLL_BTM ) )
			(void) ScClrScrlReg();
		else
		if( ScDL == NULL )
			{
			prntScr(
		 "This terminal has no scroll region or delete line capability."
				);
			return  false;
			}
		(void) ScClrText();	/* wipe screen */
		HmInit( MENU_LFT, MENU_TOP, MENU_MAXVISITEMS );
		banner();
		}
	initMenus( mainmenu );
	initCmds( mainmenu );
	return	true;
	}

STATIC int
unitStrToInt( str )
char *str;
	{
	if( strcmp( str, UNITS_INCHES ) == 0 )
		return	U_INCHES;
	if( strcmp( str, UNITS_FEET ) == 0 )
		return	U_FEET;
	if( strcmp( str, UNITS_MILLIMETERS ) == 0 )
		return	U_MILLIMETERS;
	if( strcmp( str, UNITS_CENTIMETERS ) == 0 )
		return	U_CENTIMETERS;
	if( strcmp( str, UNITS_METERS ) == 0 )
		return	U_METERS;
	return	U_BAD;
	}

/*ARGSUSED*/
STATIC void
MattackDir( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Attack azimuth", "", "%lf", "degrees" },
			{ "Attack elevation", "", "%lf", "degrees" },
			};
		register Input *ip = input;
	GetVar( viewazim, ip, DEGRAD );
	GetVar( viewelev, ip, DEGRAD );
	(void) sprintf( scrbuf, "%s\t%g %g",
			itemp != NULL ? itemp->text : cmdname,
			viewazim, viewelev );
	logCmd( scrbuf );
	viewazim /= DEGRAD;
	viewelev /= DEGRAD;
	return;
	}

/*ARGSUSED*/
STATIC void
MautoBurst( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Burst along shotline", "n", "%d", "y or n" },
			{ "Require burst air", "y", "%d", "y or n" }
			};
		register Input *ip = input;
	GetBool( shotburst, ip );
	if( shotburst )
		{
		GetBool( reqburstair, ip );
		(void) sprintf( scrbuf, "%s\t\t%s %s",
				itemp != NULL ? itemp->text : cmdname,
				shotburst ? "yes" : "no",
				reqburstair ? "yes" : "no" );
		}
	else
		(void) sprintf( scrbuf, "%s\t\t%s",
				itemp != NULL ? itemp->text : cmdname,
				shotburst ? "yes" : "no" );
	logCmd( scrbuf );

	if( shotburst )
		firemode &= ~FM_BURST; /* disable discrete burst points */
	return;
	}

/*ARGSUSED*/
STATIC void
MburstAir( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of burst air file", "", "%s", 0 },
			};
		register Input *ip = input;
		FILE *airfp;
	if( getInput( ip ) )
		(void) strcpy( airfile, ip->buffer );
	else
		airfile[0] = NUL;
	if( (airfp = fopen( airfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				airfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			airfile );
	logCmd( scrbuf );
	notify( "Reading burst air idents", NOTIFY_APPEND );
	readIdents( &airids, airfp );
	(void) fclose( airfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}

/*ARGSUSED*/
STATIC void
MburstArmor( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of burst armor file", "", "%s", 0 },
			};
		register Input *ip = input;
		FILE *armorfp;
	if( getInput( ip ) )
		(void) strcpy( armorfile, ip->buffer );
	else
		armorfile[0] = NUL;
	if( (armorfp = fopen( armorfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				armorfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != NULL ? itemp->text : cmdname,
			armorfile );
	logCmd( scrbuf );
	notify( "Reading burst armor idents", NOTIFY_APPEND );
	readIdents( &armorids, armorfp );
	(void) fclose( armorfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}

/*ARGSUSED*/
STATIC void
MburstDist( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Burst distance", "", "%lf", 0 },
			};
		register Input *ip = input;
	GetVar( bdist, ip, unitconv );
	(void) sprintf( scrbuf, "%s\t\t%g",
			itemp != NULL ? itemp->text : cmdname,
			bdist );
	logCmd( scrbuf );
	bdist /= unitconv; /* convert to millimeters */
	return;
	}

/*ARGSUSED*/
STATIC void
MburstFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of burst output file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( outfile, ip->buffer );
	else
		outfile[0] = NUL;
	if( (outfp = fopen( outfile, "w" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				outfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			outfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
McellSize( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Cell size", "", "%lf", 0 }
			};
		register Input *ip = input;
	GetVar( cellsz, ip, unitconv );
	(void) sprintf( scrbuf, "%s\t\t%g",
			itemp != NULL ? itemp->text : cmdname,
			cellsz );
	logCmd( scrbuf );
	cellsz /= unitconv; /* convert to millimeters */
	return;
	}

/*ARGSUSED*/
STATIC void
McolorFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of ident-to-color mapping file",
				"", "%s", 0 },
			};
		register Input *ip = input;
		FILE *colorfp;
	if( getInput( ip ) )
		(void) strcpy( colorfile, ip->buffer );
	else
		colorfile[0] = NUL;
	if( (colorfp = fopen( colorfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				colorfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			colorfile );
	logCmd( scrbuf );
	notify( "Reading ident-to-color mappings", NOTIFY_APPEND );
	readColors( &colorids, colorfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}

/*ARGSUSED*/
STATIC void
Mcomment( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Comment", " ", "%s", 0 },
			};
		register Input *ip = input;
	if( ! batchmode )
		{
		if( getInput( ip ) )
			{
			(void) sprintf( scrbuf, "%c%s",
					CHAR_COMMENT, ip->buffer );
			logCmd( scrbuf );
			(void) strcpy( ip->buffer, " " ); /* restore default */
			}
		}
	else
		logCmd( cmdptr );
	return;
	}

/*ARGSUSED*/
STATIC void
MconeHalfAngle( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Cone angle", "", "%lf", "degrees" },
			};
		register Input *ip = input;
	GetVar( conehfangle, ip, DEGRAD );
	(void) sprintf( scrbuf, "%s\t\t%g",
			itemp != NULL ? itemp->text : cmdname,
			conehfangle );
	logCmd( scrbuf );
	conehfangle /= DEGRAD;
	return;
	}

/*ARGSUSED*/
STATIC void
McritComp( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of critical component file", "", "%s", 0 },
			};
		register Input *ip = input;
		FILE *critfp;
	if( getInput( ip ) )
		(void) strcpy( critfile, ip->buffer );
	else
		critfile[0] = NUL;
	if( (critfp = fopen( critfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				critfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != NULL ? itemp->text : cmdname,
			critfile );
	logCmd( scrbuf );
	notify( "Reading critical component idents", NOTIFY_APPEND );
	readIdents( &critids, critfp );
	(void) fclose( critfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}


/*ARGSUSED*/
STATIC void
MdeflectSpallCone( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Deflect cone", "n", "%d", "y or n" },
			};
		register Input *ip = input;
	GetBool( deflectcone, ip );
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != NULL ? itemp->text : cmdname,
			deflectcone ? "yes" : "no" );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
Mdither( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Dither cells", "n", "%d", "y or n" },
			};
		register Input *ip = input;
	GetBool( dithercells, ip );
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			dithercells ? "yes" : "no" );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MenclosePortion( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Left border of grid", "", "%lf", 0 },
			{ "Right border of grid", "", "%lf", 0 },
			{ "Bottom border of grid", "", "%lf", 0 },
			{ "Top border of grid", "", "%lf", 0 },
			};
		register Input *ip = input;
	GetVar( gridlf, ip, unitconv );
	GetVar( gridrt, ip, unitconv );
	GetVar( griddn, ip, unitconv );
	GetVar( gridup, ip, unitconv );
	(void) sprintf( scrbuf,
			"%s\t\t%g %g %g %g",
			itemp != NULL ? itemp->text : cmdname,
			gridlf, gridrt, griddn, gridup );
	logCmd( scrbuf );
	gridlf /= unitconv; /* convert to millimeters */
	gridrt /= unitconv;
	griddn /= unitconv;
	gridup /= unitconv;
	firemode = FM_PART;
	return;
	}

/*ARGSUSED*/
STATIC void
MencloseTarget( itemp )
HmItem *itemp;
	{
	(void) sprintf( scrbuf,
			"%s",
			itemp != NULL ? itemp->text : cmdname );
	logCmd( scrbuf );
	firemode = FM_GRID;
	return;
	}

STATIC void
MerrorFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of error output file", "", "%s", 0 },
			};
		register Input *ip = input;
		static int errfd = -1;
	if( getInput( ip ) )
		(void) strcpy( errfile, ip->buffer );
	else
		(void) strncpy( errfile, "/dev/tty", LNBUFSZ );
        /* insure that error log is truncated */
        if(     (errfd =
#ifdef BSD
                creat( errfile, 0644 )) == -1
#else
                open( errfile, O_TRUNC|O_CREAT|O_WRONLY, 0644 )) == -1
#endif
                )
                {
                locPerror( errfile );
                return;
                }
        (void) close( 2 );
        if( fcntl( errfd, F_DUPFD, 2 ) == -1 )
                {
                locPerror( "fcntl" );
                return;
                }
  	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			errfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
Mexecute( itemp )
HmItem *itemp;
	{	static bool	gottree = false;
		bool		loaderror = false;
	(void) sprintf( scrbuf,
			"%s",
			itemp != NULL ? itemp->text : cmdname );
	logCmd( scrbuf );
	if( gedfile[0] == NUL )
		{
		warning( "No target file has been specified." );
		return;
		}
	notify( "Reading target data base", NOTIFY_APPEND );
	rt_prep_timer();
	if(	rtip == RTI_NULL
	    && (rtip = rt_dirbuild( gedfile, title, TITLE_LEN ))
		     == RTI_NULL )
		{
		warning( "Ray tracer failed to read the target file." );
		return;
		}
	prntTimer( "dir" );
	notify( NULL, NOTIFY_DELETE );
	/* Add air into boolean trees, must be set after rt_dirbuild() and
                before rt_gettree().
	 */
	rtip->useair = true;
	if( ! gottree )
		{	char *ptr, *obj;
		rt_prep_timer();
		for(	ptr = objects;
			(obj = strtok( ptr, WHITESPACE )) != NULL;
			ptr = NULL
			)
			{
			(void) sprintf( scrbuf, "Loading \"%s\"", obj );
			notify( scrbuf, NOTIFY_APPEND );
			if( rt_gettree( rtip, obj ) != 0 )
				{
				(void) sprintf( scrbuf,
						"Bad object \"%s\".",
						obj );
				warning( scrbuf );
				loaderror = true;
				}
			notify( NULL, NOTIFY_DELETE );
			}
		gottree = true;
		prntTimer( "load" );
		}
	if( loaderror )
		return;
	if( rtip->needprep )
		{
		notify( "Prepping solids", NOTIFY_APPEND );
		rt_prep_timer();
		rt_prep( rtip );
		prntTimer( "prep" );
		notify( NULL, NOTIFY_DELETE );
		}
	gridInit();
	if( nriplevels > 0 )
		spallInit();
	(void) signal( SIGINT, abort_sig );
	gridModel();
	(void) signal( SIGINT, norml_sig );
	return;
	}

/*ARGSUSED*/
STATIC void
MfbFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of frame buffer device", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( fbfile, ip->buffer );
	else
		fbfile[0] = NUL;
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			fbfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MgedFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of target (MGED) file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( gedfile, ip->buffer );
	if( access( gedfile, 04 ) == -1 )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				gedfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			gedfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MgridFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of grid file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( gridfile, ip->buffer );
	else
		histfile[0] = NUL;
	if( (gridfp = fopen( gridfile, "w" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				gridfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			gridfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MgroundPlane( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Activate ground plane bursting",
				"n", "%d", "y or n" },
			{ "Distance of target origin above ground plane",
				"", "%lf", 0 },
			{ "Distance out positive X-axis of target to edge",
				"", "%lf", 0 },
			{ "Distance out negative X-axis of target to edge",
				"", "%lf", 0 },
			{ "Distance out positive Y-axis of target to edge",
				"", "%lf", 0 },
			{ "Distance out negative Y-axis of target to edge",
				"", "%lf", 0 },
			};
		register Input *ip = input;
	GetBool( groundburst, ip );
	if( groundburst )
		{
		GetVar( grndht, ip, unitconv );
		GetVar( grndfr, ip, unitconv );
		GetVar( grndbk, ip, unitconv );
		GetVar( grndlf, ip, unitconv );
		GetVar( grndrt, ip, unitconv );
		(void) sprintf( scrbuf, "%s\t\tyes %g %g %g %g %g",
				itemp != NULL ? itemp->text : cmdname,
				grndht, grndfr, grndbk, grndlf, grndrt );
		grndht /= unitconv; /* convert to millimeters */
		grndfr /= unitconv;
		grndbk /= unitconv;
		grndlf /= unitconv;
		grndrt /= unitconv;
		}
	else
		(void) sprintf( scrbuf, "%s\t\tno",
				itemp != NULL ? itemp->text : cmdname
				);
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MhistFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of histogram file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( histfile, ip->buffer );
	else
		histfile[0] = NUL;
	if( (histfp = fopen( histfile, "w" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				histfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			histfile );
	logCmd( scrbuf );
	return;
	}

STATIC void
MinputBurst( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "X-coordinate of burst point", "", "%lf", 0 },
			{ "Y-coordinate of burst point", "", "%lf", 0 },
			{ "Z-coordinate of burst point", "", "%lf", 0 },
			};
		register Input *ip = input;
	GetVar( burstpoint[X], ip, unitconv );
	GetVar( burstpoint[Y], ip, unitconv );
	GetVar( burstpoint[Z], ip, unitconv );
	(void) sprintf( scrbuf, "%s\t%g %g %g",
			itemp != NULL ? itemp->text : cmdname,
			burstpoint[X], burstpoint[Y], burstpoint[Z] );
	logCmd( scrbuf );
	burstpoint[X] /= unitconv; /* convert to millimeters */
	burstpoint[Y] /= unitconv;
	burstpoint[Z] /= unitconv;
	firemode = FM_BURST | FM_3DIM;
	return;
	}

/*ARGSUSED*/
STATIC void
Minput2dShot( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Y'-coordinate of shotline", "", "%lf", 0 },
			{ "Z'-coordinate of shotline", "", "%lf", 0 },
			};
		register Input *ip = input;
	GetVar( fire[X], ip, unitconv );
	GetVar( fire[Y], ip, unitconv );
	(void) sprintf( scrbuf, "%s\t\t%g %g",
			itemp != NULL ? itemp->text : cmdname,
			fire[X], fire[Y] );
	logCmd( scrbuf );
	fire[X] /= unitconv; /* convert to millimeters */
	fire[Y] /= unitconv;
	firemode = FM_SHOT;
	return;
	}

/*ARGSUSED*/
STATIC void
Minput3dShot( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "X-coordinate of shotline", "", "%lf", 0 },
			{ "Y-coordinate of shotline", "", "%lf", 0 },
			{ "Z-coordinate of shotline", "", "%lf", 0 },
			};
		register Input *ip = input;
	GetVar( fire[X], ip, unitconv );
	GetVar( fire[Y], ip, unitconv );
	GetVar( fire[Z], ip, unitconv );
	(void) sprintf( scrbuf, "%s\t\t%g %g %g",
			itemp != NULL ? itemp->text : cmdname,
			fire[X], fire[Y], fire[Z] );
	logCmd( scrbuf );
	fire[X] /= unitconv; /* convert to millimeters */
	fire[Y] /= unitconv;
	fire[Z] /= unitconv;
	firemode = FM_SHOT | FM_3DIM;
	return;
	}

/*ARGSUSED*/
STATIC void
Mnop( itemp )
HmItem *itemp;
	{
	return;
	}

/*ARGSUSED*/
STATIC void
Mobjects( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "List of objects from target file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( objects, ip->buffer );
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			objects );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
Moverlaps( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Report overlaps", "y", "%d", "y or n" },
			};
		register Input *ip = input;
	GetBool( reportoverlaps, ip );
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			reportoverlaps ? "yes" : "no" );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MmaxBarriers( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Maximum spall barriers per ray", "", "%d", 0 },
			};
		register Input *ip = input;
	GetVar( nbarriers, ip, 1 );
	(void) sprintf( scrbuf, "%s\t\t%d",
			itemp != NULL ? itemp->text : cmdname,
			nbarriers );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MmaxSpallRays( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Maximum rays per burst", "", "%d", 0 },
			};
		register Input *ip = input;
	GetVar( nspallrays, ip, 1 );
	(void) sprintf( scrbuf, "%s\t\t%d",
			itemp != NULL ? itemp->text : cmdname,
			nspallrays );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MplotFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of UNIX plot file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( plotfile, ip->buffer );
	else
		plotfile[0] = NUL;
	if( (plotfp = fopen( plotfile, "w" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				plotfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			plotfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
Mread2dShotFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of 2-D shot input file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( shotfile, ip->buffer );
	if( (shotfp = fopen( shotfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				shotfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != NULL ? itemp->text : cmdname,
			shotfile );
	logCmd( scrbuf );
	firemode = FM_SHOT | FM_FILE ;
	return;
	}

/*ARGSUSED*/
STATIC void
Mread3dShotFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of 3-D shot input file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( shotfile, ip->buffer );
	if( (shotfp = fopen( shotfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				shotfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != NULL ? itemp->text : cmdname,
			shotfile );
	logCmd( scrbuf );
	firemode = FM_SHOT | FM_FILE | FM_3DIM;
	return;
	}

/*ARGSUSED*/
STATIC void
MreadBurstFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of 3-D burst input file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( burstfile, ip->buffer );
	if( (burstfp = fopen( burstfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				burstfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			burstfile );
	logCmd( scrbuf );
	firemode = FM_BURST | FM_3DIM | FM_FILE ;
	return;
	}

/*ARGSUSED*/
STATIC void
MreadCmdFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of command file", "", "%s", 0 },
			};
		register Input *ip = input;
		char cmdfile[LNBUFSZ];
		FILE *cmdfp;
	if( getInput( ip ) )
		(void) strcpy( cmdfile, ip->buffer );
	if( (cmdfp = fopen( cmdfile, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				cmdfile );
		warning( scrbuf );
		return;
		}
	readBatchInput( cmdfp );
	(void) fclose( cmdfp );
	return;
	}

/*ARGSUSED*/
STATIC void
MshotlineFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of shotline output file", "", "%s", 0 },
			};
		register Input *ip = input;
	if( getInput( ip ) )
		(void) strcpy( shotlnfile, ip->buffer );
	else
		shotlnfile[0] = NUL;
	if( (shotlnfp = fopen( shotlnfile, "w" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				shotlnfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			shotlnfile );
	logCmd( scrbuf );
	return;
	}

HmItem units_items[] =
	{
	{ UNITS_MILLIMETERS,
		"interpret inputs and convert outputs to millimeters",
		0, 0, 0, Mnop },
	{ UNITS_CENTIMETERS,
		"interpret inputs and convert outputs to centimeters",
		0, 0, 0, Mnop },
	{ UNITS_METERS,
		"interpret inputs and convert outputs to meters",
		0, 0, 0, Mnop },
	{ UNITS_INCHES,
		"interpret inputs and convert outputs to inches",
		0, 0, 0, Mnop },
	{ UNITS_FEET,
		"interpret inputs and convert outputs to feet",
		0, 0, 0, Mnop },
	{ 0 }
	};
HmMenu	units_hmenu = { units_items, 0, 0, 0, false };

/*ARGSUSED*/
STATIC void
Munits( itemp )
HmItem *itemp;
	{	char *unitstr;
		HmItem *itemptr;
	if( itemp != NULL )
		{
		if( (itemptr = HmHit( &units_hmenu )) == (HmItem *) NULL )
			return;
		unitstr = itemptr->text;
		}
	else
		unitstr = strtok( cmdptr, WHITESPACE );
	units = unitStrToInt( unitstr );
	if( units == U_BAD )
		{
		(void) sprintf( scrbuf, "Illegal units \"%s\"", unitstr );
		warning( scrbuf );
		return;
		}
	switch( units )
		{
	case U_INCHES :
		unitconv = 3.937008e-02;
		break;
	case U_FEET :
		unitconv = 3.280840e-03;
		break;
	case U_MILLIMETERS :
		unitconv = 1.0;
		break;
	case U_CENTIMETERS :
		unitconv = 1.0e-01;
		break;
	case U_METERS :
		unitconv = 1.0e-03;
		break;
		}
	(void) sprintf( scrbuf, "%s\t\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			unitstr );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
STATIC void
MwriteCmdFile( itemp )
HmItem *itemp;
	{	static Input input[] =
			{
			{ "Name of command file", "", "%s", 0 },
			};
		register Input *ip = input;
		char cmdfile[LNBUFSZ];
		FILE *cmdfp;
		FILE *inpfp;
	if( getInput( ip ) )
		(void) strcpy( cmdfile, ip->buffer );
	if( (cmdfp = fopen( cmdfile, "w" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				cmdfile );
		warning( scrbuf );
		return;
		}
	if( (inpfp = fopen( tmpfname, "r" )) == NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				tmpfname );
		warning( scrbuf );
		(void) fclose( cmdfp );
		return;
		}
	while( fgets( scrbuf, LNBUFSZ, inpfp ) != NULL )
		fputs( scrbuf, cmdfp );
	(void) fclose( cmdfp );
	(void) fclose( inpfp );
	return;
	}

void
intr_sig( int sig )
	{	static Input input[] =
			{
			{ "Really quit ? ", "n", "%d", "y or n" },
			};
		register Input *ip = input;
	(void) signal( SIGINT, intr_sig );
	if( getInput( ip ) )
		{
		if( ip->buffer[0] == 'y' )
			exitCleanly( SIGINT );
		else
		if( ip->buffer[0] != 'n' )
			{
			(void) sprintf( scrbuf,
					"Illegal input \"%s\".",
					ip->buffer );
			warning( scrbuf );
			return;
			}
		}
	return;
	}

void
logCmd( cmd )
char *cmd;
	{
	prntScr( "%s", cmd ); /* avoid possible problems with '%' in string */
	if( fprintf( tmpfp, "%s\n", cmd ) < 0 )
		{
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	else
		(void) fflush( tmpfp );
	return;
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
