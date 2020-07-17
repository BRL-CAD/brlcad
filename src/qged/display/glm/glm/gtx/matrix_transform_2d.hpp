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

/// @ref gtx_matrix_transform_2d
/// @file glm/gtx/matrix_transform_2d.hpp
/// @author Miguel Ángel Pérez Martínez
///
/// @see core (dependence)
///
/// @defgroup gtx_matrix_transform_2d GLM_GTX_matrix_transform_2d
/// @ingroup gtx
///
/// Include <glm/gtx/matrix_transform_2d.hpp> to use the features of this extension.
///
/// Defines functions that generate common 2d transformation matrices.

#pragma once

// Dependency:
#include "../mat3x3.hpp"
#include "../vec2.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_matrix_transform_2d is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_matrix_transform_2d extension included")
#	endif
#endif

namespace glm
{
	/// @addtogroup gtx_matrix_transform_2d
	/// @{

	/// Builds a translation 3 * 3 matrix created from a vector of 2 components.
	///
	/// @param m Input matrix multiplied by this translation matrix.
	/// @param v Coordinates of a translation vector.
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER mat<3, 3, T, Q> translate(
		mat<3, 3, T, Q> const& m,
		vec<2, T, Q> const& v);

	/// Builds a rotation 3 * 3 matrix created from an angle.
	///
	/// @param m Input matrix multiplied by this translation matrix.
	/// @param angle Rotation angle expressed in radians.
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER mat<3, 3, T, Q> rotate(
		mat<3, 3, T, Q> const& m,
		T angle);

	/// Builds a scale 3 * 3 matrix created from a vector of 2 components.
	///
	/// @param m Input matrix multiplied by this translation matrix.
	/// @param v Coordinates of a scale vector.
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER mat<3, 3, T, Q> scale(
		mat<3, 3, T, Q> const& m,
		vec<2, T, Q> const& v);

	/// Builds an horizontal (parallel to the x axis) shear 3 * 3 matrix.
	///
	/// @param m Input matrix multiplied by this translation matrix.
	/// @param y Shear factor.
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER mat<3, 3, T, Q> shearX(
		mat<3, 3, T, Q> const& m,
		T y);

	/// Builds a vertical (parallel to the y axis) shear 3 * 3 matrix.
	///
	/// @param m Input matrix multiplied by this translation matrix.
	/// @param x Shear factor.
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER mat<3, 3, T, Q> shearY(
		mat<3, 3, T, Q> const& m,
		T x);

	/// @}
}//namespace glm

#include "matrix_transform_2d.inl"
