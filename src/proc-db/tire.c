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

#define InToMM(x) (x * 25.4)

fastf_t fInToMM(fastf_t x)
{
    return x * 25.4;
}

#define ROWS 5
#define COLS 5

void printMatrix(fastf_t **mat, char *strname)
{
    int i=0;
    int j = 0;
    bu_log("\n----%s------\n", strname);
    for (i = 0; i < 30; i++) {
	if ((i+1)%6 == 0 && i != 0) {
	    bu_log("%.16f\n", mat[j][i%6]);
	    j++;
	} else {
	    bu_log("%.16f, ", mat[j][i%6]);
	}
    }
    bu_log("\n-----------\n");
    bu_log("\n----Maxima%s------\n[", strname);
    i = 0;
    j = 0;
    for (i = 0; i < 30; i++) {
	if ((i+1)%6 == 0 && i != 0) {
	    bu_log("%.7f],[", mat[j][i%6]);
	    j++;
	} else {
	    bu_log("%.7f, ", mat[j][i%6]);
	}
    }
    bu_log("\n-----------\n");
}

void printMatrixEqns(fastf_t **mat, char *strname)
{
    int i=0;
    bu_log("\n----%s------\n", strname);
    for (i = 0; i < 5; i++) {
	bu_log("eqn%1.0d : ",i+1);
	bu_log("A*%.4f+B*%.4f+C*%.4f+D*%.4f+E*%.4f-1=0;\n", mat[i][0], mat[i][1], mat[i][2], mat[i][3], mat[i][4]);
    }
    bu_log("\n");
}

void printVec(fastf_t *result1, int c, char *strname)
{
    int i=0;
    bu_log("\n\n----%s------\n", strname);
    for (i = 0; i < c; i++) {
	bu_log("%.16f ", result1[i]);
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


 void Create_Ell1_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t d1, fastf_t ztire) 
{
    fastf_t y1,z1,y2,z2,y3,z3,y4,z4,y5,z5;

    y1 = dytred/2;
    z1 = ztire-dztred;
    y2 = dytred/2;
    z2 = ztire - (dztred+2*(d1-dztred));
    y3 = 0.0;
    z3 = ztire;
    y4 = 0.0;
    z4 = ztire-2*d1;
    y5 = -dytred/2;
    z5 = ztire-dztred;

    mat[0][0] = y1*y1;
    mat[0][1] = y1*z1;
    mat[0][2] = z1*z1;
    mat[0][3] = y1;
    mat[0][4] = z1;
    mat[0][5] = -1;
    mat[1][0] = y2*y2;
    mat[1][1] = y2*z2;
    mat[1][2] = z2*z2;
    mat[1][3] = y2;
    mat[1][4] = z2;
    mat[1][5] = -1;
    mat[2][0] = y3*y3;
    mat[2][1] = y3*z3;
    mat[2][2] = z3*z3;
    mat[2][3] = y3;
    mat[2][4] = z3;
    mat[2][5] = -1;
    mat[3][0] = y4*y4;
    mat[3][1] = y4*z4;
    mat[3][2] = z4*z4;
    mat[3][3] = y4;
    mat[3][4] = z4;
    mat[3][5] = -1;
    mat[4][0] = y5*y5;
    mat[4][1] = y5*z5;
    mat[4][2] = z5*z5;
    mat[4][3] = y5;
    mat[4][4] = z5;
    mat[4][5] = -1;
    printMatrix(mat,"Ell1");
    printMatrixEqns(mat,"Ell1");
    bu_log("y1 : %6.7f, ",dytred/2);
    bu_log("z1 : %6.7f\n",ztire-dztred);
    bu_log("y2 : %6.7f, ",dytred/2);
    bu_log("z2 : %6.7f\n",(ztire - (dztred+2*(d1-dztred))));
    bu_log("y3 : %6.7f, ",0.0);
    bu_log("z3 : %6.7f\n",ztire);
    bu_log("y4 : %6.7f, ",0.0);
    bu_log("z4 : %6.7f\n",(ztire-2*d1));
    bu_log("y5 : %6.7f, ",-dytred/2);
    bu_log("z5 : %6.7f\n",ztire-dztred);
}
 

void Create_Ell2_Mat(fastf_t **mat, fastf_t dytred, fastf_t dztred, fastf_t dyside1, fastf_t zside1, fastf_t ztire, fastf_t dyhub, fastf_t zhub, fastf_t ell1slope) 
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
    mat[4][0] = 2 * (dyhub / 2);
    mat[4][1] = zhub + (dyhub / 2) * -ell1slope;
    mat[4][2] = 2*zhub*-ell1slope;
    mat[4][3] = 1;
    mat[4][4] = -ell1slope;
    mat[4][5] = 0;
}

void SortRows(fastf_t **mat, int colnum)
{
    int high_row, exam_row;
    int colcount;
    fastf_t temp_elem;
    high_row = colnum;
    for (exam_row = high_row + 1; exam_row < 5; exam_row++) {
	if (fabs(mat[exam_row][colnum]) > fabs(mat[high_row][colnum]))
	    high_row = exam_row;
    }
    if (high_row != colnum) {
	for(colcount = colnum; colcount < 6; colcount++){
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
    printMatrix(mat,"solved");
}

void CalcInputVals(fastf_t *inarray, fastf_t *outarray, int orientation)
{
    fastf_t A,B,C,D,E,Fp;
    fastf_t App, Bpp,Cpp;
    fastf_t x0,y0;
    fastf_t theta;
    fastf_t length1, length2;
    fastf_t semimajor, semiminor;
    fastf_t semimajorx,semimajory;

    bu_log("A=%6.9f,B=%6.9f,C=%6.9f,D=%6.9f,E=%6.9f,F=-1",inarray[0],inarray[1],inarray[2],inarray[3],inarray[4]);
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

void MakeTireCore(struct rt_wdb (*file), char *suffix, fastf_t dytred, fastf_t dztred, fastf_t d1, fastf_t dyside1, fastf_t zside1, fastf_t ztire, fastf_t dyhub, fastf_t zhub)
{
    struct wmember tiresideoutercutright, tiresideoutercutleft, tiresideinnercutright, tiresideinnercutleft,tirecuttopcyl;
    struct wmember tiretred, tiresides, tiresurface;
    struct wmember innersolid;
    struct wmember tire;
    struct bu_vls str;
    bu_vls_init(&str);
    int i;
    vect_t vertex,height;
    point_t origin,normal,C;
    fastf_t ell1slope,ell2slope;
    fastf_t ell1coefficients[5],ell2coefficients[5];
    fastf_t ell1cadparams[5],ell2cadparams[5];
    fastf_t **matrixell1,**matrixell2;
    matrixell1 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixell1[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");
    
    matrixell2 = (fastf_t **)bu_malloc(5 * sizeof(fastf_t *),"matrixrows");
    for (i = 0; i < 5; i++)
	matrixell2[i] = (fastf_t *)bu_malloc(6 * sizeof(fastf_t),"matrixcols");

    Create_Ell1_Mat(matrixell1, dytred, dztred, d1, ztire);
    Triangularize(matrixell1);
    SolveTri(matrixell1,ell1coefficients);
    printVec(ell1coefficients,5,"Ellipse 1 Coefficients");
    printf("elleqn1 : %6.9f*x^2+%6.9f*x*y+%6.9f*y^2+%6.9f*x+%6.9f*y-1;\n",ell1coefficients[0],ell1coefficients[1],ell1coefficients[2],ell1coefficients[3],ell1coefficients[4]);
    ell1slope = GetEllSlopeAtPoint(ell1coefficients,dytred/2,ztire-dztred);
    printf("ell1slope = %6.9f\n",ell1slope);
    Create_Ell2_Mat(matrixell2, dytred, dztred, dyside1, zside1, ztire, dyhub, zhub, ell1slope);
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
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse1%s.s", suffix);	
    mk_eto(file, bu_vls_addr(&str), origin, normal, C, ell1cadparams[1], ell1cadparams[4]);
    VSET(origin, 0, ell2cadparams[0], 0);
    VSET(normal, 0, ell2cadparams[2]/sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    VSET(C, 0, ell2cadparams[2],ell2cadparams[3]);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    mk_eto(file, bu_vls_addr(&str), origin, normal, C, ell2cadparams[1], ell2cadparams[4]);
    VSET(origin, 0, -ell2cadparams[0], 0);
    VSET(normal, 0, -ell2cadparams[2]/sqrt(ell2cadparams[2]*ell2cadparams[2]), 0);
    VSET(C, 0, -ell2cadparams[2],-ell2cadparams[3]);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    mk_eto(file, bu_vls_addr(&str), origin, normal, C, ell2cadparams[1], ell2cadparams[4]);
    VSET(vertex, 0, 0, 0);
    VSET(height, 0, ell1cadparams[2], 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipR%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire - dztred);
    VSET(vertex, 0, 0, 0);
    VSET(height, 0, -ell1cadparams[2], 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipL%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire - dztred);
    VSET(vertex, 0, -dytred/2, 0);
    VSET(height, 0, dytred, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter1%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire - dztred);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, zhub);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter2%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire);
 
    VSET(vertex, 0, -dytred/2, 0);
    VSET(height, 0, dytred, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "InnerSolid%s.s", suffix);	
    mk_rcc(file, bu_vls_addr(&str), vertex, height, ztire-dztred);
    VSET(normal, 0, 1, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "LeftCone%s.s", suffix);	
    mk_cone(file, bu_vls_addr(&str), vertex, normal, dytred/2 - dyhub/2, ztire-dztred,zhub);
    VSET(vertex, 0, dytred/2, 0);
    VSET(normal, 0, -1, 0);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "RightCone%s.s", suffix);	
    mk_cone(file, bu_vls_addr(&str), vertex, normal, dytred/2 - dyhub/2, ztire-dztred,zhub);


    BU_LIST_INIT(&tirecuttopcyl.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirecuttopcyl.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipOuter1%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tirecuttopcyl.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0) ;
    bu_vls_printf(&str, "tire-outer-sides-trim%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tirecuttopcyl, 0, NULL, NULL, NULL, 0);
    

    BU_LIST_INIT(&tiresideoutercutright.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutright.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-outer-sides-trim%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutright.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-right%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideoutercutright, 0, NULL, NULL, NULL, 0);
 
   BU_LIST_INIT(&tiresideoutercutleft.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutleft.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-outer-sides-trim%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideoutercutleft.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-left%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideoutercutleft, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tiresideinnercutright.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutright.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutright.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-right%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideinnercutright, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tiresideinnercutleft.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutleft.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresideinnercutleft.l, NULL, WMOP_INTERSECT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-left%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresideinnercutleft, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tiresides.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse2%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-right%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-right%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse3%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-inner-cut-left%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-side-outer-cut-left%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresides.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-sides%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresides, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tiretred.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "Ellipse1%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiretred.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipR%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiretred.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "TopClipL%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiretred.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-tred-surface%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiretred, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tiresurface.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-sides%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresurface.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-tred-surface%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tiresurface.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-surface%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tiresurface, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&innersolid.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "InnerSolid%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_UNION);
    if ((dytred/2 - dyhub/2) > 0 ) {
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "LeftCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "RightCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_SUBTRACT);
    }
    if ((dytred/2 - dyhub/2) < 0 ) {
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "LeftCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "RightCone%s.s", suffix);	
	(void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_UNION);
    }
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "SideClipInner%s.s", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &innersolid.l, NULL, WMOP_SUBTRACT);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-solid%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &innersolid, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&tire.l);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-surface%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tire.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire-solid%s.c", suffix);	
    (void)mk_addmember(bu_vls_addr(&str), &tire.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "tire%s.c", suffix);	
    mk_lcomb(file, bu_vls_addr(&str), &tire, 0, NULL, NULL, NULL, 0);
  
    bu_vls_free(&str);

    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixell1[i], "matrixell1 element");
    bu_free((char *)matrixell1,"matrixell1");
    for (i = 0; i < 5; i++)
    	bu_free((char *)matrixell2[i], "matrixell2 element");
    bu_free((char *)matrixell2,"matrixell2");

}

int main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    fastf_t dytred,dztred,dyside1,zside1,ztire,dyhub,zhub,d1;
    fastf_t scaley,scalez,cutscaley,cutscalez;
    fastf_t width, ratio, wheeldiam;
    struct wmember tire;
    unsigned char rgb[3];
    VSET(rgb, 40, 40, 40);
    if ((db_fp = wdb_fopen(av[1])) == NULL) {
       db_fp = wdb_fopen("tire.g");
    }
    mk_id(db_fp, "Test Database");
    
/*    dytred = 340.0;
    dztred = 30;
    d1 = 50;
    dyside1 = 440;
    zside1 = 670;
    dyside2 = 460;
    zside2 = 530;
    ztire = 760;
    dyhub = 320;
    zhub = 460; */

/*Testing automatic conversions from std dimension info to geometry*/

/*    width = 255;
    ratio = 40;
    wheeldiam = fInToMM(18.0);

    dyside1 = width;
    ztire = ((width*ratio/100)*2+wheeldiam)/2;
    zhub = ztire-width*ratio/100;
    zside1 = ztire-((ztire-zhub)/2*1.2);
    dztred = fInToMM(11.0/32.0);
    dytred = .8 * width;
    dyhub = dytred;
    d1 = (ztire-zhub)/2.5;*/

/* 255/40R18 Tredwidth=8.9in */

    dyside1 = 255;
    ztire = ((255*.4)*2+InToMM(18))/2;
    zside1 = ztire-50;    
    dytred = fInToMM(8.9);
    dztred = fInToMM(14.0/32.0);
    dyhub = fInToMM(8.9);
    zhub = ztire-255*.4; 
    d1 = (ztire-zhub)/2.5;

    bu_log("dyzide1 = %6.7f\n",dyside1);
    bu_log("ztire = %6.7f\n",ztire);
    bu_log("zside1 = %6.7f\n",zside1);
    bu_log("dytred = %6.7f\n",dytred);
    bu_log("dztred = %6.7f\n",dztred);
    bu_log("d1 = %6.7f\n",d1);
    bu_log("dyhub = %6.7f\n",dyhub);
    bu_log("zhub = %6.7f\n",zhub);

    scaley = 1;
    scalez = 1;
    cutscaley = scaley*0.8;
    cutscalez = scalez*0.9;

    MakeTireCore(db_fp, "-solid", dytred*scaley, dztred*scalez, d1*scalez, dyside1*scaley, zside1*scalez, ztire*scalez, dyhub*scaley, zhub*scalez);
    MakeTireCore(db_fp, "-cut", dytred*cutscaley, dztred*cutscalez, d1*cutscalez, dyside1*cutscaley, zside1*cutscalez, ztire*cutscalez, dyhub*cutscaley, zhub*cutscalez);
 
    BU_LIST_INIT(&tire.l);
    (void)mk_addmember("tire-solid.c", &tire.l, NULL, WMOP_UNION);
    (void)mk_addmember("tire-cut.c", &tire.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(db_fp, "tire.c", &tire, 1,  "plastic", "di=.8 sp=.2", rgb, 0);

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
