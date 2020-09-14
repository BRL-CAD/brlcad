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

/// @ref ext_quaternion_double_precision
/// @file glm/ext/quaternion_double_precision.hpp
///
/// @defgroup ext_quaternion_double_precision GLM_EXT_quaternion_double_precision
/// @ingroup ext
///
/// Exposes double-precision floating point quaternion type with various precision in term of ULPs.
///
/// Include <glm/ext/quaternion_double_precision.hpp> to use the features of this extension.

#pragma once

// Dependency:
#include "../detail/type_quat.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_quaternion_double_precision extension included")
#endif

namespace glm
{
	/// @addtogroup ext_quaternion_double_precision
	/// @{

	/// Quaternion of double-precision floating-point numbers using high precision arithmetic in term of ULPs.
	///
	/// @see ext_quaternion_double_precision
	typedef qua<double, lowp>		lowp_dquat;

	/// Quaternion of medium double-qualifier floating-point numbers using high precision arithmetic in term of ULPs.
	///
	/// @see ext_quaternion_double_precision
	typedef qua<double, mediump>	mediump_dquat;

	/// Quaternion of high double-qualifier floating-point numbers using high precision arithmetic in term of ULPs.
	///
	/// @see ext_quaternion_double_precision
	typedef qua<double, highp>		highp_dquat;

	/// @}
} //namespace glm

