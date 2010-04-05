/* glpipm.c */

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

#include <math.h>
#include <stddef.h>
#include "glpchol.h"
#include "glpipm.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- ipm1 - primal-dual interior point method for linear programming.
--
-- *Synopsis*
--
-- #include "glpipm.h"
-- int ipm1(MAT *A, double b[], double c[], double x[], double y[],
--    double z[]);
--
-- *Description*
--
-- The ipm1 routine is a *tentative* implementation of the primal-dual
-- interior point method for solving linear programming problems.
--
-- The ipm1 routine assumes the following *standard* formulation of LP
-- problem:
--
--    minimize
--
--       F = c[0] + c[1]*x[1] + c[2]*x[2] + ... + c[n]*x[n]
--
--    subject to linear constraints
--
--       a[1,1]*x[1] + a[1,2]*x[2] + ... + a[1,n]*x[n] = b[1]
--       a[2,1]*x[1] + a[2,2]*x[2] + ... + a[2,n]*x[n] = b[2]
--             . . . . . .
--       a[m,1]*x[1] + a[m,2]*x[2] + ... + a[m,n]*x[n] = b[m]
--
--    and non-negative variables
--
--       x[1] >= 0, x[2] >= 0, ..., x[n] >= 0
--
-- where:
-- F                    - objective function;
-- x[1], ..., x[n]      - (structural) variables;
-- c[0]                 - constant term of the objective function;
-- c[1], ..., c[n]      - coefficients of the objective function;
-- a[1,1], ..., a[m,n]  - constraint coefficients;
-- b[1], ..., b[n]      - right-hand sides.
--
-- The parameter A specifies the matrix of constraint coefficients. The
-- matrix A also implicitly defines the problem dimension: number of its
-- rows defines number of linear constraints (m), number of its columns
-- defines number of variables (n).
--
-- The array b specifies the vector of right-hand sides, which should be
-- placed in locations b[1], b[2], ..., b[m].
--
-- The array c specifies the vector of coefficients of the objective
-- function, which should be placed in locations c[1], c[2], ..., c[n].
-- The constant term should be placed in location c[0].
--
-- *Note* that the ipm1 routine scales the matrix A, the vector b and
-- the vector c on entry and unscales them on exit. So, these objects
-- may slightly differ from the original objects because of round-off
-- errors involved by scaling/unscaling process.
--
-- On exit the ipm1 routine computes three vectors x, y, and z, which
-- are stored in the arrays x, y, and z, respectively. These vectors
-- correspond to the best primal-dual point reached during optimization.
-- They are approximate solution of the following system (which is the
-- Karush-Kuhn-Tucker optimality conditions):
--
--    A*x      = b      (primal feasibility condition)
--    A'*y + z = c      (dual feasibility condition)
--    x'*z     = 0      (primal-dual complementarity condition)
--    x >= 0, z >= 0    (non-negativity condition)
--
-- where:
-- x[1], ..., x[n]      - primal (structural) variables;
-- y[1], ..., y[m]      - dual variables (Lagrange multipliers) for
--                        equality constraints;
-- z[1], ..., z[n]      - dual variables (Lagrange multipliers) for
--                        primal non-negativity constraints.
--
-- *Returns*
--
-- The ipm1 routine returns one of the following codes:
--
-- 0 - optimal solution found;
-- 1 - problem has no feasible (primal or dual) solution;
-- 2 - no convergence;
-- 3 - iterations limit exceeded;
-- 4 - numerical instability on solving Newtonian system.
--
-- In case of non-zero return code the routine returns the best point
-- which has been reached during optimization. */

static int iter_max = 100;
/* maximal allowed number of iterations */

static int m;
/* number of constraints */

static int n;
/* number of (structural) variables */

static MAT *A; /* MAT A[m,n]; */
/* matrix of constraint coefficients */

static double *b; /* double b[1:m]; */
/* vector of right-hand sides */

static double *c; /* double c[0:n]; */
/* vector of objective function coefficients; c[0] is the constant term
   of the objective function */

static double *x; /* double x[1:n]; */
static double *y; /* double y[1:m]; */
static double *z; /* double z[1:n]; */
/* current point in the primal-dual space; the best point on exit */

static double *R; /* double R[1:m]; */
static double *S; /* double S[1:n]; */
/* diagonal scaling matrices; the constraint matrix for the scaling
   problem is R*A*S, where A is the original constraint matrix */

static double *D; /* double D[1:n]; */
/* diagonal matrix D = X*inv(Z), where X = diag(x[j]), Z = diag(z[j]) */

static ADAT *adat;
/* Cholesky factorization of the matrix A*D*A' */

static int iter;
/* iteration number (0, 1, 2, ...); iter = 0 corresponds to the initial
   point */

static double F;
/* current value of the objective function */

static double rpi;
/* relative primal infeasibility rpi = ||A*x-b||/(1+||b||) */

static double rdi;
/* relative dual infeasibility rdi = ||A'*y+z-c||/(1+||c||) */

static double gap;
/* primal-dual gap = |c'*x-b'*y|/(1+|c'*x|); it is a relative difference
   between the primal and dual objective function values */

static double phi;
/* merit function phi = ||A*x-b||/max(1,||b||) +
                      + ||A'*y+z-c||/max(1,||c||) +
                      + |c'*x-b'*y|/max(1,||b||,||c||) */

static double mu;
/* duality measure mu = x'*z/n (used as barrier parameter) */

static double rmu;
/* rmu = max(||A*x-b||,||A'*y+z-c||)/mu */

static double rmu0;
/* the initial value of rmu (i.e. on the iteration number 0) */

static double *phi_min; /* double phi_min[1+iter_max]; */
/* phi_min[k] = min(phi[k]), where phi[k] is the value of phi on k-th
   iteration, 0 <= k <= iter */

static int iter_best;
/* number of iteration, on which the value of phi has reached its best
   (minimal) value */

static double *x_best; /* double x_best[1:n]; */
static double *y_best; /* double y_best[1:m]; */
static double *z_best; /* double z_best[1:n]; */
/* the best point (in the sense of the merit function phi), which has
   been reached on the iteration iter_best */

static double F_best;
/* the value of the objective function at the best point */

static double *dx_aff; /* double dx_aff[1:n]; */
static double *dy_aff; /* double dy_aff[1:m]; */
static double *dz_aff; /* double dz_aff[1:n]; */
/* the affine scaling direction */

static double alfa_aff_p, alfa_aff_d;
/* maximal primal and dual stepsizes in the affine scaling direction,
   on which x and z are still non-negative */

static double mu_aff;
/* duality measure mu_aff = x_aff'*z_aff/n in the boundary point
   x_aff' = x+alfa_aff_p*dx_aff, z_aff' = z+alfa_aff_d*dz_aff */

static double sigma;
/* the parameter chosen using Mehrotra's heuristic (0 <= sigma <= 1) */

static double *dx_cc; /* double dx_cc[1:n]; */
static double *dy_cc; /* double dy_cc[1:m]; */
static double *dz_cc; /* double dz_cc[1:n]; */
/* the centering corrector direction */

static double *dx; /* double dx[1:n]; */
static double *dy; /* double dy[1:m]; */
static double *dz; /* double dz[1:n]; */
/* the final combined direction dx = dx_aff+dx_cc, dy = dy_aff+dy_cc,
   dz = dz_aff+dz_cc */

static double alfa_max_p, alfa_max_d;
/* maximal primal and dual stepsizes in the combined direction, on which
   x and z are still non-negative */

/*----------------------------------------------------------------------
-- initialize - allocate and initialize common data.
--
-- This routine allocates and initializes common data used by other
-- local routines. */

static void initialize(MAT *_A, double _b[], double _c[], double _x[],
      double _y[], double _z[])
{     m = _A->m;
      n = _A->n;
      A = _A;
      b = _b;
      c = _c;
      x = _x;
      y = _y;
      z = _z;
      R = ucalloc(1+m, sizeof(double));
      S = ucalloc(1+n, sizeof(double));
      D = ucalloc(1+n, sizeof(double));
      print("Factorizing S = A*A' symbolically...");
      print("nz(A) = %d", count_nz(A, 0));
      adat = create_adat(A);
      print("nz(S) = %d (upper triangular part)", count_nz(adat->S, 0));
      print("nz(U) = %d", count_nz(adat->chol->U, 0));
      iter = 0;
      F = 0.0;
      rpi = rdi = gap = 0.0;
      phi = mu = rmu = rmu0 = 0.0;
      phi_min = ucalloc(1+iter_max, sizeof(double));
      iter_best = 0;
      x_best = ucalloc(1+n, sizeof(double));
      y_best = ucalloc(1+m, sizeof(double));
      z_best = ucalloc(1+n, sizeof(double));
      F_best = 0.0;
      dx_aff = ucalloc(1+n, sizeof(double));
      dy_aff = ucalloc(1+m, sizeof(double));
      dz_aff = ucalloc(1+n, sizeof(double));
      alfa_aff_p = alfa_aff_d = 0.0;
      mu_aff = 0.0;
      sigma = 0.0;
      dx_cc = ucalloc(1+n, sizeof(double));
      dy_cc = ucalloc(1+m, sizeof(double));
      dz_cc = ucalloc(1+n, sizeof(double));
      dx = dx_aff, dy = dy_aff, dz = dz_aff;
      alfa_max_p = alfa_max_d = 0.0;
      return;
}

/*----------------------------------------------------------------------
-- scale_lp - scale LP problem.
--
-- This routine computes diagonal scaling matrices R and S that define
-- scaled matrix A~ = R*A*S, where A is the original matrix. At first
-- the routine performs geometric mean scaling in order to minimize the
-- ratio a.max/a.min, where a.max and a.min are respectively maximal and
-- minimal absolute values of elements of the matrix A. Then the routine
-- divides elements of each row by euclidian norm of that row in order
-- that all diagonal elements of the matrix A*A' would be equal to 1 and
-- other elements would be in the range [-1,+1].
--
-- After the scaling matrices R and S have been computed the routine
-- explicitly scales the original LP problem:
--
--    F = c'*x + c[0]
--    A*x = b
--    x >= 0
--
-- that gives the scaled LP problem:
--
--    F = (S*c)'*(inv(S)*x) + c[0]
--    (R*A*S)*(inv(S)*x) = (R*b)
--    (inv(S)*x) >= 0
--
-- where x~ = inv(S)*x, A~ = R*A*S, b~ = R*b, and c~ = S*c are scaled
-- components of the problem. */

static void scale_lp(void)
{     int i, j;
      /* initialize scaling matrices */
      for (i = 1; i <= m; i++) R[i] = 1.0;
      for (j = 1; j <= n; j++) S[j] = 1.0;
#if 0 /* scaling supported in LPX object, so not needed here */
      /* perform implicit geometric mean scaling */
      gm_scaling(A, R, S, 1, 0.01, 20);
#endif
      /* perform implicit normalized scaling: each row of the matrix A
         is divided by its euclidian norm */
      for (i = 1; i <= m; i++)
      {  ELEM *e;
         double sum = 0.0, temp;
         for (e = A->row[i]; e != NULL; e = e->row)
         {  temp = R[e->i] * e->val * S[e->j];
            sum += temp * temp;
         }
         if (sum < 1e-16) sum = 1.0;
         R[i] /= sqrt(sum);
      }
      /* scale the matrix of constraint coefficients */
      for (i = 1; i <= m; i++)
      {  ELEM *e;
         for (e = A->row[i]; e != NULL; e = e->row)
         {  j = e->j;
            e->val *= (R[i] * S[j]);
         }
      }
      /* scale the vector of right-hand sides */
      for (i = 1; i <= m; i++) b[i] *= R[i];
      /* scale the vector of coefficients of the objective function */
      for (j = 1; j <= n; j++) c[j] *= S[j];
      return;
}

/*----------------------------------------------------------------------
-- decomp_ne - factorize normal equation system.
--
-- This routine performs numeric factorization of the matrix A*D*A',
-- which is a coefficient matrix of the normal equation system. It is
-- assumed that the diagonal matrix D has been computed yet. */

static void decomp_ne(void)
{     decomp_adat(adat, A, D);
      return;
}

/*----------------------------------------------------------------------
-- solve_ne - solve normal equation system.
--
-- This routine solves the normal equation system A*D*A'*y = h. It is
-- assumed that the matrix A*D*A' is previously factorized by means of
-- the decomp_ne routine.
--
-- On entry the array y contains the vector of right-hand sides h. On
-- exit this array contains the computed vector of unknowns y.
--
-- After the vector y has been computed the routine checks for numeric
-- stability. If the residual vector r = A*D*A'*y-h is relatively small,
-- the routine returns zero; otherwise the routine returns non-zero. */

static int solve_ne(double y[])
{     int i, ret = 0;
      double *h = ucalloc(1+m, sizeof(double));
      double *r = ucalloc(1+m, sizeof(double));
      /* save vector of right-hand sides h */
      for (i = 1; i <= m; i++) h[i] = y[i];
      /* solve normal equation system A*D*A'*y = h */
      solve_adat(adat, y, NULL);
      /* compute residual vector r = A*D*A'*y - h */
      sym_vec(r, adat->S, y);
      for (i = 1; i <= m; i++) r[i] -= h[i];
      /* check for numeric stability */
      for (i = 1; i <= m; i++)
      {  if (fabs(r[i]) / (1.0 + fabs(h[i])) > 1e-4)
         {  ret = 1;
            break;
         }
      }
      ufree(h);
      ufree(r);
      return ret;
}

/*----------------------------------------------------------------------
-- solve_ns - solve Newtonian system.
--
-- This routine solves the Newtonian system:
--
--    A*dx               = p
--          A'*dy +   dz = q
--    Z*dx        + X*dz = r
--
-- where X = diag(x[j]), Z = diag(z[j]), reducing it to the normal
-- equation system:
--
--    (A*inv(Z)*X*A')*dy = A*inv(Z)*(X*q-r)+p
--
-- (it is assumed that the matrix A*inv(Z)*X*A' is previously factorized
-- by means of the decomp_ne routine).
--
-- After the vector dy has been computed the routine computes other two
-- vectors dx and dz using the formulae:
--
--    dx = inv(Z)*(X*(A'*dy-q)+r)
--    dz = inv(X)*(r-Z*dx)
--
-- The solve_ns routine returns a code reported by the solve_ne routine
-- that solves the normal equation system. */

static int solve_ns(double p[], double q[], double r[], double dx[],
      double dy[], double dz[])
{     int i, j, ret;
      double *work = dx;
      /* compute the vector of right-hand sides A*inv(Z)*(X*q-r)+p for
         the normal equation system */
      for (j = 1; j <= n; j++)
         work[j] = (x[j] * q[j] - r[j]) / z[j];
      mat_vec(dy, A, work);
      for (i = 1; i <= m; i++) dy[i] += p[i];
      /* solve normal equation system to compute the vector dy */
      ret = solve_ne(dy);
      /* compute the vectors dx and dz */
      tmat_vec(dx, A, dy);
      for (j = 1; j <= n; j++)
      {  dx[j] = (x[j] * (dx[j] - q[j]) + r[j]) / z[j];
         dz[j] = (r[j] - z[j] * dx[j]) / x[j];
      }
      return ret;
}

/*----------------------------------------------------------------------
-- initial_point - guess the initial point using Mehrotra's heuristic.
--
-- This routine chooses the starting point using a heuristic proposed
-- in the paper:
--
-- S.Mehrotra. On the implementation of a primal-dual interior point
-- method. SIAM J. on Optim., 2(4), pp. 575-601, 1992.
--
-- The starting point x in the primal space is the solution of the
-- following least squares problem:
--
--    minimize    ||x||
--    subject to  A*x = b
--
-- which is given by the explicit formula:
--
--    x = A'*inv(A*A')*b
--
-- Analogously, the starting point (y,z) in the dual space is the
-- solution of the similar least squares problem:
--
--    minimize    ||z||
--    subject to  A'*y+z = c
--
-- and is given by the explicit formula:
--
--    y = inv(A*A')*A*c
--    z = c-A'*y
--
-- However, some components of the vectors x and z may be non-positive
-- or close to zero, therefore the routine uses a Mehrotra's heuristic
-- in order to find more appropriate starting point. */

static void initial_point(void)
{     int i, j;
      double dp, dd, ex, ez, xz;
      /* factorize A*A' */
      for (j = 1; j <= n; j++) D[j] = 1.0;
      decomp_ne();
      /* x~ = A'*inv(A*A')*b */
      for (i = 1; i <= m; i++) y[i] = b[i];
      solve_ne(y);
      tmat_vec(x, A, y);
      /* y~ = inv(A*A')*A*c */
      mat_vec(y, A, c);
      solve_ne(y);
      /* z~ = c - A'*y~ */
      tmat_vec(z, A, y);
      for (j = 1; j <= n; j++) z[j] = c[j] - z[j];
      /* use Mehrotra's heuristic in order to choose more appropriate
         starting point with positive components of vectors x and z */
      dp = dd = 0.0;
      for (j = 1; j <= n; j++)
      {  if (dp < -1.5 * x[j]) dp = -1.5 * x[j];
         if (dd < -1.5 * z[j]) dd = -1.5 * z[j];
      }
      /* b = 0 involves x = 0, and c = 0 involves y = 0 and z = 0,
         therefore we need be careful */
      if (dp == 0.0) dp = 1.5;
      if (dd == 0.0) dd = 1.5;
      ex = ez = xz = 0.0;
      for (j = 1; j <= n; j++)
      {  ex += (x[j] + dp);
         ez += (z[j] + dd);
         xz += (x[j] + dp) * (z[j] + dd);
      }
      dp += 0.5 * (xz / ez);
      dd += 0.5 * (xz / ex);
      for (j = 1; j <= n; j++)
      {  x[j] += dp;
         z[j] += dd;
         insist(x[j] > 0.0 && z[j] > 0.0);
      }
      return;
}

/*----------------------------------------------------------------------
-- compute - compute necessary information at the current point.
--
-- This routine computes the following quantities at the current point:
--
-- value of the objective function
--
--    F = c'*x + c[0]
--
-- relative primal infeasibility
--
--    rpi = ||A*x-b|| / (1+||b||)
--
-- relative dual infeasibility
--
--    rdi = ||A'*y+z-c|| / (1+||c||)
--
-- primal-dual gap (a relative difference between the primal and the
-- dual objective function values)
--
--    gap = |c'*x-b'*y| / (1+|c'*x|)
--
-- merit function
--
--    phi = ||A*x-b|| / max(1,||b||) + ||A'*y+z-c|| / max(1,||c||) +
--        + |c'*x-b'*y| / max(1,||b||,||c||)
--
-- duality measure
--
--    mu = x'*z / n
--
-- the ratio of infeasibility to mu
--
--    rmu = max(||A*x-b||,||A'*y+z-c||) / mu
--
-- where ||*|| denotes euclidian norm, *' denotes transposition */

static void compute(void)
{     int i, j;
      double norm1, bnorm, norm2, cnorm, cx, by, *work, temp;
      /* compute value of the objective function */
      F = c[0];
      for (j = 1; j <= n; j++) F += c[j] * x[j];
      /* norm1 = ||A*x-b|| */
      work = ucalloc(1+m, sizeof(double));
      mat_vec(work, A, x);
      norm1 = 0.0;
      for (i = 1; i <= m; i++)
         norm1 += (work[i] - b[i]) * (work[i] - b[i]);
      norm1 = sqrt(norm1);
      ufree(work);
      /* bnorm = ||b|| */
      bnorm = 0.0;
      for (i = 1; i <= m; i++) bnorm += b[i] * b[i];
      bnorm = sqrt(bnorm);
      /* compute relative primal infeasibility */
      rpi = norm1 / (1.0 + bnorm);
      /* norm2 = ||A'*y+z-c|| */
      work = ucalloc(1+n, sizeof(double));
      tmat_vec(work, A, y);
      norm2 = 0.0;
      for (j = 1; j <= n; j++)
         norm2 += (work[j] + z[j] - c[j]) * (work[j] + z[j] - c[j]);
      norm2 = sqrt(norm2);
      ufree(work);
      /* cnorm = ||c|| */
      cnorm = 0.0;
      for (j = 1; j <= n; j++) cnorm += c[j] * c[j];
      cnorm = sqrt(cnorm);
      /* compute relative dual infeasibility */
      rdi = norm2 / (1.0 + cnorm);
      /* by = b'*y */
      by = 0.0;
      for (i = 1; i <= m; i++) by += b[i] * y[i];
      /* cx = c'*x */
      cx = 0.0;
      for (j = 1; j <= n; j++) cx += c[j] * x[j];
      /* compute primal-dual gap */
      gap = fabs(cx - by) / (1.0 + fabs(cx));
      /* compute merit function */
      phi = 0.0;
      phi += norm1 / (bnorm > 1.0 ? bnorm : 1.0);
      phi += norm2 / (cnorm > 1.0 ? cnorm : 1.0);
      temp = 1.0;
      if (temp < bnorm) temp = bnorm;
      if (temp < cnorm) temp = cnorm;
      phi += fabs(cx - by) / temp;
      /* compute duality measure */
      mu = 0.0;
      for (j = 1; j <= n; j++) mu += x[j] * z[j];
      mu /= (double)n;
      /* compute the ratio of infeasibility to mu */
      rmu = (norm1 > norm2 ? norm1 : norm2) / mu;
      return;
}

/*----------------------------------------------------------------------
-- make_step - compute the next point using Mehrotra's technique.
--
-- This routine computes the next point using the predictor-corrector
-- technique proposed in the paper:
--
-- S.Mehrotra. On the implementation of a primal-dual interior point
-- method. SIAM J. on Optim., 2(4), pp. 575-601, 1992.
--
-- At first the routine computes so called affine scaling (predictor)
-- direction (dx_aff,dy_aff,dz_aff) which is the solution of the system:
--
--    A*dx_aff                       = b - A*x
--              A'*dy_aff +   dz_aff = c - A'*y - z
--    Z*dx_aff            + X*dz_aff = - X*Z*e
--
-- where (x,y,z) is the current point, X = diag(x[j]), Z = diag(z[j]),
-- e = (1,...,1)'.
--
-- Then the routine computes the centering parameter sigma, using the
-- following Mehrotra's heuristic:
--
--    alfa_aff_p = inf{0 <= alfa <= 1 | x+alfa*dx_aff >= 0}
--
--    alfa_aff_d = inf{0 <= alfa <= 1 | z+alfa*dz_aff >= 0}
--
--    mu_aff = (x+alfa_aff_p*dx_aff)'*(z+alfa_aff_d*dz_aff)/n
--
--    sigma = (mu_aff/mu)^3
--
-- where alfa_aff_p is the maximal stepsize along the affine scaling
-- direction in the primal space, alfa_aff_d is the maximal stepsize
-- along the same direction in the dual space.
--
-- After determining sigma the routine computes so called centering
-- (corrector) direction (dx_cc,dy_cc,dz_cc) which is the solution of
-- the system:
--
--    A*dx_cc                     = 0
--             A'*dy_cc +   dz_cc = 0
--    Z*dx_cc           + X*dz_cc = sigma*mu*e - X*Z*e
--
-- Finally, the routine computes the combined direction
--
--    (dx,dy,dz) = (dx_aff,dy_aff,dz_aff) + (dx_cc,dy_cc,dz_cc)
--
-- and determines maximal primal and dual stepsizes along the combined
-- direction:
--
--    alfa_max_p = inf{0 <= alfa <= 1 | x+alfa*dx >= 0}
--
--    alfa_max_d = inf{0 <= alfa <= 1 | z+alfa*dz >= 0}
--
-- In order to prevent the next point to be too close to the boundary
-- of the positive ortant, the routine decreases maximal stepsizes:
--
--    alfa_p = gamma_p * alfa_max_p
--
--    alfa_d = gamma_d * alfa_max_d
--
-- where gamma_p and gamma_d are scaling factors, and computes the next
-- point:
--
--    x_new = x + alfa_p * dx
--
--    y_new = y + alfa_d * dy
--
--    z_new = z + alfa_d * dz
--
-- which becomes the current point on the next iteration. */

static int make_step(void)
{     int i, j, ret = 0;
      double temp, gamma_p, gamma_d;
      double *p = ucalloc(1+m, sizeof(double));
      double *q = ucalloc(1+n, sizeof(double));
      double *r = ucalloc(1+n, sizeof(double));
      /* p = b - A*x */
      mat_vec(p, A, x);
      for (i = 1; i <= m; i++) p[i] = b[i] - p[i];
      /* q = c - A'*y - z */
      tmat_vec(q, A, y);
      for (j = 1; j <= n; j++) q[j] = c[j] - q[j] - z[j];
      /* r = - X * Z * e */
      for (j = 1; j <= n; j++) r[j] = - x[j] * z[j];
      /* solve the first Newtonian system */
      if (solve_ns(p, q, r, dx_aff, dy_aff, dz_aff))
      {  ret = 1;
         goto done;
      }
      /* alfa_aff_p = inf{0 <= alfa <= 1 | x+alfa*dx_aff >= 0} */
      /* alfa_aff_d = inf{0 <= alfa <= 1 | z+alfa*dz_aff >= 0} */
      alfa_aff_p = alfa_aff_d = 1.0;
      for (j = 1; j <= n; j++)
      {  if (dx_aff[j] < 0.0)
         {  temp = - x[j] / dx_aff[j];
            if (alfa_aff_p > temp) alfa_aff_p = temp;
         }
         if (dz_aff[j] < 0.0)
         {  temp = - z[j] / dz_aff[j];
            if (alfa_aff_d > temp) alfa_aff_d = temp;
         }
      }
      /* mu_aff = (x+alfa_aff_p*dx_aff)'*(z+alfa_aff_d*dz_aff)/n */
      mu_aff = 0.0;
      for (j = 1; j <= n; j++)
      {  mu_aff += (x[j] + alfa_aff_p * dx_aff[j]) *
                   (z[j] + alfa_aff_d * dz_aff[j]);
      }
      mu_aff /= (double)n;
      /* sigma = (mu_aff/mu)^3 */
      temp = mu_aff / mu;
      sigma = temp * temp * temp;
      /* p = 0 */
      for (i = 1; i <= m; i++) p[i] = 0.0;
      /* q = 0 */
      for (j = 1; j <= n; j++) q[j] = 0.0;
      /* r = sigma * mu * e - X * Z * e */
      for (j = 1; j <= n; j++)
         r[j] = sigma * mu - dx_aff[j] * dz_aff[j];
      /* solve the second Newtonian system with the same coefficients
         but with changed right-hand sides */
      if (solve_ns(p, q, r, dx_cc, dy_cc, dz_cc))
      {  ret = 1;
         goto done;
      }
      /* (dx,dy,dz) = (dx_aff,dy_aff,dz_aff) + (dx_cc,dy_cc,dz_cc) */
      for (j = 1; j <= n; j++) dx[j] = dx_aff[j] + dx_cc[j];
      for (i = 1; i <= m; i++) dy[i] = dy_aff[i] + dy_cc[i];
      for (j = 1; j <= n; j++) dz[j] = dz_aff[j] + dz_cc[j];
      /* alfa_max_p = inf{0 <= alfa <= 1 | x+alfa*dx >= 0} */
      /* alfa_max_d = inf{0 <= alfa <= 1 | z+alfa*dz >= 0} */
      alfa_max_p = alfa_max_d = 1.0;
      for (j = 1; j <= n; j++)
      {  if (dx[j] < 0.0)
         {  temp = - x[j] / dx[j];
            if (alfa_max_p > temp) alfa_max_p = temp;
         }
         if (dz[j] < 0.0)
         {  temp = - z[j] / dz[j];
            if (alfa_max_d > temp) alfa_max_d = temp;
         }
      }
      /* determine scale factors (not implemented yet) */
      gamma_p = 0.90;
      gamma_d = 0.90;
      /* compute the next point */
      for (j = 1; j <= n; j++)
      {  x[j] += gamma_p * alfa_max_p * dx[j];
         insist(x[j] > 0.0);
      }
      for (i = 1; i <= m; i++)
         y[i] += gamma_d * alfa_max_d * dy[i];
      for (j = 1; j <= n; j++)
      {  z[j] += gamma_d * alfa_max_d * dz[j];
         insist(z[j] > 0.0);
      }
done: /* return to the calling program */
      ufree(p);
      ufree(q);
      ufree(r);
      return ret;
}

/*----------------------------------------------------------------------
-- unscale_lp - unscale LP problem and obtained solution.
--
-- This routine unscales the LP problem and obtained solution using
-- scaling matrices R and S (for details see comments to the scale_lp
-- routine):
--
--    A = inv(R)*A~*inv(S)
--    b = inv(R)*b~
--    c = inv(S)*c~
--    x = S*x~
--    y = R*y~
--    z = inv(S)*z~
--
-- where ()~ denotes scaled quantities, () denotes unscaled (original)
-- quantities */

static void unscale_lp(void)
{     int i, j;
      /* unscale the matrix of constraint coefficients */
      for (i = 1; i <= m; i++)
      {  ELEM *e;
         for (e = A->row[i]; e != NULL; e = e->row)
         {  j = e->j;
            e->val /= (R[i] * S[j]);
         }
      }
      /* unscale the vector of right-hand sides */
      for (i = 1; i <= m; i++) b[i] /= R[i];
      /* unscale the vector of coefficients of the objective function */
      for (j = 1; j <= n; j++) c[j] /= S[j];
      /* unscale the solution vectors */
      for (j = 1; j <= n; j++) x[j] *= S[j];
      for (i = 1; i <= m; i++) y[i] *= R[i];
      for (j = 1; j <= n; j++) z[j] /= S[j];
      return;
}

/*----------------------------------------------------------------------
-- terminate - deallocate common data.
--
-- This routine frees all memory allocated to common data by means of
-- the initialize routine. */

static void terminate(void)
{     ufree(R);
      ufree(S);
      ufree(D);
      delete_adat(adat);
      ufree(phi_min);
      ufree(x_best);
      ufree(y_best);
      ufree(z_best);
      ufree(dx_aff);
      ufree(dy_aff);
      ufree(dz_aff);
      ufree(dx_cc);
      ufree(dy_cc);
      ufree(dz_cc);
      return;
}

/*----------------------------------------------------------------------
-- ipm1 - main routine.
--
-- This is main routine of the primal-dual interior point method. */

int ipm1(MAT *_A, double _b[], double _c[], double _x[], double _y[],
      double _z[])
{     int i, j, status;
      double temp;
      /* allocate and initialize common data */
      initialize(_A, _b, _c, _x, _y, _z);
      /* scale LP problem */
      scale_lp();
      /* guess the initial point using Mehrotra's heuristic */
      print("Guessing initial point...");
      initial_point();
      /* main loop starts here */
      print("Optimization begins...");
      for (;;)
      {  /* compute necessary information at the current point */
         compute();
         /* save the initial value of rmu */
         if (iter == 0) rmu0 = rmu;
         /* accumulate values of min(phi[k]) and save the best point */
         insist(iter <= iter_max);
         if (iter == 0 || phi_min[iter-1] > phi)
         {  phi_min[iter] = phi;
            iter_best = iter;
            for (j = 1; j <= n; j++) x_best[j] = x[j];
            for (i = 1; i <= m; i++) y_best[i] = y[i];
            for (j = 1; j <= n; j++) z_best[j] = z[j];
            F_best = F;
         }
         else
            phi_min[iter] = phi_min[iter-1];
         /* display information about the current point */
         print("%3d: F = %17.9e; rpi = %8.1e; rdi = %8.1e; gap = %8.1e",
            iter, F, rpi, rdi, gap);
         /* check whether the current solution satisfies optimality
            conditions */
         if (rpi < 1e-8 && rdi < 1e-8 && gap < 1e-8)
         {  print("OPTIMAL SOLUTION FOUND");
            status = 0;
            break;
         }
         /* check whether the problem has no feasible solutions */
         temp = 1e5 * phi_min[iter];
         if (temp < 1e-8) temp = 1e-8;
         if (phi >= temp)
         {  print("PROBLEM HAS NO FEASIBLE (PRIMAL OR DUAL) SOLUTION");
            status = 1;
            break;
         }
         /* check for very slow convergence or divergence */
         if (((rpi >= 1e-8 || rdi >= 1e-8) && rmu / rmu0 >= 1e6) ||
            (iter >= 30 && phi_min[iter] >= 0.5 * phi_min[iter-30]))
         {  print("NO CONVERGENCE; SEARCH TERMINATED");
            status = 2;
            break;
         }
         /* check for maximal number of iterations */
         if (iter == iter_max)
         {  print("ITERATIONS LIMIT EXCEEDED; SEARCH TERMINATED");
            status = 3;
            break;
         }
         /* start the next iteration */
         iter++;
         /* factorize normal equation system */
         for (j = 1; j <= n; j++) D[j] = x[j] / z[j];
         decomp_ne();
         /* compute the next point using Mehrotra's predictor-corrector
            technique */
         if (make_step())
         {  print("NUMERICAL INSTABILITY; SEARCH TERMINATED");
            status = 4;
            break;
         }
      }
      /* restore the best point */
      if (status != 0)
      {  for (j = 1; j <= n; j++) x[j] = x_best[j];
         for (i = 1; i <= m; i++) y[i] = y_best[i];
         for (j = 1; j <= n; j++) z[j] = z_best[j];
         print("The best point was reached on iteration %d: F = %17.9e",
            iter_best, F_best);
      }
      /* unscale LP problem and obtained solution */
      unscale_lp();
      /* deallocate common data */
      terminate();
      /* return to the calling program */
      return status;
}

/* eof */
