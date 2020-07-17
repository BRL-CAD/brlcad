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

/// @ref gtx_type_trait
/// @file glm/gtx/type_trait.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_type_trait GLM_GTX_type_trait
/// @ingroup gtx
///
/// Include <glm/gtx/type_trait.hpp> to use the features of this extension.
///
/// Defines traits for each type.

#pragma once

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	ifndef GLM_ENABLE_EXPERIMENTAL
#		pragma message("GLM: GLM_GTX_type_trait is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("GLM: GLM_GTX_type_trait extension included")
#	endif
#endif

// Dependency:
#include "../detail/qualifier.hpp"
#include "../gtc/quaternion.hpp"
#include "../gtx/dual_quaternion.hpp"

namespace glm
{
	/// @addtogroup gtx_type_trait
	/// @{

	template<typename T>
	struct type
	{
		static bool const is_vec = false;
		static bool const is_mat = false;
		static bool const is_quat = false;
		static length_t const components = 0;
		static length_t const cols = 0;
		static length_t const rows = 0;
	};

	template<length_t L, typename T, qualifier Q>
	struct type<vec<L, T, Q> >
	{
		static bool const is_vec = true;
		static bool const is_mat = false;
		static bool const is_quat = false;
		static length_t const components = L;
	};

	template<length_t C, length_t R, typename T, qualifier Q>
	struct type<mat<C, R, T, Q> >
	{
		static bool const is_vec = false;
		static bool const is_mat = true;
		static bool const is_quat = false;
		static length_t const components = C;
		static length_t const cols = C;
		static length_t const rows = R;
	};

	template<typename T, qualifier Q>
	struct type<qua<T, Q> >
	{
		static bool const is_vec = false;
		static bool const is_mat = false;
		static bool const is_quat = true;
		static length_t const components = 4;
	};

	template<typename T, qualifier Q>
	struct type<tdualquat<T, Q> >
	{
		static bool const is_vec = false;
		static bool const is_mat = false;
		static bool const is_quat = true;
		static length_t const components = 8;
	};

	/// @}
}//namespace glm

#include "type_trait.inl"
