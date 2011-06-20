/*                            U I . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/ui.c
 *
 */

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "vmath.h"
#include "raytrace.h"

#include "./Sc.h"
#include "./Mm.h"
#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"


#define DEBUG_UI 0

static char promptbuf[LNBUFSZ];
static const char *bannerp = "BURST (2.2)";

#define AddCmd(nm, f) \
	{ \
		Trie *p; \
		if ((p = addTrie(nm, &cmdtrie)) == TRIE_NULL) \
			prntScr("BUG: addTrie(%s) returned NULL.", nm); \
		else \
			p->l.t_func = f; \
	}

#define GetBool(var, ptr) \
	if (getInput(ptr)) { \
		if (ptr->buffer[0] == 'y') \
			var = 1; \
		else \
		if (ptr->buffer[0] == 'n') \
			var = 0; \
		else { \
			(void) snprintf(scrbuf, LNBUFSZ, "Illegal input \"%s\".", ptr->buffer); \
			warning(scrbuf); \
			return; \
		} \
		(ptr)++; \
	}

#define GetVar(var, ptr, conv)\
	{\
		if (! batchmode) {\
			(void) snprintf((ptr)->buffer, LNBUFSZ, (ptr)->fmt, var*conv);\
			(void) getInput(ptr);\
			if ((sscanf((ptr)->buffer, (ptr)->fmt, &(var))) != 1) {\
				bu_strlcpy((ptr)->buffer, "", LNBUFSZ);\
				return;\
			}\
			(ptr)++;\
		} else {\
			char *tokptr = strtok(cmdptr, WHITESPACE);\
			if (tokptr == NULL || sscanf(tokptr, (ptr)->fmt, &(var)) != 1) {\
				brst_log("ERROR -- command syntax:\n");\
				brst_log("\t%s\n", cmdbuf);\
				brst_log("\tcommand (%s): argument (%s) is of wrong type, %s expected.\n", \
						cmdptr, tokptr == NULL ? "(null)" : tokptr, \
						(ptr)->fmt);\
			}\
			cmdptr = NULL;\
		}\
	}

typedef struct {
    char *prompt;
    char buffer[LNBUFSZ];
    char *fmt;
    char *range;
} Input;

/* local menu functions, names all start with M */
static void MattackDir();
static void MautoBurst();
static void MburstArmor();
static void MburstAir();
static void MburstDist();
static void MburstFile();
static void McellSize();
static void McolorFile();
static void Mcomment();
static void MconeHalfAngle();
static void McritComp();
static void MdeflectSpallCone();
static void Mdither();
static void MenclosePortion();
static void MencloseTarget();
static void MerrorFile();
static void Mexecute();
static void MfbFile();
static void MgedFile();
static void MgridFile();
static void MgroundPlane();
static void MhistFile();
static void Minput2dShot();
static void Minput3dShot();
static void MinputBurst();
static void MmaxBarriers();
static void MmaxSpallRays();
static void Mnop();
static void Mobjects();
static void Moverlaps();
static void MplotFile();
static void Mread2dShotFile();
static void Mread3dShotFile();
static void MreadBurstFile();
static void MreadCmdFile();
static void MshotlineFile();
static void Munits();
static void MwriteCmdFile();

/* local utility functions */
static HmMenu *addMenu();
static int getInput();
static int unitStrToInt();
static void addItem();
static void banner();

typedef struct ftable Ftable;
struct ftable
{
    char *name;
    char *help;
    Ftable *next;
    Func *func;
};


Ftable shot2dmenu[] =
{
    { "read-2d-shot-file",
      "input shotline coordinates from file",
      0, Mread2dShotFile },
    { "input-2d-shot",
      "type in shotline coordinates",
      0, Minput2dShot },
    { "execute", "begin ray tracing", 0, Mexecute },
    { NULL, NULL, 0, NULL },
};


Ftable shot3dmenu[] =
{
    { "read-3d-shot-file",
      "input shotline coordinates from file",
      0, Mread3dShotFile },
    { "input-3d-shot",
      "type in shotline coordinates",
      0, Minput3dShot },
    { "execute", "begin ray tracing", 0, Mexecute },
    { NULL, NULL, 0, NULL },
};


Ftable shotcoordmenu[] =
{
    { "target coordinate system",
      "specify shotline location in model coordinates (3-d)",
      shot3dmenu, 0 },
    { "shotline coordinate system",
      "specify shotline location in attack coordinates (2-d)",
      shot2dmenu, 0 },
    { NULL, NULL, 0, NULL },
};


Ftable gridmenu[] =
{
    { "enclose-target",
      "generate a grid which covers the entire target",
      0, MencloseTarget },
    { "enclose-portion",
      "generate a grid which covers a portion of the target",
      0, MenclosePortion },
    { "execute", "begin ray tracing", 0, Mexecute },
    { NULL, NULL, 0, NULL },
};


Ftable locoptmenu[] =
{
    { "envelope",
      "generate a grid of shotlines", gridmenu, 0 },
    { "discrete shots",
      "specify each shotline by coordinates", shotcoordmenu, 0 },
    { NULL, NULL, 0, NULL },
};


Ftable burstcoordmenu[] =
{
    { "read-burst-file",
      "input burst coordinates from file",
      0, MreadBurstFile },
    { "burst-coordinates",
      "specify each burst point in target coordinates (3-d)",
      0, MinputBurst },
    { "execute", "begin ray tracing", 0, Mexecute },
    { NULL, NULL, 0, NULL },
};


Ftable burstoptmenu[] =
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
    { NULL, NULL, 0, NULL },
};


Ftable burstlocmenu[] =
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
    { NULL, NULL, 0, NULL },
};


Ftable burstmenu[] =
{
    { "bursting method",
      "choose method of creating burst points",
      burstlocmenu, 0 },
    { "bursting parameters",
      "configure spall cone generation options",
      burstoptmenu, 0 },
    { NULL, NULL, 0, NULL },
};


Ftable shotlnmenu[] =
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
    { NULL, NULL, 0, NULL },
};


Ftable targetmenu[] =
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
    { NULL, NULL, 0, NULL },
};


Ftable prefmenu[] =
{
    { "report-overlaps",
      "enable or disable the reporting of overlaps",
      0, Moverlaps },
    { NULL, NULL, 0, NULL },
};


Ftable filemenu[] =
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
    { NULL, NULL, 0, NULL },
};


Ftable mainmenu[] =
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
    { NULL, NULL, 0, NULL },
};


static void
addItem(Ftable *tp, HmItem *itemp)
{
    itemp->text = tp->name;
    itemp->help = tp->help;
    itemp->next = addMenu(tp->next);
    itemp->dfn = 0;
    itemp->bfn = 0;
    itemp->hfn = tp->func;
    return;
}


static HmMenu *
addMenu(Ftable *tp)
{
    HmMenu *menup;
    HmItem *itemp;
    Ftable *ftp = tp;
    int cnt;
    int done = 0;
    if (ftp == NULL)
	return NULL;
    for (cnt = 0; ftp->name != NULL; ftp++)
	cnt++;
    cnt++; /* Must include space for NULL entry. */
    menup = MmAllo(HmMenu);
    menup->item = MmVAllo(cnt, HmItem);
    menup->generator = 0;
    menup->prevtop = 0;
    menup->prevhit = 0;
    menup->sticky = 1;
    /* menup->item should now be as long as tp. */
    for (ftp = tp, itemp = menup->item;
	 ! done;
	 ftp++, itemp++
	) {
	addItem(ftp, itemp);
	if (ftp->name == NULL) /* Must include NULL entry. */
	    done = 1;
    }
    return menup;
}


/*
  void banner(void)

  Display program name and version on one line with BORDER_CHRs
  to border the top of the scrolling region.
*/
static void
banner()
{
    (void) snprintf(scrbuf, LNBUFSZ, "%s", bannerp);
    HmBanner(scrbuf, BORDER_CHR);
    return;
}


void
closeUi()
{
    ScMvCursor(1, ScLI);
    return;
}


static int
getInput(Input *ip)
{
    if (! batchmode) {
	int c;
	char *p;
	char *defaultp = ip->buffer;
	if (*defaultp == NUL)
	    defaultp = "no default";
	if (ip->range != NULL)
	    (void) snprintf(promptbuf, LNBUFSZ, "%s ? (%s)[%s] ",
			    ip->prompt, ip->range, defaultp);
	else
	    (void) snprintf(promptbuf, LNBUFSZ, "%s ? [%s] ",
			    ip->prompt, defaultp);
	prompt(promptbuf);
	for (p = ip->buffer; (c = HmGetchar()) != '\n';)
	    if (p - ip->buffer < LNBUFSZ-1)
		*p++ = c;
	/* In case user hit CR only, do not disturb buffer. */
	if (p != ip->buffer)
	    *p = '\0';
	prompt((char *) NULL);
    } else {
	char *str = strtok(cmdptr, WHITESPACE);
	if (str == NULL)
	    return 0;
	bu_strlcpy(ip->buffer, str, LNBUFSZ);
	cmdptr = NULL;
    }
    return 1;
}


/*
  void initCmds(void)

  Initialize the keyword commands.
*/
static void
initCmds(Ftable *tp)
{
    for (; tp->name != NULL; tp++) {
	if (tp->next != NULL)
	    initCmds(tp->next);
	else
	    AddCmd(tp->name, tp->func);
    }
    return;
}


/*
  void initMenus(void)

  Initialize the hierarchical menus.
*/
static void
initMenus(Ftable *tp)
{
    mainhmenu = addMenu(tp);
    return;
}


int
initUi()
{
    if (tty) {
	if (! ScInit(stdout))
	    return 0;
	if (ScSetScrlReg(SCROLL_TOP, SCROLL_BTM))
	    (void) ScClrScrlReg();
	else
	    if (ScDL == NULL) {
		prntScr(
		    "This terminal has no scroll region or delete line capability."
		    );
		return 0;
	    }
	(void) ScClrText();	/* wipe screen */
	HmInit(MENU_LFT, MENU_TOP, MENU_MAXVISITEMS);
	banner();
    }
    initMenus(mainmenu);
    initCmds(mainmenu);
    return 1;
}


static int
unitStrToInt(char *str)
{
    if (BU_STR_EQUAL(str, UNITS_INCHES))
	return U_INCHES;
    if (BU_STR_EQUAL(str, UNITS_FEET))
	return U_FEET;
    if (BU_STR_EQUAL(str, UNITS_MILLIMETERS))
	return U_MILLIMETERS;
    if (BU_STR_EQUAL(str, UNITS_CENTIMETERS))
	return U_CENTIMETERS;
    if (BU_STR_EQUAL(str, UNITS_METERS))
	return U_METERS;
    return U_BAD;
}


/*ARGSUSED*/
static void
MattackDir(HmItem *itemp)
{
    static Input input[] = {
	{ "Attack azimuth", "", "%lf", "degrees" },
	{ "Attack elevation", "", "%lf", "degrees" },
    };
    Input *ip = input;
    GetVar(viewazim, ip, RAD2DEG);
    GetVar(viewelev, ip, RAD2DEG);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t%g %g",
		    itemp != NULL ? itemp->text : cmdname,
		    viewazim, viewelev);
    logCmd(scrbuf);
    viewazim /= RAD2DEG;
    viewelev /= RAD2DEG;
    return;
}


/*ARGSUSED*/
static void
MautoBurst(HmItem *itemp)
{
    static Input input[] = {
	{ "Burst along shotline", "n", "%d", "y or n" },
	{ "Require burst air", "y", "%d", "y or n" }
    };
    Input *ip = input;
    GetBool(shotburst, ip);
    if (shotburst) {
	GetBool(reqburstair, ip);
	(void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s %s",
			itemp != NULL ? itemp->text : cmdname,
			shotburst ? "yes" : "no",
			reqburstair ? "yes" : "no");
    } else
	(void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
			itemp != NULL ? itemp->text : cmdname,
			shotburst ? "yes" : "no");
    logCmd(scrbuf);

    if (shotburst)
	firemode &= ~FM_BURST; /* disable discrete burst points */
    return;
}


/*ARGSUSED*/
static void
MburstAir(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of burst air file", "", "%s", 0 },
    };
    Input *ip = input;
    FILE *airfp;
    if (getInput(ip))
	bu_strlcpy(airfile, ip->buffer, LNBUFSZ);
    else
	airfile[0] = NUL;
    if ((airfp = fopen(airfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			airfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    airfile);
    logCmd(scrbuf);
    notify("Reading burst air idents", NOTIFY_APPEND);
    readIdents(&airids, airfp);
    (void) fclose(airfp);
    notify(NULL, NOTIFY_DELETE);
    return;
}


/*ARGSUSED*/
static void
MburstArmor(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of burst armor file", "", "%s", 0 },
    };
    Input *ip = input;
    FILE *armorfp;
    if (getInput(ip))
	bu_strlcpy(armorfile, ip->buffer, LNBUFSZ);
    else
	armorfile[0] = NUL;
    if ((armorfp = fopen(armorfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			armorfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    armorfile);
    logCmd(scrbuf);
    notify("Reading burst armor idents", NOTIFY_APPEND);
    readIdents(&armorids, armorfp);
    (void) fclose(armorfp);
    notify(NULL, NOTIFY_DELETE);
    return;
}


/*ARGSUSED*/
static void
MburstDist(HmItem *itemp)
{
    static Input input[] = {
	{ "Burst distance", "", "%lf", 0 },
    };
    Input *ip = input;
    GetVar(bdist, ip, unitconv);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%g",
		    itemp != NULL ? itemp->text : cmdname,
		    bdist);
    logCmd(scrbuf);
    bdist /= unitconv; /* convert to millimeters */
    return;
}


/*ARGSUSED*/
static void
MburstFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of burst output file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(outfile, ip->buffer, LNBUFSZ);
    else
	outfile[0] = NUL;
    if ((outfp = fopen(outfile, "wb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Write access denied for \"%s\"",
			outfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    outfile);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
McellSize(HmItem *itemp)
{
    static Input input[] = {
	{ "Cell size", "", "%lf", 0 }
    };
    Input *ip = input;
    GetVar(cellsz, ip, unitconv);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%g",
		    itemp != NULL ? itemp->text : cmdname,
		    cellsz);
    logCmd(scrbuf);
    cellsz /= unitconv; /* convert to millimeters */
    return;
}


/*ARGSUSED*/
static void
McolorFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of ident-to-color mapping file",
	  "", "%s", 0 },
    };
    Input *ip = input;
    FILE *colorfp;
    if (getInput(ip))
	bu_strlcpy(colorfile, ip->buffer, LNBUFSZ);
    else
	colorfile[0] = NUL;
    if ((colorfp = fopen(colorfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			colorfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    colorfile);
    logCmd(scrbuf);
    notify("Reading ident-to-color mappings", NOTIFY_APPEND);
    readColors(&colorids, colorfp);
    notify(NULL, NOTIFY_DELETE);
    return;
}


/*ARGSUSED*/
static void
Mcomment(HmItem *UNUSED(itemp))
{
    static Input input[] = {
	{ "Comment", " ", "%s", 0 },
    };
    Input *ip = input;
    if (! batchmode) {
	if (getInput(ip)) {
	    (void) snprintf(scrbuf, LNBUFSZ, "%c%s",
			    CHAR_COMMENT, ip->buffer);
	    logCmd(scrbuf);
	    bu_strlcpy(ip->buffer, " ", LNBUFSZ); /* restore default */
	}
    } else
	logCmd(cmdptr);
    return;
}


/*ARGSUSED*/
static void
MconeHalfAngle(HmItem *itemp)
{
    static Input input[] = {
	{ "Cone angle", "", "%lf", "degrees" },
    };
    Input *ip = input;
    GetVar(conehfangle, ip, RAD2DEG);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%g",
		    itemp != NULL ? itemp->text : cmdname,
		    conehfangle);
    logCmd(scrbuf);
    conehfangle /= RAD2DEG;
    return;
}


/*ARGSUSED*/
static void
McritComp(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of critical component file", "", "%s", 0 },
    };
    Input *ip = input;
    FILE *critfp;
    if (getInput(ip))
	bu_strlcpy(critfile, ip->buffer, LNBUFSZ);
    else
	critfile[0] = NUL;
    if ((critfp = fopen(critfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			critfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    critfile);
    logCmd(scrbuf);
    notify("Reading critical component idents", NOTIFY_APPEND);
    readIdents(&critids, critfp);
    (void) fclose(critfp);
    notify(NULL, NOTIFY_DELETE);
    return;
}


/*ARGSUSED*/
static void
MdeflectSpallCone(HmItem *itemp)
{
    static Input input[] = {
	{ "Deflect cone", "n", "%d", "y or n" },
    };
    Input *ip = input;
    GetBool(deflectcone, ip);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    deflectcone ? "yes" : "no");
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
Mdither(HmItem *itemp)
{
    static Input input[] = {
	{ "Dither cells", "n", "%d", "y or n" },
    };
    Input *ip = input;
    GetBool(dithercells, ip);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    dithercells ? "yes" : "no");
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MenclosePortion(HmItem *itemp)
{
    static Input input[] = {
	{ "Left border of grid", "", "%lf", 0 },
	{ "Right border of grid", "", "%lf", 0 },
	{ "Bottom border of grid", "", "%lf", 0 },
	{ "Top border of grid", "", "%lf", 0 },
    };
    Input *ip = input;
    GetVar(gridlf, ip, unitconv);
    GetVar(gridrt, ip, unitconv);
    GetVar(griddn, ip, unitconv);
    GetVar(gridup, ip, unitconv);
    (void) snprintf(scrbuf, LNBUFSZ, 
		    "%s\t\t%g %g %g %g",
		    itemp != NULL ? itemp->text : cmdname,
		    gridlf, gridrt, griddn, gridup);
    logCmd(scrbuf);
    gridlf /= unitconv; /* convert to millimeters */
    gridrt /= unitconv;
    griddn /= unitconv;
    gridup /= unitconv;
    firemode = FM_PART;
    return;
}


/*ARGSUSED*/
static void
MencloseTarget(HmItem *itemp)
{
    (void) snprintf(scrbuf, LNBUFSZ, 
		    "%s",
		    itemp != NULL ? itemp->text : cmdname);
    logCmd(scrbuf);
    firemode = FM_GRID;
    return;
}


static void
MerrorFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of error output file", "", "%s", 0 },
    };
    Input *ip = input;
    static int errfd = -1;
    if (getInput(ip))
	bu_strlcpy(errfile, ip->buffer, LNBUFSZ);
    else
	bu_strlcpy(errfile, "/dev/tty", LNBUFSZ);
    /* insure that error log is truncated */
    errfd = open(errfile, O_BINARY|O_TRUNC|O_CREAT|O_WRONLY, 0644);
    if (errfd == -1) {
	locPerror(errfile);
	return;
    }
    (void) close(2);
    if (fcntl(errfd, F_DUPFD, 2) == -1) {
	locPerror("fcntl");
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    errfile);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
Mexecute(HmItem *itemp)
{
    static int gottree = 0;
    int loaderror = 0;
    (void) snprintf(scrbuf, LNBUFSZ, 
		    "%s",
		    itemp != NULL ? itemp->text : cmdname);
    logCmd(scrbuf);
    if (gedfile[0] == NUL) {
	warning("No target file has been specified.");
	return;
    }
    notify("Reading target data base", NOTIFY_APPEND);
    rt_prep_timer();
    if (rtip == RTI_NULL
	&& (rtip = rt_dirbuild(gedfile, title, TITLE_LEN))
	== RTI_NULL) {
	warning("Ray tracer failed to read the target file.");
	return;
    }
    prntTimer("dir");
    notify(NULL, NOTIFY_DELETE);
    /* Add air into int trees, must be set after rt_dirbuild() and
       before rt_gettree().
    */
    rtip->useair = 1;
    if (! gottree) {
	char *ptr, *obj;
	rt_prep_timer();
	for (ptr = objects;
	     (obj = strtok(ptr, WHITESPACE)) != NULL;
	     ptr = NULL
	    ) {
	    (void) snprintf(scrbuf, LNBUFSZ, "Loading \"%s\"", obj);
	    notify(scrbuf, NOTIFY_APPEND);
	    if (rt_gettree(rtip, obj) != 0) {
		(void) snprintf(scrbuf, LNBUFSZ, 
				"Bad object \"%s\".",
				obj);
		warning(scrbuf);
		loaderror = 1;
	    }
	    notify(NULL, NOTIFY_DELETE);
	}
	gottree = 1;
	prntTimer("load");
    }
    if (loaderror)
	return;
    if (rtip->needprep) {
	notify("Prepping solids", NOTIFY_APPEND);
	rt_prep_timer();
	rt_prep(rtip);
	prntTimer("prep");
	notify(NULL, NOTIFY_DELETE);
    }
    gridInit();
    if (nriplevels > 0)
	spallInit();
    (void) signal(SIGINT, abort_sig);
    gridModel();
    (void) signal(SIGINT, norml_sig);
    return;
}


/*ARGSUSED*/
static void
MfbFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of frame buffer device", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(fbfile, ip->buffer, LNBUFSZ);
    else
	fbfile[0] = NUL;
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    fbfile);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MgedFile(HmItem *itemp)
{
    static Input input[] = { { "Name of target (MGED) file", "", "%s", 0 },
    };
    Input *ip = input;

    if (getInput(ip))
	bu_strlcpy(gedfile, ip->buffer, LNBUFSZ);

    if (!bu_file_exists(gedfile)) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Unable to find file \"%s\"",
			gedfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    gedfile);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MgridFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of grid file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(gridfile, ip->buffer, LNBUFSZ);
    else
	histfile[0] = NUL;
    if ((gridfp = fopen(gridfile, "wb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Write access denied for \"%s\"",
			gridfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    gridfile);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MgroundPlane(HmItem *itemp)
{
    static Input input[] = {
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
    Input *ip = input;
    GetBool(groundburst, ip);
    if (groundburst) {
	GetVar(grndht, ip, unitconv);
	GetVar(grndfr, ip, unitconv);
	GetVar(grndbk, ip, unitconv);
	GetVar(grndlf, ip, unitconv);
	GetVar(grndrt, ip, unitconv);
	(void) snprintf(scrbuf, LNBUFSZ, "%s\t\tyes %g %g %g %g %g",
			itemp != NULL ? itemp->text : cmdname,
			grndht, grndfr, grndbk, grndlf, grndrt);
	grndht /= unitconv; /* convert to millimeters */
	grndfr /= unitconv;
	grndbk /= unitconv;
	grndlf /= unitconv;
	grndrt /= unitconv;
    } else
	(void) snprintf(scrbuf, LNBUFSZ, "%s\t\tno",
			itemp != NULL ? itemp->text : cmdname
	    );
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MhistFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of histogram file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(histfile, ip->buffer, LNBUFSZ);
    else
	histfile[0] = NUL;
    if ((histfp = fopen(histfile, "wb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Write access denied for \"%s\"",
			histfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    histfile);
    logCmd(scrbuf);
    return;
}


static void
MinputBurst(HmItem *itemp)
{
    static Input input[] = {
	{ "X-coordinate of burst point", "", "%lf", 0 },
	{ "Y-coordinate of burst point", "", "%lf", 0 },
	{ "Z-coordinate of burst point", "", "%lf", 0 },
    };
    Input *ip = input;
    GetVar(burstpoint[X], ip, unitconv);
    GetVar(burstpoint[Y], ip, unitconv);
    GetVar(burstpoint[Z], ip, unitconv);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t%g %g %g",
		    itemp != NULL ? itemp->text : cmdname,
		    burstpoint[X], burstpoint[Y], burstpoint[Z]);
    logCmd(scrbuf);
    burstpoint[X] /= unitconv; /* convert to millimeters */
    burstpoint[Y] /= unitconv;
    burstpoint[Z] /= unitconv;
    firemode = FM_BURST | FM_3DIM;
    return;
}


/*ARGSUSED*/
static void
Minput2dShot(HmItem *itemp)
{
    static Input input[] = {
	{ "Y'-coordinate of shotline", "", "%lf", 0 },
	{ "Z'-coordinate of shotline", "", "%lf", 0 },
    };
    Input *ip = input;
    GetVar(fire[X], ip, unitconv);
    GetVar(fire[Y], ip, unitconv);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%g %g",
		    itemp != NULL ? itemp->text : cmdname,
		    fire[X], fire[Y]);
    logCmd(scrbuf);
    fire[X] /= unitconv; /* convert to millimeters */
    fire[Y] /= unitconv;
    firemode = FM_SHOT;
    return;
}


/*ARGSUSED*/
static void
Minput3dShot(HmItem *itemp)
{
    static Input input[] = {
	{ "X-coordinate of shotline", "", "%lf", 0 },
	{ "Y-coordinate of shotline", "", "%lf", 0 },
	{ "Z-coordinate of shotline", "", "%lf", 0 },
    };
    Input *ip = input;
    GetVar(fire[X], ip, unitconv);
    GetVar(fire[Y], ip, unitconv);
    GetVar(fire[Z], ip, unitconv);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%g %g %g",
		    itemp != NULL ? itemp->text : cmdname,
		    fire[X], fire[Y], fire[Z]);
    logCmd(scrbuf);
    fire[X] /= unitconv; /* convert to millimeters */
    fire[Y] /= unitconv;
    fire[Z] /= unitconv;
    firemode = FM_SHOT | FM_3DIM;
    return;
}


/*ARGSUSED*/
static void
Mnop(HmItem *UNUSED(itemp))
{
    return;
}


/*ARGSUSED*/
static void
Mobjects(HmItem *itemp)
{
    static Input input[] = {
	{ "List of objects from target file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(objects, ip->buffer, LNBUFSZ);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    objects);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
Moverlaps(HmItem *itemp)
{
    static Input input[] = {
	{ "Report overlaps", "y", "%d", "y or n" },
    };
    Input *ip = input;
    GetBool(reportoverlaps, ip);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    reportoverlaps ? "yes" : "no");
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MmaxBarriers(HmItem *itemp)
{
    static Input input[] = {
	{ "Maximum spall barriers per ray", "", "%d", 0 },
    };
    Input *ip = input;
    GetVar(nbarriers, ip, 1);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%d",
		    itemp != NULL ? itemp->text : cmdname,
		    nbarriers);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MmaxSpallRays(HmItem *itemp)
{
    static Input input[] = {
	{ "Maximum rays per burst", "", "%d", 0 },
    };
    Input *ip = input;
    GetVar(nspallrays, ip, 1);
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%d",
		    itemp != NULL ? itemp->text : cmdname,
		    nspallrays);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MplotFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of UNIX plot file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(plotfile, ip->buffer, LNBUFSZ);
    else
	plotfile[0] = NUL;
    if ((plotfp = fopen(plotfile, "wb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Write access denied for \"%s\"",
			plotfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    plotfile);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
Mread2dShotFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of 2-D shot input file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(shotfile, ip->buffer, LNBUFSZ);
    if ((shotfp = fopen(shotfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			shotfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    shotfile);
    logCmd(scrbuf);
    firemode = FM_SHOT | FM_FILE;
    return;
}


/*ARGSUSED*/
static void
Mread3dShotFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of 3-D shot input file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(shotfile, ip->buffer, LNBUFSZ);
    if ((shotfp = fopen(shotfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			shotfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    shotfile);
    logCmd(scrbuf);
    firemode = FM_SHOT | FM_FILE | FM_3DIM;
    return;
}


/*ARGSUSED*/
static void
MreadBurstFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of 3-D burst input file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(burstfile, ip->buffer, LNBUFSZ);
    if ((burstfp = fopen(burstfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			burstfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    burstfile);
    logCmd(scrbuf);
    firemode = FM_BURST | FM_3DIM | FM_FILE;
    return;
}


/*ARGSUSED*/
static void
MreadCmdFile(HmItem *UNUSED(itemp))
{
    static Input input[] = {
	{ "Name of command file", "", "%s", 0 },
    };
    Input *ip = input;
    char cmdfile[LNBUFSZ];
    FILE *cmdfp;
    if (getInput(ip))
	bu_strlcpy(cmdfile, ip->buffer, LNBUFSZ);
    if ((cmdfp = fopen(cmdfile, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			cmdfile);
	warning(scrbuf);
	return;
    }
    readBatchInput(cmdfp);
    (void) fclose(cmdfp);
    return;
}


/*ARGSUSED*/
static void
MshotlineFile(HmItem *itemp)
{
    static Input input[] = {
	{ "Name of shotline output file", "", "%s", 0 },
    };
    Input *ip = input;
    if (getInput(ip))
	bu_strlcpy(shotlnfile, ip->buffer, LNBUFSZ);
    else
	shotlnfile[0] = NUL;
    if ((shotlnfp = fopen(shotlnfile, "wb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ,
			"Write access denied for \"%s\"",
			shotlnfile);
	warning(scrbuf);
	return;
    }
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    shotlnfile);
    logCmd(scrbuf);
    return;
}


HmItem units_items[] =
{
    {
	UNITS_MILLIMETERS,
	"interpret inputs and convert outputs to millimeters",
	NULL, NULL, NULL, Mnop, 0 },
    {
	UNITS_CENTIMETERS,
	"interpret inputs and convert outputs to centimeters",
	NULL, NULL, NULL, Mnop, 0 },
    {
	UNITS_METERS,
	"interpret inputs and convert outputs to meters",
	NULL, NULL, NULL, Mnop, 0 },
    {
	UNITS_INCHES,
	"interpret inputs and convert outputs to inches",
	NULL, NULL, NULL, Mnop, 0 },
    {
	UNITS_FEET,
	"interpret inputs and convert outputs to feet",
	NULL, NULL, NULL, Mnop, 0 },
    { NULL, NULL, NULL, NULL, NULL, NULL, 0 },
};
HmMenu units_hmenu = { units_items, 0, 0, 0, 0 };

/*ARGSUSED*/
static void
Munits(HmItem *itemp)
{
    char *unitstr;
    HmItem *itemptr;
    if (itemp != NULL) {
	if ((itemptr = HmHit(&units_hmenu)) == (HmItem *) NULL)
	    return;
	unitstr = itemptr->text;
    } else
	unitstr = strtok(cmdptr, WHITESPACE);
    units = unitStrToInt(unitstr);
    if (units == U_BAD) {
	(void) snprintf(scrbuf, LNBUFSZ, "Illegal units \"%s\"", unitstr);
	warning(scrbuf);
	return;
    }
    switch (units) {
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
    (void) snprintf(scrbuf, LNBUFSZ, "%s\t\t\t%s",
		    itemp != NULL ? itemp->text : cmdname,
		    unitstr);
    logCmd(scrbuf);
    return;
}


/*ARGSUSED*/
static void
MwriteCmdFile(HmItem *UNUSED(itemp))
{
    static Input input[] = {
	{ "Name of command file", "", "%s", 0 },
    };
    Input *ip = input;
    char cmdfile[LNBUFSZ];
    FILE *cmdfp;
    FILE *inpfp;
    if (getInput(ip))
	bu_strlcpy(cmdfile, ip->buffer, LNBUFSZ);
    if ((cmdfp = fopen(cmdfile, "wb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Write access denied for \"%s\"",
			cmdfile);
	warning(scrbuf);
	return;
    }
    if ((inpfp = fopen(tmpfname, "rb")) == NULL) {
	(void) snprintf(scrbuf, LNBUFSZ, 
			"Read access denied for \"%s\"",
			tmpfname);
	warning(scrbuf);
	(void) fclose(cmdfp);
	return;
    }
    while (bu_fgets(scrbuf, LNBUFSZ, inpfp) != NULL)
	fputs(scrbuf, cmdfp);
    (void) fclose(cmdfp);
    (void) fclose(inpfp);
    return;
}


void
intr_sig(int UNUSED(sig))
{
    static Input input[] = {
	{ "Really quit ? ", "n", "%d", "y or n" },
    };
    Input *ip = input;
    (void) signal(SIGINT, intr_sig);
    if (getInput(ip)) {
	if (ip->buffer[0] == 'y')
	    exitCleanly(SIGINT);
	else
	    if (ip->buffer[0] != 'n') {
		(void) snprintf(scrbuf, LNBUFSZ,
				"Illegal input \"%s\".",
				ip->buffer);
		warning(scrbuf);
		return;
	    }
    }
    return;
}


void
logCmd(char *cmd)
{
    prntScr("%s", cmd); /* avoid possible problems with '%' in string */
    if (fprintf(tmpfp, "%s\n", cmd) < 0) {
	locPerror("fprintf");
	exitCleanly(1);
    } else
	(void) fflush(tmpfp);
    return;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
