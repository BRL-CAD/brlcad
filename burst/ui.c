/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <Sc/Sc.h>
#include <machine.h>
#include <vmath.h>
#include <raytrace.h>
#include "./Mm.h"
#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"
#define DEBUG_UI	true
static char	promptbuf[LNBUFSZ];
static char	*bannerp = "BURST (%s)";
static char	*pgmverp = "$Revision$";

#define AddCmd( nm, f )\
	{	Trie	*p;\
	if( (p = addTrie( nm, &cmdtrie )) == TRIE_NULL )\
		prntScr( "BUG: addTrie(%s) returned NULL.", nm );\
	else	p->l.t_func = f;\
	}

#define GetVar( var, ptr, conv )\
	{\
	if( ! batchmode )\
		{ int	items;\
		(void) sprintf( (ptr)->buffer, (ptr)->fmt, var*conv );\
		(void) getInput( ptr );\
		if( (items = sscanf( (ptr)->buffer, (ptr)->fmt, &(var) )) != 1 )\
			{\
			(void) strcpy( (ptr)->buffer, "" );\
			return;\
			}\
		(ptr)++;\
		}\
	else\
		{	char	*tokptr = strtok( cmdptr, WHITESPACE );\
		if(	tokptr == NULL\
		    ||	sscanf( tokptr, (ptr)->fmt, &(var) ) != 1 )\
			{\
			rt_log( "ERROR -- command syntax:\n" );\
			rt_log( "\t%s\n", cmdbuf );\
			rt_log( "\targument (%s) is of wrong type\n",\
				tokptr == NULL ? "(null)" : tokptr );\
			}\
		cmdptr = NULL;\
		}\
	}

typedef struct
	{
	char	*prompt;
	char	buffer[LNBUFSZ];
	char	*fmt;
	char	*range;
	}
Input;

/* local menu functions, names all start with M */
_LOCAL_ void	MattackDir();
_LOCAL_ void	MautoBurst();
_LOCAL_ void	MburstArmor();
_LOCAL_ void	MburstAir();
_LOCAL_ void	MburstDist();
_LOCAL_ void	MburstFile();
_LOCAL_ void	McellSize();
_LOCAL_ void	McolorFile();
_LOCAL_ void	MconeHalfAngle();
_LOCAL_ void	McritComp();
_LOCAL_ void	MdeflectSpallCone();
_LOCAL_ void	Mdither();
_LOCAL_ void	MenclosePortion();
_LOCAL_ void	MencloseTarget();
_LOCAL_ void	MerrorFile();
_LOCAL_ void	Mexecute();
_LOCAL_ void	MfbFile();
_LOCAL_ void	MgedFile();
_LOCAL_	void	MgridFile();
_LOCAL_ void	MhistFile();
_LOCAL_ void	Minput2dShot();
_LOCAL_ void	Minput3dShot();
_LOCAL_ void	Minput3dBurst();
_LOCAL_ void	Mnop();
_LOCAL_ void	Mobjects();
_LOCAL_ void	Moverlaps();
_LOCAL_ void	MplotFile();
_LOCAL_ void	Mread2dShotFile();
_LOCAL_ void	Mread3dShotFile();
_LOCAL_ void	MreadCmdFile();
_LOCAL_ void	MmaxSpallRays();
_LOCAL_ void	Munits();
_LOCAL_ void	MwriteCmdFile();

/* local utility functions */
_LOCAL_ HmMenu	*addMenu();
_LOCAL_ int	getInput();
_LOCAL_ int	unitStrToInt();
_LOCAL_ void	addItem();
_LOCAL_ void	banner();

typedef struct ftable	Ftable;
struct ftable
	{
	char	*name;
	char	*help;
	Ftable	*next;
	Func	*func;
	};

Ftable	shot2dmenu[] =
	{
	{ "read-2d-shot-file",
		"input shotline coordinates from file",
		0, Mread2dShotFile },
	{ "input-2d-shot",
		"type in shotline coordinates",
		0, Minput2dShot },
	{ "execute",
		"begin ray tracing", 0, Mexecute },
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
	{ "execute",
		"begin ray tracing", 0, Mexecute },
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
	{ 0 }
	};

Ftable	burstlocmenu[] =
	{
	{ "burst-coordinates",
		"specify each burst point in target coordinates (3-d)",
		0, Minput3dBurst },
	{ "ground plane",
		"burst on impact with ground plane",
		0, Mnop },
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
		"save input up to this point in a file", 0, MwriteCmdFile },
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
	{ "execute",
		"begin ray tracing", 0, Mexecute },
	{ "preferences",
		"options for tailoring behavior of user interface",
		prefmenu, 0 },
	{ 0 }
	};

_LOCAL_ void
addItem( tp, itemp )
Ftable *tp;
HmItem	*itemp;
	{
	itemp->text = tp->name;
	itemp->help = tp->help;
	itemp->next = addMenu( tp->next );
	itemp->dfn = 0;
	itemp->bfn = 0;
	itemp->hfn = tp->func;
	return;
	}

_LOCAL_ HmMenu *
addMenu( tp )
Ftable	*tp;
	{	register HmMenu	*menup;
		register HmItem	*itemp;
		register Ftable	*ftp = tp;
		register int	cnt;
		register bool	done = false;
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
        void    banner( void )

        Display program name and version on one line with BORDER_CHRs
		to border the top of the scrolling region.
 */
_LOCAL_ void
banner()
        {       register int    co;
                register char   *p;
        (void) ScMvCursor( MENU_LFT, BORDER_Y );
        (void) sprintf(	scrbuf,
			bannerp,
			pgmverp[0] == '%' ? "EXP" : pgmverp );
        for( co = 1; co <= 3; co++ )
                (void) putc( BORDER_CHR, stdout );
        for( p = scrbuf; co <= ScCO && *p != '\0'; co++, p++ )
                (void) putc( (int)(*p), stdout );
        for( ; co <= ScCO; co++ )
                (void) putc( BORDER_CHR, stdout );
        return;
        }

void
closeUi()
	{
	ScMvCursor( 1, ScLI );
	return;
	}

_LOCAL_ int
getInput( ip )
Input *ip;
	{
	if( ! batchmode )
		{	register int    c;
			register char   *p;
			char		*defaultp = ip->buffer;
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
		/* In case user hit CR only, do not disturb buffer.    */
		if( p != ip->buffer )
			*p = '\0';
		prompt( (char *) NULL );
		}
	else
		if( sscanf( cmdptr, "%s", ip->buffer ) != 1 )
			return	false;
	return  true;
	}

/*
	void initCmds( void )

	Initialize the keyword commands.
 */
_LOCAL_ void
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
_LOCAL_ void
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
			prntScr( "This terminal has no scroll region or delete line capability." );
			return  false;
			}
		(void) ScClrText(); /* Wipe screen. */
		HmInit( MENU_LFT, MENU_TOP, MENU_MAXVISITEMS );
		banner();
		}
	initMenus( mainmenu );
	initCmds( mainmenu );
	return	true;
	}

_LOCAL_ int
unitStrToInt( str )
char	*str;
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
_LOCAL_ void
MattackDir( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Attack azimuth", "", "%lf", "degrees" },
			{ "Attack elevation", "", "%lf", "degrees" },
			};
		register Input	*ip = input;
	GetVar( viewazim, ip, DEGRAD );
	GetVar( viewelev, ip, DEGRAD );
	(void) sprintf( scrbuf, "%s\t%g %g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			viewazim, viewelev );
	logCmd( scrbuf );
	viewazim /= DEGRAD;
	viewelev /= DEGRAD;
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MautoBurst( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Burst along shotline", "n", "%d", "y or n" },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		{
		if( ip->buffer[0] == 'y' )
			nriplevels = 1;
		else
		if( ip->buffer[0] == 'n' )
			nriplevels = 0;
		else
			{
			(void) sprintf( scrbuf,
					"Illegal input \"%s\".",
					ip->buffer );
			warning( scrbuf );
			return;
			}
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			nriplevels == 1 ? "yes" : "no" );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MburstAir( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of burst air file", "", "%s", 0 },
			};
		register Input	*ip = input;
		FILE		*airfp;
	if( getInput( ip ) )
		(void) strcpy( airfile, ip->buffer );
	else
		airfile[0] = NUL;
	if( (airfp = fopen( airfile, "r" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				airfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			airfile );
	logCmd( scrbuf );
	notify( "Reading burst air idents...", NOTIFY_APPEND );
	readIdents( &airids, airfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MburstArmor( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of burst armor file", "", "%s", 0 },
			};
		register Input	*ip = input;
		FILE		*armorfp;
	if( getInput( ip ) )
		(void) strcpy( armorfile, ip->buffer );
	else
		armorfile[0] = NUL;
	if( (armorfp = fopen( armorfile, "r" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				armorfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			armorfile );
	logCmd( scrbuf );
	notify( "Reading burst armor idents...", NOTIFY_APPEND );
	readIdents( &armorids, armorfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MburstDist( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Burst distance", "", "%lf", 0 },
			};
		register Input	*ip = input;
	GetVar( bdist, ip, 1.0 );
	(void) sprintf( scrbuf, "%s\t\t%g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			bdist );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MburstFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of burst output file", "", "%s", 0 },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		(void) strcpy( outfile, ip->buffer );
	else
		outfile[0] = NUL;
	if( (outfp = fopen( outfile, "w" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				outfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			outfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
McellSize( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Cell size", "", "%lf", 0 }
			};
		register Input	*ip = input;
	GetVar( cellsz, ip, unitconv );
	(void) sprintf( scrbuf, "%s\t\t%g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			cellsz );
	logCmd( scrbuf );
	cellsz /= unitconv;
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
McolorFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of ident-to-color mapping file",
				"", "%s", 0 },
			};
		register Input	*ip = input;
		FILE		*colorfp;
	if( getInput( ip ) )
		(void) strcpy( colorfile, ip->buffer );
	else
		colorfile[0] = NUL;
	if( (colorfp = fopen( colorfile, "r" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				colorfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			colorfile );
	logCmd( scrbuf );
	notify( "Reading ident-to-color mappings...", NOTIFY_APPEND );
	readColors( &colorids, colorfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MconeHalfAngle( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Cone angle", "", "%lf", "degrees" },
			};
		register Input	*ip = input;
	GetVar( conehfangle, ip, DEGRAD );
	(void) sprintf( scrbuf, "%s\t\t%g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			conehfangle );
	logCmd( scrbuf );
	conehfangle /= DEGRAD;
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
McritComp( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of critical component file", "", "%s", 0 },
			};
		register Input	*ip = input;
		FILE		*critfp;
	if( getInput( ip ) )
		(void) strcpy( critfile, ip->buffer );
	else
		critfile[0] = NUL;
	if( (critfp = fopen( critfile, "r" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Read access denied for \"%s\"",
				critfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			critfile );
	logCmd( scrbuf );
	notify( "Reading critical component idents...", NOTIFY_APPEND );
	readIdents( &critids, critfp );
	notify( NULL, NOTIFY_DELETE );
	return;
	}


/*ARGSUSED*/
_LOCAL_ void
MdeflectSpallCone( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Deflect cone", "n", "%d", "y or n" },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		{
		if( ip->buffer[0] == 'y' )
			deflectcone = true;
		else
		if( ip->buffer[0] == 'n' )
			deflectcone = false;
		else
			{
			(void) sprintf( scrbuf,
					"Illegal input \"%s\".",
					ip->buffer );
			warning( scrbuf );
			return;
			}
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			deflectcone ? "yes" : "no" );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Mdither( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Dither cells", "n", "%d", "y or n" },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		{
		if( ip->buffer[0] == 'y' )
			dithercells = true;
		else
		if( ip->buffer[0] == 'n' )
			dithercells = false;
		else
			{
			(void) sprintf( scrbuf,
					"Illegal input \"%s\".",
					ip->buffer );
			warning( scrbuf );
			return;
			}
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			dithercells ? "yes" : "no" );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MenclosePortion( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Left border of grid", "", "%lf", 0 },
			{ "Right border of grid", "", "%lf", 0 },
			{ "Bottom border of grid", "", "%lf", 0 },
			{ "Top border of grid", "", "%lf", 0 },
			};
		register Input	*ip = input;
	GetVar( gridlf, ip, unitconv );
	GetVar( gridrt, ip, unitconv );
	GetVar( griddn, ip, unitconv );
	GetVar( gridup, ip, unitconv );

	firemode = FM_PART;
	(void) sprintf( scrbuf,
			"%s\t\t%g %g %g %g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			gridlf, gridrt, griddn, gridup );
	logCmd( scrbuf );
	gridlf /= unitconv;
	gridrt /= unitconv;
	griddn /= unitconv;
	gridup /= unitconv;
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MencloseTarget( itemp )
HmItem	*itemp;
	{
	firemode = FM_GRID;
	(void) sprintf( scrbuf,
			"%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname );
	logCmd( scrbuf );
	return;
	}

_LOCAL_ void
MerrorFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of error output file", "", "%s", 0 },
			};
		register Input	*ip = input;
		static int	errfd = -1;
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
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			errfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Mexecute( itemp )
HmItem	*itemp;
	{	static bool	gottree = false;
		bool		loaderror = false;
	(void) sprintf( scrbuf,
			"%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname );
	logCmd( scrbuf );
	if( gedfile[0] == NUL )
		{
		warning( "No target file has been specified." );
		return;
		}
	notify( "Reading target data base...", NOTIFY_APPEND );
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
		{	char	*ptr, *obj;
		rt_prep_timer();
		for(	ptr = objects;
			(obj = strtok( ptr, WHITESPACE )) != NULL;
			ptr = NULL
			)
			{
			(void) sprintf( scrbuf, "Loading \"%s\"...", obj );
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
		notify( "Prepping solids...", NOTIFY_APPEND );
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
_LOCAL_ void
MfbFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of frame buffer device", "", "%s", 0 },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		(void) strcpy( fbfile, ip->buffer );
	else
		fbfile[0] = NUL;
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			fbfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MgedFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of target (MGED) file", "", "%s", 0 },
			};
		register Input	*ip = input;
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
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			gedfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MgridFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of grid file", "", "%s", 0 },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		(void) strcpy( gridfile, ip->buffer );
	else
		histfile[0] = NUL;
	if( (gridfp = fopen( gridfile, "w" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				gridfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			gridfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MhistFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of histogram file", "", "%s", 0 },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		(void) strcpy( histfile, ip->buffer );
	else
		histfile[0] = NUL;
	if( (histfp = fopen( histfile, "w" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				histfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			histfile );
	logCmd( scrbuf );
	return;
	}

_LOCAL_ void
Minput3dBurst( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Burst point (X)", "", "%lf", 0 },
			{ "Burst point (Y)", "", "%lf", 0 },
			{ "Burst point (Z)", "", "%lf", 0 },
			};
		register Input	*ip = input;
	GetVar( burstpoint[X], ip, unitconv );
	GetVar( burstpoint[Y], ip, unitconv );
	GetVar( burstpoint[Z], ip, unitconv );
	(void) sprintf( scrbuf, "%s\t%g %g %g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			burstpoint[X], burstpoint[Y], burstpoint[Z] );
	logCmd( scrbuf );
	burstpoint[X] /= unitconv; /* convert to millimeters */
	burstpoint[Y] /= unitconv;
	burstpoint[Z] /= unitconv;
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Minput2dShot( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Firing coordinate (X)", "", "%lf", 0 },
			{ "Firing coordinate (Y)", "", "%lf", 0 },
			};
		register Input	*ip = input;
	GetVar( fire[X], ip, unitconv );
	GetVar( fire[Y], ip, unitconv );
	(void) sprintf( scrbuf, "%s\t\t%g %g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			fire[X], fire[Y] );
	logCmd( scrbuf );
	fire[X] /= unitconv; /* convert to millimeters */
	fire[Y] /= unitconv;
	firemode = FM_SHOT;
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Minput3dShot( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Firing coordinate (X)", "", "%lf", 0 },
			{ "Firing coordinate (Y)", "", "%lf", 0 },
			{ "Firing coordinate (Z)", "", "%lf", 0 },
			};
		register Input	*ip = input;
		fastf_t		burstpoint[3];
	GetVar( fire[X], ip, unitconv );
	GetVar( fire[Y], ip, unitconv );
	GetVar( fire[Z], ip, unitconv );
	(void) sprintf( scrbuf, "%s\t\t%g %g %g",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			fire[X], fire[Y], fire[Z] );
	logCmd( scrbuf );
	fire[X] /= unitconv; /* convert to millimeters */
	fire[Y] /= unitconv;
	fire[Z] /= unitconv;
	firemode = FM_SHOT | FM_3DIM;
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Mnop( itemp )
HmItem	*itemp;
	{
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Mobjects( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "List of objects from target file", "", "%s", 0 },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		(void) strcpy( objects, ip->buffer );
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			objects );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Moverlaps( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Report overlaps", "y", "%d", "y or n" },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		{
		if( ip->buffer[0] == 'y' )
			reportoverlaps = true;
		else
		if( ip->buffer[0] == 'n' )
			reportoverlaps = false;
		else
			{
			(void) sprintf( scrbuf,
					"Illegal input \"%s\".",
					ip->buffer );
			warning( scrbuf );
			return;
			}
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			reportoverlaps ? "yes" : "no" );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MmaxSpallRays( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Maximum rays per burst", "", "%d", 0 },
			};
		register Input	*ip = input;
	GetVar( nspallrays, ip, 1 );
	(void) sprintf( scrbuf, "%s\t\t%d",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			nspallrays );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MplotFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of UNIX plot file", "", "%s", 0 },
			};
		register Input	*ip = input;
	if( getInput( ip ) )
		(void) strcpy( plotfile, ip->buffer );
	else
		plotfile[0] = NUL;
	if( (plotfp = fopen( plotfile, "w" )) == (FILE *) NULL )
		{
		(void) sprintf( scrbuf,
				"Write access denied for \"%s\"",
				plotfile );
		warning( scrbuf );
		return;
		}
	(void) sprintf( scrbuf, "%s\t\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			plotfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Mread2dShotFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of 2-D shot input file", "", "%s", 0 },
			};
		register Input	*ip = input;
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
	firemode = FM_SHOT | FM_FILE;
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			shotfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
Mread3dShotFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of 3-D shot input file", "", "%s", 0 },
			};
		register Input	*ip = input;
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
	firemode = FM_SHOT | FM_FILE | FM_3DIM;
	(void) sprintf( scrbuf, "%s\t%s",
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			shotfile );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MreadCmdFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of command file", "", "%s", 0 },
			};
		register Input	*ip = input;
		char		cmdfile[LNBUFSZ];
		FILE		*cmdfp;
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

HmItem	units_items[] =
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
_LOCAL_ void
Munits( itemp )
HmItem	*itemp;
	{	char	*unitstr;
		HmItem	*itemptr;
	if( itemp != (HmItem *) 0 )
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
			itemp != (HmItem *) 0 ? itemp->text : cmdname,
			unitstr );
	logCmd( scrbuf );
	return;
	}

/*ARGSUSED*/
_LOCAL_ void
MwriteCmdFile( itemp )
HmItem	*itemp;
	{	static Input	input[] =
			{
			{ "Name of command file", "", "%s", 0 },
			};
		register Input	*ip = input;
		char		cmdfile[LNBUFSZ];
		FILE		*cmdfp;
		FILE		*inpfp;
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

#if defined( SYSV )
/*ARGSUSED*/
_LOCAL_ void
#else
_LOCAL_ int
#endif
intr_sig( sig )
int	sig;
	{	static Input	input[] =
			{
			{ "Really quit ? ", "n", "%d", "y or n" },
			};
		register Input	*ip = input;
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
#if defined( SYSV )
	return;
#else
	return	sig;
#endif
	}

void
logCmd( cmd )
char	*cmd;
	{
	prntScr( "%s", cmd );
	if( fprintf( tmpfp, "%s\n", cmd ) < 0 )
		{
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	else
		(void) fflush( tmpfp );
	return;
	}
