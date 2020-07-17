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

/// @ref ext_scalar_integer
/// @file glm/ext/scalar_integer.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_scalar_integer GLM_EXT_scalar_integer
/// @ingroup ext
///
/// Include <glm/ext/scalar_integer.hpp> to use the features of this extension.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../detail/_vectorize.hpp"
#include "../detail/type_float.hpp"
#include "../vector_relational.hpp"
#include "../common.hpp"
#include <limits>

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_scalar_integer extension included")
#endif

namespace glm
{
	/// @addtogroup ext_scalar_integer
	/// @{

	/// Return true if the value is a power of two number.
	///
	/// @see ext_scalar_integer
	template<typename genIUType>
	GLM_FUNC_DECL bool isPowerOfTwo(genIUType v);

	/// Return the power of two number which value is just higher the input value,
	/// round up to a power of two.
	///
	/// @see ext_scalar_integer
	template<typename genIUType>
	GLM_FUNC_DECL genIUType nextPowerOfTwo(genIUType v);

	/// Return the power of two number which value is just lower the input value,
	/// round down to a power of two.
	///
	/// @see ext_scalar_integer
	template<typename genIUType>
	GLM_FUNC_DECL genIUType prevPowerOfTwo(genIUType v);

	/// Return true if the 'Value' is a multiple of 'Multiple'.
	///
	/// @see ext_scalar_integer
	template<typename genIUType>
	GLM_FUNC_DECL bool isMultiple(genIUType v, genIUType Multiple);

	/// Higher multiple number of Source.
	///
	/// @tparam genIUType Integer scalar or vector types.
	///
	/// @param v Source value to which is applied the function
	/// @param Multiple Must be a null or positive value
	///
	/// @see ext_scalar_integer
	template<typename genIUType>
	GLM_FUNC_DECL genIUType nextMultiple(genIUType v, genIUType Multiple);

	/// Lower multiple number of Source.
	///
	/// @tparam genIUType Integer scalar or vector types.
	///
	/// @param v Source value to which is applied the function
	/// @param Multiple Must be a null or positive value
	///
	/// @see ext_scalar_integer
	template<typename genIUType>
	GLM_FUNC_DECL genIUType prevMultiple(genIUType v, genIUType Multiple);

	/// Returns the bit number of the Nth significant bit set to
	/// 1 in the binary representation of value.
	/// If value bitcount is less than the Nth significant bit, -1 will be returned.
	///
	/// @tparam genIUType Signed or unsigned integer scalar types.
	///
	/// @see ext_scalar_integer
	template<typename genIUType>
	GLM_FUNC_DECL int findNSB(genIUType x, int significantBitCount);

	/// @}
} //namespace glm

#include "scalar_integer.inl"
