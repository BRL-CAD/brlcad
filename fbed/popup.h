/*
	SCCS id:	@(#) popup.h	1.10
	Last edit: 	4/24/86 at 13:12:05
	Retrieved: 	8/6/86 at 13:31:51
	SCCS archive:	/vld/moss/src/fbed/s.popup.h

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-283-6647
*/
#define INCL_POPUP
#define MENU_FONT	"/usr/lib/vfont/fix.6"

typedef struct
	{
	int	p_x;
	int	p_y;
	}
Point;

typedef struct
	{
	Point	r_origin;
	Point	r_corner;
	}
Rectangle;

typedef struct
	{
	RGBpixel	color;
	void	(*func)();
	char	*label;
	} Seg;

typedef struct
	{
	int		wid, hgt;
	int		n_segs, seg_hgt;
	int		max_chars, char_base;
	int		on_flag, cmap_base;
	int		last_pick;
	Rectangle	rect;
	RGBpixel	*outlines, *touching, *selected;
	RGBpixel	*under, *image;
	char		*title, *font;
	Seg		*segs;
	ColorMap	cmap;
	}
Menu;

typedef struct
	{
	RGBpixel  *n_buf;
	int	n_wid;
	int	n_hgt;
	}
Panel;

#define RESERVED_CMAP  ((pallet.cmap_base+pallet.n_segs+1)*2)
