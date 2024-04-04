/* 
 * tkTablePs.c --
 *
 *	This module implements postscript output for table widgets.
 *	Based off of Tk8.1a2 tkCanvPs.c.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * changes 1998 Copyright (c) 1998 Jeffrey Hobbs
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tkTable.h"

/* This is for Tcl_DStringAppendAll */
#if defined(__STDC__) || defined(HAS_STDARG)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifndef TCL_INTEGER_SPACE
/* This appears in 8.1 */
#define TCL_INTEGER_SPACE 24
#endif

/*
 * One of the following structures is created to keep track of Postscript
 * output being generated.  It consists mostly of information provided on
 * the widget command line.
 */

typedef struct TkPostscriptInfo {
  int x, y, width, height;	/* Area to print, in table pixel
				 * coordinates. */
  int x2, y2;			/* x+width and y+height. */
  char *pageXString;		/* String value of "-pagex" option or NULL. */
  char *pageYString;		/* String value of "-pagey" option or NULL. */
  double pageX, pageY;		/* Postscript coordinates (in points)
				 * corresponding to pageXString and
				 * pageYString. Don't forget that y-values
				 * grow upwards for Postscript! */
  char *pageWidthString;	/* Printed width of output. */
  char *pageHeightString;	/* Printed height of output. */
  double scale;			/* Scale factor for conversion: each pixel
				 * maps into this many points. */
  Tk_Anchor pageAnchor;		/* How to anchor bbox on Postscript page. */
  int rotate;			/* Non-zero means output should be rotated
				 * on page (landscape mode). */
  char *fontVar;		/* If non-NULL, gives name of global variable
				 * containing font mapping information.
				 * Malloc'ed. */
  char *colorVar;		/* If non-NULL, give name of global variable
				 * containing color mapping information.
				 * Malloc'ed. */
  char *colorMode;		/* Mode for handling colors:  "monochrome",
				 * "gray", or "color".  Malloc'ed. */
  int colorLevel;		/* Numeric value corresponding to colorMode:
				 * 0 for mono, 1 for gray, 2 for color. */
  char *fileName;		/* Name of file in which to write Postscript;
				 * NULL means return Postscript info as
				 * result. Malloc'ed. */
  char *channelName;		/* If -channel is specified, the name of
                                 * the channel to use. */
  Tcl_Channel chan;		/* Open channel corresponding to fileName. */
  Tcl_HashTable fontTable;	/* Hash table containing names of all font
				 * families used in output.  The hash table
				 * values are not used. */
  char *first, *last;		/* table indices to start and end at */
} TkPostscriptInfo;

/*
 * The table below provides a template that's used to process arguments
 * to the table "postscript" command and fill in TkPostscriptInfo
 * structures.
 */

static Tk_ConfigSpec configSpecs[] = {
  {TK_CONFIG_STRING, "-colormap", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, colorVar), 0},
  {TK_CONFIG_STRING, "-colormode", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, colorMode), 0},
  {TK_CONFIG_STRING, "-file", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, fileName), 0},
  {TK_CONFIG_STRING, "-channel", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, channelName), 0},
  {TK_CONFIG_STRING, "-first", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, first), 0},
  {TK_CONFIG_STRING, "-fontmap", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, fontVar), 0},
  {TK_CONFIG_PIXELS, "-height", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, height), 0},
  {TK_CONFIG_STRING, "-last", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, last), 0},
  {TK_CONFIG_ANCHOR, "-pageanchor", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, pageAnchor), 0},
  {TK_CONFIG_STRING, "-pageheight", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, pageHeightString), 0},
  {TK_CONFIG_STRING, "-pagewidth", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, pageWidthString), 0},
  {TK_CONFIG_STRING, "-pagex", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, pageXString), 0},
  {TK_CONFIG_STRING, "-pagey", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, pageYString), 0},
  {TK_CONFIG_BOOLEAN, "-rotate", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, rotate), 0},
  {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, width), 0},
  {TK_CONFIG_PIXELS, "-x", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, x), 0},
  {TK_CONFIG_PIXELS, "-y", (char *) NULL, (char *) NULL, "",
   Tk_Offset(TkPostscriptInfo, y), 0},
  {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
   (char *) NULL, 0, 0}
};

/*
 * The prolog data. Generated by str2c from prolog.ps
 * This was split in small chunks by str2c because
 * some C compiler have limitations on the size of static strings.
 * (str2c is a small tcl script in tcl's tool directory (source release))
 */
/*
 * This is a stripped down version of that found in tkCanvPs.c of Tk8.1a2.
 * Comments, and stuff pertaining to stipples and other unused entities
 * have been removed
 */
static CONST char * CONST  prolog[]= {
	/* Start of part 1 */
	"%%BeginProlog\n\
50 dict begin\n\
\n\
% This is standard prolog for Postscript generated by Tk's table widget.\n\
% Based of standard prolog for Tk's canvas widget.\n\
\n\
% INITIALIZING VARIABLES\n\
\n\
/baseline 0 def\n\
/height 0 def\n\
/justify 0 def\n\
/cellHeight 0 def\n\
/cellWidth 0 def\n\
/spacing 0 def\n\
/strings 0 def\n\
/xoffset 0 def\n\
/yoffset 0 def\n\
/x 0 def\n\
/y 0 def\n\
\n\
% Define the array ISOLatin1Encoding, if it isn't already present.\n\
\n\
systemdict /ISOLatin1Encoding known not {\n\
    /ISOLatin1Encoding [\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /exclam /quotedbl /numbersign /dollar /percent /ampersand\n\
	    /quoteright\n\
	/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n\
	/zero /one /two /three /four /five /six /seven\n\
	/eight /nine /colon /semicolon /less /equal /greater /question\n\
	/at /A /B /C /D /E /F /G\n\
	/H /I /J /K /L /M /N /O\n\
	/P /Q /R /S /T /U /V /W\n\
	/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n\
	/quoteleft /a /b /c /d /e /f /g\n\
	/h /i /j /k /l /m /n /o\n\
	/p /q /r /s /t /u /v /w\n\
	/x /y /z /braceleft /bar /braceright /asciitilde /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/dotlessi /grave /acute /circumflex /tilde /macron /breve /dotaccent\n\
	/dieresis /space /ring /cedilla /space /hungarumlaut /ogonek /caron\n\
	/space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n\
	/dieresis /copyright /ordfem",

	"inine /guillemotleft /logicalnot /hyphen\n\
	    /registered /macron\n\
	/degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph\n\
	    /periodcentered\n\
	/cedillar /onesuperior /ordmasculine /guillemotright /onequarter\n\
	    /onehalf /threequarters /questiondown\n\
	/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n\
	/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex\n\
	    /Idieresis\n\
	/Eth /Ntilde /Ograve /Oacute /Ocircumflex /Otilde /Odieresis /multiply\n\
	/Oslash /Ugrave /Uacute /Ucircumflex /Udieresis /Yacute /Thorn\n\
	    /germandbls\n\
	/agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n\
	/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex\n\
	    /idieresis\n\
	/eth /ntilde /ograve /oacute /ocircumflex /otilde /odieresis /divide\n\
	/oslash /ugrave /uacute /ucircumflex /udieresis /yacute /thorn\n\
	    /ydieresis\n\
    ] def\n\
} if\n",

	"\n\
% font ISOEncode font\n\
% This procedure changes the encoding of a font from the default\n\
% Postscript encoding to ISOLatin1.  It's typically invoked just\n\
% before invoking \"setfont\".  The body of this procedure comes from\n\
% Section 5.6.1 of the Postscript book.\n\
\n\
/ISOEncode {\n\
    dup length dict begin\n\
	{1 index /FID ne {def} {pop pop} ifelse} forall\n\
	/Encoding ISOLatin1Encoding def\n\
	currentdict\n\
    end\n\
\n\
    % I'm not sure why it's necessary to use \"definefont\" on this new\n\
    % font, but it seems to be important; just use the name \"Temporary\"\n\
    % for the font.\n\
\n\
    /Temporary exch definefont\n\
} bind def\n\
\n\
% -- AdjustColor --\n\
% Given a color value already set for output by the caller, adjusts\n\
% that value to a grayscale or mono value if requested by the CL variable.\n\
\n\
/AdjustColor {\n\
    setrgbcolor\n\
    CL 2 lt {\n\
	currentgray\n\
	CL 0 eq {\n\
	    .5 lt {0} {1} ifelse\n\
	} if\n\
	setgray\n\
    } if\n\
} bind def\n\
\n\
% pointSize fontName SetFont\n\
% The ISOEncode shouldn't be done to Symbol fonts...\n\
/SetFont {\n\
  findfont exch scalefont ISOEncode setfont\n\
} def\n\
\n",

	"% x y strings spacing xoffset yoffset justify ... DrawText --\n\
% This procedure does all of the real work of drawing text.  The\n\
% color and font must already have been set by the caller, and the\n\
% following arguments must be on the stack:\n\
%\n\
% x, y -	Coordinates at which to draw text.\n\
% strings -	An array of strings, one for each line of the text item,\n\
%		in order from top to bottom.\n\
% spacing -	Spacing between lines.\n\
% xoffset -	Horizontal offset for text bbox relative to x and y: 0 for\n\
%		nw/w/sw anchor, -0.5 for n/center/s, and -1.0 for ne/e/se.\n\
% yoffset -	Vertical offset for text bbox relative to x and y: 0 for\n\
%		nw/n/ne anchor, +0.5 for w/center/e, and +1.0 for sw/s/se.\n\
% justify -	0 for left justification, 0.5 for center, 1 for right justify.\n\
% cellWidth -	width for this cell\n\
% cellHeight -	height for this cell\n\
%\n\
% Also, when this procedure is invoked, the color and font must already\n\
% have been set for the text.\n\
\n",

	"/DrawCellText {\n\
    /cellHeight exch def\n\
    /cellWidth exch def\n\
    /justify exch def\n\
    /yoffset exch def\n\
    /xoffset exch def\n\
    /spacing exch def\n\
    /strings exch def\n\
    /y exch def\n\
    /x exch def\n\
\n\
    % Compute the baseline offset and the actual font height.\n\
\n\
    0 0 moveto (TXygqPZ) false charpath\n\
    pathbbox dup /baseline exch def\n\
    exch pop exch sub /height exch def pop\n\
    newpath\n\
\n\
    % Translate coordinates first so that the origin is at the upper-left\n\
    % corner of the text's bounding box. Remember that x and y for\n\
    % positioning are still on the stack.\n\
\n\
    col0 x sub row0 y sub translate\n\
    cellWidth xoffset mul\n\
    strings length 1 sub spacing mul height add yoffset mul translate\n\
\n\
    % Now use the baseline and justification information to translate so\n\
    % that the origin is at the baseline and positioning point for the\n\
    % first line of text.\n\
\n\
    justify cellWidth mul baseline neg translate\n\
\n\
    % Iterate over each of the lines to output it.  For each line,\n\
    % compute its width again so it can be properly justified, then\n\
    % display it.\n\
\n\
    strings {\n\
	dup stringwidth pop\n\
	justify neg mul 0 moveto\n\
	show\n\
	0 spacing neg translate\n\
    } forall\n\
} bind def\n\
\n",

	"%\n\
% x, y -	Coordinates at which to draw text.\n\
% strings -	An array of strings, one for each line of the text item,\n\
%		in order from top to bottom.\n\
% spacing -	Spacing between lines.\n\
% xoffset -	Horizontal offset for text bbox relative to x and y: 0 for\n\
%		nw/w/sw anchor, -0.5 for n/center/s, and -1.0 for ne/e/se.\n\
% yoffset -	Vertical offset for text bbox relative to x and y: 0 for\n\
%		nw/n/ne anchor, +0.5 for w/center/e, and +1.0 for sw/s/se.\n\
% justify -	0 for left justification, 0.5 for center, 1 for right justify.\n\
% cellWidth -	width for this cell\n\
% cellHeight -	height for this cell\n\
%\n\
% Also, when this procedure is invoked, the color and font must already\n\
% have been set for the text.\n\
\n\
/DrawCellTextOld {\n\
    /cellHeight exch def\n\
    /cellWidth exch def\n\
    /justify exch def\n\
    /yoffset exch def\n\
    /xoffset exch def\n\
    /spacing exch def\n\
    /strings exch def\n\
\n\
    % Compute the baseline offset and the actual font height.\n\
\n\
    0 0 moveto (TXygqPZ) false charpath\n\
    pathbbox dup /baseline exch def\n\
    exch pop exch sub /height exch def pop\n\
    newpath\n\
\n\
    % Translate coordinates first so that the origin is at the upper-left\n\
    % corner of the text's bounding box. Remember that x and y for\n\
    % positioning are still on the stack.\n\
\n\
    translate\n\
    cellWidth xoffset mul\n\
    strings length 1 sub spacing mul height add yoffset mul translate\n\
\n\
    % Now use the baseline and justification information to translate so\n\
    % that the origin is at the baseline and positioning point for the\n\
    % first line of text.\n\
\n\
    justify cellWidth mul baseline neg translate\n\
\n\
    % Iterate over each of the lines to output it.  For each line,\n\
    % compute its width again so it can be properly justified, then\n\
    % display it.\n\
\n\
    strings {\n\
	dup stringwidth pop\n\
	justify neg mul 0 moveto\n\
	show\n\
	0 spacing neg translate\n\
    } forall\n\
} bind def\n\
\n\
%%EndProlog\n\
",
	/* End of part 5 */

	NULL	/* End of data marker */
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	GetPostscriptPoints _ANSI_ARGS_((Tcl_Interp *interp,
			char *string, double *doublePtr));
int		Tk_TablePsFont _ANSI_ARGS_((Tcl_Interp *interp,
			Table *tablePtr, Tk_Font tkfont));
int		Tk_TablePsColor _ANSI_ARGS_((Tcl_Interp *interp,
			Table *tablePtr, XColor *colorPtr));
static int	TextToPostscript _ANSI_ARGS_((Tcl_Interp *interp,
			Table *tablePtr, TableTag *tagPtr, int tagX, int tagY,
			int width, int height, int row, int col,
			Tk_TextLayout textLayout));

/*
 * Tcl could really use some more convenience routines...
 * This is just Tcl_DStringAppend for multiple lines, including
 * the full text of each line
 */
void
Tcl_DStringAppendAll TCL_VARARGS_DEF(Tcl_DString *, arg1)
{
    va_list argList;
    Tcl_DString *dstringPtr;
    char *string;

    dstringPtr = TCL_VARARGS_START(Tcl_DString *, arg1, argList);
    while ((string = va_arg(argList, char *)) != NULL) {
      Tcl_DStringAppend(dstringPtr, string, -1);
    }
    va_end(argList);
}

/*
 *--------------------------------------------------------------
 *
 * Table_PostscriptCmd --
 *
 *	This procedure is invoked to process the "postscript" options
 *	of the widget command for table widgets. See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

    /* ARGSUSED */
int
Table_PostscriptCmd(clientData, interp, objc, objv)
     ClientData clientData;	/* Information about table widget. */
     Tcl_Interp *interp;	/* Current interpreter. */
     int objc;			/* Number of argument objects. */
     Tcl_Obj *CONST objv[];
{
#ifdef _WIN32
    /*
     * At the moment, it just doesn't like this code...
     */
    return TCL_OK;
#else
    register Table *tablePtr = (Table *) clientData;
    TkPostscriptInfo psInfo, *oldInfoPtr;
    int result;
    int row, col, firstRow, firstCol, lastRow, lastCol;
    /* dimensions of first and last cell to output */
    int x0, y0, w0, h0, xn, yn, wn, hn;
    int x, y, w, h, i;
#define STRING_LENGTH 400
    char string[STRING_LENGTH+1], *p, **argv;
    size_t length;
    int deltaX = 0, deltaY = 0;	/* Offset of lower-left corner of area to
				 * be marked up, measured in table units
				 * from the positioning point on the page
				 * (reflects anchor position).  Initial
				 * values needed only to stop compiler
				 * warnings. */
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    CONST char * CONST *chunk;
    Tk_TextLayout textLayout = NULL;
    char *value;
    int rowHeight, total, *colWidths, iW, iH;
    TableTag *tagPtr, *colPtr, *rowPtr, *titlePtr;
    Tcl_DString postscript, buffer;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "?option value ...?");
	return TCL_ERROR;
    }

    /*
     *----------------------------------------------------------------
     * Initialize the data structure describing Postscript generation,
     * then process all the arguments to fill the data structure in.
     *----------------------------------------------------------------
     */

    Tcl_DStringInit(&postscript);
    Tcl_DStringInit(&buffer);
    oldInfoPtr = tablePtr->psInfoPtr;
    tablePtr->psInfoPtr = &psInfo;
    /* This is where in the window that we start printing from */
    psInfo.x			= 0;
    psInfo.y			= 0;
    psInfo.width		= -1;
    psInfo.height		= -1;
    psInfo.pageXString		= NULL;
    psInfo.pageYString		= NULL;
    psInfo.pageX		= 72*4.25;
    psInfo.pageY		= 72*5.5;
    psInfo.pageWidthString	= NULL;
    psInfo.pageHeightString	= NULL;
    psInfo.scale		= 1.0;
    psInfo.pageAnchor		= TK_ANCHOR_CENTER;
    psInfo.rotate		= 0;
    psInfo.fontVar		= NULL;
    psInfo.colorVar		= NULL;
    psInfo.colorMode		= NULL;
    psInfo.colorLevel		= 0;
    psInfo.fileName		= NULL;
    psInfo.channelName		= NULL;
    psInfo.chan			= NULL;
    psInfo.first		= NULL;
    psInfo.last			= NULL;
    Tcl_InitHashTable(&psInfo.fontTable, TCL_STRING_KEYS);

    /*
     * The magic StringifyObjects
     */
    argv = (char **) ckalloc((objc + 1) * sizeof(char *));
    for (i = 0; i < objc; i++)
	argv[i] = Tcl_GetString(objv[i]);
    argv[i] = NULL;

    result = Tk_ConfigureWidget(interp, tablePtr->tkwin, configSpecs,
				objc-2, argv+2, (char *) &psInfo,
				TK_CONFIG_ARGV_ONLY);
    if (result != TCL_OK) {
	goto cleanup;
    }

    if (psInfo.first == NULL) {
	firstRow = 0;
	firstCol = 0;
    } else if (TableGetIndex(tablePtr, psInfo.first, &firstRow, &firstCol)
	       != TCL_OK) {
	result = TCL_ERROR;
	goto cleanup;
    }
    if (psInfo.last == NULL) {
	lastRow = tablePtr->rows-1;
	lastCol = tablePtr->cols-1;
    } else if (TableGetIndex(tablePtr, psInfo.last, &lastRow, &lastCol)
	       != TCL_OK) {
	result = TCL_ERROR;
	goto cleanup;
    }

    if (psInfo.fileName != NULL) {
	/* Check that -file and -channel are not both specified. */
	if (psInfo.channelName != NULL) {
	    Tcl_AppendResult(interp, "can't specify both -file",
			     " and -channel", (char *) NULL);
	    result = TCL_ERROR;
	    goto cleanup;
	}

	/*
	 * Check that we are not in a safe interpreter. If we are, disallow
	 * the -file specification.
	 */
	if (Tcl_IsSafe(interp)) {
	    Tcl_AppendResult(interp, "can't specify -file in a",
			     " safe interpreter", (char *) NULL);
	    result = TCL_ERROR;
	    goto cleanup;
	}

	p = Tcl_TranslateFileName(interp, psInfo.fileName, &buffer);
	if (p == NULL) {
	    result = TCL_ERROR;
	    goto cleanup;
	}
	psInfo.chan = Tcl_OpenFileChannel(interp, p, "w", 0666);
	Tcl_DStringFree(&buffer);
	Tcl_DStringInit(&buffer);
	if (psInfo.chan == NULL) {
	    result = TCL_ERROR;
	    goto cleanup;
	}
    }

    if (psInfo.channelName != NULL) {
	int mode;
	/*
	 * Check that the channel is found in this interpreter and that it
	 * is open for writing.
	 */
	psInfo.chan = Tcl_GetChannel(interp, psInfo.channelName, &mode);
	if (psInfo.chan == (Tcl_Channel) NULL) {
	    result = TCL_ERROR;
	    goto cleanup;
	}
	if ((mode & TCL_WRITABLE) == 0) {
	    Tcl_AppendResult(interp, "channel \"", psInfo.channelName,
			     "\" wasn't opened for writing", (char *) NULL);
	    result = TCL_ERROR;
	    goto cleanup;
	}
    }

    if (psInfo.colorMode == NULL) {
	psInfo.colorLevel = 2;
    } else {
	length = strlen(psInfo.colorMode);
	if (strncmp(psInfo.colorMode, "monochrome", length) == 0) {
	    psInfo.colorLevel = 0;
	} else if (strncmp(psInfo.colorMode, "gray", length) == 0) {
	    psInfo.colorLevel = 1;
	} else if (strncmp(psInfo.colorMode, "color", length) == 0) {
	    psInfo.colorLevel = 2;
	} else {
	    Tcl_AppendResult(interp, "bad color mode \"", psInfo.colorMode,
			     "\": must be monochrome, gray or color", (char *) NULL);
	    goto cleanup;
	}
    }

    TableCellCoords(tablePtr, firstRow, firstCol, &x0, &y0, &w0, &h0);
    TableCellCoords(tablePtr, lastRow, lastCol, &xn, &yn, &wn, &hn);
    psInfo.x = x0;
    psInfo.y = y0;
    if (psInfo.width == -1) {
	psInfo.width = xn+wn;
    }
    if (psInfo.height == -1) {
	psInfo.height = yn+hn;
    }
    psInfo.x2 = psInfo.x + psInfo.width;
    psInfo.y2 = psInfo.y + psInfo.height;

    if (psInfo.pageXString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageXString,
				&psInfo.pageX) != TCL_OK) {
	    goto cleanup;
	}
    }
    if (psInfo.pageYString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageYString,
				&psInfo.pageY) != TCL_OK) {
	    goto cleanup;
	}
    }
    if (psInfo.pageWidthString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageWidthString,
				&psInfo.scale) != TCL_OK) {
	    goto cleanup;
	}
	psInfo.scale /= psInfo.width;
    } else if (psInfo.pageHeightString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageHeightString,
				&psInfo.scale) != TCL_OK) {
	    goto cleanup;
	}
	psInfo.scale /= psInfo.height;
    } else {
	psInfo.scale = (72.0/25.4)*WidthMMOfScreen(Tk_Screen(tablePtr->tkwin))
	    / WidthOfScreen(Tk_Screen(tablePtr->tkwin));
    }
    switch (psInfo.pageAnchor) {
    case TK_ANCHOR_NW:
    case TK_ANCHOR_W:
    case TK_ANCHOR_SW:
	deltaX = 0;
	break;
    case TK_ANCHOR_N:
    case TK_ANCHOR_CENTER:
    case TK_ANCHOR_S:
	deltaX = -psInfo.width/2;
	break;
    case TK_ANCHOR_NE:
    case TK_ANCHOR_E:
    case TK_ANCHOR_SE:
	deltaX = -psInfo.width;
	break;
    }
    switch (psInfo.pageAnchor) {
    case TK_ANCHOR_NW:
    case TK_ANCHOR_N:
    case TK_ANCHOR_NE:
	deltaY = - psInfo.height;
	break;
    case TK_ANCHOR_W:
    case TK_ANCHOR_CENTER:
    case TK_ANCHOR_E:
	deltaY = -psInfo.height/2;
	break;
    case TK_ANCHOR_SW:
    case TK_ANCHOR_S:
    case TK_ANCHOR_SE:
	deltaY = 0;
	break;
    }

    /*
     *--------------------------------------------------------
     * Make a PREPASS over all of the tags
     * to collect information about all the fonts in use, so that
     * we can output font information in the proper form required
     * by the Document Structuring Conventions.
     *--------------------------------------------------------
     */

    Tk_TablePsFont(interp, tablePtr, tablePtr->defaultTag.tkfont);
    Tcl_ResetResult(interp);
    for (hPtr = Tcl_FirstHashEntry(tablePtr->tagTable, &search);
	 hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	tagPtr = (TableTag *) Tcl_GetHashValue(hPtr);
	if (tagPtr->tkfont != NULL) {
	    Tk_TablePsFont(interp, tablePtr, tagPtr->tkfont);
	}
    }
    Tcl_ResetResult(interp);

    /*
     *--------------------------------------------------------
     * Generate the header and prolog for the Postscript.
     *--------------------------------------------------------
     */

    sprintf(string, " %d,%d => %d,%d\n", firstRow, firstCol, lastRow, lastCol);
    Tcl_DStringAppendAll(&postscript,
			 "%!PS-Adobe-3.0 EPSF-3.0\n",
			 "%%Creator: Tk Table Widget ", TBL_VERSION, "\n",
			 "%%Title: Window ",
			 Tk_PathName(tablePtr->tkwin), string,
			 "%%BoundingBox: ",
			 (char *) NULL);
    if (!psInfo.rotate) {
	sprintf(string, "%d %d %d %d\n",
		(int) (psInfo.pageX + psInfo.scale*deltaX),
		(int) (psInfo.pageY + psInfo.scale*deltaY),
		(int) (psInfo.pageX + psInfo.scale*(deltaX + psInfo.width)
		       + 1.0),
		(int) (psInfo.pageY + psInfo.scale*(deltaY + psInfo.height)
		       + 1.0));
    } else {
	sprintf(string, "%d %d %d %d\n",
		(int) (psInfo.pageX - psInfo.scale*(deltaY + psInfo.height)),
		(int) (psInfo.pageY + psInfo.scale*deltaX),
		(int) (psInfo.pageX - psInfo.scale*deltaY + 1.0),
		(int) (psInfo.pageY + psInfo.scale*(deltaX + psInfo.width)
		       + 1.0));
    }
    Tcl_DStringAppendAll(&postscript, string,
			 "%%Pages: 1\n%%DocumentData: Clean7Bit\n",
			 "%%Orientation: ",
			 psInfo.rotate?"Landscape\n":"Portrait\n",
			 (char *) NULL);
    p = "%%DocumentNeededResources: font ";
    for (hPtr = Tcl_FirstHashEntry(&psInfo.fontTable, &search);
	 hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	sprintf(string, "%s%s\n", p, Tcl_GetHashKey(&psInfo.fontTable, hPtr));
	Tcl_DStringAppend(&postscript, string, -1);
	p = "%%+ font ";
    }
    Tcl_DStringAppend(&postscript, "%%EndComments\n\n", -1);

    /*
     * Insert the prolog
     */
    for (chunk=prolog; *chunk; chunk++) {
	Tcl_DStringAppend(&postscript, *chunk, -1);
    }

    if (psInfo.chan != NULL) {
	Tcl_Write(psInfo.chan, Tcl_DStringValue(&postscript), -1);
	Tcl_DStringFree(&postscript);
	Tcl_DStringInit(&postscript);
    }

    /*
     * Document setup:  set the color level and include fonts.
     * This is where we start using &postscript
     */

    sprintf(string, "/CL %d def\n", psInfo.colorLevel);
    Tcl_DStringAppendAll(&postscript, "%%BeginSetup\n", string, (char *) NULL);
    for (hPtr = Tcl_FirstHashEntry(&psInfo.fontTable, &search);
	 hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	sprintf(string, "%s%s\n", "%%IncludeResource: font ",
		Tcl_GetHashKey(&psInfo.fontTable, hPtr));
	Tcl_DStringAppend(&postscript, string, -1);
    }
    Tcl_DStringAppend(&postscript, "%%EndSetup\n\n", -1);

    /*
     * Page setup:  move to page positioning point, rotate if
     * needed, set scale factor, offset for proper anchor position,
     * and set clip region.
     */

    sprintf(string, "%.1f %.1f translate\n",
	    psInfo.pageX, psInfo.pageY);
    Tcl_DStringAppendAll(&postscript, "%%Page: 1 1\nsave\n",
			 string, psInfo.rotate?"90 rotate\n":"",
			 (char *) NULL);
    sprintf(string, "%.4g %.4g scale\n%d %d translate\n",
	    psInfo.scale, psInfo.scale, deltaX - psInfo.x, deltaY);
    Tcl_DStringAppend(&postscript, string, -1);
    sprintf(string, "%d %.15g moveto %d %.15g lineto %d %.15g lineto %d %.15g",
	    psInfo.x, (double) psInfo.y2-psInfo.y,
	    psInfo.x2,(double) psInfo.y2-psInfo.y,
	    psInfo.x2, 0.0, psInfo.x, 0.0);
    Tcl_DStringAppend(&postscript, string, -1);
    Tcl_DStringAppend(&postscript, " lineto closepath clip newpath\n", -1);
    if (psInfo.chan != NULL) {
	Tcl_Write(psInfo.chan, Tcl_DStringValue(&postscript), -1);
	Tcl_DStringFree(&postscript);
	Tcl_DStringInit(&postscript);
    }

    /*
     * Go through each cell, calculating full desired height
     */
    result = TCL_OK;

    hPtr = Tcl_FindHashEntry(tablePtr->tagTable, "title");
    titlePtr = (TableTag *) Tcl_GetHashValue(hPtr);

    total = 0;
    colWidths = (int *) ckalloc((lastCol-firstCol) * sizeof(int));
    for (col = 0; col <= lastCol-firstCol; col++) colWidths[col] = 0;
    Tcl_DStringAppend(&buffer, "gsave\n", -1);
    for (row = firstRow; row <= lastRow; row++) {
	rowHeight = 0;
	rowPtr = FindRowColTag(tablePtr, row+tablePtr->rowOffset, ROW);
	for (col = firstCol; col <= lastCol; col++) {
	    /* get the coordinates for the cell */
	    TableCellCoords(tablePtr, row, col, &x, &y, &w, &h);
	    if ((x >= psInfo.x2) || (x+w < psInfo.x) ||
		(y >= psInfo.y2) || (y+h < psInfo.y)) {
		continue;
	    }

	    if (row == tablePtr->activeRow && col == tablePtr->activeCol) {
		value = tablePtr->activeBuf;
	    } else {
		value = TableGetCellValue(tablePtr, row+tablePtr->rowOffset,
					  col+tablePtr->colOffset);
	    }
	    if (!strlen(value)) {
		continue;
	    }

	    /* Create the tag here */
	    tagPtr = TableNewTag();
	    /* First, merge in the default tag */
	    TableMergeTag(tagPtr, &(tablePtr->defaultTag));

	    colPtr = FindRowColTag(tablePtr, col+tablePtr->colOffset, COL);
	    if (colPtr != (TableTag *) NULL) TableMergeTag(tagPtr, colPtr);
	    if (rowPtr != (TableTag *) NULL) TableMergeTag(tagPtr, rowPtr);
	    /* Am I in the titles */
	    if (row < tablePtr->topRow || col < tablePtr->leftCol) {
		TableMergeTag(tagPtr, titlePtr);
	    }
	    /* Does this have a cell tag */
	    TableMakeArrayIndex(row+tablePtr->rowOffset,
				col+tablePtr->colOffset, string);
	    hPtr = Tcl_FindHashEntry(tablePtr->cellStyles, string);
	    if (hPtr != NULL) {
		TableMergeTag(tagPtr, (TableTag *) Tcl_GetHashValue(hPtr));
	    }

	    /*
	     * the use of -1 instead of Tcl_NumUtfChars means we don't
	     * pass NULLs to postscript
	     */
	    textLayout = Tk_ComputeTextLayout(tagPtr->tkfont, value, -1,
					      (tagPtr->wrap>0) ? w : 0,
					      tagPtr->justify,
					      (tagPtr->multiline>0) ? 0 :
					      TK_IGNORE_NEWLINES, &iW, &iH);

	    rowHeight = MAX(rowHeight, iH);
	    colWidths[col-firstCol] = MAX(colWidths[col-firstCol], iW);

	    result = TextToPostscript(interp, tablePtr, tagPtr,
				      x, y, iW, iH, row, col, textLayout);
	    Tk_FreeTextLayout(textLayout);
	    if (result != TCL_OK) {
		char msg[64 + TCL_INTEGER_SPACE];

		sprintf(msg, "\n    (generating Postscript for cell %s)",
			string);
		Tcl_AddErrorInfo(interp, msg);
		goto cleanup;
	    }
	    Tcl_DStringAppend(&buffer, Tcl_GetStringResult(interp), -1);
	}
	sprintf(string, "/row%d %d def\n",
		row, tablePtr->psInfoPtr->y2 - total);
	Tcl_DStringAppend(&postscript, string, -1);
	total += rowHeight + 2*tablePtr->defaultTag.bd;
    }
    Tcl_DStringAppend(&buffer, "grestore\n", -1);
    sprintf(string, "/row%d %d def\n", row, tablePtr->psInfoPtr->y2 - total);
    Tcl_DStringAppend(&postscript, string, -1);

    total = tablePtr->defaultTag.bd;
    for (col = firstCol; col <= lastCol; col++) {
	sprintf(string, "/col%d %d def\n", col, total);
	Tcl_DStringAppend(&postscript, string, -1);
	total += colWidths[col-firstCol] + 2*tablePtr->defaultTag.bd;
    }
    sprintf(string, "/col%d %d def\n", col, total);
    Tcl_DStringAppend(&postscript, string, -1);

    Tcl_DStringAppend(&postscript, Tcl_DStringValue(&buffer), -1);

    /*
     * Output to channel at the end of it all
     * This should more incremental, but that can't be avoided in order
     * to post-define width/height of the cols/rows
     */
    if (psInfo.chan != NULL) {
	Tcl_Write(psInfo.chan, Tcl_DStringValue(&postscript), -1);
	Tcl_DStringFree(&postscript);
	Tcl_DStringInit(&postscript);
    }

    /*
     *---------------------------------------------------------------------
     * Output page-end information, such as commands to print the page
     * and document trailer stuff.
     *---------------------------------------------------------------------
     */

    Tcl_DStringAppend(&postscript,
		      "restore showpage\n\n%%Trailer\nend\n%%EOF\n", -1);
    if (psInfo.chan != NULL) {
	Tcl_Write(psInfo.chan, Tcl_DStringValue(&postscript), -1);
	Tcl_DStringFree(&postscript);
	Tcl_DStringInit(&postscript);
    }

    /*
   * Clean up psInfo to release malloc'ed stuff.
   */

cleanup:
    ckfree((char *) argv);
    Tcl_DStringResult(interp, &postscript);
    Tcl_DStringFree(&postscript);
    Tcl_DStringFree(&buffer);
    if (psInfo.first != NULL) {
	ckfree(psInfo.first);
    }
    if (psInfo.last != NULL) {
	ckfree(psInfo.last);
    }
    if (psInfo.pageXString != NULL) {
	ckfree(psInfo.pageXString);
    }
    if (psInfo.pageYString != NULL) {
	ckfree(psInfo.pageYString);
    }
    if (psInfo.pageWidthString != NULL) {
	ckfree(psInfo.pageWidthString);
    }
    if (psInfo.pageHeightString != NULL) {
	ckfree(psInfo.pageHeightString);
    }
    if (psInfo.fontVar != NULL) {
	ckfree(psInfo.fontVar);
    }
    if (psInfo.colorVar != NULL) {
	ckfree(psInfo.colorVar);
    }
    if (psInfo.colorMode != NULL) {
	ckfree(psInfo.colorMode);
    }
    if (psInfo.fileName != NULL) {
	ckfree(psInfo.fileName);
    }
    if ((psInfo.chan != NULL) && (psInfo.channelName == NULL)) {
	Tcl_Close(interp, psInfo.chan);
    }
    if (psInfo.channelName != NULL) {
	ckfree(psInfo.channelName);
    }
    Tcl_DeleteHashTable(&psInfo.fontTable);
    tablePtr->psInfoPtr = oldInfoPtr;
    return result;
#endif
}

/*
 *--------------------------------------------------------------
 *
 * Tk_TablePsColor --
 *
 *	This procedure is called by individual table items when
 *	they want to set a color value for output.  Given information
 *	about an X color, this procedure will generate Postscript
 *	commands to set up an appropriate color in Postscript.
 *
 * Results:
 *	Returns a standard Tcl return value.  If an error occurs
 *	then an error message will be left in the interp's result.
 *	If no error occurs, then additional Postscript will be
 *	appended to the interp's result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_TablePsColor(interp, tablePtr, colorPtr)
     Tcl_Interp *interp;		/* Interpreter for returning Postscript
					 * or error message. */
     Table *tablePtr;			/* Information about table. */
     XColor *colorPtr;			/* Information about color. */
{
    TkPostscriptInfo *psInfoPtr = tablePtr->psInfoPtr;
    int tmp;
    double red, green, blue;
    char string[200];

    /*
     * If there is a color map defined, then look up the color's name
     * in the map and use the Postscript commands found there, if there
     * are any.
     */

    if (psInfoPtr->colorVar != NULL) {
	char *cmdString;

	cmdString = Tcl_GetVar2(interp, psInfoPtr->colorVar,
				Tk_NameOfColor(colorPtr), 0);
	if (cmdString != NULL) {
	    Tcl_AppendResult(interp, cmdString, "\n", (char *) NULL);
	    return TCL_OK;
	}
    }

    /*
     * No color map entry for this color.  Grab the color's intensities
     * and output Postscript commands for them.  Special note:  X uses
     * a range of 0-65535 for intensities, but most displays only use
     * a range of 0-255, which maps to (0, 256, 512, ... 65280) in the
     * X scale.  This means that there's no way to get perfect white,
     * since the highest intensity is only 65280 out of 65535.  To
     * work around this problem, rescale the X intensity to a 0-255
     * scale and use that as the basis for the Postscript colors.  This
     * scheme still won't work if the display only uses 4 bits per color,
     * but most diplays use at least 8 bits.
     */

    tmp = colorPtr->red;
    red = ((double) (tmp >> 8))/255.0;
    tmp = colorPtr->green;
    green = ((double) (tmp >> 8))/255.0;
    tmp = colorPtr->blue;
    blue = ((double) (tmp >> 8))/255.0;
    sprintf(string, "%.3f %.3f %.3f AdjustColor\n",
	    red, green, blue);
    Tcl_AppendResult(interp, string, (char *) NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_TablePsFont --
 *
 *	This procedure is called by individual table items when
 *	they want to output text.  Given information about an X
 *	font, this procedure will generate Postscript commands
 *	to set up an appropriate font in Postscript.
 *
 * Results:
 *	Returns a standard Tcl return value.  If an error occurs
 *	then an error message will be left in the interp's result.
 *	If no error occurs, then additional Postscript will be
 *	appended to the interp's result.
 *
 * Side effects:
 *	The Postscript font name is entered into psInfoPtr->fontTable
 *	if it wasn't already there.
 *
 *--------------------------------------------------------------
 */

int
Tk_TablePsFont(interp, tablePtr, tkfont)
     Tcl_Interp *interp;		/* Interpreter for returning Postscript
					 * or error message. */
     Table *tablePtr;			/* Information about table. */
     Tk_Font tkfont;			/* Information about font in which text
					 * is to be printed. */
{
    TkPostscriptInfo *psInfoPtr = tablePtr->psInfoPtr;
    char *end;
    char pointString[TCL_INTEGER_SPACE];
    Tcl_DString ds;
    int i, points;

    /*
     * First, look up the font's name in the font map, if there is one.
     * If there is an entry for this font, it consists of a list
     * containing font name and size.  Use this information.
     */

    Tcl_DStringInit(&ds);
    
    if (psInfoPtr->fontVar != NULL) {
	char *list, **argv;
	int objc;
	double size;
	char *name;

	name = Tk_NameOfFont(tkfont);
	list = Tcl_GetVar2(interp, psInfoPtr->fontVar, name, 0);
	if (list != NULL) {
	    if (Tcl_SplitList(interp, list, &objc, &argv) != TCL_OK) {
	    badMapEntry:
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "bad font map entry for \"", name,
				 "\": \"", list, "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    if (objc != 2) {
		goto badMapEntry;
	    }
	    size = strtod(argv[1], &end);
	    if ((size <= 0) || (*end != 0)) {
		goto badMapEntry;
	    }

	    Tcl_DStringAppend(&ds, argv[0], -1);
	    points = (int) size;
	    
	    ckfree((char *) argv);
	    goto findfont;
	}
    } 

    points = Tk_PostscriptFontName(tkfont, &ds);

findfont:
    sprintf(pointString, "%d", points);
    Tcl_AppendResult(interp, pointString, " /", Tcl_DStringValue(&ds),
		     " SetFont\n", (char *) NULL);
    Tcl_CreateHashEntry(&psInfoPtr->fontTable, Tcl_DStringValue(&ds), &i);
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * GetPostscriptPoints --
 *
 *	Given a string, returns the number of Postscript points
 *	corresponding to that string.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	screen distance is stored at *doublePtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	the interp's result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetPostscriptPoints(interp, string, doublePtr)
     Tcl_Interp *interp;		/* Use this for error reporting. */
     char *string;		/* String describing a screen distance. */
     double *doublePtr;		/* Place to store converted result. */
{
    char *end;
    double d;

    d = strtod(string, &end);
    if (end == string) {
    error:
	Tcl_AppendResult(interp, "bad distance \"", string,
			 "\"", (char *) NULL);
	return TCL_ERROR;
    }
#define UCHAR(c) ((unsigned char) (c))
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    switch (*end) {
    case 'c':
	d *= 72.0/2.54;
	end++;
	break;
    case 'i':
	d *= 72.0;
	end++;
	break;
    case 'm':
	d *= 72.0/25.4;
	end++;
	break;
    case 0:
	break;
    case 'p':
	end++;
	break;
    default:
	goto error;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto error;
    }
    *doublePtr = d;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TextToPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	text items.
 *
 * Results:
 *	The return value is a standard Tcl result.  If an error
 *	occurs in generating Postscript then an error message is
 *	left in the interp's result, replacing whatever used
 *	to be there.  If no error occurs, then Postscript for the
 *	item is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
TextToPostscript(interp, tablePtr, tagPtr, tagX, tagY, width, height,
		 row, col, textLayout)
     Tcl_Interp *interp;	/* Leave Postscript or error message here. */
     Table *tablePtr;		/* Information about overall canvas. */
     TableTag *tagPtr;		/*  */
     int tagX, tagY;		/*  */
     int width, height;		/*  */
     int row, col;		/*  */
     Tk_TextLayout textLayout;	/*  */
{
    int x, y;
    Tk_FontMetrics fm;
    char *justify;
    char buffer[500];
    Tk_3DBorder fg = tagPtr->fg;

    if (fg == NULL) {
	fg = tablePtr->defaultTag.fg;
    }

    if (Tk_TablePsFont(interp, tablePtr, tagPtr->tkfont) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_TablePsColor(interp, tablePtr, Tk_3DBorderColor(fg)) != TCL_OK) {
	return TCL_ERROR;
    }

    sprintf(buffer, "%% %.15g %.15g [\n", (tagX+width)/2.0,
	    tablePtr->psInfoPtr->y2 - ((tagY+height)/2.0));
    Tcl_AppendResult(interp, buffer, (char *) NULL);
    sprintf(buffer, "col%d row%d [\n", col, row);
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    Tk_TextLayoutToPostscript(interp, textLayout);

    x = 0;  y = 0;  justify = NULL;	/* lint. */
    switch (tagPtr->anchor) {
    case TK_ANCHOR_NW:		x = 0; y = 0;	break;
    case TK_ANCHOR_N:		x = 1; y = 0;	break;
    case TK_ANCHOR_NE:		x = 2; y = 0;	break;
    case TK_ANCHOR_E:		x = 2; y = 1;	break;
    case TK_ANCHOR_SE:		x = 2; y = 2;	break;
    case TK_ANCHOR_S:		x = 1; y = 2;	break;
    case TK_ANCHOR_SW:		x = 0; y = 2;	break;
    case TK_ANCHOR_W:		x = 0; y = 1;	break;
    case TK_ANCHOR_CENTER:	x = 1; y = 1;	break;
    }
    switch (tagPtr->justify) {
    case TK_JUSTIFY_RIGHT:	justify = "1";	break;
    case TK_JUSTIFY_CENTER:	justify = "0.5";break;
    case TK_JUSTIFY_LEFT:	justify = "0";
    }

    Tk_GetFontMetrics(tagPtr->tkfont, &fm);
    sprintf(buffer, "] %d %g %g %s %d %d DrawCellText\n",
	    fm.linespace, (x / -2.0), (y / 2.0), justify,
	    width, height);
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    return TCL_OK;
}
