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

/// @ref core
/// @file glm/matrix.hpp
///
/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.6 Matrix Functions</a>
///
/// @defgroup core_func_matrix Matrix functions
/// @ingroup core
///
/// Provides GLSL matrix functions.
///
/// Include <glm/matrix.hpp> to use these core features.

#pragma once

// Dependencies
#include "detail/qualifier.hpp"
#include "detail/setup.hpp"
#include "vec2.hpp"
#include "vec3.hpp"
#include "vec4.hpp"
#include "mat2x2.hpp"
#include "mat2x3.hpp"
#include "mat2x4.hpp"
#include "mat3x2.hpp"
#include "mat3x3.hpp"
#include "mat3x4.hpp"
#include "mat4x2.hpp"
#include "mat4x3.hpp"
#include "mat4x4.hpp"

namespace glm {
namespace detail
{
	template<length_t C, length_t R, typename T, qualifier Q>
	struct outerProduct_trait{};

	template<typename T, qualifier Q>
	struct outerProduct_trait<2, 2, T, Q>
	{
		typedef mat<2, 2, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<2, 3, T, Q>
	{
		typedef mat<3, 2, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<2, 4, T, Q>
	{
		typedef mat<4, 2, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<3, 2, T, Q>
	{
		typedef mat<2, 3, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<3, 3, T, Q>
	{
		typedef mat<3, 3, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<3, 4, T, Q>
	{
		typedef mat<4, 3, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<4, 2, T, Q>
	{
		typedef mat<2, 4, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<4, 3, T, Q>
	{
		typedef mat<3, 4, T, Q> type;
	};

	template<typename T, qualifier Q>
	struct outerProduct_trait<4, 4, T, Q>
	{
		typedef mat<4, 4, T, Q> type;
	};
}//namespace detail

	 /// @addtogroup core_func_matrix
	 /// @{

	 /// Multiply matrix x by matrix y component-wise, i.e.,
	 /// result[i][j] is the scalar product of x[i][j] and y[i][j].
	 ///
	 /// @tparam C Integer between 1 and 4 included that qualify the number a column
	 /// @tparam R Integer between 1 and 4 included that qualify the number a row
	 /// @tparam T Floating-point or signed integer scalar types
	 /// @tparam Q Value from qualifier enum
	 ///
	 /// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/matrixCompMult.xml">GLSL matrixCompMult man page</a>
	 /// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.6 Matrix Functions</a>
	template<length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL mat<C, R, T, Q> matrixCompMult(mat<C, R, T, Q> const& x, mat<C, R, T, Q> const& y);

	/// Treats the first parameter c as a column vector
	/// and the second parameter r as a row vector
	/// and does a linear algebraic matrix multiply c * r.
	///
	/// @tparam C Integer between 1 and 4 included that qualify the number a column
	/// @tparam R Integer between 1 and 4 included that qualify the number a row
	/// @tparam T Floating-point or signed integer scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/outerProduct.xml">GLSL outerProduct man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.6 Matrix Functions</a>
	template<length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL typename detail::outerProduct_trait<C, R, T, Q>::type outerProduct(vec<C, T, Q> const& c, vec<R, T, Q> const& r);

	/// Returns the transposed matrix of x
	///
	/// @tparam C Integer between 1 and 4 included that qualify the number a column
	/// @tparam R Integer between 1 and 4 included that qualify the number a row
	/// @tparam T Floating-point or signed integer scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/transpose.xml">GLSL transpose man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.6 Matrix Functions</a>
	template<length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL typename mat<C, R, T, Q>::transpose_type transpose(mat<C, R, T, Q> const& x);

	/// Return the determinant of a squared matrix.
	///
	/// @tparam C Integer between 1 and 4 included that qualify the number a column
	/// @tparam R Integer between 1 and 4 included that qualify the number a row
	/// @tparam T Floating-point or signed integer scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/determinant.xml">GLSL determinant man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.6 Matrix Functions</a>
	template<length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL T determinant(mat<C, R, T, Q> const& m);

	/// Return the inverse of a squared matrix.
	///
	/// @tparam C Integer between 1 and 4 included that qualify the number a column
	/// @tparam R Integer between 1 and 4 included that qualify the number a row
	/// @tparam T Floating-point or signed integer scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/inverse.xml">GLSL inverse man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.6 Matrix Functions</a>
	template<length_t C, length_t R, typename T, qualifier Q>
	GLM_FUNC_DECL mat<C, R, T, Q> inverse(mat<C, R, T, Q> const& m);

	/// @}
}//namespace glm

#include "detail/func_matrix.inl"
