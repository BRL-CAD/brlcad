/*                        G I F 2 F B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
 *
 */
/** @file gif2fb.c
 *
 * Gif to Frame Buffer
 *
 * Gif-fb takes a GIF file and writes it to a frame buffer.
 * GIF files contain one or more 1, 2, 4, or 8 bit deep pictures
 * with one or more color maps. Gif-fb will select the correct
 * color map and display the first picture on the frame-buffer.
 *
 * Entry:
 *	FB_FILE		name of the frame buffer to use.
 *	-i		invert color map.
 *	name		name of the file to display.
 *
 * Exit:
 *	the named gif file has been displayed
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "dm.h"


#define LSB 0 /* Least Significant Byte */
#define MSB 1 /* Most Significant Byte */

struct GIF_head {
    char GH_Magic[6];
    unsigned char GH_ScreenWidth[2];	/* MSB, LSB */
    unsigned char GH_ScreenHeight[2];	/* MSB, LSB */
    unsigned char GH_Flags;
    unsigned char GH_Background;
    unsigned char GH_EOB;
};


struct GIF_Image {
    char IH_Magic;
    unsigned char IH_Left[2];		/* MSB, LSB */
    unsigned char IH_Top[2];		/* MSB, LSB */
    unsigned char IH_Width[2];		/* MSB, LSB */
    unsigned char IH_Height[2];		/* MSB, LSB */
    unsigned char IH_Flags;
};


struct acolor {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};


#define WORD(x)	(((int)x[MSB]<<8)+(int)x[LSB])

int ScreenWidth, ScreenHeight;
int GlobalMap;
int CR;
int GlobalPixels;
int Background;

int MinBits;
int Bits;
int Fresh=1;

struct acolor GlobalColors[256];
struct acolor LocalColors[256];

struct GIF_head Header;
struct GIF_Image Im;

const char *framebuffer=NULL;

struct gif2fb_args {
    int help;
    int headers;
    int verbose;
    const char *framebuffer;
};

static const struct bu_arg_option gif2fb_options[] = {
    BU_ARG_FLAG("h", "help", struct gif2fb_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("H", "header-only", struct gif2fb_args, headers,
	"Print the GIF screen dimensions and exit"),
    BU_ARG_FLAG("v", "verbose", struct gif2fb_args, verbose,
	"Report GIF header and image details"),
    BU_ARG_VALUE("F", "framebuffer", struct gif2fb_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand gif2fb_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"GIF input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema gif2fb_schema = BU_ARG_SCHEMA(
    "gif2fb", "Display the first image in a GIF file on a framebuffer",
    gif2fb_options, gif2fb_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar gif2fb_grammar = {
    &gif2fb_schema, NULL, NULL
};

static int
gif2fb_help(FILE *stream, int status)
{
    char *schema_help = bu_arg_schema_help(&gif2fb_schema, "gif2fb");
    bu_flog(stream, "%s", schema_help ? schema_help : "");
    bu_free(schema_help, "gif2fb help");
    return status;
}

int getByte(FILE *inp);

int
main(int argc, char **argv)
{
    struct gif2fb_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i, idx;
    size_t n;
    int maxcolors;
    int verbose=0;
    int headers=0;
    int interlaced;
    int operand_index;

    unsigned char line[3*2048];
    unsigned char *lp;

    int lineNumber, lineInc, lineIdx;
    static int lace[4] = {8, 8, 4, 2};
    static int offs[4] = {0, 4, 2, 1};

    struct fb *fbp;
    FILE *fp;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &gif2fb_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&gif2fb_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return gif2fb_help(stdout, 0);

    headers = args.headers;
    verbose = args.verbose;
    framebuffer = args.framebuffer;

    if (argc - 1 - operand_index == 0) {
	if (isatty(fileno(stdin))) {
	    bu_flog(stderr, "gif2fb requires an input file or redirected input\n");
	    return gif2fb_help(stderr, 1);
	}
	fp = stdin;
	setmode(fileno(stdin), O_BINARY);
    } else {
	const char *input_name = argv[operand_index + 1];
	if ((fp = fopen(input_name, "rb")) == NULL) {
	    bu_flog(stderr, "%s: cannot open \"%s\" for reading\n",
		argv[0], input_name);
	    return 1;
	}
    }
/*
 * read in the Header and then check for consistency.
 */
    n = fread(&Header, 1, 13, fp);

    if (n != 13) {
	fprintf(stderr, "%s: only %ld bytes in header.\n", argv[0], (long)n);
	return 1;
    }

    ScreenWidth = WORD(Header.GH_ScreenWidth);
    ScreenHeight= WORD(Header.GH_ScreenHeight);
    GlobalMap   = (Header.GH_Flags>>7);
    CR	    = (Header.GH_Flags>>4) & 0x07;
    GlobalPixels= (Header.GH_Flags&0x07) + 1;
    if (headers) {
	fprintf(stderr, "-w%d -n%d\n", ScreenWidth, ScreenHeight);
	return 0;
    }
/*
 * In verbose mode, output a message before checking to allow the
 * "smarter" user look over the header even if the header is barfO.
 */
    if (verbose) {
	char magic[sizeof(Header.GH_Magic)+1];
	memcpy(magic, Header.GH_Magic, sizeof(Header.GH_Magic));
	magic[sizeof(magic)-1] = '\0';

	fprintf(stderr, "Magic=%.6s, -w%d -n%d, M=%d, cr=%d, pixel=%d, bg=%d\n",
		magic, ScreenWidth, ScreenHeight, GlobalMap,
		CR, GlobalPixels, Header.GH_Background);
    }

    if (Header.GH_EOB) {
	fprintf(stderr, "%s: missing EOB in header.\n", argv[0]);
	return 1;
    }
    maxcolors = 1 << GlobalPixels;

/*
 * Read in the Global color map.
 */
    for (i=0;i<maxcolors;i++) {
	n = fread(&GlobalColors[i], 1, 3, fp);
	if (n != 3) {
	    fprintf(stdout, "%s: only read %d global colors.\n",
		    argv[0], i);
	    return 1;
	}
    }
/*
 * Read in the image header.
 */
    n = fread(&Im, 1, sizeof(Im), fp);

    if (n != sizeof(Im)) {
	fprintf(stderr, "%s: only %ld bytes in image header.\n",
		argv[0], (long)n);
	return 1;
    }
    if (verbose) {
	fprintf(stderr, "Magic=%c, left=%d, top=%d, Width=%d, Height=%d\n",
		Im.IH_Magic, WORD(Im.IH_Left), WORD(Im.IH_Top), WORD(Im.IH_Width),
		WORD(Im.IH_Height));
	fprintf(stderr, "Map=%d, Interlaced=%d, pixel=%d\n",
		Im.IH_Flags>>7, (Im.IH_Flags>>6)&0x01, Im.IH_Flags&0x03);
    }

    if (WORD(Im.IH_Height) < 0 || WORD(Im.IH_Height) > 65535)
	bu_exit(1, "Bad height info in GIF header\n");

    interlaced = (Im.IH_Flags>>6)&0x01;

/*
 * Read the image color map if any.
 */
    if (Im.IH_Flags>>7) {
	GlobalPixels= (Im.IH_Flags&0x07) + 1;
	for (i=0;i<maxcolors;i++) {
	    n = fread(&GlobalColors[i], 1, 3, fp);
	    if (n != 3) {
		fprintf(stdout,
			"%s: only read %d global colors.\n",
			argv[0], i);
		return 1;
	    }
	}
    }

    if (WORD(Im.IH_Width) > 2048) {
	fprintf(stderr, "%s: Input line greater than internal buffer!\n",
		argv[0]);
	return 1;
    }

    MinBits = getc(fp) + 1;

    if (verbose) {
	fprintf(stderr, "MinBits=%d\n", MinBits);
    }

    if (interlaced) {
	lineIdx = 0;

	lineNumber = offs[lineIdx];
	lineInc= lace[lineIdx];
    } else {
	lineIdx = (sizeof(lace)/sizeof(lace[0]))-1;
	lineNumber = 0;
	lineInc = 1;
    }
/*
 * Open the frame buffer.
 */
    if ((fbp = fb_open(framebuffer, WORD(Im.IH_Width), WORD(Im.IH_Height))) != NULL) {
	int ih_height = WORD(Im.IH_Height);
	int ih_width = WORD(Im.IH_Width);

	if (ih_height < 0 || ih_height > 0xffff || ih_width < 0 || ih_width > 0xffff)
	    bu_exit(1, "Invalid height in GIF Header\n");

	/*
	 * The speed of this loop can be greatly increased by moving all of
	 * the WORD macro calls out of the loop.
	 */
	for (i=0; i<ih_height;i++) {
	    int k;
	    lp = line;
	    for (k=0;k<ih_width;k++) {
		idx = getByte(fp);
		if (idx < 0) {
		    fb_close(fbp);
		    bu_exit(1, "Error: Unexpectedly reached end of input. Output may be incomplete.\n");
		}
		*lp++ = GlobalColors[idx].red;
		*lp++ = GlobalColors[idx].green;
		*lp++ = GlobalColors[idx].blue;
	    }
	    fb_write(fbp, 0, ih_height-lineNumber, line, ih_width);
	    fb_flush(fbp);
	    lineNumber += lineInc;
	    if (lineNumber >= ih_height) {
		++lineIdx;
		V_MIN(lineIdx, (int)(sizeof(lace)/sizeof(lace[0]))-1);
		lineInc = lace[lineIdx];
		lineNumber = offs[lineIdx];
	    }
	}
	fb_close(fbp);
    }
    return 0;
}
/* getcode - Get a LWZ "code"
 *
 * getcode returns an LWZ code.  The code size is always less than
 * 12 bits but could be as small as 2 bits.  This implies that reading
 * one code may not "read" anything from a file.
 *
 * Entry:
 *	inp	a FILE pointer to the input stream.
 *
 * Exit:
 *	returns one code.
 *
 * Uses:
 *	Bits		global variable containing the size of a code.
 *	bitsleft	number of bits left in this byte.
 *	count		number of BYTES left in this Block.
 *	lastbits	the left over bits form the last getcode call.
 *
 * Calls:
 * 	none.
 *
 * Method:
 *	while not enough bits for a code do
 *		read another byte (8 bits)
 *	endwhile
 *	extract the code.
 *	clean up the state variables.
 *
 * N.B.!!!	This code depends on the '>>' operator zero filling
 *		from the left.
 */
int
getcode(FILE *inp)
{
    static unsigned int lastbits = 0;
    static int bitsleft = 0;
    static int count=0;
    int code;


    while (bitsleft < Bits) {
/*
 * get a new block counter if needed.
 */
	if (--count <= 0) {
	    count = (unsigned char) getc(inp);
	    if (count == 0) return -1;
	}
/*
 * stuff another byte into last bits.
 */
	lastbits |= (unsigned char) getc(inp) << bitsleft;
	bitsleft += 8;
    }
    code = (1<<Bits)-1;	/* make mask */
    code &= lastbits;	/* extract the code */
/*
 * clean up the state variables.
 */
    bitsleft -= Bits;
    lastbits = lastbits >> Bits;
    return code;
}


/* getByte	get a byte from the input stream decompressing as we go.
 *
 * getByte uses the somewhat standard LWZ decompress algorithm.  Most
 * of this subroutine is based on "compress.c".  I've added some and
 * deleted others but I do NOT claim that this is original code.
 *
 * Entry:
 *	inp	the input stream point
 *
 * Exit:
 *	returns a "byte".
 *
 * Calls:
 *	getcode		to get the next code from the input stream.
 *
 * Uses:
 *	Bits	current size of the code.
 *	minBits	the min. size of the code.
 *
 * Method:
 *	Being unable to read the papers that everybody else points to,
 *	I had to decipher the compress code.  This is how I thing this
 *	compression algorithm works.
 *	if this is the first time the routine has been called then
 *		initialize the code sizes
 *		set the "tree" so that all "characters" are at the root.
 *	endif
 *	get a new code
 *	if the new code is greater than the current max code then
 *		add last(?) code to the tree.
 *		follow the tree from the leaf to the root outputting
 *		    a "byte" for each node of the tree.
 *	endif
 */
int getByte(FILE *inp)
{
    int code, incode;
    static int firstcode, oldcode;
    static int firstTime = 1;
    static int clear_code, end_code;
    static int max_code, next_ent;
#define PREFIX 0
#define SUFFIX 1
    static int table[2][1<<12];
    static int stack[1<<13], *sp;

    int i;

    if (firstTime) {
	firstTime = 0;
	Bits = MinBits;
	clear_code = 1 << (Bits-1);
	end_code = clear_code+1;
	max_code=clear_code<<1;
	next_ent = clear_code+2;

	for (i=0;i<clear_code;i++) {
	    table[PREFIX][i] = 0;
	    table[SUFFIX][i]  = i;
	}

	sp = stack;

	do {
	    firstcode=oldcode=getcode(inp);
	} while (firstcode == clear_code);
	return firstcode;
    }

    if (sp > stack) return *--sp;

    while ((code=getcode(inp)) >= 0) {
	if (code == clear_code) {
	    for (i=0;i<clear_code;i++) {
		table[PREFIX][i] = 0;
		table[SUFFIX][i]  = i;
	    }
	    Bits = MinBits;
	    max_code = clear_code<<1;
	    next_ent = clear_code+2;
	    sp=stack;
	    firstcode=oldcode=getcode(inp);
	    return firstcode;
	} else if (code == end_code) {
	    return -1;
	}

	incode = code;

	if (code >= next_ent) {
	    *sp++ =firstcode;
	    code = oldcode;
	}

	while (code >= clear_code) {
	    *sp++ = table[SUFFIX][code];
	    code = table[PREFIX][code];
	}

	*sp++ = firstcode = table[SUFFIX][code];

	if ((code=next_ent) < (1<<12)) {
	    table[PREFIX][code] = oldcode;
	    table[SUFFIX][code]  = firstcode;
	    next_ent++;
	    if ((next_ent >= max_code) &&
		(max_code < (1<<12))) {
		max_code *= 2;
		Bits++;
	    }
	}

	oldcode = incode;

	if (sp >stack) return *--sp;
    }
    return code;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
