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

/// @ref gtx_fast_exponential
/// @file glm/gtx/fast_exponential.hpp
///
/// @see core (dependence)
/// @see gtx_half_float (dependence)
///
/// @defgroup gtx_fast_exponential GLM_GTX_fast_exponential
/// @ingroup gtx
///
/// Include <glm/gtx/fast_exponential.hpp> to use the features of this extension.
///
/// Fast but less accurate implementations of exponential based functions.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_fast_exponential is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_fast_exponential extension included")
#	endif
#endif

namespace glm
{
	/// @addtogroup gtx_fast_exponential
	/// @{

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<typename genType>
	GLM_FUNC_DECL genType fastPow(genType x, genType y);

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> fastPow(vec<L, T, Q> const& x, vec<L, T, Q> const& y);

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<typename genTypeT, typename genTypeU>
	GLM_FUNC_DECL genTypeT fastPow(genTypeT x, genTypeU y);

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> fastPow(vec<L, T, Q> const& x);

	/// Faster than the common exp function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	GLM_FUNC_DECL T fastExp(T x);

	/// Faster than the common exp function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> fastExp(vec<L, T, Q> const& x);

	/// Faster than the common log function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	GLM_FUNC_DECL T fastLog(T x);

	/// Faster than the common exp2 function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> fastLog(vec<L, T, Q> const& x);

	/// Faster than the common exp2 function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	GLM_FUNC_DECL T fastExp2(T x);

	/// Faster than the common exp2 function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> fastExp2(vec<L, T, Q> const& x);

	/// Faster than the common log2 function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	GLM_FUNC_DECL T fastLog2(T x);

	/// Faster than the common log2 function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> fastLog2(vec<L, T, Q> const& x);

	/// @}
}//namespace glm

#include "fast_exponential.inl"
