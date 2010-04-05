/* glplpt.c */

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
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "glpavl.h"
#include "glplib.h"
#include "glplpt.h"

struct dsa
{     /* dynamic storage area used by the routine lpt_read_prob */
      LPT *lpt;
      /* LP problem which is being read */
      AVLTREE *trow;
      /* search tree used to find rows (node keys are row names; node
         links are pointers to rows) */
      AVLTREE *tcol;
      /* search tree used to find columns (node keys are column names;
         node links are pointers to columns) */
      char *fname;
      /* name of input text file */
      FILE *fp;
      /* stream assigned to input text file */
      jmp_buf jump;
      /* label used for non-local go to in case of error */
      int count;
      /* input line count */
      char line[255+1];
      /* current input line */
      int pos;
      /* position of current character */
      int token;
      /* code of current token: */
#define T_EOF        0  /* end of file */
#define T_MINIMIZE   1  /* keyword 'minimize' */
#define T_MAXIMIZE   2  /* keyword 'maximize' */
#define T_SUBJECT_TO 3  /* keyword 'subject to' */
#define T_BOUNDS     4  /* keyword 'bounds' */
#define T_GENERAL    5  /* keyword 'general' */
#define T_INTEGER    6  /* keyword 'integer' */
#define T_BINARY     7  /* keyword 'binary' */
#define T_END        8  /* keyword 'end' */
#define T_NAME       9  /* symbolic name */
#define T_NUMBER     10 /* numeric constant */
#define T_PLUS       11 /* delimiter '+' */
#define T_MINUS      12 /* delimiter '-' */
#define T_COLON      13 /* delimiter ':' */
#define T_LE         14 /* delimiter '<=' */
#define T_GE         15 /* delimiter '>=' */
#define T_EQ         16 /* delimiter '=' */
      char image[255+1];
      /* image of current token */
      int imlen;
      /* length of token image */
      double value;
      /* value of numerical constant */
};

/*----------------------------------------------------------------------
-- fatal - print error message and terminate processing.
--
-- This routine prints an error message and performs non-local go to in
-- order to terminate input file processing. */

static void fatal(struct dsa *dsa, char *fmt, ...)
{     va_list arg;
      char msg[4095+1];
      va_start(arg, fmt);
      vsprintf(msg, fmt, arg);
      insist(strlen(msg) <= 4095);
      va_end(arg);
      print("%s:%d: %s", dsa->fname, dsa->count, msg);
      longjmp(dsa->jump, 1);
      /* no return */
}

/*----------------------------------------------------------------------
-- read_line - read next line from input file.
--
-- This routine reads a next line from the input file. If the line has
-- been read, the routine returns zero. If end-of-file has been reached,
-- the routine returns non-zero. */

static int read_line(struct dsa *dsa)
{     int c, len = 0, ret = 0;
      dsa->count++;
      for (;;)
      {  c = fgetc(dsa->fp);
         if (ferror(dsa->fp))
            fatal(dsa, "read error - %s", strerror(errno));
         if (feof(dsa->fp))
         {  if (len == 0)
               dsa->count--, ret = 1;
            else
               print("%s:%d: warning: missing final LF", dsa->fname,
                  dsa->count);
            break;
         }
         if (c == '\r') continue;
         if (c == '\n') break;
         if (isspace(c)) c = ' ';
         if (iscntrl(c))
            fatal(dsa, "invalid control character 0x%02X", c);
         if (len == sizeof(dsa->line) - 1)
            fatal(dsa, "line length exceeds %d characters",
               sizeof(dsa->line) - 1);
         dsa->line[len++] = (char)c;
      }
      dsa->line[len] = '\0';
      return ret;
}

/*----------------------------------------------------------------------
-- add_char - append character to current token.
--
-- This routine appends the current character to the the current token
-- image and then advances the current character position. */

static void add_char(struct dsa *dsa)
{     insist(dsa->imlen <= sizeof(dsa->image) - 1);
      dsa->image[dsa->imlen++] = dsa->line[dsa->pos++];
      dsa->image[dsa->imlen] = '\0';
      return;
}

/*----------------------------------------------------------------------
-- scan_keyword - recognize and scan given keyword.
--
-- At first this routine checks whether a given keyword (written in
-- lower-case letters) starts at the beginning of line and is followed
-- by blank or NUL character. If the keyword has been recognized, the
-- routine scans it advancing the character position to the terminating
-- blank or NUL character and returns non-zero. If the keyword has not
-- been recognized, the routine returns zero. */

static int scan_keyword(struct dsa *dsa, char *keyword)
{     int len = strlen(keyword), j, c;
      insist(dsa->pos == 0);
      for (j = 0; j < len; j++)
      {  c = tolower((unsigned char)dsa->line[j]);
         if (c != keyword[j]) return 0;
      }
      c = dsa->line[len];
      if (!(c == ' ' || c == '\0')) return 0;
      /* keyword has been recognized; scan it */
      for (j = 0; j < len; j++) add_char(dsa);
      return 1;
}

/*----------------------------------------------------------------------
-- scan_token - scan next token.
--
-- This routine scans a next token that appears in the input stream.
-- On exit from the routine the character position will correspond to
-- a character that immediately follows the scanned token. */

static void scan_token(struct dsa *dsa)
{     char *set = "!\"#$%&()/,.;?@_`'{}|~"; /* may appear in names */
      int c;
      dsa->token = -1;
      dsa->image[0] = '\0';
      dsa->imlen = 0;
      dsa->value = 0.0;
loop: /* if the current character is NUL, read the next line */
      while (dsa->line[dsa->pos] == '\0')
      {  if (read_line(dsa))
         {  /* end-of-file */
            dsa->token = T_EOF;
            goto done;
         }
         dsa->pos = 0;
      }
      /* if the current character being letter is at the beginning of
         the line, it may begin a keyword */
      if (dsa->pos == 0 && isalpha((unsigned char)dsa->line[0]))
      {  if (scan_keyword(dsa, "minimize"))
            dsa->token = T_MINIMIZE;
         else if (scan_keyword(dsa, "minimum"))
            dsa->token = T_MINIMIZE;
         else if (scan_keyword(dsa, "min"))
            dsa->token = T_MINIMIZE;
         else if (scan_keyword(dsa, "maximize"))
            dsa->token = T_MAXIMIZE;
         else if (scan_keyword(dsa, "maximum"))
            dsa->token = T_MAXIMIZE;
         else if (scan_keyword(dsa, "max"))
            dsa->token = T_MAXIMIZE;
         else if (scan_keyword(dsa, "subject to"))
            dsa->token = T_SUBJECT_TO;
         else if (scan_keyword(dsa, "such that"))
            dsa->token = T_SUBJECT_TO;
         else if (scan_keyword(dsa, "st"))
            dsa->token = T_SUBJECT_TO;
         else if (scan_keyword(dsa, "s.t."))
            dsa->token = T_SUBJECT_TO;
         else if (scan_keyword(dsa, "st."))
            dsa->token = T_SUBJECT_TO;
         else if (scan_keyword(dsa, "bounds"))
            dsa->token = T_BOUNDS;
         else if (scan_keyword(dsa, "bound"))
            dsa->token = T_BOUNDS;
         else if (scan_keyword(dsa, "general"))
            dsa->token = T_GENERAL;
         else if (scan_keyword(dsa, "generals"))
            dsa->token = T_GENERAL;
         else if (scan_keyword(dsa, "gen"))
            dsa->token = T_GENERAL;
         else if (scan_keyword(dsa, "integer"))
            dsa->token = T_INTEGER;
         else if (scan_keyword(dsa, "integers"))
            dsa->token = T_INTEGER;
         else if (scan_keyword(dsa, "int"))
            dsa->token = T_INTEGER;
         else if (scan_keyword(dsa, "binary"))
            dsa->token = T_BINARY;
         else if (scan_keyword(dsa, "binaries"))
            dsa->token = T_BINARY;
         else if (scan_keyword(dsa, "bin"))
            dsa->token = T_BINARY;
         else if (scan_keyword(dsa, "end"))
            dsa->token = T_END;
         else
            goto skip;
         /* the keyword has been scanned */
         goto done;
      }
skip: /* skip spaces until something significant appears */
      while (dsa->line[dsa->pos] == ' ') dsa->pos++;
      /* get the current character */
      c = (unsigned char)dsa->line[dsa->pos];
      /* recognize and scan the current token */
      if (c == '\0')
      {  /* end-of-line */
         goto loop;
      }
      else if (c == '\\')
      {  /* comment; ignore everything until end-of-line */
         dsa->line[dsa->pos] = '\0';
         goto loop;
      }
      else if (isalpha(c) || c != '.' && strchr(set, c) != NULL)
      {  /* symbolic name */
         dsa->token = T_NAME;
         for (;;)
         {  c = (unsigned char)dsa->line[dsa->pos];
            if (c == '\0') break;
            if (!(isalnum(c) || strchr(set, c) != NULL)) break;
            add_char(dsa);
         }
         if (strlen(dsa->image) > 16)
         {  dsa->image[16] = '\0';
            print("%s:%d: warning: name `%s...' too long; truncated to "
               "16 characters", dsa->fname, dsa->count, dsa->image);
         }
      }
      else if (c == '.' || isdigit(c))
      {  /* numeric constant */
         dsa->token = T_NUMBER;
         /* scan integer part */
         while (isdigit((unsigned char)dsa->line[dsa->pos]))
            add_char(dsa);
         /* scan optional fractional part (it is mandatory, if there is
            no integer part) */
         if (dsa->line[dsa->pos] == '.')
         {  add_char(dsa);
            if (dsa->imlen == 1 &&
               !isdigit((unsigned char)dsa->line[dsa->pos]))
               fatal(dsa, "invalid use of decimal point");
            while (isdigit((unsigned char)dsa->line[dsa->pos]))
               add_char(dsa);
         }
         /* scan optional decimal exponent */
         c = (unsigned char)dsa->line[dsa->pos];
         if (c == 'e' || c == 'E')
         {  add_char(dsa);
            c = (unsigned char)dsa->line[dsa->pos];
            if (c == '+' || c == '-') add_char(dsa);
            c = (unsigned char)dsa->line[dsa->pos];
            if (!isdigit(c))
               fatal(dsa, "numeric constant `%s' incomplete",
                  dsa->image);
            while (isdigit((unsigned char)dsa->line[dsa->pos]))
               add_char(dsa);
         }
         /* convert the numeric constant to floating-point */
         if (str2dbl(dsa->image, &dsa->value))
            fatal(dsa, "numeric constant `%s' out of range",
               dsa->image);
      }
      else if (c == '+')
         dsa->token = T_PLUS, add_char(dsa);
      else if (c == '-')
         dsa->token = T_MINUS, add_char(dsa);
      else if (c == ':')
         dsa->token = T_COLON, add_char(dsa);
      else if (c == '<')
      {  dsa->token = T_LE, add_char(dsa);
         if (dsa->line[dsa->pos] == '=') add_char(dsa);
      }
      else if (c == '>')
      {  dsa->token = T_GE, add_char(dsa);
         if (dsa->line[dsa->pos] == '=') add_char(dsa);
      }
      else if (c == '=')
      {  dsa->token = T_EQ, add_char(dsa);
         if (dsa->line[dsa->pos] == '<')
            dsa->token = T_LE, add_char(dsa);
         else if (dsa->line[dsa->pos] == '>')
            dsa->token = T_GE, add_char(dsa);
      }
      else
         fatal(dsa, "character `%c' not recognized", c);
done: /* bring the token to a parsing routine */
      return;
}

/*----------------------------------------------------------------------
-- find_col - find column (variable).
--
-- This routine finds a column that has a given name. If the column
-- doesn't exist, the routine create it and add to the column linked
-- list and to the column search tree. */

static LPTCOL *find_col(struct dsa *dsa, char *name)
{     AVLNODE *node;
      int len = strlen(name);
      insist(1 <= len && len <= 16);
      node = avl_find_by_key(dsa->tcol, name);
      if (node == NULL)
      {  /* column not found; create new column */
         LPTCOL *col;
         col = dmp_get_atomv(dsa->lpt->pool, sizeof(LPTCOL));
         strcpy(col->name, name);
         col->j = ++(dsa->lpt->n);
         col->kind = LPT_CON;
         col->lb = +DBL_MAX; /* lower bound is not specified yet */
         col->ub = -DBL_MAX; /* upper bound is not specified yet */
         col->next = NULL;
         /* add column to the linked list */
         if (dsa->lpt->first_col == NULL)
            dsa->lpt->first_col= col;
         else
            dsa->lpt->last_col->next = col;
         dsa->lpt->last_col = col;
         /* add column to the search tree */
         node = avl_insert_by_key(dsa->tcol, col->name);
         node->link = col;
      }
      /* return a pointer to the column */
      return node->link;
}

/*----------------------------------------------------------------------
-- parse_linear_form - parse linear form.
--
-- This routine parses linear form using the following syntax:
--
-- <variable> ::= <symbolic name>
-- <coefficient> ::= <numeric constant>
-- <term> ::= <variable> | <numeric constant> <variable>
-- <linear form> ::= <term> | + <term> | - <term> |
--    <linear form> + <term> | <linear form> - <term>
--
-- The routine builds a linked list of terms and returns a pointer to
-- the first term in the list. */

static LPTLFE *parse_linear_form(struct dsa *dsa)
{     LPTCOL *col;
      LPTLFE *head = NULL, *tail = NULL, *lfe;
      double s, coef;
loop: /* parse an optional sign */
      if (dsa->token == T_PLUS)
         s = +1.0, scan_token(dsa);
      else if (dsa->token == T_MINUS)
         s = -1.0, scan_token(dsa);
      else
         s = +1.0;
      /* parse an optional coefficient */
      if (dsa->token == T_NUMBER)
         coef = dsa->value, scan_token(dsa);
      else
         coef = 1.0;
      /* parse a variable name */
      if (dsa->token != T_NAME)
         fatal(dsa, "missing variable name");
      /* find the corresponding column */
      col = find_col(dsa, dsa->image);
      /* check if the variable is already used in the linear form */
      if (col->j < 0)
         fatal(dsa, "multiple use of variable `%s' not allowed",
            dsa->image);
      /* mark that the variable is used in the linear form */
      col->j = - col->j;
      /* create new term and add it to the linked list */
      lfe = dmp_get_atomv(dsa->lpt->pool, sizeof(LPTLFE));
      lfe->coef = s * coef;
      lfe->col = col;
      lfe->next = NULL;
      if (head == NULL)
         head = lfe;
      else
         tail->next = lfe;
      tail = lfe;
      scan_token(dsa);
      /* if the next token is a sign, here is another term */
      if (dsa->token == T_PLUS || dsa->token == T_MINUS) goto loop;
      /* clear marks of the variables used in the linear form */
      for (lfe = head; lfe != NULL; lfe = lfe->next)
      {  col = lfe->col;
         col->j = - col->j;
      }
      /* return a pointer to the linear form */
      return head;
}

/*----------------------------------------------------------------------
-- parse_objective - parse objective function.
--
-- This routine parses definition of the objective function using the
-- following syntax:
--
-- <obj sense> ::= minimize | minimum | min | maximize | maximum | max
-- <obj name> ::= <empty> | <symbolic name> :
-- <obj function> ::= <obj sense> <obj name> <linear form> */

static void parse_objective(struct dsa *dsa)
{     /* parse objective sense */
      if (dsa->token == T_MINIMIZE)
         dsa->lpt->sense = LPT_MIN, scan_token(dsa);
      else if (dsa->token == T_MAXIMIZE)
         dsa->lpt->sense = LPT_MAX, scan_token(dsa);
      else
         insist(dsa->token != dsa->token);
      /* parse objective name */
      if (dsa->token == T_NAME && dsa->line[dsa->pos] == ':')
      {  /* objective name is followed by a colon */
         insist(strlen(dsa->image) <= 16);
         strcpy(dsa->lpt->name, dsa->image);
         scan_token(dsa);
         insist(dsa->token == T_COLON);
         scan_token(dsa);
      }
      else
      {  /* objective name is not specified; use default */
         strcpy(dsa->lpt->name, "obj");
      }
      /* parse linear form */
      dsa->lpt->obj = parse_linear_form(dsa);
      return;
}

/*----------------------------------------------------------------------
-- parse_constraints - parse constraints section.
--
-- This routine parses the constraints section using the following
-- syntax:
--
-- <row name> ::= <empty> | <symbolic name> :
-- <row sense> ::= < | <= | =< | > | >= | => | =
-- <right-hand side> ::= <numeric constant> | + <numeric constant> |
--    - <numeric constant>
-- <constraint> ::= <row name> <linear form> <row sense>
--    <right-hand side>
-- <subject to> ::= subject to | such that | st | s.t. | st.
-- <constraints section> ::= <subject to> <constraint> |
--    <constraints section> <constraint> */

static void parse_constraints(struct dsa *dsa)
{     LPTROW *row;
      double s;
      /* parse the keyword 'subject to' */
      insist(dsa->token == T_SUBJECT_TO);
      scan_token(dsa);
loop: /* create new row (constraint) */
      row = dmp_get_atomv(dsa->lpt->pool, sizeof(LPTROW));
      row->i = ++(dsa->lpt->m);
      /* parse row name */
      if (dsa->token == T_NAME && dsa->line[dsa->pos] == ':')
      {  /* row name is followed by a colon */
         insist(strlen(dsa->image) <= 16);
         strcpy(row->name, dsa->image);
         if (avl_find_by_key(dsa->trow, row->name) != NULL)
            fatal(dsa, "constraint `%s' multiply defined", row->name);
         scan_token(dsa);
         insist(dsa->token == T_COLON);
         scan_token(dsa);
      }
      else
      {  /* row name is not specified; use default */
         sprintf(row->name, "r.%d", dsa->count);
      }
      /* parse linear form */
      row->ptr = parse_linear_form(dsa);
      /* parse constraint sense */
      if (dsa->token == T_LE)
         row->sense = LPT_LE, scan_token(dsa);
      else if (dsa->token == T_GE)
         row->sense = LPT_GE, scan_token(dsa);
      else if (dsa->token == T_EQ)
         row->sense = LPT_EQ, scan_token(dsa);
      else
         fatal(dsa, "missing constraint sense");
      /* parse right-hand side */
      if (dsa->token == T_PLUS)
         s = +1.0, scan_token(dsa);
      else if (dsa->token == T_MINUS)
         s = -1.0, scan_token(dsa);
      else
         s = +1.0;
      if (dsa->token != T_NUMBER)
         fatal(dsa, "missing right-hand side");
      row->rhs = s * dsa->value;
      /* add row to the linked list */
      row->next = NULL;
      if (dsa->lpt->first_row == NULL)
         dsa->lpt->first_row = row;
      else
         dsa->lpt->last_row->next = row;
      dsa->lpt->last_row = row;
      /* add row to the search tree */
      avl_insert_by_key(dsa->trow, row->name)->link = row;
      /* the rest of the current line must be empty */
      while (dsa->line[dsa->pos] == ' ') dsa->pos++;
      if (!(dsa->line[dsa->pos] == '\\' || dsa->line[dsa->pos] == '\0'))
         fatal(dsa, "invalid symbol(s) beyond right-hand side");
      scan_token(dsa);
      /* if the next token is a sign, numeric constant, or a symbolic
         name, here is another constraint */
      if (dsa->token == T_PLUS || dsa->token == T_MINUS ||
         dsa->token == T_NUMBER || dsa->token == T_NAME) goto loop;
      return;
}

/*----------------------------------------------------------------------
-- compare - compare character strings without case sensitivity.
--
-- This routine compares given character strings s1 and s2 without case
-- sensitivity. */

static int compare(char *s1, char *s2)
{     int c1, c2;
      do
      {  c1 = tolower((unsigned char)*s1++);
         c2 = tolower((unsigned char)*s2++);
      }  while (c1 && c1 == c2);
      return c1 - c2;
}

/*----------------------------------------------------------------------
-- set_lower_bound - set lower bound.
--
-- This routine sets the lower bound of a specified variable and issues
-- a warning message if the lower bound was already specified. */

static void set_lower_bound(struct dsa *dsa, LPTCOL *col, double lb)
{     if (col->lb != +DBL_MAX)
         print("%s:%d: warning: lower bound of variable `%s' redefined",
            dsa->fname, dsa->count, col->name);
      col->lb = lb;
      return;
}

/*----------------------------------------------------------------------
-- set_upper_bound - set upper bound.
--
-- This routine sets the upper bound of a specified variable and issues
-- a warning message if the upper bound was already specified. */

static void set_upper_bound(struct dsa *dsa, LPTCOL *col, double ub)
{     if (col->ub != -DBL_MAX)
         print("%s:%d: warning: upper bound of variable `%s' redefined",
            dsa->fname, dsa->count, col->name);
      col->ub = ub;
      return;
}

/*----------------------------------------------------------------------
-- parse_bounds - parse bounds section.
--
-- This routine parses the bounds section using the following syntax:
--
-- <variable> ::= <symbolic name>
-- <infinity> ::= infinity | inf
-- <bound> ::= <numeric constant> | + <numeric constant> |
--    - <numeric constant> | + <infinity> | - <infinity>
-- <lt> ::= < | <= | =<
-- <gt> ::= > | >= | =>
-- <bound definition> ::= <bound> <lt> <variable> <lt> <bound> |
--    <bound> <lt> <variable> | <variable> <lt> <bound> |
--    <variable> <gt> <bound> | <variable> = <bound> | <variable> free
-- <bounds> ::= bounds | bound
-- <bounds section> ::= <bounds> |
--    <bounds section> <bound definition> */

static void parse_bounds(struct dsa *dsa)
{     LPTCOL *col;
      int lb_flag;
      double lb, s;
      /* parse the keyword 'bounds' */
      insist(dsa->token == T_BOUNDS);
      scan_token(dsa);
loop: /* bound definition can start with a sign, numeric constant, or
         a symbolic name */
      if (!(dsa->token == T_PLUS || dsa->token == T_MINUS ||
            dsa->token == T_NUMBER || dsa->token == T_NAME)) goto done;
      /* parse bound definition */
      if (dsa->token == T_PLUS || dsa->token == T_MINUS)
      {  /* parse signed lower bound */
         lb_flag = 1;
         s = (dsa->token == T_PLUS ? +1.0 : -1.0);
         scan_token(dsa);
         if (dsa->token == T_NUMBER)
            lb = s * dsa->value, scan_token(dsa);
         else if (compare(dsa->image, "infinity") == 0 ||
                  compare(dsa->image, "inf") == 0)
         {  if (s > 0.0)
               fatal(dsa, "invalid use of `+inf' as lower bound");
            lb = -DBL_MAX, scan_token(dsa);
         }
         else
            fatal(dsa, "missing lower bound");
      }
      else if (dsa->token == T_NUMBER)
      {  /* parse unsigned lower bound */
         lb_flag = 1;
         lb = dsa->value, scan_token(dsa);
      }
      else
      {  /* lower bound is not specified */
         lb_flag = 0;
      }
      /* parse the token that should follow the lower bound */
      if (lb_flag)
      {  if (dsa->token != T_LE)
            fatal(dsa, "missing `<', `<=', or `=<' after lower bound");
         scan_token(dsa);
      }
      /* parse variable name */
      if (dsa->token != T_NAME)
         fatal(dsa, "missing variable name");
      col = find_col(dsa, dsa->image);
      /* set lower bound */
      if (lb_flag) set_lower_bound(dsa, col, lb);
      scan_token(dsa);
      /* parse the context that follows the variable name */
      if (dsa->token == T_LE)
      {  /* parse upper bound */
         scan_token(dsa);
         if (dsa->token == T_PLUS || dsa->token == T_MINUS)
         {  /* parse signed upper bound */
            s = (dsa->token == T_PLUS ? +1.0 : -1.0);
            scan_token(dsa);
            if (dsa->token == T_NUMBER)
            {  set_upper_bound(dsa, col, s * dsa->value);
               scan_token(dsa);
            }
            else if (compare(dsa->image, "infinity") == 0 ||
                     compare(dsa->image, "inf") == 0)
            {  if (s < 0.0)
                  fatal(dsa, "invalid use of `-inf' as upper bound");
               set_upper_bound(dsa, col, +DBL_MAX);
               scan_token(dsa);
            }
            else
               fatal(dsa, "missing upper bound");
         }
         else if (dsa->token == T_NUMBER)
         {  /* parse unsigned upper bound */
            set_upper_bound(dsa, col, dsa->value);
            scan_token(dsa);
         }
         else
            fatal(dsa, "missing upper bound");
      }
#if 1 /* 07/XI-02 */
      else if (dsa->token == T_GE)
      {  /* parse lower bound */
         if (lb_flag)
         {  /* the context '... <= x >= ...' is invalid */
            fatal(dsa, "invalid bound definition");
         }
         scan_token(dsa);
         if (dsa->token == T_PLUS || dsa->token == T_MINUS)
         {  /* parse signed lower bound */
            s = (dsa->token == T_PLUS ? +1.0 : -1.0);
            scan_token(dsa);
            if (dsa->token == T_NUMBER)
            {  set_lower_bound(dsa, col, s * dsa->value);
               scan_token(dsa);
            }
            else if (compare(dsa->image, "infinity") == 0 ||
                     compare(dsa->image, "inf") == 0)
            {  if (s > 0.0)
                  fatal(dsa, "invalid use of `+inf' as lower bound");
               set_lower_bound(dsa, col, -DBL_MAX);
               scan_token(dsa);
            }
            else
               fatal(dsa, "missing lower bound");
         }
         else if (dsa->token == T_NUMBER)
         {  /* parse unsigned lower bound */
            set_lower_bound(dsa, col, dsa->value);
            scan_token(dsa);
         }
         else
            fatal(dsa, "missing lower bound");
      }
#endif
      else if (dsa->token == T_EQ)
      {  /* parse fixed value */
         if (lb_flag)
         {  /* the context '... <= x = ...' is invalid */
            fatal(dsa, "invalid bound definition");
         }
         scan_token(dsa);
         if (dsa->token == T_PLUS || dsa->token == T_MINUS)
         {  /* parse signed fixed value */
            s = (dsa->token == T_PLUS ? +1.0 : -1.0);
            scan_token(dsa);
            if (dsa->token == T_NUMBER)
            {  set_lower_bound(dsa, col, s * dsa->value);
               set_upper_bound(dsa, col, s * dsa->value);
               scan_token(dsa);
            }
            else
               fatal(dsa, "missing fixed value");
         }
         else if (dsa->token == T_NUMBER)
         {  /* parse unsigned fixed value */
            set_lower_bound(dsa, col, dsa->value);
            set_upper_bound(dsa, col, dsa->value);
            scan_token(dsa);
         }
         else
            fatal(dsa, "missing fixed value");
      }
      else if (compare(dsa->image, "free") == 0)
      {  /* parse the keyword 'free' */
         if (lb_flag)
         {  /* the context '... <= x free ...' is invalid */
            fatal(dsa, "invalid bound definition");
         }
         set_lower_bound(dsa, col, -DBL_MAX);
         set_upper_bound(dsa, col, +DBL_MAX);
         scan_token(dsa);
      }
      else if (!lb_flag)
      {  /* neither lower nor upper bounds are specified */
         fatal(dsa, "invalid bound definition");
      }
      goto loop;
done: return;
}

/*----------------------------------------------------------------------
-- parse_integer - parse general, integer, or binary section.
--
-- <variable> ::= <symbolic name>
-- <general> ::= general | generals | gen
-- <integer> ::= integer | integers | int
-- <binary> ::= binary | binaries | bin
-- <section head> ::= <general> <integer> <binary>
-- <additional section> ::= <section head> |
--    <additional section> <variable> */

static void parse_integer(struct dsa *dsa)
{     LPTCOL *col;
      int kind;
      /* parse the keyword 'general', 'integer', or 'binary' */
      if (dsa->token == T_GENERAL)
         kind = LPT_INT, scan_token(dsa);
      else if (dsa->token == T_INTEGER)
         kind = LPT_INT, scan_token(dsa);
      else if (dsa->token == T_BINARY)
         kind = LPT_BIN, scan_token(dsa);
      else
         insist(dsa->token != dsa->token);
      /* parse list of variables (may be empty) */
      while (dsa->token == T_NAME)
      {  /* find the corresponding column */
         col = find_col(dsa, dsa->image);
         /* change kind of the variable */
         col->kind = kind;
         /* set 0-1 bounds for the binary variable */
         if (kind == LPT_BIN)
         {  set_lower_bound(dsa, col, 0.0);
            set_upper_bound(dsa, col, 1.0);
         }
         scan_token(dsa);
      }
      return;
}

/*----------------------------------------------------------------------
-- lpt_read_prob - read LP/MIP problem in CPLEX LP format.
--
-- *Synopsis*
--
-- #include "glplpt.h"
-- LPT *lpt_read_prob(char *fname);
--
-- *Description*
--
-- The routine lpt_read_prob reads an LP/MIP problem from a text file,
-- whose name is the character string fname, using CPLEX LP format.
--
-- *Returns*
--
-- The routine lpt_read_prob returns a pointer to the data block, which
-- contains the problem components. In case of error the routine prints
-- an appropriate error message and returns a null pointer. */

LPT *lpt_read_prob(char *fname)
{     LPT *lpt;
      struct dsa my_dsa, *dsa = &my_dsa;
      int err = 0;
      print("lpt_read_prob: reading LP data from `%s'...", fname);
      /* try to open input text file */
      dsa->fp = ufopen(fname, "r");
      if (dsa->fp == NULL)
      {  print("lpt_read_prob: unable to open `%s' - %s", fname,
            strerror(errno));
         return NULL;
      }
      /* create problem data block */
      lpt = umalloc(sizeof(LPT));
      lpt->pool = dmp_create_pool(0);
      lpt->name[0] = '\0';
      lpt->sense = 0;
      lpt->obj = NULL;
      lpt->m = lpt->n = 0;
      lpt->first_row = lpt->last_row = NULL;
      lpt->first_col = lpt->last_col = NULL;
      /* initialize dynamic storage area */
      dsa->lpt = lpt;
      dsa->trow = avl_create_tree(NULL, avl_strcmp);
      dsa->tcol = avl_create_tree(NULL, avl_strcmp);
      dsa->fname = fname;
      dsa->fp = dsa->fp;
      if (setjmp(dsa->jump))
      {  err = 1;
         goto trap;
      }
      dsa->count = 0;
      dsa->line[0] = '\0';
      dsa->pos = 0;
      dsa->token = -1;
      dsa->image[0] = '\0';
      dsa->imlen = 0;
      dsa->value = 0.0;
      /* scan the very first token */
      scan_token(dsa);
      /* parse definition of the objective function */
      if (!(dsa->token == T_MINIMIZE || dsa->token == T_MAXIMIZE))
         fatal(dsa, "`minimize' or `maximize' keyword missing");
      parse_objective(dsa);
      /* parse constraints section */
      if (dsa->token != T_SUBJECT_TO)
         fatal(dsa, "constraints section missing");
      parse_constraints(dsa);
      /* parse optional bounds section */
      if (dsa->token == T_BOUNDS) parse_bounds(dsa);
      /* parse optional general, integer, and binary sections */
      while (dsa->token == T_GENERAL ||
             dsa->token == T_INTEGER ||
             dsa->token == T_BINARY) parse_integer(dsa);
      /* check for the keyword 'end' */
      if (dsa->token == T_END)
         scan_token(dsa);
      else if (dsa->token == T_EOF)
         print("%s:%d: warning: keyword `end' missing", dsa->fname,
            dsa->count);
      else
         fatal(dsa, "symbol `%s' in wrong position", dsa->image);
      /* nothing must follow the keyword 'end' (except comments) */
      if (dsa->token != T_EOF)
         fatal(dsa, "extra symbol(s) detected beyond `end'");
      /* set default lower and upper bounds */
      {  LPTCOL *col;
         for (col = dsa->lpt->first_col; col; col = col->next)
         {  if (col->lb == +DBL_MAX) col->lb = 0.0;
            if (col->ub == -DBL_MAX) col->ub = +DBL_MAX;
         }
      }
      /* print some statistics */
      print("lpt_read_prob: %d variable%s, %d constraint%s",
         dsa->tcol->size, dsa->tcol->size == 1 ? "" : "s",
         dsa->trow->size, dsa->trow->size == 1 ? "" : "s");
      print("lpt_read_prob: %d lines were read", dsa->count);
trap: /* free auxiliary resources */
      avl_delete_tree(dsa->trow);
      avl_delete_tree(dsa->tcol);
      ufclose(dsa->fp);
      /* if an error occurred, delete the problem data block */
      if (err)
      {  dmp_delete_pool(lpt->pool);
         ufree(lpt), lpt = NULL;
      }
      /* return to the calling program */
      return lpt;
}

/*----------------------------------------------------------------------
-- lpt_free_prob - free LP/MIP problem data block.
--
-- *Synopsis*
--
-- #include "glplpt.h"
-- void lpt_free_prob(LPT *lpt);
--
-- *Description*
--
-- The routine lpt_free_prob frees all the memory allocated to LP/MIP
-- problem data block, which the parameter lpt points to. */

void lpt_free_prob(LPT *lpt)
{     dmp_delete_pool(lpt->pool);
      ufree(lpt);
      return;
}

/* eof */
