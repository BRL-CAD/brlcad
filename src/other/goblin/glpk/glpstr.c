/* glpstr.c */

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

#include <string.h>
#include "glplib.h"
#include "glpstr.h"

/*----------------------------------------------------------------------
-- create_str_pool - create pool for segmented character strings.
--
-- *Synopsis*
--
-- #include "glpstr.h"
-- DMP *create_str_pool(void);
--
-- *Description*
--
-- The create_str_pool routine creates a memory pool which may be used
-- in subsequent operations on segmented character strings. Should note
-- that different strings can share the same pool.
--
-- *Returns*
--
-- The create_str_pool routine returns a pointer to the created memory
-- pool. */

DMP *create_str_pool(void)
{     DMP *pool;
      int size1 = sizeof(STR);
      int size2 = sizeof(SQE);
      pool = dmp_create_pool(size1 >= size2 ? size1 : size2);
      return pool;
}

/*----------------------------------------------------------------------
-- create_str - create segmented character string.
--
-- *Synopsis*
--
-- #include "glpstr.h"
-- STR *create_str(DMP *pool);
--
-- *Description*
--
-- The create_str routine creates empty segmented character string.
--
-- The parameter pool specifies the memory pool created by the
-- create_str_pool routine. The create_str routine connects this pool
-- with the string, so it will be used in all subsequent operations on
-- this string.
--
-- Should note that the same pool may be shared by different strings.
--
-- *Returns*
--
-- The create_str routine returns a pointer to the created string. */

STR *create_str(DMP *pool)
{     STR *str;
      str = dmp_get_atom(pool);
      str->pool = pool;
      str->len = 0;
      str->head = str->tail = NULL;
      return str;
}

/*----------------------------------------------------------------------
-- get_str - extract value from segmented character string.
--
-- *Synopsis*
--
-- #include "glpstr.h"
-- char *get_str(char *to, STR *str);
--
-- *Description*
--
-- The get_str routine copies data from the segmented character string
-- str to the plain character string to. The array of char type should
-- have at least len+1 characters, where len is current length of the
-- string str.
--
-- *Returns*
--
-- The get_str routine returns a pointer to the string to. */

char *get_str(char *to, STR *str)
{     SQE *sqe;
      int len = str->len;
      char *ptr = to;
      for (sqe = str->head; len > 0; sqe = sqe->next)
      {  int n = (len <= SQE_SIZE ? len : SQE_SIZE);
         insist(sqe != NULL);
         memcpy(ptr, sqe->data, n);
         ptr += n, len -= n;
      }
      *ptr = '\0';
      return to;
}

/*----------------------------------------------------------------------
-- set_str - assign value to segmented character string.
--
-- *Synopsis*
--
-- #include "glpstr.h"
-- STR *set_str(STR *str, char *from);
--
-- *Description*
--
-- The set_str routine copies data from the plain character string to
-- the segmented character string str.
--
-- *Returns*
--
-- The set_str routine returns a pointer to the string str. */

STR *set_str(STR *str, char *from)
{     SQE *sqe;
      int len = strlen(from);
      char *ptr = from;
      clear_str(str);
      while (len > 0)
      {  int n = (len <= SQE_SIZE ? len : SQE_SIZE);
         sqe = dmp_get_atom(str->pool);
         memcpy(sqe->data, ptr, n);
         ptr += n, len -= n;
         sqe->next = NULL;
         str->len += n;
         if (str->head == NULL)
            str->head = sqe;
         else
            str->tail->next = sqe;
         str->tail = sqe;
      }
      return str;
}

/*----------------------------------------------------------------------
-- clear_str - clear segmented character string.
--
-- *Synopsis*
--
-- #include "glpstr.h"
-- STR *clear_str(STR *str);
--
-- *Description*
--
-- The clear_str routine makes the segmented character string str empty.
--
-- *Returns*
--
-- The clear_str routine returns a pointer to the string str. */

STR *clear_str(STR *str)
{     str->len = 0;
      while (str->head != NULL)
      {  SQE *sqe = str->head;
         str->head = sqe->next;
         dmp_free_atom(str->pool, sqe);
      }
      str->tail = NULL;
      return str;
}

/*----------------------------------------------------------------------
-- compare_str - compare segmented character strings.
--
-- *Synopsis*
--
-- #include "glpstr.h"
-- int compare_str(STR *str1, STR *str2);
--
-- *Returns*
--
-- The compare_str compares segmented character strings str1 and str2,
-- and returns one of the following values:
--
-- < 0, if str1 is lexicographically less than str2;
-- = 0, if str1 and str2 are identical;
-- > 0, if str1 is lexicographically greater than str2.
--
-- If compared strings have different length, the shorter string is
-- considered as if it would be padded by binary zeros to the length of
-- the longer string. */

int compare_str(STR *str1, STR *str2)
{     SQE *sqe1 = str1->head, *sqe2 = str2->head;
      int len1 = str1->len, len2 = str2->len, ret = 0;
      while (len1 > 0 || len2 > 0)
      {  int n1 = (len1 <= SQE_SIZE ? len1 : SQE_SIZE);
         int n2 = (len2 <= SQE_SIZE ? len2 : SQE_SIZE);
         char buf1[SQE_SIZE], buf2[SQE_SIZE];
         memset(buf1, '\0', SQE_SIZE);
         if (n1 > 0)
         {  insist(sqe1 != NULL);
            memcpy(buf1, sqe1->data, n1);
            len1 -= n1, sqe1 = sqe1->next;
         }
         memset(buf2, '\0', SQE_SIZE);
         if (n2 > 0)
         {  insist(sqe2 != NULL);
            memcpy(buf2, sqe2->data, n2);
            len2 -= n2, sqe2 = sqe2->next;
         }
         ret = memcmp(buf1, buf2, SQE_SIZE);
         if (ret != 0) break;
      }
      return ret;
}

/*----------------------------------------------------------------------
-- delete_str - delete segmented character string.
--
-- *Synopsis*
--
-- #include "glpstr.h"
-- void delete_str(STR *str);
--
-- *Description*
--
-- The delete_str routine deletes the segmented character string str,
-- returning all its memory to the corresponding memory pool. */

void delete_str(STR *str)
{     clear_str(str);
      dmp_free_atom(str->pool, str);
      return;
}

/* eof */
