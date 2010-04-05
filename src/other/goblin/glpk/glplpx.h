/* glplpx.h */

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

#ifndef _GLPLPX_H
#define _GLPLPX_H

#include "glpinv.h"
#include "glpspm.h"
#include "glpstr.h"

#define lpx_create_prob       glp_lpx_create_prob
#define lpx_realloc_prob      glp_lpx_realloc_prob
#define lpx_add_rows          glp_lpx_add_rows
#define lpx_add_cols          glp_lpx_add_cols
#define lpx_check_name        glp_lpx_check_name
#define lpx_set_prob_name     glp_lpx_set_prob_name
#define lpx_set_row_name      glp_lpx_set_row_name
#define lpx_set_col_name      glp_lpx_set_col_name
#define lpx_set_row_bnds      glp_lpx_set_row_bnds
#define lpx_set_col_bnds      glp_lpx_set_col_bnds
#define lpx_set_class         glp_lpx_set_class
#define lpx_set_col_kind      glp_lpx_set_col_kind
#define lpx_set_obj_name      glp_lpx_set_obj_name
#define lpx_set_obj_dir       glp_lpx_set_obj_dir
#define lpx_set_obj_c0        glp_lpx_set_obj_c0
#define lpx_set_row_coef      glp_lpx_set_row_coef
#define lpx_set_col_coef      glp_lpx_set_col_coef
#define lpx_set_row_stat      glp_lpx_set_row_stat
#define lpx_set_col_stat      glp_lpx_set_col_stat
#define lpx_load_mat          glp_lpx_load_mat
#define lpx_load_mat3         glp_lpx_load_mat3
#define lpx_set_mat_row       glp_lpx_set_mat_row
#define lpx_set_mat_col       glp_lpx_set_mat_col
#define lpx_unmark_all        glp_lpx_unmark_all
#define lpx_mark_row          glp_lpx_mark_row
#define lpx_mark_col          glp_lpx_mark_col
#define lpx_clear_mat         glp_lpx_clear_mat
#define lpx_del_items         glp_lpx_del_items
#define lpx_delete_prob       glp_lpx_delete_prob

#define lpx_get_num_rows      glp_lpx_get_num_rows
#define lpx_get_num_cols      glp_lpx_get_num_cols
#define lpx_get_num_int       glp_lpx_get_num_int
#define lpx_get_num_bin       glp_lpx_get_num_bin
#define lpx_get_num_nz        glp_lpx_get_num_nz
#define lpx_get_prob_name     glp_lpx_get_prob_name
#define lpx_get_row_name      glp_lpx_get_row_name
#define lpx_get_col_name      glp_lpx_get_col_name
#define lpx_get_row_bnds      glp_lpx_get_row_bnds
#define lpx_get_col_bnds      glp_lpx_get_col_bnds
#define lpx_get_class         glp_lpx_get_class
#define lpx_get_col_kind      glp_lpx_get_col_kind
#define lpx_get_obj_name      glp_lpx_get_obj_name
#define lpx_get_obj_dir       glp_lpx_get_obj_dir
#define lpx_get_obj_c0        glp_lpx_get_obj_c0
#define lpx_get_row_coef      glp_lpx_get_row_coef
#define lpx_get_col_coef      glp_lpx_get_col_coef
#define lpx_get_mat_row       glp_lpx_get_mat_row
#define lpx_get_mat_col       glp_lpx_get_mat_col
#define lpx_get_row_mark      glp_lpx_get_row_mark
#define lpx_get_col_mark      glp_lpx_get_col_mark
#define lpx_get_status        glp_lpx_get_status
#define lpx_get_prim_stat     glp_lpx_get_prim_stat
#define lpx_get_dual_stat     glp_lpx_get_dual_stat
#define lpx_get_row_info      glp_lpx_get_row_info
#define lpx_get_col_info      glp_lpx_get_col_info
#define lpx_get_obj_val       glp_lpx_get_obj_val
#define lpx_get_ips_stat      glp_lpx_get_ips_stat
#define lpx_get_ips_row       glp_lpx_get_ips_row
#define lpx_get_ips_col       glp_lpx_get_ips_col
#define lpx_get_ips_obj       glp_lpx_get_ips_obj
#define lpx_get_mip_stat      glp_lpx_get_mip_stat
#define lpx_get_mip_row       glp_lpx_get_mip_row
#define lpx_get_mip_col       glp_lpx_get_mip_col
#define lpx_get_mip_obj       glp_lpx_get_mip_obj

#define lpx_reset_parms       glp_lpx_reset_parms
#define lpx_set_int_parm      glp_lpx_set_int_parm
#define lpx_get_int_parm      glp_lpx_get_int_parm
#define lpx_set_real_parm     glp_lpx_set_real_parm
#define lpx_get_real_parm     glp_lpx_get_real_parm

#define lpx_scale_prob        glp_lpx_scale_prob
#define lpx_unscale_prob      glp_lpx_unscale_prob

#define lpx_std_basis         glp_lpx_std_basis
#define lpx_adv_basis         glp_lpx_adv_basis

#define lpx_warm_up           glp_lpx_warm_up
#define lpx_prim_opt          glp_lpx_prim_opt
#define lpx_prim_art          glp_lpx_prim_art
#define lpx_dual_opt          glp_lpx_dual_opt
#define lpx_simplex           glp_lpx_simplex
#define lpx_check_kkt         glp_lpx_check_kkt
#define lpx_interior          glp_lpx_interior
#define lpx_integer           glp_lpx_integer

#define lpx_eval_tab_row      glp_lpx_eval_tab_row
#define lpx_eval_tab_col      glp_lpx_eval_tab_col
#define lpx_transform_row     glp_lpx_transform_row
#define lpx_transform_col     glp_lpx_transform_col
#define lpx_prim_ratio_test   glp_lpx_prim_ratio_test
#define lpx_dual_ratio_test   glp_lpx_dual_ratio_test
#define lpx_eval_activity     glp_lpx_eval_activity
#define lpx_eval_red_cost     glp_lpx_eval_red_cost
#define lpx_reduce_form       glp_lpx_reduce_form
#define lpx_mixed_gomory      glp_lpx_mixed_gomory
#define lpx_estim_obj_chg     glp_lpx_estim_obj_chg

#define lpx_read_mps          glp_lpx_read_mps
#define lpx_write_mps         glp_lpx_write_mps
#define lpx_read_bas          glp_lpx_read_bas
#define lpx_write_bas         glp_lpx_write_bas
#define lpx_print_prob        glp_lpx_print_prob
#define lpx_print_sol         glp_lpx_print_sol
#define lpx_print_ips         glp_lpx_print_ips
#define lpx_print_mip         glp_lpx_print_mip

#define lpx_read_lpt          glp_lpx_read_lpt
#define lpx_write_lpt         glp_lpx_write_lpt

#define lpx_read_model        glp_lpx_read_model

/*----------------------------------------------------------------------
-- The structure LPX defines LP/MIP problem object, which includes the
-- following information:
--
--    LP/MIP problem data;
--    current basis;
--    factorization of the basis matrix;
--    LP/MIP solution components;
--    control parameters and statistics.
--
-- LP problem has the following formulation:
--
--    minimize (or maximize)
--
--       Z = c[1]*x[1] + c[2]*x[2] + ... + c[m+n]*x[m+n] + c[0]      (1)
--
--    subject to linear constraints
--
--       x[1] = a[1,1]*x[m+1] + a[1,2]*x[m+1] + ... + a[1,n]*x[m+n]
--       x[2] = a[2,1]*x[m+1] + a[2,2]*x[m+1] + ... + a[2,n]*x[m+n]  (2)
--                . . . . . .
--       x[m] = a[m,1]*x[m+1] + a[m,2]*x[m+1] + ... + a[m,n]*x[m+n]
--
--    and bounds of variables
--
--         l[1] <= x[1]   <= u[1]
--         l[2] <= x[2]   <= u[2]                                    (3)
--             . . . . . .
--       l[m+n] <= x[m+n] <= u[m+n]
--
-- where:
-- x[1], ..., x[m]      - rows (auxiliary variables);
-- x[m+1], ..., x[m+n]  - columns (structural variables);
-- Z                    - objective function;
-- c[1], ..., c[m+n]    - coefficients of the objective function;
-- c[0]                 - constant term of the objective function;
-- a[1,1], ..., a[m,n]  - constraint coefficients;
-- l[1], ..., l[m+n]    - lower bounds of variables;
-- u[1], ..., u[m+n]    - upper bounds of variables.
--
-- Using vector-matrix notations the LP problem (1)-(3) can be written
-- as follows:
--
--    minimize (or maximize)
--
--       Z = c * x + c[0]                                            (4)
--
--    subject to linear constraints
--
--       xR = A * xS                                                 (5)
--
--    and bounds of variables
--
--       l <= x <= u                                                 (6)
--
-- where:
-- xR                   - subvector of auxiliary variables (rows);
-- xS                   - subvector of structural variables (columns);
-- x = (xR, xS)         - vector of all variables;
-- c                    - vector of objective coefficients;
-- A                    - constraint matrix (has m rows and n columns);
-- l                    - vector of lower bounds of variables;
-- u                    - vector of upper bounds of variables.
--
-- The system of constraints (5) can be written in homogeneous form as
-- follows:
--
--    A~ * x = 0,                                                    (7)
--
-- where
--
--    A~ = (I | -A)                                                  (8)
--
-- is an augmented constraint matrix (has m rows and m+n columns), I is
-- the unity matrix of the order m. Note that in the structure LPX only
-- the original constraint matrix A is explicitly stored.
--
-- The current basis is defined by partitioning columns of the matrix
-- A~ into basic and non-basic ones, in which case the system (7) can
-- be written as
--
--    B * xB + N * xN = 0,                                           (9)
--
-- where B is a square non-sigular mxm matrix built of basic columns
-- and called the basis matrix, N is a mxn matrix built of non-basic
-- columns, xB is a subvector of basic variables, xN is a subvector of
-- non-basic variables.
--
-- Using the partitioning (9) the LP problem (4)-(6) can be written in
-- a form, which defines components of the corresponding basic solution
-- and is called the simplex table:
--
--    Z = d * xN + c[0]                                             (10)
--
--    xB = A~ * xN                                                  (11)
--
--    lB <= xB <= uB                                                (12)
--
--    lN <= xN <= uN                                                (13)
--
-- where:
--
--    A~ = (alfa[i,j]) = - inv(B) * N                               (14)
--
-- is the mxn matrix of influence coefficients;
--
--    d = (d[j]) = cN - N' * pi                                     (15)
--
-- is the vector of reduced costs of non-basic variables; and
--
--    pi = (pi[i]) = inv(B') * cB                                   (16)
--
-- is the vector of simplex (Lagrange) multipliers, which correspond to
-- the equiality constraints (5).
--
-- Note that signs of the reduced costs d are determined by the formula
-- (15) in both cases of minimization and maximization.
--
-- The structure LPX allows scaling the problem. In the scaled problem
-- the constraint matrix is scaled and has the form
--
--    A" = R * A * S,                                               (17)
--
-- where A is the constraint matrix of the original (unscaled) problem,
-- R and S are, respectively, diagonal scaling mxm and nxn matrices with
-- positive diagonal elements used to scale rows and columns of A.
--
-- Actually *all* components of the LP problem stored in the structure
-- LPX are in the scaled form (for example, the constraint matrix stored
-- in LPX is A" = R*A*S, not A), and if the scaling is not used, it just
-- means that the matrices R and S are unity. Each time when something
-- is entered into LPX (for example, bounds of variables or constraint
-- coefficients), it is automatically scaled and then stored, and vice
-- versa, if something is obtained from LPX (for example, components of
-- the current basic solution), it is automatically unscaled. Thus, on
-- API level the scaling process is invisible.
--
-- The connection between the original and scaled components is defined
-- by (17) and expressed by the following formulae:
--
--    cR" = inv(R) * cR    (obj. coefficients at auxiliary variables)
--
--    cS" = S * cS         (obj. coefficients at structural variables)
--
--    xR" = R * xR         (values of auxiliary variables)
--    lR" = R * lR         (lower bounds of auxiliary variables)
--    uR" = R * uR         (upper bounds of auxiliary variables)
--
--    xS" = inv(S) * xS    (values of structural variables)
--    lS" = inv(S) * lS    (lower bounds of structural variables)
--    uS" = inv(S) * uS    (upper bounds of structural variables)
--
--    A"  = R * A * S      (constraint matrix)
--
-- Note that substitution scaled components into (4)-(6) gives the same
-- LP problem. */

typedef struct LPX LPX;

/* CAUTION: DO NOT CHANGE ANY MEMBERS OF THIS STRUCTURE DIRECTLY; THIS
   MAY DAMAGE ITS INTEGRITY AND CAUSE UNPREDICTABLE ERRORS! */

struct LPX
{     /* LP/MIP problem object */
      int m_max;
      /* maximal number of rows (if necessary, this parameter can be
         automatically changed) */
      int n_max;
      /* maximal number of columns (if necessary, this parameter can be
         automatically changed) */
      int m;
      /* current number of rows (auxiliary variables) */
      int n;
      /* current number of columns (structural variables) */
      DMP *pool;
      /* memory pool for segmented character strings */
      char *buf; /* char buf[255+1]; */
      /* working buffer used to return character strings */
      /*--------------------------------------------------------------*/
      /* LP problem data */
      STR *prob;
      /* symbolic name of the LP problem object */
      int clss;
      /* problem class: */
#define LPX_LP          100   /* linear programming (LP) */
#define LPX_MIP         101   /* mixed integer programming (MIP) */
      STR **name; /* STR *name[1+m+n]; */
      /* name[0] is not used;
         name[k], 1 <= k <= m+n, is a name of the variable x[k] */
      int *typx; /* int typx[1+m_max+n_max]; */
      /* typx[0] is not used;
         typx[k], 1 <= k <= m+n, is the type of the variable x[k]: */
#define LPX_FR          110   /* free variable:  -inf <  x[k] < +inf  */
#define LPX_LO          111   /* lower bound:    l[k] <= x[k] < +inf  */
#define LPX_UP          112   /* upper bound:    -inf <  x[k] <= u[k] */
#define LPX_DB          113   /* double bound:   l[k] <= x[k] <= u[k] */
#define LPX_FX          114   /* fixed variable: l[k]  = x[k]  = u[k] */
      double *lb; /* double lb[1+m_max+n_max]; */
      /* lb[0] is not used;
         lb[k], 1 <= k <= m+n, is an lower bound of the variable x[k];
         if x[k] has no lower bound, lb[k] is zero */
      double *ub; /* double ub[1+m_max+n_max]; */
      /* ub[0] is not used;
         ub[k], 1 <= k <= m+n, is an upper bound of the variable x[k];
         if x[k] has no upper bound, ub[k] is zero; if x[k] is of fixed
         type, ub[k] is equal to lb[k] */
      double *rs; /* double rs[1+m_max+n_max]; */
      /* rs[0] is not used;
         rs[i], 1 <= i <= m, is diagonal element r[i,i] of the row
         scaling matrix R (see (17));
         rs[m+j], 1 <= j <= n, is diagonal element s[j,j] of the column
         scaling matrix S (see (17)) */
      int *mark; /* int mark[1+m+max+n_max]; */
      /* mark[0] is not used;
         mark[k], 1 <= k <= m+n, is a marker of the variable x[k];
         these markers are used to mark rows and columns for various
         purposes */
      STR *obj;
      /* symbolic name of the objective function */
      int dir;
      /* optimization direction (sense of the objective function): */
#define LPX_MIN         120   /* minimization */
#define LPX_MAX         121   /* maximization */
      double *coef; /* double coef[1+m_max+n_max]; */
      /* coef[0] is a constant term of the objective function;
         coef[k], 1 <= k <= m+n, is a coefficient of the objective
         function at the variable x[k] (note that auxiliary variables
         also may have non-zero objective coefficients) */
      SPM *A; /* SPM A[1:m,1:n]; */
      /* constraint matrix (has m rows and n columns) */
      /*--------------------------------------------------------------*/
      /* basic solution segment */
      int b_stat;
      /* status of the current basis: */
#define LPX_B_UNDEF     130   /* current basis is undefined */
#define LPX_B_VALID     131   /* current basis is valid */
      int p_stat;
      /* status of the primal solution: */
#define LPX_P_UNDEF     132   /* primal status is undefined */
#define LPX_P_FEAS      133   /* solution is primal feasible */
#define LPX_P_INFEAS    134   /* solution is primal infeasible */
#define LPX_P_NOFEAS    135   /* no primal feasible solution exists */
      int d_stat;
      /* status of the dual solution: */
#define LPX_D_UNDEF     136   /* dual status is undefined */
#define LPX_D_FEAS      137   /* solution is dual feasible */
#define LPX_D_INFEAS    138   /* solution is dual infeasible */
#define LPX_D_NOFEAS    139   /* no dual feasible solution exists */
      int *tagx; /* int tagx[1+m_max+n_max]; */
      /* tagx[0] is not used;
         tagx[k], 1 <= k <= m+n, is the status of the variable x[k]
         (contents of this array is always defined independently on the
         status of the current basis): */
#define LPX_BS          140   /* basic variable */
#define LPX_NL          141   /* non-basic variable on lower bound */
#define LPX_NU          142   /* non-basic variable on upper bound */
#define LPX_NF          143   /* non-basic free variable */
#define LPX_NS          144   /* non-basic fixed variable */
      int *posx; /* int posx[1+m_max+n_max]; */
      /* posx[0] is not used;
         posx[k], 1 <= k <= m+n, is the position of the variable x[k]
         in the vector of basic variables xB or non-basic variables xN:
         posx[k] = i   means that x[k] = xB[i], 1 <= i <= m
         posx[k] = m+j means that x[k] = xN[j], 1 <= j <= n
         (if the current basis is undefined, contents of this array is
         undefined) */
      int *indx; /* int indx[1+m_max+n_max]; */
      /* indx[0] is not used;
         indx[i], 1 <= i <= m, is the original number of the basic
         variable xB[i], i.e. indx[i] = k means that posx[k] = i
         indx[m+j], 1 <= j <= n, is the original number of the non-basic
         variable xN[j], i.e. indx[m+j] = k means that posx[k] = m+j
         (if the current basis is undefined, contents of this array is
         undefined) */
      INV *inv; /* INV inv[1:m,1:m]; */
      /* an invertable (factorized) form of the current basis matrix */
      double *bbar; /* double bbar[1+m_max]; */
      /* bbar[0] is not used;
         bbar[i], 1 <= i <= m, is a value of basic variable xB[i] */
      double *pi; /* double pi[1+m_max]; */
      /* pi[0] is not used;
         pi[i], 1 <= i <= m, is a simplex (Lagrange) multiplier, which
         corresponds to the i-th row (equality constraint) */
      double *cbar; /* double cbar[1+n_max]; */
      /* cbar[0] is not used;
         cbar[j], 1 <= j <= n, is a reduced cost of non-basic variable
         xN[j] */
      /*--------------------------------------------------------------*/
      /* interior point solution segment */
      int t_stat;
      /* status of the interior point solution: */
#define LPX_T_UNDEF     150   /* solution is undefined */
#define LPX_T_OPT       151   /* solution is optimal */
      double *pv; /* double pv[1+m_max+n_max]; */
      /* pv[0] is not used;
         pv[k], 1 <= k <= m+n, is a primal value of the variable x[k];
         if t_stat = LPX_T_UNDEF, this array may be unallocated */
      double *dv; /* double dv[1+m_max+n_max]; */
      /* dv[0] is not used;
         dv[k], 1 <= k <= m+n, is a dual value of the variable x[k];
         if t_stat = LPX_T_UNDEF, this array may be unallocated */
      /*--------------------------------------------------------------*/
      /* mixed integer programming (MIP) segment */
      int *kind; /* int kind[1+n_max]; */
      /* kind[0] is not used;
         kind[j], 1 <= j <= n, specifies the kind of the j-th structural
         variable: */
#define LPX_CV          160   /* continuous variable */
#define LPX_IV          161   /* integer variable */
      int i_stat;
      /* status of the integer solution: */
#define LPX_I_UNDEF     170   /* integer status is undefined */
#define LPX_I_OPT       171   /* solution is integer optimal */
#define LPX_I_FEAS      172   /* solution is integer feasible */
#define LPX_I_NOFEAS    173   /* no integer solution exists */
      double *mipx; /* double mipx[1+m_max+n_max]; */
      /* mipx[0] is not used;
         mipx[k], 1 <= k <= m+n, is an (unscaled!) value of the variable
         x[k] for integer optimal or integer feasible solution */
      /*--------------------------------------------------------------*/
      /* control parameters and statistics */
      int msg_lev;
      /* level of messages output by the solver:
         0 - no output
         1 - error messages only
         2 - normal output
         3 - full output (includes informational messages) */
      int scale;
      /* scaling option:
         0 - no scaling
         1 - equilibration scaling
         2 - geometric mean scaling
         3 - geometric mean scaling, then equilibration scaling */
      int sc_ord;
      /* scaling order:
         0 - at first rows, then columns
         1 - at first columns, then rows */
      int sc_max;
      /* maximal number of iterations used in geometric mean scaling */
      double sc_eps;
      /* stopping criterion used in geometric mean scaling */
      int dual;
      /* dual simplex option:
         0 - do not use the dual simplex
         1 - if the initial basic solution being primal infeasible is
             dual feasible, use the dual simplex */
      int price;
      /* pricing option (for both primal and dual simplex):
         0 - textbook pricing
         1 - steepest edge pricing */
      double relax;
      /* relaxation parameter used in the ratio test; if it is zero,
         the textbook ratio test is used; if it is non-zero (should be
         positive), Harris' two-pass ratio test is used; in the latter
         case on the first pass basic variables (in the case of primal
         simplex) or reduced costs of non-basic variables (in the case
         of dual simplex) are allowed to slightly violate their bounds,
         but not more than (relax * tol_bnd) or (relax * tol_dj) (thus,
         relax is a percentage of tol_bnd or tol_dj) */
      double tol_bnd;
      /* relative tolerance used to check if the current basic solution
         is primal feasible */
      double tol_dj;
      /* absolute tolerance used to check if the current basic solution
         is dual feasible */
      double tol_piv;
      /* relative tolerance used to choose eligible pivotal elements of
         the simplex table in the ratio test */
      int round;
      /* solution rounding option:
         0 - report all computed values and reduced costs "as is"
         1 - if possible (allowed by the tolerances), replace computed
             values and reduced costs which are close to zero by exact
             zeros */
      double obj_ll;
      /* lower limit of the objective function; if on the phase II the
         objective function reaches this limit and continues decreasing,
         the solver stops the search */
      double obj_ul;
      /* upper limit of the objective function; if on the phase II the
         objective function reaches this limit and continues increasing,
         the solver stops the search */
      int it_lim;
      /* simplex iterations limit; if this value is positive, it is
         decreased by one each time when one simplex iteration has been
         performed, and reaching zero value signals the solver to stop
         the search; negative value means no iterations limit */
      int it_cnt;
      /* simplex iterations count; this count is increased by one each
         time when one simplex iteration has been performed */
      double tm_lim;
      /* searching time limit, in seconds; if this value is positive,
         it is decreased each time when one simplex iteration has been
         performed by the amount of time spent for the iteration, and
         reaching zero value signals the solver to stop the search;
         negative value means no time limit */
      int out_frq;
      /* output frequency, in iterations; this parameter specifies how
         frequently the solver sends information about the solution to
         the standard output */
      double out_dly;
      /* output delay, in seconds; this parameter specifies how long
         the solver should delay sending information about the solution
         to the standard output; zero value means no delay */
      int branch; /* MIP */
      /* branching heuristic:
         0 - branch on the first variable
         1 - branch on the last variable
         2 - branch using a heuristic by Driebeck and Tomlin */
      int btrack; /* MIP */
      /* backtracking heuristic:
         0 - depth first search
         1 - breadth first search
         2 - backtrack using the best projection heuristic */
      double tol_int; /* MIP */
      /* relative tolerance used to check if the current basic solution
         is integer feasible */
      double tol_obj; /* MIP */
      /* relative tolerance used to check if the value of the objective
         function is not better than in the best known integer feasible
         solution */
      int mps_info; /* lpx_write_mps */
      /* if this flag is set, the routine lpx_write_mps outputs several
         comment cards that contains some information about the problem;
         otherwise the routine outputs no comment cards */
      int mps_obj; /* lpx_write_mps */
      /* this parameter tells the routine lpx_write_mps how to output
         the objective function row:
         0 - never output objective function row
         1 - always output objective function row
         2 - output objective function row if and only if the problem
             has no free rows */
      int mps_orig; /* lpx_write_mps */
      /* if this flag is set, the routine lpx_write_mps uses original
         row and column symbolic names; otherwise the routine generates
         plain names using ordinal numbers of rows and columns */
      int mps_wide; /* lpx_write_mps */
      /* if this flag is set, the routine lpx_write_mps uses all data
         fields; otherwise the routine keeps fields 5 and 6 empty */
      int mps_free; /* lpx_write_mps */
      /* if this flag is set, the routine lpx_write_mps omits column
         and vector names everytime if possible (free style); otherwise
         the routine never omits these names (pedantic style) */
      int mps_skip; /* lpx_write_mps */
      /* if this flag is set, the routine lpx_write_mps skips empty
         columns (i.e. which has no constraint coefficients); otherwise
         the routine outputs all columns */
      int lpt_orig; /* lpx_write_lpt */
      /* if this flag is set, the routine lpx_write_lpt uses original
         row and column symbolic names; otherwise the routine generates
         plain names using ordinal numbers of rows and columns */
      int presol; /* lpx_simplex */
      /* LP presolver option:
         0 - do not use LP presolver
         1 - use LP presolver */
};

/* status codes reported by the routine lpx_get_status: */
#define LPX_OPT         180   /* optimal */
#define LPX_FEAS        181   /* feasible */
#define LPX_INFEAS      182   /* infeasible */
#define LPX_NOFEAS      183   /* no feasible */
#define LPX_UNBND       184   /* unbounded */
#define LPX_UNDEF       185   /* undefined */

/* exit codes returned by the simplex-based solver routines: */
#define LPX_E_OK        200   /* success */
#define LPX_E_EMPTY     201   /* empty problem */
#define LPX_E_BADB      202   /* invalid initial basis */
#define LPX_E_INFEAS    203   /* infeasible initial solution */
#define LPX_E_FAULT     204   /* unable to start the search */
#define LPX_E_OBJLL     205   /* objective lower limit reached */
#define LPX_E_OBJUL     206   /* objective upper limit reached */
#define LPX_E_ITLIM     207   /* iterations limit exhausted */
#define LPX_E_TMLIM     208   /* time limit exhausted */
#define LPX_E_NOFEAS    209   /* no feasible solution */
#define LPX_E_INSTAB    210   /* numerical instability */
#define LPX_E_SING      211   /* problems with basis matrix */
#define LPX_E_NOCONV    212   /* no convergence (interior) */
#define LPX_E_NOPFS     213   /* no primal feas. sol. (LP presolver) */
#define LPX_E_NODFS     214   /* no dual feas. sol.   (LP presolver) */

/* control parameter identifiers: */
#define LPX_K_MSGLEV    300   /* lp->msg_lev */
#define LPX_K_SCALE     301   /* lp->scale */
#define LPX_K_DUAL      302   /* lp->dual */
#define LPX_K_PRICE     303   /* lp->price */
#define LPX_K_RELAX     304   /* lp->relax */
#define LPX_K_TOLBND    305   /* lp->tol_bnd */
#define LPX_K_TOLDJ     306   /* lp->tol_dj */
#define LPX_K_TOLPIV    307   /* lp->tol_piv */
#define LPX_K_ROUND     308   /* lp->round */
#define LPX_K_OBJLL     309   /* lp->obj_ll */
#define LPX_K_OBJUL     310   /* lp->obj_ul */
#define LPX_K_ITLIM     311   /* lp->it_lim */
#define LPX_K_ITCNT     312   /* lp->it_cnt */
#define LPX_K_TMLIM     313   /* lp->tm_lim */
#define LPX_K_OUTFRQ    314   /* lp->out_frq */
#define LPX_K_OUTDLY    315   /* lp->out_dly */
#define LPX_K_BRANCH    316   /* lp->branch */
#define LPX_K_BTRACK    317   /* lp->btrack */
#define LPX_K_TOLINT    318   /* lp->tol_int */
#define LPX_K_TOLOBJ    319   /* lp->tol_obj */
#define LPX_K_MPSINFO   320   /* lp->mps_info */
#define LPX_K_MPSOBJ    321   /* lp->mps_obj */
#define LPX_K_MPSORIG   322   /* lp->mps_orig */
#define LPX_K_MPSWIDE   323   /* lp->mps_wide */
#define LPX_K_MPSFREE   324   /* lp->mps_free */
#define LPX_K_MPSSKIP   325   /* lp->mps_skip */
#define LPX_K_LPTORIG   326   /* lp->lpt_orig */
#define LPX_K_PRESOL    327   /* lp->presol */

typedef struct LPXKKT LPXKKT;

struct LPXKKT
{     /* this structure contains results provided by the routines that
         checks Karush-Kuhn-Tucker conditions (for details see comments
         to those routines) */
      /*--------------------------------------------------------------*/
      /* xR - A * xS = 0 (KKT.PE) */
      double pe_ae_max;
      /* largest absolute error */
      int    pe_ae_row;
      /* number of row with largest absolute error */
      double pe_re_max;
      /* largest relative error */
      int    pe_re_row;
      /* number of row with largest relative error */
      int    pe_quality;
      /* quality of primal solution:
         'H' - high
         'M' - medium
         'L' - low
         '?' - primal solution is wrong */
      /*--------------------------------------------------------------*/
      /* l[k] <= x[k] <= u[k] (KKT.PB) */
      double pb_ae_max;
      /* largest absolute error */
      int    pb_ae_ind;
      /* number of variable with largest absolute error */
      double pb_re_max;
      /* largest relative error */
      int    pb_re_ind;
      /* number of variable with largest relative error */
      int    pb_quality;
      /* quality of primal feasibility:
         'H' - high
         'M' - medium
         'L' - low
         '?' - primal solution is infeasible */
      /*--------------------------------------------------------------*/
      /* A' * (dR - cR) + (dS - cS) = 0 (KKT.DE) */
      double de_ae_max;
      /* largest absolute error */
      int    de_ae_col;
      /* number of column with largest absolute error */
      double de_re_max;
      /* largest relative error */
      int    de_re_col;
      /* number of column with largest relative error */
      int    de_quality;
      /* quality of dual solution:
         'H' - high
         'M' - medium
         'L' - low
         '?' - dual solution is wrong */
      /*--------------------------------------------------------------*/
      /* d[k] >= 0 or d[k] <= 0 (KKT.DB) */
      double db_ae_max;
      /* largest absolute error */
      int    db_ae_ind;
      /* number of variable with largest absolute error */
      double db_re_max;
      /* largest relative error */
      int    db_re_ind;
      /* number of variable with largest relative error */
      int    db_quality;
      /* quality of dual feasibility:
         'H' - high
         'M' - medium
         'L' - low
         '?' - dual solution is infeasible */
      /*--------------------------------------------------------------*/
      /* (x[k] - bound of x[k]) * d[k] = 0 (KKT.CS) */
      double cs_ae_max;
      /* largest absolute error */
      int    cs_ae_ind;
      /* number of variable with largest absolute error */
      double cs_re_max;
      /* largest relative error */
      int    cs_re_ind;
      /* number of variable with largest relative error */
      int    cs_quality;
      /* quality of complementary slackness:
         'H' - high
         'M' - medium
         'L' - low
         '?' - primal and dual solutions are not complementary */
};

/* problem creating and modifying routines ---------------------------*/

LPX *lpx_create_prob(void);
/* create LP problem object */

void lpx_realloc_prob(LPX *lp, int m_max, int n_max);
/* reallocate LP problem object */

void lpx_add_rows(LPX *lp, int nrs);
/* add new rows to LP problem object */

void lpx_add_cols(LPX *lp, int ncs);
/* add new columns to LP problem object */

int lpx_check_name(char *name);
/* check correctness of symbolic name */

void lpx_set_prob_name(LPX *lp, char *name);
/* assign (change) problem name */

void lpx_set_row_name(LPX *lp, int i, char *name);
/* assign (change) row name */

void lpx_set_col_name(LPX *lp, int j, char *name);
/* assign (change) column name */

void lpx_set_row_bnds(LPX *lp, int i, int typx, double lb, double ub);
/* set (change) row bounds */

void lpx_set_col_bnds(LPX *lp, int j, int typx, double lb, double ub);
/* set (change) column bounds */

void lpx_set_class(LPX *lp, int clss);
/* set (change) problem class */

void lpx_set_col_kind(LPX *lp, int j, int kind);
/* set (change) column kind */

void lpx_set_obj_name(LPX *lp, char *name);
/* assign (change) objective function name */

void lpx_set_obj_dir(LPX *lp, int dir);
/* set (change) optimization direction */

void lpx_set_obj_c0(LPX *lp, double c0);
/* set (change) constant term of the obj. function */

void lpx_set_row_coef(LPX *lp, int i, double coef);
/* set (change) row objective coefficient */

void lpx_set_col_coef(LPX *lp, int j, double coef);
/* set (change) column objective coefficient */

void lpx_set_row_stat(LPX *lp, int i, int stat);
/* set (change) row status */

void lpx_set_col_stat(LPX *lp, int j, int stat);
/* set (change) column status */

void lpx_load_mat(LPX *lp,
      void *info, double (*mat)(void *info, int *i, int *j));
/* load constraint matrix */

void lpx_load_mat3(LPX *lp, int nz, int rn[], int cn[], double a[]);
/* load constraint matrix */

void lpx_set_mat_row(LPX *lp, int i, int len, int ndx[], double val[]);
/* set (replace) row of the constraint matrix */

void lpx_set_mat_col(LPX *lp, int j, int len, int ndx[], double val[]);
/* set (replace) column of the constraint matrix */

void lpx_unmark_all(LPX *lp);
/* unmark all rows and columns */

void lpx_mark_row(LPX *lp, int i, int mark);
/* assign mark to row */

void lpx_mark_col(LPX *lp, int j, int mark);
/* assign mark to column */

void lpx_clear_mat(LPX *lp);
/* clear rows and columns of the constraint matrix */

void lpx_del_items(LPX *lp);
/* remove rows and columns from LP problem object */

void lpx_delete_prob(LPX *lp);
/* delete LP problem object */

/* problem querying routines -----------------------------------------*/

int lpx_get_num_rows(LPX *lp);
/* determine number of rows */

int lpx_get_num_cols(LPX *lp);
/* determine number of columns */

int lpx_get_num_int(LPX *lp);
/* determine number of integer columns */

int lpx_get_num_bin(LPX *lp);
/* determine number of binary columns */

int lpx_get_num_nz(LPX *lp);
/* determine number of constraint coefficients */

char *lpx_get_prob_name(LPX *lp);
/* obtain problem name */

char *lpx_get_row_name(LPX *lp, int i);
/* obtain row name */

char *lpx_get_col_name(LPX *lp, int j);
/* obtain column name */

void lpx_get_row_bnds(LPX *lp, int i, int *typx, double *lb,
      double *ub);
/* obtain row bounds */

void lpx_get_col_bnds(LPX *lp, int j, int *typx, double *lb,
      double *ub);
/* obtain column bounds */

int lpx_get_class(LPX *lp);
/* query problem class */

int lpx_get_col_kind(LPX *lp, int j);
/* query column kind */

char *lpx_get_obj_name(LPX *lp);
/* obtain objective function name */

int lpx_get_obj_dir(LPX *lp);
/* determine optimization direction */

double lpx_get_obj_c0(LPX *lp);
/* obtain constant term of the obj. function */

double lpx_get_row_coef(LPX *lp, int i);
/* obtain row objective coefficient */

double lpx_get_col_coef(LPX *lp, int j);
/* obtain column objective coefficient */

int lpx_get_mat_row(LPX *lp, int i, int ndx[], double val[]);
/* get row of the constraint matrix */

int lpx_get_mat_col(LPX *lp, int j, int ndx[], double val[]);
/* get column of the constraint matrix */

int lpx_get_row_mark(LPX *lp, int i);
/* determine row mark */

int lpx_get_col_mark(LPX *lp, int j);
/* determine column mark */

int lpx_get_status(LPX *lp);
/* query basic solution status */

int lpx_get_prim_stat(LPX *lp);
/* query primal status of basic solution */

int lpx_get_dual_stat(LPX *lp);
/* query dual status of basic solution */

void lpx_get_row_info(LPX *lp, int i, int *tagx, double *vx, double *dx)
      ;
/* obtain row solution information */

void lpx_get_col_info(LPX *lp, int j, int *tagx, double *vx, double *dx)
      ;
/* obtain column solution information */

double lpx_get_obj_val(LPX *lp);
/* obtain value of the objective function */

int lpx_get_ips_stat(LPX *lp);
/* query status of interior point solution */

void lpx_get_ips_row(LPX *lp, int i, double *vx, double *dx);
/* obtain row interior point solution */

void lpx_get_ips_col(LPX *lp, int j, double *vx, double *dx);
/* obtain column interior point solution */

double lpx_get_ips_obj(LPX *lp);
/* obtain interior point value of the obj. function */

int lpx_get_mip_stat(LPX *lp);
/* query status of MIP solution */

double lpx_get_mip_row(LPX *lp, int i);
/* obtain row activity for MIP solution */

double lpx_get_mip_col(LPX *lp, int j);
/* obtain column activity for MIP solution */

double lpx_get_mip_obj(LPX *lp);
/* obtain value of the obj. func. for MIP solution */

/* control parameters and statistics routines ------------------------*/

void lpx_reset_parms(LPX *lp);
/* reset control parameters to default values */

void lpx_set_int_parm(LPX *lp, int parm, int val);
/* set (change) integer control parameter */

int lpx_get_int_parm(LPX *lp, int parm);
/* query integer control parameter */

void lpx_set_real_parm(LPX *lp, int parm, double val);
/* set (change) real control parameter */

double lpx_get_real_parm(LPX *lp, int parm);
/* query real control parameter */

/* problem scaling routines ------------------------------------------*/

void lpx_scale_prob(LPX *lp);
/* scale LP problem data */

void lpx_unscale_prob(LPX *lp);
/* unscale LP problem data */

/* initial basis routines --------------------------------------------*/

void lpx_std_basis(LPX *lp);
/* build standard initial basis */

void lpx_adv_basis(LPX *lp);
/* build advanced initial basis */

/* solver routines ---------------------------------------------------*/

int lpx_warm_up(LPX *lp);
/* "warm up" the initial basis */

int lpx_prim_opt(LPX *lp);
/* find optimal solution (primal simplex) */

int lpx_prim_art(LPX *lp);
/* find primal feasible solution (primal simplex) */

int lpx_dual_opt(LPX *lp);
/* find optimal solution (dual simplex) */

int lpx_simplex(LPX *lp);
/* easy-to-use driver to the simplex method */

void lpx_check_kkt(LPX *lp, int scaled, LPXKKT *kkt);
/* check Karush-Kuhn-Tucker conditions */

int lpx_interior(LPX *lp);
/* easy-to-use driver to the interior point method */

int lpx_integer(LPX *lp);
/* easy-to-use driver to the branch-and-bound method */

/* simplex table routines --------------------------------------------*/

int lpx_eval_tab_row(LPX *lp, int k, int ndx[], double val[]);
/* compute row of the simplex table */

int lpx_eval_tab_col(LPX *lp, int k, int ndx[], double val[]);
/* compute column of the simplex table */

int lpx_transform_row(LPX *lp, int len, int ndx[], double val[]);
/* transform explicitly specified row */

int lpx_transform_col(LPX *lp, int len, int ndx[], double val[]);
/* transform explicitly specified column */

int lpx_prim_ratio_test(LPX *lp, int len, int ndx[], double val[],
      int how, double tol);
/* perform primal ratio test */

int lpx_dual_ratio_test(LPX *lp, int len, int ndx[], double val[],
      int how, double tol);
/* perform dual ratio test */

double lpx_eval_activity(LPX *lp, int len, int ndx[], double val[]);
/* compute activity of explicitly specified row */

double lpx_eval_red_cost(LPX *lp, int len, int ndx[], double val[]);
/* compute red. cost of explicitly specified column */

int lpx_reduce_form(LPX *lp, int len, int ndx[], double val[],
      double work[]);
/* reduce linear form */

int lpx_mixed_gomory(LPX *lp, int kind[], int len, int ndx[],
      double val[], double work[]);
/* generate Gomory's mixed integer cut */

void lpx_estim_obj_chg(LPX *lp, int k, double dn_val, double up_val,
      double *dn_chg, double *up_chg, int ndx[], double val[]);
/* estimate obj. changes for down- and up-branch */

/* additional utility routines ---------------------------------------*/

LPX *lpx_read_mps(char *fname);
/* read in problem data using MPS format */

int lpx_write_mps(LPX *lp, char *fname);
/* write problem data using MPS format */

int lpx_read_bas(LPX *lp, char *fname);
/* read predefined basis using MPS format */

int lpx_write_bas(LPX *lp, char *fname);
/* write current basis using MPS format */

int lpx_print_prob(LPX *lp, char *fname);
/* write problem data in plain text format */

int lpx_print_sol(LPX *lp, char *fname);
/* write LP problem solution in printable format */

int lpx_print_ips(LPX *lp, char *fname);
/* write interior point solution in printable format */

int lpx_print_mip(LPX *lp, char *fname);
/* write MIP problem solution in printable format */

LPX *lpx_read_lpt(char *fname);
/* read LP/MIP problem using CPLEX LP format */

int lpx_write_lpt(LPX *lp, char *fname);
/* write problem data using CPLEX LP format */

LPX *lpx_read_model(char *model, char *data, char *output);
/* read LP/MIP model written in GNU MathProg language */

#endif

/* eof */
