/* *****************************************************************************
 *
 * Copyright (c) 2014 Alexis Naveros. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * *****************************************************************************
 */

#define M3D_VectorZero(x) ({(x)[0]=0.0;(x)[1]=0.0;(x)[2]=0.0;})
#define M3D_VectorMagnitude(x) (sqrt((x)[0]*(x)[0]+(x)[1]*(x)[1]+(x)[2]*(x)[2]))
#define M3D_VectorCopy(x,y) ({(x)[0]=(y)[0];(x)[1]=(y)[1];(x)[2]=(y)[2];})
#define M3D_VectorAdd(x,y) (x)[0]+=(y)[0];(x)[1]+=(y)[1];(x)[2]+=(y)[2]
#define M3D_VectorAddStore(x,y,z) (x)[0]=(y)[0]+(z)[0];(x)[1]=(y)[1]+(z)[1];(x)[2]=(y)[2]+(z)[2]
#define M3D_VectorAddMulScalar(x,y,z) (x)[0]+=(y)[0]*(z);(x)[1]+=(y)[1]*(z);(x)[2]+=(y)[2]*(z)
#define M3D_VectorAddMul(x,y,z) (x)[0]+=(y)[0]*(z)[0];(x)[1]+=(y)[1]*(z)[1];(x)[2]+=(y)[2]*(z)[2]
#define M3D_VectorSub(x,y) (x)[0]-=(y)[0];(x)[1]-=(y)[1];(x)[2]-=(y)[2]
#define M3D_VectorSubStore(x,y,z) (x)[0]=(y)[0]-(z)[0];(x)[1]=(y)[1]-(z)[1];(x)[2]=(y)[2]-(z)[2]
#define M3D_VectorSubMulScalar(x,y,z) (x)[0]-=(y)[0]*(z);(x)[1]-=(y)[1]*(z);(x)[2]-=(y)[2]*(z)
#define M3D_VectorSubMul(x,y,z) (x)[0]-=(y)[0]*(z)[0];(x)[1]-=(y)[1]*(z)[1];(x)[2]-=(y)[2]*(z)[2]
#define M3D_VectorMul(x,y) (x)[0]*=(y)[0];(x)[1]*=(y)[1];(x)[2]*=(y)[2]
#define M3D_VectorMulStore(x,y,z) (x)[0]=(y)[0]*(z)[0];(x)[1]=(y)[1]*(z)[1];(x)[2]=(y)[2]*(z)[2]
#define M3D_VectorMulScalar(x,y) (x)[0]*=(y);(x)[1]*=(y);(x)[2]*=(y)
#define M3D_VectorMulScalarStore(x,y,z) (x)[0]=(y)[0]*(z);(x)[1]=(y)[1]*(z);(x)[2]=(y)[2]*(z)
#define M3D_VectorDiv(x,y) (x)[0]/=(y)[0];(x)[1]/=(y)[1];(x)[2]/=(y)[2]
#define M3D_VectorDivStore(x,y,z) (x)[0]=(y)[0]/(z)[0];(x)[1]=(y)[1]/(z)[1];(x)[2]=(y)[2]/(z)[2]
#define M3D_VectorDotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define M3D_PlanePoint(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2]+(x)[3])
#define M3D_VectorNormalize(x) ({typeof(*(x)) _f;_f=1.0/sqrt((x)[0]*(x)[0]+(x)[1]*(x)[1]+(x)[2]*(x)[2]);(x)[0]*=_f;(x)[1]*=_f;(x)[2]*=_f;})

#define M3D_VectorCrossProduct(x,y,z) ({(x)[0]=((y)[1]*(z)[2])-((y)[2]*(z)[1]);(x)[1]=((y)[2]*(z)[0])-((y)[0]*(z)[2]);(x)[2]=((y)[0]*(z)[1])-((y)[1]*(z)[0]);})
#define M3D_VectorAddCrossProduct(x,y,z) ({(x)[0]+=((y)[1]*(z)[2])-((y)[2]*(z)[1]);(x)[1]+=((y)[2]*(z)[0])-((y)[0]*(z)[2]);(x)[2]+=((y)[0]*(z)[1])-((y)[1]*(z)[0]);})
#define M3D_VectorSubCrossProduct(x,y,z) ({(x)[0]-=((y)[1]*(z)[2])-((y)[2]*(z)[1]);(x)[1]-=((y)[2]*(z)[0])-((y)[0]*(z)[2]);(x)[2]-=((y)[0]*(z)[1])-((y)[1]*(z)[0]);})

#define M3D_MatrixMulVector(d,p,m) ({ \
(d)[0]=(p)[0]*(m)[0*4+0]+(p)[1]*(m)[1*4+0]+(p)[2]*(m)[2*4+0]; \
(d)[1]=(p)[0]*(m)[0*4+1]+(p)[1]*(m)[1*4+1]+(p)[2]*(m)[2*4+1]; \
(d)[2]=(p)[0]*(m)[0*4+2]+(p)[1]*(m)[1*4+2]+(p)[2]*(m)[2*4+2]; })

#define M3D_MatrixTransMulVector(d,p,m) ({ \
(d)[0]=(p)[0]*(m)[0*4+0]+(p)[1]*(m)[0*4+1]+(p)[2]*(m)[0*4+2]; \
(d)[1]=(p)[0]*(m)[1*4+0]+(p)[1]*(m)[1*4+1]+(p)[2]*(m)[1*4+2]; \
(d)[2]=(p)[0]*(m)[2*4+0]+(p)[1]*(m)[2*4+1]+(p)[2]*(m)[2*4+2]; })

#define M3D_MatrixMulVectorSelf(p,m) ({ \
typeof(*(p)) _n[3] = { p[0], p[1], p[2] }; \
(p)[0]=(_n)[0]*(m)[0*4+0]+(_n)[1]*(m)[1*4+0]+(_n)[2]*(m)[2*4+0]; \
(p)[1]=(_n)[0]*(m)[0*4+1]+(_n)[1]*(m)[1*4+1]+(_n)[2]*(m)[2*4+1]; \
(p)[2]=(_n)[0]*(m)[0*4+2]+(_n)[1]*(m)[1*4+2]+(_n)[2]*(m)[2*4+2]; })

#define M3D_MatrixMulPoint(d,p,m) ({ \
(d)[0]=(p)[0]*(m)[0*4+0]+(p)[1]*(m)[1*4+0]+(p)[2]*(m)[2*4+0]+(m)[3*4+0]; \
(d)[1]=(p)[0]*(m)[0*4+1]+(p)[1]*(m)[1*4+1]+(p)[2]*(m)[2*4+1]+(m)[3*4+1]; \
(d)[2]=(p)[0]*(m)[0*4+2]+(p)[1]*(m)[1*4+2]+(p)[2]*(m)[2*4+2]+(m)[3*4+2]; })

#define M3D_MatrixMulPointSelf(p,m) ({ \
typeof(*(p)) _n[3] = { p[0], p[1], p[2] }; \
(p)[0]=(_n)[0]*(m)[0*4+0]+(_n)[1]*(m)[1*4+0]+(_n)[2]*(m)[2*4+0]+(m)[3*4+0]; \
(p)[1]=(_n)[0]*(m)[0*4+1]+(_n)[1]*(m)[1*4+1]+(_n)[2]*(m)[2*4+1]+(m)[3*4+1]; \
(p)[2]=(_n)[0]*(m)[0*4+2]+(_n)[1]*(m)[1*4+2]+(_n)[2]*(m)[2*4+2]+(m)[3*4+2]; })

