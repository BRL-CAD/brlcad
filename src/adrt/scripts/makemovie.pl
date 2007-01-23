#!/usr/bin/perl
#                    M A K E M O V I E . P L
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#  Justin Shumaker
#
############################################################
# For use with mpeg2encode and ADRT to generate MPEG movies.
# Usage: ./makemovie.pl movie.mpeg
############################################################

if($ARGV[2] eq "") {
  print "usage: ./makemovie.pl width height quality movie.mpeg\n";
  print "       * Quality: 1 == low, 2 == medium, 3 == high.\n";
  print "       * Must be run from directory containing PNG images.\n";
  print "       * Assumes input images are of the format image_0000.png.\n";
  print "       * Assumes Image Magick (convert) is installed and in your Path.\n";
  print "       * Assumes (mpeg2encode) is installed and in your Path.\n";
  exit;
}

$width = $ARGV[0];
$height = $ARGV[1];
$bitrate = 1152000.0;
$buffers = 16;
if($ARGV[2] == 2) {
  $bitrate *= 2;
  $buffers *= 2;
} elsif($ARGV[2] == 3) {
  $bitrate *= 4;
  $buffers *= 4;
}
$movie = $ARGV[2];

$list = qx(echo *.ppm);
@images = split(" ", $list);
$num_frames = @images;

sub phase_1 {
  print "Phase 1: Converting (".@images.") images.\n";

  $i = 1;

  foreach $image (@images) {
    ($filename, $ext) = split("_", $image);
    ($num, $ext) = split('\.', $ext);

    $num = int($num);
    $newimage = $filename."_"."$num".".ppm";

#    system("convert $image $newimage");
    system("mv $image $newimage");
    $status = 100 * $i / $num_frames;
    $status = substr($status, 0, 4);
    print "  status: ".$status."%     \r";
    $| = 1; # flush stdout
    $i++;
  }
}

sub phase_2 {
  print "\n";
  print "Phase 2: Generate MPEG movie (".$movie.").\n";

  open(fh, ">mpeg.par");
  print fh <<eol;
mpeg2encode
image_%d  /* name of source files */
-         /* name of reconstructed images ("-": don't store) */
-         /* name of intra quant matrix file     ("-": default matrix) */
-         /* name of non intra quant matrix file ("-": default matrix) */
stat.out  /* name of statistics file ("-": stdout ) */
2         /* input picture file format: 0=*.Y,*.U,*.V, 1=*.yuv, 2=*.ppm */
$num_frames /* number of frames */
0         /* number of first frame */
00:00:00:00 /* timecode of first frame */
12        /* N (# of frames in GOP) */
3         /* M (I/P frame distance) */
1         /* ISO/IEC 11172-2 stream */
0         /* 0:frame pictures, 1:field pictures */
$width    /* horizontal_size */
$height   /* vertical_size */
8         /* aspect_ratio_information 8=CCIR601 625 line, 9=CCIR601 525 line */
4         /* frame_rate_code 1=23.976, 2=24, 3=25, 4=29.97, 5=30 frames/sec. */
$bitrate  /* bit_rate (bits/s) */
$buffers  /* vbv_buffer_size (in multiples of 16 kbit) */
0         /* low_delay  */
1         /* constrained_parameters_flag */
1         /* Profile ID: Simple = 5, Main = 4, SNR = 3, Spatial = 2, High = 1 */
4         /* Level ID:   Low = 10, Main = 8, High 1440 = 6, High = 4          */
1         /* progressive_sequence */
1         /* chroma_format: 1=4:2:0, 2=4:2:2, 3=4:4:4 */
2         /* video_format: 0=comp., 1=PAL, 2=NTSC, 3=SECAM, 4=MAC, 5=unspec. */
5         /* color_primaries */
5         /* transfer_characteristics */
5         /* matrix_coefficients */
$width    /* display_horizontal_size */
$height   /* display_vertical_size */
0         /* intra_dc_precision (0: 8 bit, 1: 9 bit, 2: 10 bit, 3: 11 bit */
0         /* top_field_first */
1 1 1     /* frame_pred_frame_dct (I P B) */
0 0 0     /* concealment_motion_vectors (I P B) */
0 0 0     /* q_scale_type  (I P B) */
0 0 0     /* intra_vlc_format (I P B)*/
0 0 0     /* alternate_scan (I P B) */
0         /* repeat_first_field */
1         /* progressive_frame */
0         /* P distance between complete intra slice refresh */
0         /* rate control: r (reaction parameter) */
0         /* rate control: avg_act (initial average activity) */
0         /* rate control: Xi (initial I frame global complexity measure) */
0         /* rate control: Xp (initial P frame global complexity measure) */
0         /* rate control: Xb (initial B frame global complexity measure) */
0         /* rate control: d0i (initial I frame virtual buffer fullness) */
0         /* rate control: d0p (initial P frame virtual buffer fullness) */
0         /* rate control: d0b (initial B frame virtual buffer fullness) */
2 2 11 11 /* P:  forw_hor_f_code forw_vert_f_code search_width/height */
1 1 3  3  /* B1: forw_hor_f_code forw_vert_f_code search_width/height */
1 1 7  7  /* B1: back_hor_f_code back_vert_f_code search_width/height */
1 1 7  7  /* B2: forw_hor_f_code forw_vert_f_code search_width/height */
1 1 3  3  /* B2: back_hor_f_code back_vert_f_code search_width/height */
eol

  close(fh);
  system("mpeg2encode mpeg.par $movie");
}

sub phase_3 {
  printf("Phase 3: Cleanup.\n");

  ###  Clean up
  system("rm *.ppm");
  system("rm mpeg.par");
  system("rm stat.out");
}

### MAIN ###
&phase_1;
&phase_2;
&phase_3;

# Local Variables:
# mode: Perl
# tab-width: 8
# c-basic-offset: 4
# perl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
