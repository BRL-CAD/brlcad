/*                          T I R E . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tire.c
 *
 * Tire Generator
 *
 * Program to create basic tire shapes.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

#define D2R(x) (x * DEG2RAD)
#define R2D(x) (x / DEG2RAD)

#define ROWS 5
#define COLS 5

void printMatrix(fastf_t **mat)
{
    int i=0;
    int j = 0;
    bu_log("\n-----%s------\n", "M");
    for (i = 0; i < 30; i++) {
	if (i%6 == 0 && i != 0) {
	    bu_log("]\n");
	    j++;
	}
	bu_log("%.4f, ", mat[j][i%6]);
    }
    bu_log("\n-----------\n");
}

void printVec(fastf_t *result1, int c)
{
    int i=0;
    bu_log("\n-----%s------\n", "V");
    for (i = 0; i < c; i++) {
	bu_log("%.8f ", result1[i]);
    }
    bu_log("\n-----------\n");
}

fastf_t GetEllSlopeAtPoint(fastf_t *inarray, fastf_t x)
{
    fastf_t A,B,C,D,E,F,slope;
    A = inarray[0];
    B = inarray[1];
    C = inarray[2];
    D = inarray[3];
    E = inarray[4];
    F = -1;
    slope = ((2*B*E-4*C*D-8*x*A*C+2*x*B*B)/(2*sqrt(-4*C*F+E*E+2*x*B*E-4*x*C*D-4*x*x*A*C+x*x*B*B))+B)/(2*C);
    return slope;
}

void ValidateEqn(fastf_t *coeff, fastf_t *coord)
{
    fastf_t result1,result2;
    result1 = coeff[0]*coord[0]*coord[0]+coeff[1]*coord[0]*coord[1]+coeff[2]*coord[1]*coord[1]+coeff[3]*coord[0]+coeff[4]*coord[1];
    result2 = coeff[0]*coord[2]*coord[2]+coeff[1]*coord[2]*coord[3]+coeff[2]*coord[3]*coord[3]+coeff[3]*coord[2]+coeff[4]*coord[3];
    printf("\nSanity check :\n");
    printf("Eval at %6.4f,%6.4f is %6.6f\n",coord[0],coord[1],result1+1);
    printf("Eval at %6.4f,%6.4f is %6.6f\n\n",coord[2],coord[3],result2+1);
}

void TestMatrixGeneration(fastf_t **matrix_test, fastf_t x1, fastf_t y1, fastf_t x2, fastf_t y2,
			  fastf_t x3, fastf_t y3, fastf_t x4, fastf_t y4,
			  fastf_t x5, fastf_t y5)
{
    matrix_test[0][0] = x1*x1;
    matrix_test[0][1] = x1*y1;
    matrix_test[0][2] = y1*y1;
    matrix_test[0][3] = x1;
    matrix_test[0][4] = y1;
    matrix_test[0][5] = -1;
    matrix_test[1][0] = x2*x2;
    matrix_test[1][1] = x2*y2;
    matrix_test[1][2] = y2*y2;
    matrix_test[1][3] = x2;
    matrix_test[1][4] = y2;
    matrix_test[1][5] = -1;
    matrix_test[2][0] = x3*x3;
    matrix_test[2][1] = x3*y3;
    matrix_test[2][2] = y3*y3;
    matrix_test[2][3] = x3;
    matrix_test[2][4] = y3;
    matrix_test[2][5] = -1;
    matrix_test[3][0] = x4*x4;
    matrix_test[3][1] = x4*y4;
    matrix_test[3][2] = y4*y4;
    matrix_test[3][3] = x4;
    matrix_test[3][4] = y4;
    matrix_test[3][5] = -1;
    matrix_test[4][0] = x5*x5;
    matrix_test[4][1] = x5*y5;
    matrix_test[4][2] = y5*y5;
    matrix_test[4][3] = x5;
    matrix_test[4][4] = y5;
    matrix_test[4][5] = -1;
}

void Create_Ell1_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dymeas, fastf_t zmeas, fastf_t ztire,fastf_t *coordcheck) 
{
    mat[0][0] = (dymeas/2) * (dymeas/2);
    mat[0][1] = (zmeas * dymeas/2);
    mat[0][2] = zmeas * zmeas;
    mat[0][3] = dymeas / 2;
    mat[0][4] = zmeas;
    mat[0][5] = -1;
    mat[1][0] = (dytred / 2) * (dytred / 2);
    mat[1][1] = (ztire - dztred) * (dytred / 2);
    mat[1][2] = (ztire - dztred) * (ztire - dztred);
    mat[1][3] = (dytred / 2);
    mat[1][4] = (ztire - dztred);
    mat[1][5] = -1;
    mat[2][0] = 0;
    mat[2][1] = 0;
    mat[2][2] = ztire * ztire;
    mat[2][3] = 0;
    mat[2][4] = ztire;
    mat[2][5] = -1;
    mat[3][0] = (-dymeas / 2) * (-dymeas / 2);
    mat[3][1] = (zmeas * -dymeas / 2);
    mat[3][2] = zmeas * zmeas;
    mat[3][3] = -dymeas / 2;
    mat[3][4] = zmeas;
    mat[3][5] = -1;
    mat[4][0] = (-dytred / 2) * (-dytred / 2);
    mat[4][1] = (ztire - dztred) * (-dytred / 2);
    mat[4][2] = (ztire - dztred) * (ztire - dztred);
    mat[4][3] = (-dytred / 2);
    mat[4][4] = (ztire - dztred);
    mat[4][5] = -1;

    printf("ell1x1 : %6.6f; ell1y1 : %6.6f;\nell1x2 : %6.6f; ell1y2 : %6.6f;\nell1x3 : %6.6f; ell1y3 : %6.6f;\n",dymeas/2,zmeas,dytred/2,ztire-dztred,0.0,ztire);
    coordcheck[0] = dymeas/2;
    coordcheck[1] = zmeas;
    coordcheck[2] = dytred/2;
    coordcheck[3] = ztire-dztred;

}

void Create_Ell2_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dymax, fastf_t zymax, fastf_t ztire, fastf_t dyhub, fastf_t zhub, fastf_t *coeff,
		     fastf_t *coordcheck) 
{
    fastf_t ell1slope;
    ell1slope = GetEllSlopeAtPoint(coeff,dytred/2);

    printf("slope of E1 = %6.6f\n",ell1slope);

    mat[0][0] = (dymax / 2) * (dymax / 2);
    mat[0][1] = (zymax * dymax / 2);
    mat[0][2] = zymax * zymax;
    mat[0][3] = dymax / 2;
    mat[0][4] = zymax;
    mat[0][5] = -1;
    mat[1][0] = (dytred / 2) * (dytred / 2);
    mat[1][1] = (ztire - dztred) * (dytred / 2);
    mat[1][2] = (ztire - dztred) * (ztire - dztred);
    mat[1][3] = (dytred / 2);
    mat[1][4] = (ztire - dztred);
    mat[1][5] = -1;
    mat[2][0] = (dymax / 2 - dyhub) * (dymax / 2 - dyhub);
    mat[2][1] = (dymax / 2 - dyhub) * zhub;
    mat[2][2] = zhub * zhub;
    mat[2][3] = dymax / 2 - dyhub;
    mat[2][4] = zhub;
    mat[2][5] = -1;
    mat[3][0] = 2 * (dytred / 2);
    mat[3][1] = (ztire - dztred) + (dytred / 2) * ell1slope;
    mat[3][2] = 2*(dytred / 2) * ell1slope;
    mat[3][3] = 1;
    mat[3][4] = ell1slope;
    mat[3][5] = -1;
    mat[4][0] = 2 * zymax;
    mat[4][1] = dymax / 2;
    mat[4][2] = 0;
    mat[4][3] = 0;
    mat[4][4] = 1;
    mat[4][5] =- 1;

    printf("ell2x1 : %6.6f; ell2y1 : %6.6f;\nell2x2 : %6.6f; ell2y2 : %6.6f;\nell2x3 : %6.6f; ell2y3 : %6.6f;\n",dymax/2,zymax,dytred/2,ztire-dztred,dymax/2-dyhub,zhub);
    coordcheck[0] = dymax/2;
    coordcheck[1] = zymax;
    coordcheck[2] = dytred/2;
    coordcheck[3] = ztire-dztred;

}

void SortRows(fastf_t **mat, int colnum)
{
    int high_row, exam_row;
    int colcount;
    fastf_t temp_elem;
    high_row = colnum;
    for (exam_row = high_row + 1; exam_row < 5; exam_row++) {
	if (mat[exam_row][0] > mat[high_row][0])
	    high_row = exam_row;
    }
    if (high_row != colnum) {
	for(colcount = colnum; colcount < 5; colcount++){
	    temp_elem = mat[colnum][colcount];
	    mat[colnum][colcount] = mat[high_row][colcount];
	    mat[high_row][colcount] = temp_elem;
	}
    }
}

void Triangularize(fastf_t **mat)
{
    int i,j,k;
    fastf_t pivot,rowmult;
    for(i = 0; i < 5; i++){
	SortRows(mat,i);
	pivot = mat[i][i];
	for (j = i; j < 6; j++){
	    mat[i][j] = mat[i][j] / pivot;}
	for (k = i + 1; k < 5 ; k++){
	    rowmult=mat[k][i];
	    for (j = i; j < 6; j++){
		mat[k][j] -= rowmult*mat[i][j];
	    }
	}
    }
}
	    
void SolveTri(fastf_t **mat, fastf_t *result1)
{
    int i,j;
    fastf_t inter;
    for(i = 4; i >= 0; i--){
	inter = mat[i][5];
	for(j = 4; j > i; j--){
	    inter -= mat[i][j]*result1[j];
	}
	result1[i]=inter;
    }
}

void CalcInputVals(fastf_t *inarray, fastf_t *outarray, fastf_t *coordinates)
{
    fastf_t A,B,C,D,E;
    fastf_t Ap,Cp,Dp,Ep;
    fastf_t App, Cpp, Fpp;
    fastf_t x0,y0;
    fastf_t theta;
    fastf_t semimajor, semiminor;
    fastf_t coeffarray2[5],translatedcoords[6];

    A = inarray[0];
    B = inarray[1];
    C = inarray[2];
    D = inarray[3];
    E = inarray[4];
    ValidateEqn(inarray,coordinates);
    
    /*printf("eqn1 = %6.6f*x^2+%6.6f*x*y+%6.6f*y^2+%6.6f*x+%6.6f*y+F\n",A,B,C,D,E);*/

    theta = .5*atan(B/(A-C));
    
    /*B' is zero with above theta choice*/
    Ap = A*cos(theta)*cos(theta)+B*cos(theta)*sin(theta)+C*sin(theta)*sin(theta);
    Cp = A*sin(theta)*sin(theta)-B*sin(theta)*cos(theta)+C*cos(theta)*cos(theta);
    Dp = D*cos(theta)+E*sin(theta);
    Ep = E*cos(theta)-D*sin(theta);
    coeffarray2[0] = Ap;
    coeffarray2[1] = 0;
    coeffarray2[2] = Cp;
    coeffarray2[3] = Dp;
    coeffarray2[4] = Ep;
    translatedcoords[0] = coordinates[0]*cos(theta)-coordinates[1]*sin(theta);
    translatedcoords[1] = coordinates[0]*sin(theta)+coordinates[1]*cos(theta);
    translatedcoords[2] = coordinates[2]*cos(theta)-coordinates[3]*sin(theta);
    translatedcoords[3] = coordinates[2]*sin(theta)+coordinates[3]*cos(theta);
    ValidateEqn(coeffarray2,translatedcoords);

    printf("Ap = %6.4f\n",Ap);
    printf("Cp = %6.4f\n",Cp);
    printf("Dp = %6.4f\n",Dp);
    printf("Ep = %6.4f\n",Ep);

    printf("eqn1 = %6.6f*x^2+%6.6f*y^2+%6.6f*x+%6.6f*y+F\n",Ap,Cp,Dp,Ep);

   /*Dpp and Epp are zero when translated to the origin - solve for x0,y0*/

    x0 = Dp/(2*Ap);
    y0 = Ep/(2*Cp);
    App = Ap;
    Cpp = Cp;
    Fpp = 1 + Ap*x0*x0 - Dp*x0 + Cp*y0*y0 - Ep * y0;

    semimajor = sqrt(-Fpp/Ap);
    semiminor = sqrt(-Fpp/Cp);

    outarray[0] = x0;
    outarray[1] = y0;
    outarray[2] = semimajor;
    outarray[3] = semiminor;
    outarray[4] = R2D(theta);
}

void MakeTireCore(struct rt_wdb (*file), fastf_t flat_r, fastf_t outer_r, fastf_t core_width)
{
    struct wmember tirecorelist;
    vect_t a;
    fastf_t bmag;
    vect_t b;
    vect_t c;
    point_t origin;
    point_t cut_base;
    vect_t cut_height;
    unsigned char rgb[3];
    if (!NEAR_ZERO(flat_r - outer_r, SMALL_FASTF)) 
	bmag = sqrt((core_width / 2) * (core_width / 2) / (1 - (flat_r * flat_r) / (outer_r * outer_r)));
    /* bu_log("Magnitude of flat_r is %f \n", flat_r);
     * bu_log("Magnitude of core_width is %f \n", core_width);
     * bu_log("Magnitude of outer_r is %f \n", outer_r);
     * bu_log("Magnitude of bmag is %f \n", bmag);*/
    VSET(origin, 0, 0, 0);
    VSET(a, outer_r, 0, 0);
    VSET(b, 0, bmag, 0);
    VSET(c, 0, 0, outer_r);
    VSET(cut_base, 0, -core_width/2, 0);
    VSET(cut_height, 0, core_width, 0);    
    if (NEAR_ZERO(flat_r - outer_r, SMALL_FASTF)) {
	mk_rcc(file, "core-curve.s", cut_base, cut_height, outer_r);
    } else {
	mk_ell(file, "core-curve.s", origin, a, b, c);
	mk_rcc(file, "core-cut.s", cut_base, cut_height, outer_r);
    }
    BU_LIST_INIT(&tirecorelist.l);
    (void)mk_addmember("core-curve.s", &tirecorelist.l, NULL, WMOP_UNION);
    if (!NEAR_ZERO(flat_r - outer_r, SMALL_FASTF)) 
	(void)mk_addmember("core-cut.s", &tirecorelist.l, NULL, WMOP_INTERSECT);
    VSET(rgb, 0, 0, 0);
    mk_lcomb(file, "tire-core.c", &tirecorelist, 0, "plastic", "di=.8 sp=.2", rgb, 0);
}


int main()
{
/*    struct rt_wdb *db_fp; */
    int i;
    fastf_t **matell1;
    fastf_t **matell2;
    fastf_t **testmat1;
    fastf_t result1[5],result1a[5];
    fastf_t result2[5],result2a[5];
    fastf_t coordinates[6];
    
    matell1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matell1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    matell2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matell2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    testmat1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	testmat1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
 

/*
    printMatrix(testmat1);
    Triangularize(testmat1);
    printMatrix(testmat1);
    SolveTri(testmat1,result1);
    printVec(result1,5);
    printf("slope = %6.4f\n\n",GetEllSlopeAtPoint(result1,-4.8));
    CalcInputVals(result1,result2);
    printVec(result2,5);*/

    
    Create_Ell1_Mat(matell1, 120.0,6.5,100.0,202.5,206.0,coordinates);
    Triangularize(matell1);
    SolveTri(matell1,result1);
    printVec(result1,5);
    CalcInputVals(result1,result1a,coordinates);
    printVec(result1a,5);

    Create_Ell2_Mat(matell2, 120.0,6.5,126.0,182.5,206.0,21.5,111.0,result1,coordinates);
    Triangularize(matell2);
    SolveTri(matell2,result2);
    CalcInputVals(result2,result2a,coordinates);
    printVec(result2a,5);



/*    if ((db_fp = wdb_fopen(av[1])) == NULL) {
 *       perror(av[1]);
 *       return 1;
 *   }
 *   mk_id(db_fp, "Test Database");
 *   MakeTireCore(db_fp, 100, 100, 40);
 *   wdb_close(db_fp);
 */
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
