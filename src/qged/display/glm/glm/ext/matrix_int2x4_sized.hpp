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

/// @ref ext_matrix_int2x4_sized
/// @file glm/ext/matrix_int2x4_sized.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_int2x4_sized GLM_EXT_matrix_int2x4_sized
/// @ingroup ext
///
/// Include <glm/ext/matrix_int2x4_sized.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat2x4.hpp"
#include "../ext/scalar_int_sized.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_matrix_int2x4_sized extension included")
#endif

namespace glm
{
	/// @addtogroup ext_matrix_int2x4_sized
	/// @{

	/// 8 bit signed integer 2x4 matrix.
	///
	/// @see ext_matrix_int2x4_sized
	typedef mat<2, 4, int8, defaultp>				i8mat2x4;

	/// 16 bit signed integer 2x4 matrix.
	///
	/// @see ext_matrix_int2x4_sized
	typedef mat<2, 4, int16, defaultp>				i16mat2x4;

	/// 32 bit signed integer 2x4 matrix.
	///
	/// @see ext_matrix_int2x4_sized
	typedef mat<2, 4, int32, defaultp>				i32mat2x4;

	/// 64 bit signed integer 2x4 matrix.
	///
	/// @see ext_matrix_int2x4_sized
	typedef mat<2, 4, int64, defaultp>				i64mat2x4;

	/// @}
}//namespace glm
