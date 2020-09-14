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

/// @ref gtx_color_space_YCoCg
/// @file glm/gtx/color_space_YCoCg.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_color_space_YCoCg GLM_GTX_color_space_YCoCg
/// @ingroup gtx
///
/// Include <glm/gtx/color_space_YCoCg.hpp> to use the features of this extension.
///
/// RGB to YCoCg conversions and operations

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_color_space_YCoCg is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_color_space_YCoCg extension included")
#	endif
#endif

namespace glm
{
	/// @addtogroup gtx_color_space_YCoCg
	/// @{

	/// Convert a color from RGB color space to YCoCg color space.
	/// @see gtx_color_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> rgb2YCoCg(
		vec<3, T, Q> const& rgbColor);

	/// Convert a color from YCoCg color space to RGB color space.
	/// @see gtx_color_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> YCoCg2rgb(
		vec<3, T, Q> const& YCoCgColor);

	/// Convert a color from RGB color space to YCoCgR color space.
	/// @see "YCoCg-R: A Color Space with RGB Reversibility and Low Dynamic Range"
	/// @see gtx_color_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> rgb2YCoCgR(
		vec<3, T, Q> const& rgbColor);

	/// Convert a color from YCoCgR color space to RGB color space.
	/// @see "YCoCg-R: A Color Space with RGB Reversibility and Low Dynamic Range"
	/// @see gtx_color_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> YCoCgR2rgb(
		vec<3, T, Q> const& YCoCgColor);

	/// @}
}//namespace glm

#include "color_space_YCoCg.inl"
