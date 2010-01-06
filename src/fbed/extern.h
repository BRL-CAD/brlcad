/*                        E X T E R N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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
/** @file extern.h
 *
 */

#include "common.h"

#ifndef FBED_EXTERN_H
#define FBED_EXTERN_H

#define MAX_LN			81
#define Toggle(f)		(f) = ! (f)
#define De_Bounce_Pen()		while ( do_Bitpad( &cursor_pos ) ) ;
#define BOTTOM_STATUS_AREA	2
#define TOP_SCROLL_WIN		(BOTTOM_STATUS_AREA-1)
#define PROMPT_LINE		(LI-2)
#define MACROBUFSZ		(BUFSIZ*10)
#define Malloc_Bomb() \
		fb_log(	"\"%s\"(%d) Malloc() no more space.\n", \
				__FILE__, __LINE__ \
				); \
		return 0;

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
extern char cread_buf[MACROBUFSZ], *cptr;
extern char macro_buf[MACROBUFSZ], *macro_ptr;
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

extern struct vfont font;

extern Func_Tab	*get_Func_Name(char* inbuf, size_t bufsz, const char* msg);
extern RGBpixel *get_Fb_Panel();
extern char *char_To_String(int i);
extern int add_Try(Func_Tab* ftbl, const char* name, Try** trypp);
extern int bitx(char *bitstring, int posn);
extern int fb_Init_Menu();
extern int getpos(Point *pos);
extern int get_Input(char* inbuf, size_t bufsz, const char* msg);
extern void fb_Get_Pixel(unsigned char *pixel);
extern void pos_close();
extern void init_Status(void);
extern void init_Tty(void), restore_Tty(void);
extern void prnt_Status(void);
extern void prnt_Usage(void);
extern void prnt_Scroll( const char * fmt, ... );
extern void prnt_Debug( const char *fmt, ... );
extern void prnt_Event( const char *fmt, ... );
extern void prnt_Rectangle(const char *str, Rectangle *rectp);
extern void do_Key_Cmd(int key, int n);
extern int InitTermCap();
extern void prnt_Prompt(const char *msg);
extern int empty(int fd);
extern int get_Char(void);
extern int get_Mouse_Pos(Point *pointp);
extern int SetStandout();
extern int ClrStandout();
extern int exec_Shell(char **args);
extern void do_line(int xpos, int ypos, const char *line, RGBpixel (*menu_border));
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

#endif /* FBED_EXTERN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
