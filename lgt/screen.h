/*
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
