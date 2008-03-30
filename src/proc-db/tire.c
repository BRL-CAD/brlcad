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

void Create_Ell1_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dymeas, fastf_t zmeas, fastf_t ztire) 
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
}

void Create_Ell2_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dymax, fastf_t zymax, fastf_t ztire, fastf_t dyhub, fastf_t zhub, fastf_t ell1slope) 
{
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
	printMatrix(mat);
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

void CheckRes (fastf_t *result1)
{
    int i;
    fastf_t tmp;
    fastf_t result2[5];
    result2[0] = 9;
    result2[1] = -3 * -9.813492353461946;
    result2[2] = 9.813492353461946 * 9.813492353461946;
    result2[3] = -3;
    result2[4] = -9.813492353461946;
    printVec(result2,5);
    tmp = 0;
    for (i = 0; i < 5; i++){
	tmp += result1[i]*result2[i];}
    bu_log("result = %.4f\n",tmp);
}

void CalcInputVals(fastf_t *inarray, fastf_t *outarray)
{
    outarray[0] = (inarray[1] * inarray[4] - 2 * inarray[2] * inarray[3]) / (4*inarray[0] * inarray[2] - inarray[1] * inarray[1]);
    bu_log("yc = %.2f \n",outarray[0]);
    outarray[1] = (inarray[1] * inarray[3] - 2 * inarray[0] * inarray[4])/(4 * inarray[0] * inarray[2] - inarray[1] * inarray[1]);
    outarray[2] = 2 * sqrt(2 / (inarray[0] + inarray[2] + sqrt(inarray[1] * inarray[1] + (inarray[0] - inarray[2]) * (inarray[0] - inarray[2]))));
    outarray[3] = 2 * sqrt(2 / (inarray[0] + inarray[2] - sqrt(inarray[1] * inarray[1] + (inarray[0] - inarray[2]) * (inarray[0] - inarray[2]))));
    outarray[4] = outarray[2] * sin(atan(inarray[1] / (inarray[0] - inarray[2])));
    outarray[5] = outarray[3] * cos(atan(inarray[1] / (inarray[0] - inarray[2])));
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
    fastf_t result1[5];
    fastf_t result1a[6];
    
    matell1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matell1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    matell2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matell2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    testmat1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	testmat1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    testmat1[0][0] = 9;
    testmat1[0][1] = -3 * -9.813492353461946;
    testmat1[0][2] = 9.813492353461946 * 9.813492353461946;
    testmat1[0][3] = -3;
    testmat1[0][4] = -9.813492353461946;
    testmat1[0][5] = -1;
    testmat1[1][0] = -4 * -4;
    testmat1[1][1] = -4 * -10.96321449884281;
    testmat1[1][2] = -10.96321449884281 * -10.96321449884281;
    testmat1[1][3] = -4;
    testmat1[1][4] = -10.96321449884281;
    testmat1[1][5] = -1;
    testmat1[2][0] = -5 * -5;
    testmat1[2][1] = -5 * -11.3;
    testmat1[2][2] = -11.3 * -11.3;
    testmat1[2][3] = -5;
    testmat1[2][4] = -11.3;
    testmat1[2][5] = -1;
    testmat1[3][0] = -6.1 * -6.1;
    testmat1[3][1] = -6.1 * -10.890043451677;
    testmat1[3][2] = -10.890043451677 * -10.890043451677;
    testmat1[3][3] = -6.1;
    testmat1[3][4] = -10.890043451677;
    testmat1[3][5] = -1;
    testmat1[4][0] = -7.2 * -7.2;
    testmat1[4][1] = -7.2 * -9.438518609916185;
    testmat1[4][2] = -9.438518609916185 * -9.438518609916185;
    testmat1[4][3] = -7.2;
    testmat1[4][4] = -9.438518609916185;
    testmat1[4][5] = -1;
    printMatrix(testmat1);
    Triangularize(testmat1);
    printMatrix(testmat1);
    SolveTri(testmat1,result1);
    printVec(result1,5);
    CheckRes(result1);
    CalcInputVals(result1,result1a);
    printVec(result1a,6);

/*
 *   Create_Ell1_Mat(matell1, 1015.0,50.0,1000.0,1420.0,1450.0);
 *   Create_Ell2_Mat(matell2, 6.0,1.0,8.0,10.0,12.0,1.0,4.0,-3.0);
 *   Triangularize(matell1);
 *   Triangularize(matell2);
 *   printMatrix(matell1);
 *  SolveTri(matell1,result1);
 *   printVec(result1,5);
 *   CalcInputVals(result1,result1a);
 *   printVec(result1a,6);
 */

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
