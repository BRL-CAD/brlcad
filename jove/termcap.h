/*
 *			T E R M C A P . H 
 *
 * $Revision$
 *
 * $Log$
 */

/* Termcap definitions */

extern char	*UP,	/* Scroll reverse, or up */
		*CS,	/* If on vt100 */
		*SO,	/* Start standout */
		*SE,	/* End standout */
		*CM,	/* The cursor motion string */
		*CL,	/* Clear screen */
		*CE,	/* Clear to end of line */
		*HO,	/* Home cursor */
		*AL,	/* Addline (insert line) */
		*DL,	/* Delete line */
		*IS,	/* Initial start */
		*VS,	/* Visual start */
		*VE,	/* Visual end */
		*IC,	/* Insert char */
		*DC,	/* Delete char */
		*IM,	/* Insert mode */
		*EI,	/* End insert mode */
		*LL,	/* Last line, first column */
		*SR,	/* Scroll reverse */
		*VB;	/* Visible bell */

extern int LI;		/* Number of lines */
extern int CO;		/* Number of columns */

extern int TABS;	/* Whether we are in tabs mode */
extern int UpLen;	/* Length of the UP string */
extern int HomeLen;	/* Length of Home string */
extern int LowerLen;	/* Length of lower string */

extern int	BG;	/* Are we on a bitgraph */

extern char PC;
extern char *BC;	/* Back space */
extern int ospeed;
