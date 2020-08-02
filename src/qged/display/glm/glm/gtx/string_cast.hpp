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

/// @ref gtx_string_cast
/// @file glm/gtx/string_cast.hpp
///
/// @see core (dependence)
/// @see gtx_integer (dependence)
/// @see gtx_quaternion (dependence)
///
/// @defgroup gtx_string_cast GLM_GTX_string_cast
/// @ingroup gtx
///
/// Include <glm/gtx/string_cast.hpp> to use the features of this extension.
///
/// Setup strings for GLM type values
///
/// This extension is not supported with CUDA

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtc/type_precision.hpp"
#include "../gtc/quaternion.hpp"
#include "../gtx/dual_quaternion.hpp"
#include <string>
#include <cmath>

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_string_cast is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_string_cast extension included")
#	endif
#endif

#if(GLM_COMPILER & GLM_COMPILER_CUDA)
#	error "GLM_GTX_string_cast is not supported on CUDA compiler"
#endif

namespace glm
{
	/// @addtogroup gtx_string_cast
	/// @{

	/// Create a string from a GLM vector or matrix typed variable.
	/// @see gtx_string_cast extension.
	template<typename genType>
	GLM_FUNC_DECL std::string to_string(genType const& x);

	/// @}
}//namespace glm

#include "string_cast.inl"
