/* glplib3.c */

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
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "glplib.h"

/*----------------------------------------------------------------------
-- str2int - convert character string to value of integer type.
--
-- *Synopsis*
--
-- #include "glplib.h"
-- int str2int(char *str, int *val);
--
-- *Description*
--
-- The routine str2int converts the character string str to a value of
-- integer type and stores the value into location, which the parameter
-- val points to (in the case of error content of this location is not
-- changed).
--
-- *Returns*
--
-- The routine returns one of the following error codes:
--
-- 0 - no error;
-- 1 - value out of range;
-- 2 - character string is syntactically incorrect. */

int str2int(char *str, int *_val)
{     int d, k, s, val = 0;
      /* scan optional sign */
      if (str[0] == '+')
         s = +1, k = 1;
      else if (str[0] == '-')
         s = -1, k = 1;
      else
         s = +1, k = 0;
      /* check for the first digit */
      if (!isdigit((unsigned char)str[k])) return 2;
      /* scan digits */
      while (isdigit((unsigned char)str[k]))
      {  d = str[k++] - '0';
         if (s > 0)
         {  if (val > INT_MAX / 10) return 1;
            val *= 10;
            if (val > INT_MAX - d) return 1;
            val += d;
         }
         else
         {  if (val < INT_MIN / 10) return 1;
            val *= 10;
            if (val < INT_MIN + d) return 1;
            val -= d;
         }
      }
      /* check for terminator */
      if (str[k] != '\0') return 2;
      /* conversion is completed */
      *_val = val;
      return 0;
}

/*----------------------------------------------------------------------
-- str2dbl - convert character string to value of double type.
--
-- *Synopsis*
--
-- #include "glplib.h"
-- int str2dbl(char *str, double *val);
--
-- *Description*
--
-- The routine str2dbl converts the character string str to a value of
-- double type and stores the value into location, which the parameter
-- val points to (in the case of error content of this location is not
-- changed).
--
-- *Returns*
--
-- The routine returns one of the following error codes:
--
-- 0 - no error;
-- 1 - value out of range;
-- 2 - character string is syntactically incorrect. */

int str2dbl(char *str, double *_val)
{     int k;
      double val;
      /* scan optional sign */
      k = (str[0] == '+' || str[0] == '-' ? 1 : 0);
      /* check for decimal point */
      if (str[k] == '.')
      {  k++;
         /* a digit should follow it */
         if (!isdigit((unsigned char)str[k])) return 2;
         k++;
         goto frac;
      }
      /* integer part should start with a digit */
      if (!isdigit((unsigned char)str[k])) return 2;
      /* scan integer part */
      while (isdigit((unsigned char)str[k])) k++;
      /* check for decimal point */
      if (str[k] == '.') k++;
frac: /* scan optional fraction part */
      while (isdigit((unsigned char)str[k])) k++;
      /* check for decimal exponent */
      if (str[k] == 'E' || str[k] == 'e')
      {  k++;
         /* scan optional sign */
         if (str[k] == '+' || str[k] == '-') k++;
         /* a digit should follow E, E+ or E- */
         if (!isdigit((unsigned char)str[k])) return 2;
      }
      /* scan optional exponent part */
      while (isdigit((unsigned char)str[k])) k++;
      /* check for terminator */
      if (str[k] != '\0') return 2;
      /* perform conversion */
      {  char *endptr;
         val = strtod(str, &endptr);
         if (*endptr != '\0') return 2;
      }
      /* check for overflow */
      if (!(-DBL_MAX <= val && val <= +DBL_MAX)) return 1;
      /* check for underflow */
      if (-DBL_MIN < val && val < +DBL_MIN) val = 0.0;
      /* conversion is completed */
      *_val = val;
      return 0;
}

/*----------------------------------------------------------------------
-- strspx - remove all spaces from character string.
--
-- *Synopsis*
--
-- #include "glplib.h"
-- char *strspx(char *str);
--
-- *Description*
--
-- The routine strspx removes all spaces from the character string str.
--
-- *Examples*
--
-- strspx("   Errare   humanum   est   ") => "Errarehumanumest"
--
-- strspx("      ")                       => ""
--
-- *Returns*
--
-- The routine returns a pointer to the character string. */

char *strspx(char *str)
{     char *s, *t;
      for (s = t = str; *s; s++) if (*s != ' ') *t++ = *s;
      *t = '\0';
      return str;
}

/*----------------------------------------------------------------------
-- strtrim - remove trailing spaces from character string.
--
-- *Synopsis*
--
-- #include "glplib.h"
-- char *strtrim(char *str);
--
-- *Description*
--
-- The routine strtrim removes trailing spaces from the character
-- string str.
--
-- *Examples*
--
-- strtrim("Errare humanum est   ") => "Errare humanum est"
--
-- strtrim("      ")                => ""
--
-- *Returns*
--
-- The routine returns a pointer to the character string. */

char *strtrim(char *str)
{     char *t;
      for (t = strrchr(str, '\0') - 1; t >= str; t--)
      {  if (*t != ' ') break;
         *t = '\0';
      }
      return str;
}

/* eof */
