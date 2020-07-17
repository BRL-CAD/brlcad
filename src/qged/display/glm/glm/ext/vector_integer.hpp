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

/// @ref ext_vector_integer
/// @file glm/ext/vector_integer.hpp
///
/// @see core (dependence)
/// @see ext_vector_integer (dependence)
///
/// @defgroup ext_vector_integer GLM_EXT_vector_integer
/// @ingroup ext
///
/// Include <glm/ext/vector_integer.hpp> to use the features of this extension.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../detail/_vectorize.hpp"
#include "../vector_relational.hpp"
#include "../common.hpp"
#include <limits>

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_vector_integer extension included")
#endif

namespace glm
{
	/// @addtogroup ext_vector_integer
	/// @{

	/// Return true if the value is a power of two number.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, bool, Q> isPowerOfTwo(vec<L, T, Q> const& v);

	/// Return the power of two number which value is just higher the input value,
	/// round up to a power of two.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> nextPowerOfTwo(vec<L, T, Q> const& v);

	/// Return the power of two number which value is just lower the input value,
	/// round down to a power of two.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> prevPowerOfTwo(vec<L, T, Q> const& v);

	/// Return true if the 'Value' is a multiple of 'Multiple'.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, bool, Q> isMultiple(vec<L, T, Q> const& v, T Multiple);

	/// Return true if the 'Value' is a multiple of 'Multiple'.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, bool, Q> isMultiple(vec<L, T, Q> const& v, vec<L, T, Q> const& Multiple);

	/// Higher multiple number of Source.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @param v Source values to which is applied the function
	/// @param Multiple Must be a null or positive value
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> nextMultiple(vec<L, T, Q> const& v, T Multiple);

	/// Higher multiple number of Source.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @param v Source values to which is applied the function
	/// @param Multiple Must be a null or positive value
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> nextMultiple(vec<L, T, Q> const& v, vec<L, T, Q> const& Multiple);

	/// Lower multiple number of Source.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @param v Source values to which is applied the function
	/// @param Multiple Must be a null or positive value
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> prevMultiple(vec<L, T, Q> const& v, T Multiple);

	/// Lower multiple number of Source.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Signed or unsigned integer scalar types.
	/// @tparam Q Value from qualifier enum
	///
	/// @param v Source values to which is applied the function
	/// @param Multiple Must be a null or positive value
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> prevMultiple(vec<L, T, Q> const& v, vec<L, T, Q> const& Multiple);

	/// Returns the bit number of the Nth significant bit set to
	/// 1 in the binary representation of value.
	/// If value bitcount is less than the Nth significant bit, -1 will be returned.
	///
	/// @tparam L An integer between 1 and 4 included that qualify the dimension of the vector.
	/// @tparam T Signed or unsigned integer scalar types.
	///
	/// @see ext_vector_integer
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, int, Q> findNSB(vec<L, T, Q> const& Source, vec<L, int, Q> SignificantBitCount);

	/// @}
} //namespace glm

#include "vector_integer.inl"
