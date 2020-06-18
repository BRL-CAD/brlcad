/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef POLARDECOMPOSITION_H
#define POLARDECOMPOSITION_H

#include "btMatrix3x3.h"

/**
 * This class is used to compute the polar decomposition of a matrix. In
 * general, the polar decomposition factorizes a matrix, A, into two parts: a
 * unitary matrix (U) and a positive, semi-definite Hermitian matrix (H).
 * However, in this particular implementation the original matrix, A, is
 * required to be a square 3x3 matrix with real elements. This means that U will
 * be an orthogonal matrix and H with be a positive-definite, symmetric matrix.
 */
class btPolarDecomposition
{
  public:
    static const btScalar DEFAULT_TOLERANCE;
    static const unsigned int DEFAULT_MAX_ITERATIONS;

    /**
     * Creates an instance with optional parameters.
     *
     * @param tolerance     - the tolerance used to determine convergence of the
     *                        algorithm
     * @param maxIterations - the maximum number of iterations used to achieve
     *                        convergence
     */
    btPolarDecomposition(btScalar tolerance = DEFAULT_TOLERANCE, 
      unsigned int maxIterations = DEFAULT_MAX_ITERATIONS);

    /**
     * Decomposes a matrix into orthogonal and symmetric, positive-definite
     * parts. If the number of iterations returned by this function is equal to
     * the maximum number of iterations, the algorithm has failed to converge.
     *
     * @param a - the original matrix
     * @param u - the resulting orthogonal matrix
     * @param h - the resulting symmetric matrix
     *
     * @return the number of iterations performed by the algorithm.
     */
    unsigned int decompose(const btMatrix3x3& a, btMatrix3x3& u, btMatrix3x3& h) const; 

    /**
     * Returns the maximum number of iterations that this algorithm will perform
     * to achieve convergence.
     *
     * @return maximum number of iterations
     */
    unsigned int maxIterations() const;

  private:
    btScalar m_tolerance;
    unsigned int m_maxIterations;
};

/**
 * This functions decomposes the matrix 'a' into two parts: an orthogonal matrix
 * 'u' and a symmetric, positive-definite matrix 'h'. If the number of
 * iterations returned by this function is equal to
 * btPolarDecomposition::DEFAULT_MAX_ITERATIONS, the algorithm has failed to
 * converge.
 *
 * @param a - the original matrix
 * @param u - the resulting orthogonal matrix
 * @param h - the resulting symmetric matrix
 *
 * @return the number of iterations performed by the algorithm.
 */
unsigned int polarDecompose(const btMatrix3x3& a, btMatrix3x3& u, btMatrix3x3& h); 

#endif // POLARDECOMPOSITION_H

