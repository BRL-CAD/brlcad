/* glpspx2.c (generic simplex method routines) */

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
#include <math.h>
#include <stddef.h>
#include "glplib.h"
#include "glpspx.h"

/*----------------------------------------------------------------------
-- spx_eval_xn_j - determine value of non-basic variable.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_eval_xn_j(LPX *lp, int j);
--
-- *Returns*
--
-- The routine spx_eval_xn_j returns a value of the non-basic variable
-- xN[j], 1 <= j <= n, for the current basic solution. */

double spx_eval_xn_j(LPX *lp, int j)
{     int m = lp->m;
      int n = lp->n;
      double *lb = lp->lb;
      double *ub = lp->ub;
      int *tagx = lp->tagx;
      int *indx = lp->indx;
      int k;
      double xn_j;
      insist(1 <= j && j <= n);
      k = indx[m+j]; /* x[k] = xN[j] */
      switch (tagx[k])
      {  case LPX_NL:
            /* xN[j] is on its lower bound */
            xn_j = lb[k]; break;
         case LPX_NU:
            /* xN[j] is on its upper bound */
            xn_j = ub[k]; break;
         case LPX_NF:
            /* xN[j] is free variable */
            xn_j = 0.0; break;
         case LPX_NS:
            /* xN[j] is fixed variable */
            xn_j = lb[k]; break;
         default:
            insist(tagx != tagx);
      }
      return xn_j;
}

/*----------------------------------------------------------------------
-- spx_eval_bbar - compute values of basic variables.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_eval_bbar(LPX *lp);
--
-- *Description*
--
-- The routine spx_eval_bbar computes values of basic variables
--
--    xB = beta = (beta_1, ..., beta_m)
--
-- for the current basis and stores elements of the vector beta to the
-- locations bbar[1], ..., bbar[m].
--
-- The vector beta is computed using the following formula:
--
--    beta = - inv(B) * (N * xN) =
--
--         = inv(B) * (- N[1]*xN[1] - ... - N[n]*xN[n]),
--
-- where N[j] is the j-th column of the augmented constraint matrix A~,
-- which corresponds to the non-basic variable xN[j]. */

void spx_eval_bbar(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *indx = lp->indx;
      double *bbar = lp->bbar;
      int i, j, j_beg, j_end, j_ptr, k;
      double *rhs = bbar, xn_j;
      /* rhs := - N*xN = - N[1]*xN[1] - ... - N[n]*xN[n] */
      for (i = 1; i <= m; i++) rhs[i] = 0.0;
      for (j = 1; j <= n; j++)
      {  xn_j = spx_eval_xn_j(lp, j);
         if (xn_j == 0.0) continue;
         k = indx[m+j]; /* x[k] = xN[j] */
         if (k <= m)
         {  /* x[k] is auxiliary variable */
            rhs[k] -= xn_j;
         }
         else
         {  /* x[k] is structural variable */
            j_beg = aa_ptr[k];
            j_end = j_beg + aa_len[k] - 1;
            for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
               rhs[sv_ndx[j_ptr]] += sv_val[j_ptr] * xn_j;
         }
      }
      /* bbar := inv(B) * rhs */
      spx_ftran(lp, rhs, 0);
      return;
}

/*----------------------------------------------------------------------
-- spx_eval_pi - compute simplex multipliers.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_eval_pi(LPX *lp);
--
-- *Description*
--
-- The routine spx_eval_pi computes simplex multipliers (i.e. Lagrange
-- multipliers that correspond to the equality constraints)
--
--    pi = (pi_1, ..., pi_m)
--
-- for the current basis and stores elements of the vector pi to the
-- locations pi[1], ..., pi[m].
--
-- The vector pi is computed using the following formula:
--
--    pi = inv(B') * cB,
--
-- where B' is a matrix transposed to the current basis matrix B, cB is
-- the vector of objective coefficients at basic variables xB. */

void spx_eval_pi(LPX *lp)
{     int m = lp->m;
      double *coef = lp->coef;
      int *indx = lp->indx;
      int i;
      double *cb = lp->pi;
      /* make the vector cB */
      for (i = 1; i <= m; i++) cb[i] = coef[indx[i]];
      /* pi := inv(B') * cB */
      spx_btran(lp, cb);
      return;
}

/*----------------------------------------------------------------------
-- spx_eval_cbar - compute reduced costs of non-basic variables.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_eval_cbar(LPX *lp);
--
-- *Description*
--
-- The routine spx_eval_cbar computes reduced costs
--
--    d = (d_1, ..., d_n)
--
-- of non-basic variables for the current basis and stores elements of
-- the vector d to the locations cbar[1], ..., cbar[n].
--
-- The vector d is computed using the following formula:
--
--    d[j] = cN[j] - pi' * N[j], j = 1, ..., n,
--
-- where cN[j] is coefficient of the objective function at the variable
-- xN[j], pi is the vector of simplex multipliers (should be computed
-- before), N[j] is the j-th column of the augmented constraint matrix
-- A~, which corresponds to the non-basic variable xN[j]. */

void spx_eval_cbar(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      double *coef = lp->coef;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *indx = lp->indx;
      double *pi = lp->pi;
      double *cbar = lp->cbar;
      int j, j_beg, j_end, j_ptr, k;
      double cbar_j;
      /* d[j] := cN[j] - pi' * N[j] */
      for (j = 1; j <= n; j++)
      {  k = indx[m+j]; /* x[k] = xN[j] */
         cbar_j = coef[k];
         if (k <= m)
         {  /* x[k] is auxiliary variable */
            cbar_j -= pi[k];
         }
         else
         {  /* x[k] is structural variable */
            j_beg = aa_ptr[k];
            j_end = j_beg + aa_len[k] - 1;
            for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
               cbar_j += pi[sv_ndx[j_ptr]] * sv_val[j_ptr];
         }
         cbar[j] = cbar_j;
      }
      return;
}

/*----------------------------------------------------------------------
-- spx_eval_obj - compute value of the objective function.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_eval_obj(LPX *lp);
--
-- *Returns*
--
-- The routine spx_eval_obj returns the current value of the objective
-- function (used mainly for displaying). */

double spx_eval_obj(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int *tagx = lp->tagx;
      int *posx = lp->posx;
      double *coef = lp->coef;
      double *bbar = lp->bbar;
      int i, j, k;
      double obj = coef[0];
      for (k = 1; k <= m+n; k++)
      {  if (tagx[k] == LPX_BS)
         {  /* x[k] = xB[i] */
            i = posx[k];
            insist(1 <= i && i <= m);
            obj += coef[k] * bbar[i];
         }
         else
         {  /* x[k] = xN[j] */
            j = posx[k] - m;
            obj += coef[k] * spx_eval_xn_j(lp, j);
         }
      }
      return obj;
}

/*----------------------------------------------------------------------
-- spx_eval_col - compute column of the simplex table.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_eval_col(LPX *lp, int j, double col[], int save);
--
-- *Description*
--
-- The routine spx_eval_col computes the j-th column of the current
-- simplex table A^ = -inv(B)*N and stores elements of this column to
-- the locations col[1], ..., col[m].
--
-- The parameter save is a flag. If this flag is set, it means that the
-- computed column is the column of non-basic variable xN[q], which has
-- been chosen to enter the basis (i.e. j = q). This flag is used by the
-- routine spx_ftran in order to save and pass the corresponding column
-- to the routine spx_update.
--
-- The j-th column is computed using the following formula:
--
--    A^[j] = - inv(B) * N[j],
--
-- where N[j] is the j-th column of the augmented constraint matrix A~,
-- which corresponds to the non-basic variable xN[j]. */

void spx_eval_col(LPX *lp, int j, double col[], int save)
{     int m = lp->m;
      int n = lp->n;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *indx = lp->indx;
      int i, j_beg, j_end, j_ptr, k;
      double *rhs = col;
      insist(1 <= j && j <= n);
      /* rhs := N[j] */
      for (i = 1; i <= m; i++) rhs[i] = 0.0;
      k = indx[m+j]; /* x[k] = xN[j] */
      if (k <= m)
      {  /* x[k] is auxiliary variable */
         rhs[k] = +1.0;
      }
      else
      {  /* x[k] is structural variable */
         j_beg = aa_ptr[k];
         j_end = j_beg + aa_len[k] - 1;
         for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
            rhs[sv_ndx[j_ptr]] = - sv_val[j_ptr];
      }
      /* A^[j] := - inv(B) * N[j] */
      spx_ftran(lp, rhs, save);
      for (i = 1; i <= m; i++) col[i] = - col[i];
      return;
}

/*----------------------------------------------------------------------
-- spx_eval_rho - compute row of the inverse.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_eval_rho(LPX *lp, int i, double rho[]);
--
-- *Description*
--
-- The routine spx_eval_rho computes the i-th row of the matrix inv(B),
-- where B is the current basis matrix, and stores elements of this row
-- to the locations rho[1], ..., rho[m].
--
-- The i-th row of the inverse is computed using the following formula:
--
--    zeta = inv(B') * e[i],
--
-- where B' is a matrix transposed to B, e[i] is a unity vector, which
-- contains one in the i-th position. */

void spx_eval_rho(LPX *lp, int i, double rho[])
{     int m = lp->m;
      int j;
      double *rhs = rho;
      insist(1 <= i && i <= m);
      for (j = 1; j <= m; j++) rhs[j] = 0.0;
      rhs[i] = +1.0;
      spx_btran(lp, rhs);
      return;
}

/*----------------------------------------------------------------------
-- spx_eval_row - compute row of the simplex table.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_eval_row(LPX *lp, double rho[], double row[]);
--
-- *Description*
--
-- The routine spx_eval_col computes the i-th row of the current simplex
-- table A^ = -inv(B)*N and stores elements of this row to the locations
-- row[1], ..., row[n].
--
-- The row number i is defined implicitly by the array rho, which on
-- entry should contain the i-th row of the inverse inv(B) computed by
-- means of the routine spx_eval_rho. This array is not changed on exit.
--
-- In order to exploit possible sparsity of the vector rho, the routine
-- computes the i-th row as a linear combination of rows of the matrix N
-- using the following formula:
--
--    a^[i] = - N' * rho = - N'[1]*rho[1] - ... - N'[m]*rho[m],
--
-- N' is a matrix transposed to N, N is the matrix of non-basic columns
-- of the augmented constraint matrix A~, N'[i] is the i-th row of the
-- matrix N. */

void spx_eval_row(LPX *lp, double rho[], double row[])
{     int m = lp->m;
      int n = lp->n;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *posx = lp->posx;
      int i, i_beg, i_end, i_ptr, j;
      double rho_i;
      for (j = 1; j <= n; j++) row[j] = 0.0;
      for (i = 1; i <= m; i++)
      {  rho_i = rho[i];
         if (rho_i == 0.0) continue;
         /* i-th row of the augmented constraint matrix A~ has the form
            (0 ... 0 +1 0 ... 0 | -a[i,1] ... -a[i,m]), where the unity
            subvector is not stored explicitly; we need to consider only
            elements, which are placed in non-basic columns */
         /* look through the unity subvector of the i-th row of A~ */
         j = posx[i] - m;
         if (j > 0)
         {  /* xN[j] is the auxiliary variable x[i] */
            row[j] -= rho_i;
         }
         /* look through other non-zero elements in the i-th row of A~
            that correspond to structural variables */
         i_beg = aa_ptr[i];
         i_end = i_beg + aa_len[i] - 1;
         for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
         {  j = posx[sv_ndx[i_ptr] + m] - m;
            if (j > 0)
            {  /* xN[j] is some structural variable */
               row[j] += rho_i * sv_val[i_ptr];
            }
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- spx_check_bbar - check primal feasibility.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_check_bbar(LPX *lp, double tol);
--
-- *Description*
--
-- The routine spx_check_bbar checks if the current basic solution is
-- primal feasible, i.e. whether the current values of basic variables
-- are within their bounds, using a relative tolerance tol.
--
-- *Returns*
--
-- The routine returns the non-positive sum of primal infeasibilities.
-- If the basic solution is primal feasible, this sum is exact zero. */

double spx_check_bbar(LPX *lp, double tol)
{     int m = lp->m;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      int *indx = lp->indx;
      double *bbar = lp->bbar;
      int i, k, typx_k;
      double lb_k, ub_k, bbar_i;
      double sum = 0.0;
      for (i = 1; i <= m; i++)
      {  k = indx[i]; /* x[k] = xB[i] */
         typx_k = typx[k];
         bbar_i = bbar[i];
         if (typx_k == LPX_LO || typx_k == LPX_DB || typx_k == LPX_FX)
         {  /* xB[i] has lower bound */
            lb_k = lb[k];
            if ((lb_k - bbar_i) / (1.0 + fabs(lb_k)) > tol)
               sum += (lb_k - bbar_i);
         }
         if (typx_k == LPX_UP || typx_k == LPX_DB || typx_k == LPX_FX)
         {  /* xB[i] has upper bound */
            ub_k = ub[k];
            if ((bbar_i - ub_k) / (1.0 + fabs(ub_k)) > tol)
               sum += (bbar_i - ub_k);
         }
      }
      return sum;
}

/*----------------------------------------------------------------------
-- spx_check_cbar - check dual feasibility.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_check_cbar(LPX *lp, double tol);
--
-- *Description*
--
-- The routine spx_check_cbar checks if the current basic solution is
-- dual feasible, i.e. whether the current reduced costs of non-basic
-- variables have correct signs, using an absolute tolerance tol.
--
-- *Returns*
--
-- The routine returns the non-positive sum of dual infeasibilities. If
-- the basic solution is dual feasible, this sum is exact zero. */

double spx_check_cbar(LPX *lp, double tol)
{     int m = lp->m;
      int n = lp->n;
      double dir = (lp->dir == LPX_MIN ? +1.0 : -1.0);
      int *tagx = lp->tagx;
      int *indx = lp->indx;
      double *cbar = lp->cbar;
      int j, k, tagx_k;
      double cbar_j;
      double sum = 0.0;
      for (j = 1; j <= n; j++)
      {  k = indx[m+j]; /* x[k] = xN[j] */
         tagx_k = tagx[k];
         cbar_j = dir * cbar[j];
         if (tagx_k == LPX_NF || tagx_k == LPX_NL)
         {  /* xN[j] can increase */
            if (cbar_j < - tol) sum -= cbar_j;
         }
         if (tagx_k == LPX_NF || tagx_k == LPX_NU)
         {  /* xN[j] can decrease */
            if (cbar_j > + tol) sum += cbar_j;
         }
      }
      return sum;
}

/*----------------------------------------------------------------------
-- spx_prim_chuzc - choose non-basic variable (primal simplex).
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_prim_chuzc(SPX *spx, double tol);
--
-- *Description*
--
-- The routine spx_prim_chuzc chooses a non-basic variable xN[q], which
-- should enter the basis.
--
-- The parameter tol is an absolute tolerance, which is used to check
-- if some non-basic variable violates the dual feasibility condition
-- and therefore can be considered as a possible candidate.
--
-- The routine chooses a non-basic variable xN[q], which can change in
-- a feasible direction improving the objective fucntion and which has
-- the largest weighted reduced cost:
--
--    |d[q]| / sqrt(gamma[q]) = max(|d[j]| / sqrt(gamma[j])),
--
-- where d[j] is a reduced cost of the non-basic variable xN[j], and
-- gamma[j] is a "weight" of this variable.
--
-- If the non-basic variable xN[q] has been chosen, the routine sets q
-- to the number of this variable, 1 <= q <= n. However, if the current
-- basis is dual feasible and therefore no choice has been made, the
-- routine sets q to 0.
--
-- *Returns*
--
-- Non-zero return code means that the routine recomputed all the basic
-- solution components in order to improve their numeric accuracy. */

int spx_prim_chuzc(SPX *spx, double tol)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      double dir = (lp->dir == LPX_MIN ? +1.0 : -1.0);
      double *coef = lp->coef;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *tagx = lp->tagx;
      int *indx = lp->indx;
      double *pi = lp->pi;
      double *cbar = lp->cbar;
      double *gvec = spx->gvec;
      int j, j_beg, j_end, j_ptr, k, q, retry = 0;
      double best, temp, cbar_j, cbar_q;
loop: /* recompute basic solution components (if required) */
      if (retry)
      {  spx_eval_bbar(lp);
         spx_eval_pi(lp);
         spx_eval_cbar(lp);
      }
      /* nothing chosen so far */
      q = 0, best = 0.0;
      /* look through the list of non-basic variables */
      for (j = 1; j <= n; j++)
      {  cbar_j = dir * cbar[j];
         /* if xN[j] doesn't affect on the obj. function, skip it */
         if (cbar_j == 0.0) continue;
         /* analyze main cases */
         k = indx[m+j]; /* x[k] = xN[j] */
         switch (tagx[k])
         {  case LPX_NL:
               /* xN[j] can increase */
               if (cbar_j > -tol) continue;
               break;
            case LPX_NU:
               /* xN[j] can decrease */
               if (cbar_j < +tol) continue;
               break;
            case LPX_NF:
               /* xN[j] can change in any direction */
               if (-tol < cbar_j && cbar_j < +tol) continue;
               break;
            case LPX_NS:
               /* xN[j] can't change */
               continue;
            default:
               insist(tagx != tagx);
         }
         /* xN[j] can improve the objective function */
         temp = (cbar_j * cbar_j) / gvec[j];
         if (best < temp) q = j, best = temp;
      }
      /* since reduced costs are not computed directly every time, but
         recomputed recursively, the choice sometimes may be unreliable
         due to excessive round-off errors in the reduced costs; so, if
         q = 0 or if some test shows that the current value of cbar[q]
         is inaccurate, it is reasonable to recompute components of the
         current basic solution and then repeat the choice */
      if (!retry)
      {  if (q == 0)
         {  /* the current basic solution seems to be dual feasible,
               but we need to be sure that this is really so */
            retry = 1;
         }
         else
         {  /* xN[q] has been chosen; recompute its reduced cost more
               accurately using the formula d[q] = cN[q] - pi' * N[q] */
            k = indx[m+q]; /* x[k] = xN[q] */
            cbar_q = coef[k];
            if (k <= m)
            {  /* x[k] is auxiliary variable */
               cbar_q -= pi[k];
            }
            else
            {  /* x[k] is structural variable */
               j_beg = aa_ptr[k];
               j_end = j_beg + aa_len[k] - 1;
               for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
                  cbar_q += pi[sv_ndx[j_ptr]] * sv_val[j_ptr];
            }
            /* estimate an error in cbar[q] */
            temp = fabs(cbar[q] - cbar_q) / (1.0 + fabs(cbar_q));
            if (temp <= 0.10 * tol)
            {  /* the error is not so big; replace cbar[q] by the new,
                  more accurate value */
               cbar[q] = cbar_q;
            }
            else
            {  /* the error is too big; in this case cbar[q] may have
                  wrong sign (if it is close to zero) and involve wrong
                  choice a basic variable xB[p] in the future causing
                  numeric instability; therefore the vector of reduced
                  costs should be recomputed more accurately */
               if (lp->msg_lev >= 3)
                  print("spx_prim_chuzc: recomputing basic solution com"
                     "ponents");
               retry = 1;
            }
         }
         if (retry) goto loop;
      }
      /* store the number q and return */
      spx->q = q;
      return retry;
}

/*----------------------------------------------------------------------
-- spx_prim_chuzr - choose basic variable (primal simplex).
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- int spx_prim_chuzr(SPX *spx, double relax);
--
-- *Description*
--
-- The routine spx_prim_chuzr chooses a basic variable xB[p], which
-- should leave the basis.
--
-- The parameter relax is used to relax bounds of basic variables. If
-- relax = 0, the routine implements standard ("textbook") ratio test.
-- Otherwise, if relax > 0, the routine implements two-pass ratio test
-- proposed by P.Harris.
--
-- The routine chooses a basic variable xB[p], which reaches its bound
-- before any other basic variables when the chosen non-basic variable
-- xN[q] is changing in a valid direction.
--
-- If the basic variable xB[p] has been chosen, the routine sets p to
-- the number of this variable, 1 <= p <= m, and also provide the tag
-- p_tag, which should be set for xB[p] after it has left the basis.
-- The special case p < 0 means that the non-basic variable xN[q] being
-- double-bounded just goes to its opposite bound, so the current basis
-- remains unchanged. If xN[q] can infinitely change and therefore the
-- choice cannot be made, the routine sets p to 0.
--
-- *Returns*
--
-- Non-zero return code means that all elements of the q-th column of
-- the simplex table are too small (in absolute value); in this case
-- it is desirable to reinvert the current basis matrix and repeat the
-- choice. */

int spx_prim_chuzr(SPX *spx, double relax)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      double dir = (lp->dir == LPX_MIN ? +1.0 : -1.0);
      double *bbar = lp->bbar;
      double *cbar = lp->cbar;
      int *indx = lp->indx;
      int q = spx->q;
      double *aq = spx->aq;
      int i, i_tag, k, p, p_tag, ret = 0;
      double aq_i, abs_aq_i, big, eps, temp, teta;
      insist(1 <= q && q <= n);
      /* turn to the case of increasing xN[q] in order to simplify the
         program logic */
      if (dir * cbar[q] > 0.0)
         for (i = 1; i <= m; i++) aq[i] = - aq[i];
      /* compute the largest absolute value of elements of the q-th
         column of the simplex table */
      big = 0.0;
      for (i = 1; i <= m; i++)
      {  temp = aq[i];
         if (temp < 0.0) temp = - temp;
         if (big < temp) big = temp;
      }
#if 0
      /* if all elements of the q-th column of the simplex table are
         too small, tell the calling program to reinvert the basis and
         repeat the choice */
      if (big < lp->tol_piv) ret = 1;
#endif
      /* compute the absolute tolerance eps used to check elements of
         the q-th column */
      eps = lp->tol_piv * (1.0 + big);
      /* initial settings for the first pass */
      k = indx[m+q]; /* x[k] = xN[q] */
      if (typx[k] == LPX_DB)
      {  /* xN[q] has both lower and upper bounds */
         p = -1, p_tag = 0, teta = (ub[k] - lb[k]) + relax, big = 1.0;
      }
      else
      {  /* xN[q] has no opposite bound */
         p = 0, p_tag = 0, teta = DBL_MAX, big = 0.0;
      }
      /* look through the list of basic variables */
      for (i = 1; i <= m; i++)
      {  aq_i = aq[i];
         /* if xN[q] doesn't affect on xB[i], skip the latter */
         if (aq_i == 0.0) continue;
         abs_aq_i = (aq_i > 0.0 ? +aq_i : -aq_i);
         /* analyze main cases */
         k = indx[i]; /* x[k] = xB[i] */
         switch (typx[k])
         {  case LPX_FR:
               /* xB[i] is free variable */
               continue;
            case LPX_LO:
lo_1:          /* xB[i] has lower bound */
               if (aq_i > -eps) continue;
               i_tag = LPX_NL;
               temp = ((lb[k] - relax) - bbar[i]) / aq_i;
               break;
            case LPX_UP:
               /* xB[i] has upper bound */
up_1:          if (aq_i < +eps) continue;
               i_tag = LPX_NU;
               temp = ((ub[k] + relax) - bbar[i]) / aq_i;
               break;
            case LPX_DB:
               /* xB[i] has both lower and upper bounds */
               if (aq_i < 0.0) goto lo_1; else goto up_1;
            case LPX_FX:
               /* xB[i] is fixed variable */
               if (abs_aq_i < eps) continue;
               i_tag = LPX_NS;
               temp = relax / abs_aq_i;
               break;
            default:
               insist(typx != typx);
         }
         /* if xB[i] violates its bound (slightly, because the current
            basis is assumed to be primal feasible), temp is negative;
            we can think that this happens due to round-off errors and
            xB[i] is exactly on its bound; this allows to replace temp
            by zero */
         if (temp < 0.0) temp = 0.0;
         /* apply minimal ratio test */
         if (teta > temp || teta == temp && big < abs_aq_i)
            p = i, p_tag = i_tag, teta = temp, big = abs_aq_i;
      }
      /* the standard ratio test has been completed */
      if (relax == 0.0) goto done;
      /* if no basic variable has been chosen on the first pass, the
         second pass should be skipped */
      if (p == 0) goto done;
      /* teta is a maximal change (in absolute value) of the variable
         xN[q] in a valid direction, on which all the basic variables
         still remain within their (relaxed) bounds; on the second pass
         we try to choose a basic variable xB[p], for which the change
         of xN[q] is not greater than teta and which has the greatest
         (in absolute value) influence coefficient (element of the q-th
         column of the simplex table) */
      teta *= (1.0 + 3.0 * DBL_EPSILON);
      /* if xN[q] is double-bounded variable and can go to the opposite
         bound, it is reasonable to choose it */
      k = indx[m+q]; /* x[k] = xN[q] */
      if (typx[k] == LPX_DB && ub[k] - lb[k] <= teta)
      {  p = -1, p_tag = 0;
         goto done;
      }
      /* initial settings for the second pass */
      p = 0, p_tag = 0, big = 0.0;
      /* look through the list of basic variables */
      for (i = 1; i <= m; i++)
      {  aq_i = aq[i];
         /* if xN[q] doesn't affect on xB[i], skip the latter */
         if (aq_i == 0.0) continue;
         abs_aq_i = (aq_i > 0.0 ? +aq_i : -aq_i);
         /* analyze main cases */
         k = indx[i]; /* x[k] = xB[i] */
         switch (typx[k])
         {  case LPX_FR:
               /* xB[i] is free variable */
               continue;
            case LPX_LO:
lo_2:          /* xB[i] has lower bound */
               if (aq_i > -eps) continue;
               i_tag = LPX_NL;
               temp = (lb[k] - bbar[i]) / aq_i;
               break;
            case LPX_UP:
up_2:          /* xB[i] has upper bound */
               if (aq_i < +eps) continue;
               i_tag = LPX_NU;
               temp = (ub[k] - bbar[i]) / aq_i;
               break;
            case LPX_DB:
               /* xB[i] has both lower and upper bounds */
               if (aq_i < 0.0) goto lo_2; else goto up_2;
            case LPX_FX:
               /* xB[i] is fixed variable */
               if (abs_aq_i < eps) continue;
               i_tag = LPX_NS;
               temp = 0.0;
               break;
            default:
               insist(typx != typx);
         }
         /* see comments about this trick in the first pass loop */
         if (temp < 0.0) temp = 0.0;
         /* apply Harris' rule */
         if (temp <= teta && big < abs_aq_i)
            p = i, p_tag = i_tag, big = abs_aq_i;
      }
      /* the second pass should always choose some xB[p] */
      insist(1 <= p && p <= m);
done: /* restore original signs of the coefficients aq[i] */
      if (dir * cbar[q] > 0.0)
         for (i = 1; i <= m; i++) aq[i] = - aq[i];
      /* store the number p and the tag p_tag */
      spx->p = p, spx->p_tag = p_tag;
      /* return to the simplex method program */
      return ret;
}

/*----------------------------------------------------------------------
-- spx_dual_chuzr - choose basic variable (dual simplex).
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_dual_chuzr(SPX *spx, double tol);
--
-- *Description*
--
-- The routine spx_dual_chuzr chooses a basic variable xB[p], which
-- should leave the basis.
--
-- The parameter tol is a relative tolerance, which is used to check
-- if some basic variable violates the primal feasibility condition and
-- therefore can be considered as a possible candidate.
--
-- The routine chooses a basic variable xB[p], which has the largest
-- weighted residual:
--
--    |r[p]| / sqrt(delta[p]) = max(|r[i]| / sqrt(delta[i])),
--
-- where r[i] is a current residual for variable xB[i]:
--
--    r[i] = max(0, lB[i] - bbar[i]), if xB[i] has an lower bound,
--
--    r[i] = max(0, bbar[i] - uB[i]), if xB[i] has an upper bound,
--
-- and delta[i] is a "weight" of the variable xB[i].
--
-- If the basic variable xB[p] has been chosen, the routine sets p to
-- the number of this variable, 1 <= p <= m, and also provide the tag
-- p_tag, which should be set for xB[p] after it has left the basis.
-- Note that if xB[p] is fixed variable, the routine sets the tag as if
-- xB[p] were double-bounded variable; this is used in order to know in
-- what direction this variable will change leaving the basis. If the
-- current basis is primal feasible and therefore no choice has been
-- made, the routine sets p to 0. */

void spx_dual_chuzr(SPX *spx, double tol)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      int *indx = lp->indx;
      double *bbar = lp->bbar;
      double *dvec = spx->dvec;
      int i, k, p, p_tag, typx_k;
      double best, lb_k, ub_k, bbar_i, temp;
      /* nothing chosen so far */
      p = 0, p_tag = 0, best = 0.0;
      /* look through the list of basic variables */
      for (i = 1; i <= m; i++)
      {  k = indx[i]; /* x[k] = xB[i] */
         typx_k = typx[k];
         bbar_i = bbar[i];
         if (typx_k == LPX_LO || typx_k == LPX_DB || typx_k == LPX_FX)
         {  /* xB[i] has lower bound */
            lb_k = lb[k];
            temp = bbar_i - lb_k;
            lb_k = (lb_k < 0.0 ? -lb_k : +lb_k);
            if (temp / (1.0 + lb_k) < - tol)
            {  /* xB[i] violates its lower bound */
               temp = (temp * temp) / dvec[i];
               if (best < temp) p = i, p_tag = LPX_NL, best = temp;
            }
         }
         if (typx_k == LPX_UP || typx_k == LPX_DB || typx_k == LPX_FX)
         {  /* xB[i] has upper bound */
            ub_k = ub[k];
            temp = bbar_i - ub_k;
            ub_k = (ub_k < 0.0 ? -ub_k : +ub_k);
            if (temp / (1.0 + ub_k) > + tol)
            {  /* xB[i] violates its upper bound */
               temp = (temp * temp) / dvec[i];
               if (best < temp) p = i, p_tag = LPX_NU, best = temp;
            }
         }
      }
      spx->p = p, spx->p_tag = p_tag;
      return;
}

/*----------------------------------------------------------------------
-- spx_dual_chuzc - choose non-basic variable (dual simplex).
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- int spx_dual_chuzc(SPX *spx, double relax);
--
-- *Description*
--
-- The routine spx_dual_chuzc chooses a non-basic variable xN[q], which
-- should enter the basis.
--
-- The parameter relax is used to relax (zero) bounds of reduced costs
-- of non-basic variables (i.e. bounds of dual variables). If relax = 0,
-- the routine implements standard ("textbook") ratio test. Otherwise,
-- if relax > 0, the routine implements two-pass ratio test proposed by
-- P.Harris.
--
-- The routine chooses a non-basic variables xN[q], whose reduced cost
-- reaches zero before reduced costs of any other non-basic variables
-- when reduced cost of the chosen basic variable xB[p] is changing in
-- in a valid direction starting from zero.
--
-- If the non-basic variable xN[q] has been chosen, the routine sets q
-- to the number of this variable, 1 <= q <= n. If reduced cost of xB[p]
-- can infinitely change and therefore the choice cannot be made, the
-- routine sets q to 0. */

int spx_dual_chuzc(SPX *spx, double relax)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      double dir = (lp->dir == LPX_MIN ? +1.0 : -1.0);
      int *indx = lp->indx;
      int *tagx = lp->tagx;
      double *cbar = lp->cbar;
      int p = spx->p;
      int p_tag = spx->p_tag;
      double *ap = spx->ap;
      int j, k, q, ret = 0;
      double big, eps, teta, ap_j, abs_ap_j, temp;
      insist(1 <= p && p <= m);
      insist(p_tag == LPX_NL || p_tag == LPX_NU);
      /* turn to the case of increasing xB[p] in order to simplify the
         program logic */
      if (p_tag == LPX_NU)
         for (j = 1; j <= n; j++) ap[j] = - ap[j];
      /* compute the largest absolute value of elements of the p-th row
         of the simplex table */
      big = 0.0;
      for (j = 1; j <= n; j++)
      {  temp = ap[j];
         if (temp < 0.0) temp = - temp;
         if (big < temp) big = temp;
      }
#if 0
      /* if all elements in the p-th row of the simplex table are too
         small, tell the calling program to reinvert the basis and then
         repeat the choice */
      if (big < lp->tol_piv) ret = 1;
#endif
      /* compute the absolute tolerance eps used to check elements of
         the p-th row */
      eps = lp->tol_piv * (1.0 + big);
      /* initial settings for the first pass */
      q = 0, teta = DBL_MAX, big = 0.0;
      /* look through the list of non-basic variables */
      for (j = 1; j <= n; j++)
      {  ap_j = ap[j];
         /* if xN[j] doesn't affect ob xB[p], skip the former */
         if (ap_j == 0.0) continue;
         abs_ap_j = (ap_j > 0.0 ? +ap_j : -ap_j);
         /* analyze main cases */
         k = indx[m+j]; /* x[k] = xN[j] */
         switch (tagx[k])
         {  case LPX_NF:
               /* xN[j] is free variable */
               if (abs_ap_j < eps) continue;
               temp = relax / abs_ap_j;
               break;
            case LPX_NL:
               /* xN[j] is on its lower bound */
               if (ap_j < +eps) continue;
               temp = (dir * cbar[j] + relax) / ap_j;
               break;
            case LPX_NU:
               /* xN[j] is on its upper bound */
               if (ap_j > -eps) continue;
               temp = (dir * cbar[j] - relax) / ap_j;
               break;
            case LPX_NS:
               /* xN[j] is fixed variable */
               continue;
            default:
               insist(tagx != tagx);
         }
         /* if reduced cost of xN[j] violates its zero bound (slightly,
            because the current basis is assumed to be dual feasible),
            temp is negative; we can think that this happens due to
            round-off errors and reduced cost of xN[j] is exact zero in
            this case; this allows to replace temp by zero */
         if (temp < 0.0) temp = 0.0;
         /* apply minimal ratio test */
         if (teta > temp || teta == temp && big < abs_ap_j)
            q = j, teta = temp, big = abs_ap_j;
      }
      /* the standard ratio test has been completed */
      if (relax == 0.0) goto done;
      /* if no variable has been chosen on the first pass, the second
         pass should be skipped */
      if (q == 0) goto done;
      /* teta is a maximal change (in absolute value) of reduced cost
         of the variable xB[p] in a valid direction, on which reduced
         costs of all the non-basic variables still remain within their
         relaxed bounds; on the second pass we try to choose a non-basic
         variable xN[q], for which the change of reduced cost of xB[p]
         if not greater than teta and which has the largest (in absolute
         value) influence coefficient (i.e. element of the p-th row of
         the simplex table) */
      teta *= (1.0 + 3.0 * DBL_EPSILON);
      /* initial settings for the second pass */
      q = 0, big = 0.0;
      /* look through the list of non-basic variables */
      for (j = 1; j <= n; j++)
      {  ap_j = ap[j];
         /* if xN[j] doesn't affect ob xB[p], skip the former */
         if (ap_j == 0.0) continue;
         abs_ap_j = (ap_j > 0.0 ? +ap_j : -ap_j);
         /* analyze main cases */
         k = indx[m+j]; /* x[k] = xN[j] */
         switch (tagx[k])
         {  case LPX_NF:
               /* xN[j] is free variable */
               if (abs_ap_j < eps) continue;
               temp = 0.0;
               break;
            case LPX_NL:
               /* xN[j] is on its lower bound */
               if (ap_j < +eps) continue;
               temp = (dir * cbar[j]) / ap_j;
               break;
            case LPX_NU:
               /* xN[j] is on its upper bound */
               if (ap_j > -eps) continue;
               temp = (dir * cbar[j]) / ap_j;
               break;
            case LPX_NS:
               /* xN[j] is fixed variable */
               continue;
            default:
               insist(tagx != tagx);
         }
         /* see comments about this trick in the first pass loop */
         if (temp < 0.0) temp = 0.0;
         /* apply Harris' rule */
         if (temp <= teta && big < abs_ap_j)
            q = j, big = abs_ap_j;
      }
      /* the second pass should always choose some xN[q] */
      insist(1 <= q && q <= n);
done: /* restore original signs of the coefficients ap[j] */
      if (p_tag == LPX_NU)
         for (j = 1; j <= n; j++) ap[j] = - ap[j];
      /* store the number q */
      spx->q = q;
      /* return to the simplex method program */
      return ret;
}

/*----------------------------------------------------------------------
-- spx_update_bbar - update values of basic variables.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_update_bbar(SPX *spx, double *obj);
--
-- *Description*
--
-- The routine spx_update_bbar recomputes values of basic variables for
-- an adjacent basis.
--
-- If the parameter obj is not NULL, it is assumed that *obj is the
-- current value of the objective function, in which case the routine
-- recomputes it for the adjacent basis.
--
-- This routine assumes that the basic variable xB[p] and the non-basic
-- variable xN[q] (that define the adjacent basis) have been chosen but
-- the current basis has not been changed yet. */

void spx_update_bbar(SPX *spx, double *obj)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      int *tagx = lp->tagx;
      int *indx = lp->indx;
      double *bbar = lp->bbar;
      int p = spx->p;
      int p_tag = spx->p_tag;
      int q = spx->q;
      double *aq = spx->aq;
      int i, k;
      double aq_i, dxn_q, new_xb_p;
      if (p < 0)
      {  /* the special case: xN[q] goes to its opposite bound */
         insist(1 <= q && q <= n);
         k = indx[m+q]; /* x[k] = xN[q] */
         insist(typx[k] == LPX_DB);
         switch (tagx[k])
         {  case LPX_NL:
               /* xN[q] goes from its lower bound to its upper one */
               dxn_q = ub[k] - lb[k];
               break;
            case LPX_NU:
               /* xN[q] goes from its upper bound to its lower one */
               dxn_q = lb[k] - ub[k];
               break;
            default:
               insist(tagx != tagx);
         }
         /* recompute values of the basic variables */
         for (i = 1; i <= m; i++)
         {  aq_i = aq[i];
            if (aq_i != 0.0) bbar[i] += aq_i * dxn_q;
         }
      }
      else
      {  /* xB[p] will leave and xN[q] will enter the basis */
         insist(1 <= p && p <= m);
         insist(1 <= q && q <= n);
         /* determine value of xB[p] in the adjacent basis */
         k = indx[p];
         switch (p_tag)
         {  case LPX_NL:
               new_xb_p = lb[k]; break;
            case LPX_NU:
               new_xb_p = ub[k]; break;
            case LPX_NF:
               new_xb_p = 0.0; break;
            case LPX_NS:
               new_xb_p = lb[k]; break;
            default:
               insist(p_tag != p_tag);
         }
         /* determine increment of xN[q] in the adjacent basis */
         insist(aq[p] != 0.0);
         dxn_q = (new_xb_p - bbar[p]) / aq[p];
         /* compute a new value of xN[q] when it has entered the basis
            and taken the place of xB[p] */
         bbar[p] = spx_eval_xn_j(lp, q) + dxn_q;
         /* recompute values of other basic variables */
         for (i = 1; i <= m; i++)
         {  if (i == p) continue;
            aq_i = aq[i];
            if (aq_i != 0.0) bbar[i] += aq_i * dxn_q;
         }
      }
      /* recompute value of the objective function (if req'd) */
      if (obj != NULL) *obj += lp->cbar[q] * dxn_q;
      return;
}

/*----------------------------------------------------------------------
-- spx_update_pi - update simplex multipliers.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_update_pi(SPX *spx);
--
-- *Description*
--
-- The routine spx_update_pi recomputes simplex multipliers (Lagrange
-- multipliers for the equality constraints) for an adjacent basis.
--
-- This routine assumes that the basic variable xB[p] and the non-basic
-- variable xN[q] (that define the adjacent basis) have been chosen but
-- the current basis has not been changed yet. */

void spx_update_pi(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      double *pi = lp->pi;
      double *cbar = lp->cbar;
      int p = spx->p;
      int q = spx->q;
      double *zeta = spx->zeta;
      double *ap = spx->ap;
      int i;
      double new_cbar_q, zeta_i;
      /* xB[p] will leave and xN[q] will enter the basis */
      insist(1 <= p && p <= m);
      insist(1 <= q && q <= n);
      insist(ap[q] != 0.0);
      new_cbar_q = cbar[q] / ap[q];
      for (i = 1; i <= m; i++)
      {  zeta_i = zeta[i];
         if (zeta_i != 0.0) pi[i] -= zeta_i * new_cbar_q;
      }
      return;
}

/*----------------------------------------------------------------------
-- spx_update_cbar - update reduced costs of non-basic variables.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_update_cbar(SPX *spx, int all);
--
-- *Description*
--
-- The routine spx_update_cbar recomputes reduced costs of non-basic
-- variables for an adjacent basis.
--
-- If the parameter all is set, the routine recomputes reduced costs of
-- all non-basic variables. Otherwise reduced costs for fixed non-basic
-- variables are not recomputed.
--
-- This routine assumes that the basic variable xB[p] and the non-basic
-- variable xN[q] (that define the adjacent basis) have been chosen but
-- the current basis has not been changed yet. */

void spx_update_cbar(SPX *spx, int all)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *tagx = lp->tagx;
      int *indx = lp->indx;
      double *cbar = lp->cbar;
      int p = spx->p;
      int q = spx->q;
      double *ap = spx->ap;
      int j, k;
      double new_cbar_q, ap_j;
      /* xB[p] will leave and xN[q] will enter the basis */
      insist(1 <= p && p <= m);
      insist(1 <= q && q <= n);
      /* compute a new reduced cost of xB[p] when it has left the basis
         and taken the place of xN[q] */
      new_cbar_q = (cbar[q] /= ap[q]);
      /* recompute reduced costs of other non-basic variables */
      for (j = 1; j <= n; j++)
      {  if (j == q) continue;
         if (!all)
         {  k = indx[m+j]; /* x[k] = xN[j] */
            if (tagx[k] == LPX_NS)
            {  cbar[j] = 0.0;
               continue;
            }
         }
         ap_j = ap[j];
         if (ap_j != 0.0) cbar[j] -= ap_j * new_cbar_q;
      }
      return;
}

/*----------------------------------------------------------------------
-- change_basis - change basis and update the factorization.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- int spx_change_basis(SPX *spx);
--
-- *Description*
--
-- The routine spx_change_basis changes the current basis assuming that
-- the basic variable xB[p] leaves the basis and the non-basic variable
-- xN[q] enters it. Then the routine updates the factorization of the
-- basis matrix for the adjacent basis.
--
-- In the specilal case, which is indicated by p < 0, the current basis
-- is not changed, but the non-basic double-bounded variable xN[q] goes
-- from its current bound to the opposite one.
--
-- *Returns*
--
-- Zero return code means that the factorization has been successfully
-- updated. Non-zero return code means that the factorization should be
-- reinverted by the routine spx_invert. */

int spx_change_basis(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *typx = lp->typx;
      int *tagx = lp->tagx;
      int *posx = lp->posx;
      int *indx = lp->indx;
      int p = spx->p;
      int p_tag = spx->p_tag;
      int q = spx->q;
      int k, kp, kq, ret;
      if (p < 0)
      {  /* the special case: xN[q] goes to its opposite bound */
         insist(1 <= q && q <= n);
         k = indx[m+q]; /* x[k] = xN[q] */
         insist(typx[k] == LPX_DB);
         insist(tagx[k] == LPX_NL || tagx[k] == LPX_NU);
         tagx[k] = (tagx[k] == LPX_NL ? LPX_NU : LPX_NL);
         ret = 0;
      }
      else
      {  /* xB[p] leaves the basis, xN[q] enters the basis */
         insist(1 <= p && p <= m);
         insist(1 <= q && q <= n);
         /* xN[q] takes the place of xB[p] and vice versa */
         kp = indx[p];   /* x[kp] = xB[p] */
         kq = indx[m+q]; /* x[kq] = xN[q] */
         tagx[kp] = p_tag,  posx[kp] = m+q, indx[m+q] = kp;
         tagx[kq] = LPX_BS, posx[kq] = p,   indx[p]   = kq;
         /* check the tag of xN[q] (former xB[p]) */
         switch (typx[kp])
         {  case LPX_FR:
               insist(p_tag == LPX_NF); break;
            case LPX_LO:
               insist(p_tag == LPX_NL); break;
            case LPX_UP:
               insist(p_tag == LPX_NU); break;
            case LPX_DB:
               insist(p_tag == LPX_NL || p_tag == LPX_NU); break;
            case LPX_FX:
               insist(p_tag == LPX_NS); break;
            default:
               insist(typx != typx);
         }
         /* update the factorization of the basis matrix */
         ret = spx_update(lp, p);
      }
      /* one simplex iteration has been performed */
      if (lp->it_lim > 0) lp->it_lim--;
      lp->it_cnt++;
      /* return to the simplex method program */
      return ret;
}

/*----------------------------------------------------------------------
-- spx_err_in_bbar - compute maximal absolute error in bbar.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_err_in_bbar(SPX *spx);
--
-- *Returns*
--
-- The routine spx_err_in_bbar returns the maximal absolute error
--
--    max|bbar[i] - bbar'[i]|,
--
-- where bbar and bbar' are, respectively, directly computed and the
-- current (updated) values of basic variables.
--
-- This routine is intended for debugging purposes only. */

double spx_err_in_bbar(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      double *bbar = lp->bbar;
      int i;
      double d, dmax;
      lp->bbar = ucalloc(1+m, sizeof(double));
      spx_eval_bbar(lp);
      dmax = 0.0;
      for (i = 1; i <= m; i++)
      {  d = fabs(lp->bbar[i] - bbar[i]);
         if (dmax < d) dmax = d;
      }
      ufree(lp->bbar);
      lp->bbar = bbar;
      return dmax;
}

/*----------------------------------------------------------------------
-- spx_err_in_pi - compute maximal absolute error in pi.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_err_in_pi(SPX *spx);
--
-- *Returns*
--
-- The routine spx_err_in_pi returns the maximal absolute error
--
--    max|pi[i] - pi'[i]|,
--
-- where pi and pi' are, respectively, directly computed and the current
-- (updated) values of simplex multipliers.
--
-- This routine is intended for debugging purposes only. */

double spx_err_in_pi(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      double *pi = lp->pi;
      int i;
      double d, dmax;
      lp->pi = ucalloc(1+m, sizeof(double));
      spx_eval_pi(lp);
      dmax = 0.0;
      for (i = 1; i <= m; i++)
      {  d = fabs(lp->pi[i] - pi[i]);
         if (dmax < d) dmax = d;
      }
      ufree(lp->pi);
      lp->pi = pi;
      return dmax;
}

/*----------------------------------------------------------------------
-- spx_err_in_cbar - compute maximal absolute error in cbar.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_err_in_cbar(SPX *spx, int all);
--
-- *Returns*
--
-- The routine spx_err_in_cbar returns the maximal absolute error
--
--    max|cbar[j] - cbar'[j]|,
--
-- where cbar and cbar' are, respectively, directly computed and the
-- current (updated) reduced costs of non-basic variables (if the flag
-- all is not set, fixed variables are not considered).
--
-- This routine is intended for debugging purposes only. */

double spx_err_in_cbar(SPX *spx, int all)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *tagx = lp->tagx;
      int *indx = lp->indx;
      double *cbar = lp->cbar;
      int j, k;
      double d, dmax;
      lp->cbar = ucalloc(1+n, sizeof(double));
      spx_eval_cbar(lp);
      dmax = 0.0;
      for (j = 1; j <= n; j++)
      {  if (!all)
         {  k = indx[m+j]; /* x[k] = xN[j] */
            if (tagx[k] == LPX_NS) continue;
         }
         d = fabs(lp->cbar[j] - cbar[j]);
         if (dmax < d) dmax = d;
      }
      ufree(lp->cbar);
      lp->cbar = cbar;
      return dmax;
}

/*----------------------------------------------------------------------
-- spx_reset_refsp - reset the reference space.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_reset_refsp(SPX *spx);
--
-- *Description*
--
-- The routine spx_reset_refsp resets (redefines) the reference space
-- and the vector gamma (in the case of primal simplex) or the vector
-- delta (in the case of dual simplex) used in the projected steepest
-- edge technique. After reset the reference space corresponds to the
-- current set of non-basic (primal simplex) or basic (dual simplex)
-- variables, and therefore all elements of the vector gamma or delta
-- are equal to one (by the definition). */

void spx_reset_refsp(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *tagx = lp->tagx;
      double *gvec = spx->gvec;
      double *dvec = spx->dvec;
      int *refsp = spx->refsp;
      int i, j, k;
      switch (spx->meth)
      {  case 'P':
            /* primal simplex is used */
            for (k = 1; k <= m+n; k++)
               refsp[k] = (tagx[k] == LPX_BS ? 0 : 1);
            for (j = 1; j <= n; j++) gvec[j] = 1.0;
            break;
         case 'D':
            /* dual simplex is used */
            for (k = 1; k <= m+n; k++)
               refsp[k] = (tagx[k] == LPX_BS ? 1 : 0);
            for (i = 1; i <= m; i++) dvec[i] = 1.0;
            break;
         default:
            insist(spx->meth != spx->meth);
      }
      /* after this number of simplex iterations the reference space
         should be again reset */
      spx->count = 1000;
      return;
}

/*----------------------------------------------------------------------
-- spx_update_gvec - update the vector gamma for adjacent basis.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_update_gvec(SPX *spx);
--
-- *Description*
--
-- The routine spx_update_gvec recomputes the vector gamma (used in the
-- primal projected steepest edge technique) for an adjacent basis.
--
-- This routine assumes that the basic variable xB[p] and the non-basic
-- variable xN[q] (that define the adjacent basis) have been chosen but
-- the current basis has not been changed yet. */

void spx_update_gvec(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *tagx = lp->tagx;
      int *indx = lp->indx;
      int p = spx->p;
      int q = spx->q;
      double *ap = spx->ap;
      double *aq = spx->aq;
      double *gvec = spx->gvec;
      int *refsp = spx->refsp;
      int i, j, j_beg, j_end, j_ptr, k, ref_k, ref_p, ref_q;
      double ap_j, ap_q, aq_i, s_j, t1, t2, t3, sum, temp;
      double *w = spx->work;
      insist(1 <= p && p <= m);
      insist(1 <= q && q <= n);
      /* check if it's time to reset the reference space */
      if (spx->count <= 0)
      {  /* yes, do that */
         spx_reset_refsp(spx);
         goto done;
      }
      else
      {  /* otherwise decrease the count */
         spx->count--;
      }
      /* compute t1 and w */
      t1 = 0.0;
      for (i = 1; i <= m; i++)
      {  if (i != p && refsp[indx[i]])
         {  w[i] = aq[i];
            t1 += w[i] * w[i];
         }
         else
            w[i] = 0.0;
      }
      spx_btran(lp, w);
      /* update the vector gamma */
      ref_p = refsp[indx[p]];    /* if xB[p] belongs to the space */
      ref_q = refsp[indx[m+q]];  /* if xN[q] belongs to the space */
      ap_q = ap[q];
      insist(ap_q != 0.0);
      for (j = 1; j <= n; j++)
      {  /* gvec[q] will be computed later */
         if (j == q) continue;
         /* if xN[j] is fixed variable, its weight is not used, because
            fixed variables never enter the basis */
         k = indx[m+j]; /* x[k] = xN[j] */
         if (tagx[k] == LPX_NS)
         {  gvec[j] = 1.0;
            continue;
         }
         ref_k = refsp[k]; /* if xN[j] belongs to the space */
         /* compute s[j] */
         ap_j = ap[j];
         s_j = gvec[j];
         if (ref_p) s_j -= ap_j * ap_j;
         if (ref_k) s_j -= 1.0;
         if (ap_j == 0.0)
            t3 = 0.0;
         else
         {  /* t2 := N[j] * w */
            if (k <= m)
            {  /* x[k] is auxiliary variable */
               t2 = + w[k];
            }
            else
            {  /* x[k] is structural variable */
               j_beg = aa_ptr[k];
               j_end = j_beg + aa_len[k] - 1;
               t2 = 0.0;
               for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
                  t2 -= sv_val[j_ptr] * w[sv_ndx[j_ptr]];
            }
            t3 = ap_j / ap_q;
            s_j += (2.0 * t2 + t1 * t3) * t3;
         }
         /* update gvec[j] */
         if (ref_k) s_j += 1.0;
         if (ref_q) s_j += t3 * t3;
         if (s_j < DBL_EPSILON) s_j = 1.0;
         gvec[j] = s_j;
      }
      /* compute exact value of gvec[q] */
      sum = (ref_p ? 1.0 : 0.0);
      temp = ap_q * ap_q;
      for (i = 1; i <= m; i++)
      {  if (i == p)
         {  if (ref_q)
               sum += 1.0 / temp;
         }
         else
         {  if (refsp[indx[i]])
            {  aq_i = aq[i];
               sum += (aq_i * aq_i) / temp;
            }
         }
      }
      gvec[q] = sum;
done: return;
}

/*----------------------------------------------------------------------
-- spx_err_in_gvec - compute maximal absolute error in gvec.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_err_in_gvec(SPX *spx);
--
-- *Returns*
--
-- The routine spx_err_in_gvec returns the maximal absolute error
--
--    max|gvec[j] - gvec'[j]|,
--
-- where gvec and gvec' are, respectively, directly computed and the
-- current (updated) elements of the vector gamma.
--
-- This routine is intended for debugging purposes only. */

double spx_err_in_gvec(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *indx = lp->indx;
      double *gvec = spx->gvec;
      int *refsp = spx->refsp;
      double *aj = spx->work;
      int i, j, k;
      double dmax, d, gvec_j;
      dmax = 0.0;
      for (j = 1; j <= n; j++)
      {  /* if xN[j] is fixed variable, skip it, because its weight is
            not updated */
         k = indx[m+j];
         if (lp->typx[k] == LPX_FX)
         {  insist(lp->tagx[k] == LPX_NS);
            continue;
         }
         /* compute exact value of gvec[j] */
         spx_eval_col(lp, j, aj, 0);
         gvec_j = (refsp[indx[m+j]] ? 1.0 : 0.0);
         for (i = 1; i <= m; i++)
            if (refsp[indx[i]]) gvec_j += aj[i] * aj[i];
         /* compute absolute error in gvec[j] */
         d = fabs(gvec_j - gvec[j]);
         if (dmax < d) dmax = d;
      }
      return dmax;
}

/*----------------------------------------------------------------------
-- spx_update_dvec - update the vector delta for adjacent basis.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_update_dvec(SPX *spx);
--
-- *Description*
--
-- The routine spx_update_dvec recomputes the vector delta (used in the
-- dual projected steepest edge technique) for an adjacent basis.
--
-- This routine assumes that the basic variable xB[p] and the non-basic
-- variable xN[q] (that define the adjacent basis) have been chosen but
-- the current basis has not been changed yet. */

void spx_update_dvec(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *typx = lp->typx;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *indx = lp->indx;
      int p = spx->p;
      int q = spx->q;
      double *ap = spx->ap;
      double *aq = spx->aq;
      double *dvec = spx->dvec;
      int *refsp = spx->refsp;
      double *w = spx->work;
      int i, j, j_beg, j_end, j_ptr, k, ref_k, ref_p, ref_q;
      double ap_j, aq_p, aq_i, s_i, t1, sum, temp;
      insist(1 <= p && p <= m);
      insist(1 <= q && q <= n);
      /* check if it's time to reset the reference space */
      if (spx->count <= 0)
      {  /* yes, do that */
         spx_reset_refsp(spx);
         goto done;
      }
      else
      {  /* otherwise decrease the count */
         spx->count--;
      }
      /* compute t1 */
      t1 = 0.0;
      for (j = 1; j <= n; j++)
      {  if (j != q && refsp[indx[m+j]])
         {  ap_j = ap[j];
            t1 += ap_j * ap_j;
         }
      }
      /* compute w */
      for (i = 1; i <= m; i++) w[i] = 0.0;
      for (j = 1; j <= n; j++)
      {  if (j != q && refsp[indx[m+j]])
         {  /* w += ap[j] * N[j] */
            ap_j = ap[j];
            if (ap_j == 0.0) continue;
            k = indx[m+j]; /* x[k] = xN[j] */
            if (k <= m)
               /* xN[j] is auxiliary variable */
               w[k] += ap_j;
            else
            {  /* xN[j] is structural variable */
               j_beg = aa_ptr[k];
               j_end = j_beg + aa_len[k] - 1;
               for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
                  w[sv_ndx[j_ptr]] -= ap_j * sv_val[j_ptr];
            }
         }
      }
      spx_ftran(lp, w, 0);
      /* update the vector delta */
      ref_p = refsp[indx[p]];    /* if xB[p] belongs to the space */
      ref_q = refsp[indx[m+q]];  /* if xN[q] belongs to the space */
      aq_p = aq[p];
      insist(aq_p != 0.0);
      for (i = 1; i <= m; i++)
      {  /* dvec[p] will be computed later */
         if (i == p) continue;
         /* if xB[i] is free variable, its weight is not used, because
            free variables never leave the basis */
         k = indx[i];
         if (typx[k] == LPX_FR)
         {  dvec[i] = 1.0;
            continue;
         }
         ref_k = refsp[k]; /* if xB[i] belongs to the space */
         /* compute s_i */
         aq_i = aq[i];
         s_i = dvec[i];
         if (ref_k) s_i -= 1.0;
         if (ref_q) s_i -= aq[i] * aq[i];
         if (aq_i == 0.0)
            temp = 0.0;
         else
         {  temp = aq_i / aq_p;
            s_i += (2.0 * w[i] + t1 * temp) * temp;
         }
         /* update dvec[i] */
         if (ref_k) s_i += 1.0;
         if (ref_p) s_i += temp * temp;
         if (s_i < DBL_EPSILON) s_i = 1.0;
         dvec[i] = s_i;
      }
      /* compute exact value of dvec[p] */
      sum = (ref_q ? 1.0 : 0.0);
      temp = aq_p * aq_p;
      for (j = 1; j <= n; j++)
      {  if (j == q)
         {  if (ref_p)
               sum += 1.0 / temp;
         }
         else
         {  if (refsp[indx[m+j]])
            {  ap_j = ap[j];
               sum += (ap_j * ap_j) / temp;
            }
         }
      }
      dvec[p] = sum;
done: return;
}

/*----------------------------------------------------------------------
-- spx_err_in_dvec - compute maximal absolute error in dvec.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- double spx_err_in_dvec(SPX *spx);
--
-- *Returns*
--
-- The routine spx_err_in_dvec returns the maximal absolute error
--
--    max|dvec[j] - dvec'[j]|,
--
-- where dvec and dvec' are, respectively, directly computed and the
-- current (updated) elements of the vector delta.
--
-- This routine is intended for debugging purposes only. */

double spx_err_in_dvec(SPX *spx)
{     LPX *lp = spx->lp;
      int m = lp->m;
      int n = lp->n;
      int *typx = lp->typx;
      int *indx = lp->indx;
      double *dvec = spx->dvec;
      int *refsp = spx->refsp;
      double *rho = spx->work;
      double *ai = &spx->work[m];
      int i, j, k;
      double dmax, d, dvec_i;
      dmax = 0.0;
      for (i = 1; i <= m; i++)
      {  /* if xB[i] is free variable, skip it, because its weight is
            not updated */
         k = indx[i];
         if (typx[k] == LPX_FR) continue;
         /* compute exact value of dvec[i] */
         spx_eval_rho(lp, i, rho);
         spx_eval_row(lp, rho, ai);
         dvec_i = (refsp[indx[i]] ? 1.0 : 0.0);
         for (j = 1; j <= n; j++)
            if (refsp[indx[m+j]]) dvec_i += ai[j] * ai[j];
         /* compute absolute error in dvec[i] */
         d = fabs(dvec_i - dvec[i]);
         if (dmax < d) dmax = d;
      }
      return dmax;
}

/* eof */
