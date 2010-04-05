/* glpstr.h */

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

#ifndef _GLPSTR_H
#define _GLPSTR_H

#include "glpdmp.h"

#define clear_str             glp_clear_str
#define compare_str           glp_compare_str
#define create_str            glp_create_str
#define create_str_pool       glp_create_str_pool
#define delete_str            glp_delete_str
#define get_str               glp_get_str
#define set_str               glp_set_str

typedef struct STR STR;
typedef struct SQE SQE;

struct STR
{     /* segmented character string of arbitrary length */
      DMP *pool;
      /* memory pool holding string elements */
      int len;
      /* current string length */
      SQE *head;
      /* pointer to the first string element */
      SQE *tail;
      /* pointer to the last string element */
};

#define SQE_SIZE 12
/* number of characters allocated in each string element */

struct SQE
{     /* element of segmented character string */
      char data[SQE_SIZE];
      /* characters allocated in this element */
      SQE *next;
      /* pointer to the next string element */
};

extern STR *clear_str(STR *str);
/* clear segmented character string */

extern int compare_str(STR *str1, STR *str2);
/* compare segmented character strings */

extern STR *create_str(DMP *pool);
/* create segmented character string */

extern DMP *create_str_pool(void);
/* create pool for segmented character strings */

extern void delete_str(STR *str);
/* delete segmented character string */

extern char *get_str(char *to, STR *str);
/* extract value from segmented character string */

extern STR *set_str(STR *str, char *from);
/* assign value to segmented character string */

#endif

/* eof */
