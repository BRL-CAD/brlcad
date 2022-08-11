/*                         B U R S T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @file burst/burst.h
 *
 */

#ifndef BURST_BURST_H
#define BURST_BURST_H

#include "common.h"
#include <stdio.h>
#include <signal.h>

#include "vmath.h"
#include "dm.h"
#include "raytrace.h"

/* NSIG not always defined in <signal.h> */
#ifndef NSIG
#  define NSIG 64
#endif


/* menu configuration */
#define MENU_LFT	1
#define MENU_TOP	2
#define MENU_MAXVISITEMS	10

/* prompt line configuration */
#define PROMPT_X	HmXPROMPT
#define PROMPT_Y	HmYPROMPT

/* banner (border) configuration */
#define BORDER_CHR	'_'
#define BORDER_Y	(PROMPT_Y+1)

/* grid offset printing window */
#define GRID_X		55	/* where grid indices are printed */
#define GRID_Y		BORDER_Y

/* scroll region configuration */
#define SCROLL_TOP	(BORDER_Y+1)
#define SCROLL_BTM	(ScLI-1) /* bottom line causes scroll */

/* timer (cpu statistics) window configuration */
#define TIMER_X		1
#define TIMER_Y		1

/* buffer sizes */
#define LNBUFSZ		256	/* buffer for one-line messages */
#define MAXDEVWID	10000	/* maximum width of frame buffer */

#define CHAR_COMMENT	'#'
#define CMD_COMMENT	"comment"

/* default parameters */
#define DFL_AZIMUTH	0.0
#define DFL_BARRIERS	100
#define DFL_BDIST	0.0
#define DFL_CELLSIZE	101.6
#define DFL_CONEANGLE	(45.0/RAD2DEG)
#define DFL_DEFLECT	0
#define DFL_DITHER	0
#define DFL_ELEVATION	0.0
#define DFL_NRAYS	200
#define DFL_OVERLAPS	1
#define DFL_RIPLEVELS	1
#define DFL_UNITS	U_MILLIMETERS

/* firing mode bit definitions */
#define ASNBIT(w, b)	(w = (b))
#define SETBIT(w, b)	(w |= (b))
#define CLRBIT(w, b)	(w &= ~(b))
#define TSTBIT(w, b)	((w)&(b))
#define FM_GRID  0	/* generate grid of shotlines */
#define FM_DFLT	 FM_GRID
#define FM_PART  (1)	/* bit 0: ON = partial envelope, OFF = full */
#define FM_SHOT	 (1<<1)	/* bit 1: ON = discrete shots, OFF = gridding */
#define FM_FILE	 (1<<2)	/* bit 2: ON = file input, OFF = direct input */
#define FM_3DIM	 (1<<3)	/* bit 3: ON = 3-D coords., OFF = 2-D coords */
#define FM_BURST (1<<4) /* bit 4: ON = discrete burst points, OFF = shots */

/* flags for notify() */
#define NOTIFY_APPEND	1
#define NOTIFY_DELETE	2
#define NOTIFY_ERASE	4

#define NOTIFY_DELIM	':'

#define PB_ASPECT_INIT		'1'
#define PB_CELL_IDENT		'2'
#define PB_RAY_INTERSECT	'3'
#define PB_RAY_HEADER		'4'
#define PB_REGION_HEADER	'5'

#define PS_ASPECT_INIT		'1'
#define PS_CELL_IDENT		'2'
#define PS_SHOT_INTERSECT	'3'

#define TITLE_LEN	72
#define TIMER_LEN	72

#define U_INCHES	0
#define U_FEET		1
#define U_MILLIMETERS	2
#define U_CENTIMETERS	3
#define U_METERS	4
#define U_BAD		-1

#define UNITS_INCHES		"inches"
#define UNITS_FEET		"feet"
#define UNITS_MILLIMETERS	"millimeters"
#define UNITS_CENTIMETERS	"centimeters"
#define UNITS_METERS		"meters"

/* white space in input tokens */
#define WHITESPACE	" \t"

/* colors for UNIX plot files */
#define R_GRID		255	/* grid - yellow */
#define G_GRID		255
#define B_GRID		0

#define R_BURST		255	/* burst cone - red */
#define G_BURST		0
#define B_BURST		0

#define R_OUTAIR	100	/* outside air - light blue */
#define G_OUTAIR	100
#define B_OUTAIR	255

#define R_INAIR		100	/* inside air - light green */
#define G_INAIR		255
#define B_INAIR		100

#define R_COMP		0	/* component (default) - blue */
#define G_COMP		0
#define B_COMP		255

#define R_CRIT		255	/* critical component (default) - purple */
#define G_CRIT		0
#define B_CRIT		255

#define C_MAIN		0
#define C_CRIT		1

#define COS_TOL		0.01
#define LOS_TOL		0.1
#define VEC_TOL		0.001
#define OVERLAP_TOL	0.25	/* thinner overlaps not reported */
#define EXIT_AIR	9	/* exit air is called 09 air */
#define OUTSIDE_AIR	1	/* outside air is called 01 air */

#define Air(rp)		((rp)->reg_aircode > 0)
#define DiffAir(rp, sp)	((rp)->reg_aircode != (sp)->reg_aircode)
#define SameAir(rp, sp)	((rp)->reg_aircode == (sp)->reg_aircode)
#define SameCmp(rp, sp)	((rp)->reg_regionid == (sp)->reg_regionid)
#define OutsideAir(rp)	((rp)->reg_aircode == OUTSIDE_AIR)
#define InsideAir(rp)	(Air(rp)&& !OutsideAir(rp))

#define Malloc_Bomb(_bytes_) \
		brst_log("\"%s\"(%d) : allocation of %d bytes failed.\n", \
				__FILE__, __LINE__, _bytes_)

#define Swap_Doubles(a_, b_) \
		{	fastf_t f_ = a_; \
		a_ = b_; \
		b_ = f_; \
		}
#define Toggle(f)	(f) = !(f)

typedef struct ids Ids;
struct ids
{
    short i_lower;
    short i_upper;
    Ids *i_next;
};
#define IDS_NULL (Ids *) 0

typedef struct colors Colors;
struct colors
{
    short c_lower;
    short c_upper;
    unsigned char c_rgb[3];
    Colors *c_next;
};
#define COLORS_NULL (Colors *) 0

typedef struct pt_queue Pt_Queue;
struct pt_queue
{
    struct partition *q_part;
    Pt_Queue *q_next;
};


#define PT_Q_NULL (Pt_Queue *) 0

/*
 * MUVES "Hm" (hierarchical menu) package definitions
 *
 * This software and documentation is derived in part from the
 * menuhit(9.3) manual pages (not source code) in AT&T 9th Edition
 * UNIX, Version 1 Programmer's Manual.
 *
 * The Hm package provides a pop-up menu implementation which uses a
 * terminal-independent screen management facility (Sc package) to
 * simulate the necessary graphics using the ASCII character set.
 * Only a few keyboard characters are required to control the menus,
 * but when possible, as with DMD terminals running a MYX terminal
 * emulator, the menus can be controlled with the mouse.
 */
/**
 * Each menu is defined by the following structure:
 *
 * typedef struct {
 * HmItem *item;
 * HmItem *(*generator)();
 * short prevtop;
 * short prevhit;
 * int sticky;
 * }
 * HmMenu;
 *
 * A menu can be built as an array of HmItem's, pointed to by item, or
 * with a generator function.  If item is (HmItem *) 0, the generator
 * field is assumed to be valid.  The generator function is passed an
 * integer parameter, and must return the pointer to an HmItem in a
 * static area (the result is only needed until the next call).  The
 * n's are guaranteed to start at 0 and increase by 1 each invocation
 * until generator returns (HmItem *) 0.  Prevtop will contain the
 * index of the menu item which appeared at the top of the menu the
 * last time it was displayed.  Prevhit will contain the index of the
 * last item traversed (indices begin at 0).  If sticky is true, the
 * menu will hang around after an entry is selected so that another
 * entry may be chosen and the menu must be exited explicitly with the
 * appropriate key-stroke or equivalent mouse operation.  Otherwise,
 * the menu will exit after selection of an item (and any actions
 * resulting from that selection such as submenus have finished).
 * WARNING: if a menu is to be used more than once during recursive
 * calls to HmHit, there needs to be distinct copies (allocated
 * storage) since the item field is filled in by the generator
 * function, and the prevtop and prevhit fields should be private to
 * the current invocation of HmHit.
 */

struct HmMenu;

typedef struct
{
    char *text;			/* menu item string */
    char *help;			/* help string */
    struct HmMenu *next;	/* sub-menu pointer or NULL */
    void (*dfn)();
    void (*bfn)();
    void (*hfn)();
    long data;
}
HmItem;

/**
 * Menu items are defined by the following structure:
 *
 * typedef struct {
 * char *text;
 * char *help;
 * HmMenu *next;
 * void (*dfn)();
 * void (*bfn)();
 * void (*hfn)();
 * long data;
 * }
 * HmItem;
 *
 * The text field will be displayed as the name of the item.
 * Characters with the 0200 bit set are regarded as fill characters.
 * For example, the string "\240X" will appear in the menu as a
 * right-justified X (040 is the ASCII space character).  Menu strings
 * without fill characters are drawn centered in the menu.  Whether
 * generated statically or dynamically, the list of HmItem's must be
 * terminated by a NULL text field.  The help string will be displayed
 * when the user presses the help key.  If next is not equal to
 * (HmMenu *) 0, it is assumed to point to a submenu, "dfn()" if
 * non-zero is called just before the submenu is invoked, and "bfn()"
 * likewise just afterwards.  These functions are passed the current
 * menu item.  If the menu item is selected and next is 0, "hfn()" is
 * called (if non-zero), also with the current menu item.  The "data"
 * field is reserved for the user to pass information between these
 * functions and the calling routine.
 */
typedef struct HmMenu
{
    HmItem *item;		/* List of menu items or 0.		*/
    HmItem *(*generator)();	/* If item == 0, generates items.	*/
    short prevtop;		/* Top entry currently visible */
    short prevhit;		/* Offset from top of last select */
    int sticky;			/* If true, menu stays around after
				   SELECT, and until QUIT. */
}
HmMenu;

/* Structure used internally for screen management.  These are stacked
   dynamically with calls to HmHit() such that the head of the chain
   is the current menu, and the end of the chain such that next is
   0, is the top-level menu.
*/
typedef struct HmWin
{
    struct HmWin *next;	/* Parent of this menu.			*/
    HmMenu *menup;	/* Address of menu data structure.	*/
    int menux;		/* Position on screen where top-left */
    int menuy;		/* corner of menu will be displayed.	*/
    int width;		/* Width of menu, not including border.	*/
    int height;		/* Number of menu entries.		*/
    int *dirty;		/* Dynamically allocated bitmap. ON bits
			   mean character needs a redraw.	*/
}
HmWindow;

/**
 * int HmInit(int x, int y, int maxvis)
 *
 * HmInit() must be called before any other routines in the Hm package
 * to initialize the screen position of the top-left corner of the
 * top-level menu to x and y and to set the maximum number of menu
 * entries which will be visible at one time to maxvis.  If the number
 * of entries in a menu exceeds maxvis, the menu will scroll to
 * accommodate them.  The values of x, y and maxvis are stored in
 * these external variables:
 *
 * int HmLftMenu
 * int HmTopMenu
 * int HmMaxVis
 *
 * If this routine is not called, default parameters will be used.
 *
 * HmInit() also opens "/dev/tty" for input and stores its file
 * descriptor in HmTtyFd and associated stream handle in HmTtyFp.
 *
 * int HmTtyFd
 * FILE *HmTtyFp
 *
 * This routine returns true or false on success or failure to open
 * "/dev/tty".
 */
extern int HmInit(int x, int y, int maxvis);
extern FILE *HmTtyFp;
extern int HmLftMenu;
extern int HmTopMenu;
extern int HmMaxVis;
extern int HmLastMaxVis;
extern int HmTtyFd;

/**
 * HmItem *HmHit(HmMenu *menup)
 *
 * HmHit() presents the user with a menu specified by HmMenu pointer
 * menup and returns a pointer to the selected HmItem or 0 if nothing
 * was selected.  The menu is presented to the user with the item
 * corresponding to prevhit (current item) highlighted.  If a menu has
 * not been accessed yet, prevhit will be set to the first item.  The
 * user may move the cursor to the item below the current one by
 * pressing the 'd' key, or move up by pressing the 'u'.  If the user
 * presses 'h', the help message for the current item will be
 * displayed until another key is struck.  To select an item, the user
 * presses the space bar.  An actual selection has been made when the
 * selected item does not have a submenu, otherwise, it may be termed
 * a traversal of that item.  Traversing an item does change the value
 * of prevhit, but is not final as the submenu can be exited by the
 * user pressing 'q', before selecting an item.  In case the screen
 * becomes disturbed (i.e. by the output of another process), holding
 * down the CONTROL key and striking an 'l' will re-display all of the
 * menus.
 *
 * Both the help and warning (non-fatal error) messages will be
 * displayed in the one-line window at HmYCOMMO.  For instance, if the
 * user hits any keys other than the ones mentioned above, an error
 * message will appear in this window.  Also, a one-line window is
 * reserved for application prompts at HmYPROMPT.  The prompt window
 * is cleared just prior to blocking on user input, and the message
 * window is cleared when the user provides input (strikes a key).
 *
 * If a menu has more items than can be displayed at once, the corners
 * of the menu will indicate this as follows:
 *
 * '+' means the adjacent displayed item is the actual first (or last
 * if a bottom corner) item in the menu.
 *
 * 'v' means that the entry displayed in the previous line is not the
 * last, and the menu can be scrolled upwards.
 *
 * '^' means that the entry displayed on the next line is not actually
 * the first, and the menu can be scrolled downwards.
 *
 * Attempting to move the cursor below the bottom entry of the menu
 * will cause the menu to scroll half a page or until the entries run
 * out, which ever comes first.  If there are no entries to scroll the
 * terminal will beep.  The analogous holds true for attempting to
 * move upward past the top entry of the menu.  If a DMD terminal with
 * MYX running is used, a special cursor will appear, and the user may
 * use the mouse rather than the keyboard as follows: Clicking button
 * 1 of the mouse while the cursor is outside the "current menu", will
 * cause the terminal to beep.  If the cursor is on an item, that item
 * will be made "current".  If on the "current" item, that item will
 * be selected.  If on the top border of the current menu, that menu
 * will scroll down if possible, and if not, that menu will exit.  If
 * on the bottom border, that menu will scroll upward if possible and
 * beep if not.
 */
    extern HmItem *HmHit(HmMenu *menup);

/**
 * void HmRedraw(void)
 *
 * HmRedraw() will force the entire set of active menus to be redrawn
 * on the next call to HmHit().  This is useful when an application
 * interferes with the portion of the screen used by the Hm package
 * (from HmTopMenu to HmYCOMMO).
**/
extern void HmRedraw(void);

/**
 * void HmError(const char *string)
 *
 * HmError() will display string on line HmYCOMMO.
 */
extern void HmError(const char *str);

#define HmYCOMMO (HmTopMenu+HmMaxVis+HmHGTBORDER)
#define HmYPROMPT (HmYCOMMO+1)
#define HmXPROMPT 1
#define HmYBORDER (HmYPROMPT+1)

/**
 * int HmGetchar(void)
 * int HmUngetchar(int c)
 *
 * HmGetchar() and HmUngetchar() are used by the Hm package to read a
 * character from the keyboard and to stuff one character back on the
 * input stream.  They may be both be supplied by the application if
 * the default behavior is not desirable.  HmGetchar() returns the
 * next character on the standard input.  This command will block
 * until input is available.  HmUngetchar() inserts the character c on
 * the standard input.  An EOF will be returned if this is not
 * possible or c is equal to EOF.  In general, this is guaranteed to
 * work for one character assuming that something has already been
 * read with HmGetchar() and the input stream is buffered.
 */
extern int HmGetchar(void);
extern int HmUngetchar(int c);

/**
 * void HmTtySet(void)
 * void HmTtyReset(void)
 *
 * HmTtySet() and HmTtyReset() set/restore the terminal modes for the
 * menus to work properly.  These are mainly internal routines which
 * are called from HmHit(), but are provided in case an escape from
 * the program is provided by the application or job control is
 * enabled in the underlying shell, in which case, these routines can
 * be called from a menu function or signal handler.
 */
extern void HmTtySet();
extern void HmTtyReset();


#define HmMAXLINE 132
#define HmHGTBORDER 2

#ifdef NULL_FUNC
#  undef NULL_FUNC
#endif

#define NULL_FUNC ((Func *) NULL)
#define TRIE_NULL ((Trie *) NULL)

/* Datum for trie leaves.  */
typedef void Func();

/* Trie tree node.  */
typedef union trie Trie;
union trie {
    struct {
	/* Internal nodes: datum is current letter. */
	int t_char;   /* Current letter.  */
	Trie *t_altr; /* Alternate letter node link.  */
	Trie *t_next; /* Next letter node link.  */
    }
    n;
    struct {
	/* Leaf nodes: datum is function ptr.  */
	Func *t_func; /* Function pointer.  */
	Trie *t_altr; /* Alternate letter node link.  */
	Trie *t_next; /* Next letter node link.  */
    }
    l;
};
#define NewTrie(p) if (((p) = (Trie *) malloc(sizeof(Trie))) == TRIE_NULL) {\
	Malloc_Bomb(sizeof(Trie));\
	return TRIE_NULL;\
}
extern Trie *cmd_trie;


struct burst_state {
    Colors colorids;           /* ident range to color mappings for plots */
    struct fb *fbiop;          /* frame buffer specific access from libfb */
    FILE *burstfp;             /* input stream for burst point locations */
    FILE *gridfp;              /* grid file output stream (2-d shots) */
    FILE *histfp;              /* histogram output stream (statistics) */
    FILE *outfp;               /* burst point library output stream */
    FILE *plotfp;              /* 3-D UNIX plot stream (debugging) */
    FILE *shotfp;              /* input stream for shot positions */
    FILE *shotlnfp;            /* shotline file output stream */
    FILE *tmpfp;               /* temporary file output stream for logging input */
    HmMenu *mainhmenu;         /* */
    Ids airids;                /* burst air idents */
    Ids armorids;              /* burst armor idents */
    Ids critids;               /* critical component idents */
    unsigned char *pixgrid;    /* */
    unsigned char pixaxis[3];  /* grid axis */
    unsigned char pixbhit[3];  /* burst ray hit non-critical comps */
    unsigned char pixbkgr[3];  /* outside grid */
    unsigned char pixblack[3]; /* black */
    unsigned char pixcrit[3];  /* burst ray hit critical component */
    unsigned char pixghit[3];  /* ground burst */
    unsigned char pixmiss[3];  /* shot missed target */
    unsigned char pixtarg[3];  /* shot hit target */
    Trie *cmdtrie;             /* */

    int plotline;              /* boolean for plot lines (otherwise plots points) */
    int batchmode;             /* are we processing batch input now */
    int cantwarhead;           /* pitch or yaw will be applied to warhead */
    int deflectcone;           /* cone axis deflects towards normal */
    int dithercells;           /* if true, randomize shot within cell */
    int fatalerror;            /* must abort ray tracing */
    int groundburst;           /* if true, burst on imaginary ground */
    int reportoverlaps;        /* if true, overlaps are reported */
    int reqburstair;           /* if true, burst air required for shotburst */
    int shotburst;    	       /* if true, burst along shotline */
    int tty;    	       /* if true, full screen display is used */
    int userinterrupt;         /* has the ray trace been interrupted */

    char airfile[LNBUFSZ];     /* input file name for burst air ids */
    char armorfile[LNBUFSZ];   /* input file name for burst armor ids */
    char burstfile[LNBUFSZ];   /* input file name for burst points */
    char cmdbuf[LNBUFSZ];      /* */
    char cmdname[LNBUFSZ];     /* */
    char colorfile[LNBUFSZ];   /* ident range-to-color file name */
    char critfile[LNBUFSZ];    /* input file for critical components */
    char errfile[LNBUFSZ];     /* errors/diagnostics log file name */
    char fbfile[LNBUFSZ];      /* frame buffer image file name */
    char gedfile[LNBUFSZ];     /* MGED data base file name */
    char gridfile[LNBUFSZ];    /* saved grid (2-d shots) file name */
    char histfile[LNBUFSZ];    /* histogram file name (statistics) */
    char objects[LNBUFSZ];     /* list of objects from MGED file */
    char outfile[LNBUFSZ];     /* burst point library output file name */
    char plotfile[LNBUFSZ];    /* 3-D UNIX plot file name (debugging) */
    char scrbuf[LNBUFSZ];      /* scratch buffer for temporary use */
    char scriptfile[LNBUFSZ];  /* shell script file name */
    char shotfile[LNBUFSZ];    /* input file of firing coordinates */
    char shotlnfile[LNBUFSZ];  /* shotline output file name */
    char title[TITLE_LEN];     /* title of MGED target description */
    char timer[TIMER_LEN];     /* CPU usage statistics */
    char tmpfname[TIMER_LEN];  /* temporary file for logging input */
    char *cmdptr;              /* */

    fastf_t bdist;             /* fusing distance for warhead */
    fastf_t burstpoint[3];     /* explicit burst point coordinates */
    fastf_t cellsz;            /* shotline separation */
    fastf_t conehfangle;       /* spall cone half angle */
    fastf_t fire[3];           /* explicit firing coordinates (2-D or 3-D) */
    fastf_t griddn;            /* distance in model coordinates from origin to bottom border of grid */
    fastf_t gridlf;            /* distance to left border */
    fastf_t gridrt;            /* distance to right border */
    fastf_t gridup;            /* distance to top border */
    fastf_t gridhor[3];        /* horizontal grid direction cosines */
    fastf_t gridsoff[3];       /* origin of grid translated by stand-off */
    fastf_t gridver[3];        /* vertical grid direction cosines */
    fastf_t grndbk;            /* distance to back border of ground plane (-X) */
    fastf_t grndht;            /* distance of ground plane below target origin (-Z) */
    fastf_t grndfr;            /* distance to front border of ground plane (+X) */
    fastf_t grndlf;            /* distance to left border of ground plane (+Y) */
    fastf_t grndrt;            /* distance to right border of ground plane (-Y) */
    fastf_t modlcntr[3];       /* centroid of target's bounding RPP */
    fastf_t modldn;            /* distance in model coordinates from origin to bottom extent of projection of model in grid plane */
    fastf_t modllf;            /* distance to left extent */
    fastf_t modlrt;            /* distance to right extent */
    fastf_t modlup;            /* distance to top extent */
    fastf_t raysolidangle;     /* solid angle per spall sampling ray */
    fastf_t standoff;          /* distance from model origin to grid */
    fastf_t unitconv;          /* units conversion factor (mm to "units") */
    fastf_t viewazim;          /* degrees from X-axis to firing position */
    fastf_t viewelev;          /* degrees from XY-plane to firing position */

    /* These are the angles and fusing distance used to specify the path of
       the canted warhead in Bob Wilson's simulation.
       */
    fastf_t pitch;	       /* elevation above path of main penetrator */
    fastf_t yaw;	       /* deviation right of path of main penetrator */

    /* useful vectors */
    fastf_t xaxis[3];
    fastf_t zaxis[3];
    fastf_t negzaxis[3];

    int co;    		       /* columns of text displayable on video screen */
    int devwid;    	       /* width in pixels of frame buffer window */
    int devhgt;    	       /* height in pixels of frame buffer window */
    int firemode;              /* mode of specifying shots */
    int gridsz;
    int gridxfin;
    int gridyfin;
    int gridxorg;
    int gridyorg;
    int gridwidth;    	       /* Grid width in cells. */
    int gridheight;    	       /* Grid height in cells. */
    int li;    		       /* lines of text displayable on video screen */
    int nbarriers;             /* no. of barriers allowed to critical comp */
    int noverlaps;             /* no. of overlaps encountered in this view */
    int nprocessors;           /* no. of processors running concurrently */
    int nriplevels;            /* no. of levels of ripping (0 = no ripping) */
    int nspallrays;            /* no. of spall rays at each burst point */
    int units;                 /* target units (default is millimeters) */
    int zoom;                  /* magnification factor on frame buffer */

    struct rt_i *rtip;         /* model specific access from librt */

    /* signal handlers */
    void (*norml_sig)();       /* active during interactive operation */
    void (*abort_sig)();       /* active during ray tracing only */
};

void burst_state_init(struct burst_state *s);

#endif  /* BURST_BURST_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
