/* glphbsm.c */

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
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "glphbsm.h"
#include "glplib.h"
#include "glpstr.h"

/*----------------------------------------------------------------------
-- load_hbsm - load sparse matrix in Harwell-Boeing format.
--
-- *Synopsis*
--
-- #include "glphbsm.h"
-- HBSM *load_hbsm(char *fname);
--
-- *Description*
--
-- The load_hbsm routine loads a sparse matrix in the Harwell-Boeing
-- format from the text file whose name is the character string fname.
--
-- Detailed description of the Harwell-Boeing format recognised by this
-- routine is given in the following report:
--
-- I.S.Duff, R.G.Grimes, J.G.Lewis. User's Guide for the Harwell-Boeing
-- Sparse Matrix Collection (Release I), TR/PA/92/86, October 1992.
--
-- *Returns*
--
-- The load_hbsm routine returns a pointer to the object of HBSM type
-- that represents the loaded matrix in the Harwell-Boeing format. In
-- case of error the routine prints an appropriate error message and
-- returns NULL. */

static char *fname;
/* name of the input text file */

static FILE *fp;
/* stream assigned to the input text file */

static int seqn;
/* card sequential number */

static char card[80+1];
/* card image buffer */

static int read_card(void);
/* read the next card */

static int scan_int(char *fld, int pos, int width, int *val);
/* scan integer value from the current card */

static int parse_fmt(char *fmt);
/* parse Fortran format specification */

static int read_int_array(char *name, char *fmt, int n, int val[]);
/* read array of integer type */

static int read_real_array(char *name, char *fmt, int n, double val[]);
/* read array of real type */

HBSM *load_hbsm(char *_fname)
{     HBSM *hbsm = NULL;
      /* initialization */
      fname = _fname;
      print("load_hbsm: reading matrix from `%s'...", fname);
      fp = fopen(fname, "r");
      if (fp == NULL)
      {  print("load_hbsm: unable to open `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      seqn = 0;
      hbsm = umalloc(sizeof(HBSM));
      memset(hbsm, 0, sizeof(HBSM));
      /* read the first heading card */
      if (read_card()) goto fail;
      memcpy(hbsm->title, card, 72), hbsm->title[72] = '\0';
      strtrim(hbsm->title);
      print("%s", hbsm->title);
      memcpy(hbsm->key, card+72, 8), hbsm->key[8] = '\0';
      strspx(hbsm->key);
      print("key = `%s'", hbsm->key);
      /* read the second heading card */
      if (read_card()) goto fail;
      if (scan_int("totcrd",  0, 14, &hbsm->totcrd)) goto fail;
      if (scan_int("ptrcrd", 14, 14, &hbsm->ptrcrd)) goto fail;
      if (scan_int("indcrd", 28, 14, &hbsm->indcrd)) goto fail;
      if (scan_int("valcrd", 42, 14, &hbsm->valcrd)) goto fail;
      if (scan_int("rhscrd", 56, 14, &hbsm->rhscrd)) goto fail;
      print("totcrd = %d; ptrcrd = %d; indcrd = %d; valcrd = %d; rhscrd"
         " = %d", hbsm->totcrd, hbsm->ptrcrd, hbsm->indcrd,
         hbsm->valcrd, hbsm->rhscrd);
      /* read the third heading card */
      if (read_card()) goto fail;
      memcpy(hbsm->mxtype, card, 3), hbsm->mxtype[3] = '\0';
      if (strchr("RCP",   hbsm->mxtype[0]) == NULL ||
          strchr("SUHZR", hbsm->mxtype[1]) == NULL ||
          strchr("AE",    hbsm->mxtype[2]) == NULL)
      {  print("%s:%d: matrix type `%s' not recognised", fname, seqn,
            hbsm->mxtype);
         goto fail;
      }
      if (scan_int("nrow", 14, 14, &hbsm->nrow)) goto fail;
      if (scan_int("ncol", 28, 14, &hbsm->ncol)) goto fail;
      if (scan_int("nnzero", 42, 14, &hbsm->nnzero)) goto fail;
      if (scan_int("neltvl", 56, 14, &hbsm->neltvl)) goto fail;
      print("mxtype = `%s'; nrow = %d; ncol = %d; nnzero = %d; neltvl ="
         " %d", hbsm->mxtype, hbsm->nrow, hbsm->ncol, hbsm->nnzero,
         hbsm->neltvl);
      /* read the fourth heading card */
      if (read_card()) goto fail;
      memcpy(hbsm->ptrfmt, card, 16), hbsm->ptrfmt[16] = '\0';
      strspx(hbsm->ptrfmt);
      memcpy(hbsm->indfmt, card+16, 16), hbsm->indfmt[16] = '\0';
      strspx(hbsm->indfmt);
      memcpy(hbsm->valfmt, card+32, 20), hbsm->valfmt[20] = '\0';
      strspx(hbsm->valfmt);
      memcpy(hbsm->rhsfmt, card+52, 20), hbsm->rhsfmt[20] = '\0';
      strspx(hbsm->rhsfmt);
      print("ptrfmt = %s; indfmt = %s; valfmt = %s; rhsfmt = %s",
         hbsm->ptrfmt, hbsm->indfmt, hbsm->valfmt, hbsm->rhsfmt);
      /* read the fifth heading card (optional) */
      if (hbsm->rhscrd <= 0)
      {  strcpy(hbsm->rhstyp, "???");
         hbsm->nrhs = 0;
         hbsm->nrhsix = 0;
      }
      else
      {  if (read_card()) goto fail;
         memcpy(hbsm->rhstyp, card, 3), hbsm->rhstyp[3] = '\0';
         if (scan_int("nrhs", 14, 14, &hbsm->nrhs)) goto fail;
         if (scan_int("nrhsix", 28, 14, &hbsm->nrhsix)) goto fail;
         print("rhstyp = `%s'; nrhs = %d; nrhsix = %d",
            hbsm->rhstyp, hbsm->nrhs, hbsm->nrhsix);
      }
      /* read matrix structure */
      hbsm->colptr = ucalloc(1+hbsm->ncol+1, sizeof(int));
      if (read_int_array("colptr", hbsm->ptrfmt, hbsm->ncol+1,
         hbsm->colptr)) goto fail;
      hbsm->rowind = ucalloc(1+hbsm->nnzero, sizeof(int));
      if (read_int_array("rowind", hbsm->indfmt, hbsm->nnzero,
         hbsm->rowind)) goto fail;
      /* read matrix values */
      if (hbsm->valcrd <= 0) goto done;
      if (hbsm->mxtype[2] == 'A')
      {  /* assembled matrix */
         hbsm->values = ucalloc(1+hbsm->nnzero, sizeof(double));
         if (read_real_array("values", hbsm->valfmt, hbsm->nnzero,
            hbsm->values)) goto fail;
      }
      else
      {  /* elemental (unassembled) matrix */
         hbsm->values = ucalloc(1+hbsm->neltvl, sizeof(double));
         if (read_real_array("values", hbsm->valfmt, hbsm->neltvl,
            hbsm->values)) goto fail;
      }
      /* read right-hand sides */
      if (hbsm->nrhs <= 0) goto done;
      if (hbsm->rhstyp[0] == 'F')
      {  /* dense format */
         hbsm->nrhsvl = hbsm->nrow * hbsm->nrhs;
         hbsm->rhsval = ucalloc(1+hbsm->nrhsvl, sizeof(double));
         if (read_real_array("rhsval", hbsm->rhsfmt, hbsm->nrhsvl,
            hbsm->rhsval)) goto fail;
      }
      else if (hbsm->rhstyp[0] == 'M' && hbsm->mxtype[2] == 'A')
      {  /* sparse format */
         /* read pointers */
         hbsm->rhsptr = ucalloc(1+hbsm->nrhs+1, sizeof(int));
         if (read_int_array("rhsptr", hbsm->ptrfmt, hbsm->nrhs+1,
            hbsm->rhsptr)) goto fail;
         /* read sparsity pattern */
         hbsm->rhsind = ucalloc(1+hbsm->nrhsix, sizeof(int));
         if (read_int_array("rhsind", hbsm->indfmt, hbsm->nrhsix,
            hbsm->rhsind)) goto fail;
         /* read values */
         hbsm->rhsval = ucalloc(1+hbsm->nrhsix, sizeof(double));
         if (read_real_array("rhsval", hbsm->rhsfmt, hbsm->nrhsix,
            hbsm->rhsval)) goto fail;
      }
      else if (hbsm->rhstyp[0] == 'M' && hbsm->mxtype[2] == 'E')
      {  /* elemental format */
         hbsm->rhsval = ucalloc(1+hbsm->nrhsvl, sizeof(double));
         if (read_real_array("rhsval", hbsm->rhsfmt, hbsm->nrhsvl,
            hbsm->rhsval)) goto fail;
      }
      else
      {  print("%s:%d: right-hand side type `%c' not recognised",
            fname, seqn, hbsm->rhstyp[0]);
         goto fail;
      }
      /* read starting guesses */
      if (hbsm->rhstyp[1] == 'G')
      {  hbsm->nguess = hbsm->nrow * hbsm->nrhs;
         hbsm->sguess = ucalloc(1+hbsm->nguess, sizeof(double));
         if (read_real_array("sguess", hbsm->rhsfmt, hbsm->nguess,
            hbsm->sguess)) goto fail;
      }
      /* read solution vectors */
      if (hbsm->rhstyp[2] == 'X')
      {  hbsm->nexact = hbsm->nrow * hbsm->nrhs;
         hbsm->xexact = ucalloc(1+hbsm->nexact, sizeof(double));
         if (read_real_array("xexact", hbsm->rhsfmt, hbsm->nexact,
            hbsm->xexact)) goto fail;
      }
done: /* loading has been completed */
      print("load_hbsm: %d cards were read", seqn);
      return hbsm;
fail: /* something wrong in Danish kingdom */
      if (hbsm != NULL)
      {  if (hbsm->colptr != NULL) ufree(hbsm->colptr);
         if (hbsm->rowind != NULL) ufree(hbsm->rowind);
         if (hbsm->rhsptr != NULL) ufree(hbsm->rhsptr);
         if (hbsm->rhsind != NULL) ufree(hbsm->rhsind);
         if (hbsm->values != NULL) ufree(hbsm->values);
         if (hbsm->rhsval != NULL) ufree(hbsm->rhsval);
         if (hbsm->sguess != NULL) ufree(hbsm->sguess);
         if (hbsm->xexact != NULL) ufree(hbsm->xexact);
         ufree(hbsm);
      }
      if (fp != NULL) fclose(fp);
      return NULL;
}

/*----------------------------------------------------------------------
-- read_card - read the next card.
--
-- This routine reads the next 80-column card from the input text file
-- and places its image into the character string card. If the card was
-- read successfully, the routine returns zero, otherwise non-zero. */

static int read_card(void)
{     int k, c;
      seqn++;
      memset(card, ' ', 80), card[80] = '\0';
      k = 0;
      for (;;)
      {  c = fgetc(fp);
         if (ferror(fp))
         {  print("%s:%d: read error - %s", fname, seqn,
               strerror(errno));
            return 1;
         }
         if (feof(fp))
         {  if (k == 0)
               print("%s:%d: unexpected eof", fname, seqn);
            else
               print("%s:%d: missing final LF", fname, seqn);
            return 1;
         }
         if (c == '\r') continue;
         if (c == '\n') break;
         if (iscntrl(c))
         {  print("%s:%d: invalid control character 0x%02X", fname,
               seqn, c);
            return 1;
         }
         if (k == 80)
         {  print("%s:%d: card image too long", fname, seqn);
            return 1;
         }
         card[k++] = (char)c;
      }
      return 0;
}

/*----------------------------------------------------------------------
-- scan_int - scan integer value from the current card.
--
-- This routine scans an integer value from the current card, where fld
-- is the name of the field, pos is the position of the field, width is
-- the width of the field, val points to a location to which the scanned
-- value should be stored. If the value was scanned successfully, the
-- routine returns zero, otherwise non-zero. */

static int scan_int(char *fld, int pos, int width, int *val)
{     char str[80+1];
      insist(1 <= width && width <= 80);
      memcpy(str, card+pos, width), str[width] = '\0';
      if (str2int(strspx(str), val))
      {  print("%s:%d: field `%s' contains invalid value `%s'",
            fname, seqn, fld, str);
         return 1;
      }
      return 0;
}

/*----------------------------------------------------------------------
-- parse_fmt - parse Fortran format specification.
--
-- This routine parses the Fortran format specification represented as
-- character string which fmt points to and stores format elements into
-- appropriate static locations. Should note that not all valid Fortran
-- format specifications may be recognised. If the format specification
-- was recognised, the routine returns zero, otherwise non-zero. */

static int fmt_p; /* scale factor */
static int fmt_k; /* iterator */
static int fmt_f; /* format code */
static int fmt_w; /* field width */
static int fmt_d; /* number of places after point */

static int parse_fmt(char *fmt)
{     int k, s, val;
      char str[80+1];
      /* first character should be left parenthesis */
      if (fmt[0] != '(')
fail: {  print("load_hbsm: format `%s' not recognised", fmt);
         return 1;
      }
      k = 1;
      /* optional scale factor */
      fmt_p = 0;
      if (isdigit((unsigned char)fmt[k]))
      {  s = 0;
         while (isdigit((unsigned char)fmt[k]))
         {  if (s == 80) goto fail;
            str[s++] = fmt[k++];
         }
         str[s] = '\0';
         if (str2int(str, &val)) goto fail;
         if (toupper((unsigned char)fmt[k]) != 'P') goto iter;
         fmt_p = val, k++;
         if (!(0 <= fmt_p && fmt_p <= 255)) goto fail;
         /* optional comma may follow scale factor */
         if (fmt[k] == ',') k++;
      }
      /* optional iterator */
      fmt_k = 1;
      if (isdigit((unsigned char)fmt[k]))
      {  s = 0;
         while (isdigit((unsigned char)fmt[k]))
         {  if (s == 80) goto fail;
            str[s++] = fmt[k++];
         }
         str[s] = '\0';
         if (str2int(str, &val)) goto fail;
iter:    fmt_k = val;
         if (!(1 <= fmt_k && fmt_k <= 255)) goto fail;
      }
      /* format code */
      fmt_f = toupper((unsigned char)fmt[k++]);
      if (!(fmt_f == 'D' || fmt_f == 'E' || fmt_f == 'F' ||
            fmt_f == 'G' || fmt_f == 'I')) goto fail;
      /* field width */
      if (!isdigit((unsigned char)fmt[k])) goto fail;
      s = 0;
      while (isdigit((unsigned char)fmt[k]))
      {  if (s == 80) goto fail;
         str[s++] = fmt[k++];
      }
      str[s] = '\0';
      if (str2int(str, &fmt_w)) goto fail;
      if (!(1 <= fmt_w && fmt_w <= 255)) goto fail;
      /* optional point number of places after point */
      fmt_d = 0;
      if (fmt[k] == '.')
      {  k++;
         if (!isdigit((unsigned char)fmt[k])) goto fail;
         s = 0;
         while (isdigit((unsigned char)fmt[k]))
         {  if (s == 80) goto fail;
            str[s++] = fmt[k++];
         }
         str[s] = '\0';
         if (str2int(str, &fmt_d)) goto fail;
         if (!(0 <= fmt_d && fmt_d <= 255)) goto fail;
      }
      /* last character should be right parenthesis */
      if (!(fmt[k] == ')' && fmt[k+1] == '\0')) goto fail;
      return 0;
}

/*----------------------------------------------------------------------
-- read_int_array - read array of integer type.
--
-- This routine reads an integer array from the input text file, where
-- name is array name, fmt is Fortran format specification that controls
-- reading, n is number of array elements, val is array of integer type.
-- If the array was read successful, the routine returns zero, otherwise
-- non-zero. */

static int read_int_array(char *name, char *fmt, int n, int val[])
{     int k, pos;
      char str[80+1];
      if (parse_fmt(fmt)) return 1;
      if (!(fmt_f == 'I' && fmt_w <= 80 && fmt_k * fmt_w <= 80))
      {  print("%s:%d: can't read array `%s' - invalid format `%s'",
            fname, seqn, name, fmt);
         return 1;
      }
      for (k = 1, pos = INT_MAX; k <= n; k++, pos++)
      {  if (pos >= fmt_k)
         {  if (read_card()) return 1;
            pos = 0;
         }
         memcpy(str, card + fmt_w * pos, fmt_w), str[fmt_w] = '\0';
         strspx(str);
         if (str2int(str, &val[k]))
         {  print("%s:%d: can't read array `%s' - invalid value `%s'",
               fname, seqn, name, str);
            return 1;
         }
      }
      return 0;
}

/*----------------------------------------------------------------------
-- read_real_array - read array of real type.
--
-- This routine reads a real array from the input text file, where name
-- is array name, fmt is Fortran format specification that controls
-- reading, n is number of array elements, val is array of real type.
-- If the array was read successful, the routine returns zero, otherwise
-- non-zero. */

static int read_real_array(char *name, char *fmt, int n, double val[])
{     int k, pos;
      char str[80+1], *ptr;
      if (parse_fmt(fmt)) return 1;
      if (!(fmt_f != 'I' && fmt_w <= 80 && fmt_k * fmt_w <= 80))
      {  print("%s:%d: can't read array `%s' - invalid format `%s'",
            fname, seqn, name, fmt);
         return 1;
      }
      for (k = 1, pos = INT_MAX; k <= n; k++, pos++)
      {  if (pos >= fmt_k)
         {  if (read_card()) return 1;
            pos = 0;
         }
         memcpy(str, card + fmt_w * pos, fmt_w), str[fmt_w] = '\0';
         strspx(str);
         if (strchr(str, '.') == NULL && strcmp(str, "0"))
         {  print("%s(%d): can't read array `%s' - value `%s' has no de"
               "cimal point", fname, seqn, name, str);
            return 1;
         }
         /* sometimes lower case letters appear */
         for (ptr = str; *ptr; ptr++)
            *ptr = (char)toupper((unsigned char)*ptr);
         ptr = strchr(str, 'D');
         if (ptr != NULL) *ptr = 'E';
         /* value may appear with decimal exponent but without letters
            E or D (for example, -123.456-012), so missing letter should
            be inserted */
         ptr = strchr(str+1, '+');
         if (ptr == NULL) ptr = strchr(str+1, '-');
         if (ptr != NULL && *(ptr-1) != 'E')
         {  insist(strlen(str) < 80);
            memmove(ptr+1, ptr, strlen(ptr)+1);
            *ptr = 'E';
         }
         if (str2dbl(str, &val[k]))
         {  print("%s:%d: can't read array `%s' - invalid value `%s'",
               fname, seqn, name, str);
            return 1;
         }
      }
      return 0;
}

/*----------------------------------------------------------------------
-- free_hbsm - free sparse matrix in Harwell-Boeing format.
--
-- *Synopsis*
--
-- #include "glphbsm.h"
-- void free_hbsm(HBSM *hbsm);
--
-- *Description*
--
-- The free_hbsm routine frees all memory allocated to the object of
-- HBSM type that represents sparse matrix in Harwell-Boeing format. */

void free_hbsm(HBSM *hbsm)
{     if (hbsm->colptr != NULL) ufree(hbsm->colptr);
      if (hbsm->rowind != NULL) ufree(hbsm->rowind);
      if (hbsm->rhsptr != NULL) ufree(hbsm->rhsptr);
      if (hbsm->rhsind != NULL) ufree(hbsm->rhsind);
      if (hbsm->values != NULL) ufree(hbsm->values);
      if (hbsm->rhsval != NULL) ufree(hbsm->rhsval);
      if (hbsm->sguess != NULL) ufree(hbsm->sguess);
      if (hbsm->xexact != NULL) ufree(hbsm->xexact);
      ufree(hbsm);
      return;
}

/*----------------------------------------------------------------------
-- hbsm_to_mat - convert sparse matrix from HBSM to MAT.
--
-- *Synopsis*
--
-- #include "glphbsm.h"
-- MAT *hbsm_to_mat(HBSM *hbsm);
--
-- *Description*
--
-- The hbsm_to_mat routine converts the sparse matrix from the object of
-- HBSM type (Harwell-Boeing format) to the object of MAT type (standard
-- format).
--
-- In case of symmetric (or skew symmetric) matrices the hbsm_to_mat
-- routine forms both lower and upper traingles of the result matrices.
-- In case of matrices that have pattern only the hbsm_to_mat routine
-- set all non-zero elements of the result matrices to 1.
--
-- This version of the hbsm_to_mat routine can't convert complex and
-- elemental (unassembled) matrices.
--
-- *Returns*
--
-- If the conversion was successful the hbsm_to_mat routine returns a
-- pointer to an object of MAT type that presents the result matrix in
-- standard format. Otherwise the routine returns NULL. */

MAT *hbsm_to_mat(HBSM *hbsm)
{     MAT *A = NULL;
      int i, j, k;
      double val;
      if (hbsm->nrow < 1)
      {  print("hbsm_to_mat: invalid number of rows");
         goto fail;
      }
      if (hbsm->ncol < 1)
      {  print("hbsm_to_mat: invalid number of columns");
         goto fail;
      }
      if (strcmp(hbsm->mxtype, "RSA") == 0)
      {  /* real symmetric assembled */
         if (hbsm->nrow != hbsm->ncol)
err1:    {  print("hbsm_to_mat: number of rows not equal to number of c"
               "olumns");
            goto fail;
         }
         if (hbsm->colptr == NULL)
err2:    {  print("hbsm_to_mat: array `colptr' not allocated");
            goto fail;
         }
         if (hbsm->rowind == NULL)
err3:    {  print("hbsm_to_mat: array `rowind' not allocated");
            goto fail;
         }
         if (hbsm->values == NULL)
err4:    {  print("hbsm_to_mat: array `values' not allocated");
            goto fail;
         }
         A = create_mat(hbsm->nrow, hbsm->ncol);
         for (j = 1; j <= hbsm->ncol; j++)
         for (k = hbsm->colptr[j]; k < hbsm->colptr[j+1]; k++)
         {  if (!(1 <= k && k <= hbsm->nnzero))
err5:       {  print("hbsm_to_mat: invalid column pointer");
               goto fail;
            }
            i = hbsm->rowind[k];
            if (!(1 <= i && i <= hbsm->nrow))
err6:       {  print("hbsm_to_mat: invalid row index");
               goto fail;
            }
            if (i < j)
err7:       {  print("hbsm_to_mat: invalid matrix structure");
               goto fail;
            }
            val = hbsm->values[k];
            if (val != 0.0)
            {  new_elem(A, i, j, val);
               if (i != j) new_elem(A, j, i, val);
            }
         }
      }
      else if (strcmp(hbsm->mxtype, "RUA") == 0)
      {  /* real unsymmetric assembled */
         if (hbsm->nrow != hbsm->ncol) goto err1;
         if (hbsm->colptr == NULL) goto err2;
         if (hbsm->rowind == NULL) goto err3;
         if (hbsm->values == NULL) goto err4;
         A = create_mat(hbsm->nrow, hbsm->ncol);
         for (j = 1; j <= hbsm->ncol; j++)
         for (k = hbsm->colptr[j]; k < hbsm->colptr[j+1]; k++)
         {  if (!(1 <= k && k <= hbsm->nnzero)) goto err5;
            i = hbsm->rowind[k];
            if (!(1 <= i && i <= hbsm->nrow)) goto err6;
            val = hbsm->values[k];
            if (val != 0.0) new_elem(A, i, j, val);
         }
      }
      else if (strcmp(hbsm->mxtype, "RZA") == 0)
      {  /* real skew symmetric assembled */
         if (hbsm->nrow != hbsm->ncol) goto err1;
         if (hbsm->colptr == NULL) goto err2;
         if (hbsm->rowind == NULL) goto err3;
         if (hbsm->values == NULL) goto err4;
         A = create_mat(hbsm->nrow, hbsm->ncol);
         for (j = 1; j <= hbsm->ncol; j++)
         for (k = hbsm->colptr[j]; k < hbsm->colptr[j+1]; k++)
         {  if (!(1 <= k && k <= hbsm->nnzero)) goto err5;
            i = hbsm->rowind[k];
            if (!(1 <= i && i <= hbsm->nrow)) goto err6;
            if (i <= j) goto err7;
            val = hbsm->values[k];
            if (val != 0.0)
            {  new_elem(A, i, j, +val);
               new_elem(A, j, i, -val);
            }
         }
      }
      else if (strcmp(hbsm->mxtype, "RRA") == 0)
      {  /* real rectangular assembled */
         if (hbsm->colptr == NULL) goto err2;
         if (hbsm->rowind == NULL) goto err3;
         if (hbsm->values == NULL) goto err4;
         A = create_mat(hbsm->nrow, hbsm->ncol);
         for (j = 1; j <= hbsm->ncol; j++)
         for (k = hbsm->colptr[j]; k < hbsm->colptr[j+1]; k++)
         {  if (!(1 <= k && k <= hbsm->nnzero)) goto err5;
            i = hbsm->rowind[k];
            if (!(1 <= i && i <= hbsm->nrow)) goto err6;
            val = hbsm->values[k];
            if (val != 0.0) new_elem(A, i, j, val);
         }
      }
      else if (strcmp(hbsm->mxtype, "PSA") == 0)
      {  /* pattern symmetric assembled */
         if (hbsm->nrow != hbsm->ncol) goto err1;
         if (hbsm->colptr == NULL) goto err2;
         if (hbsm->rowind == NULL) goto err3;
         A = create_mat(hbsm->nrow, hbsm->ncol);
         for (j = 1; j <= hbsm->ncol; j++)
         for (k = hbsm->colptr[j]; k < hbsm->colptr[j+1]; k++)
         {  if (!(1 <= k && k <= hbsm->nnzero)) goto err5;
            i = hbsm->rowind[k];
            if (!(1 <= i && i <= hbsm->nrow)) goto err6;
            if (i < j) goto err7;
            val = 1.0;
            new_elem(A, i, j, val);
            if (i != j) new_elem(A, j, i, val);
         }
      }
      else if (strcmp(hbsm->mxtype, "PUA") == 0)
      {  /* pattern unsymmetric assembled */
         if (hbsm->nrow != hbsm->ncol) goto err1;
         if (hbsm->colptr == NULL) goto err2;
         if (hbsm->rowind == NULL) goto err3;
         A = create_mat(hbsm->nrow, hbsm->ncol);
         for (j = 1; j <= hbsm->ncol; j++)
         for (k = hbsm->colptr[j]; k < hbsm->colptr[j+1]; k++)
         {  if (!(1 <= k && k <= hbsm->nnzero)) goto err5;
            i = hbsm->rowind[k];
            if (!(1 <= i && i <= hbsm->nrow)) goto err6;
            val = 1.0;
            new_elem(A, i, j, val);
         }
      }
      else if (strcmp(hbsm->mxtype, "PRA") == 0)
      {  /* pattern rectangular assembled */
         if (hbsm->colptr == NULL) goto err2;
         if (hbsm->rowind == NULL) goto err3;
         A = create_mat(hbsm->nrow, hbsm->ncol);
         for (j = 1; j <= hbsm->ncol; j++)
         for (k = hbsm->colptr[j]; k < hbsm->colptr[j+1]; k++)
         {  if (!(1 <= k && k <= hbsm->nnzero)) goto err5;
            i = hbsm->rowind[k];
            if (!(1 <= i && i <= hbsm->nrow)) goto err6;
            val = 1.0;
            new_elem(A, i, j, val);
         }
      }
      else
      {  print("hbsm_to_mat: can't convert matrix of type `%s'",
            hbsm->mxtype);
         goto fail;
      }
      return A;
fail: if (A != NULL) delete_mat(A);
      return NULL;
}

/* eof */
