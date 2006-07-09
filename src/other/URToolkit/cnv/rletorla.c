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
/*
 * rletorla - A program which will convert Utah's "rle" images to
 *            Wavefronts "rlb" image format.
 *
 * Author:      Wesley C. Barris
 *              AHPCRC
 *              Minnesota Supercomputer Center, Inc.
 * Date:        Thu June 21 1990
 * Copyright (c) Wesley C. Barris
 */
/*-----------------------------------------------------------------------------
 * System includes.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "rlb_header.h"
#include "rle.h"

#ifdef USE_STDLIB_H
#  include <stdlib.h>
#else
#  ifdef USE_STRING_H
#    include <string.h>
#  else
#    include <strings.h>
#    define strrchr rindex
#    define strchr index
#  endif
#  ifdef VOID_STAR
extern void *malloc();
#  else
extern char *malloc();
#  endif
extern void free();
#endif /* USE_STDLIB_H */

#define VPRINTF if (verbose || header) fprintf
#define GRAYSCALE   001	/* 8 bits, no colormap */
#define PSEUDOCOLOR 010	/* 8 bits, colormap */
#define TRUECOLOR   011	/* 24 bits, colormap */
#define DIRECTCOLOR 100	/* 24 bits, no colormap */
#define TRUE          1
#define FALSE         0

typedef unsigned char U_CHAR;

/*
 * Wavefront type declarations.
 */
RLB_HEADER	rlb_head;
/*
 * Utah type declarations.
 */
rle_map		*colormap;
/*
 * Other declarations.
 */
FILE		*fpin, *fpout;
char		rlaname[BUFSIZ], progname[30], *str;
int		visual, is_alpha, maplen;
int		verbose = 0, header = 0;
int		i;
/*-----------------------------------------------------------------------------
 *                                                         Read the rle header.
 */
void read_rle_header(minx, maxx, miny, maxy)
int	*minx, *maxx, *miny, *maxy;
{
   rle_dflt_hdr.rle_file = fpin;
   rle_get_setup(&rle_dflt_hdr);
   *minx = rle_dflt_hdr.xmin;
   *maxx = rle_dflt_hdr.xmax;
   *miny = rle_dflt_hdr.ymin;
   *maxy = rle_dflt_hdr.ymax;
   VPRINTF(stderr, "Image size: %dx%d\n", *maxx-*minx+1, *maxy-*miny+1);
   if (rle_dflt_hdr.ncolors == 1 && rle_dflt_hdr.ncmap == 3) {
      visual = PSEUDOCOLOR;
      colormap = rle_dflt_hdr.cmap;
      maplen = (1 << rle_dflt_hdr.cmaplen);
      VPRINTF(stderr, "Mapped color image with a map of length %d.\n", maplen);
      }
   else if (rle_dflt_hdr.ncolors == 3 && rle_dflt_hdr.ncmap == 0) {
      visual = DIRECTCOLOR;
      VPRINTF(stderr, "24 bit color image, no colormap.\n");
      }
   else if (rle_dflt_hdr.ncolors == 3 && rle_dflt_hdr.ncmap == 3) {
      visual = TRUECOLOR;
      colormap = rle_dflt_hdr.cmap;
      maplen = (1 << rle_dflt_hdr.cmaplen);
      VPRINTF(stderr, "24 bit color image with color map of length %d\n" ,maplen);
      }
   else if (rle_dflt_hdr.ncolors == 1 && rle_dflt_hdr.ncmap == 0) {
      visual = GRAYSCALE;
      VPRINTF(stderr, "Grayscale image.\n");
      }
   else {
      fprintf(stderr,
              "ncolors = %d, ncmap = %d, I don't know how to handle this!\n",
              rle_dflt_hdr.ncolors, rle_dflt_hdr.ncmap);
      exit(-1);
      }
   if (rle_dflt_hdr.alpha == 0) {
      is_alpha = FALSE;
      VPRINTF(stderr, "No alpha channel.\n");
      }
   else if (rle_dflt_hdr.alpha == 1) {
      is_alpha = TRUE;
      VPRINTF(stderr, "Alpha channel exists!\n");
      }
   else {
      fprintf(stderr, "alpha = %d, I don't know how to handle this!\n",
              rle_dflt_hdr.alpha);
      exit(-1);
      }
   switch (rle_dflt_hdr.background) {
      case 0:
         VPRINTF(stderr, "Use all pixels, ignore background color.");
         break;
      case 1:
         VPRINTF(stderr,
                  "Use only non-background pixels, ignore background color.");
         break;
      case 2:
         VPRINTF(stderr,
        "Use only non-background pixels, clear to background color (default).");
         break;
      default:
         VPRINTF(stderr, "Unknown background flag!\n");
         break;
      }
   for (i = 0; i < rle_dflt_hdr.ncolors; i++)
      VPRINTF(stderr, " %d", rle_dflt_hdr.bg_color[i]);
   if (rle_dflt_hdr.ncolors == 1 && rle_dflt_hdr.ncmap == 3) {
      VPRINTF(stderr, " (%d %d %d)\n",
              rle_dflt_hdr.cmap[rle_dflt_hdr.bg_color[0]]>>8,
              rle_dflt_hdr.cmap[rle_dflt_hdr.bg_color[0]+256]>>8,
              rle_dflt_hdr.cmap[rle_dflt_hdr.bg_color[0]+512]>>8);
      }
   else if (rle_dflt_hdr.ncolors == 3 && rle_dflt_hdr.ncmap == 3) {
      VPRINTF(stderr, " (%d %d %d)\n",
              rle_dflt_hdr.cmap[rle_dflt_hdr.bg_color[0]]>>8,
              rle_dflt_hdr.cmap[rle_dflt_hdr.bg_color[1]+256]>>8,
              rle_dflt_hdr.cmap[rle_dflt_hdr.bg_color[2]+512]>>8);
      }
   else
      VPRINTF(stderr, "\n");
   if (rle_dflt_hdr.comments)
      for (i = 0; rle_dflt_hdr.comments[i] != NULL; i++)
         VPRINTF(stderr, "%s\n", rle_dflt_hdr.comments[i]);
}
/*-----------------------------------------------------------------------------
 *                                                  Write the wavefront header.
 */
void write_rlb_header(minx, maxx, miny, maxy, frame_number)
int	minx, maxx, miny, maxy, frame_number;
{
   char		*ctime();
   char		*d_str;
   long		second;

   bzero(&rlb_head, 740);
   rlb_head.window.left   = minx;
   rlb_head.window.right  = maxx;
   rlb_head.window.top    = maxy;
   rlb_head.window.bottom = miny;
   rlb_head.active_window.left   = rlb_head.window.left;
   rlb_head.active_window.right  = rlb_head.window.right;
   rlb_head.active_window.top    = rlb_head.window.top;
   rlb_head.active_window.bottom = rlb_head.window.bottom;
   rlb_head.frame     = frame_number;
   rlb_head.num_chan  = 3;
   rlb_head.num_matte = 1;
   strcpy(rlb_head.gamma, "2.2000");
   strcpy(rlb_head.red_pri, "0.670 0.330");
   strcpy(rlb_head.green_pri, "0.210 0.710");
   strcpy(rlb_head.blue_pri, "0.140 0.080");
   strcpy(rlb_head.white_pt, "0.310 0.316");
   d_str = strrchr(rlaname, '/');
   if ( d_str == NULL )
       d_str = rlaname;
   strncpy(rlb_head.name, d_str, 29);
   rlb_head.name[29] = '\0';
   strcpy(rlb_head.desc, "A Wavefront file converted from an rle file.");
   strcpy(rlb_head.program, progname);
#if NO_GETHOSTNAME
   /* from the Solaris2 Porting FAQ */
   if (sysinfo(SI_HOSTNAME, rlb_head.machine,32) <0) {
     perror("SI_HOSTNAME");
     exit(BAD);
   }
#else
   gethostname(rlb_head.machine, 32);
#endif
   strcpy(rlb_head.user, getenv("USER"));
   second = time((long *)NULL);
   d_str = ctime(&second);
   strncpy(rlb_head.date, &d_str[4], 12);
   strncpy(&rlb_head.date[12], &d_str[19], 5);
   rlb_head.date[17] = '\0';
   strcpy(rlb_head.aspect, "user defined");
   sprintf(rlb_head.aspect_ratio, "%3.2f", (float)(maxx-minx+1)/(maxy-miny+1));
   strcpy(rlb_head.chan, "rgb");
/*
 * The following fields do not really need to be set because they should
 * all be zero and already are because bzero was used on the entire header.
 */
/*
   rlb_head.aux_mask = 0;
   rlb_head.encode_type = 0;
   rlb_head.field = 0;
   rlb_head.filter_type = 0;
   rlb_head.job_num = 0;
   rlb_head.lut_size = 0;
   rlb_head.lut_type = 0;
   rlb_head.magic_number = 0;
   rlb_head.mix_type = 0;
   rlb_head.num_aux = 0;
   rlb_head.padding = 0;
   rlb_head.storage_type = 0;
   rlb_head.user_space_size = 0;
   rlb_head.wf_space_size = 0;
*/
   fwrite(&rlb_head, 740, 1, fpout);
}
/*-----------------------------------------------------------------------------
 *                                              Encode the data before writing.
 */
int encode(c_in, c_out, width)
U_CHAR	*c_in;
U_CHAR	*c_out;
int	width;
{
   int		len;
   int		ct;

   len = 0;
   while (width > 0) {
      if ((width > 1) && (c_in[0] == c_in[1])) {
/*
 * Cruise until we find a different pixel value.
 */
         for (ct = 2; ct < width; ct++) {
            if (c_in[ct] != c_in[ct-1])
               break;
            if (ct >= 127)
               break;
            }
/*
 * Write out the count.
 */
         *c_out++ = ct - 1;
         len++;
/*
 * Write out the value.
 */
         *c_out++ = *c_in;
         len++;
         width -= ct;
         c_in += ct;
         }
      else {
/*
 * Cruise until they can be encoded again.
 */
         for (ct = 1; ct < width; ct++) {
            if ((width - ct > 1) && (c_in[ct] == c_in[ct+1]))
               break;
            if (ct >= 127)
               break;
            }
/*
 * Write out the count.
 */
         *c_out++ = 256 - ct;
         len++;
/*
 * Copy string of pixels.
 */
         for (; ct-- > 0; len++, width--)
            *c_out++ = *c_in++;
         }
      }
      return len;
}
/*-----------------------------------------------------------------------------
 *                                          Write the data portion of the file.
 */
void write_rlb_data()
{
   rle_pixel	**scanline;
   U_CHAR	*red, *green, *blue, *matte;
   U_CHAR	*buf;
   int		*offset;
   int		width, height;
   int		scan, x;
   short	len;
   long		offptr;
/*
 * Create a scanline offset table then write it to reserve space in the file.
 */
   width  = rlb_head.window.right - rlb_head.window.left + 1;
   height = rlb_head.window.top - rlb_head.window.bottom + 1;
   if (!(offset = (int *)malloc(sizeof(int) * height))) {
      fprintf(stderr, "Offset malloc failed!\n");
      exit(-3);
      }
   offptr = ftell(fpout);
   if (fwrite(offset, sizeof(int), height, fpout) != height) {
      fprintf(stderr, "Offset table write failed!\n");
      exit(-4);
      }
   if (!(buf = (U_CHAR *)malloc(sizeof(U_CHAR) * width * 2))) {
      fprintf(stderr, "Buf malloc failed!\n");
      exit(-7);
      }
   if (!(red = (U_CHAR *)malloc(sizeof(U_CHAR) * width * 4))) {
      fprintf(stderr, "Red scanline malloc failed!\n");
      exit(-8);
      }
   green = &red[width];
   blue  = &green[width];
   matte = &blue[width];
/*
 * Loop through those scan lines.
 */
   if (!(rle_row_alloc(&rle_dflt_hdr, &scanline) == 0)) {
      fprintf(stderr, "rle row alloc failed!\n");
      exit(-1);
      }
   for (scan = 0; scan < height; scan++) {
      (void)rle_getrow(&rle_dflt_hdr, scanline);
      switch (visual) {
         case GRAYSCALE:	/* 8 bits without colormap */
               red   = scanline[0];
               green = scanline[0];
               blue  = scanline[0];
               for (x = 0; x < width; x++)
                  matte[x] = (red[x] || green[x] || blue ? 255 : 0);
            break;
         case TRUECOLOR:	/* 24 bits with colormap */
            if (is_alpha) {
               matte = scanline[-1];
               for (x = 0; x < width; x++) {
                  red[x]   = colormap[scanline[0][x]]>>8;
                  green[x] = colormap[scanline[1][x]+256]>>8;
                  blue[x]  = colormap[scanline[2][x]+512]>>8;
                  }
               }
            else
               for (x = 0; x < width; x++) {
                  if (colormap[scanline[0][x]]>>8 != scanline[0][x] ||
                      colormap[scanline[1][x]+256]>>8 != scanline[1][x] ||
                      colormap[scanline[2][x]+512]>>8 != scanline[2][x])
                     fprintf(stderr,"A truecolor image with colormap whose resulting values don't match!\n");
                  red[x]   = colormap[scanline[0][x]]>>8;
                  green[x] = colormap[scanline[1][x]+256]>>8;
                  blue[x]  = colormap[scanline[2][x]+512]>>8;
                  matte[x] = (red[x] || green[x] || blue ? 255 : 0);
                  }
            break;
         case DIRECTCOLOR:	/* 24 bits without colormap */
            if (is_alpha) {
               matte = scanline[-1];
               red   = scanline[0];
               green = scanline[1];
               blue  = scanline[2];
               }
            else {
               red   = scanline[0];
               green = scanline[1];
               blue  = scanline[2];
               for (x = 0; x < width; x++)
                  matte[x] = (red[x] || green[x] || blue ? 255 : 0);
               }
            break;
         case PSEUDOCOLOR:	/* 8 bits with colormap */
               for (x = 0; x < width; x++) {
                  red[x]   = colormap[scanline[0][x]]>>8;
                  green[x] = colormap[scanline[0][x]+256]>>8;
                  blue[x]  = colormap[scanline[0][x]+512]>>8;
                  matte[x] = (red[x] || green[x] || blue ? 255 : 0);
                  }
            break;
         default:
            break;
         }
/*
 * Record the location in the file.
 */
      offset[scan] = ftell(fpout);
/*
 * Write the red scan line.
 */
      len = encode(red, buf, width);
      fwrite(&len, sizeof(short), 1, fpout);
      fwrite(buf, sizeof(U_CHAR), (int)len, fpout);
/*
 * Write the green scan line.
 */
      len = encode(green, buf, width);
      fwrite(&len, sizeof(short), 1, fpout);
      fwrite(buf, sizeof(U_CHAR), (int)len, fpout);
/*
 * Write the blue scan line.
 */
      len = encode(blue, buf, width);
      fwrite(&len, sizeof(short), 1, fpout);
      fwrite(buf, sizeof(U_CHAR), (int)len, fpout);
/*
 * Write the matte scan line.
 */
      len = encode(matte, buf, width);
      fwrite(&len, sizeof(short), 1, fpout);
      fwrite(buf, sizeof(U_CHAR), (int)len, fpout);
      } /* end of for scan = 0 to height */
/*
 * Write that offset table again with correct values.
 */
   fseek(fpout, offptr, 0);
   fwrite(offset, sizeof(int), height, fpout);
/*
 * Free up some stuff.
 */
   free(offset);
   free(buf);
   free(red);
}
/*-----------------------------------------------------------------------------
 *                                        Convert an rle file into an rla file.
 */
int
main(argc, argv)
int argc;
char **argv;
{
   char		*lperiodP, *fname = NULL;
   int		minx, maxx, miny, maxy, frame_number = 1;
/*
 * Get those options.
 */
   if (!scanargs(argc,argv,
       "% v%- h%- infile%s",
       &verbose,
       &header,
       &fname))
      exit(-1);
/*
 * Open the file.
 */
   fpin = rle_open_f( cmd_name( argv ), fname, "r" );
   if (!header) {
      strcpy(rlaname, fname);
      lperiodP = strrchr(rlaname, '.');
      if (lperiodP)
         strcpy(lperiodP, ".rla");
      else
         strcat(rlaname, ".rla");
      if (!(fpout = fopen(rlaname, "w"))) {
         fprintf(stderr, "Cannot open %s for writing.\n", rlaname);
         exit(-1);
         }
      }
   strncpy(progname, cmd_name( argv ), 29);
   progname[29] = 0;
/*
 * Read the rle file header.
 */
   read_rle_header(&minx, &maxx, &miny, &maxy);
   if (header)
      exit(0);
/*
 * Write the rlb file header.
 */
   write_rlb_header(minx, maxx, miny, maxy, frame_number);
/*
 * Write the rest of the Wavefront (rlb) file.
 */
   write_rlb_data();
   fclose(fpin);
   fclose(fpout);

   return 0;
}
