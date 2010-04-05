/* glptsp.c */

/*----------------------------------------------------------------------
-- Copyright (C) 2000, 2001, 2002, 2003 Andrew Makhorin, Department
-- for Applied Informatics, Moscow Aviation Institute, Moscow, Russia.
-- All rights reserved. E-mail: <mao@mai2.rcnet.ru>.
--
-- This file is a part of GLPK (GNU Linear Programming Kit).
--
-- GLPK is free software; you can redistribute it and/or modify it
-- under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2, or (at your option)
-- any later version.
--
-- GLPK is distributed in the hope that it will be useful, but WITHOUT
-- ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
-- License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with GLPK; see the file COPYING. If not, write to the Free
-- Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
-- 02111-1307, USA.
----------------------------------------------------------------------*/

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "glplib.h"
#include "glptsp.h"

/*----------------------------------------------------------------------
-- tsp_read_data - read TSP instance data.
--
-- *Synopsis*
--
-- #include "glptsp.h"
-- TSP *tsp_read_data(char *fname);
--
-- *Description*
--
-- The routine tsp_read_data reads a TSP (or related problem) instance
-- data from the text file, whose name is the character string fname.
--
-- For detailed description of the format recognized by the routine see
-- the report: G.Reinelt, TSPLIB 95.
--
-- *Returns*
--
-- If no error occurred, the routine tsp_read_data returns a pointer to
-- the TSP instance data block, which contains loaded data. In the case
-- of error the routine prints an error message and returns NULL. */

struct dsa
{     /* dynamic storage area used by the routine tsp_read_data */
      char *fname;
      /* name of the input text file */
      FILE *fp;
      /* stream assigned to the input text file */
      int seqn;
      /* line sequential number */
      int c;
      /* current character */
      char token[255+1];
      /* current token */
};

static int get_char(struct dsa *dsa)
{     dsa->c = fgetc(dsa->fp);
      if (ferror(dsa->fp))
      {  print("%s:%d: read error - %s", dsa->fname, dsa->seqn,
            strerror(errno));
         return 1;
      }
      if (feof(dsa->fp))
         dsa->c = EOF;
      else if (dsa->c == '\n')
         dsa->seqn++;
      else if (isspace(dsa->c))
         dsa->c = ' ';
      else if (iscntrl(dsa->c))
      {  print("%s:%d: invalid control character 0x%02X", dsa->fname,
            dsa->seqn, dsa->c);
         return 1;
      }
      return 0;
}

static int skip_spaces(struct dsa *dsa, int across)
{     while (dsa->c == ' ' || (across && dsa->c == '\n'))
         if (get_char(dsa)) return 1;
      return 0;
}

static int scan_keyword(struct dsa *dsa)
{     int len = 0;
      if (skip_spaces(dsa, 0)) return 1;
      dsa->token[0] = '\0';
      while (isalnum(dsa->c) || dsa->c == '_')
      {  if (len == 31)
         {  print("%s:%d: keyword `%s...' too long", dsa->fname,
               dsa->seqn, dsa->token);
            return 1;
         }
         dsa->token[len++] = (char)dsa->c, dsa->token[len] = '\0';
         if (get_char(dsa)) return 1;
      }
      if (len == 0)
      {  print("%s:%d: missing keyword", dsa->fname, dsa->seqn);
         return 1;
      }
      return 0;
}

static int check_colon(struct dsa *dsa)
{     if (skip_spaces(dsa, 0)) return 1;
      if (dsa->c != ':')
      {  print("%s:%d: missing colon after `%s'", dsa->fname,
            dsa->seqn, dsa->token);
         return 1;
      }
      if (get_char(dsa)) return 1;
      return 0;
}

static int scan_token(struct dsa *dsa, int across)
{     int len = 0;
      if (skip_spaces(dsa, across)) return 1;
      dsa->token[0] = '\0';
      while (!(dsa->c == EOF || dsa->c == '\n' || dsa->c == ' '))
      {  if (len == 255)
         {  dsa->token[31] = '\0';
            print("%s:%d: token `%s...' too long", dsa->fname,
               dsa->seqn, dsa->token);
            return 1;
         }
         dsa->token[len++] = (char)dsa->c, dsa->token[len] = '\0';
         if (get_char(dsa)) return 1;
      }
      return 0;
}

static int check_newline(struct dsa *dsa)
{     if (skip_spaces(dsa, 0)) return 1;
      if (!(dsa->c == EOF || dsa->c == '\n'))
      {  print("%s:%d: extra symbols detected", dsa->fname, dsa->seqn);
         return 1;
      }
      if (get_char(dsa)) return 1;
      return 0;
}

static int scan_comment(struct dsa *dsa)
{     int len = 0;
      if (skip_spaces(dsa, 0)) return 1;
      dsa->token[0] = '\0';
      while (!(dsa->c == EOF || dsa->c == '\n'))
      {  if (len == 255)
         {  print("%s:%d: comment too long", dsa->fname, dsa->seqn);
            return 1;
         }
         dsa->token[len++] = (char)dsa->c, dsa->token[len] = '\0';
         if (get_char(dsa)) return 1;
      }
      return 0;
}

static int scan_integer(struct dsa *dsa, int across, int *val)
{     if (scan_token(dsa, across)) return 1;
      if (strlen(dsa->token) == 0)
      {  print("%s:%d: missing integer", dsa->fname, dsa->seqn);
         return 1;
      }
      if (str2int(dsa->token, val))
      {  print("%s:%d: integer `%s' invalid", dsa->fname, dsa->seqn,
            dsa->token);
         return 1;
      }
      return 0;
}

static int scan_number(struct dsa *dsa, int across, double *val)
{     if (scan_token(dsa, across)) return 1;
      if (strlen(dsa->token) == 0)
      {  print("%s:%d: missing number", dsa->fname, dsa->seqn);
         return 1;
      }
      if (str2dbl(dsa->token, val))
      {  print("%s:%d: number `%s' invalid", dsa->fname, dsa->seqn,
            dsa->token);
         return 1;
      }
      return 0;
}

TSP *tsp_read_data(char *fname)
{     struct dsa _dsa, *dsa = &_dsa;
      TSP *tsp = NULL;
      dsa->fname = fname;
      print("tsp_read_data: reading TSP data from `%s'...", dsa->fname);
      dsa->fp = ufopen(dsa->fname, "r");
      if (dsa->fp == NULL)
      {  print("tsp_read_data: unable to open `%s' - %s", dsa->fname,
            strerror(errno));
         goto fail;
      }
      tsp = umalloc(sizeof(TSP));
      tsp->name = NULL;
      tsp->type = TSP_UNDEF;
      tsp->comment = NULL;
      tsp->dimension = 0;
      tsp->edge_weight_type = TSP_UNDEF;
      tsp->edge_weight_format = TSP_UNDEF;
      tsp->display_data_type = TSP_UNDEF;
      tsp->node_x_coord = NULL;
      tsp->node_y_coord = NULL;
      tsp->dply_x_coord = NULL;
      tsp->dply_y_coord = NULL;
      tsp->tour = NULL;
      tsp->edge_weight = NULL;
      dsa->seqn = 1;
      if (get_char(dsa)) goto fail;
loop: if (scan_keyword(dsa)) goto fail;
      if (strcmp(dsa->token, "NAME") == 0)
      {  if (tsp->name != NULL)
         {  print("%s:%d: NAME entry multiply defined", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (check_colon(dsa)) goto fail;
         if (scan_token(dsa, 0)) goto fail;
         if (strlen(dsa->token) == 0)
         {  print("%s:%d: NAME entry incomplete", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         tsp->name = umalloc(strlen(dsa->token) + 1);
         strcpy(tsp->name, dsa->token);
         print("tsp_read_data: NAME: %s", tsp->name);
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "TYPE") == 0)
      {  if (tsp->type != TSP_UNDEF)
         {  print("%s:%d: TYPE entry multiply defined", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (check_colon(dsa)) goto fail;
         if (scan_keyword(dsa)) goto fail;
         if (strcmp(dsa->token, "TSP") == 0)
            tsp->type = TSP_TSP;
         else if (strcmp(dsa->token, "ATSP") == 0)
            tsp->type = TSP_ATSP;
         else if (strcmp(dsa->token, "TOUR") == 0)
            tsp->type = TSP_TOUR;
         else
         {  print("%s:%d: data type `%s' not recognized", dsa->fname,
               dsa->seqn, dsa->token);
            goto fail;
         }
         print("tsp_read_data: TYPE: %s", dsa->token);
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "COMMENT") == 0)
      {  if (tsp->comment != NULL)
         {  print("%s:%d: COMMENT entry multiply defined", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (check_colon(dsa)) goto fail;
         if (scan_comment(dsa)) goto fail;
         tsp->comment = umalloc(strlen(dsa->token) + 1);
         strcpy(tsp->comment, dsa->token);
         print("tsp_read_data: COMMENT: %s", tsp->comment);
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "DIMENSION") == 0)
      {  if (tsp->dimension != 0)
         {  print("%s:%d: DIMENSION entry multiply defined", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (check_colon(dsa)) goto fail;
         if (scan_integer(dsa, 0, &tsp->dimension)) goto fail;
         if (tsp->dimension < 1)
         {  print("%s:%d: invalid dimension", dsa->fname, dsa->seqn);
            goto fail;
         }
         print("tsp_read_data: DIMENSION: %d", tsp->dimension);
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "EDGE_WEIGHT_TYPE") == 0)
      {  if (tsp->edge_weight_type != TSP_UNDEF)
         {  print("%s:%d: EDGE_WEIGHT_TYPE entry multiply defined",
               dsa->fname, dsa->seqn);
            goto fail;
         }
         if (check_colon(dsa)) goto fail;
         if (scan_keyword(dsa)) goto fail;
         if (strcmp(dsa->token, "GEO") == 0)
            tsp->edge_weight_type = TSP_GEO;
         else if (strcmp(dsa->token, "EUC_2D") == 0)
            tsp->edge_weight_type = TSP_EUC_2D;
         else if (strcmp(dsa->token, "ATT") == 0)
            tsp->edge_weight_type = TSP_ATT;
         else if (strcmp(dsa->token, "EXPLICIT") == 0)
            tsp->edge_weight_type = TSP_EXPLICIT;
         else if (strcmp(dsa->token, "CEIL_2D") == 0)
            tsp->edge_weight_type = TSP_CEIL_2D;
         else
         {  print("%s:%d: edge weight type `%s' not recognized",
               dsa->fname, dsa->seqn, dsa->token);
            goto fail;
         }
         print("tsp_read_data: EDGE_WEIGHT_TYPE: %s", dsa->token);
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "EDGE_WEIGHT_FORMAT") == 0)
      {  if (tsp->edge_weight_format != TSP_UNDEF)
         {  print("%s:%d: EDGE_WEIGHT_FORMAT entry multiply defined",
               dsa->fname, dsa->seqn);
            goto fail;
         }
         if (check_colon(dsa)) goto fail;
         if (scan_keyword(dsa)) goto fail;
         if (strcmp(dsa->token, "UPPER_ROW") == 0)
            tsp->edge_weight_format = TSP_UPPER_ROW;
         else if (strcmp(dsa->token, "FULL_MATRIX") == 0)
            tsp->edge_weight_format = TSP_FULL_MATRIX;
         else if (strcmp(dsa->token, "FUNCTION") == 0)
            tsp->edge_weight_format = TSP_FUNCTION;
         else if (strcmp(dsa->token, "LOWER_DIAG_ROW") == 0)
            tsp->edge_weight_format = TSP_LOWER_DIAG_ROW;
         else
         {  print("%s:%d: edge weight format `%s' not recognized",
               dsa->fname, dsa->seqn, dsa->token);
            goto fail;
         }
         print("tsp_read_data: EDGE_WEIGHT_FORMAT: %s", dsa->token);
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "DISPLAY_DATA_TYPE") == 0)
      {  if (tsp->display_data_type != TSP_UNDEF)
         {  print("%s:%d: DISPLAY_DATA_TYPE entry multiply defined",
               dsa->fname, dsa->seqn);
            goto fail;
         }
         if (check_colon(dsa)) goto fail;
         if (scan_keyword(dsa)) goto fail;
         if (strcmp(dsa->token, "COORD_DISPLAY") == 0)
            tsp->display_data_type = TSP_COORD_DISPLAY;
         else if (strcmp(dsa->token, "TWOD_DISPLAY") == 0)
            tsp->display_data_type = TSP_TWOD_DISPLAY;
         else
         {  print("%s:%d: display data type `%s' not recognized",
               dsa->fname, dsa->seqn, dsa->token);
            goto fail;
         }
         print("tsp_read_data: DISPLAY_DATA_TYPE: %s", dsa->token);
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "NODE_COORD_SECTION") == 0)
      {  int n = tsp->dimension, k, node;
         if (n == 0)
         {  print("%s:%d: DIMENSION entry not specified", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (tsp->node_x_coord != NULL)
         {  print("%s:%d: NODE_COORD_SECTION multiply specified",
               dsa->fname, dsa->seqn);
            goto fail;
         }
         if (check_newline(dsa)) goto fail;
         tsp->node_x_coord = ucalloc(1+n, sizeof(double));
         tsp->node_y_coord = ucalloc(1+n, sizeof(double));
         for (node = 1; node <= n; node++)
            tsp->node_x_coord[node] = tsp->node_y_coord[node] = DBL_MAX;
         for (k = 1; k <= n; k++)
         {  if (scan_integer(dsa, 0, &node)) goto fail;
            if (!(1 <= node && node <= n))
            {  print("%s:%d: invalid node number %d", dsa->fname,
                  dsa->seqn, node);
               goto fail;
            }
            if (tsp->node_x_coord[node] != DBL_MAX)
            {  print("%s:%d: node number %d multiply specified",
                  dsa->fname, dsa->seqn, node);
               goto fail;
            }
            if (scan_number(dsa, 0, &tsp->node_x_coord[node]))
               goto fail;
            if (scan_number(dsa, 0, &tsp->node_y_coord[node]))
               goto fail;
            if (check_newline(dsa)) goto fail;
         }
      }
      else if (strcmp(dsa->token, "DISPLAY_DATA_SECTION") == 0)
      {  int n = tsp->dimension, k, node;
         if (n == 0)
         {  print("%s:%d: DIMENSION entry not specified", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (tsp->dply_x_coord != NULL)
         {  print("%s:%d: DISPLAY_DATA_SECTION multiply specified",
               dsa->fname, dsa->seqn);
            goto fail;
         }
         if (check_newline(dsa)) goto fail;
         tsp->dply_x_coord = ucalloc(1+n, sizeof(double));
         tsp->dply_y_coord = ucalloc(1+n, sizeof(double));
         for (node = 1; node <= n; node++)
            tsp->dply_x_coord[node] = tsp->dply_y_coord[node] = DBL_MAX;
         for (k = 1; k <= n; k++)
         {  if (scan_integer(dsa, 0, &node)) goto fail;
            if (!(1 <= node && node <= n))
            {  print("%s:%d: invalid node number %d", dsa->fname,
                  dsa->seqn, node);
               goto fail;
            }
            if (tsp->dply_x_coord[node] != DBL_MAX)
            {  print("%s:%d: node number %d multiply specified",
                  dsa->fname, dsa->seqn, node);
               goto fail;
            }
            if (scan_number(dsa, 0, &tsp->dply_x_coord[node]))
               goto fail;
            if (scan_number(dsa, 0, &tsp->dply_y_coord[node]))
               goto fail;
            if (check_newline(dsa)) goto fail;
         }
      }
      else if (strcmp(dsa->token, "TOUR_SECTION") == 0)
      {  int n = tsp->dimension, k, node;
         if (n == 0)
         {  print("%s:%d: DIMENSION entry not specified", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (tsp->tour != NULL)
         {  print("%s:%d: TOUR_SECTION multiply specified", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (check_newline(dsa)) goto fail;
         tsp->tour = ucalloc(1+n, sizeof(int));
         for (k = 1; k <= n; k++)
         {  if (scan_integer(dsa, 1, &node)) goto fail;
            if (!(1 <= node && node <= n))
            {  print("%s:%d: invalid node number %d", dsa->fname,
                  dsa->seqn, node);
               goto fail;
            }
            tsp->tour[k] = node;
         }
         if (scan_integer(dsa, 1, &node)) goto fail;
         if (node != -1)
         {  print("%s:%d: extra node(s) detected", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "EDGE_WEIGHT_SECTION") == 0)
      {  int n = tsp->dimension, i, j, temp;
         if (n == 0)
         {  print("%s:%d: DIMENSION entry not specified", dsa->fname,
               dsa->seqn);
            goto fail;
         }
         if (tsp->edge_weight_format == TSP_UNDEF)
         {  print("%s:%d: EDGE_WEIGHT_FORMAT entry not specified",
               dsa->fname, dsa->seqn);
            goto fail;
         }
         if (tsp->edge_weight != NULL)
         {  print("%s:%d: EDGE_WEIGHT_SECTION multiply specified",
               dsa->fname, dsa->seqn);
            goto fail;
         }
         if (check_newline(dsa)) goto fail;
         tsp->edge_weight = ucalloc(1+n*n, sizeof(int));
         switch (tsp->edge_weight_format)
         {  case TSP_FULL_MATRIX:
               for (i = 1; i <= n; i++)
               {  for (j = 1; j <= n; j++)
                  {  if (scan_integer(dsa, 1, &temp)) goto fail;
                     tsp->edge_weight[(i - 1) * n + j] = temp;
                  }
               }
               break;
            case TSP_UPPER_ROW:
               for (i = 1; i <= n; i++)
               {  tsp->edge_weight[(i - 1) * n + i] = 0;
                  for (j = i + 1; j <= n; j++)
                  {  if (scan_integer(dsa, 1, &temp)) goto fail;
                     tsp->edge_weight[(i - 1) * n + j] = temp;
                     tsp->edge_weight[(j - 1) * n + i] = temp;
                  }
               }
               break;
            case TSP_LOWER_DIAG_ROW:
               for (i = 1; i <= n; i++)
               {  for (j = 1; j <= i; j++)
                  {  if (scan_integer(dsa, 1, &temp)) goto fail;
                     tsp->edge_weight[(i - 1) * n + j] = temp;
                     tsp->edge_weight[(j - 1) * n + i] = temp;
                  }
               }
               break;
            default:
               goto fail;
         }
         if (check_newline(dsa)) goto fail;
      }
      else if (strcmp(dsa->token, "EOF") == 0)
      {  if (check_newline(dsa)) goto fail;
         goto done;
      }
      else
      {  print("%s:%d: keyword `%s' not recognized", dsa->fname,
            dsa->seqn, dsa->token);
         goto fail;
      }
      goto loop;
done: print("tsp_read_data: %d lines were read", dsa->seqn - 1);
      ufclose(dsa->fp);
      return tsp;
fail: if (tsp != NULL)
      {  if (tsp->name != NULL) ufree(tsp->name);
         if (tsp->comment != NULL) ufree(tsp->comment);
         if (tsp->node_x_coord != NULL) ufree(tsp->node_x_coord);
         if (tsp->node_y_coord != NULL) ufree(tsp->node_y_coord);
         if (tsp->dply_x_coord != NULL) ufree(tsp->dply_x_coord);
         if (tsp->dply_y_coord != NULL) ufree(tsp->dply_y_coord);
         if (tsp->tour != NULL) ufree(tsp->tour);
         if (tsp->edge_weight != NULL) ufree(tsp->edge_weight);
         ufree(tsp);
      }
      if (dsa->fp != NULL) fclose(dsa->fp);
      return NULL;
}

/*----------------------------------------------------------------------
-- tsp_free_data - free TSP instance data.
--
-- *Synopsis*
--
-- #include "glptsp.h"
-- void tsp_free_data(TSP *tsp);
--
-- *Description*
--
-- The routine tsp_free_data frees all the memory allocated to the TSP
-- instance data block, which the parameter tsp points to. */

void tsp_free_data(TSP *tsp)
{     if (tsp->name != NULL) ufree(tsp->name);
      if (tsp->comment != NULL) ufree(tsp->comment);
      if (tsp->node_x_coord != NULL) ufree(tsp->node_x_coord);
      if (tsp->node_y_coord != NULL) ufree(tsp->node_y_coord);
      if (tsp->dply_x_coord != NULL) ufree(tsp->dply_x_coord);
      if (tsp->dply_y_coord != NULL) ufree(tsp->dply_y_coord);
      if (tsp->tour != NULL) ufree(tsp->tour);
      if (tsp->edge_weight != NULL) ufree(tsp->edge_weight);
      ufree(tsp);
      return;
}

/*----------------------------------------------------------------------
-- tsp_distance - compute distance between two nodes.
--
-- *Synopsis*
--
-- #include "glptsp.h"
-- int tsp_distance(TSP *tsp, int i, int j);
--
-- *Description*
--
-- The routine tsp_distance computes the distance between i-th and j-th
-- nodes for the TSP instance, which tsp points to.
--
-- *Returns*
--
-- The routine tsp_distance returns the computed distance. */

#define nint(x) ((int)((x) + 0.5))

static double rad(double x)
{     /* convert input coordinate to longitude/latitude, in radians */
      double pi = 3.141592, deg, min;
      deg = (int)x;
      min = x - deg;
      return pi * (deg + 5.0 * min / 3.0) / 180.0;
}

int tsp_distance(TSP *tsp, int i, int j)
{     int n = tsp->dimension, dij;
      if (!(tsp->type == TSP_TSP || tsp->type == TSP_ATSP))
         fault("tsp_distance: invalid TSP instance");
      if (!(1 <= i && i <= n && 1 <= j && j <= n))
         fault("tsp_distance: node number out of range");
      switch (tsp->edge_weight_type)
      {  case TSP_UNDEF:
            fault("tsp_distance: edge weight type not specified");
         case TSP_EXPLICIT:
            if (tsp->edge_weight == NULL)
               fault("tsp_distance: edge weights not specified");
            dij = tsp->edge_weight[(i - 1) * n + j];
            break;
         case TSP_EUC_2D:
            if (tsp->node_x_coord == NULL || tsp->node_y_coord == NULL)
               fault("tsp_distance: node coordinates not specified");
            {  double xd, yd;
               xd = tsp->node_x_coord[i] - tsp->node_x_coord[j];
               yd = tsp->node_y_coord[i] - tsp->node_y_coord[j];
               dij = nint(sqrt(xd * xd + yd * yd));
            }
            break;
         case TSP_CEIL_2D:
            if (tsp->node_x_coord == NULL || tsp->node_y_coord == NULL)
               fault("tsp_distance: node coordinates not specified");
            {  double xd, yd;
               xd = tsp->node_x_coord[i] - tsp->node_x_coord[j];
               yd = tsp->node_y_coord[i] - tsp->node_y_coord[j];
               dij = (int)ceil(sqrt(xd * xd + yd * yd));
            }
            break;
         case TSP_GEO:
            if (tsp->node_x_coord == NULL || tsp->node_y_coord == NULL)
               fault("tsp_distance: node coordinates not specified");
            {  double rrr = 6378.388;
               double latitude_i = rad(tsp->node_x_coord[i]);
               double latitude_j = rad(tsp->node_x_coord[j]);
               double longitude_i = rad(tsp->node_y_coord[i]);
               double longitude_j = rad(tsp->node_y_coord[j]);
               double q1 = cos(longitude_i - longitude_j);
               double q2 = cos(latitude_i - latitude_j);
               double q3 = cos(latitude_i + latitude_j);
               dij = (int)(rrr * acos(0.5 * ((1.0 + q1) * q2 -
                  (1.0 - q1) *q3)) + 1.0);
            }
            break;
         case TSP_ATT:
            if (tsp->node_x_coord == NULL || tsp->node_y_coord == NULL)
               fault("tsp_distance: node coordinates not specified");
            {  int tij;
               double xd, yd, rij;
               xd = tsp->node_x_coord[i] - tsp->node_x_coord[j];
               yd = tsp->node_y_coord[i] - tsp->node_y_coord[j];
               rij = sqrt((xd * xd + yd * yd) / 10.0);
               tij = nint(rij);
               if (tij < rij) dij = tij + 1; else dij = tij;
            }
            break;
         default:
            insist(tsp->edge_weight_type != tsp->edge_weight_type);
      }
      return dij;
}

/* eof */
