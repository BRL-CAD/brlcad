/*                  P I C K E T _ F E N C E . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file picket_fence.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


int main(int argc, char *argv[])
{
  struct rt_wdb *fp_db;
  long i, j, k, l;
  char name[256];
  char pname[256];
  char number[256];
  char w1name[256];
  char w2name[256];
  char firstname[256];
  char prefix[256];
  
  struct wmember wm;
  struct wmember wm2;
  struct wmember fwm;
  struct wmember swm;
  struct wmember *nwm;

  fastf_t first_mat[16];

  fastf_t s0[24] = {0, 0, 0,
		   0, 0, 0,
		   15, 0, 0,
		   15, 0, 0,
		   0, 0, 0,
		   0, 0, 0,
		   15, 0, 0,
		   15, 0, 0};
  point_t c0 = {15, 0, 0};
  point_t w0 = {0, 0, 0};
  point_t w1 = {25, 0, 0};
  fastf_t s1[24] = {-0.001, 0, 0,
		   -0.001, 0, 0,
		   -12.6, 0, 0,
		   -12.6, 0, 0,
		   -0.001, 0, 0,
		   -0.001, 0, 0,
		   -12.6, 0, 0,
		   -12.6, 0, 0};
  fastf_t xlen = .0001;
  fastf_t ylen = -26;
  fastf_t zlen;
  fastf_t xt, yt;
  fastf_t x_top_len = 20;
  vect_t w0x = {0, 1, 0},
	 w0z = {0, 0, -1},
	 w1x = {0, -1, 0},
	 w1z = {0, 0, -1},
	 c0a = {4, 0, 0},
	 c0b = {0, 0, 0},
	 c0c = {4, 0, 0},
	 c0d = {0, 0, 0},
	 c0h = {0, 0, 0};
  fastf_t x0, y0, z0, x1, y1, z1;
  fastf_t height;
  fastf_t width;
  fastf_t pwidth;
  fastf_t ps;
  long numposts;
  fastf_t zstep;

  int round = 0;
  
  if (argc < 10)
    {
      fprintf(stderr, "Usage: pf <filename> <prefix> <height in mm> <spacing> <x0> <y0> <z0> ... <xn> <yn> <zn> [-r]\n");
      exit(1);
    }
  strcpy(prefix, argv[2]);
  ps = atof(argv[4]);

  fp_db = wdb_fopen(argv[1]);
  if (fp_db == NULL) {
    perror(argv[1]);
    exit(2);
  }
  mk_id(fp_db, "Picket Fence");
  
  BU_LIST_INIT(&wm.l);
  BU_LIST_INIT(&wm2.l);
  BU_LIST_INIT(&fwm.l);
  BU_LIST_INIT(&swm.l);

  for (j = 0; j < ((argc - 4) / 3) - 1; j++)
    {
      if (!strcmp(argv[argc - 1], "-r"))
	round = 1;
      x0 = (fastf_t)atof(argv[5 + (3 * j)]);
      y0 = (fastf_t)atof(argv[6 + (3 * j)]);
      z0 = (fastf_t)atof(argv[7 + (3 * j)]);
      x1 = (fastf_t)atof(argv[8 + (3 * j)]);
      y1 = (fastf_t)atof(argv[9 + (3 * j)]);
      z1 = (fastf_t)atof(argv[10 + (3 * j)]);
      height = (fastf_t)atof(argv[3]);
      width = sqrt(((x1 - x0) * (x1 - x0)) + ((y1 - y0) * (y1 - y0)));;
      pwidth = ((fastf_t)width / 
		(fastf_t)(((int)(width / (51+ps))) * (51+ps))) * (51+ps);
      numposts = (width / pwidth);
      zstep = (z1 - z0) / numposts;
      zlen = -.076001 * height;
      s0[4] = s0[7] = s0[16] = s0[19] = pwidth - ps;
      s0[14] = s0[17] = s0[20] = s0[23] = height;
      c0[1] = (pwidth - ps) / 2.0;
      w0[2] = w1[2] = .924 * height;
      w1[1] = pwidth - ps;
      s1[2] = s1[11] = z0 + (height / 3);
      s1[5] = s1[8] = z1 + (height / 3);
      s1[14] = s1[23] = z0 + ((height / 3) + 102);
      s1[17] = s1[20] = z1 + ((height / 3) + 102);
      s1[4] = s1[7] = s1[16] = s1[19] = width;
      c0b[1] = (pwidth - ps) / 2.0;
      c0d[1] = (pwidth - ps) / 2.0;
      c0h[2] = height;

      sprintf(w1name, "%swedge1-%ld.s", prefix, j);
      mk_wedge(fp_db, w1name, w0, w0x, w0z, xlen, ylen, zlen, x_top_len);
      
      sprintf(w2name, "%swedge2-%ld.s", prefix, j);
      mk_wedge(fp_db, w2name, w1, w1x, w1z, xlen, ylen, zlen, x_top_len);
      
      sprintf(name, "%spost-%ld.s", prefix, j);
      mk_arb8(fp_db, name, s0);
      mk_addmember(name, &wm.l, NULL, WMOP_UNION);
      mk_addmember(w1name, &wm.l, NULL, WMOP_SUBTRACT);
      mk_addmember(w2name, &wm.l, NULL, WMOP_SUBTRACT);
      
      if (round)
	{
	  sprintf(name, "%spost_c.s", prefix);
	  mk_tgc(fp_db, name, c0, c0h, c0a, c0b, c0c, c0d);
	  mk_addmember(name, &wm.l, NULL, WMOP_UNION);
	  mk_addmember(w1name, &wm.l, NULL, WMOP_SUBTRACT);
	  mk_addmember(w2name, &wm.l, NULL, WMOP_SUBTRACT);
	}
      sprintf(name, "%sls%ld.s", prefix, j);
      mk_arb8(fp_db, name, s1);
      mk_addmember(name, &swm.l, NULL, WMOP_UNION);
      
      for (k = 0; k < 8; k++)
	s1[(3 * k) + 2] += (height / 3);
      
      sprintf(name, "%shs%ld.s", prefix, j);
      mk_arb8(fp_db, name, s1);
      mk_addmember(name, &swm.l, NULL, WMOP_UNION);
      
      sprintf(pname, "%sp-%ld.c", prefix, j);
      mk_lcomb(fp_db, pname, &wm, 0, "plastic", "", "50 30 10", 0);
      
      for (i = 0; i < numposts; i++)
	{
	  sprintf(name, "%sp%ld-%ld.r", prefix, j, i);
	  mk_addmember(pname, &wm2.l, NULL, WMOP_UNION);
	  mk_lcomb(fp_db, name, &wm2, 0, "plastic", "", "50 50 20", 0);
	  nwm = mk_addmember(name, &swm.l, NULL, WMOP_UNION);
	  for (k = 0; k < 16; k++)
	    nwm->wm_mat[k] = 0;
	  nwm->wm_mat[0] = 1;
	  nwm->wm_mat[5] = 1;
	  nwm->wm_mat[7] = i * pwidth;
	  nwm->wm_mat[10] = 1;
	  nwm->wm_mat[11] = i * zstep;
	  nwm->wm_mat[15] = 1;
	}
      
      sprintf(name, "%ssec%ld.c", prefix, j);
      mk_lcomb(fp_db, name, &swm, 0, "plastic", "", "50 50 20", 0);
      nwm = mk_addmember(name, &fwm.l, NULL, WMOP_SUBTRACT);
      xt = x1 - x0;
      yt = y1 - y0;
      xt /= sqrt((xt * xt) + (yt * yt));
      yt /= sqrt((xt * xt) + (yt * yt));
      nwm->wm_mat[0] = nwm->wm_mat[5] = cos(atan2(-xt, yt));
      nwm->wm_mat[1] = -sin(atan2(-xt, yt));
      nwm->wm_mat[3] = x0;
      nwm->wm_mat[4] = -(nwm->wm_mat[1]);
      nwm->wm_mat[5] = nwm->wm_mat[0];
      nwm->wm_mat[7] = y0;
      nwm->wm_mat[10] = 1;
      nwm->wm_mat[11] = z0;
      nwm->wm_mat[15] = 1;
      nwm = mk_addmember(name, &fwm.l, NULL, WMOP_UNION);
      xt = x1 - x0;
      yt = y1 - y0;
      xt /= sqrt((xt * xt) + (yt * yt));
      yt /= sqrt((xt * xt) + (yt * yt));
      nwm->wm_mat[0] = nwm->wm_mat[5] = cos(atan2(-xt, yt));
      nwm->wm_mat[1] = -sin(atan2(-xt, yt));
      nwm->wm_mat[3] = x0;
      nwm->wm_mat[4] = -(nwm->wm_mat[1]);
      nwm->wm_mat[5] = nwm->wm_mat[0];
      nwm->wm_mat[7] = y0;
      nwm->wm_mat[10] = 1;
      nwm->wm_mat[11] = z0;
      nwm->wm_mat[15] = 1;
      if (j == 0)
	{
	  strcpy(firstname, name);
	  for (l = 0; l < 16; l++)
	    first_mat[l] = nwm->wm_mat[l];
	}
    }
  nwm = mk_addmember(firstname, &fwm.l, NULL, WMOP_SUBTRACT);
  for (l = 0; l < 16; l++)
    nwm->wm_mat[l] = first_mat[l];
  sprintf(name, "%sfence.c", prefix);
  mk_lcomb(fp_db, name, &fwm, 0, "plastic", "", "50 50 20", 0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
