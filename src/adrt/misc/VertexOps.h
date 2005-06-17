/****************************************************
 * Vertex Ops Class
 * Oct 09, 2000
 * Justin Shumaker
 *
 * Description:
 * Rotates a Vertex by given X,Y,Z angle supplied
 *****************************************************/


#ifndef VertexOps_H
#define VertexOps_H

#include <math.h>
#include <string.h>
#include "NurbanaMath.h"

#ifndef NGLOBAL_H
#include "NDefs/NGlobal.h"
#endif

class VertexOps {
  public:
    // Matrix Stuff
    static void		MakeXMatrix(double m[16], float radians);
    static void		MakeYMatrix(double m[16], float radians);
    static void		MakeZMatrix(double m[16], float radians);

    static void		IdentMatrix(double m[16]);

    static void		InvertMatrix(double m2[16], double m1[16]);
    static void		MultMatrix(double m2[16], double m1[16]);

    static void		TransposeMatrix(double m[16]);

    // Point Stuff
    static void		MultPtMatrix(Point3d &result, Point3d v, double m[16]);
    static void		RotateVertex(float Xan, float Yan, float Zan, Point3d &VertArray);

    // X,Y,Z,W [0]..[3]
    // Quat / Matrix conversion stuff
    static void		Quat2Matrix(double m[16], float q[4]);
    static void		Matrix2Quat(double q[4], double m[16]);

    // Quat Stuff
    static double	MagnitudeQuat(double q[4]);
    static void		NormalizeQuat(double q[4]);
    static void		InvertQuat(float q2[4], float q1[4]);
    static void		MultQuat(float q2[4], float q1[4]);
    
    // Euler Stuff
    static void		Matrix2Euler(double m[16], double XYZ[3]);

}; //eof class VertexOps
#endif
