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

void printMatrix(fastf_t **mat, char *strname)
{
    int i=0;
    int j = 0;
    bu_log("\n-----%s------\n", strname);
    for (i = 0; i < 30; i++) {
	if (i%6 == 0 && i != 0) {
	    bu_log("]\n");
	    j++;
	}
	bu_log("%.4f, ", mat[j][i%6]);
    }
    bu_log("\n-----------\n");
}

void printVec(fastf_t *result1, int c, char *strname)
{
    int i=0;
    bu_log("\n\n-----%s------\n", strname);
    for (i = 0; i < c; i++) {
	bu_log("%.8f ", result1[i]);
    }
    bu_log("\n-------------------------\n\n");
}

fastf_t GetEllSlopeAtPoint(fastf_t *inarray, fastf_t x, fastf_t y)
{
    fastf_t A,B,C,D,E,F,slope;
    A = inarray[0];
    B = inarray[1];
    C = inarray[2];
    D = inarray[3];
    E = inarray[4];
    F = -1;
    slope = -(D+y*B+2*x*A)/(E+2*y*C+x*B);
/*    slope = -((2*B*E - 4*C*D - 8*x*A*C + 2*x*B*B)/(2*sqrt(E*E + 2*x*B*E - 4*x*C*D - 4*x*x*A*C + x*x*B*B))+B)/(2*C);*/
    return slope;
}


void Create_Ell1_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dytop, fastf_t dztop, fastf_t ztire) 
{
    mat[0][0] = (dytop/2) * (dytop/2);
    mat[0][1] = ((ztire-dztop) * dytop/2);
    mat[0][2] = (ztire-dztop) * (ztire-dztop);
    mat[0][3] = dytop / 2;
    mat[0][4] = (ztire-dztop);
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
    mat[3][0] = (-dytop / 2) * (-dytop / 2);
    mat[3][1] = ((ztire-dztop) * -dytop / 2);
    mat[3][2] = (ztire-dztop) * (ztire-dztop);
    mat[3][3] = -dytop / 2;
    mat[3][4] = (ztire-dztop);
    mat[3][5] = -1;
    mat[4][0] = (-dytred / 2) * (-dytred / 2);
    mat[4][1] = (ztire - dztred) * (-dytred / 2);
    mat[4][2] = (ztire - dztred) * (ztire - dztred);
    mat[4][3] = (-dytred / 2);
    mat[4][4] = (ztire - dztred);
    mat[4][5] = -1;
}

void Create_Ell2_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dyside1, fastf_t zside1, fastf_t dyside2, fastf_t zside2, fastf_t ztire, fastf_t dyhub, fastf_t zhub, fastf_t ell1slope) 
{
    mat[0][0] = (dyside1 / 2) * (dyside1 / 2);
    mat[0][1] = (zside1 * dyside1 / 2);
    mat[0][2] = zside1 * zside1;
    mat[0][3] = dyside1 / 2;
    mat[0][4] = zside1;
    mat[0][5] = -1;
    mat[1][0] = (dytred / 2) * (dytred / 2);
    mat[1][1] = (ztire - dztred) * (dytred / 2);
    mat[1][2] = (ztire - dztred) * (ztire - dztred);
    mat[1][3] = (dytred / 2);
    mat[1][4] = (ztire - dztred);
    mat[1][5] = -1;
    mat[2][0] = (dyhub/2) * (dyhub/2);
    mat[2][1] = (dyhub/2) * zhub;
    mat[2][2] = zhub * zhub;
    mat[2][3] = dyhub/2;
    mat[2][4] = zhub;
    mat[2][5] = -1;
    mat[3][0] = 2 * (dytred / 2);
    mat[3][1] = (ztire - dztred) + (dytred / 2) * ell1slope;
    mat[3][2] = 2*(ztire - dztred) * ell1slope;
    mat[3][3] = 1;
    mat[3][4] = ell1slope;
    mat[3][5] = 0;
    mat[4][0] = (dyside2 / 2) * (dyside2 / 2);
    mat[4][1] = (zside2*dyside2 / 2);
    mat[4][2] = zside2*zside2;
    mat[4][3] = dyside2 / 2;
    mat[4][4] = zside2;
    mat[4][5] = -1;
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

void CalcInputVals(fastf_t *inarray, fastf_t *outarray, int orientation)
{
    fastf_t A,B,C,D,E,F;
    fastf_t Dp,Ep,Fp;
    fastf_t App, Bpp,Cpp,Dpp, Epp,Fpp;
    fastf_t x0,y0;
    fastf_t theta;
    fastf_t length1, length2;
    fastf_t semimajor, semiminor;
    fastf_t semimajorx,semimajory;


    A = inarray[0];
    B = inarray[1];
    C = inarray[2];
    D = inarray[3];
    E = inarray[4];

    /*Translation to Center of Ellipse*/
    x0 = -(B*E-2*C*D)/(4*A*C-B*B);
    y0 = -(B*D-2*A*E)/(4*A*C-B*B);

    /*Translate to the Origin for Rotation*/

    Fp = 1-y0*E-x0*D+y0*y0*C+x0*y0*B+x0*x0*A;

    /*Rotation Angle*/
    theta = .5*atan(1000*B/(1000*A-1000*C));
    
    /*B' is zero with above theta choice*/
    App = A*cos(theta)*cos(theta)+B*cos(theta)*sin(theta)+C*sin(theta)*sin(theta);
    Bpp = 2*(C-A)*cos(theta)*sin(theta)+B*cos(theta)*cos(theta)-B*sin(theta)*sin(theta);
    Cpp = A*sin(theta)*sin(theta)-B*sin(theta)*cos(theta)+C*cos(theta)*cos(theta);

    length1 = sqrt(-Fp/App);
    length2 = sqrt(-Fp/Cpp);

    if (length1 > length2) {
	semimajor = length1;
	semiminor = length2;
    } else {
	semimajor = length2;
	semiminor = length1;
    }

    /*Based on orientation of Ellipse, largest component of C is either in the
     *y direction (0) or z direction (1);
     */

    if (orientation == 0){
	semimajorx = semimajor*cos(-theta);
	semimajory = semimajor*sin(-theta);
    }else{
	semimajorx = semimajor*sin(-theta);
	semimajory = semimajor*cos(-theta);
    }

    outarray[0] = -x0;
    outarray[1] = -y0;
    outarray[2] = semimajorx;
    outarray[3] = semimajory;
    outarray[4] = semiminor;
}

void MakeTireCore(struct rt_wdb (*file), fastf_t dytred, fastf_t dztred, fastf_t dytop, fastf_t dztop, fastf_t dyside2, fastf_t zside2, fastf_t ztire, fastf_t dyside1, fastf_t zside1, fastf_t dyhub, fastf_t zhub)
{
    struct wmember tiresideoutercutright, tiresideoutercutleft, tiresideinnercutright, tiresideinnercutleft;
    struct wmember tiretred, tiresides, tiresurface;
    struct wmember innersolid;
    struct wmember tire;
    int i;
    vect_t vertex,height;
    point_t origin,normal,C;
    unsigned char rgb[3];
    fastf_t ell1slope,ell2slope;
    fastf_t ell1coefficients[5],ell2coefficients[5];
    fastf_t ell1cadparams[5],ell2cadparams[5];
    fastf_t **matrixell1,**matrixell2,**matrixell3;
    matrixell1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixell1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    matrixell2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixell2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");

    matrixell3 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixell3[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
   
    Create_Ell1_Mat(matrixell1, dytred, dztred, dytop, dztop, ztire);
    Triangularize(matrixell1);
    SolveTri(matrixell1,ell1coefficients);
    printVec(ell1coefficients,5,"Ellipse 1 Coefficients");
    printf("elleqn1 : %6.9f*x^2+%6.9f*x*y+%6.9f*y^2+%6.9f*x+%6.9f*y-1;\n",ell1coefficients[0],ell1coefficients[1],ell1coefficients[2],ell1coefficients[3],ell1coefficients[4]);
    ell1slope = GetEllSlopeAtPoint(ell1coefficients,dytred/2,ztire-dztred);
    printf("ell1slope = %6.9f\n",ell1slope);
    Create_Ell2_Mat(matrixell2, dytred, dztred, dyside1, zside1, dyside2, zside2, ztire, dyhub, zhub, ell1slope);
    Triangularize(matrixell2);
    SolveTri(matrixell2,ell2coefficients);
    printVec(ell2coefficients,5,"Ellipse 2 Coefficients");
    printf("elleqn1 : %6.9f*x^2+%6.9f*x*y+%6.9f*y^2+%6.9f*x+%6.9f*y-1;\n",ell2coefficients[0],ell2coefficients[1],ell2coefficients[2],ell2coefficients[3],ell2coefficients[4]);
    ell2slope = GetEllSlopeAtPoint(ell2coefficients,dytred/2,ztire-dztred);
    printf("ell2slope = %6.9f\n",ell2slope);
    CalcInputVals(ell1coefficients,ell1cadparams,0);
    printVec(ell1cadparams,5,"Ellipse 1 Input Parameters");
    CalcInputVals(ell2coefficients,ell2cadparams,1);
    printVec(ell2cadparams,5,"Ellipse 2 Input Parameters");


    VSET(origin, 0, ell1cadparams[0], 0);
    VSET(normal, 0, 1, 0);
    VSET(C, 0, ell1cadparams[2],ell1cadparams[3]);
    mk_eto(file, "Ellipse1.s", origin, normal, C, ell1cadparams[1], ell1cadparams[4]);
    VSET(origin, 0, ell2cadparams[0], 0);
    VSET(normal, 0, ell2cadparams[2]/sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    VSET(C, 0, ell2cadparams[2],ell2cadparams[3]);
    mk_eto(file, "Ellipse2.s", origin, normal, C, ell2cadparams[1], ell2cadparams[4]);
    VSET(origin, 0, -ell2cadparams[0], 0);
    VSET(normal, 0, -ell2cadparams[2]/sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    VSET(C, 0, -ell2cadparams[2],-ell2cadparams[3]);
    mk_eto(file, "Ellipse3.s", origin, normal, C, ell2cadparams[1], ell2cadparams[4]);
    VSET(vertex, 0, 0, 0);
    VSET(height, 0, dyside1+sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    mk_rcc(file, "TopClipR.s", vertex, height, ztire - dztred);
    VSET(vertex, 0, 0, 0);
    VSET(height, 0, -dyside1-sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    mk_rcc(file, "TopClipL.s", vertex, height, ztire - dztred);
    VSET(vertex, 0, -dyside1-sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    VSET(height, 0, 2*(dyside1+sqrt(ell2cadparams[2]*ell2cadparams[2])), 0);
    mk_rcc(file, "SideClipOuter.s", vertex, height, ztire - dztred);
    mk_rcc(file, "SideClipInner.s", vertex, height, zhub);

    VSET(vertex, 0, -dytred/2, 0);
    VSET(height, 0, dytred, 0);
    mk_rcc(file, "InnerSolid.s", vertex, height, ztire-dztred);
    VSET(normal, 0, 1, 0);
    mk_cone(file, "LeftCone.s", vertex, normal, dytred/2 - dyhub/2, ztire-dztred,zhub);
    VSET(vertex, 0, dytred/2, 0);
    VSET(normal, 0, -1, 0);
    mk_cone(file, "RightCone.s", vertex, normal, dytred/2 - dyhub/2, ztire-dztred,zhub);


    

    VSET(rgb, 217, 217, 217);
    BU_LIST_INIT(&tiresideoutercutright.l);
    (void)mk_addmember("Ellipse2.s", &tiresideoutercutright.l, NULL, WMOP_UNION);
    (void)mk_addmember("SideClipOuter.s", &tiresideoutercutright.l, NULL, WMOP_INTERSECT);
    mk_lcomb(file, "tire-side-outer-cut-right.c", &tiresideoutercutright, 0, "plastic", "di=.8 sp=.2", rgb, 0);
    BU_LIST_INIT(&tiresideoutercutleft.l);
    (void)mk_addmember("Ellipse3.s", &tiresideoutercutleft.l, NULL, WMOP_UNION);
    (void)mk_addmember("SideClipOuter.s", &tiresideoutercutleft.l, NULL, WMOP_INTERSECT);
    mk_lcomb(file, "tire-side-outer-cut-left.c", &tiresideoutercutleft, 0, "plastic", "di=.8 sp=.2", rgb, 0);
    BU_LIST_INIT(&tiresideinnercutright.l);
    (void)mk_addmember("tire-side-outer-cut-right.c", &tiresideinnercutright.l, NULL, WMOP_UNION);
    (void)mk_addmember("SideClipInner.s", &tiresideinnercutright.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(file, "tire-side-inner-cut-right.c", &tiresideinnercutright, 0, "plastic", "di=.8 sp=.2", rgb, 0);
    BU_LIST_INIT(&tiresideinnercutleft.l);
    (void)mk_addmember("tire-side-outer-cut-left.c", &tiresideinnercutleft.l, NULL, WMOP_UNION);
    (void)mk_addmember("SideClipInner.s", &tiresideinnercutleft.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(file, "tire-side-inner-cut-left.c", &tiresideinnercutleft, 0, "plastic", "di=.8 sp=.2", rgb, 0);
    BU_LIST_INIT(&tiresides.l);
    (void)mk_addmember("tire-side-inner-cut-right.c", &tiresides.l, NULL, WMOP_UNION);
    (void)mk_addmember("tire-side-inner-cut-left.c", &tiresides.l, NULL, WMOP_UNION);
    mk_lcomb(file, "tire-sides.c", &tiresides, 0, "plastic", "di=.8 sp=.2", rgb, 0);

    BU_LIST_INIT(&tiretred.l);
    (void)mk_addmember("Ellipse1.s", &tiretred.l, NULL, WMOP_UNION);
    (void)mk_addmember("TopClipR.s", &tiretred.l, NULL, WMOP_SUBTRACT);
    (void)mk_addmember("TopClipL.s", &tiretred.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(file, "tire-tred-surface.c", &tiretred, 0, "plastic", "di=.8 sp=.2", rgb, 0);

    BU_LIST_INIT(&tiresurface.l);
    (void)mk_addmember("tire-sides.c", &tiresurface.l, NULL, WMOP_UNION);
    (void)mk_addmember("tire-tred-surface.c", &tiresurface.l, NULL, WMOP_UNION);
    mk_lcomb(file, "tire-surface.c", &tiresurface, 0, "plastic", "di=.8 sp=.2", rgb, 0);

    BU_LIST_INIT(&innersolid.l);
    (void)mk_addmember("InnerSolid.s", &innersolid.l, NULL, WMOP_UNION);
    if ((dytred/2 - dyhub/2) > 0 ) {
	(void)mk_addmember("LeftCone.s", &innersolid.l, NULL, WMOP_SUBTRACT);
	(void)mk_addmember("RightCone.s", &innersolid.l, NULL, WMOP_SUBTRACT);
    }
    if ((dytred/2 - dyhub/2) < 0 ) {
	(void)mk_addmember("LeftCone.s", &innersolid.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightCone.s", &innersolid.l, NULL, WMOP_UNION);
    }
    (void)mk_addmember("SideClipInner.s", &innersolid.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(file, "tire-solid.c", &innersolid, 0, "plastic", "di=.8 sp=.2", rgb, 0);

    BU_LIST_INIT(&tire.l);
    (void)mk_addmember("tire-surface.c", &tire.l, NULL, WMOP_UNION);
    (void)mk_addmember("tire-solid.c", &tire.l, NULL, WMOP_UNION);
    mk_lcomb(file, "tire.c", &tire, 1, "plastic", "di=.8 sp=.2", rgb, 0);
  

/*
    if (!NEAR_ZERO(flat_r - outer_r, SMALL_FASTF)) 
	bmag = sqrt((core_width / 2) * (core_width / 2) / (1 - (flat_r * flat_r) / (outer_r * outer_r)));
	* bu_log("Magnitude of flat_r is %f \n", flat_r);
	* bu_log("Magnitude of core_width is %f \n", core_width);
	* bu_log("Magnitude of outer_r is %f \n", outer_r);
	* bu_log("Magnitude of bmag is %f \n", bmag);
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
*/
}

int main(int ac, char *av[])
{
    struct rt_wdb *db_fp;

    if ((db_fp = wdb_fopen(av[1])) == NULL) {
       db_fp = wdb_fopen("tire.g");
    }
    mk_id(db_fp, "Test Database");
    MakeTireCore(db_fp, 120.0,6.5,100.0,3.5,90.0,120.0,206.0,146.0,182.5,126.0,111.0);
 
    wdb_close(db_fp);

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
