/*
	SCCS id:	@(#) popup.h	2.1
	Modified: 	12/9/86 at 15:55:41
	Retrieved: 	12/26/86 at 21:53:44
	SCCS archive:	/vld/moss/src/fbed/s.popup.h

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#define INCL_POPUP
#define MENU_FONT	"/usr/lib/vfont/fix.6"

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
	RGBpixel color;
	void (*func)();
	char *label;
	} Seg;

typedef struct
	{
	int wid, hgt;
	int n_segs, seg_hgt;
	int max_chars, char_base;
	int on_flag, cmap_base;
	int last_pick;
	Rectangle rect;
	RGBpixel *outlines, *touching, *selected;
	RGBpixel *under, *image;
	char *title, *font;
	Seg		*segs;
	ColorMap	cmap;
	}
Menu;

typedef struct
	{
	RGBpixel  *n_buf;
	int n_wid;
	int n_hgt;
	}
Panel;

#define RESERVED_CMAP  ((pallet.cmap_base+pallet.n_segs+1)*2)
