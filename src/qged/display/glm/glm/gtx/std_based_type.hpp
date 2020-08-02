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

/// @ref gtx_std_based_type
/// @file glm/gtx/std_based_type.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_std_based_type GLM_GTX_std_based_type
/// @ingroup gtx
///
/// Include <glm/gtx/std_based_type.hpp> to use the features of this extension.
///
/// Adds vector types based on STL value types.

#pragma once

// Dependency:
#include "../glm.hpp"
#include <cstdlib>

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_std_based_type is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_std_based_type extension included")
#	endif
#endif

namespace glm
{
	/// @addtogroup gtx_std_based_type
	/// @{

	/// Vector type based of one std::size_t component.
	/// @see GLM_GTX_std_based_type
	typedef vec<1, std::size_t, defaultp>		size1;

	/// Vector type based of two std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef vec<2, std::size_t, defaultp>		size2;

	/// Vector type based of three std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef vec<3, std::size_t, defaultp>		size3;

	/// Vector type based of four std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef vec<4, std::size_t, defaultp>		size4;

	/// Vector type based of one std::size_t component.
	/// @see GLM_GTX_std_based_type
	typedef vec<1, std::size_t, defaultp>		size1_t;

	/// Vector type based of two std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef vec<2, std::size_t, defaultp>		size2_t;

	/// Vector type based of three std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef vec<3, std::size_t, defaultp>		size3_t;

	/// Vector type based of four std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef vec<4, std::size_t, defaultp>		size4_t;

	/// @}
}//namespace glm

#include "std_based_type.inl"
