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

/// @ref gtc_ulp
/// @file glm/gtc/ulp.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_ulp GLM_GTC_ulp
/// @ingroup gtc
///
/// Include <glm/gtc/ulp.hpp> to use the features of this extension.
///
/// Allow the measurement of the accuracy of a function against a reference
/// implementation. This extension works on floating-point data and provide results
/// in ULP.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../detail/_vectorize.hpp"
#include "../ext/scalar_int_sized.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_ulp extension included")
#endif

namespace glm
{
	/// Return the next ULP value(s) after the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see gtc_ulp
	template<typename genType>
	GLM_FUNC_DECL genType next_float(genType x);

	/// Return the previous ULP value(s) before the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see gtc_ulp
	template<typename genType>
	GLM_FUNC_DECL genType prev_float(genType x);

	/// Return the value(s) ULP distance after the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see gtc_ulp
	template<typename genType>
	GLM_FUNC_DECL genType next_float(genType x, int ULPs);

	/// Return the value(s) ULP distance before the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see gtc_ulp
	template<typename genType>
	GLM_FUNC_DECL genType prev_float(genType x, int ULPs);

	/// Return the distance in the number of ULP between 2 single-precision floating-point scalars.
	///
	/// @see gtc_ulp
	GLM_FUNC_DECL int float_distance(float x, float y);

	/// Return the distance in the number of ULP between 2 double-precision floating-point scalars.
	///
	/// @see gtc_ulp
	GLM_FUNC_DECL int64 float_distance(double x, double y);

	/// Return the next ULP value(s) after the input value(s).
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> next_float(vec<L, T, Q> const& x);

	/// Return the value(s) ULP distance after the input value(s).
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> next_float(vec<L, T, Q> const& x, int ULPs);

	/// Return the value(s) ULP distance after the input value(s).
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> next_float(vec<L, T, Q> const& x, vec<L, int, Q> const& ULPs);

	/// Return the previous ULP value(s) before the input value(s).
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> prev_float(vec<L, T, Q> const& x);

	/// Return the value(s) ULP distance before the input value(s).
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> prev_float(vec<L, T, Q> const& x, int ULPs);

	/// Return the value(s) ULP distance before the input value(s).
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> prev_float(vec<L, T, Q> const& x, vec<L, int, Q> const& ULPs);

	/// Return the distance in the number of ULP between 2 single-precision floating-point scalars.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, int, Q> float_distance(vec<L, float, Q> const& x, vec<L, float, Q> const& y);

	/// Return the distance in the number of ULP between 2 double-precision floating-point scalars.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam Q Value from qualifier enum
	///
	/// @see gtc_ulp
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, int64, Q> float_distance(vec<L, double, Q> const& x, vec<L, double, Q> const& y);

	/// @}
}//namespace glm

#include "ulp.inl"
