/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
/*
	Originally extracted from SCCS archive:
		SCCS id:	@(#) screen.h	2.2
		Modified: 	1/30/87 at 17:22:15	G S M
		Retrieved: 	2/4/87 at 08:53:28
		SCCS archive:	/vld/moss/src/lgt/s.screen.h
*/

#define TOP_MOVE()		MvCursor(  1, 1 )
#define TITLE_MOVE()		MvCursor(  8, 1 )
#define TIMER_MOVE()		MvCursor(  8, 2 )
#define F_SCRIPT_MOVE()		MvCursor( 17, 3 )
#define FIELD_OF_VU_MOVE()	MvCursor( 73, 3 )
#define F_ERRORS_MOVE()		MvCursor( 17, 4 )
#define GRID_DIS_MOVE()		MvCursor( 68, 4 )
#define F_MAT_DB_MOVE()		MvCursor( 17, 5 )
#define GRID_XOF_MOVE()		MvCursor( 68, 5 )
#define F_LGT_DB_MOVE()		MvCursor( 17, 6 )
#define GRID_YOF_MOVE()		MvCursor( 68, 6 )
#define F_RASTER_MOVE()		MvCursor( 17, 7 )
#define MODEL_RA_MOVE()		MvCursor( 68, 7 )
#define F_TEXTURE_MOVE()	MvCursor( 17, 8 )
#define BACKGROU_MOVE()		MvCursor( 68, 8 )
#define BUFFERED_MOVE()		MvCursor( 17, 9 )
#define DEBUGGER_MOVE()		MvCursor( 30, 9 )
#define MAX_BOUN_MOVE()		MvCursor( 52, 9 )
#define IR_AUTO_MAP_MOVE()	MvCursor( 54, 9 )
#define IR_PAINT_MOVE()		MvCursor( 73, 9 )
#define PROGRAM_NM_MOVE()	MvCursor( 3, 10 )
#define F_GED_DB_MOVE()		MvCursor( 20, 10 )
#define GRID_PIX_MOVE()		MvCursor( 53, 10 )
#define GRID_SIZ_MOVE()		MvCursor( 60, 10 )
#define GRID_SCN_MOVE()		MvCursor( 65, 10 )
#define GRID_FIN_MOVE()		MvCursor( 70, 10 )
#define TOP_SCROLL_WIN		11		/* Next available line.	*/
#define SCROLL_DL_MOVE()	MvCursor( 1, TOP_SCROLL_WIN )
#define SCROLL_PR_MOVE()	MvCursor( 1, PROMPT_LINE - 1 )
#define PROMPT_MOVE()		MvCursor( 1, PROMPT_LINE )
#define EVENT_MOVE()		MvCursor( 1, PROMPT_LINE+1 )
#define IDLE_MOVE()		EVENT_MOVE()
#define PROMPT_LINE		((li-1))
extern int			LI; /* From "libcursor.a".		*/
extern int			li; /* Actual # of lines in window.	*/
