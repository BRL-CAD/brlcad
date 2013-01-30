/*    L I B N U R B S _ S U R F A C E T R E E U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file libnurbs_surfacetreeutil.cpp
 *
 * Brief description
 *
 */

#include <vector>
#include <iostream>

#include "opennurbs.h"

int ON_Surface_Patch_Trimmed(const ON_Surface *srf, ON_CurveTree *m_ctree, ON_Interval *u_val, ON_Interval *v_val) 
{
    return 0;
}

int ON_Surface_Flat_Straight(ON_SimpleArray<ON_Plane> *frames, std::vector<int> *frame_index, double flatness_threshold, double straightness_threshold) {
    double Fdot = 1.0;
    double Sdot = 1.0;
    // This uses 0 and 1 only - bug?
    if ((Sdot = Sdot * ON_DotProduct(frames->At(frame_index->at(0))->xaxis, frames->At(frame_index->at(1))->xaxis)) < straightness_threshold) {
	std::cout << "Straightness: " << Sdot << "\n";
	return 1;
    }
    for(int i=0; i<8; i++) {
	for( int j=i+1; j<9; j++) {
	    if ((Fdot = Fdot * ON_DotProduct(frames->At(frame_index->at(i))->zaxis, frames->At(frame_index->at(j))->zaxis)) < flatness_threshold) {
		std::cout << "Flatness: " << Fdot << "\n";
		return 1;
	    }
	}
    }
    return 0;
}

// Directional flatness functions
int ON_Surface_Flat_U(ON_SimpleArray<ON_Plane> *frames, std::vector<int> *f_ind, double flatness_threshold)
{
    // check surface normals in U direction
    double Ndot = 1.0;
    if ((Ndot=frames->At(f_ind->at(0))->zaxis * frames->At(f_ind->at(1))->zaxis) < flatness_threshold) {
	return 1;
    } else if ((Ndot=Ndot * frames->At(f_ind->at(2))->zaxis * frames->At(f_ind->at(3))->zaxis) < flatness_threshold) {
	return 1;
    } else if ((Ndot=Ndot * frames->At(f_ind->at(5))->zaxis * frames->At(f_ind->at(7))->zaxis) < flatness_threshold) {
	return 1;
    } else if ((Ndot=Ndot * frames->At(f_ind->at(6))->zaxis * frames->At(f_ind->at(8))->zaxis) < flatness_threshold) {
	return 1;
    }

    // check for U twist within plane
    double Xdot = 1.0;
    if ((Xdot=frames->At(f_ind->at(0))->xaxis * frames->At(f_ind->at(1))->xaxis) < flatness_threshold) {
	return 1;
    } else if ((Xdot=Xdot * frames->At(f_ind->at(2))->xaxis * frames->At(f_ind->at(3))->xaxis) < flatness_threshold) {
	return 1;
    } else if ((Xdot=Xdot * frames->At(f_ind->at(5))->xaxis * frames->At(f_ind->at(7))->xaxis) < flatness_threshold) {
	return 1;
    } else if ((Xdot=Xdot * frames->At(f_ind->at(6))->xaxis * frames->At(f_ind->at(8))->xaxis) < flatness_threshold) {
	return 1;
    }

    return 0;
}

int ON_Surface_Flat_V(ON_SimpleArray<ON_Plane> *frames, std::vector<int> *f_ind, double flatness_threshold)
{
    // check surface normals in V direction
    double Ndot = 1.0;
    if ((Ndot=frames->At(f_ind->at(0))->zaxis * frames->At(f_ind->at(3))->zaxis) < flatness_threshold) {
	return 1;
    } else if ((Ndot=Ndot * frames->At(f_ind->at(1))->zaxis * frames->At(f_ind->at(2))->zaxis) < flatness_threshold) {
	return 1;
    } else if ((Ndot=Ndot * frames->At(f_ind->at(5))->zaxis * frames->At(f_ind->at(6))->zaxis) < flatness_threshold) {
	return 1;
    } else if ((Ndot=Ndot * frames->At(f_ind->at(7))->zaxis * frames->At(f_ind->at(8))->zaxis) < flatness_threshold) {
	return 1;
    }

    // check for V twist within plane
    double Xdot = 1.0;
    if ((Xdot=frames->At(f_ind->at(0))->xaxis * frames->At(f_ind->at(3))->xaxis) < flatness_threshold) {
	return 1;
    } else if ((Xdot=Xdot * frames->At(f_ind->at(1))->xaxis * frames->At(f_ind->at(2))->xaxis) < flatness_threshold) {
	return 1;
    } else if ((Xdot=Xdot * frames->At(f_ind->at(5))->xaxis * frames->At(f_ind->at(6))->xaxis) < flatness_threshold) {
	return 1;
    } else if ((Xdot=Xdot * frames->At(f_ind->at(7))->xaxis * frames->At(f_ind->at(8))->xaxis) < flatness_threshold) {
	return 1;
    }

    return 0;
}


// Check the ratio of the surface's "flattened" width to height
int ON_Surface_Width_vs_Height(const ON_Surface *srf, double ratio, int *u_split, int *v_split)
{
    double width = 0;
    double height = 0;
    int split = 0;
    double l_ratio = 0;
    srf->GetSurfaceSize(&width, &height);
    (ratio > 1.0) ? (l_ratio = ratio) : (l_ratio = 1/ratio);
    if (width/height > l_ratio) {
	(*u_split) += 1;
	split++;
    }
     if (height/width > l_ratio) {
	(*v_split) += 1;
	split++;
    }
    return split;
}


// Check an individual domain and knot array for a split knot
int ON_Surface_Find_Split_Knot(ON_Interval *val, int kcnt, double *knots, double *mid) 
{
    int split = 0;
    double midpt_dist = val->Length();

    for (int k_ind = 1; k_ind <= kcnt; k_ind++) {
	if (val->Includes(knots[k_ind], true)) {
	    double d_to_midpt = fabs(val->Mid()-knots[k_ind]);
	    if (d_to_midpt < midpt_dist) {
		(*mid) = knots[k_ind];
		midpt_dist = fabs(val->Mid()-knots[k_ind]);
		split++;
	    }
	}
    }

    int retval = (split == 0) ? (0) : (1);
    return retval;
}

// Figure out whether knot splitting is needed, and if so prepare values
int ON_Surface_Knots_Split(ON_Interval *u_val, ON_Interval *v_val, int u_kcnt, int v_kcnt, double *u_knots, double *v_knots, double *u_mid, double *v_mid) 
{
    int usplit = ON_Surface_Find_Split_Knot(u_val, u_kcnt, u_knots, u_mid);
    int vsplit = ON_Surface_Find_Split_Knot(v_val, v_kcnt, v_knots, v_mid);

    if (usplit && !vsplit) (*v_mid) = v_val->Mid();
    if (!usplit && vsplit) (*u_mid) = u_val->Mid();

    int retval = (usplit || vsplit) ? (1) : (0);
    return retval;
}

// Prepare frames according to the standard node layout
int ON_Populate_Frame_Array(const ON_Surface *srf, ON_Interval *u_domain, ON_Interval *v_domain, double umid, double vmid, ON_SimpleArray<ON_Plane> *frames, std::vector<int> *frame_index) {
    ON_Plane frame;
    if (frame_index->size() < 9) return -1;
    double uqmin = fabs(umid - u_domain->Min())*0.25;
    double uqmax = fabs(u_domain->Max() - umid)*0.25;
    double vqmin = fabs(vmid - v_domain->Min())*0.25;
    double vqmax = fabs(v_domain->Max() - vmid)*0.25;
    if (frame_index->at(0) == -1) {
	srf->FrameAt(u_domain->Min(), v_domain->Min(), frame);
	frames->Append(frame);
	frame_index->at(0) = frames->Count() - 1;    
    }
    if (frame_index->at(1) == -1) {
	srf->FrameAt(u_domain->Max(), v_domain->Min(), frame);
	frames->Append(frame);
	frame_index->at(1) = frames->Count() - 1;    
    }
    if (frame_index->at(2) == -1) {
	srf->FrameAt(u_domain->Max(), v_domain->Max(), frame);
	frames->Append(frame);
	frame_index->at(2) = frames->Count() - 1;    
    }
    if (frame_index->at(3) == -1) {
	srf->FrameAt(u_domain->Min(), v_domain->Max(), frame);
	frames->Append(frame);
	frame_index->at(3) = frames->Count() - 1;    
    }
    if (frame_index->at(4) == -1) {
	srf->FrameAt(umid, vmid, frame);
	frames->Append(frame);
	frame_index->at(4) = frames->Count() - 1;    
    }
    if (frame_index->at(5) == -1) {
	srf->FrameAt(umid - uqmin, vmid - vqmin, frame);
	frames->Append(frame);
	frame_index->at(5) = frames->Count() - 1;    
    }
    if (frame_index->at(6) == -1) {
	srf->FrameAt(umid - uqmin, vmid + vqmax, frame);
	frames->Append(frame);
	frame_index->at(6) = frames->Count() - 1;    
    }
    if (frame_index->at(7) == -1) {
	srf->FrameAt(umid + uqmax, vmid - vqmin, frame); 
	frames->Append(frame);
	frame_index->at(7) = frames->Count() - 1;    
    }
    if (frame_index->at(8) == -1) {
	srf->FrameAt(umid + uqmax, vmid + vqmax, frame);
	frames->Append(frame);
	frame_index->at(8) = frames->Count() - 1;    
    }
    return 0;
}

// Prepare frames according to the standard node layout
int ON_Extend_Frame_Array(const ON_Surface *srf, ON_Interval *u_domain, ON_Interval *v_domain, double umid, double vmid, ON_SimpleArray<ON_Plane> *frames, std::vector<int> *frame_index) {
    ON_Plane frame;
    if (frame_index->size() < 13) return -1;
    if (frame_index->at(9) == -1) {
	srf->FrameAt(umid, v_domain->Min(), frame);
	frames->Append(frame);
	frame_index->at(9) = frames->Count() - 1;    
    }
    if (frame_index->at(10) == -1) {
	srf->FrameAt(u_domain->Min(), vmid, frame);
	frames->Append(frame);
	frame_index->at(10) = frames->Count() - 1;    
    }
    if (frame_index->at(11) == -1) {
	srf->FrameAt(umid, v_domain->Max(), frame);
	frames->Append(frame);
	frame_index->at(11) = frames->Count() - 1;    
    }
    if (frame_index->at(12) == -1) {
	srf->FrameAt(u_domain->Max(), vmid, frame);
	frames->Append(frame);
	frame_index->at(12) = frames->Count() - 1;    
    }
    return 0;
}

// For any pre-existing surface passed as one of the t* args, this is a no-op
void ON_Surface_Create_Scratch_Surfaces(
	ON_Surface **t1,
	ON_Surface **t2,
	ON_Surface **t3,
	ON_Surface **t4)
{
    if (!(*t1)) {
	ON_NurbsSurface *nt1 = ON_NurbsSurface::New();
	(*t1)= (ON_Surface *)(nt1);
    }
    if (!(*t2)) {
	ON_NurbsSurface *nt2 = ON_NurbsSurface::New();
	(*t2)= (ON_Surface *)(nt2);
    }
    if (!(*t3)) {
	ON_NurbsSurface *nt3 = ON_NurbsSurface::New();
	(*t3)= (ON_Surface *)(nt3);
    }
    if (!(*t4)) {
	ON_NurbsSurface *nt4 = ON_NurbsSurface::New();
	(*t4)= (ON_Surface *)(nt4);
    }
}


// Given a surface and UV intervals, return a NURBS surface corresponding to
// that subset of the patch.  If t1-t3 and result are supplied externally,
// they are re-used - if not, local versions must be created and destroyed.
// The latter usage will have a malloc overhead penalty.
//
// Returns true if splits successful (or if none were needed), false if one 
// or more splits failed
bool ON_Surface_SubSurface(
	const ON_Surface *srf, 
	ON_Surface **result,
	ON_Interval *u_val,
	ON_Interval *v_val,
	ON_Surface **t1,
	ON_Surface **t2,
	ON_Surface **t3,
	ON_Surface **t4)
{
    bool split;
    int t1_del, t2_del, t3_del;

    // Make sure we have intervals with non-zero lengths
    if ((u_val->Length() <= ON_ZERO_TOLERANCE) || (v_val->Length() <= ON_ZERO_TOLERANCE))
	return false;  

    // If we have the original surface domain, just return true 
    if ((fabs(u_val->Min() - srf->Domain(0).Min()) <= ON_ZERO_TOLERANCE) &&
        (fabs(u_val->Max() - srf->Domain(0).Max()) <= ON_ZERO_TOLERANCE) && 
        (fabs(v_val->Min() - srf->Domain(1).Min()) <= ON_ZERO_TOLERANCE) &&
	(fabs(v_val->Max() - srf->Domain(1).Max()) <= ON_ZERO_TOLERANCE)) {
        (*result) = (ON_Surface *)srf;
	return true;
    }
    t1_del = (*t1) ? (0) : (1);
    t2_del = (*t2) ? (0) : (1);
    t3_del = (*t3) ? (0) : (1);
    ON_Surface *ssplit = (ON_Surface *)srf;
    ON_Surface_Create_Scratch_Surfaces(t1, t2, t3, t4); 
    if (fabs(u_val->Min() - srf->Domain(0).Min()) > ON_ZERO_TOLERANCE) {
	split = ssplit->Split(0, u_val->Min(), *t1, *t2);
	ssplit = *t2;
    }
    if ((fabs(u_val->Max() - srf->Domain(0).Max()) > ON_ZERO_TOLERANCE) && split) {
	split = ssplit->Split(0, u_val->Max(), *t1, *t3);
	ssplit = *t1;
    }
    if ((fabs(v_val->Min() - srf->Domain(1).Min()) > ON_ZERO_TOLERANCE) && split) {
	split = ssplit->Split(1, v_val->Min(), *t2, *t3);
        ssplit = *t3;
    }
    if ((fabs(v_val->Max() - srf->Domain(1).Max()) > ON_ZERO_TOLERANCE) && split) {
	split = ssplit->Split(1, v_val->Max(), *t4, *t2);
        (*result) = *t4;
    }
    if (t1_del) delete *t1;
    if (t2_del) delete *t2;
    if (t3_del) delete *t3;
    return split;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
