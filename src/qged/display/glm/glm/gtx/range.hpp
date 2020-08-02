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

/// @ref gtx_range
/// @file glm/gtx/range.hpp
/// @author Joshua Moerman
///
/// @defgroup gtx_range GLM_GTX_range
/// @ingroup gtx
///
/// Include <glm/gtx/range.hpp> to use the features of this extension.
///
/// Defines begin and end for vectors and matrices. Useful for range-based for loop.
/// The range is defined over the elements, not over columns or rows (e.g. mat4 has 16 elements).

#pragma once

// Dependencies
#include "../detail/setup.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_range is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_range extension included")
#	endif
#endif

#include "../gtc/type_ptr.hpp"
#include "../gtc/vec1.hpp"

namespace glm
{
	/// @addtogroup gtx_range
	/// @{

#	if GLM_COMPILER & GLM_COMPILER_VC
#		pragma warning(push)
#		pragma warning(disable : 4100) // unreferenced formal parameter
#	endif

	template<typename T, qualifier Q>
	inline length_t components(vec<1, T, Q> const& v)
	{
		return v.length();
	}

	template<typename T, qualifier Q>
	inline length_t components(vec<2, T, Q> const& v)
	{
		return v.length();
	}

	template<typename T, qualifier Q>
	inline length_t components(vec<3, T, Q> const& v)
	{
		return v.length();
	}

	template<typename T, qualifier Q>
	inline length_t components(vec<4, T, Q> const& v)
	{
		return v.length();
	}

	template<typename genType>
	inline length_t components(genType const& m)
	{
		return m.length() * m[0].length();
	}

	template<typename genType>
	inline typename genType::value_type const * begin(genType const& v)
	{
		return value_ptr(v);
	}

	template<typename genType>
	inline typename genType::value_type const * end(genType const& v)
	{
		return begin(v) + components(v);
	}

	template<typename genType>
	inline typename genType::value_type * begin(genType& v)
	{
		return value_ptr(v);
	}

	template<typename genType>
	inline typename genType::value_type * end(genType& v)
	{
		return begin(v) + components(v);
	}

#	if GLM_COMPILER & GLM_COMPILER_VC
#		pragma warning(pop)
#	endif

	/// @}
}//namespace glm
