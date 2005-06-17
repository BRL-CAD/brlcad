#include "VertexOps.h"


void VertexOps::MakeXMatrix(double m[16], float radians) {
  double c= (double)cos(radians);
  double s= (double)sin(radians);

  m[0]= 1.0;   m[1]= 0.0;   m[2]= 0.0;   m[3]= 0.0;
  m[4]= 0.0;   m[5]= c;     m[6]= s;     m[7]= 0.0;
  m[8]= 0.0;   m[9]= -s;    m[10]= c;    m[11]= 0.0;
  m[12]= 0.0;  m[13]= 0.0;  m[14]= 0.0;  m[15]= 1.0;
} //eof VertexOps::MakeXmatrix()


void VertexOps::MakeYMatrix(double m[16], float radians) {
  double c= (double)cos(radians);
  double s= (double)sin(radians);

  m[0]= c;     m[1]= 0.0;   m[2]= -s;    m[3]= 0.0;
  m[4]= 0.0;   m[5]= 1.0;   m[6]= 0.0;   m[7]= 0.0;
  m[8]= s;     m[9]= 0.0;   m[10]= c;    m[11]= 0.0;
  m[12]= 0.0;  m[13]= 0.0;  m[14]= 0.0;  m[15]= 1.0;
} //eof VertexOps::MakeYmatrix()


void VertexOps::MakeZMatrix(double m[16], float radians) {
  double c= (double)cos(radians);
  double s= (double)sin(radians);

  m[0]= c;     m[1]= s;     m[2]= 0.0;   m[3]= 0.0;
  m[4]= -s;    m[5]= c;     m[6]= 0.0;   m[7]= 0.0;
  m[8]= 0.0;   m[9]= 0.0;   m[10]= 1.0;  m[11]= 0.0;
  m[12]= 0.0;  m[13]= 0.0;  m[14]= 0.0;  m[15]= 1.0;
} //eof VertexOps::MakeZMatrix()


void VertexOps::IdentMatrix(double m[16]) {
  m[0]= 1;   m[1]= 0;   m[2]= 0;   m[3]= 0;
  m[4]= 0;   m[5]= 1;   m[6]= 0;   m[7]= 0;
  m[8]= 0;   m[9]= 0;   m[10]= 1;  m[11]= 0;
  m[12]= 0;  m[13]= 0;  m[14]= 0;  m[15]= 1;
} //eof VertexOps::IdentMatrix()


void _SwapRows(double m[16], int r1, int r2) {
  double	tmp;
  int		i;

  for (i= 0; i<4; i++) {
    tmp= m[r1*4 + i];
    m[r1*4 + i]= m[r2*4 + i];
    m[r2*4 + i]= tmp;
  } //eof
} //eof _SwapRows()


void VertexOps::InvertMatrix(double m2[16], double m1[16]) {
  int		i,j,k;
  int		maxrow;
  double	maxval;
  double	val;
  double	m3[16];

  memcpy(m3, m1, 16*sizeof(double));

  IdentMatrix(m2);
  maxval= m1[0];
  maxrow= 0; 

  for (i= 0; i<4; i++) {
    // Find row with largest value at the diagonal.
    maxval= m1[i*4 + i];
    maxrow= i;

    for (j= i+1; j<4; j++) {
      val= m1[j*4 + i];
      if (fabs(val) > fabs(maxval)) {
        maxval= val;
        maxrow= j;
      } //fi
    } //eof


    // Swap the row with largest value with current row.
    if (maxrow != i) {
      _SwapRows(m1, i, maxrow);
      _SwapRows(m2, i, maxrow);
    } //fi

    // Divide the entire current row with maxval to get a 1 on the diagonal
    for (k= 0; k<4; k++) {
      m1[i*4 + k] /= maxval;
      m2[i*4 + k] /= maxval; 
    } //eof

    // Subtract current row from all other rows so their values before the
    // diagnal go zero.
    for (j= i+1; j<4; j++) {
      val= m1[j*4 + i];
      for (k= 0; k<4; k++) {
        m1[j*4 + k]-= m1[i*4 + k] * val;
        m2[j*4 + k]-= m2[i*4 + k] * val;
      } //eof
    } //eof
  } //eof

  // Finally substract values so that the original matrix becomes identity
  for (i= 3; i>=0; i--) {
    for (j= i-1; j>=0; j--) {
      val= m1[j*4 + i];
      for (k= 0; k<4; k++) {
        m1[j*4 + k]-= m1[i*4 + k] * val;
        m2[j*4 + k]-= m2[i*4 + k] * val;
      } //eof
    } //eof
  } //eof

  memcpy(m1, m3, 16*sizeof(double));
} //eof VertexOps::InvertMatrix()


void VertexOps::MultPtMatrix(Point3d &result, Point3d v, double m[16]) {
  double	w;
  double	ptmp[3];

  ptmp[0]= (v.x * m[0]) + (v.y * m[4]) + (v.z * m[8]) + m[12];
  ptmp[1]= (v.x * m[1]) + (v.y * m[5]) + (v.z * m[9]) + m[13];
  ptmp[2]= (v.x * m[2]) + (v.y * m[6]) + (v.z * m[10]) + m[14];
  w      = (v.x * m[3]) + (v.y * m[7]) + (v.z * m[11]) + m[15];
  if (w) { ptmp[0] /= w;  ptmp[1] /= w;  ptmp[2] /= w; }
  result.x= ptmp[0];
  result.y= ptmp[1];
  result.z= ptmp[2];
} //eof VertexOps::MultPtMatrix()


void VertexOps::RotateVertex(float Xan, float Yan, float Zan, Point3d &VertArray) {
  double	mx[16],my[16],mz[16];
  Point3d	TempTransform;

  MakeXMatrix(mx,(float)NurbanaMath::DtoR(Xan));
  MakeYMatrix(my,(float)NurbanaMath::DtoR(Yan));
  MakeZMatrix(mz,(float)NurbanaMath::DtoR(Zan));

  //Transform vertices to buffer
  MultPtMatrix(TempTransform,VertArray,mx);
  MultPtMatrix(TempTransform,TempTransform,my);
  MultPtMatrix(TempTransform,TempTransform,mz);

  //Transfer buffer to actual object
  VertArray.x= TempTransform.x;
  VertArray.y= TempTransform.y;
  VertArray.z= TempTransform.z;
} //eof VertexOps::RotateVertex()


void VertexOps::MultMatrix(double m2[16], double m1[16]) {
  //Resulting Matrix is m2
  int		i,n,k;
  double	tm[16];

  for (i= 0; i < 16; i++) {
    tm[i]= m2[i];
    m2[i]= 0;
  } //eof
  
  for (i= 0; i < 4; i++) { // each row of matrix 1
    for (n= 0; n < 4; n++) { // each column of matrix 2
      for (k= 0; k < 4; k++) { // each element of the column in matrix 2
        m2[i*4+n]+= m1[i*4+k]*tm[n+k*4];
      } //eof
    } //eof  
  } //eof
} //eof VertexOps::MultMatrix()


void Swap(double &A, double &B) {
  double	T;
  T= A;
  A= B;
  B= T;
} //eof Swap()


void VertexOps::TransposeMatrix(double m[16]) {
  Swap(m[1],m[4]);
  Swap(m[2],m[8]);
  Swap(m[6],m[9]);
  Swap(m[3],m[12]);
  Swap(m[7],m[13]);
  Swap(m[11],m[14]);
} //eof VertexOps::TransposeMatrix()


void VertexOps::Quat2Matrix(double m[16], float q[4]) {
  // q[4]= x,y,z,w
  m[0]=  1 - 2 * (q[1]*q[1] + q[2]*q[2]);
  m[1]=      2 * (q[0]*q[1] - q[2]*q[3]);
  m[2]=      2 * (q[0]*q[2] + q[1]*q[3]);

  m[4]=      2 * (q[0]*q[1] + q[2]*q[3]);
  m[5]=  1 - 2 * (q[0]*q[0] + q[2]*q[2]);
  m[6]=      2 * (q[1]*q[2] - q[0]*q[3]);

  m[8]=      2 * (q[0]*q[2] - q[1]*q[3]);
  m[9]=      2 * (q[1]*q[2] + q[0]*q[3]);
  m[10]= 1 - 2 * (q[0]*q[0] + q[1]*q[1]);

  m[3]= m[7]= m[11]= m[12]= m[13]= m[14]= 0;
  m[15]= 1;
} //eof VertexOps::Quat2Matrix()


void VertexOps::Matrix2Quat(double q[4], double m[16]) {
  double	S,V,T;
  char		Column;

  T= m[0] + m[5] + m[10] + 1;

  if (T > 0) {
    S= 0.5 / sqrt(T);
    q[0]= (m[9] - m[6]) * S;
    q[1]= (m[2] - m[8]) * S;
    q[2]= (m[4] - m[1]) * S;
    q[3]= 0.25 / S;
  } else {
    Column= 0;
    V= m[0];
    if (m[5] > V) {
      Column= 1;
      V= m[5];
    } //fi
    if (m[10] > V) {
      Column= 2;
      V= m[10];
    } //fi

    switch( Column ) {
      case 0:
        S= sqrt(1.0 + m[0] - m[5] - m[10]) * 2;
        q[0]= 0.5 / S;
        q[1]= (m[1] + m[4]) / S;
        q[2]= (m[2] + m[8]) / S;
        q[3]= (m[6] + m[9]) / S;
        break;

      case 1:
        S= sqrt(1.0 + m[5] - m[0] - m[10]) * 2;
        q[0]= (m[1] + m[4]) / S;
        q[1]= 0.5 / S;
        q[2]= (m[6] + m[9]) / S;
        q[3]= (m[2] + m[8]) / S;
        break;

      case 2:
        S= sqrt(1.0 + m[10] - m[0] - m[5]) * 2;
        q[0]= (m[2] + m[8] ) / S;
        q[1]= (m[6] + m[9] ) / S;
        q[2]= 0.5 / S;
        q[3]= (m[1] + m[4] ) / S;
        break;


      default:
        break;
    } //eos
  } //fi
} //eof VertexOps::Matrix2Quat()


double VertexOps::MagnitudeQuat(double q[4]) {
  return( sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]) );
} //eof VertexOps::MagnitudeQuat()


void VertexOps::NormalizeQuat(double q[4]) {
  double	m;

  m= MagnitudeQuat(q);
  q[0]/= m;
  q[1]/= m;
  q[2]/= m;
  q[3]/= m;
} //eof VertexOps::NormalizeQuat()


void VertexOps::InvertQuat(float q2[4], float q1[4]) {
  q2[0]= -q1[0];
  q2[1]= -q1[1];
  q2[2]= -q1[2];
  q2[3]=  q1[3];
} //eof VertexOps::InvertQuat()


void VertexOps::MultQuat(float q2[4], float q1[4]) {
  float		qt[4];
  int		i;

  // Copy q2 into qt
  for (i= 0; i < 4; i++) qt[i]= q2[i];

  q2[0]= q1[3]*qt[0] + q1[0]*qt[3] + q1[1]*qt[2] - q1[2]*qt[1];
  q2[1]= q1[3]*qt[1] + q1[1]*qt[3] + q1[2]*qt[0] - q1[0]*qt[2];
  q2[2]= q1[3]*qt[2] + q1[2]*qt[3] + q1[0]*qt[1] - q1[1]*qt[0];
  q2[3]= q1[3]*qt[3] - q1[0]*qt[0] - q1[1]*qt[1] - q1[2]*qt[2];
} //eof VertexOps::MultQuat()


void VertexOps::Matrix2Euler(double m[16], double XYZ[3]) {
  double	A,B,C;

  XYZ[1]= asin(m[2]);  // Calculate Y-axis angle
  C=  cos(XYZ[1]);
  XYZ[1]= NurbanaMath::RtoD(XYZ[1]);

  if (fabs(C) > 0.005) {  // Gimball lock?
    A= m[10]/C;  // No, so get X-axis angle
    B= -m[6]/C;

    XYZ[0]= atan2(B,A);
    XYZ[0]= NurbanaMath::RtoD(XYZ[0]);

    A= m[0]/C;  // Get Z-axis angle
    B= -m[1]/C;

    XYZ[2]= atan2(B,A);
    XYZ[2]= NurbanaMath::RtoD(XYZ[2]);

  } else {  // Gimball lock has occurred

    XYZ[0]= 0;  // Set X-axis angle to zero

    A= m[5];  // And calculate Z-axis angle
    B= m[4];  

    XYZ[2]= atan2(B,A);
    XYZ[2]= NurbanaMath::RtoD(XYZ[2]);
  } //fi

  XYZ[0]= -XYZ[0];
  XYZ[1]= -XYZ[1];
  XYZ[2]= -XYZ[2];
} //eof VertexOps::Matrix2Euler()
