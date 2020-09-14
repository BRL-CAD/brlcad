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

/// @ref gtx_normalize_dot
/// @file glm/gtx/normalize_dot.hpp
///
/// @see core (dependence)
/// @see gtx_fast_square_root (dependence)
///
/// @defgroup gtx_normalize_dot GLM_GTX_normalize_dot
/// @ingroup gtx
///
/// Include <glm/gtx/normalized_dot.hpp> to use the features of this extension.
///
/// Dot product of vectors that need to be normalize with a single square root.

#pragma once

// Dependency:
#include "../gtx/fast_square_root.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_normalize_dot is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_normalize_dot extension included")
#	endif
#endif

namespace glm
{
	/// @addtogroup gtx_normalize_dot
	/// @{

	/// Normalize parameters and returns the dot product of x and y.
	/// It's faster that dot(normalize(x), normalize(y)).
	///
	/// @see gtx_normalize_dot extension.
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL T normalizeDot(vec<L, T, Q> const& x, vec<L, T, Q> const& y);

	/// Normalize parameters and returns the dot product of x and y.
	/// Faster that dot(fastNormalize(x), fastNormalize(y)).
	///
	/// @see gtx_normalize_dot extension.
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL T fastNormalizeDot(vec<L, T, Q> const& x, vec<L, T, Q> const& y);

	/// @}
}//namespace glm

#include "normalize_dot.inl"
