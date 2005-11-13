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
 * rlatorle - A program which will convert Wavefront's "rla" or "rlb" images
 *            into Utah's "rle" image format.
 *
 * Author:      Wesley C. Barris
 *              AHPCRC
 *              Minnesota Supercomputer Center, Inc.
 * Date:        June 20, 1990
 *              Copyright @ 1990, Minnesota Supercomputer Center, Inc.
 *
 * RESTRICTED RIGHTS LEGEND
 *
 * Use, duplication, or disclosure of this software and its documentation
 * by the Government is subject to restrictions as set forth in subdivision
 * { (b) (3) (ii) } of the Rights in Technical Data and Computer Software
 * clause at 52.227-7013.
 */
static char rcsid[] = "$Header$";
/*
rlatorle()				Tag the file.
*/
/*-----------------------------------------------------------------------------
 * System includes.
 */
#include "rle.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rla_header.h"
#include "rlb_header.h"

#define VPRINTF if (verbose || header) fprintf

typedef unsigned char U_CHAR;
/*
 * Global variables.
 */
rle_hdr         hdr;
union
{
    RLA_HEADER	rla_head;
    RLB_HEADER	rlb_head;
} head;

#ifdef CRAY2CC
#define SHORTREAD(var, fp) {fread(&craybuf, 2, 1, fp); *var=craybuf>>48;}
#define LONGREAD(var, fp) {fread(&craybuf, 4, 1, fp); *var=craybuf>>32;}
#else
#define SHORTREAD(var, fp) {fread(var, 2, 1, fp);}
#define LONGREAD(var, fp) {fread(var, 4, 1, fp);}
#endif

static FILE		*fp;
static int		act_x_res, act_y_res;
static int		width, height;
static int		verbose = 0, header = 0, do_matte = 0;
static int		rlb_flag = 0;
/*-----------------------------------------------------------------------------
 *                                        Read the Wavefront image file header.
 */
void read_rla_header(act_x_res, act_y_res, width, height)
int *act_x_res;
int *act_y_res;
int *width;
int *height;
{

#ifdef CRAY2CC
   long craybuf;

   SHORTREAD(&head.rla_head.window.left, fp);
   SHORTREAD(&head.rla_head.window.right, fp);
   SHORTREAD(&head.rla_head.window.bottom, fp);
   SHORTREAD(&head.rla_head.window.top, fp);
   SHORTREAD(&head.rla_head.active_window.left, fp);
   SHORTREAD(&head.rla_head.active_window.right, fp);
   SHORTREAD(&head.rla_head.active_window.bottom, fp);
   SHORTREAD(&head.rla_head.active_window.top, fp);
   SHORTREAD(&head.rla_head.frame, fp);
   SHORTREAD(&head.rla_head.storage_type, fp);
   SHORTREAD(&head.rla_head.num_chan, fp);
   SHORTREAD(&head.rla_head.num_matte, fp);
   SHORTREAD(&head.rla_head.num_aux, fp);
   SHORTREAD(&head.rla_head.aux_mask, fp);
   fread(&head.rla_head.gamma, 16, 1, fp);
   fread(&head.rla_head.red_pri, 24, 1, fp);
   fread(&head.rla_head.green_pri, 24, 1, fp);
   fread(&head.rla_head.blue_pri, 24, 1, fp);
   fread(&head.rla_head.white_pt, 24, 1, fp);
   LONGREAD(&head.rla_head.job_num, fp);
   fread(&head.rla_head.name, 128, 1, fp);
   fread(&head.rla_head.desc, 128, 1, fp);
   fread(&head.rla_head.program, 64, 1, fp);
   fread(&head.rla_head.machine, 32, 1, fp);
   fread(&head.rla_head.user, 32, 1, fp);
   fread(&head.rla_head.date, 20, 1, fp);
   if (rlb_flag) {
      fread(&head.rlb_head.aspect, 24, 1, fp);
      fread(&head.rlb_head.aspect_ratio, 8, 1, fp);
      fread(&head.rlb_head.chan, 32, 1, fp);
      SHORTREAD(&head.rlb_head.field, fp);
      SHORTREAD(&head.rlb_head.filter_type, fp);
      LONGREAD(&head.rlb_head.magic_number, fp);
      LONGREAD(&head.rlb_head.lut_size, fp);
      LONGREAD(&head.rlb_head.user_space_size, fp);
      LONGREAD(&head.rlb_head.wf_space_size, fp);
      SHORTREAD(&head.rlb_head.lut_type, fp);
      SHORTREAD(&head.rlb_head.mix_type, fp);
      SHORTREAD(&head.rlb_head.encode_type, fp);
      SHORTREAD(&head.rlb_head.padding, fp);
      fread(&head.rlb_head.space, 100, 1, fp);
      }
   else {
      fread(&head.rla_head.aspect, 32, 1, fp);
      fread(&head.rla_head.chan, 32, 1, fp);
      fread(&head.rla_head.space, 128, 1, fp);
      }
#else
   if (fread(&head, 740, 1, fp) != 1) {
      fprintf(stderr, "Error reading Wavefront file header!\n");
      exit(-2);
      }
#endif
   *act_x_res = head.rla_head.active_window.right-
       head.rla_head.active_window.left+1;
   *act_y_res = head.rla_head.active_window.top-
       head.rla_head.active_window.bottom+1;
   *width     = head.rla_head.window.right-head.rla_head.window.left+1;
   *height    = head.rla_head.window.top-head.rla_head.window.bottom+1;
   VPRINTF(stderr, "Full image:         %dx%d\n", *width, *height);
   VPRINTF(stderr, "Active window:      %dx%d\n", *act_x_res, *act_y_res);
   VPRINTF(stderr, "Number of channels: %d\n", head.rla_head.num_chan);
   VPRINTF(stderr, "Number of mattes:   %d\n", head.rla_head.num_matte);
   VPRINTF(stderr, "Image gamma:        %s\n", head.rla_head.gamma);
   VPRINTF(stderr, "Original filename:  %s\n", head.rla_head.name);
   VPRINTF(stderr, "Description:        %s\n", head.rla_head.desc);
   VPRINTF(stderr, "Machine:            %s\n", head.rla_head.machine);
   VPRINTF(stderr, "User:               %s\n", head.rla_head.user);
   VPRINTF(stderr, "Date:               %s\n", head.rla_head.date);
   if ( rlb_flag ) {
       VPRINTF(stderr, "Aspect:             %s\n", head.rlb_head.aspect);
       VPRINTF(stderr, "Aspect ratio:       %s\n", head.rlb_head.aspect_ratio);
       }
   else {
       VPRINTF(stderr, "Aspect:             %s\n", head.rla_head.aspect);
       VPRINTF(stderr, "Aspect ratio:       %s\n", "-unused-");
       }
   VPRINTF(stderr, "Channel color space %s\n", head.rla_head.chan);
   if ( rlb_flag )
       VPRINTF(stderr, "Interlaced?         %s\n", head.rlb_head.filter_type);
   else
       VPRINTF(stderr, "Interlaced?         %s\n", "-unused-");
   if (do_matte)
      VPRINTF(stderr, "Converting matte channel only...\n");
}
/*-----------------------------------------------------------------------------
 *                                             Write the rle image file header.
 */
void write_rle_header()
{
   hdr.xmin    = head.rla_head.window.left;
   hdr.xmax    = head.rla_head.window.right;
   hdr.ymin    = head.rla_head.window.bottom;
   hdr.ymax    = head.rla_head.window.top;
   hdr.alpha   = (head.rla_head.num_matte && !do_matte ? 1 : 0);
   hdr.ncolors = (do_matte ? 1 : head.rla_head.num_chan);
   /*hdr.ncmap   = (map ? 3 : 0);*/
   /*hdr.cmaplen = (map ? 8 : 0);*/
   /*hdr.cmap    = (map ? color_map : NULL);*/
   rle_putcom(head.rla_head.desc, &hdr);
   /*hdr.background = 0;*/
   if (hdr.alpha)
      RLE_SET_BIT(hdr, RLE_ALPHA);
   if (!do_matte) {
      RLE_SET_BIT(hdr, RLE_RED);
      RLE_SET_BIT(hdr, RLE_GREEN);
      RLE_SET_BIT(hdr, RLE_BLUE);
      }
   rle_put_setup(&hdr);
}
/*-----------------------------------------------------------------------------
 *                                            Decode run length encoded pixels.
 */
void
decode(c_in, c_out, len)
U_CHAR	*c_in;
U_CHAR	*c_out;
int	len;
{
   int	ct;

   while(len > 0) {
      ct = *c_in++;
      len--;
      if (ct < 128) {
/*
 * Repeat pixel value ct+1 times.
 */
         while (ct-- >= 0)
            *c_out++ = *c_in;
         c_in++;
         len--;
         }
      else {
/*
 * Copy ct unencoded values.
 */
         for (ct = 256-ct; ct-- > 0; len--)
            *c_out++ = *c_in++;
         }
      }
}
/*-----------------------------------------------------------------------------
 *                                      Write the rle data portion of the file.
 */
void write_rle_data()
{
   int		*offset;
   int		x;
   int		bottom;
   int		left;
   int		scan;
#ifdef CRAY2CC
   long		craybuf;
#endif
   U_CHAR	*red, *green, *blue, *matte;
   U_CHAR	*buf;
   short	len;
   rle_pixel	*scanline[4];
/*
 * Read scanline offset table.
 */
   if (!(offset = (int *)malloc(sizeof(int) * act_y_res))) {
      fprintf(stderr, "Offset malloc failed!\n");
      exit(-3);
      }
#ifdef CRAY2CC
   for (scan=0;scan<act_y_res;scan++) {
      fread(&craybuf, 4, 1, fp); offset[scan] = craybuf >> 32;
      }
#else
   if (fread(offset, 4, act_y_res, fp) != act_y_res) {
      fprintf(stderr, "Offset table read failed!\n");
      exit(-4);
      }
#endif
/*
 * Allocate some memory.
 */
   if (!(buf = (U_CHAR *)malloc(width * 2))) {
      fprintf(stderr, "Buf malloc failed!\n");
      exit(-7);
      }
   if (!(red = (U_CHAR *)malloc(width * 4))) {
      fprintf(stderr, "Red scanline malloc failed!\n");
      exit(-8);
      }
   green = &red[width];
   blue  = &green[width];
   matte = &blue[width];
   /*bzero((char *)blank, width*4);*/
   if (((scanline[0]=(rle_pixel *)malloc(width))==NULL) ||
       ((scanline[1]=(rle_pixel *)malloc(width))==NULL) ||
       ((scanline[2]=(rle_pixel *)malloc(width))==NULL) ||
       ((scanline[3]=(rle_pixel *)malloc(width))==NULL)) {
      fprintf(stderr, "Unable to malloc space for pixels\n");
      exit(-1);
      }
/*
 * Loop through the rla files image window, write blank lines outside
 * active window.
 */
   bottom = head.rla_head.active_window.bottom;
   left   = head.rla_head.active_window.left;
   for (scan = head.rla_head.window.bottom;
	scan <= head.rla_head.window.top; scan++) {
/*
 * Check for black regions outside active window.
 */
      if ((scan < head.rla_head.active_window.bottom) ||
          (scan > head.rla_head.active_window.top))
         rle_skiprow(&hdr, 1);
      else {
         if (fseek(fp, (long)offset[scan-bottom], 0)) {
            fprintf(stderr, "rla file incomplete!\n");
            exit(-9);
            }
/*
 * Red scanline.
 */
         SHORTREAD(&len, fp);
         fread(buf, 1, (int)len, fp);
         decode(buf, red, (int)len);
/*
 * Green scanline.
 */
         SHORTREAD(&len, fp);
         fread(buf, 1, (int)len, fp);
         decode(buf, green, (int)len);
/*
 * Blue scanline.
 */
         SHORTREAD(&len, fp);
         fread(buf, 1, (int)len, fp);
         decode(buf, blue, (int)len);
/*
 * Matte scanline.
 */
         SHORTREAD(&len, fp);
         fread(buf, 1, (int)len, fp);
         decode(buf, matte, (int)len);
/*
 * Write out RGBM for each pixel.
 */
         for (x=head.rla_head.window.left;
	      x<=head.rla_head.window.right; x++) {
            if ((x < head.rla_head.active_window.left) ||
                (x > head.rla_head.active_window.right)) {
               scanline[0][x] = 0;
               scanline[1][x] = 0;
               scanline[2][x] = 0;
               scanline[3][x] = 0;
               }
            else
               if (do_matte) {
                  scanline[0][x] = (red[x-left] ||
                                  green[x-left] ||
                                   blue[x-left] ? 255 : 0);
                  }
               else {
                  scanline[0][x] = matte[x-left];
                  scanline[1][x] = red[x-left];
                  scanline[2][x] = green[x-left];
                  scanline[3][x] = blue[x-left];
                  }
            }
         if (do_matte)
            rle_putrow(scanline, width, &hdr);
         else
            rle_putrow(scanline + 1, width, &hdr);
         } /* end of if scan is within active y */
      } /* end of for every scanline */
   VPRINTF(stderr, "Done -- write oef to RLE data.\n");
   rle_puteof(&hdr);
/*
 * Free up some stuff.
 */
   free(offset);
   /*free(blank);*/
   free(buf);
   free(red);
   free(scanline[0]);
   free(scanline[1]);
   free(scanline[2]);
   free(scanline[3]);
}
/*-----------------------------------------------------------------------------
 *                      Convert an Wavefront image file into an rle image file.
 */
int
main(argc, argv)
int argc;
char **argv;
{
   char		*periodP, *inname = NULL, *outname = NULL;
   static char	filename[BUFSIZ];
   int		oflag = 0;
/*
 * Get those options.
 */
   if (!scanargs(argc,argv,
       "% b%- v%- h%- m%- o%-outfile!s infile.rla%s",
       &rlb_flag,
       &verbose,
       &header,
       &do_matte,
       &oflag, &outname,
       &inname))
      exit(-1);
/*
 * Open the file.
 */
   if (inname == NULL) {
      strcpy(filename, "stdin");
      fp = stdin;
      }
   else {
      periodP = strrchr(inname, (int)'.');
      strcpy(filename, inname);
      if (periodP) {
         if (rlb_flag) {
            if (strcmp(periodP, ".rlb")) { /* does not end in rla */
               if (!strcmp(periodP, ".rla")) {
                  fprintf(stderr, "This is an rla file -- don't use the -b flag.\n");
                  exit(1);
                  }
               strcat(filename, ".rlb");
               }
            }
         else {
            if (strcmp(periodP, ".rla")) { /* does not end in rla */
               if (!strcmp(periodP, ".rlb"))
                  rlb_flag = 1;
               else
                  strcat(filename, ".rla");
               }
            }
         }
      else				/* no ext -- add one */
         if (rlb_flag)
            strcat(filename, ".rlb");
         else
            strcat(filename, ".rla");
      if (!(fp = fopen(filename, "r"))) {
         fprintf(stderr, "Cannot open %s for reading.\n", filename);
         exit(-1);
         }
      }
/*
 * Read the Wavefront file file header.
 */
   read_rla_header(&act_x_res, &act_y_res, &width, &height);
   if (header)
      exit(0);
/*
 * Write the rle file header.
 */
   hdr = *rle_hdr_init( (rle_hdr *)NULL );
   hdr.rle_file = rle_open_f( cmd_name(argv), outname, "w" );
   rle_names( &hdr, cmd_name(argv), outname, 0 );
   rle_addhist(argv, (rle_hdr *)NULL, &hdr);
   write_rle_header();
/*
 * Write the rle file data.
 */
   write_rle_data();
   fclose(fp);

   return 0;
}
