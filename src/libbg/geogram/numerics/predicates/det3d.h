/*
 *  Copyright (c) 2000-2023 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

/* Automatically generated code, do not edit */
/* Generated from source file: det3d.pck */

inline int det_3d_filter( const double* p0, const double* p1, const double* p2) {
    double Delta;
    Delta = (((p0[0] * ((p1[1] * p2[2]) - (p1[2] * p2[1]))) - (p1[0] * ((p0[1] * p2[2]) - (p0[2] * p2[1])))) + (p2[0] * ((p0[1] * p1[2]) - (p0[2] * p1[1]))));
    int int_tmp_result;
    double eps;
    double max1 = fabs(p0[0]);
    if( (max1 < fabs(p1[0])) )
    {
        max1 = fabs(p1[0]);
    }
    if( (max1 < fabs(p2[0])) )
    {
        max1 = fabs(p2[0]);
    }
    double max2 = fabs(p0[1]);
    if( (max2 < fabs(p0[2])) )
    {
        max2 = fabs(p0[2]);
    }
    if( (max2 < fabs(p1[1])) )
    {
        max2 = fabs(p1[1]);
    }
    if( (max2 < fabs(p1[2])) )
    {
        max2 = fabs(p1[2]);
    }
    double max3 = fabs(p1[1]);
    if( (max3 < fabs(p1[2])) )
    {
        max3 = fabs(p1[2]);
    }
    if( (max3 < fabs(p2[1])) )
    {
        max3 = fabs(p2[1]);
    }
    if( (max3 < fabs(p2[2])) )
    {
        max3 = fabs(p2[2]);
    }
    double lower_bound_1;
    double upper_bound_1;
    lower_bound_1 = max1;
    upper_bound_1 = max1;
    if( (max2 < lower_bound_1) )
    {
        lower_bound_1 = max2;
    }
    else
    {
        if( (max2 > upper_bound_1) )
        {
            upper_bound_1 = max2;
        }
    }
    if( (max3 < lower_bound_1) )
    {
        lower_bound_1 = max3;
    }
    else
    {
        if( (max3 > upper_bound_1) )
        {
            upper_bound_1 = max3;
        }
    }
    if( (lower_bound_1 < 1.92663387981871579179e-98) )
    {
        return FPG_UNCERTAIN_VALUE;
    }
    else
    {
        if( (upper_bound_1 > 1.11987237108890185662e+102) )
        {
            return FPG_UNCERTAIN_VALUE;
        }
        eps = (3.11133555671680765034e-15 * ((max2 * max3) * max1));
        if( (Delta > eps) )
        {
            int_tmp_result = 1;
        }
        else
        {
            if( (Delta < -eps) )
            {
                int_tmp_result = -1;
            }
            else
            {
                return FPG_UNCERTAIN_VALUE;
            }
        }
    }
    return int_tmp_result;
}
