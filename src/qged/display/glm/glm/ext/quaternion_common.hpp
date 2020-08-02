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

/// @ref ext_quaternion_common
/// @file glm/ext/quaternion_common.hpp
///
/// @defgroup ext_quaternion_common GLM_EXT_quaternion_common
/// @ingroup ext
///
/// Provides common functions for quaternion types
///
/// Include <glm/ext/quaternion_common.hpp> to use the features of this extension.
///
/// @see ext_scalar_common
/// @see ext_vector_common
/// @see ext_quaternion_float
/// @see ext_quaternion_double
/// @see ext_quaternion_exponential
/// @see ext_quaternion_geometric
/// @see ext_quaternion_relational
/// @see ext_quaternion_trigonometric
/// @see ext_quaternion_transform

#pragma once

// Dependency:
#include "../ext/scalar_constants.hpp"
#include "../ext/quaternion_geometric.hpp"
#include "../common.hpp"
#include "../trigonometric.hpp"
#include "../exponential.hpp"
#include <limits>

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_quaternion_common extension included")
#endif

namespace glm
{
	/// @addtogroup ext_quaternion_common
	/// @{

	/// Spherical linear interpolation of two quaternions.
	/// The interpolation is oriented and the rotation is performed at constant speed.
	/// For short path spherical linear interpolation, use the slerp function.
	///
	/// @param x A quaternion
	/// @param y A quaternion
	/// @param a Interpolation factor. The interpolation is defined beyond the range [0, 1].
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	///
	/// @see - slerp(qua<T, Q> const& x, qua<T, Q> const& y, T const& a)
	template<typename T, qualifier Q>
	GLM_FUNC_DECL qua<T, Q> mix(qua<T, Q> const& x, qua<T, Q> const& y, T a);

	/// Linear interpolation of two quaternions.
	/// The interpolation is oriented.
	///
	/// @param x A quaternion
	/// @param y A quaternion
	/// @param a Interpolation factor. The interpolation is defined in the range [0, 1].
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	GLM_FUNC_DECL qua<T, Q> lerp(qua<T, Q> const& x, qua<T, Q> const& y, T a);

	/// Spherical linear interpolation of two quaternions.
	/// The interpolation always take the short path and the rotation is performed at constant speed.
	///
	/// @param x A quaternion
	/// @param y A quaternion
	/// @param a Interpolation factor. The interpolation is defined beyond the range [0, 1].
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	GLM_FUNC_DECL qua<T, Q> slerp(qua<T, Q> const& x, qua<T, Q> const& y, T a);

    /// Spherical linear interpolation of two quaternions with multiple spins over rotation axis.
    /// The interpolation always take the short path when the spin count is positive and long path
    /// when count is negative. Rotation is performed at constant speed.
    ///
    /// @param x A quaternion
    /// @param y A quaternion
    /// @param a Interpolation factor. The interpolation is defined beyond the range [0, 1].
    /// @param k Additional spin count. If Value is negative interpolation will be on "long" path.
    ///
    /// @tparam T A floating-point scalar type
    /// @tparam S An integer scalar type
    /// @tparam Q A value from qualifier enum
    template<typename T, typename S, qualifier Q>
    GLM_FUNC_DECL qua<T, Q> slerp(qua<T, Q> const& x, qua<T, Q> const& y, T a, S k);

	/// Returns the q conjugate.
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	GLM_FUNC_DECL qua<T, Q> conjugate(qua<T, Q> const& q);

	/// Returns the q inverse.
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	GLM_FUNC_DECL qua<T, Q> inverse(qua<T, Q> const& q);

	/// Returns true if x holds a NaN (not a number)
	/// representation in the underlying implementation's set of
	/// floating point representations. Returns false otherwise,
	/// including for implementations with no NaN
	/// representations.
	///
	/// /!\ When using compiler fast math, this function may fail.
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<4, bool, Q> isnan(qua<T, Q> const& x);

	/// Returns true if x holds a positive infinity or negative
	/// infinity representation in the underlying implementation's
	/// set of floating point representations. Returns false
	/// otherwise, including for implementations with no infinity
	/// representations.
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<4, bool, Q> isinf(qua<T, Q> const& x);

	/// @}
} //namespace glm

#include "quaternion_common.inl"
