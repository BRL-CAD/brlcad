/*                        S C R E E N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file screen.h
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651

	$Header$ (BRL)
*/

#define TITLE_PTR		&template[ 0][ 7]
#define TIMER_PTR		&template[ 1][ 7]
#define F_SCRIPT_PTR		&template[ 2][16]
#define GRID_XOF_PTR		&template[ 2][67]
#define F_ERRORS_PTR		&template[ 3][16]
#define GRID_YOF_PTR		&template[ 3][67]
#define F_MAT_DB_PTR		&template[ 4][16]
#define GRID_DIS_PTR		&template[ 4][67]
#define F_LGT_DB_PTR		&template[ 5][16]
#define VU_SIZE_PTR		&template[ 5][67]
#define F_RASTER_PTR		&template[ 6][16]
#define MODEL_RA_PTR		&template[ 6][67]
#define BACKGROU_PTR		&template[ 7][67]
#define BUFFERED_PTR		&template[ 8][16]
#define DEBUGGER_PTR		&template[ 8][29]
#define MAX_BOUN_PTR		&template[ 8][51]
#define IR_AUTO_MAP_PTR		&template[ 8][53]
#define IR_PAINT_PTR		&template[ 8][72]
#define PROGRAM_NM_PTR		&template[ 9][ 2]
#define F_GED_DB_PTR		&template[ 9][23]
#define GRID_PIX_PTR		&template[ 9][49]
#define GRID_SIZ_PTR		&template[ 9][56]
#define GRID_SCN_PTR		&template[ 9][61]
#define GRID_FIN_PTR		&template[ 9][66]
#define FRAME_NO_PTR		&template[ 9][71]
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

extern char			template[][TEMPLATE_COLS];

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
