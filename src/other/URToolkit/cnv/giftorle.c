/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* +------------------------------------------------------------------+ */
/* | Copyright 1989, David Koblas.                                    | */
/* |   You may copy this file in whole or in part as long as you      | */
/* |   don't try to make money off it, or pretend that you wrote it.  | */
/* +------------------------------------------------------------------+ */

#include	<stdio.h>
#include	"rle.h"

#ifndef lint
static char rcsid[] = "$Id$";
#endif
/*
giftorle()		Tag the file.
*/

#define	MAXCOLORMAPSIZE		256

#define	TRUE	1
#define	FALSE	0

#define CM_RED		0
#define CM_GREEN	1
#define CM_BLUE		2

#define	MAX_LWZ_BITS		12

#define	ReadOK(file,buffer,len)	(fread(buffer,len,1,file)!=0)
#define	EasyFail(str,status)	{fprintf(stderr,str);return(status);}
#define	HardFail(str,status)	{fprintf(stderr,str);exit  (status);}

#define LM_to_uint(a,b)			(((b)<<8)|(a))

int ReadGIF();
int ReadColorMap(), IgnoreExtention(), GetCode(), LWZReadByte();
int ReadInterlaced(), ReadRaster();

static rle_map out_map[3*(1<<8)];

CONST_DECL char *MY_NAME = "giftorle";

FILE *outfile;

struct {
	unsigned int		Width;
	unsigned int		Height;
	unsigned char		ColorMap[3][MAXCOLORMAPSIZE];
	unsigned int		BitPixel;
	unsigned int		ColorResolution;
	unsigned int		Background;
} Screen;

static int output_colormap = FALSE;

void
main(argc,argv)
int	argc;
char	**argv;
{
    int		oflag = 0, 
    nfname = 0;
    char	       *outfname = NULL, 
    **infname = NULL;

    MY_NAME = cmd_name( argv );

    if ( scanargs( argc, argv, "% c%- o%-outfile.rle!s infile.gif%*s",
		   &output_colormap, &oflag, &outfname,
		   &nfname, &infname ) == 0 )
	exit( 1 );

    outfile = rle_open_f( MY_NAME, outfname, "w" );
    /* Questionable practice.  Modifies default values for headers. */
    rle_addhist( argv, (rle_hdr *)0, &rle_dflt_hdr );

    /* Always read at least one (standard input). */
    if ( nfname == 0 )
	(void)ReadGIF( NULL );

    while ( nfname-- > 0 )
	(void)ReadGIF( *infname++ );
    exit( 0 );
}

int
ReadGIF(filename)
char	*filename;
{
    unsigned char	buf[16];
    unsigned char	c;
    unsigned char	LocalColorMap[3][MAXCOLORMAPSIZE];
    FILE			*fd;
    int				use_global_colormap;
    int				bit_pixel;
    int				count=0;

    fd = rle_open_f( MY_NAME, filename, "r" );

    if (! ReadOK(fd,buf,6))
	EasyFail("error reading magic number\n",TRUE);
    if (strncmp((char *)buf,"GIF87a",6)!=0)
	EasyFail("bad magic number (version mismatch?)\n",TRUE);
    if (! ReadOK(fd,buf,7))
	EasyFail("error reading screen descriptor\n",TRUE);
    Screen.Width           = LM_to_uint(buf[0],buf[1]);
    Screen.Height          = LM_to_uint(buf[2],buf[3]);
    Screen.BitPixel        = 2<<(buf[4]&0x07);
    Screen.ColorResolution = (((buf[4]&0x70)>>3)+1);
    Screen.Background      = buf[5];
    if ((buf[4]&0x80)==0x80) {
	if (ReadColorMap(fd,Screen.BitPixel,Screen.ColorMap))
	    return (TRUE);
    }

    while (1) {
	if (! ReadOK(fd,&c,1))
	    EasyFail("No image data -- EOF\n",TRUE);
	if (c == ';') 
	    return FALSE;
	if (c == '!') {
	    if (! ReadOK(fd,&c,1))
		EasyFail("No extention function code -- EOF\n",TRUE);
	    if (IgnoreExtention(fd))
		return(TRUE);
	    continue;
	}
	if (c != ',') {
	    fprintf(stderr,"Bogus character ignoring '%c'\n",c);
	    continue;
	}
/*
	if (count == 1)
	    HardFail("This file contains more than one image! FAILING\n",1);
*/
	count++;

	if (! ReadOK(fd,buf,9))
	    EasyFail("Couldn't read left/top/width/height\n",TRUE);
	if ((buf[8]&0x80)==0x80)
	    use_global_colormap = FALSE ;
	else
	    use_global_colormap = TRUE ;

	bit_pixel = 1<<((buf[8]&0x07)+1);

	if (! use_global_colormap) {
	    if (ReadColorMap(fd,bit_pixel,LocalColorMap))
		return TRUE;
	}

	if (ReadRaster((buf[8]&0x40)==0x40, fd,LM_to_uint(buf[4],buf[5]),
		       LM_to_uint(buf[6],buf[7]),
		       use_global_colormap?Screen.ColorMap:LocalColorMap))
		return TRUE;
    }
}

int
ReadColorMap(fd,number,buffer)
FILE			*fd;
int				number;
unsigned char	buffer[3][MAXCOLORMAPSIZE];
{
    int				i;
    unsigned char	rgb[3];

    for (i=0;i<number;i++) {
	if (! ReadOK(fd,rgb,sizeof(rgb)))
	    EasyFail("Bogus colormap\n",TRUE);
	buffer[CM_RED][i] = rgb[0] ;
	buffer[CM_GREEN][i] = rgb[1] ;
	buffer[CM_BLUE][i] = rgb[2] ;
    }
    return FALSE;
}

int
IgnoreExtention(fd)
FILE	*fd;
{
    static char		buf[256];
    unsigned char	c;

    while (1) {
	if (! ReadOK(fd,&c,1))
	    EasyFail("EOF in extention\n",TRUE);
	if (c == 0)
	    return FALSE;
	if (read(fd,buf,(int) c)!=(int) c) 
	    EasyFail("EOF in extention\n",TRUE);
    }
}

int
GetCode(fd, code_size, flag)
FILE	*fd;
int		code_size;
int		flag;
{
    static unsigned char	buf[280];
    static int				curbit,lastbit,done,last_byte;
    int						i, j, ret;
    unsigned char			count;

    if (flag) {
	curbit = 0;
	lastbit = 0;
	done = FALSE;
	return 0;
    }

    if ( (curbit+code_size) >= lastbit) {
	if (done) {
	    if (curbit>=lastbit)
		EasyFail("Ran off the end of my bits\n",-1);
	}
	buf[0] = buf[last_byte-2];
	buf[1] = buf[last_byte-1];
	if (! ReadOK(fd,&count,1)) {
	    EasyFail("Error in getting buffer size\n",-1);
	}
	if (count == 0) {
	    done = TRUE;
	} else if (! ReadOK(fd,&buf[2],count))
	    EasyFail("Error in getting buffer\n",-1);
	last_byte = 2 + count;
	curbit = (curbit - lastbit) + 16;
	lastbit = (2+count)*8 ;
    }

    ret = 0;
    for( i = curbit, j = 0; j < code_size; i++, j++ ) 
	ret |= ((buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;

    curbit += code_size;

    return ret;
}

int
LWZReadByte(fd,flag,input_code_size)
FILE	*fd;
int		flag;
int		input_code_size;
{
    static int		fresh=FALSE;
    int				code,incode;
    static int		code_size,set_code_size;
    static int		max_code,max_code_size;
    static int		firstcode,oldcode;
    static int		clear_code,end_code;
    static int		table[2][(1<< MAX_LWZ_BITS)];
    static int		stack[(1<<(MAX_LWZ_BITS))*2],*sp;
    register int	i;

    if (flag) {
	set_code_size = input_code_size;
	code_size = set_code_size+1;
	clear_code = 1 << set_code_size ;
	end_code = clear_code + 1;
	max_code_size = 2*clear_code;
	max_code = clear_code+2;

	GetCode(fd, 0,TRUE);
		
	fresh=TRUE;

	for (i=0;i<clear_code;i++) {
	    table[0][i] = 0;
	    table[1][i] = i;
	}
	for (;i<(1<<MAX_LWZ_BITS);i++)
	    table[0][i] = table[1][0] = 0;

	sp=stack;

	return 0;
    } else if (fresh) {
	fresh = FALSE;
	do {
	    firstcode=oldcode=
		GetCode(fd, code_size, FALSE);
	} while (firstcode == clear_code);
	return firstcode;
    }

    if (sp > stack) 
	return *--sp;

    while ((code=GetCode(fd,code_size,FALSE))>=0) {
	if (code == clear_code) {
	    for (i=0;i<clear_code;i++) {
		table[0][i] = 0;
		table[1][i] = i;
	    }
	    for (;i<(1<<MAX_LWZ_BITS);i++)
		table[0][i] = table[1][i] = 0;
	    code_size = set_code_size+1;
	    max_code_size = 2*clear_code;
	    max_code = clear_code+2;
	    sp=stack; 
	    firstcode=oldcode=
		GetCode(fd,code_size,FALSE);
	    return firstcode;
	} else if (code == end_code) {
	    unsigned char		count;
	    unsigned char		junk;

	    while (ReadOK(fd,&count,1) && (count!=0))
		while (count-->0 && ReadOK(fd,&junk,1));
	    if (count!=0)
		EasyFail("missing EOD in data stream (common occurance)\n",-3);
	    return -2;
	}

	incode = code;

	if (code >= max_code) {
	    *sp++ = firstcode;
	    code = oldcode;
	}

	while (code >= clear_code) {
	    *sp++ = table[1][code];
	    if (code == table[0][code])
		EasyFail("Circular table entry BIG ERROR\n",-1);
	    code = table[0][code];
	}

	*sp++ = firstcode = table[1][code];

	if ((code=max_code)<(1<<MAX_LWZ_BITS)) {
	    table[0][code] = oldcode;
	    table[1][code] = firstcode;
	    max_code++;
	    if ((max_code >= max_code_size) && 
		(max_code_size < (1<<MAX_LWZ_BITS))) {
		max_code_size *= 2;
		code_size++;
	    }
	}

	oldcode = incode;

	if (sp > stack) 
	    return *--sp;
    }
    return code;
}

int
ReadRaster(interlace,fd,len,height,cmap)
int interlace;
FILE	*fd;
int		len,height;
char	cmap[3][MAXCOLORMAPSIZE];
{
    unsigned char	c;	
    int			v;
    int			xpos=0;
    rle_pixel	      **scanline[3];
    rle_pixel	       *ptr[3] ;
    rle_hdr		hdr;
    int			i,j;
    int			ypos=0, pass=interlace ? 0 : 4;


    for (j=0;j<(output_colormap?1:3);j++) {
	if ((scanline[j]=(rle_pixel **)
	     malloc(height*sizeof(rle_pixel *)))==NULL)
	    EasyFail("Unable to malloc space for pixels #1\n",1);
	for (i=0;i<height;i++) {
	    if ((scanline[j][i]=(rle_pixel *)
		 malloc(len*sizeof(rle_pixel)))==NULL) {
		for (;j>=0;j--)
		    for (;i>=0;i--)
			if (scanline[j][i]!=NULL) free(scanline[i]);
		EasyFail("Unable to malloc space for pixels #2\n",1);
	    }
	}
    }

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, "giftorle", NULL, 0 );

    if (output_colormap) {
	hdr.ncolors =  1;
	hdr.ncmap = 3;
	hdr.cmaplen = 8;
	hdr.cmap = out_map;
	for (i=0;i<(1<<8);i++) {
	    out_map[i+(0<<8)] = cmap[CM_RED][i] << 8;
	    out_map[i+(1<<8)] = cmap[CM_GREEN][i] << 8;
	    out_map[i+(2<<8)] = cmap[CM_BLUE][i] << 8;
	}
    } else {
	RLE_SET_BIT(hdr, RLE_RED);
	RLE_SET_BIT(hdr, RLE_GREEN);
	RLE_SET_BIT(hdr, RLE_BLUE);
	hdr.ncolors =  3;
    }
    hdr.rle_file = outfile ;
    hdr.xmax = len - 1;
    hdr.ymax = height - 1;

    rle_put_setup(&hdr);

    if (! ReadOK(fd,&c,1))
	EasyFail("Bogus image data -- EOF\n",TRUE);
    if (LWZReadByte(fd,TRUE,c)<0)
	return TRUE;

    while ((v=LWZReadByte(fd,FALSE,c))>=0) {
	if (output_colormap) {
	    scanline[RLE_RED][ypos][xpos]   = v ;
	} else {
	    scanline[RLE_RED][ypos][xpos]   = cmap[CM_RED][v] ;
	    scanline[RLE_GREEN][ypos][xpos] = cmap[CM_GREEN][v] ;
	    scanline[RLE_BLUE][ypos][xpos]  = cmap[CM_BLUE][v] ;
	}
	xpos++;
	if (xpos==len) {
	    xpos = 0;
	    switch (pass) {
	    case 0: 
	    case 1:
		ypos += 8; break;
	    case 2:
		ypos += 4; break;
	    case 3:
		ypos += 2; break;
	    case 4:		/* Not interlaced */
		ypos++; break;
	    default:
		fprintf( stderr, "%s: Data past end of image.\n", hdr.cmd );
		break;
	    }
	    if (ypos >= height) {
		pass++;
		switch (pass) {
		case 1:
		    ypos = 4; break;
		case 2:
		    ypos = 2; break;
		case 3:
		    ypos = 1; break;
		default:		/* Shouldn't happen. */
		    ypos = 0; break;
		}
	    }
	}
    }

    for ( i = height - 1; i >= 0; i-- ) {
	ptr[0] = scanline[RLE_RED][i] ;
	if (! output_colormap) {
	    ptr[1] = scanline[RLE_GREEN][i] ;
	    ptr[2] = scanline[RLE_BLUE][i] ;
	}
	rle_putrow(ptr,len,&hdr);
    }

    rle_puteof(&hdr);

    for (i=0;i<height;i++) {
	if (scanline[0][i]!=NULL) free(scanline[0][i]);
	if (output_colormap) continue;

	if (scanline[1][i]!=NULL) free(scanline[1][i]);
	if (scanline[2][i]!=NULL) free(scanline[2][i]);
    }
    free(scanline[0]);
    if (! output_colormap) {
	free(scanline[1]); 
	free(scanline[2]);
    }

    if (v == -2)
	return FALSE;
    return TRUE;
}

