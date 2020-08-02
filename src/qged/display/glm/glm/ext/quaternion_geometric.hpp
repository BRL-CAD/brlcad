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

/// @ref ext_quaternion_geometric
/// @file glm/ext/quaternion_geometric.hpp
///
/// @defgroup ext_quaternion_geometric GLM_EXT_quaternion_geometric
/// @ingroup ext
///
/// Provides geometric functions for quaternion types
///
/// Include <glm/ext/quaternion_geometric.hpp> to use the features of this extension.
///
/// @see core_geometric
/// @see ext_quaternion_float
/// @see ext_quaternion_double

#pragma once

// Dependency:
#include "../geometric.hpp"
#include "../exponential.hpp"
#include "../ext/vector_relational.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_quaternion_geometric extension included")
#endif

namespace glm
{
	/// @addtogroup ext_quaternion_geometric
	/// @{

	/// Returns the norm of a quaternions
	///
	/// @tparam T Floating-point scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_quaternion_geometric
	template<typename T, qualifier Q>
	GLM_FUNC_DECL T length(qua<T, Q> const& q);

	/// Returns the normalized quaternion.
	///
	/// @tparam T Floating-point scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_quaternion_geometric
	template<typename T, qualifier Q>
	GLM_FUNC_DECL qua<T, Q> normalize(qua<T, Q> const& q);

	/// Returns dot product of q1 and q2, i.e., q1[0] * q2[0] + q1[1] * q2[1] + ...
	///
	/// @tparam T Floating-point scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_quaternion_geometric
	template<typename T, qualifier Q>
	GLM_FUNC_DECL T dot(qua<T, Q> const& x, qua<T, Q> const& y);

	/// Compute a cross product.
	///
	/// @tparam T Floating-point scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_quaternion_geometric
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER qua<T, Q> cross(qua<T, Q> const& q1, qua<T, Q> const& q2);

	/// @}
} //namespace glm

#include "quaternion_geometric.inl"
