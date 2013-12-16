/*                        S C R E E N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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
 */
/** @file lgt/screen.h
    Author:		Gary S. Moss
*/

#ifndef LGT_SCREEN_H
#define  LGT_SCREEN_H

#define TITLE_PTR		&screen_template[ 0][ 7]
#define TIMER_PTR		&screen_template[ 1][ 7]
#define F_SCRIPT_PTR		&screen_template[ 2][16]
#define GRID_XOF_PTR		&screen_template[ 2][67]
#define F_ERRORS_PTR		&screen_template[ 3][16]
#define GRID_YOF_PTR		&screen_template[ 3][67]
#define F_MAT_DB_PTR		&screen_template[ 4][16]
#define GRID_DIS_PTR		&screen_template[ 4][67]
#define F_LGT_DB_PTR		&screen_template[ 5][16]
#define VU_SIZE_PTR		&screen_template[ 5][67]
#define F_RASTER_PTR		&screen_template[ 6][16]
#define MODEL_RA_PTR		&screen_template[ 6][67]
#define BACKGROU_PTR		&screen_template[ 7][67]
#define BUFFERED_PTR		&screen_template[ 8][16]
#define DEBUGGER_PTR		&screen_template[ 8][29]
#define MAX_BOUN_PTR		&screen_template[ 8][51]
#define IR_AUTO_MAP_PTR		&screen_template[ 8][53]
#define IR_PAINT_PTR		&screen_template[ 8][72]
#define PROGRAM_NM_PTR		&screen_template[ 9][ 2]
#define F_GED_DB_PTR		&screen_template[ 9][23]
#define GRID_PIX_PTR		&screen_template[ 9][49]
#define GRID_SIZ_PTR		&screen_template[ 9][56]
#define GRID_SCN_PTR		&screen_template[ 9][61]
#define GRID_FIN_PTR		&screen_template[ 9][66]
#define FRAME_NO_PTR		&screen_template[ 9][71]
#define TOP_SCROLL_WIN		11		/* Next available line.	*/
#define TOP_MOVE()		MvCursor( 1, 1 )
#define SCROLL_DL_MOVE()	MvCursor( 1, TOP_SCROLL_WIN )
#define SCROLL_PR_MOVE()	MvCursor( 1, PROMPT_LINE - 1 )
#define PROMPT_MOVE()		MvCursor( 1, PROMPT_LINE )
#define EVENT_MOVE()		MvCursor( 1, PROMPT_LINE+1 )
#define IDLE_MOVE()		EVENT_MOVE()
#define PROMPT_LINE		((li-1))
#define TEMPLATE_COLS		79
extern int			LI; /* From "libcursor.a".		*/
extern int			li; /* Actual # of lines in window.	*/

extern char			screen_template[][TEMPLATE_COLS+1];

#endif /* LGT_SCREEN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
