/*
 * bltGrLegd.h --
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 */

#ifndef _BLT_GR_LEGEND_H
#define _BLT_GR_LEGEND_H

#define LEGEND_RIGHT	(1<<0)	/* Right margin */
#define LEGEND_LEFT	(1<<1)	/* Left margin */
#define LEGEND_BOTTOM	(1<<2)	/* Bottom margin */
#define LEGEND_TOP	(1<<3)	/* Top margin, below the graph title. */
#define LEGEND_PLOT	(1<<4)	/* Plot area */
#define LEGEND_XY	(1<<5)	/* Screen coordinates in the plotting 
				 * area. */
#define LEGEND_WINDOW	(1<<6)	/* External window. */
#define LEGEND_IN_MARGIN \
	(LEGEND_RIGHT | LEGEND_LEFT | LEGEND_BOTTOM | LEGEND_TOP)
#define LEGEND_IN_PLOT  (LEGEND_PLOT | LEGEND_XY)

extern int Blt_CreateLegend _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyLegend _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DrawLegend _ANSI_ARGS_((Legend *legendPtr, Drawable drawable));
extern void Blt_MapLegend _ANSI_ARGS_((Legend *legendPtr, int width,
	int height));
extern int Blt_LegendOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_LegendSite _ANSI_ARGS_((Legend *legendPtr));
extern int Blt_LegendWidth _ANSI_ARGS_((Legend *legendPtr));
extern int Blt_LegendHeight _ANSI_ARGS_((Legend *legendPtr));
extern int Blt_LegendIsHidden _ANSI_ARGS_((Legend *legendPtr));
extern int Blt_LegendIsRaised _ANSI_ARGS_((Legend *legendPtr));
extern int Blt_LegendX _ANSI_ARGS_((Legend *legendPtr));
extern int Blt_LegendY _ANSI_ARGS_((Legend *legendPtr));
extern void Blt_LegendRemoveElement _ANSI_ARGS_((Legend *legendPtr, 
	Element *elemPtr));
#endif /* BLT_GR_LEGEND_H */
