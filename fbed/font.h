/*
	Authors:	Paul R. Stay
			Gary S. Moss

			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651

	$Header$
*/
/*	font.h - Header file for putting fonts up. */
#define INCL_FONT
#if defined(sel) || defined(gould) || defined(sgi) || defined(alliant) || defined(sun)
#define BIGENDIAN
#endif
#if defined(BIGENDIAN)
#define SWAB(shrt)	(shrt=(((shrt)>>8) & 0xff) | (((shrt)<<8) & 0xff00))
#define SWABV(shrt)	((((shrt)>>8) & 0xff) | (((shrt)<<8) & 0xff00))
#else
#define	SWAB(shrt)
#define SWABV(shrt)	(shrt)
#endif
#if defined(mips)
#define CHARS_UNSIGNED_ONLY
#endif

#define FONTBUFSZ 200
#ifndef FONTDIR
#define FONTDIR		"/usr/lib/vfont" /* default font directory */
#endif
#define FONTNAME	"nonie.r.12"	 /* default font name */
#define FONTNAMESZ	128

#define SIGNBIT		(1<<7)
#define SIGNMASK	~SIGNBIT
#define TWOSCOMP(chr)	((~(chr)&0xff)+1)
#ifdef CHARS_UNSIGNED_ONLY
#define SignedChar(chr)	(((chr)&SIGNBIT) ? -TWOSCOMP(chr) : (chr))
#else
#define SignedChar(chr)	chr
#endif

/*	vfont.h	4.1	83/05/03 from 4.2 Berkley */
/* The structures header and dispatch define the format of a font file. */
struct header {
	short		magic;
	unsigned short	size;
	short		maxx;
	short		maxy;
	short		xtend;
}; 
struct dispatch
	{
	unsigned short	addr;
	short		nbytes;
	char up, down, left, right;
	short		width;
	};

/* Variables controlling the font itself. */
extern FILE *ffdes;		/* Fontfile file descriptor. */
extern int offset;		/* Offset to data in the file. */
extern struct header hdr;	/* Header for font file. */
extern struct dispatch dir[256];/* Directory for character font. */
extern int width, height;	/* Width and height of current char. */
