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

/// @ref gtx_matrix_operation
/// @file glm/gtx/matrix_operation.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_matrix_operation GLM_GTX_matrix_operation
/// @ingroup gtx
///
/// Include <glm/gtx/matrix_operation.hpp> to use the features of this extension.
///
/// Build diagonal matrices from vectors.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_matrix_operation is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_matrix_operation extension included")
#	endif
#endif

namespace glm
{
	/// @addtogroup gtx_matrix_operation
	/// @{

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<2, 2, T, Q> diagonal2x2(
		vec<2, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<2, 3, T, Q> diagonal2x3(
		vec<2, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<2, 4, T, Q> diagonal2x4(
		vec<2, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<3, 2, T, Q> diagonal3x2(
		vec<2, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<3, 3, T, Q> diagonal3x3(
		vec<3, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<3, 4, T, Q> diagonal3x4(
		vec<3, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<4, 2, T, Q> diagonal4x2(
		vec<2, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<4, 3, T, Q> diagonal4x3(
		vec<3, T, Q> const& v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<4, 4, T, Q> diagonal4x4(
		vec<4, T, Q> const& v);

	/// Build an adjugate  matrix.
	/// From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<2, 2, T, Q> adjugate(mat<2, 2, T, Q> const& m);

	/// Build an adjugate  matrix.
	/// From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<3, 3, T, Q> adjugate(mat<3, 3, T, Q> const& m);

	/// Build an adjugate  matrix.
	/// From GLM_GTX_matrix_operation extension.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL mat<4, 4, T, Q> adjugate(mat<4, 4, T, Q> const& m);

	/// @}
}//namespace glm

#include "matrix_operation.inl"
