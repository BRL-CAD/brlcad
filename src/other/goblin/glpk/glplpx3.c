/* glplpx3.c (control parameters and statistics routines) */

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

#include <float.h>
#include "glplib.h"
#include "glplpx.h"

/*----------------------------------------------------------------------
-- lpx_reset_parms - reset control parameters to default values.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_reset_parms(LPX *lp);
--
-- *Description*
--
-- The routine lpx_reset_parms resets all control parameters associated
-- with an LP problem object, which the parameter lp points to, to their
-- default values. */

void lpx_reset_parms(LPX *lp)
{     lp->msg_lev  = 3;
      lp->scale    = 3;
      lp->sc_ord   = 0;
      lp->sc_max   = 20;
      lp->sc_eps   = 0.01;
      lp->dual     = 0;
      lp->price    = 1;
      lp->relax    = 0.07;
      lp->tol_bnd  = 1e-7;
      lp->tol_dj   = 1e-7;
      lp->tol_piv  = 1e-9;
      lp->round    = 0;
      lp->obj_ll   = -DBL_MAX;
      lp->obj_ul   = +DBL_MAX;
      lp->it_lim   = -1;
      lp->it_cnt   = 0;
      lp->tm_lim   = -1.0;
      lp->out_frq  = 200;
      lp->out_dly  = 0.0;
      lp->branch   = 2;
      lp->btrack   = 2;
      lp->tol_int  = 1e-6;
      lp->tol_obj  = 1e-7;
      lp->mps_info = 1;
      lp->mps_obj  = 2;
      lp->mps_orig = 0;
      lp->mps_wide = 1;
      lp->mps_free = 0;
      lp->mps_skip = 0;
      lp->lpt_orig = 0;
      lp->presol = 0;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_int_parm - set (change) integer control parameter.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_int_parm(LPX *lp, int parm, int val);
--
-- *Description*
--
-- The routine lpx_set_int_parm sets (changes) the current value of an
-- integer control parameter parm. The parameter val specifies a new
-- value of the control parameter. */

void lpx_set_int_parm(LPX *lp, int parm, int val)
{     switch (parm)
      {  case LPX_K_MSGLEV:
            if (!(0 <= val && val <= 3))
               fault("lpx_set_int_parm: MSGLEV = %d; invalid value",
                  val);
            lp->msg_lev = val;
            break;
         case LPX_K_SCALE:
            if (!(0 <= val && val <= 3))
               fault("lpx_set_int_parm: SCALE = %d; invalid value",
                  val);
            lp->scale = val;
            break;
         case LPX_K_DUAL:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: DUAL = %d; invalid value",
                  val);
            lp->dual = val;
            break;
         case LPX_K_PRICE:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: PRICE = %d; invalid value",
                  val);
            lp->price = val;
            break;
         case LPX_K_ROUND:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: ROUND = %d; invalid value",
                  val);
            lp->round = val;
            break;
         case LPX_K_ITLIM:
            lp->it_lim = val;
            break;
         case LPX_K_ITCNT:
            lp->it_cnt = val;
            break;
         case LPX_K_OUTFRQ:
            if (!(val > 0))
               fault("lpx_set_int_parm: OUTFRQ = %d; invalid value",
                  val);
            lp->out_frq = val;
            break;
         case LPX_K_BRANCH:
            if (!(val == 0 || val == 1 || val == 2))
               fault("lpx_set_int_parm: BRANCH = %d; invalid value",
                  val);
            lp->branch = val;
            break;
         case LPX_K_BTRACK:
            if (!(val == 0 || val == 1 || val == 2))
               fault("lpx_set_int_parm: BTRACK = %d; invalid value",
                  val);
            lp->btrack = val;
            break;
         case LPX_K_MPSINFO:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: MPSINFO = %d; invalid value",
                  val);
            lp->mps_info = val;
            break;
         case LPX_K_MPSOBJ:
            if (!(val == 0 || val == 1 || val == 2))
               fault("lpx_set_int_parm: MPSOBJ = %d; invalid value",
                  val);
            lp->mps_obj = val;
            break;
         case LPX_K_MPSORIG:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: MPSORIG = %d; invalid value",
                  val);
            lp->mps_orig = val;
            break;
         case LPX_K_MPSWIDE:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: MPSWIDE = %d; invalid value",
                  val);
            lp->mps_wide = val;
            break;
         case LPX_K_MPSFREE:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: MPSFREE = %d; invalid value",
                  val);
            lp->mps_free = val;
            break;
         case LPX_K_MPSSKIP:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: MPSSKIP = %d; invalid value",
                  val);
            lp->mps_skip = val;
            break;
         case LPX_K_LPTORIG:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: LPTORIG = %d; invalid value",
                  val);
            lp->lpt_orig = val;
            break;
         case LPX_K_PRESOL:
            if (!(val == 0 || val == 1))
               fault("lpx_set_int_parm: PRESOL = %d; invalid value",
                  val);
            lp->presol = val;
            break;
         default:
            fault("lpx_set_int_parm: parm = %d; invalid parameter",
               parm);
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_get_int_parm - query integer control parameter.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_int_parm(LPX *lp, int parm);
--
-- *Returns*
--
-- The routine lpx_get_int_parm returns the current value of an integer
-- control parameter parm. */

int lpx_get_int_parm(LPX *lp, int parm)
{     int val = 0;
      switch (parm)
      {  case LPX_K_MSGLEV:
            val = lp->msg_lev; break;
         case LPX_K_SCALE:
            val = lp->scale; break;
         case LPX_K_DUAL:
            val = lp->dual; break;
         case LPX_K_PRICE:
            val = lp->price; break;
         case LPX_K_ROUND:
            val = lp->round; break;
         case LPX_K_ITLIM:
            val = lp->it_lim; break;
         case LPX_K_ITCNT:
            val = lp->it_cnt; break;
         case LPX_K_OUTFRQ:
            val = lp->out_frq; break;
         case LPX_K_BRANCH:
            val = lp->branch; break;
         case LPX_K_BTRACK:
            val = lp->btrack; break;
         case LPX_K_MPSINFO:
            val = lp->mps_info; break;
         case LPX_K_MPSOBJ:
            val = lp->mps_obj; break;
         case LPX_K_MPSORIG:
            val = lp->mps_orig; break;
         case LPX_K_MPSWIDE:
            val = lp->mps_wide; break;
         case LPX_K_MPSFREE:
            val = lp->mps_free; break;
         case LPX_K_MPSSKIP:
            val = lp->mps_skip; break;
         case LPX_K_LPTORIG:
            val = lp->lpt_orig; break;
         case LPX_K_PRESOL:
            val = lp->presol; break;
         default:
            fault("lpx_get_int_parm: parm = %d; invalid parameter",
               parm);
      }
      return val;
}

/*----------------------------------------------------------------------
-- lpx_set_real_parm - set (change) real control parameter.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_real_parm(LPX *lp, int parm, double val);
--
-- *Description*
--
-- The routine lpx_set_real_parm sets (changes) the current value of
-- a real (floating point) control parameter parm. The parameter val
-- specifies a new value of the control parameter. */

void lpx_set_real_parm(LPX *lp, int parm, double val)
{     switch (parm)
      {  case LPX_K_RELAX:
            if (!(0.0 <= val && val <= 1.0))
               fault("lpx_set_real_parm: RELAX = %g; invalid value",
                  val);
            lp->relax = val;
            break;
         case LPX_K_TOLBND:
            if (!(DBL_EPSILON <= val && val <= 0.001))
               fault("lpx_set_real_parm: TOLBND = %g; invalid value",
                  val);
            if (lp->tol_bnd > val)
            {  /* invalidate the basic solution */
               lp->p_stat = LPX_P_UNDEF;
               lp->d_stat = LPX_D_UNDEF;
            }
            lp->tol_bnd = val;
            break;
         case LPX_K_TOLDJ:
            if (!(DBL_EPSILON <= val && val <= 0.001))
               fault("lpx_set_real_parm: TOLDJ = %g; invalid value",
                  val);
            if (lp->tol_dj > val)
            {  /* invalidate the basic solution */
               lp->p_stat = LPX_P_UNDEF;
               lp->d_stat = LPX_D_UNDEF;
            }
            lp->tol_dj = val;
            break;
         case LPX_K_TOLPIV:
            if (!(DBL_EPSILON <= val && val <= 0.001))
               fault("lpx_set_real_parm: TOLPIV = %g; invalid value",
                  val);
            lp->tol_piv = val;
            break;
         case LPX_K_OBJLL:
            lp->obj_ll = val;
            break;
         case LPX_K_OBJUL:
            lp->obj_ul = val;
            break;
         case LPX_K_TMLIM:
            lp->tm_lim = val;
            break;
         case LPX_K_OUTDLY:
            lp->out_dly = val;
            break;
         case LPX_K_TOLINT:
            if (!(DBL_EPSILON <= val && val <= 0.001))
               fault("lpx_set_real_parm: TOLINT = %g; invalid value",
                  val);
            lp->tol_int = val;
            break;
         case LPX_K_TOLOBJ:
            if (!(DBL_EPSILON <= val && val <= 0.001))
               fault("lpx_set_real_parm: TOLOBJ = %g; invalid value",
                  val);
            lp->tol_obj = val;
            break;
         default:
            fault("lpx_set_real_parm: parm = %d; invalid parameter",
               parm);
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_get_real_parm - query real control parameter.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_real_parm(LPX *lp, int parm);
--
-- *Returns*
--
-- The routine lpx_get_real_parm returns the current value of a real
-- (floating point) control parameter parm. */

double lpx_get_real_parm(LPX *lp, int parm)
{     double val = 0.0;
      switch (parm)
      {  case LPX_K_RELAX:
            val = lp->relax;
            break;
         case LPX_K_TOLBND:
            val = lp->tol_bnd;
            break;
         case LPX_K_TOLDJ:
            val = lp->tol_dj;
            break;
         case LPX_K_TOLPIV:
            val = lp->tol_piv;
            break;
         case LPX_K_OBJLL:
            val = lp->obj_ll;
            break;
         case LPX_K_OBJUL:
            val = lp->obj_ul;
            break;
         case LPX_K_TMLIM:
            val = lp->tm_lim;
            break;
         case LPX_K_OUTDLY:
            val = lp->out_dly;
            break;
         case LPX_K_TOLINT:
            val = lp->tol_int;
            break;
         case LPX_K_TOLOBJ:
            val = lp->tol_obj;
            break;
         default:
            fault("lpx_get_real_parm: parm = %d; invalid parameter",
               parm);
      }
      return val;
}

/* eof */
