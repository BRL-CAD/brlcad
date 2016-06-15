/** @File opennurbs_fit.h
 *
 * Extensions to the openNURBS library, based off of Thomas Mörwald's
 * surface fitting code in the Point Cloud Library (pcl). His code is
 * BSD licensed:
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

#ifndef LIBBREP_OPENNURBS_FIT_H
#define LIBBREP_OPENNURBS_FIT_H

#include "opennurbs.h"
#include <vector>
#include <list>
#include <map>
#include <stdio.h>

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC shadow warnings via pragma due to Eigen headers..."
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Eigen headers..."
#  pragma message "Disabling GCC inline failure warnings via pragma due to Eigen headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Winline"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Winline"
#endif
#undef Success
#include <Eigen/StdVector>
#undef Success
#include <Eigen/Dense>
#undef Success
#include <Eigen/Sparse>
#undef Success
#include <Eigen/SparseCholesky>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

namespace on_fit
{

// **********************
// * from nurbs_data.h *
// **********************

typedef std::vector<ON_2dVector > vector_vec2d;
typedef std::vector<ON_3dVector > vector_vec3d;

/** \brief Data structure for NURBS surface fitting
 * (FittingSurface, FittingSurfaceTDM, FittingCylinder, GlobalOptimization, GlobalOptimizationTDM) */
struct NurbsDataSurface
{
  ON_Matrix *eigenvectors;
  ON_3dVector mean;

  vector_vec3d interior; ///<< input
  std::vector<double> interior_weight; ///<< input
  std::vector<double> interior_error; ///>> output
  vector_vec2d interior_param; ///>> output
  vector_vec3d interior_line_start; ///>> output
  vector_vec3d interior_line_end; ///>> output
  vector_vec3d interior_normals; ///>> output

  vector_vec3d boundary; ///<< input
  std::vector<double> boundary_weight; ///<< input
  std::vector<double> boundary_error; ///>> output
  vector_vec2d boundary_param; ///>> output
  vector_vec3d boundary_line_start; ///>> output
  vector_vec3d boundary_line_end; ///>> output
  vector_vec3d boundary_normals; ///>> output

  vector_vec3d common_boundary_point;
  std::vector<unsigned> common_boundary_idx;
  vector_vec2d common_boundary_param;

  /** \brief Clear all interior data */
  inline void
  clear_interior ()
  {
    interior.clear ();
    interior_weight.clear ();
    interior_error.clear ();
    interior_param.clear ();
    interior_line_start.clear ();
    interior_line_end.clear ();
    interior_normals.clear ();
  }

  /** \brief Clear all boundary data */
  inline void
  clear_boundary ()
  {
    boundary.clear ();
    boundary_weight.clear ();
    boundary_error.clear ();
    boundary_param.clear ();
    boundary_line_start.clear ();
    boundary_line_end.clear ();
    boundary_normals.clear ();
  }

  /** \brief Clear all common data */
  inline void
  clear_common_boundary ()
  {
    common_boundary_point.clear ();
    common_boundary_idx.clear ();
    common_boundary_param.clear ();
  }

};

// **********************
// * from nurbs_tools.h *
// **********************
enum
{
  NORTH = 1, NORTHEAST = 2, EAST = 3, SOUTHEAST = 4, SOUTH = 5, SOUTHWEST = 6, WEST = 7, NORTHWEST = 8
};

/** \brief Some useful tools for initialization, point search, ... */
class NurbsTools
{
public:

  /** \brief Get the closest point with respect to 'point'
   *  \param[in] point The point to which the closest point is searched for.
   *  \param[in] data Vector containing the set of points for searching. */
  static unsigned
  getClosestPoint (const ON_2dPoint &point, const vector_vec2d &data);

  /** \brief Get the closest point with respect to 'point'
   *  \param[in] point The point to which the closest point is searched for.
   *  \param[in] data Vector containing the set of points for searching. */
  static unsigned
  getClosestPoint (const ON_3dPoint &point, const vector_vec3d &data);

  /** \brief Get the closest point with respect to 'point' in Non-Euclidean metric
   *  \brief Related paper: TODO
   *  \param[in] point The point to which the closest point is searched for.
   *  \param[in] dir The direction defining 'inside' and 'outside'
   *  \param[in] data Vector containing the set of points for searching.
   *  \param[out] idxcp Closest point with respect to Euclidean metric. */
  static unsigned
  getClosestPoint (const ON_2dPoint &point, const ON_2dVector &dir, const vector_vec2d &data,
		   unsigned &idxcp);

  /** \brief Compute the mean of a set of points
   *  \param[in] data Set of points.     */
  static ON_3dVector
  computeMean (const vector_vec3d &data);
  /** \brief Compute the mean of a set of points
   *  \param[in] data Set of points.     */
  static ON_2dVector
  computeMean (const vector_vec2d &data);

  /** \brief PCA - principal-component-analysis
   *  \param[in] data Set of points.
   *  \param[out] mean The mean of the set of points.
   *  \param[out] eigenvectors Matrix containing column-wise the eigenvectors of the set of points.
   *  \param[out] eigenvalues The eigenvalues of the set of points with respect to the eigenvectors. */
  static void
      pca (const vector_vec3d &data, ON_3dVector &mean, Eigen::Matrix3d &eigenvectors,
	   Eigen::Vector3d &eigenvalues);

  /** \brief Downsample data points to a certain size.
   *  \param[in] data1 The original set of points.
   *  \param[out] data2 The downsampled set of points of size 'size'.
   *  \param[in] size The desired size of the resulting set of points.       */
  static void
  downsample_random (const vector_vec3d &data1, vector_vec3d &data2, unsigned size);
  /** \brief Downsample data points to a certain size.
   *  \param[in/out] data1 The set of points for downsampling;
   *  will be replaced by the resulting set of points of size 'size'.
   *  \param[in] size The desired size of the resulting set of points.       */
  static void
  downsample_random (vector_vec3d &data1, unsigned size);

};

// **********************
// * from nurbs_solve.h *
// **********************

/** \brief Solving the linear system of equations using Eigen or UmfPack.
 * (can be defined in on_nurbs.cmake)*/
class NurbsSolve
{
public:
  /** \brief Empty constructor */
  NurbsSolve () :
    m_quiet (true)
  {
  }

  /** \brief Assign size and dimension (2D, 3D) of system of equations. */
  void
  assign (unsigned rows, unsigned cols, unsigned dims);

  /** \brief Set value for system matrix K (stiffness matrix, basis functions) */
  void
  K (unsigned i, unsigned j, double v);
  /** \brief Set value for state vector x (control points) */
  void
  x (unsigned i, unsigned j, double v);
  /** \brief Set value for target vector f (force vector) */
  void
  f (unsigned i, unsigned j, double v);

  /** \brief Get value for system matrix K (stiffness matrix, basis functions) */
  double
  K (unsigned i, unsigned j);
  /** \brief Get value for state vector x (control points) */
  double
  x (unsigned i, unsigned j);
  /** \brief Get value for target vector f (force vector) */
  double
  f (unsigned i, unsigned j);

  /** \brief Resize target vector f (force vector) */
  void
  resizeF (unsigned rows);

  /** \brief Print system matrix K (stiffness matrix, basis functions) */
  void
  printK ();
  /** \brief Print state vector x (control points) */
  void
  printX ();
  /** \brief Print target vector f (force vector) */
  void
  printF ();

  /** \brief Solves the system of equations with respect to x.
   *  - Using UmfPack incredibly speeds up this function.      */
  bool
  solve ();

  /** \brief Compute the difference between solution K*x and target f */
  ON_Matrix *diff ();

  /** \brief Enable/Disable debug outputs in console. */
  inline void
  setQuiet (bool val)
  {
    m_quiet = val;
  }

private:
  bool m_quiet;
  Eigen::SparseMatrix<double> m_Ksparse;
  ON_Matrix *m_xeig;
  ON_Matrix *m_feig;

};


// ******************************
// * from fitting_surface_pdm.h *
// ******************************

/** \brief Fitting a B-Spline surface to 3D point-clouds using point-distance-minimization
 *  Based on paper: TODO
 * \author Thomas Moerwald
 * \ingroup surface     */
class FittingSurface
{
public:
  ON_NurbsSurface m_nurbs;
  NurbsDataSurface *m_data;

  class myvec
  {
  public:
    int side;
    double hint;
    myvec (int in_side, double in_hint)
    {
      this->side = in_side;
      this->hint = in_hint;
    }
  };

  /** \brief Parameters for fitting */
  struct Parameter
  {
    double interior_weight;
    double interior_smoothness;
    double interior_regularisation;

    double boundary_weight;
    double boundary_smoothness;
    double boundary_regularisation;

    unsigned regularisation_resU;
    unsigned regularisation_resV;

    Parameter (double intW = 1.0, double intS = 0.000001, double intR = 0.0, double bndW = 1.0,
	       double bndS = 0.000001, double bndR = 0.0, unsigned regU = 0, unsigned regV = 0) :
      interior_weight (intW), interior_smoothness (intS), interior_regularisation (intR), boundary_weight (bndW),
	  boundary_smoothness (bndS), boundary_regularisation (bndR), regularisation_resU (regU),
	  regularisation_resV (regV)
    {

    }

  };

  /** \brief Constructor initializing with the B-Spline surface given in argument 2.
   * \param[in] data pointer to the 3D point-cloud data to be fit.
   * \param[in] ns B-Spline surface used for fitting.        */
  FittingSurface (NurbsDataSurface *data, const ON_NurbsSurface &ns);

  /** \brief Constructor initializing B-Spline surface using initNurbsPCA(...).
   * \param[in] order the polynomial order of the B-Spline surface.
   * \param[in] data pointer to the 2D point-cloud data to be fit.
   * \param[in] z vector defining front face of surface.        */
  FittingSurface (int order, NurbsDataSurface *data, ON_3dVector z = ON_3dVector(0.0, 0.0, 1.0));

  /** \brief Refines surface by inserting a knot in the middle of each element.
   *  \param[in] dim dimension of refinement (0,1)  */
  void
  refine (int dim);

  /** \brief Assemble the system of equations for fitting
   * - for large point-clouds this is time consuming.
   * - should be done once before refinement to initialize the starting points for point inversion. */
  virtual void
  assemble (Parameter param = Parameter ());

  /** \brief Solve system of equations using Eigen or UmfPack (can be defined in on_nurbs.cmake),
   *  and updates B-Spline surface if a solution can be obtained. */
  virtual void
  solve (double damp = 1.0);

  /** \brief Update surface according to the current system of equations.
   *  \param[in] damp damping factor from one iteration to the other. */
  virtual void
  updateSurf (double damp);

  /** \brief Set parameters for inverse mapping
   *  \param[in] in_max_steps maximum number of iterations.
   *  \param[in] in_accuracy stops iteration if specified accuracy is reached. */
  void
  setInvMapParams (unsigned in_max_steps, double in_accuracy);

  /** \brief Get the elements of a B-Spline surface.*/
  static std::vector<double>
  getElementVector (const ON_NurbsSurface &nurbs, int dim);

  /** \brief Inverse mapping / point inversion: Given a point pt, this function finds the closest
   * point on the B-Spline surface using Newtons method and point-distance (L2-, Euclidean norm).
   *  \param[in] nurbs the B-Spline surface.
   *  \param[in] pt the point to which the closest point on the surface will be computed.
   *  \param[in] hint the starting point in parametric domain (warning: may lead to convergence at local minima).
   *  \param[in] error the distance between the point pt and p after convergence.
   *  \param[in] p closest point on surface.
   *  \param[in] tu the tangent vector at point p in u-direction.
   *  \param[in] tv the tangent vector at point p in v-direction.
   *  \param[in] maxSteps maximum number of iterations.
   *  \param[in] accuracy convergence criteria: if error is lower than accuracy the function returns
   *  \return closest point on surface in parametric domain.*/
  static ON_2dPoint
  inverseMapping (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt, const ON_2dPoint &hint,
		  double &error, ON_3dPoint &p, ON_3dVector &tu, ON_3dVector &tv, int maxSteps = 100,
		  double accuracy = 1e-6, bool quiet = true);

  /** \brief Given a point pt, the function finds the closest midpoint of the elements of the surface.
   *  \param[in] nurbs the B-Spline surface.
   *  \param[in] pt the point to which the closest midpoint of the elements will be computed.
   *  return closest midpoint in parametric domain. */
  static ON_2dPoint
  findClosestElementMidPoint (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt);

  /** \brief Inverse mapping / point inversion: Given a point pt, this function finds the closest
   * point on the boundary of the B-Spline surface using Newtons method and point-distance (L2-, Euclidean norm).
   *  \param[in] nurbs the B-Spline surface.
   *  \param[in] pt the point to which the closest point on the surface will be computed.
   *  \param[in] error the distance between the point pt and p after convergence.
   *  \param[in] p closest boundary point on surface.
   *  \param[in] tu the tangent vector at point p in u-direction.
   *  \param[in] tv the tangent vector at point p in v-direction.
   *  \param[in] maxSteps maximum number of iterations.
   *  \param[in] accuracy convergence criteria: if error is lower than accuracy the function returns
   *  \return closest point on surface in parametric domain.*/
  static ON_2dPoint
  inverseMappingBoundary (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt, double &error,
			  ON_3dPoint &p, ON_3dVector &tu, ON_3dVector &tv, int maxSteps = 100,
			  double accuracy = 1e-6, bool quiet = true);

  /** \brief Inverse mapping / point inversion: Given a point pt, this function finds the closest
   * point on one side of the boundary of the B-Spline surface using Newtons method and
   * point-distance (L2-, Euclidean norm).
   *  \param[in] nurbs the B-Spline surface.
   *  \param[in] pt the point to which the closest point on the surface will be computed.
   *  \param[in] side the side of the boundary (NORTH, SOUTH, EAST, WEST)
   *  \param[in] hint the starting point in parametric domain (warning: may lead to convergence at local minima).
   *  \param[in] error the distance between the point pt and p after convergence.
   *  \param[in] p closest boundary point on surface.
   *  \param[in] tu the tangent vector at point p in u-direction.
   *  \param[in] tv the tangent vector at point p in v-direction.
   *  \param[in] maxSteps maximum number of iterations.
   *  \param[in] accuracy convergence criteria: if error is lower than accuracy the function returns
   *  \return closest point on surface in parametric domain.*/
  static ON_2dPoint
  inverseMappingBoundary (const ON_NurbsSurface &nurbs, const ON_3dPoint &pt, int side, double hint,
			  double &error, ON_3dPoint &p, ON_3dVector &tu, ON_3dVector &tv,
			  int maxSteps = 100, double accuracy = 1e-6, bool quiet = true);

  /** \brief Initializing a B-Spline surface using 4 corners */
  static ON_NurbsSurface
  initNurbs4Corners (int order, ON_3dPoint ll, ON_3dPoint lr, ON_3dPoint ur, ON_3dPoint ul);

  /** \brief Initializing a B-Spline surface using principal-component-analysis and eigen values */
  static ON_NurbsSurface
  initNurbsPCA (int order, NurbsDataSurface *data, ON_3dVector z = ON_3dVector(0.0, 0.0, 1.0));

  /** \brief Initializing a B-Spline surface using principal-component-analysis and bounding box of points */
  static ON_NurbsSurface
  initNurbsPCABoundingBox (int order, NurbsDataSurface *data, ON_3dVector z = ON_3dVector(0.0, 0.0, 1.0));

  /** \brief Enable/Disable debug outputs in console. */
  inline void
  setQuiet (bool val)
  {
    m_quiet = val;
    m_solver.setQuiet(val);
  }

protected:

  /** \brief Initialisation of member variables */
  void
  init ();

  /** \brief Assemble point-to-surface constraints for interior points. */
  virtual void
  assembleInterior (double wInt, unsigned &row);

  /** \brief Assemble point-to-surface constraints for boundary points. */
  virtual void
  assembleBoundary (double wBnd, unsigned &row);

  /** \brief Add minimization constraint: point-to-surface distance (point-distance-minimization). */
  virtual void
  addPointConstraint (const ON_2dVector &params, const ON_3dPoint &point, double weight, unsigned &row);
  //  void addBoundaryPointConstraint(double paramU, double paramV, double weight, unsigned &row);

  /** \brief Add minimization constraint: interior smoothness by control point regularisation. */
  virtual void
  addCageInteriorRegularisation (double weight, unsigned &row);

  /** \brief Add minimization constraint: boundary smoothness by control point regularisation. */
  virtual void
  addCageBoundaryRegularisation (double weight, int side, unsigned &row);

  /** \brief Add minimization constraint: corner smoothness by control point regularisation. */
  virtual void
  addCageCornerRegularisation (double weight, unsigned &row);

  /** \brief Add minimization constraint: interior smoothness by derivatives regularisation. */
  virtual void
  addInteriorRegularisation (int order, int resU, int resV, double weight, unsigned &row);

  /** \brief Add minimization constraint: boundary smoothness by derivatives regularisation. */
  virtual void
  addBoundaryRegularisation (int order, int resU, int resV, double weight, unsigned &row);

  NurbsSolve m_solver;

  bool m_quiet;

  std::vector<double> m_elementsU;
  std::vector<double> m_elementsV;

  double m_minU;
  double m_minV;
  double m_maxU;
  double m_maxV;

  int in_max_steps;
  double in_accuracy;

  // index routines
  int
  grc2gl (int I, int J)
  {
    return m_nurbs.CVCount (1) * I + J;
  } // global row/col index to global lexicographic index
  int
  lrc2gl (int E, int F, int i, int j)
  {
    return grc2gl (E + i, F + j);
  } // local row/col index to global lexicographic index
  int
  gl2gr (int A)
  {
    return (int)(A / m_nurbs.CVCount (1));
  } // global lexicographic in global row index
  int
  gl2gc (int A)
  {
    return (int)(A % m_nurbs.CVCount (1));
  } // global lexicographic in global col index
};

}
#endif

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
