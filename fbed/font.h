/* 
 * font.h - Header file for putting fonts up
 * 
 * Author:	Paul R. Stay
 * 		Ballistics Research Labratory
 * 		APG, Md.
 * Date:	Fri Jan 11 1985
 */
#define INCL_FONT
/*	vfont.h	4.1	83/05/03 from 4.2 Berkley	*/

/*
 * The structures header and dispatch define the format of a font file.
 */

struct header {
	short magic;
	unsigned short size;
	short maxx;
	short maxy;
	short xtend;
}; 

struct dispatch {
	unsigned short addr;
	short nbytes;
	char up,down,left,right;
	short width;
};

#define BUFFSIZ 200
#define MAXLEN 100
#define FONT_DEFAULT	"nonie.r.12"

/* Variables controlling the font itself */
extern FILE *ffdes;			/* Fontfile file descriptor. */
extern int offset;			/* Offset to data in the file. */
extern struct header hdr;		/* Header for font file. */
extern struct dispatch dir[256];	/* Directory for character font. */
extern int width, height;	  /* Width and height of current character. */
					/* embolden characters */
extern char fontname[128];		/* font for normal size chars */
