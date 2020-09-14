/*
 * OpenGL Mathematics (GLM)
 * --------------------------------------------------------------------------------
 * GLM is licensed under The Happy Bunny License or MIT License
 *
 * BRL-CAD will use glm under the standard MIT license
 *
 * ================================================================================
 * The MIT License
 * --------------------------------------------------------------------------------
 * Copyright (c) 2005 - G-Truc Creation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/// @ref gtx_matrix_factorisation
/// @file glm/gtx/matrix_factorisation.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_matrix_factorisation GLM_GTX_matrix_factorisation
/// @ingroup gtx
///
/// Include <glm/gtx/matrix_factorisation.hpp> to use the features of this extension.
///
/// Functions to factor matrices in various forms

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_matrix_factorisation is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_matrix_factorisation extension included")
#	endif
#endif

/*
Suggestions:
 - Move helper functions flipud and fliplr to another file: They may be helpful in more general circumstances.
 - Implement other types of matrix factorisation, such as: QL and LQ, L(D)U, eigendecompositions, etc...
*/

namespace glm
{
	/// @addtogroup gtx_matrix_factorisation
	/// @{

	/// Flips the matrix rows up and down.
	///
	/// From GLM_GTX_matrix_factorisation extension.
	template <length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL mat<C, R, T, Q> flipud(mat<C, R, T, Q> const& in);

	/// Flips the matrix columns right and left.
	///
	/// From GLM_GTX_matrix_factorisation extension.
	template <length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL mat<C, R, T, Q> fliplr(mat<C, R, T, Q> const& in);

	/// Performs QR factorisation of a matrix.
	/// Returns 2 matrices, q and r, such that the columns of q are orthonormal and span the same subspace than those of the input matrix, r is an upper triangular matrix, and q*r=in.
	/// Given an n-by-m input matrix, q has dimensions min(n,m)-by-m, and r has dimensions n-by-min(n,m).
	///
	/// From GLM_GTX_matrix_factorisation extension.
	template <length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL void qr_decompose(mat<C, R, T, Q> const& in, mat<(C < R ? C : R), R, T, Q>& q, mat<C, (C < R ? C : R), T, Q>& r);

	/// Performs RQ factorisation of a matrix.
	/// Returns 2 matrices, r and q, such that r is an upper triangular matrix, the rows of q are orthonormal and span the same subspace than those of the input matrix, and r*q=in.
	/// Note that in the context of RQ factorisation, the diagonal is seen as starting in the lower-right corner of the matrix, instead of the usual upper-left.
	/// Given an n-by-m input matrix, r has dimensions min(n,m)-by-m, and q has dimensions n-by-min(n,m).
	///
	/// From GLM_GTX_matrix_factorisation extension.
	template <length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL void rq_decompose(mat<C, R, T, Q> const& in, mat<(C < R ? C : R), R, T, Q>& r, mat<C, (C < R ? C : R), T, Q>& q);

	/// @}
}

#include "matrix_factorisation.inl"
