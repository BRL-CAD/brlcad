/*
	SCCS id:	@(#) char.c	1.13
	Modified: 	5/6/86 at 15:33:30 Gary S. Moss
	Retrieved: 	8/6/86 at 13:35:23
	SCCS archive:	/vld/moss/src/fbed/s.char.c
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) char.c 1.13, modified 5/6/86 at 15:33:30, archive /vld/moss/src/fbed/s.char.c";
#endif
/* 
 * char.c - display a character on the ikonas frambuffer
 * 
 * Author:	Paul R. Stay
 * 		Ballistics Research Labratory
 * 		APG, Md.
 * Date:	Tue Jan  8 1985
 */
#include <stdio.h>
#include <fb.h>
#include "./font.h"
#include "./popup.h"
#include "./extern.h"
extern void	fudge_Pixel();
extern void	fill_buf(), clear_buf();
extern void	squash();

void		dochar(), menu_char();

void
do_line( xpos, ypos, line, menu_border )
int		xpos, ypos;
register char	*line;
Pixel		*menu_border; /* Menu outline color, if NULL, do filtering. */
	{	register int    currx;
		register int    char_count, char_id;
		register int	len = strlen( line );
		int		curry;
	if( ffdes == NULL )
		return;
	currx = xpos;
	curry = ypos;

	for( char_count = 0; char_count < len; char_count++ )
		{
		char_id = (int) line[char_count] & 0377;

		/* locate the bitmap for the character in the file */
		(void) fseek( ffdes, (long)(dir[char_id].addr+offset), 0 );

		/* Read in the dimensions for the character */
		width = dir[char_id].left + dir[char_id].right;
		height = dir[char_id].up + dir[char_id].down;

		if( currx + width > _fbsize - 1 )
			break;		/* won't fit on screen */

		if( menu_border == (Pixel *) NULL )
			dochar( char_id, currx, curry, dir[char_id].down%2 );
		else
			menu_char(	currx,
					curry,
					dir[char_id].down % 2,
					menu_border
					);
		currx += dir[char_id].width;
    		}
	return;
	}

/* Shared by dochar() and menu_char().					*/
static int		filterbuf[BUFFSIZ][BUFFSIZ];

void
dochar( c, xpos, ypos, odd )
int c;
int xpos, ypos, odd;
	{
	register int    i, j, k;
	int		base;
	int     	totwid = width;
	int     	up, down;
	static float	resbuf[BUFFSIZ];
	static Pixel	fbline[BUFFSIZ * BUFFSIZ / 4],
			fbbuff[BUFFSIZ * BUFFSIZ / 4];

	/* Read in the character bit map, with two blank lines on each end. */
	for (i = 0; i < 2; i++)
		clear_buf (totwid, filterbuf[i]);
	for (i = height + 1; i >= 2; i--)
		fill_buf (width, filterbuf[i]);
	for (i = height + 2; i < height + 4; i++)
		clear_buf (totwid, filterbuf[i]);

	up = dir[c].up;
	down = dir[c].down;

	/* Initial base line for filtering depends on odd flag. */
	base = (odd ? 1 : 2);

	/* Read in the area of the Frame Buffer for filtering */
	fbbget(	xpos,
		xpos + (totwid + 3) - 2,
		ypos - up,
		ypos + down + 3,
		fbbuff
		);

	/* Produce a Pixel buffer from a description of the character and
	 	the read back data from the frame buffer for anti-aliasing.
	 */
	for (i = height + base, k = 0; i >= base; i--, k++)
		{
		squash(	filterbuf[i - 1],	/* filter info */
			filterbuf[i],
			filterbuf[i + 1],
			resbuf,
			totwid + 4
			);
		for (j = 0; j < (totwid + 3) - 1; j++)
			{	register int	h = (k*((totwid+3)-1))+j;
			fbline[h].red =
				(int)(paint.red*resbuf[j]+(1-resbuf[j]) *
		    		(fbbuff[h].red & 0377))
			 	& 0377;
			fbline[h].green =
				(int)(paint.green*resbuf[j]+(1-resbuf[j]) *
				(fbbuff[h].green & 0377))
				& 0377;
			fbline[h].blue =
				(int)(paint.blue*resbuf[j]+(1-resbuf[j]) *
				(fbbuff[h].blue & 0377))
				& 0377;
			}
		}
	/* Send the character data back to be read into the frame buffer */
	fbbput( xpos,
		xpos + (totwid + 3) - 2,
		ypos - up,
		ypos - up + k - 1,
		fbline
		);
	return;
	}

void
menu_char( x_adjust, menu_wid, odd, menu_border )
int x_adjust, menu_wid, odd;
register
Pixel	*menu_border;
	{	register int    i, j, k;
		int		embold = 1;
		int		base;
		int		totwid = width;
	/* Read in the character bit map, with two blank lines on each end. */
	for (i = 0; i < 2; i++)
		clear_buf (totwid, filterbuf[i]);
	for (i = height + 1; i >= 2; i--)
		fill_buf (width, filterbuf[i]);
	for (i = height + 2; i < height + 4; i++)
		clear_buf (totwid, filterbuf[i]);

	for (k=0; k<embold; k++)
		for (i=2; i<height+2; i++)
			for (j=totwid+1; j>=2; j--)
		  		filterbuf[i][j+1] |= filterbuf[i][j];
 
	/* Initial base line for filtering depends on odd flag. */
	base = (odd ? 1 : 2);

	/* Change bits in menu that correspond to character bitmap.	*/
	for (i = height + base, k = 0; i >= base; i--, k++)
		{	register Pixel	*menu;
		menu = menu_addr + k * menu_wid + x_adjust;
		for (j = 0; j < (totwid + 3) - 1; j++, menu++ )
			if( filterbuf[i][j] )
				*menu = *menu_border;
		}
	return;
	}
