/* glpspx.h */

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

#ifndef _GLPSPX_H
#define _GLPSPX_H

#include "glplpx.h"

#define spx_invert            glp_spx_invert
#define spx_ftran             glp_spx_ftran
#define spx_btran             glp_spx_btran
#define spx_update            glp_spx_update

#define spx_eval_xn_j         glp_spx_eval_xn_j
#define spx_eval_bbar         glp_spx_eval_bbar
#define spx_eval_pi           glp_spx_eval_pi
#define spx_eval_cbar         glp_spx_eval_cbar
#define spx_eval_obj          glp_spx_eval_obj
#define spx_eval_col          glp_spx_eval_col
#define spx_eval_rho          glp_spx_eval_rho
#define spx_eval_row          glp_spx_eval_row
#define spx_check_bbar        glp_spx_check_bbar
#define spx_check_cbar        glp_spx_check_cbar
#define spx_prim_chuzc        glp_spx_prim_chuzc
#define spx_prim_chuzr        glp_spx_prim_chuzr
#define spx_dual_chuzr        glp_spx_dual_chuzr
#define spx_dual_chuzc        glp_spx_dual_chuzc
#define spx_update_bbar       glp_spx_update_bbar
#define spx_update_pi         glp_spx_update_pi
#define spx_update_cbar       glp_spx_update_cbar
#define spx_change_basis      glp_spx_change_basis
#define spx_err_in_bbar       glp_spx_err_in_bbar
#define spx_err_in_pi         glp_spx_err_in_pi
#define spx_err_in_cbar       glp_spx_err_in_cbar
#define spx_reset_refsp       glp_spx_reset_refsp
#define spx_update_gvec       glp_spx_update_gvec
#define spx_err_in_gvec       glp_spx_err_in_gvec
#define spx_update_dvec       glp_spx_update_dvec
#define spx_err_in_dvec       glp_spx_err_in_dvec

typedef struct SPX SPX;

struct SPX
{     /* common block used by the simplex method routines */
      LPX *lp;
      /* LP problem object */
      int meth;
      /* what method is used:
         'P' - primal simplex
         'D' - dual simplex */
      int p;
      /* the number of basic variable xB[p], 1 <= p <= m, chosen to
         leave the basis; the special case p < 0 means that non-basic
         double-bounded variable xN[q] just goes to its opposite bound,
         and the basis remains unchanged; p = 0 means that no choice
         can be made (in the case of primal simplex non-basic variable
         xN[q] can infinitely change, in the case of dual simplex the
         current basis is primal feasible) */
      int p_tag;
      /* if 1 <= p <= m, p_tag is a non-basic tag, which should be set
         for the variable xB[p] after it has left the basis */
      int q;
      /* the number of non-basic variable xN[q], 1 <= q <= n, chosen to
         enter the basis; q = 0 means that no choice can be made (in
         the case of primal simplex the current basis is dual feasible,
         in the case of dual simplex the dual variable that corresponds
         to xB[p] can infinitely change) */
      double *zeta; /* double zeta[1+m]; */
      /* the p-th row of the inverse inv(B) */
      double *ap; /* double ap[1+n]; */
      /* the p-th row of the current simplex table:
         ap[0] is not used;
         ap[j], 1 <= j <= n, is an influence coefficient, which defines
         how the non-basic variable xN[j] affects on the basic variable
         xB[p] = ... + ap[j] * xN[j] + ... */
      double *aq; /* double aq[1+m]; */
      /* the q-th column of the current simplex table;
         aq[0] is not used;
         aq[i], 1 <= i <= m, is an influence coefficient, which defines
         how the non-basic variable xN[q] affects on the basic variable
         xB[i] = ... + aq[i] * xN[q] + ... */
      double *gvec; /* double gvec[1+n]; */
      /* gvec[0] is not used;
         gvec[j], 1 <= j <= n, is a weight of non-basic variable xN[j];
         this vector is used to price non-basic variables in the primal
         simplex (for example, using the steepest edge technique) */
      double *dvec; /* double dvec[1+m]; */
      /* dvec[0] is not used;
         dvec[i], 1 <= i <= m, is a weight of basic variable xB[i]; it
         is used to price basic variables in the dual simplex */
      int *refsp; /* int refsp[1+m+n]; */
      /* the current reference space (used in the projected steepest
         edge technique); the flag refsp[k], 1 <= k <= m+n, is set if
         the variable x[k] belongs to the current reference space */
      int count;
      /* if this count (used in the projected steepest edge technique)
         gets zero, the reference space is automatically redefined */
      double *work; /* double work[1+m+n]; */
      /* working array (used for various purposes) */
      int *orig_typx; /* orig_typx[1+m+n]; */
      /* is used to save the original types of variables */
      double *orig_lb; /* orig_lb[1+m+n]; */
      /* is used to save the original lower bounds of variables */
      double *orig_ub; /* orig_ub[1+m+n]; */
      /* is used to save the original upper bounds of variables */
      int orig_dir;
      /* is used to save the original optimization direction */
      double *orig_coef; /* orig_coef[1+m+n]; */
      /* is used to save the original objective coefficients */
};

/* basis maintenance routines ----------------------------------------*/

int spx_invert(LPX *lp);
/* reinvert the basis matrix */

void spx_ftran(LPX *lp, double x[], int save);
/* perform forward transformation (FTRAN) */

void spx_btran(LPX *lp, double x[]);
/* perform backward transformation (BTRAN) */

int spx_update(LPX *lp, int j);
/* update factorization for adjacent basis matrix */

/* generic simplex method routines -----------------------------------*/

double spx_eval_xn_j(LPX *lp, int j);
/* determine value of non-basic variable */

void spx_eval_bbar(LPX *lp);
/* compute values of basic variables */

void spx_eval_pi(LPX *lp);
/* compute simplex multipliers */

void spx_eval_cbar(LPX *lp);
/* compute reduced costs of non-basic variables */

double spx_eval_obj(LPX *lp);
/* compute value of the objective function */

void spx_eval_col(LPX *lp, int j, double col[], int save);
/* compute column of the simplex table */

void spx_eval_rho(LPX *lp, int i, double rho[]);
/* compute row of the inverse */

void spx_eval_row(LPX *lp, double rho[], double row[]);
/* compute row of the simplex table */

double spx_check_bbar(LPX *lp, double tol);
/* check primal feasibility */

double spx_check_cbar(LPX *lp, double tol);
/* check dual feasibility */

int spx_prim_chuzc(SPX *spx, double tol);
/* choose non-basic variable (primal simplex) */

int spx_prim_chuzr(SPX *spx, double relax);
/* choose basic variable (primal simplex) */

void spx_dual_chuzr(SPX *spx, double tol);
/* choose basic variable (dual simplex) */

int spx_dual_chuzc(SPX *spx, double relax);
/* choose non-basic variable (dual simplex) */

void spx_update_bbar(SPX *spx, double *obj);
/* update values of basic variables */

void spx_update_pi(SPX *spx);
/* update simplex multipliers */

void spx_update_cbar(SPX *spx, int all);
/* update reduced costs of non-basic variables */

int spx_change_basis(SPX *spx);
/* change basis and update the factorization */

double spx_err_in_bbar(SPX *spx);
/* compute maximal absolute error in bbar */

double spx_err_in_pi(SPX *spx);
/* compute maximal absolute error in pi */

double spx_err_in_cbar(SPX *spx, int all);
/* compute maximal absolute error in cbar */

void spx_reset_refsp(SPX *spx);
/* reset the reference space */

void spx_update_gvec(SPX *spx);
/* update the vector gamma for adjacent basis */

double spx_err_in_gvec(SPX *spx);
/* compute maximal absolute error in gvec */

void spx_update_dvec(SPX *spx);
/* update the vector delta for adjacent basis */

double spx_err_in_dvec(SPX *spx);
/* compute maximal absolute error in dvec */

#endif

/* eof */
