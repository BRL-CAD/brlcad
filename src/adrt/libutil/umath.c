#include "umath.h"
#include <string.h>
#include <stdlib.h>


void math_math_init(void);
void math_math_free(void);
void math_mat_ident(tfloat *M, int S);
void math_mat_mult(tfloat *A, int Ar, int Ac, tfloat *B, int Br, int Bc, tfloat *C);
void math_swap_rows(tfloat m[16], int r1, int r2);
void math_mat_invert(tfloat *D, tfloat *M, int S);


void math_mat_ident(tfloat *M, int S) {
  int	i,j;

  for(i = 0; i < S; i++)
    for(j = 0; j < S; j++)
      M[i*S+j] = (i == j) ? 1 : 0;
}


void math_mat_mult(tfloat *A, int Ar, int Ac, tfloat *B, int Br, int Bc, tfloat *C) {
  int		i, j, k;
  tfloat	*M;

  if(Ac == Br) {
    M = (tfloat*)malloc(sizeof(tfloat)*Ar*Bc);
    for (i = 0; i < Bc; i++)
      for (j = 0; j < Ar; j++) {
        M[j*Bc+i] = 0;
        for (k = 0; k < Br; k++)
          M[j*Bc+i] += A[j*Ac+k]*B[k*Bc+i];
      }
    memcpy(C, M, sizeof(tfloat)*Ar*Bc);
    free(M);
  }
}


void math_swap_rows(tfloat m[16], int r1, int r2) {
  tfloat        tmp;
  int           i;

  for (i= 0; i < 4; i++) {
    tmp= m[r1*4 + i];
    m[r1*4 + i]= m[r2*4 + i];
    m[r2*4 + i]= tmp;
  }
}


void math_mat_invert(tfloat *D, tfloat *M, int S) {
  int           i, j, k;
  int           maxrow;
  tfloat        maxval;
  tfloat        val;
  tfloat        T[16];

  memcpy(T, M, 16*sizeof(tfloat));

  math_mat_ident(D, 4);
  maxval = M[0];
  maxrow = 0;

  for(i = 0; i < 4; i++) {
    /* Find row with largest value at the diagonal */
    maxval = M[i*4 + i];
    maxrow = i;

    for(j = i+1; j < 4; j++) {
      val = M[j*4 + i];
      if(fabs(val) > fabs(maxval)) {
        maxval = val;
        maxrow = j;
      }
    }

    /* Swap the row with largest value with current row */
    if(maxrow != i) {
      math_swap_rows(M, i, maxrow);
      math_swap_rows(D, i, maxrow);
    }

    /* Divide the entire current row with maxval to get a 1 on the diagonal */
    for(k = 0; k<4; k++) {
      M[i*4 + k] /= maxval;
      D[i*4 + k] /= maxval;
    }

    /* Subtract current row from all other rows so their values before the diagonal go zero */
    for(j = i+1; j < 4; j++) {
      val = M[j*4 + i];
      for(k = 0; k < 4; k++) {
        M[j*4 + k] -= M[i*4 + k] * val;
        D[j*4 + k] -= D[i*4 + k] * val;
      }
    }
  }

  /* Finally substract values so that the original matrix becomes identity */
  for(i = 3; i >= 0; i--) {
    for(j = i-1; j >= 0; j--) {
      val = M[j*4 + i];
      for(k = 0; k < 4; k++) {
        M[j*4 + k] -= M[i*4 + k] * val;
        D[j*4 + k] -= D[i*4 + k] * val;
      }
    }
  }

  memcpy(M, T, 16*sizeof(tfloat));
}
