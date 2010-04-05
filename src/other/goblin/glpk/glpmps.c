/* glpmps.c */

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
#include "glpavl.h"
#include "glplib.h"
#include "glpmps.h"
#include "glpstr.h"

/*----------------------------------------------------------------------
-- load_mps - load linear programming model in MPS format.
--
-- *Synopsis*
--
-- #include "glpmps.h"
-- MPS *load_mps(char *fname);
--
-- *Description*
--
-- The load_mps routine loads a linear programming model in the MPS
-- format from the text file whose name is the character string fname.
--
-- Detailed description of the MPS format can be found, for example,
-- in the following book:
--
-- B.A.Murtagh. Advanced Linear Programming: Computation and Practice.
-- McGraw-Hill, 1981.
--
-- *Returns*
--
-- The load_mps routine returns a pointer to the object of MPS type
-- that represents the loaded data in the MPS format. In case of error
-- the routine prints an appropriate error message and returns NULL. */

static MPS *mps;
/* pointer to MPS data block */

static AVLTREE *t_row, *t_col, *t_rhs, *t_rng, *t_bnd;
/* symbol tables for rows, columns, right-hand sides, ranges, and
   bounds, respectively */

static char *fname;
/* name of the input text file */

static FILE *fp;
/* stream assigned to the input text file */

static int seqn;
/* card sequential number */

static char card[80+1];
/* card image buffer */

static char f1[2+1], f2[31+1], f3[31+1], f4[12+1], f5[31+1], f6[12+1];
/* splitted card fields */

static char *unknown = "UNKNOWN";
/* default name */

static int read_card(void);
/* read the next card */

static int split_card(void);
/* split data card to separate fileds */

static int load_rows(void);
/* load ROWS section */

static int load_columns(AVLTREE *t_xxx);
/* load COLUMNS, RHS, or RANGES section */

static int load_bounds(void);
/* load BOUNDS section */

static int load_quadobj(void);
/* load QUADOBJ section */

MPS *load_mps(char *_fname)
{     AVLNODE *node;
      MPSCOL *col; MPSCQE *cptr, *cqe;
      MPSBND *bnd; MPSBQE *bptr, *bqe;
      MPSQFE *qptr, *qfe;
      /* initialization */
      mps = NULL;
      t_row = t_col = t_rhs = t_rng = t_bnd = NULL;
      fname = _fname;
      print("load_mps: reading LP data from `%s'...", fname);
      fp = fopen(fname, "r");
      if (fp == NULL)
      {  print("load_mps: unable to open `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      seqn = 0;
      mps = umalloc(sizeof(MPS));
      memset(mps, 0, sizeof(MPS));
      mps->pool = dmp_create_pool(0);
      t_row = avl_create_tree(NULL, avl_strcmp);
      t_col = avl_create_tree(NULL, avl_strcmp);
      t_rhs = avl_create_tree(NULL, avl_strcmp);
      t_rng = avl_create_tree(NULL, avl_strcmp);
      t_bnd = avl_create_tree(NULL, avl_strcmp);
      /* process NAME indicator card */
      if (read_card()) goto fail;
      if (memcmp(card, "NAME ", 5) != 0)
      {  print("%s:%d: NAME indicator card missing", fname, seqn);
         goto fail;
      }
      memcpy(f3, card+14, 8); f3[8] = '\0'; strspx(f3);
      if (f3[0] == '\0') strcpy(f3, unknown);
      mps->name = dmp_get_atomv(mps->pool, strlen(f3)+1);
      strcpy(mps->name, f3);
      print("load_mps: name `%s'", mps->name);
      /* process ROWS section */
      if (read_card()) goto fail;
      if (memcmp(card, "ROWS ", 5) != 0)
      {  print("%s:%d: ROWS indicator card missing", fname, seqn);
         goto fail;
      }
      if (load_rows()) goto fail;
      mps->n_row = t_row->size;
      print("load_mps: %d rows", mps->n_row);
      /* process COLUMNS section */
      if (memcmp(card, "COLUMNS ", 8) != 0)
      {  print("%s:%d: COLUMNS indicator card missing", fname, seqn);
         goto fail;
      }
      if (load_columns(t_col)) goto fail;
      mps->n_col = t_col->size;
      print("load_mps: %d columns", mps->n_col);
      /* count non-zeros */
      {  int nz = 0;
         for (node = avl_find_next_node(t_col, NULL); node != NULL;
            node = avl_find_next_node(t_col, node))
         {  col = node->link;
            for (cqe = col->ptr; cqe != NULL; cqe = cqe->next) nz++;
         }
         print("load_mps: %d non-zeros", nz);
      }
      /* process RHS section */
      if (memcmp(card, "RHS ", 4) == 0)
         if (load_columns(t_rhs)) goto fail;
      mps->n_rhs = t_rhs->size;
      print("load_mps: %d right-hand side vector(s)", mps->n_rhs);
      /* process RANGES section */
      if (memcmp(card, "RANGES ", 7) == 0)
         if (load_columns(t_rng)) goto fail;
      mps->n_rng = t_rng->size;
      print("load_mps: %d range vector(s)", mps->n_rng);
      /* process BOUNDS section */
      if (memcmp(card, "BOUNDS ", 7) == 0)
         if (load_bounds()) goto fail;
      mps->n_bnd = t_bnd->size;
      print("load_mps: %d bound vector(s)", mps->n_bnd);
      /* process QUADOBJ section */
      mps->quad = NULL;
      if (memcmp(card, "QUADOBJ ", 8) == 0)
      {  int count = 0;
         if (load_quadobj()) goto fail;
         for (qfe = mps->quad; qfe != NULL; qfe = qfe->next) count++;
         print("load_mps: %d quadratic form elements", count);
      }
      /* process ENDATA indicator card */
      if (memcmp(card, "ENDATA ", 7) != 0)
      {  print("%s:%d: invalid indicator card", fname, seqn);
         goto fail;
      }
      print("load_mps: %d cards were read", seqn);
      fclose(fp);
      /* build row list */
      mps->row = ucalloc(1+mps->n_row, sizeof(MPSROW *));
      for (node = avl_find_next_node(t_row, NULL); node != NULL;
         node = avl_find_next_node(t_row, node))
         mps->row[node->type] = node->link;
      avl_delete_tree(t_row);
      /* build column list and restore original order of elements */
      mps->col = ucalloc(1+mps->n_col, sizeof(MPSCOL *));
      for (node = avl_find_next_node(t_col, NULL); node != NULL;
         node = avl_find_next_node(t_col, node))
      {  col = node->link; cptr = NULL;
         while (col->ptr != NULL)
         {  cqe = col->ptr;
            col->ptr = cqe->next;
            cqe->next = cptr;
            cptr = cqe;
         }
         col->ptr = cptr;
         mps->col[node->type] = col;
      }
      avl_delete_tree(t_col);
      /* build rhs list and restore original order of elements */
      mps->rhs = ucalloc(1+mps->n_rhs, sizeof(MPSCOL *));
      for (node = avl_find_next_node(t_rhs, NULL); node != NULL;
         node = avl_find_next_node(t_rhs, node))
      {  col = node->link; cptr = NULL;
         while (col->ptr != NULL)
         {  cqe = col->ptr;
            col->ptr = cqe->next;
            cqe->next = cptr;
            cptr = cqe;
         }
         col->ptr = cptr;
         mps->rhs[node->type] = col;
      }
      avl_delete_tree(t_rhs);
      /* build ranges list and restore original order of elements */
      mps->rng = ucalloc(1+mps->n_rng, sizeof(MPSCOL *));
      for (node = avl_find_next_node(t_rng, NULL); node != NULL;
         node = avl_find_next_node(t_rng, node))
      {  col = node->link; cptr = NULL;
         while (col->ptr != NULL)
         {  cqe = col->ptr;
            col->ptr = cqe->next;
            cqe->next = cptr;
            cptr = cqe;
         }
         col->ptr = cptr;
         mps->rng[node->type] = col;
      }
      avl_delete_tree(t_rng);
      /* build bounds list and restore original order of elements */
      mps->bnd = ucalloc(1+mps->n_bnd, sizeof(MPSBND *));
      for (node = avl_find_next_node(t_bnd, NULL); node != NULL;
         node = avl_find_next_node(t_bnd, node))
      {  bnd = node->link; bptr = NULL;
         while (bnd->ptr != NULL)
         {  bqe = bnd->ptr;
            bnd->ptr = bqe->next;
            bqe->next = bptr;
            bptr = bqe;
         }
         bnd->ptr = bptr;
         mps->bnd[node->type] = bnd;
      }
      avl_delete_tree(t_bnd);
      /* restore original order of quadratic form elements */
      qptr = NULL;
      while (mps->quad != NULL)
      {  qfe = mps->quad;
         mps->quad = qfe->next;
         qfe->next = qptr;
         qptr = qfe;
      }
      mps->quad = qptr; 
      /* loading has been completed */
      return mps;
fail: /* something wrong in Danish kingdom */
      if (mps != NULL)
      {  if (mps->pool != NULL) dmp_delete_pool(mps->pool);
         if (mps->row != NULL) ufree(mps->row);
         if (mps->col != NULL) ufree(mps->col);
         if (mps->rhs != NULL) ufree(mps->rhs);
         if (mps->rng != NULL) ufree(mps->rng);
         if (mps->bnd != NULL) ufree(mps->bnd);
         ufree(mps);
      }
      if (t_row != NULL) avl_delete_tree(t_row);
      if (t_col != NULL) avl_delete_tree(t_col);
      if (t_rhs != NULL) avl_delete_tree(t_rhs);
      if (t_rng != NULL) avl_delete_tree(t_rng);
      if (t_bnd != NULL) avl_delete_tree(t_bnd);
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
loop: seqn++;
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
      /* asterisk in the leftmost column means comment */
      if (card[0] == '*') goto loop;
      return 0;
}

/*----------------------------------------------------------------------
-- split_card - split data card to separate fields.
--
-- This routine splits the current data card to separate fileds f1, f2,
-- f3, f4, f5, and f6. If the data card has correct format, the routine
-- returns zero, otherwise non-zero. */

static int split_card(void)
{     /* col. 1: blank */
      if (memcmp(card+0, " ", 1))
fail: {  print("%s:%d: invalid data card", fname, seqn);
         return 1;
      }
      /* col. 2-3: field 1 (code) */
      memcpy(f1, card+1, 2); f1[2] = '\0'; strspx(f1);
      /* col. 4: blank */
      if (memcmp(card+3, " ", 1)) goto fail;
      /* col. 5-12: field 2 (name) */
      memcpy(f2, card+4, 8); f2[8] = '\0'; strspx(f2);
      /* col. 13-14: blanks */
      if (memcmp(card+12, "  ", 2)) goto fail;
      /* col. 15-22: field 3 (name) */
      memcpy(f3, card+14, 8); f3[8] = '\0'; strspx(f3);
      if (f3[0] == '$')
      {  /* from col. 15 to the end of the card is a comment */
         f3[0] = f4[0] = f5[0] = f6[0] = '\0';
         goto done;
      }
      /* col. 23-24: blanks */
      if (memcmp(card+22, "  ", 2)) goto fail;
      /* col. 25-36: field 4 (number) */
      memcpy(f4, card+24, 12); f4[12] = '\0'; strspx(f4);
      /* col. 37-39: blanks */
      if (memcmp(card+36, "   ", 3)) goto fail;
      /* col. 40-47: field 5 (name) */
      memcpy(f5, card+39,  8); f5[8]  = '\0'; strspx(f5);
      if (f5[0] == '$')
      {  /* from col. 40 to the end of the card is a comment */
         f5[0] = f6[0] = '\0';
         goto done;
      }
      /* col. 48-49: blanks */
      if (memcmp(card+47, "  ", 2)) goto fail;
      /* col. 50-61: field 6 (number) */
      memcpy(f6, card+49, 12); f6[12] = '\0'; strspx(f6);
      /* col. 62-71: blanks */
      if (memcmp(card+61, "          ", 10)) goto fail;
done: return 0;
}

/*----------------------------------------------------------------------
-- load_rows - load ROWS section.
--
-- The load_rows routine loads ROWS section reading data cards that
-- are placed after ROWS indicator card. If loading is ok, the routine
-- returns zero, otherwise non-zero. */

static int load_rows(void)
{     MPSROW *row;
      AVLNODE *node;
loop: /* process the next data card */
      if (read_card()) return 1;
      if (card[0] != ' ') goto done;
      if (split_card()) return 1;
      if (strcmp(f3, "'MARKER'") == 0)
      {  print("%s:%d: invalid use of marker in ROWS section", fname,
            seqn);
         return 1;
      }
      if (f1[0] == '\0')
      {  print("%s:%d: missing row type in field 1", fname, seqn);
         return 1;
      }
      if (!(strchr("NGLE", f1[0]) != NULL && f1[1] == '\0'))
      {  print("%s:%d: unknown row type `%s' in field 1", fname, seqn,
            f1);
         return 1;
      }
      if (f2[0] == '\0')
      {  print("%s:%d: missing row name in field 2", fname, seqn);
         return 1;
      }
      if (f3[0] != '\0' || f4[0] != '\0' || f5[0] != '\0'
         || f6[0] != '\0')
      {  print("%s:%d: invalid data in fields 3-6", fname, seqn);
         return 1;
      }
      /* create new row */
      if (avl_find_by_key(t_row, f2) != NULL)
      {  print("%s:%d: row `%s' multiply specified", fname, seqn, f2);
         return 1;
      }
      row = dmp_get_atomv(mps->pool, sizeof(MPSROW));
      row->name = dmp_get_atomv(mps->pool, strlen(f2)+1);
      strcpy(row->name, f2);
      strcpy(row->type, f1);
      /* add row name to the symbol table */
      node = avl_insert_by_key(t_row, row->name);
      node->type = t_row->size;
      node->link = row;
      goto loop;
done: return 0;
}

/*----------------------------------------------------------------------
-- load_columns - load COLUMNS, RHS, or RANGES section.
--
-- The load_columns routine loads COLUMNS, RHS, or RANGES section (that
-- depends upon the parameter t_xxx) reading data cards that are placed
-- after the corresponding indicator card. If loading is ok, the routine
-- return zero, otherwise non-zero. */

static int load_columns(AVLTREE *t_xxx)
{     MPSCOL *col;
      MPSCQE *cqe;
      AVLNODE *node, *ref;
      char name[31+1]; double val;
      int flag = 0;
      strcpy(name, (t_xxx == t_col) ? "" : unknown);
loop: /* process the next data card */
      if (read_card()) return 1;
      if (card[0] != ' ') goto done;
      if (split_card()) return 1;
      /* process optional INTORG/INTEND markers */
      if (strcmp(f3, "'MARKER'") == 0)
      {  if (t_xxx != t_col)
         {  print("%s:%d): invalid use of marker in RHS or RANGES secti"
               "on", fname, seqn);
            return 1;
         }
         if (!(f1[0] == '\0' && f4[0] == '\0' && f6[0] == '\0'))
         {  print("%s:%d: invalid data in fields 1, 4, or 6", fname,
               seqn);
            return 1;
         }
         if (f2[0] == '\0')
         {  print("%s:%d: missing marker name in field 2", fname, seqn);
            return 1;
         }
         if (strcmp(f5, "'INTORG'") == 0)
            flag = 1;
         else if (strcmp(f5, "'INTEND'") == 0)
            flag = 0;
         else
         {  print("%s:%d: unknown marker in field 5", fname, seqn);
            return 1;
         }
         goto skip;
      }
      /* process the data card */
      if (f1[0] != '\0')
      {  print("%s:%d: invalid data in field 1", fname, seqn);
         return 1;
      }
      if (f2[0] == '\0') strcpy(f2, name);
      if (f2[0] == '\0')
      {  print("%s:%d: missing column name in field 2", fname, seqn);
         return 1;
      }
      strcpy(name, f2);
      /* search for column or vector specified in field 2 */
      node = avl_find_by_key(t_xxx, f2);
      if (node == NULL)
      {  /* not found; create new column or vector */
         col = dmp_get_atomv(mps->pool, sizeof(MPSCOL));
         col->name = dmp_get_atomv(mps->pool, strlen(f2)+1);
         strcpy(col->name, f2);
         col->flag = 0;
         col->ptr = NULL;
         /* add column or vector name to the symbol table */
         node = avl_insert_by_key(t_xxx, col->name);
         node->type = t_xxx->size;
         node->link = col;
      }
      col->flag = flag;
#if 1
      /* all elements of the same column or vector should be placed
         together as specified by MPS format, although such restriction
         is not essential for this routine */
      if (node->type < t_xxx->size)
      {  print("%s:%d: %s `%s' multiply specified", fname, seqn,
            t_xxx == t_col ? "column" :
            t_xxx == t_rhs ? "right-hand side vector" :
            t_xxx == t_rng ? "range vector" : "???", f2);
         return 1;
      }
#endif
      /* process the first row-element pair (fields 3 and 4) */
      if (f3[0] == '\0')
      {  print("%s:%d: missing row name in field 3", fname, seqn);
         return 1;
      }
      if (f4[0] == '\0')
      {  print("%s:%d: missing value in field 4", fname, seqn);
         return 1;
      }
      /* create new column or vector element */
      ref = avl_find_by_key(t_row, f3);
      if (ref == NULL)
      {  print("%s:%d: row `%s' not found", fname, seqn, f3);
         return 1;
      }
      if (str2dbl(f4, &val))
      {  print("%s:%d: invalid value `%s'", fname, seqn, f4);
         return 1;
      }
      cqe = dmp_get_atomv(mps->pool, sizeof(MPSCQE));
      cqe->ind = ref->type;
      cqe->val = val;
      cqe->next = col->ptr;
      col->ptr = cqe;
      /* process the second row-element pair (fields 5 and 6) */
      if (f5[0] == '\0' && f6[0] == '\0') goto skip;
      if (f5[0] == '\0')
      {  print("%s:%d: missing row name in field 5", fname, seqn);
         return 1;
      }
      if (f6[0] == '\0')
      {  print("%s:%d: missing value in filed 6", fname, seqn);
         return 1;
      }
      /* create new column or vector element */
      ref = avl_find_by_key(t_row, f5);
      if (ref == NULL)
      {  print("%s:%d: row `%s' not found", fname, seqn, f5);
         return 1;
      }
      if (str2dbl(f6, &val))
      {  print("%s:%d: invalid value `%s'", fname, seqn, f6);
         return 1;
      }
      cqe = dmp_get_atomv(mps->pool, sizeof(MPSCQE));
      cqe->ind = ref->type;
      cqe->val = val;
      cqe->next = col->ptr;
      col->ptr = cqe;
skip: goto loop;
done: return 0;
}

/*----------------------------------------------------------------------
-- load_bounds - load BOUNDS section.
--
-- The load_bounds routine loads BOUNDS section reading data cards that
-- are placed after BOUNDS indicator card. If loading is ok, the routine
-- returns zero, otherwise non-zero. */

static int load_bounds(void)
{     MPSBND *bnd;
      MPSBQE *bqe;
      AVLNODE *node, *ref;
      char name[31+1]; double val;
      strcpy(name, unknown);
loop: /* process the next data card */
      if (read_card()) return 1;
      if (card[0] != ' ') goto done;
      if (split_card()) return 1;
      if (strcmp(f3, "'MARKER'") == 0)
      {  print("%s:%d: invalid use of marker in BOUNDS section", fname,
            seqn);
         return 1;
      }
      if (f1[0] == '\0')
      {  print("%s:%d: missing bound type in field 1", fname, seqn);
         return 1;
      }
      if (strcmp(f1, "LO") && strcmp(f1, "UP") && strcmp(f1, "FX")
         && strcmp(f1, "FR") && strcmp(f1, "MI") && strcmp(f1, "PL")
         && strcmp(f1, "UI") && strcmp(f1, "BV"))
      {  print("%s:%d: unknown bound type `%s' in field 1", fname, seqn,
            f1);
         return 1;
      }
      if (f2[0] == '\0') strcpy(f2, name);
      strcpy(name, f2);
      if (f3[0] == '\0')
      {  print("%s:%d: missing column name in field 3", fname, seqn);
         return 1;
      }
      if (f4[0] == '\0')
      if (!strcmp(f1, "LO") || !strcmp(f1, "UP") || !strcmp(f1, "FX")
         || !strcmp(f1, "UI"))
      {  print("%s:%d: missing value in field 4", fname, seqn);
         return 1;
      }
      if (f5[0] != '\0' && f6[0] != '\0')
      {  print("%s:%d: invalid data in field 5-6", fname, seqn);
         return 1;
      }
      /* search for bound vector specified in field 2 */
      node = avl_find_by_key(t_bnd, f2);
      if (node == NULL)
      {  /* not found; create new bound vector */
         bnd = dmp_get_atomv(mps->pool, sizeof(MPSBND));
         bnd->name = dmp_get_atomv(mps->pool, strlen(f2)+1);
         strcpy(bnd->name, f2);
         bnd->ptr = NULL;
         /* add vector name to the symbol table */
         node = avl_insert_by_key(t_bnd, bnd->name);
         node->type = t_bnd->size;
         node->link = bnd;
      }
#if 1
      /* all elements of the same bound vector should be placed
         together as specified by MPS format, although such restriction
         is not essential for this routine */
      if (node->type < t_bnd->size)
      {  print("%s:%d: bound vector `%s' multiply specified", fname,
            seqn, f2);
         return 1;
      }
#endif
      /* process column-element pair */
      ref = avl_find_by_key(t_col, f3);
      if (ref == NULL)
      {  print("%s:%d: column `%s' not found", fname, seqn, f3);
         return 1;
      }
      val = 0.0;
      if (f4[0] != '\0' && str2dbl(f4, &val))
      {  print("%s:%d: invalid value `%s'", fname, seqn, f4);
         return 1;
      }
      /* create new bound vector element */
      bqe = dmp_get_atomv(mps->pool, sizeof(MPSBQE));
      strcpy(bqe->type, f1);
      bqe->ind = ref->type;
      bqe->val = val;
      bqe->next = bnd->ptr;
      bnd->ptr = bqe;
      goto loop;
done: return 0;
}

/*----------------------------------------------------------------------
-- load_quadobj - load QUADOBJ section.
--
-- The load_quadobj routine loads QUADOBJ section reading data cards
-- that are placed after QUADOBJ indicator card. If loading is ok, the
-- routine returns zero, otherwise non-zero.
--
-- The QUADOBJ section specifies quadratic part x'*Q*x of the objective
-- function. Should note that this feature is non-standard extension of
-- MPS format. For detailed format description see:
--
-- I.Maros, C.Meszaros. A Repository of Convex Quadratic Programming
-- Problems. */

static int load_quadobj(void)
{     MPSQFE *qfe;
      AVLNODE *ref1, *ref2;
      double val;
loop: /* process the next data card */
      if (read_card()) return 1;
      if (card[0] != ' ') goto done;
      if (split_card()) return 1;
      if (strcmp(f3, "'MARKER'") == 0)
      {  print("%s:%d: invalid use of marker on QUADOBJ section",
            fname, seqn);
         return 1;
      }
      if (!(f1[0] == '\0' && f5[0] == '\0' && f6[0] == '\0'))
      {  print("%s:%d: invalid data in fields 1, 5, or 6",
            fname, seqn);
         return 1;
      }
      if (f2[0] == '\0')
      {  print("%s:%d: missing first column name in field 2",
            fname, seqn);
         return 1;
      }
      if (f3[0] == '\0')
      {  print("%s:%d: missing second column name in field 3",
            fname, seqn);
         return 1;
      }
      if (f4[0] == '\0')
      {  print("%s:%d: missing value in field 4", fname, seqn);
         return 1;
      }
      ref1 = avl_find_by_key(t_col, f2);
      if (ref1 == NULL)
      {  print("%s:%d: column `%s' not found", fname, seqn, f2);
         return 1;
      }
      ref2 = avl_find_by_key(t_col, f3);
      if (ref2 == NULL)
      {  print("%s:%d: column `%s' not found", fname, seqn, f3);
         return 1;
      }
      val = 0.0;
      if (f4[0] != '\0' && str2dbl(f4, &val))
      {  print("%s:%d: invalid value `%s'", fname, seqn, f4);
         return 1;
      }
      /* create new quadratic form element */
      qfe = dmp_get_atomv(mps->pool, sizeof(MPSQFE));
      qfe->ind1 = ref1->type;
      qfe->ind2 = ref2->type;
      qfe->val = val;
      qfe->next = mps->quad;
      mps->quad = qfe;
      goto loop;
done: return 0;
}

/*----------------------------------------------------------------------
-- free_mps - free linear programming model in MPS format.
--
-- *Synopsis*
--
-- #include "glpmps.h"
-- void free_mps(MPS *mps);
--
-- *Description*
--
-- The free_mps routine frees all memory allocated to the object of MPS
-- type that represents linear programming model in the MPS format. */

void free_mps(MPS *mps)
{       dmp_delete_pool(mps->pool);
        ufree(mps->row);
        ufree(mps->col);
        ufree(mps->rhs);
        ufree(mps->rng);
        ufree(mps->bnd);
        ufree(mps);
        return;
}

#if 0
/*----------------------------------------------------------------------
-- mps_to_lp - convert linear programming problem from MPS to LP.
--
-- *Synopsis*
--
-- #include "glpmps.h"
-- LP *mps_to_lp(MPS *mps, char *obj, char *rhs, char *rng, char *bnd);
--
-- *Description*
--
-- The mps_to_lp routine converts the linear programming problem data
-- block from the object of MPS type (which mps points to) to the object
-- of LP type.
--
-- The character string obj specifies row name, which has to be used as
-- the objective function. If obj is empty string, the first row of N
-- type (free auxiliary variable) is used as the objective function (if
-- the problem has no rows of N type, it is assumed that the objective
-- function is identically equal to zero).
--
-- The character string rhs specifies the name of right-hand side (RHS)
-- vector. If rhs is empty string, the first RHS vector is used (if the
-- problem has at least one RHS vector).
--
-- The character string rng specifies the name of range vector. If rng
-- is empty string, the first range vector is used (if the problem has
-- at least one range vector).
--
-- The character string bnd specifies the name of bound vector. If bnd
-- is empty string, the first bound vector is used (if the problem has
-- at least one bound vector).
--
-- *Returns*
--
-- If the conversion was successful, the mps_to_lp routine returns a
-- pointer to an object of LP type, which represents linear programming
-- data block in standard format. In case of error the routine returns
-- NULL. */

LP *mps_to_lp(MPS *mps, char *obj, char *rhs, char *rng, char *bnd)
{     LP *lp;
      MPSROW *row; MPSCOL *col; MPSCQE *cqe; MPSBQE *bqe;
      int m = mps->n_row, n = mps->n_col, i, j, k, objrow, v;
      lp = create_lp(m, n, 1);
      /* process ROWS section */
      for (i = 1; i <= m; i++)
      {  k = i; /* x[k] = i-th auxiliary variable */
         row = mps->row[i];
         if (strcmp(row->type, "N") == 0)
            lp->type[k] = 'F';
         else if (strcmp(row->type, "G") == 0)
            lp->type[k] = 'L';
         else if (strcmp(row->type, "L") == 0)
            lp->type[k] = 'U';
         else if (strcmp(row->type, "E") == 0)
            lp->type[k] = 'S';
         else
         {  print("mps_to_lp: row `%s' has unknown type `%s'",
               row->name, row->type);
            goto fail;
         }
      }
      /* process COLUMNS section */
      for (j = 1; j <= n; j++)
      {  col = mps->col[j];
         /* add column elements to the constraint matrix */
         for (cqe = col->ptr; cqe != NULL; cqe = cqe->next)
            new_elem(lp->A, cqe->ind, j, cqe->val);
         /* set integer variable flag */
         lp->kind[j] = (col->flag ? 1 : 0);
      }
      /* determine row of the objective function */
      objrow = 0;
      for (i = 1; i <= m; i++)
      {  row = mps->row[i];
         if ((obj[0] == '\0' && strcmp(row->type, "N") == 0) ||
             (obj[0] != '\0' && strcmp(row->name, obj) == 0))
         {  objrow = i;
            break;
         }
      }
      if (obj[0] != '\0' && objrow == 0)
      {  print("mps_to_lp: objective function row `%s' not found",
            obj);
         goto fail;
      }
      /* copy coefficients of the objective function */
      if (objrow != 0)
      {  ELEM *e;
         for (e = lp->A->row[objrow]; e != NULL; e = e->row)
            lp->c[e->j] += e->val;
      }
      /* process RHS section */
      if (rhs[0] == '\0')
         v = (mps->n_rhs == 0 ? 0 : 1);
      else
      {  for (v = 1; v <= mps->n_rhs; v++)
            if (strcmp(mps->rhs[v]->name, rhs) == 0) break;
         if (v > mps->n_rhs)
         {  print("mps_to_lp: right-hand side vector `%s' not found",
               rhs);
            goto fail;
         }
      }
      if (v > 0)
      {  for (cqe = mps->rhs[v]->ptr; cqe != NULL; cqe = cqe->next)
         {  i = cqe->ind;
            k = i; /* x[k] = i-th auxiliary variable */
            row = mps->row[i];
            /* some MPS files contains RHS elements for rows of N type;
               these elements mean that if the corresponding auxiliary
               variable (i.e. row) is non-basis, it has the given RHS
               value (instead zero); however, these elements are ignored
               by this routine, except RHS element for the objective
               function row, which is treated as its constant term with
               opposite sign */
            if (strcmp(row->type, "N") == 0)
            {  if (i == objrow)
                  lp->c[0] = - cqe->val;
               else
                  /* nop */;
            }
            else if (strcmp(row->type, "G") == 0)
               lp->lb[k] = cqe->val;
            else if (strcmp(row->type, "L") == 0)
               lp->ub[k] = cqe->val;
            else if (strcmp(row->type, "E") == 0)
               lp->lb[k] = lp->ub[k] = cqe->val;
            else
               insist(row->type != row->type);
         }
      }
      /* process RANGES section */
      if (rng[0] == '\0')
         v = (mps->n_rng == 0 ? 0 : 1);
      else
      {  for (v = 1; v <= mps->n_rng; v++)
            if (strcmp(mps->rng[v]->name, rng) == 0) break;
         if (v > mps->n_rng)
         {  print("mps_to_lp: range vector `%s' not found", rng);
            goto fail;
         }
      }
      if (v > 0)
      {  for (cqe = mps->rng[v]->ptr; cqe != NULL; cqe = cqe->next)
         {  i = cqe->ind;
            k = i; /* x[k] = i-th auxiliary variable */
            row = mps->row[i];
            if (strcmp(row->type, "N") == 0)
            {  print("mps_to_lp: range vector entry refers to row `%s' "
                  " of N type", row->name);
               goto fail;
            }
            else if (strcmp(row->type, "G") == 0)
incr:          lp->ub[k] = lp->lb[k] + fabs(cqe->val);
            else if (strcmp(row->type, "L") == 0)
decr:          lp->lb[k] = lp->ub[k] - fabs(cqe->val);
            else if (strcmp(row->type, "E") == 0)
            {  if (cqe->val > 0.0) goto incr;
               if (cqe->val < 0.0) goto decr;
            }
            else
               insist(row->type != row->type);
            lp->type[k] = (lp->lb[k] == lp->ub[k] ? 'S' : 'D');
         }
      }
      /* process BOUNDS section */
      if (bnd[0] == '\0')
         v = (mps->n_bnd == 0 ? 0 : 1);
      else
      {  for (v = 1; v <= mps->n_bnd; v++)
            if (strcmp(mps->bnd[v]->name, bnd) == 0) break;
         if (v > mps->n_bnd)
         {  print("mps_to_lp: bound vector `%s' not found", bnd);
            goto fail;
         }
      }
      if (v > 0)
      {  for (k = m+1; k <= m+n; k++)
            lp->lb[k] = 0.0, lp->ub[k] = +DBL_MAX;
         for (bqe = mps->bnd[v]->ptr; bqe != NULL; bqe = bqe->next)
         {  j = bqe->ind;
            k = m + j; /* x[k] = j-th structural variable */
            col = mps->col[j];
            if (strcmp(bqe->type, "LO") == 0)
               lp->lb[k] = bqe->val;
            else if (strcmp(bqe->type, "UP") == 0)
               lp->ub[k] = bqe->val;
            else if (strcmp(bqe->type, "FX") == 0)
               lp->lb[k] = lp->ub[k] = bqe->val;
            else if (strcmp(bqe->type, "FR") == 0)
               lp->lb[k] = -DBL_MAX, lp->ub[k] = +DBL_MAX;
            else if (strcmp(bqe->type, "MI") == 0)
               lp->lb[k] = -DBL_MAX;
            else if (strcmp(bqe->type, "PL") == 0)
               lp->ub[k] = +DBL_MAX;
            else if (strcmp(bqe->type, "UI") == 0)
            {  /* integer structural variable with upper bound */
               lp->ub[k] = bqe->val;
               lp->kind[j] = 1;
            }
            else if (strcmp(bqe->type, "BV") == 0)
            {  /* binary structural variable */
               lp->lb[k] = 0.0, lp->ub[k] = 1.0;
               lp->kind[j] = 1;
            }
            else
            {  print("mps_to_lp: bound vector entry for column `%s' has"
                  " unknown type `%s'", col->name, bqe->type);
               goto fail;
            }
         }
         for (k = m+1; k <= m+n; k++)
         {  if (lp->lb[k] == -DBL_MAX && lp->ub[k] == +DBL_MAX)
               lp->type[k] = 'F', lp->lb[k] = lp->ub[k] = 0.0;
            else if (lp->ub[k] == +DBL_MAX)
               lp->type[k] = 'L', lp->ub[k] = 0.0;
            else if (lp->lb[k] == -DBL_MAX)
               lp->type[k] = 'U', lp->lb[k] = 0.0;
            else if (lp->lb[k] != lp->ub[k])
               lp->type[k] = 'D';
            else
               lp->type[k] = 'S';
         }
      }
      /* if the problem has no integer columns, free the array kind */
      for (j = 1; j <= n; j++) if (lp->kind[j]) break;
      if (j > n) ufree(lp->kind), lp->kind = NULL;
      /* return to the calling program */
      return lp;
fail: /* conversion failed */
      delete_lp(lp);
      return NULL;
}
#endif

/* eof */
