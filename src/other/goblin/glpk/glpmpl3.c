/* glpmpl3.c */

/*----------------------------------------------------------------------
-- GLPMPL -- GNU MathProg Translator.
--
-- Author:        Andrew Makhorin, Department for Applied Informatics,
--                Moscow Aviation Institute, Moscow, Russia.
-- E-mail:        <mao@mai2.rcnet.ru>
-- Date written:  Spring 2003.
-- Last modified: May 2003.
--
-- This file is a part of GLPK (GNU Linear Programming Kit).
--
-- Copyright (C) 2000, 2001, 2002, 2003 Andrew Makhorin, Department
-- for Applied Informatics, Moscow Aviation Institute, Moscow, Russia.
-- All rights reserved.
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
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include "glplib.h"
#include "glpmpl.h"

/**********************************************************************/
/* * *                   FLOATING-POINT NUMBERS                   * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- fp_add - floating-point addition.
--
-- This routine computes the sum x + y. */

double fp_add(MPL *mpl, double x, double y)
{     if (x > 0.0 && y > 0.0 && x > + 0.999 * DBL_MAX - y ||
          x < 0.0 && y < 0.0 && x < - 0.999 * DBL_MAX - y)
         error(mpl, "%.*g + %.*g; floating-point overflow",
            DBL_DIG, x, DBL_DIG, y);
      return x + y;
}

/*----------------------------------------------------------------------
-- fp_sub - floating-point subtraction.
--
-- This routine computes the difference x - y. */

double fp_sub(MPL *mpl, double x, double y)
{     if (x > 0.0 && y < 0.0 && x > + 0.999 * DBL_MAX + y ||
          x < 0.0 && y > 0.0 && x < - 0.999 * DBL_MAX + y)
         error(mpl, "%.*g - %.*g; floating-point overflow",
            DBL_DIG, x, DBL_DIG, y);
      return x - y;
}

/*----------------------------------------------------------------------
-- fp_less - floating-point non-negative subtraction.
--
-- This routine computes the non-negative difference max(0, x - y). */

double fp_less(MPL *mpl, double x, double y)
{     if (x < y) return 0.0;
      if (x > 0.0 && y < 0.0 && x > + 0.999 * DBL_MAX + y)
         error(mpl, "%.*g less %.*g; floating-point overflow",
            DBL_DIG, x, DBL_DIG, y);
      return x - y;
}

/*----------------------------------------------------------------------
-- fp_mul - floating-point multiplication.
--
-- This routine computes the product x * y. */

double fp_mul(MPL *mpl, double x, double y)
{     if (fabs(y) > 1.0 && fabs(x) > (0.999 * DBL_MAX) / fabs(y))
         error(mpl, "%.*g * %.*g; floating-point overflow",
            DBL_DIG, x, DBL_DIG, y);
      return x * y;
}

/*----------------------------------------------------------------------
-- fp_div - floating-point division.
--
-- This routine computes the quotient x / y. */

double fp_div(MPL *mpl, double x, double y)
{     if (fabs(y) < DBL_MIN)
         error(mpl, "%.*g / %.*g; floating-point zero divide",
            DBL_DIG, x, DBL_DIG, y);
      if (fabs(y) < 1.0 && fabs(x) > (0.999 * DBL_MAX) * fabs(y))
         error(mpl, "%.*g / %.*g; floating-point overflow",
            DBL_DIG, x, DBL_DIG, y);
      return x / y;
}

/*----------------------------------------------------------------------
-- fp_idiv - floating-point quotient of exact division.
--
-- This routine computes the quotient of exact division x div y. */

double fp_idiv(MPL *mpl, double x, double y)
{     if (fabs(y) < DBL_MIN)
         error(mpl, "%.*g div %.*g; floating-point zero divide",
            DBL_DIG, x, DBL_DIG, y);
      if (fabs(y) < 1.0 && fabs(x) > (0.999 * DBL_MAX) * fabs(y))
         error(mpl, "%.*g div %.*g; floating-point overflow",
            DBL_DIG, x, DBL_DIG, y);
      x /= y;
      return x > 0.0 ? floor(x) : x < 0.0 ? ceil(x) : 0.0;
}

/*----------------------------------------------------------------------
-- fp_mod - floating-point remainder of exact division.
--
-- This routine computes the remainder of exact division x mod y.
--
-- NOTE: By definition x mod y = x - y * floor(x / y). */

double fp_mod(MPL *mpl, double x, double y)
{     double r;
      insist(mpl == mpl);
      if (x == 0.0)
         r = 0.0;
      else if (y == 0.0)
         r = x;
      else
      {  r = fmod(fabs(x), fabs(y));
         if (r != 0.0)
         {  if (x < 0.0) r = - r;
            if (x > 0.0 && y < 0.0 || x < 0.0 && y > 0.0) r += y;
         }
      }
      return r;
}

/*----------------------------------------------------------------------
-- fp_power - floating-point exponentiation (raise to power).
--
-- This routine computes the exponentiation x ** y. */

double fp_power(MPL *mpl, double x, double y)
{     double r;
      if (x == 0.0 && y <= 0.0 || x < 0.0 && y != floor(y))
         error(mpl, "%.*g ** %.*g; result undefined",
            DBL_DIG, x, DBL_DIG, y);
      if (fabs(x) > 1.0 && y > +1.0 &&
            +log(fabs(x)) > (0.999 * log(DBL_MAX)) / y ||
          fabs(x) < 1.0 && y < -1.0 &&
            +log(fabs(x)) < (0.999 * log(DBL_MAX)) / y)
         error(mpl, "%.*g ** %.*g; floating-point overflow",
            DBL_DIG, x, DBL_DIG, y);
      if (fabs(x) > 1.0 && y < -1.0 &&
            -log(fabs(x)) < (0.999 * log(DBL_MAX)) / y ||
          fabs(x) < 1.0 && y > +1.0 &&
            -log(fabs(x)) > (0.999 * log(DBL_MAX)) / y)
         r = 0.0;
      else
         r = pow(x, y);
      return r;
}

/*----------------------------------------------------------------------
-- fp_exp - floating-point base-e exponential.
--
-- This routine computes the base-e exponential e ** x. */

double fp_exp(MPL *mpl, double x)
{     if (x > 0.999 * log(DBL_MAX))
         error(mpl, "exp(%.*g); floating-point overflow", DBL_DIG, x);
      return exp(x);
}

/*----------------------------------------------------------------------
-- fp_log - floating-point natural logarithm.
--
-- This routine computes the natural logarithm log x. */

double fp_log(MPL *mpl, double x)
{     if (x <= 0.0)
         error(mpl, "log(%.*g); non-positive argument", DBL_DIG, x);
      return log(x);
}

/*----------------------------------------------------------------------
-- fp_log10 - floating-point common (decimal) logarithm.
--
-- This routine computes the common (decimal) logarithm lg x. */

double fp_log10(MPL *mpl, double x)
{     if (x <= 0.0)
         error(mpl, "log10(%.*g); non-positive argument", DBL_DIG, x);
      return log10(x);
}

/*----------------------------------------------------------------------
-- fp_sqrt - floating-point square root.
--
-- This routine computes the square root x ** 0.5. */

double fp_sqrt(MPL *mpl, double x)
{     if (x < 0.0)
         error(mpl, "sqrt(%.*g); negative argument", DBL_DIG, x);
      return sqrt(x);
}

/**********************************************************************/
/* * *                SEGMENTED CHARACTER STRINGS                 * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- create_string - create character string.
--
-- This routine creates a segmented character string, which is exactly
-- equivalent to specified character string. */

STRING *create_string
(     MPL *mpl,
      char buf[MAX_LENGTH+1]  /* not changed */
)
{     STRING *head, *tail;
      int i, j;
      insist(buf != NULL);
      insist(strlen(buf) <= MAX_LENGTH+1);
      head = tail = dmp_get_atom(mpl->strings);
      for (i = j = 0; ; i++)
      {  if ((tail->seg[j++] = buf[i]) == '\0') break;
         if (j == STRSEG_SIZE)
            tail = (tail->next = dmp_get_atom(mpl->strings)), j = 0;
      }
      tail->next = NULL;
      return head;
}

/*----------------------------------------------------------------------
-- copy_string - make copy of character string.
--
-- This routine returns an exact copy of segmented character string. */

STRING *copy_string
(     MPL *mpl,
      STRING *str             /* not changed */
)
{     STRING *head, *tail;
      insist(str != NULL);
      head = tail = dmp_get_atom(mpl->strings);
      for (; str != NULL; str = str->next)
      {  memcpy(tail->seg, str->seg, STRSEG_SIZE);
         if (str->next != NULL)
            tail = (tail->next = dmp_get_atom(mpl->strings));
      }
      tail->next = NULL;
      return head;
}

/*----------------------------------------------------------------------
-- compare_strings - compare one character string with another.
--
-- This routine compares one segmented character strings with another
-- and returns the result of comparison as follows:
--
-- = 0 - both strings are identical;
-- < 0 - the first string precedes the second one;
-- > 0 - the first string follows the second one. */

int compare_strings
(     MPL *mpl,
      STRING *str1,           /* not changed */
      STRING *str2            /* not changed */
)
{     int j, c1, c2;
      insist(mpl == mpl);
      for (;; str1 = str1->next, str2 = str2->next)
      {  insist(str1 != NULL);
         insist(str2 != NULL);
         for (j = 0; j < STRSEG_SIZE; j++)
         {  c1 = (unsigned char)str1->seg[j];
            c2 = (unsigned char)str2->seg[j];
            if (c1 < c2) return -1;
            if (c1 > c2) return +1;
            if (c1 == '\0') goto done;
         }
      }
done: return 0;
}

/*----------------------------------------------------------------------
-- fetch_string - extract content of character string.
--
-- This routine returns a character string, which is exactly equivalent
-- to specified segmented character string. */

char *fetch_string
(     MPL *mpl,
      STRING *str,            /* not changed */
      char buf[MAX_LENGTH+1]  /* modified */
)
{     int i, j;
      insist(mpl == mpl);
      insist(buf != NULL);
      for (i = 0; ; str = str->next)
      {  insist(str != NULL);
         for (j = 0; j < STRSEG_SIZE; j++)
            if ((buf[i++] = str->seg[j]) == '\0') goto done;
      }
done: insist(strlen(buf) <= MAX_LENGTH);
      return buf;
}

/*----------------------------------------------------------------------
-- delete_string - delete character string.
--
-- This routine deletes specified segmented character string. */

void delete_string
(     MPL *mpl,
      STRING *str             /* destroyed */
)
{     STRING *temp;
      insist(str != NULL);
      while (str != NULL)
      {  temp = str;
         str = str->next;
         dmp_free_atom(mpl->strings, temp);
      }
      return;
}

/**********************************************************************/
/* * *                          SYMBOLS                           * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- create_symbol_num - create symbol of numeric type.
--
-- This routine creates a symbol, which has a numeric value specified
-- as floating-point number. */

SYMBOL *create_symbol_num(MPL *mpl, double num)
{     SYMBOL *sym;
      sym = dmp_get_atom(mpl->symbols);
      sym->num = num;
      sym->str = NULL;
      return sym;
}

/*----------------------------------------------------------------------
-- create_symbol_str - create symbol of abstract type.
--
-- This routine creates a symbol, which has an abstract value specified
-- as segmented character string. */

SYMBOL *create_symbol_str
(     MPL *mpl,
      STRING *str             /* destroyed */
)
{     SYMBOL *sym;
      insist(str != NULL);
      sym = dmp_get_atom(mpl->symbols);
      sym->num = 0.0;
      sym->str = str;
      return sym;
}

/*----------------------------------------------------------------------
-- copy_symbol - make copy of symbol.
--
-- This routine returns an exact copy of symbol. */

SYMBOL *copy_symbol
(     MPL *mpl,
      SYMBOL *sym             /* not changed */
)
{     SYMBOL *copy;
      insist(sym != NULL);
      copy = dmp_get_atom(mpl->symbols);
      if (sym->str == NULL)
      {  copy->num = sym->num;
         copy->str = NULL;
      }
      else
      {  copy->num = 0.0;
         copy->str = copy_string(mpl, sym->str);
      }
      return copy;
}

/*----------------------------------------------------------------------
-- compare_symbols - compare one symbol with another.
--
-- This routine compares one symbol with another and returns the result
-- of comparison as follows:
--
-- = 0 - both symbols are identical;
-- < 0 - the first symbol precedes the second one;
-- > 0 - the first symbol follows the second one.
--
-- Note that the linear order, in which symbols follow each other, is
-- implementation-dependent. It may be not an alphabetical order. */

int compare_symbols
(     MPL *mpl,
      SYMBOL *sym1,           /* not changed */
      SYMBOL *sym2            /* not changed */
)
{     insist(sym1 != NULL);
      insist(sym2 != NULL);
      /* let all numeric quantities precede all symbolic quantities */
      if (sym1->str == NULL && sym2->str == NULL)
      {  if (sym1->num < sym2->num) return -1;
         if (sym1->num > sym2->num) return +1;
         return 0;
      }
      if (sym1->str == NULL) return -1;
      if (sym2->str == NULL) return +1;
      return compare_strings(mpl, sym1->str, sym2->str);
}

/*----------------------------------------------------------------------
-- delete_symbol - delete symbol.
--
-- This routine deletes specified symbol. */

void delete_symbol
(     MPL *mpl,
      SYMBOL *sym             /* destroyed */
)
{     insist(sym != NULL);
      if (sym->str != NULL) delete_string(mpl, sym->str);
      dmp_free_atom(mpl->symbols, sym);
      return;
}

/*----------------------------------------------------------------------
-- format_symbol - format symbol for displaying or printing.
--
-- This routine converts specified symbol to a charater string, which
-- is suitable for displaying or printing.
--
-- The resultant string is never longer than 255 characters. If it gets
-- longer, it is truncated from the right and appended by dots. */

char *format_symbol
(     MPL *mpl,
      SYMBOL *sym             /* not changed */
)
{     char *buf = mpl->sym_buf;
      insist(sym != NULL);
      if (sym->str == NULL)
         sprintf(buf, "%.*g", DBL_DIG, sym->num);
      else
      {  char str[MAX_LENGTH+1];
         int quoted, j, len;
         fetch_string(mpl, sym->str, str);
         if (!(isalpha((unsigned char)str[0]) || str[0] == '_'))
            quoted = 1;
         else
         {  quoted = 0;
            for (j = 1; str[j] != '\0'; j++)
            {  if (!(isalnum((unsigned char)str[j]) ||
                     strchr("+-._", (unsigned char)str[j]) != NULL))
               {  quoted = 1;
                  break;
               }
            }
         }
#        define safe_append(c) \
            (void)(len < 255 ? (buf[len++] = (char)(c)) : 0)
         buf[0] = '\0', len = 0;
         if (quoted) safe_append('\'');
         for (j = 0; str[j] != '\0'; j++)
         {  if (quoted && str[j] == '\'') safe_append('\'');
            safe_append(str[j]);
         }
         if (quoted) safe_append('\'');
#        undef safe_append
         buf[len] = '\0';
         if (len == 255) strcpy(buf+252, "...");
      }
      insist(strlen(buf) <= 255);
      return buf;
}

/*----------------------------------------------------------------------
-- concat_symbols - concatenate one symbol with another.
--
-- This routine concatenates values of two given symbols and assigns
-- the resultant character string to a new symbol, which is returned on
-- exit. Both original symbols are destroyed. */

SYMBOL *concat_symbols
(     MPL *mpl,
      SYMBOL *sym1,           /* destroyed */
      SYMBOL *sym2            /* destroyed */
)
{     char str1[MAX_LENGTH+1], str2[MAX_LENGTH+1];
      insist(MAX_LENGTH >= DBL_DIG + DBL_DIG);
      if (sym1->str == NULL)
         sprintf(str1, "%.*g", DBL_DIG, sym1->num);
      else
         fetch_string(mpl, sym1->str, str1);
      if (sym2->str == NULL)
         sprintf(str2, "%.*g", DBL_DIG, sym2->num);
      else
         fetch_string(mpl, sym2->str, str2);
      if (strlen(str1) + strlen(str2) > MAX_LENGTH)
      {  char buf[255+1];
         strcpy(buf, format_symbol(mpl, sym1));
         insist(strlen(buf) < sizeof(buf));
         error(mpl, "%s & %s; resultant symbol exceeds %d characters",
            buf, format_symbol(mpl, sym2), MAX_LENGTH);
      }
      delete_symbol(mpl, sym1);
      delete_symbol(mpl, sym2);
      return create_symbol_str(mpl, create_string(mpl, strcat(str1,
         str2)));
}

/**********************************************************************/
/* * *                          N-TUPLES                          * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- create_tuple - create n-tuple.
--
-- This routine creates a n-tuple, which initially has no components,
-- i.e. which is 0-tuple. */

TUPLE *create_tuple(MPL *mpl)
{     TUPLE *tuple;
      insist(mpl == mpl);
      tuple = NULL;
      return tuple;
}

/*----------------------------------------------------------------------
-- expand_tuple - append symbol to n-tuple.
--
-- This routine expands n-tuple appending to it a given symbol, which
-- becomes its new last component. */

TUPLE *expand_tuple
(     MPL *mpl,
      TUPLE *tuple,           /* destroyed */
      SYMBOL *sym             /* destroyed */
)
{     TUPLE *tail, *temp;
      insist(sym != NULL);
      /* create a new component */
      tail = dmp_get_atom(mpl->tuples);
      tail->sym = sym;
      tail->next = NULL;
      /* and append it to the component list */
      if (tuple == NULL)
         tuple = tail;
      else
      {  for (temp = tuple; temp->next != NULL; temp = temp->next);
         temp->next = tail;
      }
      return tuple;
}

/*----------------------------------------------------------------------
-- tuple_dimen - determine dimension of n-tuple.
--
-- This routine returns dimension of n-tuple, i.e. number of components
-- in the n-tuple. */

int tuple_dimen
(     MPL *mpl,
      TUPLE *tuple            /* not changed */
)
{     TUPLE *temp;
      int dim = 0;
      insist(mpl == mpl);
      for (temp = tuple; temp != NULL; temp = temp->next) dim++;
      return dim;
}

/*----------------------------------------------------------------------
-- copy_tuple - make copy of n-tuple.
--
-- This routine returns an exact copy of n-tuple. */

TUPLE *copy_tuple
(     MPL *mpl,
      TUPLE *tuple            /* not changed */
)
{     TUPLE *head, *tail;
      if (tuple == NULL)
         head = NULL;
      else
      {  head = tail = dmp_get_atom(mpl->tuples);
         for (; tuple != NULL; tuple = tuple->next)
         {  insist(tuple->sym != NULL);
            tail->sym = copy_symbol(mpl, tuple->sym);
            if (tuple->next != NULL)
               tail = (tail->next = dmp_get_atom(mpl->tuples));
         }
         tail->next = NULL;
      }
      return head;
}

/*----------------------------------------------------------------------
-- compare_tuples - compare one n-tuple with another.
--
-- This routine compares two given n-tuples, which must have the same
-- dimension (not checked for the sake of efficiency), and returns one
-- of the following codes:
--
-- = 0 - both n-tuples are identical;
-- < 0 - the first n-tuple precedes the second one;
-- > 0 - the first n-tuple follows the second one.
--
-- Note that the linear order, in which n-tuples follow each other, is
-- implementation-dependent. It may be not an alphabetical order. */

int compare_tuples
(     MPL *mpl,
      TUPLE *tuple1,          /* not changed */
      TUPLE *tuple2           /* not changed */
)
{     TUPLE *item1, *item2;
      int ret;
      insist(mpl == mpl);
      for (item1 = tuple1, item2 = tuple2; item1 != NULL;
           item1 = item1->next, item2 = item2->next)
      {  insist(item2 != NULL);
         insist(item1->sym != NULL);
         insist(item2->sym != NULL);
         ret = compare_symbols(mpl, item1->sym, item2->sym);
         if (ret != 0) return ret;
      }
      insist(item2 == NULL);
      return 0;
}

/*----------------------------------------------------------------------
-- build_subtuple - build subtuple of given n-tuple.
--
-- This routine builds subtuple, which consists of first dim components
-- of given n-tuple. */

TUPLE *build_subtuple
(     MPL *mpl,
      TUPLE *tuple,           /* not changed */
      int dim
)
{     TUPLE *head, *temp;
      int j;
      head = create_tuple(mpl);
      for (j = 1, temp = tuple; j <= dim; j++, temp = temp->next)
      {  insist(temp != NULL);
         head = expand_tuple(mpl, head, copy_symbol(mpl, temp->sym));
      }
      return head;
}

/*----------------------------------------------------------------------
-- delete_tuple - delete n-tuple.
--
-- This routine deletes specified n-tuple. */

void delete_tuple
(     MPL *mpl,
      TUPLE *tuple            /* destroyed */
)
{     TUPLE *temp;
      while (tuple != NULL)
      {  temp = tuple;
         tuple = temp->next;
         insist(temp->sym != NULL);
         delete_symbol(mpl, temp->sym);
         dmp_free_atom(mpl->tuples, temp);
      }
      return;
}

/*----------------------------------------------------------------------
-- format_tuple - format n-tuple for displaying or printing.
--
-- This routine converts specified n-tuple to a character string, which
-- is suitable for displaying or printing.
--
-- The resultant string is never longer than 255 characters. If it gets
-- longer, it is truncated from the right and appended by dots. */

char *format_tuple
(     MPL *mpl,
      int c,
      TUPLE *tuple            /* not changed */
)
{     TUPLE *temp;
      int dim, j, len;
      char *buf = mpl->tup_buf, str[255+1], *save;
#     define safe_append(c) \
         (void)(len < 255 ? (buf[len++] = (char)(c)) : 0)
      buf[0] = '\0', len = 0;
      dim = tuple_dimen(mpl, tuple);
      if (c == '[' && dim > 0) safe_append('[');
      if (c == '(' && dim > 1) safe_append('(');
      for (temp = tuple; temp != NULL; temp = temp->next)
      {  if (temp != tuple) safe_append(',');
         insist(temp->sym != NULL);
         save = mpl->sym_buf;
         mpl->sym_buf = str;
         format_symbol(mpl, temp->sym);
         mpl->sym_buf = save;
         insist(strlen(str) < sizeof(str));
         for (j = 0; str[j] != '\0'; j++) safe_append(str[j]);
      }
      if (c == '[' && dim > 0) safe_append(']');
      if (c == '(' && dim > 1) safe_append(')');
#     undef safe_append
      buf[len] = '\0';
      if (len == 255) strcpy(buf+252, "...");
      insist(strlen(buf) <= 255);
      return buf;
}

/**********************************************************************/
/* * *                       ELEMENTAL SETS                       * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- create_elemset - create elemental set.
--
-- This routine creates an elemental set, whose members are n-tuples of
-- specified dimension. Being created the set is initially empty. */

ELEMSET *create_elemset(MPL *mpl, int dim)
{     ELEMSET *set;
      insist(dim > 0);
      set = create_array(mpl, A_NONE, dim);
      return set;
}

/*----------------------------------------------------------------------
-- find_tuple - check if elemental set contains given n-tuple.
--
-- This routine finds given n-tuple in specified elemental set in order
-- to check if the set contains that n-tuple. If the n-tuple is found,
-- the routine returns pointer to corresponding array member. Otherwise
-- null pointer is returned. */

MEMBER *find_tuple
(     MPL *mpl,
      ELEMSET *set,           /* not changed */
      TUPLE *tuple            /* not changed */
)
{     insist(set != NULL);
      insist(set->type == A_NONE);
      insist(set->dim == tuple_dimen(mpl, tuple));
      return find_member(mpl, set, tuple);
}

/*----------------------------------------------------------------------
-- add_tuple - add new n-tuple to elemental set.
--
-- This routine adds given n-tuple to specified elemental set.
--
-- For the sake of efficiency this routine doesn't check whether the
-- set already contains the same n-tuple or not. Therefore the calling
-- program should use the routine find_tuple (if necessary) in order to
-- make sure that the given n-tuple is not contained in the set, since
-- duplicate n-tuples within the same set are not allowed. */

MEMBER *add_tuple
(     MPL *mpl,
      ELEMSET *set,           /* modified */
      TUPLE *tuple            /* destroyed */
)
{     MEMBER *memb;
      insist(set != NULL);
      insist(set->type == A_NONE);
      insist(set->dim == tuple_dimen(mpl, tuple));
      memb = add_member(mpl, set, tuple);
      memb->value.none = NULL;
      return memb;
}

/*----------------------------------------------------------------------
-- check_then_add - check and add new n-tuple to elemental set.
--
-- This routine is equivalent to the routine add_tuple except that it
-- does check for duplicate n-tuples. */

MEMBER *check_then_add
(     MPL *mpl,
      ELEMSET *set,           /* modified */
      TUPLE *tuple            /* destroyed */
)
{     if (find_tuple(mpl, set, tuple) != NULL)
         error(mpl, "duplicate tuple %s detected", format_tuple(mpl,
            '(', tuple));
      return add_tuple(mpl, set, tuple);
}

/*----------------------------------------------------------------------
-- copy_elemset - make copy of elemental set.
--
-- This routine makes an exact copy of elemental set. */

ELEMSET *copy_elemset
(     MPL *mpl,
      ELEMSET *set            /* not changed */
)
{     ELEMSET *copy;
      MEMBER *memb;
      insist(set != NULL);
      insist(set->type == A_NONE);
      insist(set->dim > 0);
      copy = create_elemset(mpl, set->dim);
      for (memb = set->head; memb != NULL; memb = memb->next)
         add_tuple(mpl, copy, copy_tuple(mpl, memb->tuple));
      return copy;
}

/*----------------------------------------------------------------------
-- delete_elemset - delete elemental set.
--
-- This routine deletes specified elemental set. */

void delete_elemset
(     MPL *mpl,
      ELEMSET *set            /* destroyed */
)
{     insist(set != NULL);
      insist(set->type == A_NONE);
      delete_array(mpl, set);
      return;
}

/*----------------------------------------------------------------------
-- arelset_size - compute size of "arithmetic" elemental set.
--
-- This routine computes the size of "arithmetic" elemental set, which
-- is specified in the form of arithmetic progression:
--
--    { t0 .. tf by dt }.
--
-- The size is computed using the formula:
--
--    n = max(0, floor((tf - t0) / dt) + 1). */

int arelset_size(MPL *mpl, double t0, double tf, double dt)
{     double temp;
      if (dt == 0.0)
         error(mpl, "%.*g .. %.*g by %.*g; zero stride not allowed",
            DBL_DIG, t0, DBL_DIG, tf, DBL_DIG, dt);
      if (tf > 0.0 && t0 < 0.0 && tf > + 0.999 * DBL_MAX + t0)
         temp = +DBL_MAX;
      else if (tf < 0.0 && t0 > 0.0 && tf < - 0.999 * DBL_MAX + t0)
         temp = -DBL_MAX;
      else
         temp = tf - t0;
      if (fabs(dt) < 1.0 && fabs(temp) > (0.999 * DBL_MAX) * fabs(dt))
      {  if (temp > 0.0 && dt > 0.0 || temp < 0.0 && dt < 0.0)
            temp = +DBL_MAX;
         else
            temp = 0.0;
      }
      else
      {  temp = floor(temp / dt) + 1.0;
         if (temp < 0.0) temp = 0.0;
      }
      insist(temp >= 0.0);
      if (temp > (double)(INT_MAX - 1))
         error(mpl, "%.*g .. %.*g by %.*g; set too large",
            DBL_DIG, t0, DBL_DIG, tf, DBL_DIG, dt);
      return (int)(temp + 0.5);
}

/*----------------------------------------------------------------------
-- arelset_member - compute member of "arithmetic" elemental set.
--
-- This routine returns a numeric value of symbol, which is equivalent
-- to j-th member of given "arithmetic" elemental set specified in the
-- form of arithmetic progression:
--
--    { t0 .. tf by dt }.
--
-- The symbol value is computed with the formula:
--
--    j-th member = t0 + (j - 1) * dt,
--
-- The number j must satisfy to the restriction 1 <= j <= n, where n is
-- the set size computed by the routine arelset_size. */

double arelset_member(MPL *mpl, double t0, double tf, double dt, int j)
{     insist(1 <= j && j <= arelset_size(mpl, t0, tf, dt));
      return t0 + (double)(j - 1) * dt;
}

/*----------------------------------------------------------------------
-- create_arelset - create "arithmetic" elemental set.
--
-- This routine creates "arithmetic" elemental set, which is specified
-- in the form of arithmetic progression:
--
--    { t0 .. tf by dt }.
--
-- Components of this set are 1-tuples. */

ELEMSET *create_arelset(MPL *mpl, double t0, double tf, double dt)
{     ELEMSET *set;
      int j, n;
      set = create_elemset(mpl, 1);
      n = arelset_size(mpl, t0, tf, dt);
      for (j = 1; j <= n; j++)
      {  add_tuple
         (  mpl,
            set,
            expand_tuple
            (  mpl,
               create_tuple(mpl),
               create_symbol_num
               (  mpl,
                  arelset_member(mpl, t0, tf, dt, j)
               )
            )
         );
      }
      return set;
}

/*----------------------------------------------------------------------
-- set_union - union of two elemental sets.
--
-- This routine computes the union:
--
--    X U Y = { j | (j in X) or (j in Y) },
--
-- where X and Y are given elemental sets (destroyed on exit). */

ELEMSET *set_union
(     MPL *mpl,
      ELEMSET *X,             /* destroyed */
      ELEMSET *Y              /* destroyed */
)
{     MEMBER *memb;
      insist(X != NULL);
      insist(X->type == A_NONE);
      insist(X->dim > 0);
      insist(Y != NULL);
      insist(Y->type == A_NONE);
      insist(Y->dim > 0);
      insist(X->dim == Y->dim);
      for (memb = Y->head; memb != NULL; memb = memb->next)
      {  if (find_tuple(mpl, X, memb->tuple) == NULL)
            add_tuple(mpl, X, copy_tuple(mpl, memb->tuple));
      }
      delete_elemset(mpl, Y);
      return X;
}

/*----------------------------------------------------------------------
-- set_diff - difference between two elemental sets.
--
-- This routine computes the difference:
--
--    X \ Y = { j | (j in X) and (j not in Y) },
--
-- where X and Y are given elemental sets (destroyed on exit). */

ELEMSET *set_diff
(     MPL *mpl,
      ELEMSET *X,             /* destroyed */
      ELEMSET *Y              /* destroyed */
)
{     ELEMSET *Z;
      MEMBER *memb;
      insist(X != NULL);
      insist(X->type == A_NONE);
      insist(X->dim > 0);
      insist(Y != NULL);
      insist(Y->type == A_NONE);
      insist(Y->dim > 0);
      insist(X->dim == Y->dim);
      Z = create_elemset(mpl, X->dim);
      for (memb = X->head; memb != NULL; memb = memb->next)
      {  if (find_tuple(mpl, Y, memb->tuple) == NULL)
            add_tuple(mpl, Z, copy_tuple(mpl, memb->tuple));
      }
      delete_elemset(mpl, X);
      delete_elemset(mpl, Y);
      return Z;
}

/*----------------------------------------------------------------------
-- set_symdiff - symmetric difference between two elemental sets.
--
-- This routine computes the symmetric difference:
--
--    X (+) Y = (X \ Y) U (Y \ X),
--
-- where X and Y are given elemental sets (destroyed on exit). */

ELEMSET *set_symdiff
(     MPL *mpl,
      ELEMSET *X,             /* destroyed */
      ELEMSET *Y              /* destroyed */
)
{     ELEMSET *Z;
      MEMBER *memb;
      insist(X != NULL);
      insist(X->type == A_NONE);
      insist(X->dim > 0);
      insist(Y != NULL);
      insist(Y->type == A_NONE);
      insist(Y->dim > 0);
      insist(X->dim == Y->dim);
      /* Z := X \ Y */
      Z = create_elemset(mpl, X->dim);
      for (memb = X->head; memb != NULL; memb = memb->next)
      {  if (find_tuple(mpl, Y, memb->tuple) == NULL)
            add_tuple(mpl, Z, copy_tuple(mpl, memb->tuple));
      }
      /* Z := Z U (Y \ X) */
      for (memb = Y->head; memb != NULL; memb = memb->next)
      {  if (find_tuple(mpl, X, memb->tuple) == NULL)
            add_tuple(mpl, Z, copy_tuple(mpl, memb->tuple));
      }
      delete_elemset(mpl, X);
      delete_elemset(mpl, Y);
      return Z;
}

/*----------------------------------------------------------------------
-- set_inter - intersection of two elemental sets.
--
-- This routine computes the intersection:
--
--    X ^ Y = { j | (j in X) and (j in Y) },
--
-- where X and Y are given elemental sets (destroyed on exit). */

ELEMSET *set_inter
(     MPL *mpl,
      ELEMSET *X,             /* destroyed */
      ELEMSET *Y              /* destroyed */
)
{     ELEMSET *Z;
      MEMBER *memb;
      insist(X != NULL);
      insist(X->type == A_NONE);
      insist(X->dim > 0);
      insist(Y != NULL);
      insist(Y->type == A_NONE);
      insist(Y->dim > 0);
      insist(X->dim == Y->dim);
      Z = create_elemset(mpl, X->dim);
      for (memb = X->head; memb != NULL; memb = memb->next)
      {  if (find_tuple(mpl, Y, memb->tuple) != NULL)
            add_tuple(mpl, Z, copy_tuple(mpl, memb->tuple));
      }
      delete_elemset(mpl, X);
      delete_elemset(mpl, Y);
      return Z;
}

/*----------------------------------------------------------------------
-- set_cross - cross (Cartesian) product of two elemental sets.
--
-- This routine computes the cross (Cartesian) product:
--
--    X x Y = { (i,j) | (i in X) and (j in Y) },
--
-- where X and Y are given elemental sets (destroyed on exit). */

ELEMSET *set_cross
(     MPL *mpl,
      ELEMSET *X,             /* destroyed */
      ELEMSET *Y              /* destroyed */
)
{     ELEMSET *Z;
      MEMBER *memx, *memy;
      TUPLE *tuple, *temp;
      insist(X != NULL);
      insist(X->type == A_NONE);
      insist(X->dim > 0);
      insist(Y != NULL);
      insist(Y->type == A_NONE);
      insist(Y->dim > 0);
      Z = create_elemset(mpl, X->dim + Y->dim);
      for (memx = X->head; memx != NULL; memx = memx->next)
      {  for (memy = Y->head; memy != NULL; memy = memy->next)
         {  tuple = copy_tuple(mpl, memx->tuple);
            for (temp = memy->tuple; temp != NULL; temp = temp->next)
               tuple = expand_tuple(mpl, tuple, copy_symbol(mpl,
                  temp->sym));
            add_tuple(mpl, Z, tuple);
         }
      }
      delete_elemset(mpl, X);
      delete_elemset(mpl, Y);
      return Z;
}

/**********************************************************************/
/* * *                    ELEMENTAL VARIABLES                     * * */
/**********************************************************************/

/* (there are no specific routines for elemental variables) */

/**********************************************************************/
/* * *                        LINEAR FORMS                        * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- constant_term - create constant term.
--
-- This routine creates the linear form, which is a constant term. */

FORMULA *constant_term(MPL *mpl, double coef)
{     FORMULA *form;
      if (coef == 0.0)
         form = NULL;
      else
      {  form = dmp_get_atom(mpl->formulae);
         form->coef = coef;
         form->var = NULL;
         form->next = NULL;
      }
      return form;
}

/*----------------------------------------------------------------------
-- single_variable - create single variable.
--
-- This routine creates the linear form, which is a single elemental
-- variable. */

FORMULA *single_variable
(     MPL *mpl,
      ELEMVAR *var            /* referenced */
)
{     FORMULA *form;
      insist(var != NULL);
      form = dmp_get_atom(mpl->formulae);
      form->coef = 1.0;
      form->var = var;
      form->next = NULL;
      return form;
}

/*----------------------------------------------------------------------
-- copy_formula - make copy of linear form.
--
-- This routine returns an exact copy of linear form. */

FORMULA *copy_formula
(     MPL *mpl,
      FORMULA *form           /* not changed */
)
{     FORMULA *head, *tail;
      if (form == NULL)
         head = NULL;
      else
      {  head = tail = dmp_get_atom(mpl->formulae);
         for (; form != NULL; form = form->next)
         {  tail->coef = form->coef;
            tail->var = form->var;
            if (form->next != NULL)
               tail = (tail->next = dmp_get_atom(mpl->formulae));
         }
         tail->next = NULL;
      }
      return head;
}

/*----------------------------------------------------------------------
-- delete_formula - delete linear form.
--
-- This routine deletes specified linear form. */

void delete_formula
(     MPL *mpl,
      FORMULA *form           /* destroyed */
)
{     FORMULA *temp;
      while (form != NULL)
      {  temp = form;
         form = form->next;
         dmp_free_atom(mpl->formulae, temp);
      }
      return;
}

/*----------------------------------------------------------------------
-- linear_comb - linear combination of two linear forms.
--
-- This routine computes the linear combination:
--
--    a * fx + b * fy,
--
-- where a and b are numeric coefficients, fx and fy are linear forms
-- (destroyed on exit). */

FORMULA *linear_comb
(     MPL *mpl,
      double a, FORMULA *fx,  /* destroyed */
      double b, FORMULA *fy   /* destroyed */
)
{     FORMULA *form = NULL, *term, *temp;
      double c0 = 0.0;
      for (term = fx; term != NULL; term = term->next)
      {  if (term->var == NULL)
            c0 = fp_add(mpl, c0, fp_mul(mpl, a, term->coef));
         else
            term->var->temp =
               fp_add(mpl, term->var->temp, fp_mul(mpl, a, term->coef));
      }
      for (term = fy; term != NULL; term = term->next)
      {  if (term->var == NULL)
            c0 = fp_add(mpl, c0, fp_mul(mpl, b, term->coef));
         else
            term->var->temp =
               fp_add(mpl, term->var->temp, fp_mul(mpl, b, term->coef));
      }
      for (term = fx; term != NULL; term = term->next)
      {  if (term->var != NULL && term->var->temp != 0.0)
         {  temp = dmp_get_atom(mpl->formulae);
            temp->coef = term->var->temp, temp->var = term->var;
            temp->next = form, form = temp;
            term->var->temp = 0.0;
         }
      }
      for (term = fy; term != NULL; term = term->next)
      {  if (term->var != NULL && term->var->temp != 0.0)
         {  temp = dmp_get_atom(mpl->formulae);
            temp->coef = term->var->temp, temp->var = term->var;
            temp->next = form, form = temp;
            term->var->temp = 0.0;
         }
      }
      if (c0 != 0.0)
      {  temp = dmp_get_atom(mpl->formulae);
         temp->coef = c0, temp->var = NULL;
         temp->next = form, form = temp;
      }
      delete_formula(mpl, fx);
      delete_formula(mpl, fy);
      return form;
}

/*----------------------------------------------------------------------
-- remove_constant - remove constant term from linear form.
--
-- This routine removes constant term from linear form and stores its
-- value to given location. */

FORMULA *remove_constant
(     MPL *mpl,
      FORMULA *form,          /* destroyed */
      double *coef            /* modified */
)
{     FORMULA *head = NULL, *temp;
      *coef = 0.0;
      while (form != NULL)
      {  temp = form;
         form = form->next;
         if (temp->var == NULL)
         {  /* constant term */
            *coef = fp_add(mpl, *coef, temp->coef);
            dmp_free_atom(mpl->formulae, temp);
         }
         else
         {  /* linear term */
            temp->next = head;
            head = temp;
         }
      }
      return head;
}

/**********************************************************************/
/* * *                   ELEMENTAL CONSTRAINTS                    * * */
/**********************************************************************/

/* (there are no specific routines for elemental constraints) */

/**********************************************************************/
/* * *                       GENERIC VALUES                       * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- delete_value - delete generic value.
--
-- This routine deletes specified generic value.
--
-- NOTE: The generic value to be deleted must be valid. */

void delete_value
(     MPL *mpl,
      int type,
      VALUE *value            /* content destroyed */
)
{     insist(value != NULL);
      switch (type)
      {  case A_NONE:
            value->none = NULL;
            break;
         case A_NUMERIC:
            value->num = 0.0;
            break;
         case A_SYMBOLIC:
            delete_symbol(mpl, value->sym), value->sym = NULL;
            break;
         case A_LOGICAL:
            value->bit = 0;
            break;
         case A_TUPLE:
            delete_tuple(mpl, value->tuple), value->tuple = NULL;
            break;
         case A_ELEMSET:
            delete_elemset(mpl, value->set), value->set = NULL;
            break;
         case A_ELEMVAR:
            value->var = NULL;
            break;
         case A_FORMULA:
            delete_formula(mpl, value->form), value->form = NULL;
            break;
         case A_ELEMCON:
            value->con = NULL;
            break;
         default:
            insist(type != type);
      }
      return;
}

/**********************************************************************/
/* * *                SYMBOLICALLY INDEXED ARRAYS                 * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- create_array - create array.
--
-- This routine creates an array of specified type and dimension. Being
-- created the array is initially empty.
--
-- The type indicator determines generic values, which can be assigned
-- to the array members:
--
-- A_NONE     - none (members have no assigned values)
-- A_NUMERIC  - floating-point numbers
-- A_SYMBOLIC - symbols
-- A_ELEMSET  - elemental sets
-- A_ELEMVAR  - elemental variables
-- A_ELEMCON  - elemental constraints
--
-- The dimension may be 0, in which case the array consists of the only
-- member (such arrays represent 0-dimensional objects). */

ARRAY *create_array(MPL *mpl, int type, int dim)
{     ARRAY *array;
      insist(type == A_NONE || type == A_NUMERIC ||
             type == A_SYMBOLIC || type == A_ELEMSET ||
             type == A_ELEMVAR || type == A_ELEMCON);
      insist(dim >= 0);
      array = dmp_get_atom(mpl->arrays);
      array->type = type;
      array->dim = dim;
      array->size = 0;
      array->head = NULL;
      array->tail = NULL;
      array->tree = NULL;
      array->prev = NULL;
      array->next = mpl->a_list;
      /* include the array in the global array list */
      if (array->next != NULL) array->next->prev = array;
      mpl->a_list = array;
      return array;
}

/*----------------------------------------------------------------------
-- find_member - find array member with given n-tuple.
--
-- This routine finds an array member, which has given n-tuple. If the
-- array is short, the linear search is used. Otherwise the routine
-- autimatically creates the search tree (i.e. the array index) to find
-- members for logarithmic time. */

static int compare_member_tuples(void *info, void *key1, void *key2)
{     /* this is an auxiliary routine used to compare keys, which are
         n-tuples assigned to array members */
      return compare_tuples((MPL *)info, (TUPLE *)key1, (TUPLE *)key2);
}

MEMBER *find_member
(     MPL *mpl,
      ARRAY *array,           /* not changed */
      TUPLE *tuple            /* not changed */
)
{     MEMBER *memb;
      insist(array != NULL);
      /* the n-tuple must have the same dimension as the array */
      insist(tuple_dimen(mpl, tuple) == array->dim);
      /* if the array is large enough, create the search tree and index
         all existing members of the array */
      if (array->size > 30 && array->tree == NULL)
      {  array->tree = avl_create_tree(mpl, compare_member_tuples);
         for (memb = array->head; memb != NULL; memb = memb->next)
            avl_insert_by_key(array->tree, memb->tuple)->link =
               (void *)memb;
      }
      /* find a member, which has the given tuple */
      if (array->tree == NULL)
      {  /* the search tree doesn't exist; use the linear search */
         for (memb = array->head; memb != NULL; memb = memb->next)
            if (compare_tuples(mpl, memb->tuple, tuple) == 0) break;
      }
      else
      {  /* the search tree exists; use the binary search */
         AVLNODE *node;
         node = avl_find_by_key(array->tree, tuple);
         memb = (MEMBER *)(node == NULL ? NULL : node->link);
      }
      return memb;
}

/*----------------------------------------------------------------------
-- add_member - add new member to array.
--
-- This routine creates a new member with given n-tuple and adds it to
-- specified array.
--
-- For the sake of efficiency this routine doesn't check whether the
-- array already contains a member with the given n-tuple or not. Thus,
-- if necessary, the calling program should use the routine find_member
-- in order to be sure that the array contains no member with the same
-- n-tuple, because members with duplicate n-tuples are not allowed.
--
-- This routine assigns no generic value to the new member, because the
-- calling program must do that. */

MEMBER *add_member
(     MPL *mpl,
      ARRAY *array,           /* modified */
      TUPLE *tuple            /* destroyed */
)
{     MEMBER *memb;
      insist(array != NULL);
      /* the n-tuple must have the same dimension as the array */
      insist(tuple_dimen(mpl, tuple) == array->dim);
      /* create new member */
      memb = dmp_get_atom(mpl->members);
      memb->tuple = tuple;
      memb->next = NULL;
      memset(&memb->value, '?', sizeof(VALUE));
      /* and append it to the member list */
      array->size++;
      if (array->head == NULL)
         array->head = memb;
      else
         array->tail->next = memb;
      array->tail = memb;
      /* if the search tree exists, index the new member */
      if (array->tree != NULL)
         avl_insert_by_key(array->tree, memb->tuple)->link =
            (void *)memb;
      return memb;
}

/*----------------------------------------------------------------------
-- delete_array - delete array.
--
-- This routine deletes specified array.
--
-- Generic values assigned to the array members are not deleted by this
-- routine. The calling program itself must delete all assigned generic
-- values before deleting the array. */

void delete_array
(     MPL *mpl,
      ARRAY *array            /* destroyed */
)
{     MEMBER *memb;
      insist(array != NULL);
      /* delete all existing array members */
      while (array->head != NULL)
      {  memb = array->head;
         array->head = memb->next;
         delete_tuple(mpl, memb->tuple);
         dmp_free_atom(mpl->members, memb);
      }
      /* if the search tree exists, also delete it */
      if (array->tree != NULL) avl_delete_tree(array->tree);
      /* remove the array from the global array list */
      if (array->prev == NULL)
         mpl->a_list = array->next;
      else
         array->prev->next = array->next;
      if (array->next == NULL)
         ;
      else
         array->next->prev = array->prev;
      /* delete the array descriptor */
      dmp_free_atom(mpl->arrays, array);
      return;
}

/**********************************************************************/
/* * *                 DOMAINS AND DUMMY INDICES                  * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- assign_dummy_index - assign new value to dummy index.
--
-- This routine assigns new value to specified dummy index and, that is
-- important, invalidates all temporary resultant values, which depends
-- on that dummy index. */

void assign_dummy_index
(     MPL *mpl,
      DOMAIN_SLOT *slot,      /* modified */
      SYMBOL *value           /* not changed */
)
{     CODE *leaf, *code;
      insist(slot != NULL);
      insist(value != NULL);
      /* delete the current value assigned to the dummy index */
      if (slot->value != NULL)
      {  /* if the current value and the new one are identical, actual
            assignment is not needed */
         if (compare_symbols(mpl, slot->value, value) == 0) goto done;
         /* delete a symbol, which is the current value */
         delete_symbol(mpl, slot->value), slot->value = NULL;
      }
      /* now walk through all the pseudo-codes with op = O_INDEX, which
         refer to the dummy index to be changed (these pseudo-codes are
         leaves in the forest of *all* expressions in the database) */
      for (leaf = slot->list; leaf != NULL; leaf = leaf->arg.index.
         next)
      {  insist(leaf->op == O_INDEX);
         /* invalidate all resultant values, which depend on the dummy
            index, walking from the current leaf toward the root of the
            corresponding expression tree */
         for (code = leaf; code != NULL; code = code->up)
         {  if (code->valid)
            {  /* invalidate and delete resultant value */
               code->valid = 0;
               delete_value(mpl, code->type, &code->value);
            }
         }
      }
      /* assign new value to the dummy index */
      slot->value = copy_symbol(mpl, value);
done: return;
}

/*----------------------------------------------------------------------
-- update_dummy_indices - update current values of dummy indices.
--
-- This routine assigns components of "backup" n-tuple to dummy indices
-- of specified domain block. If no "backup" n-tuple is defined for the
-- domain block, values of the dummy indices remain untouched. */

void update_dummy_indices
(     MPL *mpl,
      DOMAIN_BLOCK *block     /* not changed */
)
{     DOMAIN_SLOT *slot;
      TUPLE *temp;
      if (block->backup != NULL)
      {  for (slot = block->list, temp = block->backup; slot != NULL;
            slot = slot->next, temp = temp->next)
         {  insist(temp != NULL);
            insist(temp->sym != NULL);
            assign_dummy_index(mpl, slot, temp->sym);
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- enter_domain_block - enter domain block.
--
-- Let specified domain block have the form:
--
--    { ..., (j1, j2, ..., jn) in J, ... }
--
-- where j1, j2, ..., jn are dummy indices, J is a basic set.
--
-- This routine does the following:
--
-- 1. Checks if the given n-tuple is a member of the basic set J. Note
--    that J being *out of the scope* of the domain block cannot depend
--    on the dummy indices in the same and inner domain blocks, so it
--    can be computed before the dummy indices are assigned new values.
--    If this check fails, the routine returns with non-zero code.
--
-- 2. Saves current values of the dummy indices j1, j2, ..., jn.
--
-- 3. Assigns new values, which are components of the given n-tuple, to
--    the dummy indices j1, j2, ..., jn. If dimension of the n-tuple is
--    larger than n, its extra components n+1, n+2, ... are not used.
--
-- 4. Calls the formal routine func which either enters the next domain
--    block or evaluates some code within the domain scope.
--
-- 5. Restores former values of the dummy indices j1, j2, ..., jn.
--
-- Since current values assigned to the dummy indices on entry to this
-- routine are restored on exit, the formal routine func is allowed to
-- call this routine recursively. */

int enter_domain_block
(     MPL *mpl,
      DOMAIN_BLOCK *block,    /* not changed */
      TUPLE *tuple,           /* not changed */
      void *info, void (*func)(MPL *mpl, void *info)
)
{     TUPLE *backup;
      int ret = 0;
      /* check if the given n-tuple is a member of the basic set */
      insist(block->code != NULL);
      if (!is_member(mpl, block->code, tuple))
      {  ret = 1;
         goto done;
      }
      /* save reference to "backup" n-tuple, which was used to assign
         current values of the dummy indices (it is sufficient to save
         reference, not value, because that n-tuple is defined in some
         outer level of recursion and therefore cannot be changed on
         this and deeper recursive calls) */
      backup = block->backup;
      /* set up new "backup" n-tuple, which defines new values of the
         dummy indices */
      block->backup = tuple;
      /* assign new values to the dummy indices */
      update_dummy_indices(mpl, block);
      /* call the formal routine that does the rest part of the job */
      func(mpl, info);
      /* restore reference to the former "backup" n-tuple */
      block->backup = backup;
      /* restore former values of the dummy indices; note that if the
         domain block just escaped has no other active instances which
         may exist due to recursion (it is indicated by a null pointer
         to the former n-tuple), former values of the dummy indices are
         undefined; therefore in this case the routine keeps currently
         assigned values of the dummy indices that involves keeping all
         dependent temporary results and thereby, if this domain block
         is not used recursively, allows improving efficiency */
      update_dummy_indices(mpl, block);
done: return ret;
}

/*----------------------------------------------------------------------
-- eval_within_domain - perform evaluation within domain scope.
--
-- This routine assigns new values (symbols) to all dummy indices of
-- specified domain and calls the formal routine func, which is used to
-- evaluate some code in the domain scope. Each free dummy index in the
-- domain is assigned a value specified in the corresponding component
-- of given n-tuple. Non-free dummy indices are assigned values, which
-- are computed by this routine.
--
-- Number of components in the given n-tuple must be the same as number
-- of free indices in the domain.
--
-- If the given n-tuple is not a member of the domain set, the routine
-- func is not called, and non-zero code is returned.
--
-- For the sake of convenience it is allowed to specify domain as NULL
-- (then n-tuple also must be 0-tuple, i.e. empty), in which case this
-- routine just calls the routine func and returns zero.
--
-- This routine allows recursive calls from the routine func providing
-- correct values of dummy indices for each instance.
--
-- NOTE: The n-tuple passed to this routine must not be changed by any
--       other routines called from the formal routine func until this
--       routine has returned. */

struct eval_domain_info
{     /* working info used by the routine eval_within_domain */
      DOMAIN *domain;
      /* domain, which has to be entered */
      DOMAIN_BLOCK *block;
      /* domain block, which is currently processed */
      TUPLE *tuple;
      /* tail of original n-tuple, whose components have to be assigned
         to free dummy indices in the current domain block */
      void *info;
      /* transit pointer passed to the formal routine func */
      void (*func)(MPL *mpl, void *info);
      /* routine, which has to be executed in the domain scope */
      int failure;
      /* this flag indicates that given n-tuple is not a member of the
         domain set */
};

static void eval_domain_func(MPL *mpl, void *_my_info)
{     /* this routine recursively enters into the domain scope and then
         calls the routine func */
      struct eval_domain_info *my_info = _my_info;
      if (my_info->block != NULL)
      {  /* the current domain block to be entered exists */
         DOMAIN_BLOCK *block;
         DOMAIN_SLOT *slot;
         TUPLE *tuple = NULL, *temp = NULL;
         /* save pointer to the current domain block */
         block = my_info->block;
         /* and get ready to enter the next block (if it exists) */
         my_info->block = block->next;
         /* construct temporary n-tuple, whose components correspond to
            dummy indices (slots) of the current domain; components of
            the temporary n-tuple that correspond to free dummy indices
            are assigned references (not values!) to symbols specified
            in the corresponding components of the given n-tuple, while
            other components that correspond to non-free dummy indices
            are assigned symbolic values computed here */
         for (slot = block->list; slot != NULL; slot = slot->next)
         {  /* create component that corresponds to the current slot */
            if (tuple == NULL)
               tuple = temp = dmp_get_atom(mpl->tuples);
            else
               temp = (temp->next = dmp_get_atom(mpl->tuples));
            if (slot->code == NULL)
            {  /* dummy index is free; take reference to symbol, which
                  is specified in the corresponding component of given
                  n-tuple */
               insist(my_info->tuple != NULL);
               temp->sym = my_info->tuple->sym;
               insist(temp->sym != NULL);
               my_info->tuple = my_info->tuple->next;
            }
            else
            {  /* dummy index is non-free; compute symbolic value to be
                  temporarily assigned to the dummy index */
               temp->sym = eval_symbolic(mpl, slot->code);
            }
         }
         temp->next = NULL;
         /* enter the current domain block */
         if (enter_domain_block(mpl, block, tuple, my_info,
               eval_domain_func)) my_info->failure = 1;
         /* delete temporary n-tuple as well as symbols that correspond
            to non-free dummy indices (they were computed here) */
         for (slot = block->list; slot != NULL; slot = slot->next)
         {  insist(tuple != NULL);
            temp = tuple;
            tuple = tuple->next;
            if (slot->code != NULL)
            {  /* dummy index is non-free; delete symbolic value */
               delete_symbol(mpl, temp->sym);
            }
            /* delete component that corresponds to the current slot */
            dmp_free_atom(mpl->tuples, temp);
         }
      }
      else
      {  /* there are no more domain blocks, i.e. we have reached the
            domain scope */
         insist(my_info->tuple == NULL);
         /* check optional predicate specified for the domain */
         if (my_info->domain->code != NULL && !eval_logical(mpl,
            my_info->domain->code))
         {  /* the predicate is false */
            my_info->failure = 2;
         }
         else
         {  /* the predicate is true; do the job */
            my_info->func(mpl, my_info->info);
         }
      }
      return;
}

int eval_within_domain
(     MPL *mpl,
      DOMAIN *domain,         /* not changed */
      TUPLE *tuple,           /* not changed */
      void *info, void (*func)(MPL *mpl, void *info)
)
{     /* this routine performs evaluation within domain scope */
      struct eval_domain_info _my_info, *my_info = &_my_info;
      if (domain == NULL)
      {  insist(tuple == NULL);
         func(mpl, info);
         my_info->failure = 0;
      }
      else
      {  insist(tuple != NULL);
         my_info->domain = domain;
         my_info->block = domain->list;
         my_info->tuple = tuple;
         my_info->info = info;
         my_info->func = func;
         my_info->failure = 0;
         /* enter the very first domain block */
         eval_domain_func(mpl, my_info);
      }
      return my_info->failure;
}

/*----------------------------------------------------------------------
-- loop_within_domain - perform iterations within domain scope.
--
-- This routine iteratively assigns new values (symbols) to the dummy
-- indices of specified domain by enumerating all n-tuples, which are
-- members of the domain set, and for every n-tuple it calls the formal
-- routine func to evaluate some code within the domain scope.
--
-- If the routine func returns non-zero, enumeration within the domain
-- is prematurely terminated.
--
-- For the sake of convenience it is allowed to specify domain as NULL,
-- in which case this routine just calls the routine func only once and
-- returns zero.
--
-- This routine allows recursive calls from the routine func providing
-- correct values of dummy indices for each instance. */

struct loop_domain_info
{     /* working info used by the routine loop_within_domain */
      DOMAIN *domain;
      /* domain, which has to be entered */
      DOMAIN_BLOCK *block;
      /* domain block, which is currently processed */
      int looping;
      /* clearing this flag leads to terminating enumeration */
      void *info;
      /* transit pointer passed to the formal routine func */
      int (*func)(MPL *mpl, void *info);
      /* routine, which needs to be executed in the domain scope */
};

static void loop_domain_func(MPL *mpl, void *_my_info)
{     /* this routine enumerates all n-tuples in the basic set of the
         current domain block, enters recursively into the domain scope
         for every n-tuple, and then calls the routine func */
      struct loop_domain_info *my_info = _my_info;
      if (my_info->block != NULL)
      {  /* the current domain block to be entered exists */
         DOMAIN_BLOCK *block;
         DOMAIN_SLOT *slot;
         TUPLE *bound;
         /* save pointer to the current domain block */
         block = my_info->block;
         /* and get ready to enter the next block (if it exists) */
         my_info->block = block->next;
         /* compute symbolic values, at which non-free dummy indices of
            the current domain block are bound; since that values don't
            depend on free dummy indices of the current block, they can
            be computed once out of the enumeration loop */
         bound = create_tuple(mpl);
         for (slot = block->list; slot != NULL; slot = slot->next)
         {  if (slot->code != NULL)
               bound = expand_tuple(mpl, bound, eval_symbolic(mpl,
                  slot->code));
         }
         /* start enumeration */
         insist(block->code != NULL);
         if (block->code->op == O_DOTS)
         {  /* the basic set is "arithmetic", in which case it doesn't
               need to be computed explicitly */
            TUPLE *tuple;
            int n, j;
            double t0, tf, dt;
            /* compute "parameters" of the basic set */
            t0 = eval_numeric(mpl, block->code->arg.arg.x);
            tf = eval_numeric(mpl, block->code->arg.arg.y);
            if (block->code->arg.arg.z == NULL)
               dt = 1.0;
            else
               dt = eval_numeric(mpl, block->code->arg.arg.z);
            /* determine cardinality of the basic set */
            n = arelset_size(mpl, t0, tf, dt);
            /* create dummy 1-tuple for members of the basic set */
            tuple = expand_tuple(mpl, create_tuple(mpl),
               create_symbol_num(mpl, 0.0));
            /* in case of "arithmetic" set there is exactly one dummy
               index, which cannot be non-free */
            insist(bound == NULL);
            /* walk through 1-tuples of the basic set */
            for (j = 1; j <= n && my_info->looping; j++)
            {  /* construct dummy 1-tuple for the current member */
               tuple->sym->num = arelset_member(mpl, t0, tf, dt, j);
               /* enter the current domain block */
               enter_domain_block(mpl, block, tuple, my_info,
                  loop_domain_func);
            }
            /* delete dummy 1-tuple */
            delete_tuple(mpl, tuple);
         }
         else
         {  /* the basic set is of general kind, in which case it needs
               to be explicitly computed */
            ELEMSET *set;
            MEMBER *memb;
            TUPLE *temp1, *temp2;
            /* compute the basic set */
            set = eval_elemset(mpl, block->code);
            /* walk through all n-tuples of the basic set */
            for (memb = set->head; memb != NULL && my_info->looping;
               memb = memb->next)
            {  /* all components of the current n-tuple that correspond
                  to non-free dummy indices must be feasible; otherwise
                  the n-tuple is not in the basic set */
               temp1 = memb->tuple;
               temp2 = bound;
               for (slot = block->list; slot != NULL; slot = slot->next)
               {  insist(temp1 != NULL);
                  if (slot->code != NULL)
                  {  /* non-free dummy index */
                     insist(temp2 != NULL);
                     if (compare_symbols(mpl, temp1->sym, temp2->sym)
                        != 0)
                     {  /* the n-tuple is not in the basic set */
                        goto skip;
                     }
                     temp2 = temp2->next;
                  }
                  temp1 = temp1->next;
               }
               insist(temp1 == NULL);
               insist(temp2 == NULL);
               /* enter the current domain block */
               enter_domain_block(mpl, block, memb->tuple, my_info,
                  loop_domain_func);
skip:          ;
            }
            /* delete the basic set */
            delete_elemset(mpl, set);
         }
         /* delete symbolic values binding non-free dummy indices */
         delete_tuple(mpl, bound);
         /* restore pointer to the current domain block */
         my_info->block = block;
      }
      else
      {  /* there are no more domain blocks, i.e. we have reached the
            domain scope */
         /* check optional predicate specified for the domain */
         if (my_info->domain->code != NULL && !eval_logical(mpl,
            my_info->domain->code))
         {  /* the predicate is false */
            /* nop */;
         }
         else
         {  /* the predicate is true; do the job */
            my_info->looping = !my_info->func(mpl, my_info->info);
         }
      }
      return;
}

void loop_within_domain
(     MPL *mpl,
      DOMAIN *domain,         /* not changed */
      void *info, int (*func)(MPL *mpl, void *info)
)
{     /* this routine performs iterations within domain scope */
      struct loop_domain_info _my_info, *my_info = &_my_info;
      if (domain == NULL)
         func(mpl, info);
      else
      {  my_info->domain = domain;
         my_info->block = domain->list;
         my_info->looping = 1;
         my_info->info = info;
         my_info->func = func;
         /* enter the very first domain block */
         loop_domain_func(mpl, my_info);
      }
      return;
}

/*----------------------------------------------------------------------
-- out_of_domain - raise domain exception.
--
-- This routine is called when a reference is made to a member of some
-- model object, but its n-tuple is out of the object domain. */

void out_of_domain
(     MPL *mpl,
      char *name,             /* not changed */
      TUPLE *tuple            /* not changed */
)
{     insist(name != NULL);
      insist(tuple != NULL);
      error(mpl, "%s%s out of domain", name, format_tuple(mpl, '[',
         tuple));
      /* no return */
}

/*----------------------------------------------------------------------
-- get_domain_tuple - obtain current n-tuple from domain.
--
-- This routine constructs n-tuple, whose components are current values
-- assigned to *free* dummy indices of specified domain.
--
-- For the sake of convenience it is allowed to specify domain as NULL,
-- in which case this routine returns 0-tuple.
--
-- NOTE: This routine must not be called out of domain scope. */

TUPLE *get_domain_tuple
(     MPL *mpl,
      DOMAIN *domain          /* not changed */
)
{     DOMAIN_BLOCK *block;
      DOMAIN_SLOT *slot;
      TUPLE *tuple;
      tuple = create_tuple(mpl);
      if (domain != NULL)
      {  for (block = domain->list; block != NULL; block = block->next)
         {  for (slot = block->list; slot != NULL; slot = slot->next)
            {  if (slot->code == NULL)
               {  insist(slot->value != NULL);
                  tuple = expand_tuple(mpl, tuple, copy_symbol(mpl,
                     slot->value));
               }
            }
         }
      }
      return tuple;
}

/*----------------------------------------------------------------------
-- clean_domain - clean domain.
--
-- This routine cleans specified domain that assumes deleting all stuff
-- dynamically allocated during the generation phase. */

void clean_domain(MPL *mpl, DOMAIN *domain)
{     DOMAIN_BLOCK *block;
      DOMAIN_SLOT *slot;
      /* if no domain is specified, do nothing */
      if (domain == NULL) goto done;
      /* clean all domain blocks */
      for (block = domain->list; block != NULL; block = block->next)
      {  /* clean all domain slots */
         for (slot = block->list; slot != NULL; slot = slot->next)
         {  /* clean pseudo-code for computing bound value */
            clean_code(mpl, slot->code);
            /* delete symbolic value assigned to dummy index */
            if (slot->value != NULL)
               delete_symbol(mpl, slot->value), slot->value = NULL;
         }
         /* clean pseudo-code for computing basic set */
         clean_code(mpl, block->code);
      }
      /* clean pseudo-code for computing domain predicate */
      clean_code(mpl, domain->code);
done: return;
}

/**********************************************************************/
/* * *                         MODEL SETS                         * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- check_elem_set - check elemental set assigned to set member.
--
-- This routine checks if given elemental set being assigned to member
-- of specified model set satisfies to all restrictions.
--
-- NOTE: This routine must not be called out of domain scope. */

void check_elem_set
(     MPL *mpl,
      SET *set,               /* not changed */
      TUPLE *tuple,           /* not changed */
      ELEMSET *refer          /* not changed */
)
{     WITHIN *within;
      MEMBER *memb;
      int eqno;
      /* elemental set must be within all specified supersets */
      for (within = set->within, eqno = 1; within != NULL; within =
         within->next, eqno++)
      {  insist(within->code != NULL);
         for (memb = refer->head; memb != NULL; memb = memb->next)
         {  if (!is_member(mpl, within->code, memb->tuple))
            {  char buf[255+1];
               strcpy(buf, format_tuple(mpl, '(', memb->tuple));
               insist(strlen(buf) < sizeof(buf));
               error(mpl, "%s%s contains %s which not within specified "
                  "set; see (%d)", set->name, format_tuple(mpl, '[',
                     tuple), buf, eqno);
            }
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- take_member_set - obtain elemental set assigned to set member.
--
-- This routine obtains a reference to elemental set assigned to given
-- member of specified model set and returns it on exit.
--
-- NOTE: This routine must not be called out of domain scope. */

ELEMSET *take_member_set      /* returns reference, not value */
(     MPL *mpl,
      SET *set,               /* not changed */
      TUPLE *tuple            /* not changed */
)
{     MEMBER *memb;
      ELEMSET *refer;
      /* find member in the set array */
      memb = find_member(mpl, set->array, tuple);
      if (memb != NULL)
      {  /* member exists, so just take the reference */
         refer = memb->value.set;
      }
      else if (set->assign != NULL)
      {  /* compute value using assignment expression */
         refer = eval_elemset(mpl, set->assign);
add:     /* check that the elemental set satisfies to all restrictions,
            assign it to new member, and add the member to the array */
         check_elem_set(mpl, set, tuple, refer);
         memb = add_member(mpl, set->array, copy_tuple(mpl, tuple));
         memb->value.set = refer;
      }
      else if (set->option != NULL)
      {  /* compute default elemental set */
         refer = eval_elemset(mpl, set->option);
         goto add;
      }
      else
      {  /* no value (elemental set) is provided */
         error(mpl, "no value for %s%s", set->name, format_tuple(mpl,
            '[', tuple));
      }
      return refer;
}

/*----------------------------------------------------------------------
-- eval_member_set - evaluate elemental set assigned to set member.
--
-- This routine evaluates a reference to elemental set assigned to given
-- member of specified model set and returns it on exit. */

struct eval_set_info
{     /* working info used by the routine eval_member_set */
      SET *set;
      /* model set */
      TUPLE *tuple;
      /* n-tuple, which defines set member */
      MEMBER *memb;
      /* normally this pointer is NULL; the routine uses this pointer
         to check data provided in the data section, in which case it
         points to a member currently checked; this check is performed
         automatically only once when a reference to any member occurs
         for the first time */
      ELEMSET *refer;
      /* evaluated reference to elemental set */
};

static void eval_set_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine to work within domain scope */
      struct eval_set_info *info = _info;
      if (info->memb != NULL)
      {  /* checking call; check elemental set being assigned */
         check_elem_set(mpl, info->set, info->memb->tuple,
            info->memb->value.set);
      }
      else
      {  /* normal call; evaluate member, which has given n-tuple */
         info->refer = take_member_set(mpl, info->set, info->tuple);
      }
      return;
}

ELEMSET *eval_member_set      /* returns reference, not value */
(     MPL *mpl,
      SET *set,               /* not changed */
      TUPLE *tuple            /* not changed */
)
{     /* this routine evaluates set member */
      struct eval_set_info _info, *info = &_info;
      insist(set->dim == tuple_dimen(mpl, tuple));
      info->set = set;
      info->tuple = tuple;
      if (set->data == 1)
      {  /* check data, which are provided in the data section, but not
            checked yet */
         /* save pointer to the last array member; note that during the
            check new members may be added beyond the last member due to
            references to the same parameter from default expression as
            well as from expressions that define restricting supersets;
            however, values assigned to the new members will be checked
            by other routine, so we don't need to check them here */
         MEMBER *tail = set->array->tail;
         /* change the data status to prevent infinite recursive loop
            due to references to the same set during the check */
         set->data = 2;
         /* check elemental sets assigned to array members in the data
            section until the marked member has been reached */
         for (info->memb = set->array->head; info->memb != NULL;
            info->memb = info->memb->next)
         {  if (eval_within_domain(mpl, set->domain, info->memb->tuple,
               info, eval_set_func))
               out_of_domain(mpl, set->name, info->memb->tuple);
            if (info->memb == tail) break;
         }
         /* the check has been finished */
      }
      /* evaluate member, which has given n-tuple */
      info->memb = NULL;
      if (eval_within_domain(mpl, info->set->domain, info->tuple, info,
         eval_set_func))
      out_of_domain(mpl, set->name, info->tuple);
      /* bring evaluated reference to the calling program */
      return info->refer;
}

/*----------------------------------------------------------------------
-- eval_whole_set - evaluate model set over entire domain.
--
-- This routine evaluates all members of specified model set over entire
-- domain. */

static int whole_set_func(MPL *mpl, void *info)
{     /* this is auxiliary routine to work within domain scope */
      SET *set = (SET *)info;
      TUPLE *tuple = get_domain_tuple(mpl, set->domain);
      eval_member_set(mpl, set, tuple);
      delete_tuple(mpl, tuple);
      return 0;
}

void eval_whole_set(MPL *mpl, SET *set)
{     loop_within_domain(mpl, set->domain, set, whole_set_func);
      return;
}

/*----------------------------------------------------------------------
-- clean set - clean model set.
--
-- This routine cleans specified model set that assumes deleting all
-- stuff dynamically allocated during the generation phase. */

void clean_set(MPL *mpl, SET *set)
{     WITHIN *within;
      MEMBER *memb;
      /* clean subscript domain */
      clean_domain(mpl, set->domain);
      /* clean pseudo-code for computing supersets */
      for (within = set->within; within != NULL; within = within->next)
         clean_code(mpl, within->code);
      /* clean pseudo-code for computing assigned value */
      clean_code(mpl, set->assign);
      /* clean pseudo-code for computing default value */
      clean_code(mpl, set->option);
      /* reset data status flag */
      set->data = 0;
      /* delete content array */
      for (memb = set->array->head; memb != NULL; memb = memb->next)
         delete_value(mpl, set->array->type, &memb->value);
      delete_array(mpl, set->array), set->array = NULL;
      return;
}

/**********************************************************************/
/* * *                      MODEL PARAMETERS                      * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- check_value_num - check numeric value assigned to parameter member.
--
-- This routine checks if numeric value being assigned to some member
-- of specified numeric model parameter satisfies to all restrictions.
--
-- NOTE: This routine must not be called out of domain scope. */

void check_value_num
(     MPL *mpl,
      PARAMETER *par,         /* not changed */
      TUPLE *tuple,           /* not changed */
      double value
)
{     CONDITION *cond;
      WITHIN *in;
      int eqno;
      /* the value must satisfy to the parameter type */
      switch (par->type)
      {  case A_NUMERIC:
            break;
         case A_INTEGER:
            if (value != floor(value))
               error(mpl, "%s%s = %.*g not integer", par->name,
                  format_tuple(mpl, '[', tuple), DBL_DIG, value);
            break;
         case A_BINARY:
            if (!(value == 0.0 || value == 1.0))
               error(mpl, "%s%s = %.*g not binary", par->name,
                  format_tuple(mpl, '[', tuple), DBL_DIG, value);
            break;
         default:
            insist(par != par);
      }
      /* the value must satisfy to all specified conditions */
      for (cond = par->cond, eqno = 1; cond != NULL; cond = cond->next,
         eqno++)
      {  double bound;
         char *rho;
         insist(cond->code != NULL);
         bound = eval_numeric(mpl, cond->code);
         switch (cond->rho)
         {  case O_LT:
               if (!(value < bound))
               {  rho = "<";
err:              error(mpl, "%s%s = %.*g not %s %.*g; see (%d)",
                     par->name, format_tuple(mpl, '[', tuple), DBL_DIG,
                     value, rho, DBL_DIG, bound, eqno);
               }
               break;
            case O_LE:
               if (!(value <= bound)) { rho = "<="; goto err; }
               break;
            case O_EQ:
               if (!(value == bound)) { rho = "="; goto err; }
               break;
            case O_GE:
               if (!(value >= bound)) { rho = ">="; goto err; }
               break;
            case O_GT:
               if (!(value > bound)) { rho = ">"; goto err; }
               break;
            case O_NE:
               if (!(value != bound)) { rho = "<>"; goto err; }
               break;
            default:
               insist(cond != cond);
         }
      }
      /* the value must be in all specified supersets */
      for (in = par->in, eqno = 1; in != NULL; in = in->next, eqno++)
      {  TUPLE *dummy;
         insist(in->code != NULL);
         insist(in->code->dim == 1);
         dummy = expand_tuple(mpl, create_tuple(mpl),
            create_symbol_num(mpl, value));
         if (!is_member(mpl, in->code, dummy))
            error(mpl, "%s%s = %.*g not in specified set; see (%d)",
               par->name, format_tuple(mpl, '[', tuple), DBL_DIG,
               value, eqno);
         delete_tuple(mpl, dummy);
      }
      return;
}

/*----------------------------------------------------------------------
-- take_member_num - obtain num. value assigned to parameter member.
--
-- This routine obtains a numeric value assigned to member of specified
-- numeric model parameter and returns it on exit.
--
-- NOTE: This routine must not be called out of domain scope. */

double take_member_num
(     MPL *mpl,
      PARAMETER *par,         /* not changed */
      TUPLE *tuple            /* not changed */
)
{     MEMBER *memb;
      double value;
      /* find member in the parameter array */
      memb = find_member(mpl, par->array, tuple);
      if (memb != NULL)
      {  /* member exists, so just take its value */
         value = memb->value.num;
      }
      else if (par->assign != NULL)
      {  /* compute value using assignment expression */
         value = eval_numeric(mpl, par->assign);
add:     /* check that the value satisfies to all restrictions, assign
            it to new member, and add the member to the array */
         check_value_num(mpl, par, tuple, value);
         memb = add_member(mpl, par->array, copy_tuple(mpl, tuple));
         memb->value.num = value;
      }
      else if (par->option != NULL)
      {  /* compute default value */
         value = eval_numeric(mpl, par->option);
         goto add;
      }
      else if (par->defval != NULL)
      {  /* take default value provided in the data section */
         if (par->defval->str != NULL)
            error(mpl, "cannot convert %s to floating-point number",
               format_symbol(mpl, par->defval));
         value = par->defval->num;
         goto add;
      }
      else
      {  /* no value is provided */
         error(mpl, "no value for %s%s", par->name, format_tuple(mpl,
            '[', tuple));
      }
      return value;
}

/*----------------------------------------------------------------------
-- eval_member_num - evaluate num. value assigned to parameter member.
--
-- This routine evaluates a numeric value assigned to given member of
-- specified numeric model parameter and returns it on exit. */

struct eval_num_info
{     /* working info used by the routine eval_member_num */
      PARAMETER *par;
      /* model parameter  */
      TUPLE *tuple;
      /* n-tuple, which defines parameter member */
      MEMBER *memb;
      /* normally this pointer is NULL; the routine uses this pointer
         to check data provided in the data section, in which case it
         points to a member currently checked; this check is performed
         automatically only once when a reference to any member occurs
         for the first time */
      double value;
      /* evaluated numeric value */
};

static void eval_num_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine to work within domain scope */
      struct eval_num_info *info = _info;
      if (info->memb != NULL)
      {  /* checking call; check numeric value being assigned */
         check_value_num(mpl, info->par, info->memb->tuple,
            info->memb->value.num);
      }
      else
      {  /* normal call; evaluate member, which has given n-tuple */
         info->value = take_member_num(mpl, info->par, info->tuple);
      }
      return;
}

double eval_member_num
(     MPL *mpl,
      PARAMETER *par,         /* not changed */
      TUPLE *tuple            /* not changed */
)
{     /* this routine evaluates numeric parameter member */
      struct eval_num_info _info, *info = &_info;
      insist(par->type == A_NUMERIC || par->type == A_INTEGER ||
             par->type == A_BINARY);
      insist(par->dim == tuple_dimen(mpl, tuple));
      info->par = par;
      info->tuple = tuple;
      if (par->data == 1)
      {  /* check data, which are provided in the data section, but not
            checked yet */
         /* save pointer to the last array member; note that during the
            check new members may be added beyond the last member due to
            references to the same parameter from default expression as
            well as from expressions that define restricting conditions;
            however, values assigned to the new members will be checked
            by other routine, so we don't need to check them here */
         MEMBER *tail = par->array->tail;
         /* change the data status to prevent infinite recursive loop
            due to references to the same parameter during the check */
         par->data = 2;
         /* check values assigned to array members in the data section
            until the marked member has been reached */
         for (info->memb = par->array->head; info->memb != NULL;
            info->memb = info->memb->next)
         {  if (eval_within_domain(mpl, par->domain, info->memb->tuple,
               info, eval_num_func))
               out_of_domain(mpl, par->name, info->memb->tuple);
            if (info->memb == tail) break;
         }
         /* the check has been finished */
      }
      /* evaluate member, which has given n-tuple */
      info->memb = NULL;
      if (eval_within_domain(mpl, info->par->domain, info->tuple, info,
         eval_num_func))
         out_of_domain(mpl, par->name, info->tuple);
      /* bring evaluated value to the calling program */
      return info->value;
}

/*----------------------------------------------------------------------
-- check_value_sym - check symbolic value assigned to parameter member.
--
-- This routine checks if symbolic value being assigned to some member
-- of specified symbolic model parameter satisfies to all restrictions.
--
-- NOTE: This routine must not be called out of domain scope. */

void check_value_sym
(     MPL *mpl,
      PARAMETER *par,         /* not changed */
      TUPLE *tuple,           /* not changed */
      SYMBOL *value           /* not changed */
)
{     CONDITION *cond;
      WITHIN *in;
      int eqno;
      /* the value must satisfy to all specified conditions */
      for (cond = par->cond, eqno = 1; cond != NULL; cond = cond->next,
         eqno++)
      {  SYMBOL *bound;
         char buf[255+1];
         insist(cond->code != NULL);
         bound = eval_symbolic(mpl, cond->code);
         switch (cond->rho)
         {  case O_EQ:
               if (!(compare_symbols(mpl, value, bound) == 0))
               {  strcpy(buf, format_symbol(mpl, bound));
                  insist(strlen(buf) < sizeof(buf));
                  error(mpl, "%s%s = %s not = %s",
                     par->name, format_tuple(mpl, '[', tuple),
                     format_symbol(mpl, value), buf, eqno);
               }
               break;
            case O_NE:
               if (!(compare_symbols(mpl, value, bound) != 0))
               {  strcpy(buf, format_symbol(mpl, bound));
                  insist(strlen(buf) < sizeof(buf));
                  error(mpl, "%s%s = %s not <> %s",
                     par->name, format_tuple(mpl, '[', tuple),
                     format_symbol(mpl, value), buf, eqno);
               }
               break;
            default:
               insist(cond != cond);
         }
         delete_symbol(mpl, bound);
      }
      /* the value must be in all specified supersets */
      for (in = par->in, eqno = 1; in != NULL; in = in->next, eqno++)
      {  TUPLE *dummy;
         insist(in->code != NULL);
         insist(in->code->dim == 1);
         dummy = expand_tuple(mpl, create_tuple(mpl), copy_symbol(mpl,
            value));
         if (!is_member(mpl, in->code, dummy))
            error(mpl, "%s%s = %s not in specified set; see (%d)",
               par->name, format_tuple(mpl, '[', tuple),
               format_symbol(mpl, value), eqno);
         delete_tuple(mpl, dummy);
      }
      return;
}

/*----------------------------------------------------------------------
-- take_member_sym - obtain symb. value assigned to parameter member.
--
-- This routine obtains a symbolic value assigned to member of specified
-- symbolic model parameter and returns it on exit.
--
-- NOTE: This routine must not be called out of domain scope. */

SYMBOL *take_member_sym       /* returns value, not reference */
(     MPL *mpl,
      PARAMETER *par,         /* not changed */
      TUPLE *tuple            /* not changed */
)
{     MEMBER *memb;
      SYMBOL *value;
      /* find member in the parameter array */
      memb = find_member(mpl, par->array, tuple);
      if (memb != NULL)
      {  /* member exists, so just take its value */
         value = copy_symbol(mpl, memb->value.sym);
      }
      else if (par->assign != NULL)
      {  /* compute value using assignment expression */
         value = eval_symbolic(mpl, par->assign);
add:     /* check that the value satisfies to all restrictions, assign
            it to new member, and add the member to the array */
         check_value_sym(mpl, par, tuple, value);
         memb = add_member(mpl, par->array, copy_tuple(mpl, tuple));
         memb->value.sym = copy_symbol(mpl, value);
      }
      else if (par->option != NULL)
      {  /* compute default value */
         value = eval_symbolic(mpl, par->option);
         goto add;
      }
      else if (par->defval != NULL)
      {  /* take default value provided in the data section */
         value = copy_symbol(mpl, par->defval);
         goto add;
      }
      else
      {  /* no value is provided */
         error(mpl, "no value for %s%s", par->name, format_tuple(mpl,
            '[', tuple));
      }
      return value;
}

/*----------------------------------------------------------------------
-- eval_member_sym - evaluate symb. value assigned to parameter member.
--
-- This routine evaluates a symbolic value assigned to given member of
-- specified symbolic model parameter and returns it on exit. */

struct eval_sym_info
{     /* working info used by the routine eval_member_sym */
      PARAMETER *par;
      /* model parameter */
      TUPLE *tuple;
      /* n-tuple, which defines parameter member */
      MEMBER *memb;
      /* normally this pointer is NULL; the routine uses this pointer
         to check data provided in the data section, in which case it
         points to a member currently checked; this check is performed
         automatically only once when a reference to any member occurs
         for the first time */
      SYMBOL *value;
      /* evaluated symbolic value */
};

static void eval_sym_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine to work within domain scope */
      struct eval_sym_info *info = _info;
      if (info->memb != NULL)
      {  /* checking call; check symbolic value being assigned */
         check_value_sym(mpl, info->par, info->memb->tuple,
            info->memb->value.sym);
      }
      else
      {  /* normal call; evaluate member, which has given n-tuple */
         info->value = take_member_sym(mpl, info->par, info->tuple);
      }
      return;
}

SYMBOL *eval_member_sym       /* returns value, not reference */
(     MPL *mpl,
      PARAMETER *par,         /* not changed */
      TUPLE *tuple            /* not changed */
)
{     /* this routine evaluates symbolic parameter member */
      struct eval_sym_info _info, *info = &_info;
      insist(par->type == A_SYMBOLIC);
      insist(par->dim == tuple_dimen(mpl, tuple));
      info->par = par;
      info->tuple = tuple;
      if (par->data == 1)
      {  /* check data, which are provided in the data section, but not
            checked yet */
         /* save pointer to the last array member; note that during the
            check new members may be added beyond the last member due to
            references to the same parameter from default expression as
            well as from expressions that define restricting conditions;
            however, values assigned to the new members will be checked
            by other routine, so we don't need to check them here */
         MEMBER *tail = par->array->tail;
         /* change the data status to prevent infinite recursive loop
            due to references to the same parameter during the check */
         par->data = 2;
         /* check values assigned to array members in the data section
            until the marked member has been reached */
         for (info->memb = par->array->head; info->memb != NULL;
            info->memb = info->memb->next)
         {  if (eval_within_domain(mpl, par->domain, info->memb->tuple,
               info, eval_sym_func))
               out_of_domain(mpl, par->name, info->memb->tuple);
            if (info->memb == tail) break;
         }
         /* the check has been finished */
      }
      /* evaluate member, which has given n-tuple */
      info->memb = NULL;
      if (eval_within_domain(mpl, info->par->domain, info->tuple, info,
         eval_sym_func))
         out_of_domain(mpl, par->name, info->tuple);
      /* bring evaluated value to the calling program */
      return info->value;
}

/*----------------------------------------------------------------------
-- eval_whole_par - evaluate model parameter over entire domain.
--
-- This routine evaluates all members of specified model parameter over
-- entire domain. */

static int whole_par_func(MPL *mpl, void *info)
{     /* this is auxiliary routine to work within domain scope */
      PARAMETER *par = (PARAMETER *)info;
      TUPLE *tuple = get_domain_tuple(mpl, par->domain);
      switch (par->type)
      {  case A_NUMERIC:
         case A_INTEGER:
         case A_BINARY:
            eval_member_num(mpl, par, tuple);
            break;
         case A_SYMBOLIC:
            delete_symbol(mpl, eval_member_sym(mpl, par, tuple));
            break;
         default:
            insist(par != par);
      }
      delete_tuple(mpl, tuple);
      return 0;
}

void eval_whole_par(MPL *mpl, PARAMETER *par)
{     loop_within_domain(mpl, par->domain, par, whole_par_func);
      return;
}

/*----------------------------------------------------------------------
-- clean_parameter - clean model parameter.
--
-- This routine cleans specified model parameter that assumes deleting
-- all stuff dynamically allocated during the generation phase. */

void clean_parameter(MPL *mpl, PARAMETER *par)
{     CONDITION *cond;
      WITHIN *in;
      MEMBER *memb;
      /* clean subscript domain */
      clean_domain(mpl, par->domain);
      /* clean pseudo-code for computing restricting conditions */
      for (cond = par->cond; cond != NULL; cond = cond->next)
         clean_code(mpl, cond->code);
      /* clean pseudo-code for computing restricting supersets */
      for (in = par->in; in != NULL; in = in->next)
         clean_code(mpl, in->code);
      /* clean pseudo-code for computing assigned value */
      clean_code(mpl, par->assign);
      /* clean pseudo-code for computing default value */
      clean_code(mpl, par->option);
      /* reset data status flag */
      par->data = 0;
      /* delete default symbolic value */
      if (par->defval != NULL)
         delete_symbol(mpl, par->defval), par->defval = NULL;
      /* delete content array */
      for (memb = par->array->head; memb != NULL; memb = memb->next)
         delete_value(mpl, par->array->type, &memb->value);
      delete_array(mpl, par->array), par->array = NULL;
      return;
}

/**********************************************************************/
/* * *                      MODEL VARIABLES                       * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- take_member_var - obtain reference to elemental variable.
--
-- This routine obtains a reference to elemental variable assigned to
-- given member of specified model variable and returns it on exit. If
-- necessary, new elemental variable is created.
--
-- NOTE: This routine must not be called out of domain scope. */

ELEMVAR *take_member_var      /* returns reference */
(     MPL *mpl,
      VARIABLE *var,          /* not changed */
      TUPLE *tuple            /* not changed */
)
{     MEMBER *memb;
      ELEMVAR *refer;
      /* find member in the variable array */
      memb = find_member(mpl, var->array, tuple);
      if (memb != NULL)
      {  /* member exists, so just take the reference */
         refer = memb->value.var;
      }
      else
      {  /* member is referenced for the first time and therefore does
            not exist; create new elemental variable, assign it to new
            member, and add the member to the variable array */
         memb = add_member(mpl, var->array, copy_tuple(mpl, tuple));
         refer = (memb->value.var = dmp_get_atom(mpl->elemvars));
         refer->j = 0;
         refer->var = var;
         refer->memb = memb;
         /* compute lower bound */
         if (var->lbnd == NULL)
            refer->lbnd = 0.0;
         else
            refer->lbnd = eval_numeric(mpl, var->lbnd);
         /* compute upper bound */
         if (var->ubnd == NULL)
            refer->ubnd = 0.0;
         else if (var->ubnd == var->lbnd)
            refer->ubnd = refer->lbnd;
         else
            refer->ubnd = eval_numeric(mpl, var->ubnd);
         /* nullify working quantity */
         refer->temp = 0.0;
      }
      return refer;
}

/*----------------------------------------------------------------------
-- eval_member_var - evaluate reference to elemental variable.
--
-- This routine evaluates a reference to elemental variable assigned to
-- member of specified model variable and returns it on exit. */

struct eval_var_info
{     /* working info used by the routine eval_member_var */
      VARIABLE *var;
      /* model variable */
      TUPLE *tuple;
      /* n-tuple, which defines variable member */
      ELEMVAR *refer;
      /* evaluated reference to elemental variable */
};

static void eval_var_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine to work within domain scope */
      struct eval_var_info *info = _info;
      info->refer = take_member_var(mpl, info->var, info->tuple);
      return;
}

ELEMVAR *eval_member_var      /* returns reference */
(     MPL *mpl,
      VARIABLE *var,          /* not changed */
      TUPLE *tuple            /* not changed */
)
{     /* this routine evaluates variable member */
      struct eval_var_info _info, *info = &_info;
      insist(var->dim == tuple_dimen(mpl, tuple));
      info->var = var;
      info->tuple = tuple;
      /* evaluate member, which has given n-tuple */
      if (eval_within_domain(mpl, info->var->domain, info->tuple, info,
         eval_var_func))
         out_of_domain(mpl, var->name, info->tuple);
      /* bring evaluated reference to the calling program */
      return info->refer;
}

/*----------------------------------------------------------------------
-- eval_whole_var - evaluate model variable over entire domain.
--
-- This routine evaluates all members of specified model variable over
-- entire domain. */

static int whole_var_func(MPL *mpl, void *info)
{     /* this is auxiliary routine to work within domain scope */
      VARIABLE *var = (VARIABLE *)info;
      TUPLE *tuple = get_domain_tuple(mpl, var->domain);
      eval_member_var(mpl, var, tuple);
      delete_tuple(mpl, tuple);
      return 0;
}

void eval_whole_var(MPL *mpl, VARIABLE *var)
{     loop_within_domain(mpl, var->domain, var, whole_var_func);
      return;
}

/*----------------------------------------------------------------------
-- clean_variable - clean model variable.
--
-- This routine cleans specified model variable that assumes deleting
-- all stuff dynamically allocated during the generation phase. */

void clean_variable(MPL *mpl, VARIABLE *var)
{     MEMBER *memb;
      /* clean subscript domain */
      clean_domain(mpl, var->domain);
      /* clean code for computing lower bound */
      clean_code(mpl, var->lbnd);
      /* clean code for computing upper bound */
      if (var->ubnd != var->lbnd) clean_code(mpl, var->ubnd);
      /* delete content array */
      for (memb = var->array->head; memb != NULL; memb = memb->next)
         dmp_free_atom(mpl->elemvars, memb->value.var);
      delete_array(mpl, var->array), var->array = NULL;
      return;
}

/**********************************************************************/
/* * *              MODEL CONSTRAINTS AND OBJECTIVES              * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- take_member_con - obtain reference to elemental constraint.
--
-- This routine obtains a reference to elemental constraint assigned
-- to given member of specified model constraint and returns it on exit.
-- If necessary, new elemental constraint is created.
--
-- NOTE: This routine must not be called out of domain scope. */

ELEMCON *take_member_con      /* returns reference */
(     MPL *mpl,
      CONSTRAINT *con,        /* not changed */
      TUPLE *tuple            /* not changed */
)
{     MEMBER *memb;
      ELEMCON *refer;
      /* find member in the constraint array */
      memb = find_member(mpl, con->array, tuple);
      if (memb != NULL)
      {  /* member exists, so just take the reference */
         refer = memb->value.con;
      }
      else
      {  /* member is referenced for the first time and therefore does
            not exist; create new elemental constraint, assign it to new
            member, and add the member to the constraint array */
         memb = add_member(mpl, con->array, copy_tuple(mpl, tuple));
         refer = (memb->value.con = dmp_get_atom(mpl->elemcons));
         refer->i = 0;
         refer->con = con;
         refer->memb = memb;
         /* compute linear form */
         insist(con->code != NULL);
         refer->form = eval_formula(mpl, con->code);
         /* compute lower and upper bounds */
         if (con->lbnd == NULL && con->ubnd == NULL)
         {  /* objective has no bounds */
            double temp;
            insist(con->type == A_MINIMIZE || con->type == A_MAXIMIZE);
            /* carry the constant term to the right-hand side */
            refer->form = remove_constant(mpl, refer->form, &temp);
            refer->lbnd = refer->ubnd = - temp;
         }
         else if (con->lbnd != NULL && con->ubnd == NULL)
         {  /* constraint a * x + b >= c * y + d is transformed to the
               standard form a * x - c * y >= d - b */
            double temp;
            insist(con->type == A_CONSTRAINT);
            refer->form = linear_comb(mpl,
               +1.0, refer->form,
               -1.0, eval_formula(mpl, con->lbnd));
            refer->form = remove_constant(mpl, refer->form, &temp);
            refer->lbnd = - temp;
            refer->ubnd = 0.0;
         }
         else if (con->lbnd == NULL && con->ubnd != NULL)
         {  /* constraint a * x + b <= c * y + d is transformed to the
               standard form a * x - c * y <= d - b */
            double temp;
            insist(con->type == A_CONSTRAINT);
            refer->form = linear_comb(mpl,
               +1.0, refer->form,
               -1.0, eval_formula(mpl, con->ubnd));
            refer->form = remove_constant(mpl, refer->form, &temp);
            refer->lbnd = 0.0;
            refer->ubnd = - temp;
         }
         else if (con->lbnd == con->ubnd)
         {  /* constraint a * x + b = c * y + d is transformed to the
               standard form a * x - c * y = d - b */
            double temp;
            insist(con->type == A_CONSTRAINT);
            refer->form = linear_comb(mpl,
               +1.0, refer->form,
               -1.0, eval_formula(mpl, con->lbnd));
            refer->form = remove_constant(mpl, refer->form, &temp);
            refer->lbnd = refer->ubnd = - temp;
         }
         else
         {  /* ranged constraint c <= a * x + b <= d is transformed to
               the standard form c - b <= a * x <= d - b */
            double temp, temp1, temp2;
            insist(con->type == A_CONSTRAINT);
            refer->form = remove_constant(mpl, refer->form, &temp);
            insist(remove_constant(mpl, eval_formula(mpl, con->lbnd),
               &temp1) == NULL);
            insist(remove_constant(mpl, eval_formula(mpl, con->ubnd),
               &temp2) == NULL);
            refer->lbnd = fp_sub(mpl, temp1, temp);
            refer->ubnd = fp_sub(mpl, temp2, temp);
         }
      }
      return refer;
}

/*----------------------------------------------------------------------
-- eval_member_con - evaluate reference to elemental constraint.
--
-- This routine evaluates a reference to elemental constraint assigned
-- to member of specified model constraint and returns it on exit. */

struct eval_con_info
{     /* working info used by the routine eval_member_con */
      CONSTRAINT *con;
      /* model constraint */
      TUPLE *tuple;
      /* n-tuple, which defines constraint member */
      ELEMCON *refer;
      /* evaluated reference to elemental constraint */
};

static void eval_con_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine to work within domain scope */
      struct eval_con_info *info = _info;
      info->refer = take_member_con(mpl, info->con, info->tuple);
      return;
}

ELEMCON *eval_member_con      /* returns reference */
(     MPL *mpl,
      CONSTRAINT *con,        /* not changed */
      TUPLE *tuple            /* not changed */
)
{     /* this routine evaluates constraint member */
      struct eval_con_info _info, *info = &_info;
      insist(con->dim == tuple_dimen(mpl, tuple));
      info->con = con;
      info->tuple = tuple;
      /* evaluate member, which has given n-tuple */
      if (eval_within_domain(mpl, info->con->domain, info->tuple, info,
         eval_con_func))
         out_of_domain(mpl, con->name, info->tuple);
      /* bring evaluated reference to the calling program */
      return info->refer;
}

/*----------------------------------------------------------------------
-- eval_whole_con - evaluate model constraint over entire domain.
--
-- This routine evaluates all members of specified model constraint over
-- entire domain. */

static int whole_con_func(MPL *mpl, void *info)
{     /* this is auxiliary routine to work within domain scope */
      CONSTRAINT *con = (CONSTRAINT *)info;
      TUPLE *tuple = get_domain_tuple(mpl, con->domain);
      eval_member_con(mpl, con, tuple);
      delete_tuple(mpl, tuple);
      return 0;
}

void eval_whole_con(MPL *mpl, CONSTRAINT *con)
{     loop_within_domain(mpl, con->domain, con, whole_con_func);
      return;
}

/*----------------------------------------------------------------------
-- clean_constraint - clean model constraint.
--
-- This routine cleans specified model constraint that assumes deleting
-- all stuff dynamically allocated during the generation phase. */

void clean_constraint(MPL *mpl, CONSTRAINT *con)
{     MEMBER *memb;
      /* clean subscript domain */
      clean_domain(mpl, con->domain);
      /* clean code for computing main linear form */
      clean_code(mpl, con->code);
      /* clean code for computing lower bound */
      clean_code(mpl, con->lbnd);
      /* clean code for computing upper bound */
      if (con->ubnd != con->lbnd) clean_code(mpl, con->ubnd);
      /* delete content array */
      for (memb = con->array->head; memb != NULL; memb = memb->next)
      {  delete_formula(mpl, memb->value.con->form);
         dmp_free_atom(mpl->elemcons, memb->value.con);
      }
      delete_array(mpl, con->array), con->array = NULL;
      return;
}

/**********************************************************************/
/* * *                        PSEUDO-CODE                         * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- eval_numeric - evaluate pseudo-code to determine numeric value.
--
-- This routine evaluates specified pseudo-code to determine resultant
-- numeric value, which is returned on exit. */

struct iter_num_info
{     /* working info used by the routine iter_num_func */
      CODE *code;
      /* pseudo-code for iterated operation to be performed */
      double value;
      /* resultant value */
};

static int iter_num_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine used to perform iterated operation
         on numeric "integrand" within domain scope */
      struct iter_num_info *info = _info;
      double temp;
      temp = eval_numeric(mpl, info->code->arg.loop.x);
      switch (info->code->op)
      {  case O_SUM:
            /* summation over domain */
            info->value = fp_add(mpl, info->value, temp);
            break;
         case O_PROD:
            /* multiplication over domain */
            info->value = fp_mul(mpl, info->value, temp);
            break;
         case O_MINIMUM:
            /* minimum over domain */
            if (info->value > temp) info->value = temp;
            break;
         case O_MAXIMUM:
            /* maximum over domain */
            if (info->value < temp) info->value = temp;
            break;
         default:
            insist(info != info);
      }
      return 0;
}

double eval_numeric(MPL *mpl, CODE *code)
{     double value;
      insist(code != NULL);
      insist(code->type == A_NUMERIC);
      insist(code->dim == 0);
      /* if resultant value is valid, no evaluation is needed */
      if (code->valid)
      {  value = code->value.num;
         goto done;
      }
      /* evaluate pseudo-code recursively */
      switch (code->op)
      {  case O_NUMBER:
            /* take floating-point number */
            value = code->arg.num;
            break;
         case O_MEMNUM:
            /* take member of numeric parameter */
            {  TUPLE *tuple;
               ARG_LIST *e;
               tuple = create_tuple(mpl);
               for (e = code->arg.par.list; e != NULL; e = e->next)
                  tuple = expand_tuple(mpl, tuple, eval_symbolic(mpl,
                     e->x));
               value = eval_member_num(mpl, code->arg.par.par, tuple);
               delete_tuple(mpl, tuple);
            }
            break;
         case O_CVTNUM:
            /* conversion to numeric */
            {  SYMBOL *sym;
               sym = eval_symbolic(mpl, code->arg.arg.x);
               if (sym->str != NULL)
                  error(mpl, "cannot convert %s to floating-point numbe"
                     "r", format_symbol(mpl, sym));
               value = sym->num;
               delete_symbol(mpl, sym);
            }
            break;
         case O_PLUS:
            /* unary plus */
            value = + eval_numeric(mpl, code->arg.arg.x);
            break;
         case O_MINUS:
            /* unary minus */
            value = - eval_numeric(mpl, code->arg.arg.x);
            break;
         case O_ABS:
            /* absolute value */
            value = fabs(eval_numeric(mpl, code->arg.arg.x));
            break;
         case O_CEIL:
            /* round upward ("ceiling of x") */
            value = ceil(eval_numeric(mpl, code->arg.arg.x));
            break;
         case O_FLOOR:
            /* round downward ("floor of x") */
            value = floor(eval_numeric(mpl, code->arg.arg.x));
            break;
         case O_EXP:
            /* base-e exponential */
            value = fp_exp(mpl, eval_numeric(mpl, code->arg.arg.x));
            break;
         case O_LOG:
            /* natural logarithm */
            value = fp_log(mpl, eval_numeric(mpl, code->arg.arg.x));
            break;
         case O_LOG10:
            /* common (decimal) logarithm */
            value = fp_log10(mpl, eval_numeric(mpl, code->arg.arg.x));
            break;
         case O_SQRT:
            /* square root */
            value = fp_sqrt(mpl, eval_numeric(mpl, code->arg.arg.x));
            break;
         case O_ADD:
            /* addition */
            value = fp_add(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_SUB:
            /* subtraction */
            value = fp_sub(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_LESS:
            /* non-negative subtraction */
            value = fp_less(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_MUL:
            /* multiplication */
            value = fp_mul(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_DIV:
            /* division */
            value = fp_div(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_IDIV:
            /* quotient of exact division */
            value = fp_idiv(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_MOD:
            /* remainder of exact division */
            value = fp_mod(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_POWER:
            /* exponentiation (raise to power) */
            value = fp_power(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_FORK:
            /* if-then-else */
            if (eval_logical(mpl, code->arg.arg.x))
               value = eval_numeric(mpl, code->arg.arg.y);
            else if (code->arg.arg.z == NULL)
               value = 0.0;
            else
               value = eval_numeric(mpl, code->arg.arg.z);
            break;
         case O_MIN:
            /* minimal value (n-ary) */
            {  ARG_LIST *e;
               double temp;
               value = +DBL_MAX;
               for (e = code->arg.list; e != NULL; e = e->next)
               {  temp = eval_numeric(mpl, e->x);
                  if (value > temp) value = temp;
               }
            }
            break;
         case O_MAX:
            /* maximal value (n-ary) */
            {  ARG_LIST *e;
               double temp;
               value = -DBL_MAX;
               for (e = code->arg.list; e != NULL; e = e->next)
               {  temp = eval_numeric(mpl, e->x);
                  if (value < temp) value = temp;
               }
            }
            break;
         case O_SUM:
            /* summation over domain */
            {  struct iter_num_info _info, *info = &_info;
               info->code = code;
               info->value = 0.0;
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_num_func);
               value = info->value;
            }
            break;
         case O_PROD:
            /* multiplication over domain */
            {  struct iter_num_info _info, *info = &_info;
               info->code = code;
               info->value = 1.0;
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_num_func);
               value = info->value;
            }
            break;
         case O_MINIMUM:
            /* minimum over domain */
            {  struct iter_num_info _info, *info = &_info;
               info->code = code;
               info->value = +DBL_MAX;
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_num_func);
               if (info->value == +DBL_MAX)
                  error(mpl, "min{} over empty set; result undefined");
               value = info->value;
            }
            break;
         case O_MAXIMUM:
            /* maximum over domain */
            {  struct iter_num_info _info, *info = &_info;
               info->code = code;
               info->value = -DBL_MAX;
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_num_func);
               if (info->value == -DBL_MAX)
                  error(mpl, "max{} over empty set; result undefined");
               value = info->value;
            }
            break;
         default:
            insist(code != code);
      }
      /* save resultant value */
      insist(!code->valid);
      code->valid = 1;
      code->value.num = value;
done: return value;
}

/*----------------------------------------------------------------------
-- eval_symbolic - evaluate pseudo-code to determine symbolic value.
--
-- This routine evaluates specified pseudo-code to determine resultant
-- symbolic value, which is returned on exit. */

SYMBOL *eval_symbolic(MPL *mpl, CODE *code)
{     SYMBOL *value;
      insist(code != NULL);
      insist(code->type == A_SYMBOLIC);
      insist(code->dim == 0);
      /* if resultant value is valid, no evaluation is needed */
      if (code->valid)
      {  value = copy_symbol(mpl, code->value.sym);
         goto done;
      }
      /* evaluate pseudo-code recursively */
      switch (code->op)
      {  case O_STRING:
            /* take character string */
            value = create_symbol_str(mpl, create_string(mpl,
               code->arg.str));
            break;
         case O_INDEX:
            /* take dummy index */
            insist(code->arg.index.slot->value != NULL);
            value = copy_symbol(mpl, code->arg.index.slot->value);
            break;
         case O_MEMSYM:
            /* take member of symbolic parameter */
            {  TUPLE *tuple;
               ARG_LIST *e;
               tuple = create_tuple(mpl);
               for (e = code->arg.par.list; e != NULL; e = e->next)
                  tuple = expand_tuple(mpl, tuple, eval_symbolic(mpl,
                     e->x));
               value = eval_member_sym(mpl, code->arg.par.par, tuple);
               delete_tuple(mpl, tuple);
            }
            break;
         case O_CVTSYM:
            /* conversion to symbolic */
            value = create_symbol_num(mpl, eval_numeric(mpl,
               code->arg.arg.x));
            break;
         case O_CONCAT:
            /* concatenation */
            value = concat_symbols(mpl,
               eval_symbolic(mpl, code->arg.arg.x),
               eval_symbolic(mpl, code->arg.arg.y));
            break;
         case O_FORK:
            /* if-then-else */
            if (eval_logical(mpl, code->arg.arg.x))
               value = eval_symbolic(mpl, code->arg.arg.y);
            else if (code->arg.arg.z == NULL)
               value = create_symbol_num(mpl, 0.0);
            else
               value = eval_symbolic(mpl, code->arg.arg.z);
            break;
         default:
            insist(code != code);
      }
      /* save resultant value */
      insist(!code->valid);
      code->valid = 1;
      code->value.sym = copy_symbol(mpl, value);
done: return value;
}

/*----------------------------------------------------------------------
-- eval_logical - evaluate pseudo-code to determine logical value.
--
-- This routine evaluates specified pseudo-code to determine resultant
-- logical value, which is returned on exit. */

struct iter_log_info
{     /* working info used by the routine iter_log_func */
      CODE *code;
      /* pseudo-code for iterated operation to be performed */
      int value;
      /* resultant value */
};

static int iter_log_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine used to perform iterated operation
         on logical "integrand" within domain scope */
      struct iter_log_info *info = _info;
      int ret = 0;
      switch (info->code->op)
      {  case O_FORALL:
            /* conjunction over domain */
            info->value &= eval_logical(mpl, info->code->arg.loop.x);
            if (!info->value) ret = 1;
            break;
         case O_EXISTS:
            /* disjunction over domain */
            info->value |= eval_logical(mpl, info->code->arg.loop.x);
            if (info->value) ret = 1;
            break;
         default:
            insist(info != info);
      }
      return ret;
}

int eval_logical(MPL *mpl, CODE *code)
{     int value;
      insist(code->type == A_LOGICAL);
      insist(code->dim == 0);
      /* if resultant value is valid, no evaluation is needed */
      if (code->valid)
      {  value = code->value.bit;
         goto done;
      }
      /* evaluate pseudo-code recursively */
      switch (code->op)
      {  case O_CVTLOG:
            /* conversion to logical */
            value = (eval_numeric(mpl, code->arg.arg.x) != 0.0);
            break;
         case O_NOT:
            /* negation (logical "not") */
            value = !eval_logical(mpl, code->arg.arg.x);
            break;
         case O_LT:
            /* comparison on 'less than' */
            value = (eval_numeric(mpl, code->arg.arg.x) <
                     eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_LE:
            /* comparison on 'not greater than' */
            value = (eval_numeric(mpl, code->arg.arg.x) <=
                     eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_EQ:
            /* comparison on 'equal to' */
            insist(code->arg.arg.x != NULL);
            if (code->arg.arg.x->type == A_NUMERIC)
               value = (eval_numeric(mpl, code->arg.arg.x) ==
                        eval_numeric(mpl, code->arg.arg.y));
            else
            {  SYMBOL *sym1 = eval_symbolic(mpl, code->arg.arg.x);
               SYMBOL *sym2 = eval_symbolic(mpl, code->arg.arg.y);
               value = (compare_symbols(mpl, sym1, sym2) == 0);
               delete_symbol(mpl, sym1);
               delete_symbol(mpl, sym2);
            }
            break;
         case O_GE:
            /* comparison on 'not less than' */
            value = (eval_numeric(mpl, code->arg.arg.x) >=
                     eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_GT:
            /* comparison on 'greater than' */
            value = (eval_numeric(mpl, code->arg.arg.x) >
                     eval_numeric(mpl, code->arg.arg.y));
            break;
         case O_NE:
            /* comparison on 'not equal to' */
            insist(code->arg.arg.x != NULL);
            if (code->arg.arg.x->type == A_NUMERIC)
               value = (eval_numeric(mpl, code->arg.arg.x) !=
                        eval_numeric(mpl, code->arg.arg.y));
            else
            {  SYMBOL *sym1 = eval_symbolic(mpl, code->arg.arg.x);
               SYMBOL *sym2 = eval_symbolic(mpl, code->arg.arg.y);
               value = (compare_symbols(mpl, sym1, sym2) != 0);
               delete_symbol(mpl, sym1);
               delete_symbol(mpl, sym2);
            }
            break;
         case O_AND:
            /* conjunction (logical "and") */
            value = eval_logical(mpl, code->arg.arg.x) &&
                    eval_logical(mpl, code->arg.arg.y);
            break;
         case O_OR:
            /* disjunction (logical "or") */
            value = eval_logical(mpl, code->arg.arg.x) ||
                    eval_logical(mpl, code->arg.arg.y);
            break;
         case O_IN:
            /* test on 'x in Y' */
            {  TUPLE *tuple;
               tuple = eval_tuple(mpl, code->arg.arg.x);
               value = is_member(mpl, code->arg.arg.y, tuple);
               delete_tuple(mpl, tuple);
            }
            break;
         case O_NOTIN:
            /* test on 'x not in Y' */
            {  TUPLE *tuple;
               tuple = eval_tuple(mpl, code->arg.arg.x);
               value = !is_member(mpl, code->arg.arg.y, tuple);
               delete_tuple(mpl, tuple);
            }
            break;
         case O_WITHIN:
            /* test on 'X within Y' */
            {  ELEMSET *set;
               MEMBER *memb;
               set = eval_elemset(mpl, code->arg.arg.x);
               value = 1;
               for (memb = set->head; memb != NULL; memb = memb->next)
               {  if (!is_member(mpl, code->arg.arg.y, memb->tuple))
                  {  value = 0;
                     break;
                  }
               }
               delete_elemset(mpl, set);
            }
            break;
         case O_NOTWITHIN:
            /* test on 'X not within Y' */
            {  ELEMSET *set;
               MEMBER *memb;
               set = eval_elemset(mpl, code->arg.arg.x);
               value = 1;
               for (memb = set->head; memb != NULL; memb = memb->next)
               {  if (is_member(mpl, code->arg.arg.y, memb->tuple))
                  {  value = 0;
                     break;
                  }
               }
               delete_elemset(mpl, set);
            }
            break;
         case O_FORALL:
            /* conjunction (A-quantification) */
            {  struct iter_log_info _info, *info = &_info;
               info->code = code;
               info->value = 1;
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_log_func);
               value = info->value;
            }
            break;
         case O_EXISTS:
            /* disjunction (E-quantification) */
            {  struct iter_log_info _info, *info = &_info;
               info->code = code;
               info->value = 0;
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_log_func);
               value = info->value;
            }
            break;
         default:
            insist(code != code);
      }
      /* save resultant value */
      insist(!code->valid);
      code->valid = 1;
      code->value.bit = value;
done: return value;
}

/*----------------------------------------------------------------------
-- eval_tuple - evaluate pseudo-code to construct n-tuple.
--
-- This routine evaluates specified pseudo-code to construct resultant
-- n-tuple, which is returned on exit. */

TUPLE *eval_tuple(MPL *mpl, CODE *code)
{     TUPLE *value;
      insist(code != NULL);
      insist(code->type == A_TUPLE);
      insist(code->dim > 0);
      /* if resultant value is valid, no evaluation is needed */
      if (code->valid)
      {  value = copy_tuple(mpl, code->value.tuple);
         goto done;
      }
      /* evaluate pseudo-code recursively */
      switch (code->op)
      {  case O_TUPLE:
            /* make n-tuple */
            {  ARG_LIST *e;
               value = create_tuple(mpl);
               for (e = code->arg.list; e != NULL; e = e->next)
                  value = expand_tuple(mpl, value, eval_symbolic(mpl,
                     e->x));
            }
            break;
         case O_CVTTUP:
            /* convert to 1-tuple */
            value = expand_tuple(mpl, create_tuple(mpl),
               eval_symbolic(mpl, code->arg.arg.x));
            break;
         default:
            insist(code != code);
      }
      /* save resultant value */
      insist(!code->valid);
      code->valid = 1;
      code->value.tuple = copy_tuple(mpl, value);
done: return value;
}

/*----------------------------------------------------------------------
-- eval_elemset - evaluate pseudo-code to construct elemental set.
--
-- This routine evaluates specified pseudo-code to construct resultant
-- elemental set, which is returned on exit. */

struct iter_set_info
{     /* working info used by the routine iter_set_func */
      CODE *code;
      /* pseudo-code for iterated operation to be performed */
      ELEMSET *value;
      /* resultant value */
};

static int iter_set_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine used to perform iterated operation
         on n-tuple "integrand" within domain scope */
      struct iter_set_info *info = _info;
      TUPLE *tuple;
      switch (info->code->op)
      {  case O_SETOF:
            /* compute next n-tuple and add it to the set; in this case
               duplicate n-tuples are silently ignored */
            tuple = eval_tuple(mpl, info->code->arg.loop.x);
            if (find_tuple(mpl, info->value, tuple) == NULL)
               add_tuple(mpl, info->value, tuple);
            else
               delete_tuple(mpl, tuple);
            break;
         case O_BUILD:
            /* construct next n-tuple using current values assigned to
               *free* dummy indices as its components and add it to the
               set; in this case duplicate n-tuples cannot appear */
            add_tuple(mpl, info->value, get_domain_tuple(mpl,
               info->code->arg.loop.domain));
            break;
         default:
            insist(info != info);
      }
      return 0;
}

ELEMSET *eval_elemset(MPL *mpl, CODE *code)
{     ELEMSET *value;
      insist(code != NULL);
      insist(code->type == A_ELEMSET);
      insist(code->dim > 0);
      /* if resultant value is valid, no evaluation is needed */
      if (code->valid)
      {  value = copy_elemset(mpl, code->value.set);
         goto done;
      }
      /* evaluate pseudo-code recursively */
      switch (code->op)
      {  case O_MEMSET:
            /* take member of set */
            {  TUPLE *tuple;
               ARG_LIST *e;
               tuple = create_tuple(mpl);
               for (e = code->arg.set.list; e != NULL; e = e->next)
                  tuple = expand_tuple(mpl, tuple, eval_symbolic(mpl,
                     e->x));
               value = copy_elemset(mpl,
                  eval_member_set(mpl, code->arg.set.set, tuple));
               delete_tuple(mpl, tuple);
            }
            break;
         case O_MAKE:
            /* make elemental set of n-tuples */
            {  ARG_LIST *e;
               value = create_elemset(mpl, code->dim);
               for (e = code->arg.list; e != NULL; e = e->next)
                  check_then_add(mpl, value, eval_tuple(mpl, e->x));
            }
            break;
         case O_UNION:
            /* union of two elemental sets */
            value = set_union(mpl,
               eval_elemset(mpl, code->arg.arg.x),
               eval_elemset(mpl, code->arg.arg.y));
            break;
         case O_DIFF:
            /* difference between two elemental sets */
            value = set_diff(mpl,
               eval_elemset(mpl, code->arg.arg.x),
               eval_elemset(mpl, code->arg.arg.y));
            break;
         case O_SYMDIFF:
            /* symmetric difference between two elemental sets */
            value = set_symdiff(mpl,
               eval_elemset(mpl, code->arg.arg.x),
               eval_elemset(mpl, code->arg.arg.y));
            break;
         case O_INTER:
            /* intersection of two elemental sets */
            value = set_inter(mpl,
               eval_elemset(mpl, code->arg.arg.x),
               eval_elemset(mpl, code->arg.arg.y));
            break;
         case O_CROSS:
            /* cross (Cartesian) product of two elemental sets */
            value = set_cross(mpl,
               eval_elemset(mpl, code->arg.arg.x),
               eval_elemset(mpl, code->arg.arg.y));
            break;
         case O_DOTS:
            /* build "arithmetic" elemental set */
            value = create_arelset(mpl,
               eval_numeric(mpl, code->arg.arg.x),
               eval_numeric(mpl, code->arg.arg.y),
               code->arg.arg.z == NULL ? 1.0 : eval_numeric(mpl,
                  code->arg.arg.z));
            break;
         case O_SETOF:
            /* compute elemental set */
            {  struct iter_set_info _info, *info = &_info;
               info->code = code;
               info->value = create_elemset(mpl, code->dim);
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_set_func);
               value = info->value;
            }
            break;
         case O_BUILD:
            /* build elemental set identical to domain set */
            {  struct iter_set_info _info, *info = &_info;
               info->code = code;
               info->value = create_elemset(mpl, code->dim);
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_set_func);
               value = info->value;
            }
            break;
         default:
            insist(code != code);
      }
      /* save resultant value */
      insist(!code->valid);
      code->valid = 1;
      code->value.set = copy_elemset(mpl, value);
done: return value;
}

/*----------------------------------------------------------------------
-- is_member - check if n-tuple is in set specified by pseudo-code.
--
-- This routine checks if given n-tuple is a member of elemental set
-- specified in the form of pseudo-code (i.e. by expression).
--
-- The n-tuple may have more components that dimension of the elemental
-- set, in which case the extra components are ignored. */

static void null_func(MPL *mpl, void *info)
{     /* this is dummy routine used to enter the domain scope */
      insist(mpl == mpl);
      insist(info == NULL);
      return;
}

int is_member(MPL *mpl, CODE *code, TUPLE *tuple)
{     int value;
      insist(code != NULL);
      insist(code->type == A_ELEMSET);
      insist(code->dim > 0);
      insist(tuple != NULL);
      switch (code->op)
      {  case O_MEMSET:
            /* check if given n-tuple is member of elemental set, which
               is assigned to member of model set */
            {  ARG_LIST *e;
               TUPLE *temp;
               ELEMSET *set;
               /* evaluate reference to elemental set */
               temp = create_tuple(mpl);
               for (e = code->arg.set.list; e != NULL; e = e->next)
                  temp = expand_tuple(mpl, temp, eval_symbolic(mpl,
                     e->x));
               set = eval_member_set(mpl, code->arg.set.set, temp);
               delete_tuple(mpl, temp);
               /* check if the n-tuple is contained in the set array */
               temp = build_subtuple(mpl, tuple, set->dim);
               value = (find_tuple(mpl, set, temp) != NULL);
               delete_tuple(mpl, temp);
            }
            break;
         case O_MAKE:
            /* check if given n-tuple is member of literal set */
            {  ARG_LIST *e;
               TUPLE *temp, *that;
               value = 0;
               temp = build_subtuple(mpl, tuple, code->dim);
               for (e = code->arg.list; e != NULL; e = e->next)
               {  that = eval_tuple(mpl, e->x);
                  value = (compare_tuples(mpl, temp, that) == 0);
                  delete_tuple(mpl, that);
                  if (value) break;
               }
               delete_tuple(mpl, temp);
            }
            break;
         case O_UNION:
            value = is_member(mpl, code->arg.arg.x, tuple) ||
                    is_member(mpl, code->arg.arg.y, tuple);
            break;
         case O_DIFF:
            value = is_member(mpl, code->arg.arg.x, tuple) &&
                   !is_member(mpl, code->arg.arg.y, tuple);
            break;
         case O_SYMDIFF:
            {  int in1 = is_member(mpl, code->arg.arg.x, tuple);
               int in2 = is_member(mpl, code->arg.arg.y, tuple);
               value = (in1 && !in2) || (!in1 && in2);
            }
            break;
         case O_INTER:
            value = is_member(mpl, code->arg.arg.x, tuple) &&
                    is_member(mpl, code->arg.arg.y, tuple);
            break;
         case O_CROSS:
            {  int j;
               value = is_member(mpl, code->arg.arg.x, tuple);
               if (value)
               {  for (j = 1; j <= code->arg.arg.x->dim; j++)
                  {  insist(tuple != NULL);
                     tuple = tuple->next;
                  }
                  value = is_member(mpl, code->arg.arg.y, tuple);
               }
            }
            break;
         case O_DOTS:
            /* check if given 1-tuple is member of "arithmetic" set */
            {  int j;
               double x, t0, tf, dt;
               insist(code->dim == 1);
               /* compute "parameters" of the "arithmetic" set */
               t0 = eval_numeric(mpl, code->arg.arg.x);
               tf = eval_numeric(mpl, code->arg.arg.y);
               if (code->arg.arg.z == NULL)
                  dt = 1.0;
               else
                  dt = eval_numeric(mpl, code->arg.arg.z);
               /* make sure the parameters are correct */
               arelset_size(mpl, t0, tf, dt);
               /* if component of 1-tuple is symbolic, not numeric, the
                  1-tuple cannot be member of "arithmetic" set */
               insist(tuple->sym != NULL);
               if (tuple->sym->str != NULL)
               {  value = 0;
                  break;
               }
               /* determine numeric value of the component */
               x = tuple->sym->num;
               /* if the component value is out of the set range, the
                  1-tuple is not in the set */
               if (dt > 0.0 && !(t0 <= x && x <= tf) ||
                   dt < 0.0 && !(tf <= x && x <= t0))
               {  value = 0;
                  break;
               }
               /* estimate ordinal number of the 1-tuple in the set */
               j = (int)(((x - t0) / dt) + 0.5) + 1;
               /* perform the main check */
               value = (arelset_member(mpl, t0, tf, dt, j) == x);
            }
            break;
         case O_SETOF:
            /* check if given n-tuple is member of computed set */
            /* it is not clear how to efficiently perform the check not
               computing the entire elemental set :+( */
            error(mpl, "implementation restriction; in/within setof{} n"
               "ot allowed");
            break;
         case O_BUILD:
            /* check if given n-tuple is member of domain set */
            {  TUPLE *temp;
               temp = build_subtuple(mpl, tuple, code->dim);
               /* try to enter the domain scope; if it is successful,
                  the n-tuple is in the domain set */
               value = (eval_within_domain(mpl, code->arg.loop.domain,
                  temp, NULL, null_func) == 0);
               delete_tuple(mpl, temp);
            }
            break;
         default:
            insist(code != code);
      }
      return value;
}

/*----------------------------------------------------------------------
-- eval_formula - evaluate pseudo-code to construct linear form.
--
-- This routine evaluates specified pseudo-code to construct resultant
-- linear form, which is returned on exit. */

struct iter_form_info
{     /* working info used by the routine iter_form_func */
      CODE *code;
      /* pseudo-code for iterated operation to be performed */
      FORMULA *value;
      /* resultant value */
};

static int iter_form_func(MPL *mpl, void *_info)
{     /* this is auxiliary routine used to perform iterated operation
         on linear form "integrand" within domain scope */
      struct iter_form_info *info = _info;
      switch (info->code->op)
      {  case O_SUM:
            /* summation over domain */
            info->value =
               linear_comb(mpl,
                  +1.0, info->value,
                  +1.0, eval_formula(mpl, info->code->arg.loop.x));
            break;
         default:
            insist(info != info);
      }
      return 0;
}

FORMULA *eval_formula(MPL *mpl, CODE *code)
{     FORMULA *value;
      insist(code != NULL);
      insist(code->type == A_FORMULA);
      insist(code->dim == 0);
      /* if resultant value is valid, no evaluation is needed */
      if (code->valid)
      {  value = copy_formula(mpl, code->value.form);
         goto done;
      }
      /* evaluate pseudo-code recursively */
      switch (code->op)
      {  case O_MEMVAR:
            /* take member of variable */
            {  TUPLE *tuple;
               ARG_LIST *e;
               tuple = create_tuple(mpl);
               for (e = code->arg.var.list; e != NULL; e = e->next)
                  tuple = expand_tuple(mpl, tuple, eval_symbolic(mpl,
                     e->x));
               value = single_variable(mpl,
                  eval_member_var(mpl, code->arg.var.var, tuple));
               delete_tuple(mpl, tuple);
            }
            break;
         case O_CVTLFM:
            /* convert to linear form */
            value = constant_term(mpl, eval_numeric(mpl,
               code->arg.arg.x));
            break;
         case O_PLUS:
            /* unary plus */
            value = linear_comb(mpl,
                0.0, constant_term(mpl, 0.0),
               +1.0, eval_formula(mpl, code->arg.arg.x));
            break;
         case O_MINUS:
            /* unary minus */
            value = linear_comb(mpl,
                0.0, constant_term(mpl, 0.0),
               -1.0, eval_formula(mpl, code->arg.arg.x));
            break;
         case O_ADD:
            /* addition */
            value = linear_comb(mpl,
               +1.0, eval_formula(mpl, code->arg.arg.x),
               +1.0, eval_formula(mpl, code->arg.arg.y));
            break;
         case O_SUB:
            /* subtraction */
            value = linear_comb(mpl,
               +1.0, eval_formula(mpl, code->arg.arg.x),
               -1.0, eval_formula(mpl, code->arg.arg.y));
            break;
         case O_MUL:
            /* multiplication */
            insist(code->arg.arg.x != NULL);
            insist(code->arg.arg.y != NULL);
            if (code->arg.arg.x->type == A_NUMERIC)
            {  insist(code->arg.arg.y->type == A_FORMULA);
               value = linear_comb(mpl,
                  eval_numeric(mpl, code->arg.arg.x),
                  eval_formula(mpl, code->arg.arg.y),
                  0.0, constant_term(mpl, 0.0));
            }
            else
            {  insist(code->arg.arg.x->type == A_FORMULA);
               insist(code->arg.arg.y->type == A_NUMERIC);
               value = linear_comb(mpl,
                  eval_numeric(mpl, code->arg.arg.y),
                  eval_formula(mpl, code->arg.arg.x),
                  0.0, constant_term(mpl, 0.0));
            }
            break;
         case O_DIV:
            /* division */
            value = linear_comb(mpl,
               fp_div(mpl, 1.0, eval_numeric(mpl, code->arg.arg.y)),
               eval_formula(mpl, code->arg.arg.x),
               0.0, constant_term(mpl, 0.0));
            break;
         case O_FORK:
            /* if-then-else */
            if (eval_logical(mpl, code->arg.arg.x))
               value = eval_formula(mpl, code->arg.arg.y);
            else if (code->arg.arg.z == NULL)
               value = constant_term(mpl, 0.0);
            else
               value = eval_formula(mpl, code->arg.arg.z);
            break;
         case O_SUM:
            /* summation over domain */
            {  struct iter_form_info _info, *info = &_info;
               info->code = code;
               info->value = constant_term(mpl, 0.0);
               loop_within_domain(mpl, code->arg.loop.domain, info,
                  iter_form_func);
               value = info->value;
            }
            break;
         default:
            insist(code != code);
      }
      /* save resultant value */
      insist(!code->valid);
      code->valid = 1;
      code->value.form = copy_formula(mpl, value);
done: return value;
}

/*----------------------------------------------------------------------
-- clean_code - clean pseudo-code.
--
-- This routine recursively cleans specified pseudo-code that assumes
-- deleting all temporary resultant values. */

void clean_code(MPL *mpl, CODE *code)
{     ARG_LIST *e;
      /* if no pseudo-code is specified, do nothing */
      if (code == NULL) goto done;
      /* if resultant value is valid (exists), delete it */
      if (code->valid)
      {  code->valid = 0;
         delete_value(mpl, code->type, &code->value);
      }
      /* recursively clean pseudo-code for operands */
      switch (code->op)
      {  case O_NUMBER:
         case O_STRING:
         case O_INDEX:
            break;
         case O_MEMNUM:
         case O_MEMSYM:
            for (e = code->arg.par.list; e != NULL; e = e->next)
               clean_code(mpl, e->x);
            break;
         case O_MEMSET:
            for (e = code->arg.set.list; e != NULL; e = e->next)
               clean_code(mpl, e->x);
            break;
         case O_MEMVAR:
            for (e = code->arg.var.list; e != NULL; e = e->next)
               clean_code(mpl, e->x);
            break;
         case O_TUPLE:
         case O_MAKE:
            for (e = code->arg.list; e != NULL; e = e->next)
               clean_code(mpl, e->x);
            break;
         case O_SLICE:
            insist(code != code);
         case O_CVTNUM:
         case O_CVTSYM:
         case O_CVTLOG:
         case O_CVTTUP:
         case O_CVTLFM:
         case O_PLUS:
         case O_MINUS:
         case O_NOT:
         case O_ABS:
         case O_CEIL:
         case O_FLOOR:
         case O_EXP:
         case O_LOG:
         case O_LOG10:
         case O_SQRT:
            /* unary operation */
            clean_code(mpl, code->arg.arg.x);
            break;
         case O_ADD:
         case O_SUB:
         case O_LESS:
         case O_MUL:
         case O_DIV:
         case O_IDIV:
         case O_MOD:
         case O_POWER:
         case O_CONCAT:
         case O_LT:
         case O_LE:
         case O_EQ:
         case O_GE:
         case O_GT:
         case O_NE:
         case O_AND:
         case O_OR:
         case O_UNION:
         case O_DIFF:
         case O_SYMDIFF:
         case O_INTER:
         case O_CROSS:
         case O_IN:
         case O_NOTIN:
         case O_WITHIN:
         case O_NOTWITHIN:
            /* binary operation */
            clean_code(mpl, code->arg.arg.x);
            clean_code(mpl, code->arg.arg.y);
            break;
         case O_DOTS:
         case O_FORK:
            /* ternary operation */
            clean_code(mpl, code->arg.arg.x);
            clean_code(mpl, code->arg.arg.y);
            clean_code(mpl, code->arg.arg.z);
            break;
         case O_MIN:
         case O_MAX:
            /* n-ary operation */
            for (e = code->arg.list; e != NULL; e = e->next)
               clean_code(mpl, e->x);
            break;
         case O_SUM:
         case O_PROD:
         case O_MINIMUM:
         case O_MAXIMUM:
         case O_FORALL:
         case O_EXISTS:
         case O_SETOF:
         case O_BUILD:
            /* iterated operation */
            clean_domain(mpl, code->arg.loop.domain);
            clean_code(mpl, code->arg.loop.x);
            break;
         default:
            insist(code->op != code->op);
      }
done: return;
}

/**********************************************************************/
/* * *                      MODEL STATEMENTS                      * * */
/**********************************************************************/

void execute_check(MPL *mpl, CHECK *chk);
/* see this routine in another file */

/*----------------------------------------------------------------------
-- clean_check - clean check statement.
--
-- This routine cleans specified check statement that assumes deleting
-- all stuff dynamically allocated during the generation phase. */

void clean_check(MPL *mpl, CHECK *chk)
{     /* clean subscript domain */
      clean_domain(mpl, chk->domain);
      /* clean pseudo-code for computing predicate */
      clean_code(mpl, chk->code);
}

void execute_display(MPL *mpl, DISPLAY *dpy);
/* see this routine in another file */

/*----------------------------------------------------------------------
-- clean_display - clean display statement.
--
-- This routine cleans specified display statement that assumes deleting
-- all stuff dynamically allocated during the generation phase. */

void clean_display(MPL *mpl, DISPLAY *dpy)
{     DISPLAY1 *d;
      ARG_LIST *e;
      /* clean subscript domain */
      clean_domain(mpl, dpy->domain);
      /* clean display list */
      for (d = dpy->list; d != NULL; d = d->next)
      {  /* clean pseudo-code for computing expression */
         if (d->type == A_EXPRESSION)
            clean_code(mpl, d->u.code);
         /* clean pseudo-code for computing subscripts */
         for (e = d->list; e != NULL; e = e->next)
            clean_code(mpl, e->x);
      }
      return;
}

/* eof */
