/** @file opennurbs_fit.cpp
 *
 * Extensions to the openNURBS library, based off of Thomas Mörwald's
 * surface fitting code in the Point Cloud Library (pcl). His code is
 # BSD licensed:
 *
 *  Copyright (c) 2011, Thomas Mörwald, Jonathan Balzer, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Thomas Mörwald or Jonathan Balzer nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "common.h"

#include <iostream>
#include <stdexcept>
#include <map>
#include <algorithm>

#include "opennurbs_fit.h"
#include "bu.h"
#include "vmath.h"


using namespace on_fit;


// ************************
// * from nurbs_tools.cpp *
// ************************

void
NurbsTools::downsample_random (const vector_vec3d &data1, vector_vec3d &data2, unsigned size)
{
    if (data1.size () <= size && size > 0)
    {
	data2 = data1;
	return;
    }

    unsigned s = data1.size ();
    data2.clear ();

    for (unsigned i = 0; i < size; i++)
    {
	unsigned rnd = unsigned (s * (double (rand ()) / RAND_MAX));
	data2.push_back (data1[rnd]);
    }
}

void
NurbsTools::downsample_random (vector_vec3d &data, unsigned size)
{
    if (data.size () <= size && size > 0)
	return;

    unsigned s = data.size ();

    vector_vec3d data_tmp;

    for (unsigned i = 0; i < size; i++)
    {
	unsigned rnd = unsigned ((s - 1) * (double (rand ()) / RAND_MAX));
	data_tmp.push_back (data[rnd]);
    }

    data = data_tmp;
}

unsigned
NurbsTools::getClosestPoint (const ON_2dPoint &p, const vector_vec2d &data)
{
    if (data.empty ())
	throw std::runtime_error ("[NurbsTools::getClosestPoint(2d)] Data empty.\n");

    unsigned idx (0);
    double dist2 (DBL_MAX);
    for (unsigned i = 0; i < data.size (); i++)
    {
	ON_2dVector v = (data[i] - p);
	double d2 = ON_DotProduct(v, v);  // Was the squaredNorm in Eigen
	if (d2 < dist2)
	{
	    idx = i;
	    dist2 = d2;
	}
    }
    return idx;
}

unsigned
NurbsTools::getClosestPoint (const ON_3dPoint &p, const vector_vec3d &data)
{
    if (data.empty ())
	throw std::runtime_error ("[NurbsTools::getClosestPoint(2d)] Data empty.\n");

    unsigned idx (0);
    double dist2 (DBL_MAX);
    for (unsigned i = 0; i < data.size (); i++)
    {
	ON_3dVector v = (data[i] - p);
	double d2 = ON_DotProduct(v, v);  // Was the squaredNorm in Eigen
	if (d2 < dist2)
	{
	    idx = i;
	    dist2 = d2;
	}
    }
    return idx;
}

unsigned
NurbsTools::getClosestPoint (const ON_2dPoint &p, const ON_2dVector &dir, const vector_vec2d &data,
			     unsigned &idxcp)
{
    if (data.empty ())
	throw std::runtime_error ("[NurbsTools::getClosestPoint(2d)] Data empty.\n");

    unsigned idx (0);
    idxcp = 0;
    double dist2 (0.0);
    double dist2cp (DBL_MAX);
    for (unsigned i = 0; i < data.size (); i++)
    {
	ON_2dVector v = (data[i] - p);
	double d2 = ON_DotProduct(v, v);

	if (d2 < dist2cp)
	{
	    idxcp = i;
	    dist2cp = d2;
	}

	if (NEAR_ZERO(d2, SMALL_FASTF))
	    return i;

	v.Unitize();

	double d1 = ON_DotProduct(dir, v);
	if (d1 / d2 > dist2)
	{
	    idx = i;
	    dist2 = d1 / d2;
	}
    }
    return idx;
}

ON_3dVector
NurbsTools::computeMean (const vector_vec3d &data)
{
    ON_3dVector u(0.0, 0.0, 0.0);

    unsigned s = data.size ();
    double ds = 1.0 / s;

    for (unsigned i = 0; i < s; i++)
	u += (data[i] * ds);

    return u;
}

ON_2dVector
NurbsTools::computeMean (const vector_vec2d &data)
{
    ON_2dVector u (0.0, 0.0);

    unsigned s = data.size ();
    double ds = 1.0 / s;

    for (unsigned i = 0; i < s; i++)
	u += (data[i] * ds);

    return u;
}

void
NurbsTools::pca (const vector_vec3d &data, ON_3dVector &mean, Eigen::Matrix3d &eigenvectors,
		 Eigen::Vector3d &eigenvalues)
{
    if (data.empty ())
    {
	printf ("[NurbsTools::pca] Error, data is empty\n");
	abort ();
    }

    mean = computeMean (data);

    unsigned s = data.size ();

    ON_Matrix Q(3, s);

    for (unsigned i = 0; i < s; i++) {
	Q[0][i] = data[i].x - mean.x;
	Q[1][i] = data[i].y - mean.y;
	Q[2][i] = data[i].z - mean.z;
    }

    ON_Matrix Qt = Q;
    Qt.Transpose();

    ON_Matrix oC;
    oC.Multiply(Q,Qt);

    Eigen::Matrix3d C(3,3);
    for (unsigned i = 0; i < 3; i++) {
	for (unsigned j = 0; j < 3; j++) {
	    C(i,j) = oC[i][j];
	}
    }

    Eigen::SelfAdjointEigenSolver < Eigen::Matrix3d > eigensolver (C);
    if (eigensolver.info () != Eigen::Success)
    {
	printf ("[NurbsTools::pca] Can not find eigenvalues.\n");
	abort ();
    }

    for (int i = 0; i < 3; ++i)
    {
	eigenvalues (i) = eigensolver.eigenvalues () (2 - i);
	if (i == 2)
	    eigenvectors.col (2) = eigenvectors.col (0).cross (eigenvectors.col (1));
	else
	    eigenvectors.col (i) = eigensolver.eigenvectors ().col (2 - i);
    }
}


// ******************************
// * from nurbs_solve_eigen.cpp *
// ******************************

void
NurbsSolve::assign (unsigned rows, unsigned cols, unsigned dims)
{
    m_Ksparse.resize(rows, cols);
    //std::cout << "m_Ksparse(rows=" << rows << ",cols=" << cols << "\n";
    m_Ksparse.setZero();
    m_xeig = new ON_Matrix(cols,dims);
    m_xeig->Zero();
    //std::cout << "m_xeig(rows=" << cols << ",cols=" << dims << "\n";
    m_feig = new ON_Matrix(rows, dims);
    m_feig->Zero();
    //std::cout << "m_feig(rows=" << rows << ",cols=" << dims << "\n";
}

void
NurbsSolve::K (unsigned i, unsigned j, double v)
{
    m_Ksparse.insert(i,j) = v;
    //std::cout << "m_Ksparse count: " << m_Ksparse.nonZeros() << "\n";
    //std::cout << "m_Ksparse item(" << i << "," << j << "):" << m_Ksparse.coeffRef(i,j) << "\n";
}
void
NurbsSolve::x (unsigned i, unsigned j, double v)
{
    (*m_xeig)[i][j] = v;
}
void
NurbsSolve::f (unsigned i, unsigned j, double v)
{
    (*m_feig)[i][j] = v;
}

double
NurbsSolve::K (unsigned i, unsigned j)
{
    return m_Ksparse.coeffRef(i,j);
}
double
NurbsSolve::x (unsigned i, unsigned j)
{
    return (*m_xeig)[i][j];
}
double
NurbsSolve::f (unsigned i, unsigned j)
{
    return (*m_feig)[i][j];
}

void
NurbsSolve::resizeF (unsigned rows)
{
    ON_Matrix *new_feig = new ON_Matrix(rows, (*m_feig).ColCount());
    for (int i = 0; i < (*m_feig).RowCount(); i++) {
	for (int j = 0; j < (*m_feig).ColCount(); j++) {
	    (*new_feig)[i][j] = (*m_feig)[i][j];
	}
    }
    delete m_feig;
    m_feig = new_feig;
}

bool
solveSparseLinearSystemLQ (Eigen::SparseMatrix<double>* A, Eigen::MatrixXd* b, Eigen::MatrixXd* x)
{
    Eigen::SparseMatrix<double> At(A->cols(), A->rows());
    Eigen::MatrixXd Atb(A->cols(), b->cols());

    At = A->transpose();
    Eigen::SparseMatrix<double> AtA = At * (*A);
    Atb = At * (*b);

    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double> > solver;
    solver.compute(AtA);
    if (solver.info()!=Eigen::Success) {
	// decomposition failed
	std::cout << "decomposition failed\n";
    }

    (*x) = solver.solve(Atb);

    if (solver.info()!=Eigen::Success) {
	std::cout << "solver failed: " << solver.info() << "\n";
	return false;
    } else {
	return true;
    }
}

bool
NurbsSolve::solve ()
{

    clock_t time_start, time_end;
    time_start = clock ();

    Eigen::MatrixXd e_m_xeig = Eigen::MatrixXd::Zero ((*m_xeig).RowCount(), (*m_xeig).ColCount());
    Eigen::MatrixXd e_m_feig = Eigen::MatrixXd::Zero ((*m_feig).RowCount(), (*m_feig).ColCount());

    for (int i = 0; i < (*m_xeig).RowCount(); i++) {
	for (int j = 0; j < (*m_xeig).ColCount(); j++) {
	    e_m_xeig (i,j) = (*m_xeig)[i][j];
	}
    }
    for (int i = 0; i < (*m_feig).RowCount(); i++) {
	for (int j = 0; j < (*m_feig).ColCount(); j++) {
	    e_m_feig (i,j) = (*m_feig)[i][j];
	}
    }

    bool success = solveSparseLinearSystemLQ (&m_Ksparse, &e_m_feig, &e_m_xeig);
    if (!success) {
	std::cout << "solver failed!\n";
	return false;
    }

    for (int i = 0; i < (*m_xeig).RowCount(); i++) {
	for (int j = 0; j < (*m_xeig).ColCount(); j++) {
	    (*m_xeig)[i][j] = e_m_xeig (i,j);
	}
    }

    time_end = clock ();

    if (!m_quiet)
    {
	double solve_time = (double)(time_end - time_start) / (double)(CLOCKS_PER_SEC);
	printf ("[NurbsSolve[Eigen::SimplicialLDLT]::solve_()] solution found! (%f sec)\n", solve_time);
    }

    return true;
}

// ********************************
// * from fitting_surface_pdm.cpp *
// ********************************

FittingSurface::FittingSurface (int order, NurbsDataSurface *in_m_data, ON_3dVector z)
{
    if (order < 2)
	throw std::runtime_error ("[FittingSurface::FittingSurface] Error order too low (order<2).");

    ON::Begin ();

    this->m_data = in_m_data;
    m_nurbs = initNurbsPCA (order, m_data, z);

    this->init ();
}

FittingSurface::FittingSurface (NurbsDataSurface *in_m_data, const ON_NurbsSurface &ns)
{
    ON::Begin ();

    this->m_nurbs = ON_NurbsSurface (ns);
    this->m_data = in_m_data;

    this->init ();
}

void
FittingSurface::refine (int dim)
{
    std::vector<double> xi;
    std::vector<double> elements = getElementVector (m_nurbs, dim);

    for (unsigned i = 0; i < elements.size () - 1; i++)
	xi.push_back (elements[i] + 0.5 * (elements[i + 1] - elements[i]));

    for (unsigned i = 0; i < xi.size (); i++)
	m_nurbs.InsertKnot (dim, xi[i], 1);

    m_elementsU = getElementVector (m_nurbs, 0);
    m_elementsV = getElementVector (m_nurbs, 1);
    m_minU = m_elementsU[0];
    m_minV = m_elementsV[0];
    m_maxU = m_elementsU[m_elementsU.size () - 1];
    m_maxV = m_elementsV[m_elementsV.size () - 1];
}

void
FittingSurface::assemble (Parameter param)
{
    clock_t time_start, time_end;
    time_start = clock ();

    int nBnd = m_data->boundary.size ();
    int nInt = m_data->interior.size ();
    int nCurInt = param.regularisation_resU * param.regularisation_resV;
    int nCurBnd = 2 * param.regularisation_resU + 2 * param.regularisation_resV;
    int nCageReg = (m_nurbs.m_cv_count[0] - 2) * (m_nurbs.m_cv_count[1] - 2);
    int nCageRegBnd = 2 * (m_nurbs.m_cv_count[0] - 1) + 2 * (m_nurbs.m_cv_count[1] - 1);

    if (param.boundary_weight <= 0.0)
	nBnd = 0;
    if (param.interior_weight <= 0.0)
	nInt = 0;
    if (param.boundary_regularisation <= 0.0)
	nCurBnd = 0;
    if (param.interior_regularisation <= 0.0)
	nCurInt = 0;
    if (param.interior_smoothness <= 0.0)
	nCageReg = 0;
    if (param.boundary_smoothness <= 0.0)
	nCageRegBnd = 0;

    int ncp = m_nurbs.m_cv_count[0] * m_nurbs.m_cv_count[1];
    int nrows = nBnd + nInt + nCurInt + nCurBnd + nCageReg + nCageRegBnd;

    m_solver.assign (nrows, ncp, 3);

    unsigned row = 0;

    // boundary points should lie on edges of surface
    if (nBnd > 0)
	assembleBoundary (param.boundary_weight, row);

    // interior points should lie on surface
    if (nInt > 0)
	assembleInterior (param.interior_weight, row);

    // minimal curvature on surface
    if (nCurInt > 0)
    {
	if (m_nurbs.Order (0) < 3 || m_nurbs.Order (1) < 3)
	    printf ("[FittingSurface::assemble] Error insufficient NURBS order to add curvature regularisation.\n");
	else
	    addInteriorRegularisation (2, param.regularisation_resU, param.regularisation_resV,
				       param.interior_regularisation / param.regularisation_resU, row);
    }

    // minimal curvature on boundary
    if (nCurBnd > 0)
    {
	if (m_nurbs.Order (0) < 3 || m_nurbs.Order (1) < 3)
	    printf ("[FittingSurface::assemble] Error insufficient NURBS order to add curvature regularisation.\n");
	else
	    addBoundaryRegularisation (2, param.regularisation_resU, param.regularisation_resV,
				       param.boundary_regularisation / param.regularisation_resU, row);
    }

    // cage regularisation
    if (nCageReg > 0)
	addCageInteriorRegularisation (param.interior_smoothness, row);

    if (nCageRegBnd > 0)
    {
	addCageBoundaryRegularisation (param.boundary_smoothness, NORTH, row);
	addCageBoundaryRegularisation (param.boundary_smoothness, SOUTH, row);
	addCageBoundaryRegularisation (param.boundary_smoothness, WEST, row);
	addCageBoundaryRegularisation (param.boundary_smoothness, EAST, row);
	addCageCornerRegularisation (param.boundary_smoothness * 2.0, row);
    }

    time_end = clock ();
    if (!m_quiet)
    {
	double solve_time = (double)(time_end - time_start) / (double)(CLOCKS_PER_SEC);
	printf ("[FittingSurface::assemble()] (assemble (%d,%d): %f sec)\n", nrows, ncp, solve_time);
    }
}

void
FittingSurface::init ()
{
    m_elementsU = getElementVector (m_nurbs, 0);
    m_elementsV = getElementVector (m_nurbs, 1);
    m_minU = m_elementsU[0];
    m_minV = m_elementsV[0];
    m_maxU = m_elementsU[m_elementsU.size () - 1];
    m_maxV = m_elementsV[m_elementsV.size () - 1];

    in_max_steps = 100;
    in_accuracy = 1e-4;

    m_quiet = true;
}

void
FittingSurface::solve (double damp)
{
    if (m_solver.solve ())
	updateSurf (damp);
}

void
FittingSurface::updateSurf (double damp)
{
    int ncp = m_nurbs.m_cv_count[0] * m_nurbs.m_cv_count[1];

    for (int A = 0; A < ncp; A++)
    {

	int I = gl2gr (A);
	int J = gl2gc (A);

	ON_3dPoint cp_prev;
	m_nurbs.GetCV (I, J, cp_prev);

	ON_3dPoint cp;
	cp.x = cp_prev.x + damp * (m_solver.x (A, 0) - cp_prev.x);
	cp.y = cp_prev.y + damp * (m_solver.x (A, 1) - cp_prev.y);
	cp.z = cp_prev.z + damp * (m_solver.x (A, 2) - cp_prev.z);

	m_nurbs.SetCV (I, J, cp);

    }

}

void
FittingSurface::setInvMapParams (unsigned max_steps, double accuracy)
{
    this->in_max_steps = max_steps;
    this->in_accuracy = accuracy;
}

std::vector<double>
FittingSurface::getElementVector (const ON_NurbsSurface &nurbs, int dim) // !
{
    std::vector<double> result;

    int idx_min = 0;
    int idx_max = nurbs.KnotCount (dim) - 1;
    if (nurbs.IsClosed (dim))
    {
	idx_min = nurbs.Order (dim) - 2;
	idx_max = nurbs.KnotCount (dim) - nurbs.Order (dim) + 1;
    }

    const double* knots = nurbs.Knot (dim);

    result.push_back (knots[idx_min]);

    //for (int E=(m_nurbs.Order(0)-2); E<(m_nurbs.KnotCount(0)-m_nurbs.Order(0)+2); E++) {
    for (int E = idx_min + 1; E <= idx_max; E++)
    {

	if (!NEAR_EQUAL(knots[E], knots[E - 1], SMALL_FASTF)) // do not count double knots
	    result.push_back (knots[E]);

    }

    return result;
}

void
FittingSurface::assembleInterior (double wInt, unsigned &row)
{
    m_data->interior_line_start.clear ();
    m_data->interior_line_end.clear ();
    m_data->interior_error.clear ();
    m_data->interior_normals.clear ();
    unsigned nInt = m_data->interior.size ();
    for (unsigned p = 0; p < nInt; p++)
    {
	ON_3dVector &pcp = m_data->interior[p];

	// inverse mapping
	ON_2dVector params;
	ON_3dPoint pt;
	ON_3dVector tu, tv, n;
	double error;
	if (p < m_data->interior_param.size ())
	{
	    params = inverseMapping (m_nurbs, pcp, m_data->interior_param[p], error, pt, tu, tv, in_max_steps, in_accuracy);
	    m_data->interior_param[p] = params;
	}
	else
	{
	    params = findClosestElementMidPoint (m_nurbs, pcp);
	    params = inverseMapping (m_nurbs, pcp, params, error, pt, tu, tv, in_max_steps, in_accuracy);
	    m_data->interior_param.push_back (params);
	}
	m_data->interior_error.push_back (error);

	n = ON_CrossProduct(tu, tv);
	n.Unitize();

	m_data->interior_normals.push_back (n);
	m_data->interior_line_start.push_back (pcp);
	m_data->interior_line_end.push_back (pt);

	double w (wInt);
	if (p < m_data->interior_weight.size ())
	    w = m_data->interior_weight[p];

	addPointConstraint (m_data->interior_param[p], m_data->interior[p], w, row);
    }
}

void
FittingSurface::assembleBoundary (double wBnd, unsigned &row)
{
    m_data->boundary_line_start.clear ();
    m_data->boundary_line_end.clear ();
    m_data->boundary_error.clear ();
    m_data->boundary_normals.clear ();
    unsigned nBnd = m_data->boundary.size ();
    for (unsigned p = 0; p < nBnd; p++)
    {
	ON_3dVector &pcp = m_data->boundary[p];

	double error;
	ON_3dPoint pt;
	ON_3dVector tu, tv, n;
	ON_2dVector params = inverseMappingBoundary (m_nurbs, pcp, error, pt, tu, tv, in_max_steps, in_accuracy);
	m_data->boundary_error.push_back (error);

	if (p < m_data->boundary_param.size ())
	{
	    m_data->boundary_param[p] = params;
	}
	else
	{
	    m_data->boundary_param.push_back (params);
	}

	n = ON_CrossProduct(tu, tv);
	n.Unitize();

	m_data->boundary_normals.push_back (n);
	m_data->boundary_line_start.push_back (pcp);
	m_data->boundary_line_end.push_back (pt);

	double w (wBnd);
	if (p < m_data->boundary_weight.size ())
	    w = m_data->boundary_weight[p];

	addPointConstraint (m_data->boundary_param[p], m_data->boundary[p], w, row);
    }
}

ON_NurbsSurface
FittingSurface::initNurbs4Corners (int order, ON_3dPoint ll, ON_3dPoint lr, ON_3dPoint ur, ON_3dPoint ul)
{
    std::map<int, std::map<int, ON_3dPoint> > cv_map;

    double dc = 1.0 / (order - 1);

    for (int i = 0; i < order; i++)
    {
	double di = dc * i;
	cv_map[i][0] = ll + (lr - ll) * di;
	cv_map[0][i] = ll + (ul - ll) * di;
	cv_map[i][order - 1] = ul + (ur - ul) * di;
	cv_map[order - 1][i] = lr + (ur - lr) * di;
    }

    for (int i = 1; i < order - 1; i++)
    {
	for (int j = 1; j < order - 1; j++)
	{
	    ON_3dPoint du = cv_map[0][j] + (cv_map[order - 1][j] - cv_map[0][j]) * dc * i;
	    ON_3dPoint dv = cv_map[i][0] + (cv_map[i][order - 1] - cv_map[i][0]) * dc * j;
	    cv_map[i][j] = du * 0.5 + dv * 0.5;
	}
    }

    ON_NurbsSurface nurbs (3, false, order, order, order, order);
    nurbs.MakeClampedUniformKnotVector (0, 1.0);
    nurbs.MakeClampedUniformKnotVector (1, 1.0);

    for (int i = 0; i < order; i++)
    {
	for (int j = 0; j < order; j++)
	{
	    nurbs.SetCV (i, j, cv_map[i][j]);
	}
    }
    return nurbs;
}

ON_NurbsSurface
FittingSurface::initNurbsPCA (int order, NurbsDataSurface *m_data, ON_3dVector z)
{
    ON_3dVector mean;
    Eigen::Matrix3d eigenvectors;
    Eigen::Vector3d eigenvalues;

    unsigned s = m_data->interior.size ();

    NurbsTools::pca (m_data->interior, mean, eigenvectors, eigenvalues);

    m_data->mean = mean;
    //m_data->eigenvectors = (*eigenvectors);

    bool flip (false);
    Eigen::Vector3d ez(z[0],z[1],z[2]);
    if (eigenvectors.col (2).dot (ez) < 0.0)
	flip = true;

    eigenvalues = eigenvalues / s; // seems that the eigenvalues are dependent on the number of points (???)

    ON_3dVector sigma(sqrt(eigenvalues[0]), sqrt(eigenvalues[1]), sqrt(eigenvalues[2]));

    ON_NurbsSurface nurbs (3, false, order, order, order, order);
    nurbs.MakeClampedUniformKnotVector (0, 1.0);
    nurbs.MakeClampedUniformKnotVector (1, 1.0);

    // +- 2 sigma -> 95,45 % aller Messwerte
    double dcu = (4.0 * sigma[0]) / (nurbs.Order (0) - 1);
    double dcv = (4.0 * sigma[1]) / (nurbs.Order (1) - 1);

    ON_3dVector cv_t, cv;
    Eigen::Vector3d ecv_t, ecv;
    Eigen::Vector3d emean(mean[0], mean[1], mean[2]);
    for (int i = 0; i < nurbs.Order (0); i++)
    {
	for (int j = 0; j < nurbs.Order (1); j++)
	{
	    cv[0] = -2.0 * sigma[0] + dcu * i;
	    cv[1] = -2.0 * sigma[1] + dcv * j;
	    cv[2] = 0.0;
	    ecv (0) = -2.0 * sigma[0] + dcu * i;
	    ecv (1) = -2.0 * sigma[1] + dcv * j;
	    ecv (2) = 0.0;
	    ecv_t = eigenvectors * ecv + emean;
	    cv_t[0] = ecv_t (0);
	    cv_t[1] = ecv_t (1);
	    cv_t[2] = ecv_t (2);
	    if (flip)
		nurbs.SetCV (nurbs.Order (0) - 1 - i, j, ON_3dPoint (cv_t[0], cv_t[1], cv_t[2]));
	    else
		nurbs.SetCV (i, j, ON_3dPoint (cv_t[0], cv_t[1], cv_t[2]));
	}
    }
    return nurbs;
}

ON_NurbsSurface
FittingSurface::initNurbsPCABoundingBox (int order, NurbsDataSurface *m_data, ON_3dVector z)
{
    ON_3dVector mean;
    Eigen::Matrix3d eigenvectors;
    Eigen::Vector3d eigenvalues;

    unsigned s = m_data->interior.size ();
    m_data->interior_param.clear ();

    NurbsTools::pca (m_data->interior, mean, eigenvectors, eigenvalues);

    m_data->mean = mean;
    //m_data->eigenvectors = (*eigenvectors);

    bool flip (false);
    Eigen::Vector3d ez(z[0],z[1],z[2]);
    if (eigenvectors.col (2).dot (ez) < 0.0)
	flip = true;

    eigenvalues = eigenvalues / s; // seems that the eigenvalues are dependent on the number of points (???)
    Eigen::Matrix3d eigenvectors_inv = eigenvectors.inverse ();

    ON_3dVector v_max(0.0, 0.0, 0.0);
    ON_3dVector v_min(DBL_MAX, DBL_MAX, DBL_MAX);
    Eigen::Vector3d emean(mean[0], mean[1], mean[2]);
    for (unsigned i = 0; i < s; i++)
    {
	Eigen::Vector3d eint(m_data->interior[i][0], m_data->interior[i][1], m_data->interior[i][2]);
	Eigen::Vector3d ep = eigenvectors_inv * (eint - emean);
	ON_3dPoint p(ep (0), ep (1), ep(2));
	m_data->interior_param.push_back (ON_2dPoint(p[0], p[1]));

	if (p[0] > v_max[0])
	    v_max[0] = p[0];
	if (p[1] > v_max[1])
	    v_max[1] = p[1];
	if (p[2] > v_max[2])
	    v_max[2] = p[2];

	if (p[0] < v_min[0])
	    v_min[0] = p[0];
	if (p[1] < v_min[1])
	    v_min[1] = p[1];
	if (p[2] < v_min[2])
	    v_min[2] = p[2];
    }

    for (unsigned i = 0; i < s; i++)
    {
	ON_2dVector &p = m_data->interior_param[i];
	if (v_max[0] > v_min[0] && v_max[0] > v_min[0])
	{
	    p[0] = (p[0] - v_min[0]) / (v_max[0] - v_min[0]);
	    p[1] = (p[1] - v_min[1]) / (v_max[1] - v_min[1]);
	}
	else
	{
	    throw std::runtime_error ("[NurbsTools::initNurbsPCABoundingBox] Error: v_max <= v_min");
	}
    }

    ON_NurbsSurface nurbs (3, false, order, order, order, order);

    nurbs.MakeClampedUniformKnotVector (0, 1.0);
    nurbs.MakeClampedUniformKnotVector (1, 1.0);

    double dcu = (v_max[0] - v_min[0]) / (nurbs.Order (0) - 1);
    double dcv = (v_max[1] - v_min[1]) / (nurbs.Order (1) - 1);

    ON_3dPoint cv_t, cv;
    Eigen::Vector3d ecv_t2, ecv2;
    Eigen::Vector3d emean2(mean[0],mean[1],mean[2]);
    for (int i = 0; i < nurbs.Order (0); i++)
    {
	for (int j = 0; j < nurbs.Order (1); j++)
	{
	    cv[0] = v_min[0] + dcu * i;
	    cv[1] = v_min[1] + dcv * j;
	    cv[2] = 0.0;
	    ecv2 (0) = cv[0];
	    ecv2 (1) = cv[1];
	    ecv2 (2) = cv[2];
	    ecv_t2 = eigenvectors * ecv2 + emean2;
	    if (flip)
		nurbs.SetCV (nurbs.Order (0) - 1 - i, j, cv_t);
	    else
		nurbs.SetCV (i, j, cv_t);
	}
    }
    return nurbs;
}

void
FittingSurface::addPointConstraint (const ON_2dVector &params, const ON_3dPoint &point, double weight,
				    unsigned &row)
{
    double *N0 = new double[m_nurbs.Order (0) * m_nurbs.Order (0)];
    double *N1 = new double[m_nurbs.Order (1) * m_nurbs.Order (1)];

    int E = ON_NurbsSpanIndex (m_nurbs.m_order[0], m_nurbs.m_cv_count[0], m_nurbs.m_knot[0], params[0], 0, 0);
    int F = ON_NurbsSpanIndex (m_nurbs.m_order[1], m_nurbs.m_cv_count[1], m_nurbs.m_knot[1], params[1], 0, 0);

    ON_EvaluateNurbsBasis (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, params[0], N0);
    ON_EvaluateNurbsBasis (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, params[1], N1);

    m_solver.f (row, 0, point[0] * weight);
    m_solver.f (row, 1, point[1] * weight);
    m_solver.f (row, 2, point[2] * weight);

    for (int i = 0; i < m_nurbs.Order (0); i++)
    {

	for (int j = 0; j < m_nurbs.Order (1); j++)
	{

	    m_solver.K (row, lrc2gl (E, F, i, j), weight * N0[i] * N1[j]);

	} // j

    } // i

    row++;

    delete [] N0;
    delete [] N1;

}

void
FittingSurface::addCageInteriorRegularisation (double weight, unsigned &row)
{
    for (int i = 1; i < (m_nurbs.m_cv_count[0] - 1); i++)
    {
	for (int j = 1; j < (m_nurbs.m_cv_count[1] - 1); j++)
	{

	    m_solver.f (row, 0, 0.0);
	    m_solver.f (row, 1, 0.0);
	    m_solver.f (row, 2, 0.0);

	    m_solver.K (row, grc2gl (i + 0, j + 0), -4.0 * weight);
	    m_solver.K (row, grc2gl (i + 0, j - 1), 1.0 * weight);
	    m_solver.K (row, grc2gl (i + 0, j + 1), 1.0 * weight);
	    m_solver.K (row, grc2gl (i - 1, j + 0), 1.0 * weight);
	    m_solver.K (row, grc2gl (i + 1, j + 0), 1.0 * weight);

	    row++;
	}
    }
}

void
FittingSurface::addCageBoundaryRegularisation (double weight, int side, unsigned &row)
{
    int i = 0;
    int j = 0;

    switch (side)
    {
	case SOUTH:
	    j = m_nurbs.m_cv_count[1] - 1;
	case NORTH:
	    for (i = 1; i < (m_nurbs.m_cv_count[0] - 1); i++)
	    {

		m_solver.f (row, 0, 0.0);
		m_solver.f (row, 1, 0.0);
		m_solver.f (row, 2, 0.0);

		m_solver.K (row, grc2gl (i + 0, j), -2.0 * weight);
		m_solver.K (row, grc2gl (i - 1, j), 1.0 * weight);
		m_solver.K (row, grc2gl (i + 1, j), 1.0 * weight);

		row++;
	    }
	    break;

	case EAST:
	    i = m_nurbs.m_cv_count[0] - 1;
	case WEST:
	    for (j = 1; j < (m_nurbs.m_cv_count[1] - 1); j++)
	    {

		m_solver.f (row, 0, 0.0);
		m_solver.f (row, 1, 0.0);
		m_solver.f (row, 2, 0.0);

		m_solver.K (row, grc2gl (i, j + 0), -2.0 * weight);
		m_solver.K (row, grc2gl (i, j - 1), 1.0 * weight);
		m_solver.K (row, grc2gl (i, j + 1), 1.0 * weight);

		row++;
	    }
	    break;
    }
}

void
FittingSurface::addCageCornerRegularisation (double weight, unsigned &row)
{
    { // NORTH-WEST
	int i = 0;
	int j = 0;

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	m_solver.K (row, grc2gl (i + 0, j + 0), -2.0 * weight);
	m_solver.K (row, grc2gl (i + 1, j + 0), 1.0 * weight);
	m_solver.K (row, grc2gl (i + 0, j + 1), 1.0 * weight);

	row++;
    }

    { // NORTH-EAST
	int i = m_nurbs.m_cv_count[0] - 1;
	int j = 0;

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	m_solver.K (row, grc2gl (i + 0, j + 0), -2.0 * weight);
	m_solver.K (row, grc2gl (i - 1, j + 0), 1.0 * weight);
	m_solver.K (row, grc2gl (i + 0, j + 1), 1.0 * weight);

	row++;
    }

    { // SOUTH-EAST
	int i = m_nurbs.m_cv_count[0] - 1;
	int j = m_nurbs.m_cv_count[1] - 1;

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	m_solver.K (row, grc2gl (i + 0, j + 0), -2.0 * weight);
	m_solver.K (row, grc2gl (i - 1, j + 0), 1.0 * weight);
	m_solver.K (row, grc2gl (i + 0, j - 1), 1.0 * weight);

	row++;
    }

    { // SOUTH-WEST
	int i = 0;
	int j = m_nurbs.m_cv_count[1] - 1;

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	m_solver.K (row, grc2gl (i + 0, j + 0), -2.0 * weight);
	m_solver.K (row, grc2gl (i + 1, j + 0), 1.0 * weight);
	m_solver.K (row, grc2gl (i + 0, j - 1), 1.0 * weight);

	row++;
    }

}


void
FittingSurface::addInteriorRegularisation (int order, int resU, int resV, double UNUSED(weight), unsigned &row)
{
    double *N0 = new double[m_nurbs.Order (0) * m_nurbs.Order (0)];
    double *N1 = new double[m_nurbs.Order (1) * m_nurbs.Order (1)];

    double dU = (m_maxU - m_minU) / resU;
    double dV = (m_maxV - m_minV) / resV;

    for (int u = 0; u < resU; u++)
    {
	for (int v = 0; v < resV; v++)
	{

	    ON_2dPoint params;
	    params[0] = m_minU + u * dU + 0.5 * dU;
	    params[1] = m_minV + v * dV + 0.5 * dV;

	    //			printf("%f %f, %f %f\n", m_minU, dU, params(0), params(1));

	    int E = ON_NurbsSpanIndex (m_nurbs.m_order[0], m_nurbs.m_cv_count[0], m_nurbs.m_knot[0], params[0], 0, 0);
	    int F = ON_NurbsSpanIndex (m_nurbs.m_order[1], m_nurbs.m_cv_count[1], m_nurbs.m_knot[1], params[1], 0, 0);

	    ON_EvaluateNurbsBasis (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, params[0], N0);
	    ON_EvaluateNurbsBasis (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, params[1], N1);
	    ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, order, N0); // derivative order?
	    ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, order, N1);

	    m_solver.f (row, 0, 0.0);
	    m_solver.f (row, 1, 0.0);
	    m_solver.f (row, 2, 0.0);

	    for (int i = 0; i < m_nurbs.Order (0); i++)
	    {

		for (int j = 0; j < m_nurbs.Order (1); j++)
		{

		    //m_solver.K (row, lrc2gl (E, F, i, j),
		    //            weight * (N0[order * m_nurbs.Order (0) + i] * N1[j] + N0[i] * N1[order * m_nurbs.Order (1) + j]));

		} // i

	    } // j

	    row++;

	}
    }
    delete [] N0;
    delete [] N1;
}

void
FittingSurface::addBoundaryRegularisation (int order, int resU, int resV, double weight, unsigned &row)
{
    double *N0 = new double[m_nurbs.Order (0) * m_nurbs.Order (0)];
    double *N1 = new double[m_nurbs.Order (1) * m_nurbs.Order (1)];

    double dU = (m_maxU - m_minU) / resU;
    double dV = (m_maxV - m_minV) / resV;

    for (int u = 0; u < resU; u++)
    {

	ON_2dPoint params;
	params[0] = m_minU + u * dU + 0.5 * dU;
	params[1] = m_minV;

	int E = ON_NurbsSpanIndex (m_nurbs.m_order[0], m_nurbs.m_cv_count[0], m_nurbs.m_knot[0], params[0], 0, 0);
	int F = ON_NurbsSpanIndex (m_nurbs.m_order[1], m_nurbs.m_cv_count[1], m_nurbs.m_knot[1], params[1], 0, 0);

	ON_EvaluateNurbsBasis (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, params[0], N0);
	ON_EvaluateNurbsBasis (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, params[1], N1);
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, order, N0); // derivative order?
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, order, N1);

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	for (int i = 0; i < m_nurbs.Order (0); i++)
	{

	    for (int j = 0; j < m_nurbs.Order (1); j++)
	    {

		m_solver.K (row, lrc2gl (E, F, i, j),
			    weight * (N0[order * m_nurbs.Order (0) + i] * N1[j] + N0[i] * N1[order * m_nurbs.Order (1) + j]));

	    } // i

	} // j

	row++;

    }

    for (int u = 0; u < resU; u++)
    {

	ON_2dPoint params;
	params[0] = m_minU + u * dU + 0.5 * dU;
	params[1] = m_maxV;

	int E = ON_NurbsSpanIndex (m_nurbs.m_order[0], m_nurbs.m_cv_count[0], m_nurbs.m_knot[0], params[0], 0, 0);
	int F = ON_NurbsSpanIndex (m_nurbs.m_order[1], m_nurbs.m_cv_count[1], m_nurbs.m_knot[1], params[1], 0, 0);

	ON_EvaluateNurbsBasis (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, params[0], N0);
	ON_EvaluateNurbsBasis (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, params[1], N1);
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, order, N0); // derivative order?
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, order, N1);

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	for (int i = 0; i < m_nurbs.Order (0); i++)
	{

	    for (int j = 0; j < m_nurbs.Order (1); j++)
	    {

		m_solver.K (row, lrc2gl (E, F, i, j),
			    weight * (N0[order * m_nurbs.Order (0) + i] * N1[j] + N0[i] * N1[order * m_nurbs.Order (1) + j]));

	    } // i

	} // j

	row++;

    }

    for (int v = 0; v < resV; v++)
    {

	ON_2dPoint params;
	params[0] = m_minU;
	params[1] = m_minV + v * dV + 0.5 * dV;

	int E = ON_NurbsSpanIndex (m_nurbs.m_order[0], m_nurbs.m_cv_count[0], m_nurbs.m_knot[0], params[0], 0, 0);
	int F = ON_NurbsSpanIndex (m_nurbs.m_order[1], m_nurbs.m_cv_count[1], m_nurbs.m_knot[1], params[1], 0, 0);

	ON_EvaluateNurbsBasis (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, params[0], N0);
	ON_EvaluateNurbsBasis (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, params[1], N1);
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, order, N0); // derivative order?
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, order, N1);

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	for (int i = 0; i < m_nurbs.Order (0); i++)
	{

	    for (int j = 0; j < m_nurbs.Order (1); j++)
	    {

		m_solver.K (row, lrc2gl (E, F, i, j),
			    weight * (N0[order * m_nurbs.Order (0) + i] * N1[j] + N0[i] * N1[order * m_nurbs.Order (1) + j]));

	    } // i

	} // j

	row++;

    }

    for (int v = 0; v < resV; v++)
    {

	ON_2dPoint params;
	params[0] = m_maxU;
	params[1] = m_minV + v * dV + 0.5 * dV;

	int E = ON_NurbsSpanIndex (m_nurbs.m_order[0], m_nurbs.m_cv_count[0], m_nurbs.m_knot[0], params[0], 0, 0);
	int F = ON_NurbsSpanIndex (m_nurbs.m_order[1], m_nurbs.m_cv_count[1], m_nurbs.m_knot[1], params[1], 0, 0);

	ON_EvaluateNurbsBasis (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, params[0], N0);
	ON_EvaluateNurbsBasis (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, params[1], N1);
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (0), m_nurbs.m_knot[0] + E, order, N0); // derivative order?
	ON_EvaluateNurbsBasisDerivatives (m_nurbs.Order (1), m_nurbs.m_knot[1] + F, order, N1);

	m_solver.f (row, 0, 0.0);
	m_solver.f (row, 1, 0.0);
	m_solver.f (row, 2, 0.0);

	for (int i = 0; i < m_nurbs.Order (0); i++)
	{

	    for (int j = 0; j < m_nurbs.Order (1); j++)
	    {

		m_solver.K (row, lrc2gl (E, F, i, j),
			    weight * (N0[order * m_nurbs.Order (0) + i] * N1[j] + N0[i] * N1[order * m_nurbs.Order (1) + j]));

	    } // i

	} // j

	row++;

    }
    delete [] N0;
    delete [] N1;
}

ON_2dPoint
FittingSurface::inverseMapping (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt, const ON_2dPoint &hint, double &error,
				ON_3dPoint &p, ON_3dVector &tu, ON_3dVector &tv, int maxSteps, double accuracy, bool quiet)
{

    double pointAndTangents[9];

    ON_2dVector current, delta;
    ON_Matrix A(2,2);
    ON_2dVector b;
    ON_3dVector r;
    std::vector<double> elementsU = getElementVector (nurbs, 0);
    std::vector<double> elementsV = getElementVector (nurbs, 1);
    double minU = elementsU[0];
    double minV = elementsV[0];
    double maxU = elementsU[elementsU.size () - 1];
    double maxV = elementsV[elementsV.size () - 1];

    current = hint;

    for (int k = 0; k < maxSteps; k++)
    {

	nurbs.Evaluate (current[0], current[1], 1, 3, pointAndTangents);
	p[0] = pointAndTangents[0];
	p[1] = pointAndTangents[1];
	p[2] = pointAndTangents[2];
	tu[0] = pointAndTangents[3];
	tu[1] = pointAndTangents[4];
	tu[2] = pointAndTangents[5];
	tv[0] = pointAndTangents[6];
	tv[1] = pointAndTangents[7];
	tv[2] = pointAndTangents[8];

	r = p - pt;

	b[0] = -ON_DotProduct(r,tu);
	b[1] = -ON_DotProduct(r,tv);

	A[0][0] = ON_DotProduct(tu,tu);
	A[0][1] = ON_DotProduct(tu,tv);
	A[1][0] = A[0][1];
	A[1][1] = ON_DotProduct(tv,tv);

	Eigen::Vector2d eb(b[0],b[1]);
	Eigen::Vector2d edelta;
	Eigen::Matrix2d eA;

	eA (0, 0) = A[0][0];
	eA (0, 1) = A[0][1];
	eA (1, 0) = A[1][0];
	eA (1, 1) = A[1][1];

	edelta = eA.ldlt ().solve (eb);

	delta[0] = edelta (0);
	delta[1] = edelta (1);

	if (sqrt(ON_DotProduct(delta,delta)) < accuracy)
	{

	    error = sqrt(ON_DotProduct(r,r));
	    return current;

	}
	else
	{
	    current = current + delta;

	    if (current[0] < minU)
		current[0] = minU;
	    else if (current[0] > maxU)
		current[0] = maxU;

	    if (current[1] < minV)
		current[1] = minV;
	    else if (current[1] > maxV)
		current[1] = maxV;

	}

    }

    error = sqrt(ON_DotProduct(r,r));
    if (!quiet)
    {
	printf ("[FittingSurface::inverseMapping] Warning: Method did not converge (%e %d)\n", accuracy, maxSteps);
	printf ("  %f %f ... %f %f\n", hint[0], hint[1], current[0], current[1]);
    }

    return current;

}

ON_2dPoint
FittingSurface::findClosestElementMidPoint (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt)
{
    ON_2dPoint hint;
    ON_3dVector r;
    std::vector<double> elementsU = getElementVector (nurbs, 0);
    std::vector<double> elementsV = getElementVector (nurbs, 1);

    double d_shortest (DBL_MAX);
    for (unsigned i = 0; i < elementsU.size () - 1; i++)
    {
	for (unsigned j = 0; j < elementsV.size () - 1; j++)
	{
	    double points[3];
	    double d;

	    double xi = elementsU[i] + 0.5 * (elementsU[i + 1] - elementsU[i]);
	    double eta = elementsV[j] + 0.5 * (elementsV[j + 1] - elementsV[j]);

	    nurbs.Evaluate (xi, eta, 0, 3, points);
	    r[0] = points[0] - pt[0];
	    r[1] = points[1] - pt[1];
	    r[2] = points[2] - pt[2];

	    d = ON_DotProduct(r,r);

	    if ((i == 0 && j == 0) || d < d_shortest)
	    {
		d_shortest = d;
		hint[0] = xi;
		hint[1] = eta;
	    }
	}
    }

    return hint;
}

ON_2dPoint
FittingSurface::inverseMappingBoundary (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt, double &error, ON_3dPoint &p,
					ON_3dVector &tu, ON_3dVector &tv, int maxSteps, double accuracy, bool quiet)
{

    ON_2dPoint result;
    double min_err = 100.0;
    std::vector<myvec> ini_points;
    double err_tmp;
    ON_3dPoint p_tmp;
    ON_3dVector tu_tmp, tv_tmp;

    std::vector<double> elementsU = getElementVector (nurbs, 0);
    std::vector<double> elementsV = getElementVector (nurbs, 1);

    // NORTH - SOUTH
    for (unsigned i = 0; i < (elementsV.size () - 1); i++)
    {
	ini_points.push_back (myvec (WEST, elementsV[i] + 0.5 * (elementsV[i + 1] - elementsV[i])));
	ini_points.push_back (myvec (EAST, elementsV[i] + 0.5 * (elementsV[i + 1] - elementsV[i])));
    }

    // WEST - EAST
    for (unsigned i = 0; i < (elementsU.size () - 1); i++)
    {
	ini_points.push_back (myvec (NORTH, elementsU[i] + 0.5 * (elementsU[i + 1] - elementsU[i])));
	ini_points.push_back (myvec (SOUTH, elementsU[i] + 0.5 * (elementsU[i + 1] - elementsU[i])));
    }

    for (unsigned i = 0; i < ini_points.size (); i++)
    {

	ON_2dPoint params = inverseMappingBoundary (nurbs, pt, ini_points[i].side, ini_points[i].hint, err_tmp, p_tmp,
						    tu_tmp, tv_tmp, maxSteps, accuracy, quiet);

	if (i == 0 || err_tmp < min_err)
	{
	    min_err = err_tmp;
	    result = params;
	    p = p_tmp;
	    tu = tu_tmp;
	    tv = tv_tmp;
	}
    }

    error = min_err;
    return result;

}

ON_2dPoint
FittingSurface::inverseMappingBoundary (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt, int side, double hint,
					double &error, ON_3dPoint &p, ON_3dVector &tu, ON_3dVector &tv, int maxSteps,
					double accuracy, bool quiet)
{

    double pointAndTangents[9];
    double current, delta;
    ON_3dVector r, t;
    ON_2dPoint params;

    current = hint;

    std::vector<double> elementsU = getElementVector (nurbs, 0);
    std::vector<double> elementsV = getElementVector (nurbs, 1);
    double minU = elementsU[0];
    double minV = elementsV[0];
    double maxU = elementsU[elementsU.size () - 1];
    double maxV = elementsV[elementsV.size () - 1];

    for (int k = 0; k < maxSteps; k++)
    {

	switch (side)
	{

	    case WEST:

		params[0] = minU;
		params[1] = current;
		nurbs.Evaluate (minU, current, 1, 3, pointAndTangents);
		p[0] = pointAndTangents[0];
		p[1] = pointAndTangents[1];
		p[2] = pointAndTangents[2];
		tu[0] = pointAndTangents[3];
		tu[1] = pointAndTangents[4];
		tu[2] = pointAndTangents[5];
		tv[0] = pointAndTangents[6];
		tv[1] = pointAndTangents[7];
		tv[2] = pointAndTangents[8];

		t = tv; // use tv

		break;
	    case SOUTH:

		params[0] = current;
		params[1] = maxV;
		nurbs.Evaluate (current, maxV, 1, 3, pointAndTangents);
		p[0] = pointAndTangents[0];
		p[1] = pointAndTangents[1];
		p[2] = pointAndTangents[2];
		tu[0] = pointAndTangents[3];
		tu[1] = pointAndTangents[4];
		tu[2] = pointAndTangents[5];
		tv[0] = pointAndTangents[6];
		tv[1] = pointAndTangents[7];
		tv[2] = pointAndTangents[8];

		t = tu; // use tu

		break;
	    case EAST:

		params[0] = maxU;
		params[1] = current;
		nurbs.Evaluate (maxU, current, 1, 3, pointAndTangents);
		p[0] = pointAndTangents[0];
		p[1] = pointAndTangents[1];
		p[2] = pointAndTangents[2];
		tu[0] = pointAndTangents[3];
		tu[1] = pointAndTangents[4];
		tu[2] = pointAndTangents[5];
		tv[0] = pointAndTangents[6];
		tv[1] = pointAndTangents[7];
		tv[2] = pointAndTangents[8];

		t = tv; // use tv

		break;
	    case NORTH:

		params[0] = current;
		params[1] = minV;
		nurbs.Evaluate (current, minV, 1, 3, pointAndTangents);
		p[0] = pointAndTangents[0];
		p[1] = pointAndTangents[1];
		p[2] = pointAndTangents[2];
		tu[0] = pointAndTangents[3];
		tu[1] = pointAndTangents[4];
		tu[2] = pointAndTangents[5];
		tv[0] = pointAndTangents[6];
		tv[1] = pointAndTangents[7];
		tv[2] = pointAndTangents[8];

		t = tu; // use tu

		break;
	    default:
		throw std::runtime_error ("[FittingSurface::inverseMappingBoundary] ERROR: Specify a boundary!");

	}

	r[0] = pointAndTangents[0] - pt[0];
	r[1] = pointAndTangents[1] - pt[1];
	r[2] = pointAndTangents[2] - pt[2];

	delta = -0.5 * ON_DotProduct(r,t) / ON_DotProduct(t,t);

	if (fabs (delta) < accuracy)
	{

	    error = sqrt(ON_DotProduct(r,r));
	    return params;

	}
	else
	{

	    current = current + delta;

	    bool stop = false;

	    switch (side)
	    {

		case WEST:
		case EAST:
		    if (current < minV)
		    {
			params[1] = minV;
			stop = true;
		    }
		    else if (current > maxV)
		    {
			params[1] = maxV;
			stop = true;
		    }

		    break;

		case NORTH:
		case SOUTH:
		    if (current < minU)
		    {
			params[0] = minU;
			stop = true;
		    }
		    else if (current > maxU)
		    {
			params[0] = maxU;
			stop = true;
		    }

		    break;
	    }

	    if (stop)
	    {
		error = sqrt(ON_DotProduct(r,r));
		return params;
	    }

	}

    }

    error = sqrt(ON_DotProduct(r,r));
    if (!quiet)
	printf (
	    "[FittingSurface::inverseMappingBoundary] Warning: Method did not converge! (residual: %f, delta: %f, params: %f %f)\n",
	    error, delta, params[0], params[1]);

    return params;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * coding: utf-8
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
