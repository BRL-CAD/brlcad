/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/

/* Set pre-processor switch for getting signal() handler declarations right.
 */

#if defined(sun) && ! defined(SunOS4)
/* For Suns running older releases, compile with -DSunOS4=0 to suppress
	bogus warning messages. */
#define SunOS4	1
#endif
#if __STDC__ || (defined(SYSV) && ! defined(cray)) || SunOS4
#define STD_SIGNAL_DECLS 1
#else
#define STD_SIGNAL_DECLS 0
#endif

/*
 * Note: all files that include this one should have the following set of
 *       includes *before* #include "./extern.h".
 *
 * #include "fb.h"
 *
 * #include "./std.h"
 * #include "./ascii.h"
 * #include "./font.h"
 * #include "./try.h"
 *
 */

/* For production use, set to "static" */
#ifndef STATIC
#define STATIC static
#endif

typedef struct
	{
	int p_x;
	int p_y;
	}
Point;

typedef struct
	{
	Point r_origin;
	Point r_corner;
	}
Rectangle;

typedef struct
	{
	RGBpixel *n_buf;
	int n_wid;
	int n_hgt;
	}
Panel;

extern FBIO *fbp;
extern RGBpixel *menu_addr;
extern RGBpixel paint;
extern Point cursor_pos;
extern Point image_center;
extern Point windo_center;
extern Point windo_anchor;
extern Try *try_rootp;
extern bool isSGI;
extern char cread_buf[BUFSIZ*10], *cptr;
extern char macro_buf[];
extern char *macro_ptr;
extern int brush_sz;
extern int gain;
extern int pad_flag;
extern int remembering;
extern int report_status;
extern int reposition_cursor;
extern int tty;
extern int tty_fd;
extern int zoom_factor;
extern int LI, CO;

extern Func_Tab	*get_Func_Name(char *inbuf, int bufsz, char *msg);
extern RGBpixel *get_Fb_Panel();
extern char *char_To_String(int i);
extern int add_Try(Func_Tab *ftbl, register char *name, register Try **trypp);
extern int bitx(register char *bitstring, register int posn);
extern int fb_Init_Menu();
extern int getpos(Point *pos);
extern int get_Input(char *inbuf, int bufsz, char *msg);
extern void fb_Get_Pixel(unsigned char *pixel);
extern void pos_close();
extern void init_Status(void);
extern void init_Tty(void), restore_Tty(void);
extern void prnt_Status(void);
extern void prnt_Usage(void);
#if __STDC__
extern void prnt_Scroll( char * fmt, ... );
extern void prnt_Event( char *fmt, ... );
#else
extern void prnt_Event();
extern void prnt_Scroll();
#endif
extern void prnt_Rectangle(char *str, register Rectangle *rectp);
extern void do_Key_Cmd(register int key, register int n);
extern int InitTermCap();
extern int get_Font(char *fontname);
extern void prnt_Prompt(char *msg);
extern int empty(int fd);
extern int get_Char(void);
extern int get_Mouse_Pos(Point *pointp);
extern int SetStandout();
extern int ClrStandout();
extern int exec_Shell(char **args);
extern void do_line(int xpos, int ypos, register char *line, RGBpixel (*menu_border));
extern int pad_open(int n);
extern void save_Tty();
extern void set_Raw();
extern void clr_Tabs();
extern void clr_Echo();
extern void clr_CRNL();
extern int MvCursor();
extern void pad_close(void);
extern void reset_Tty();
extern int ClrText();
extern int HmCursor();
extern int DeleteLn();
extern int ClrEOL();
extern int SetScrlReg();
extern int ResetScrlReg();
extern void set_HUPCL();

#define MAX_LN			81
#define Toggle(f)		(f) = ! (f)
#define De_Bounce_Pen()		while( do_Bitpad( &cursor_pos ) ) ;
#define BOTTOM_STATUS_AREA	2
#define TOP_SCROLL_WIN		(BOTTOM_STATUS_AREA-1)
#define PROMPT_LINE		(LI-2)
#define MACROBUFSZ		(BUFSIZ*10)
#define Malloc_Bomb() \
		fb_log(	"\"%s\"(%d) Malloc() no more space.\n", \
				__FILE__, __LINE__ \
				); \
		return 0;
