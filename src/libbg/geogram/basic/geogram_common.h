/*
 *  Copyright (c) 2000-2022 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

#ifndef GEOBRLCAD_BASIC_GEOGRAM_COMMON
#define GEOBRLCAD_BASIC_GEOGRAM_COMMON


/**
 * \brief Basic definitions for the Geogram C API
 */

/*
 * Deactivate warnings about documentation
 * We do that, because CLANG's doxygen parser does not know
 * some doxygen commands that we use (retval, copydoc) and
 * generates many warnings for them...
 */
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#endif

/**
 * \brief Linkage declaration for geogram symbols.
 */

#if defined(GEOBRL_DYNAMIC_LIBS)
#if defined(_MSC_VER)
#define GEOBRL_IMPORT __declspec(dllimport)
#define GEOBRL_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define GEOBRL_IMPORT
#define GEOBRL_EXPORT __attribute__ ((visibility("default")))
#else
#define GEOBRL_IMPORT
#define GEOBRL_EXPORT
#endif
#else
#define GEOBRL_IMPORT
#define GEOBRL_EXPORT
#endif

#ifdef BRLCAD_GEOGRAM_EMBED
/*
 * When built as part of libbg, mirror BG_EXPORT.
 * common.h (always the first include in any BRL-CAD translation unit)
 * defines COMPILER_DLLEXPORT and COMPILER_DLLIMPORT before this header
 * is processed.
 */
#  undef GEOBRLCAD_API
#  if defined(BG_DLL_EXPORTS) && defined(BG_DLL_IMPORTS)
#    error "Only BG_DLL_EXPORTS or BG_DLL_IMPORTS can be defined, not both."
#  elif defined(BG_DLL_EXPORTS)
#    define GEOBRLCAD_API COMPILER_DLLEXPORT
#  elif defined(BG_DLL_IMPORTS)
#    define GEOBRLCAD_API COMPILER_DLLIMPORT
#  else
#    define GEOBRLCAD_API
#  endif
#else
#  ifdef geogram_EXPORTS
#    define GEOBRLCAD_API GEOBRL_EXPORT
#  else
#    define GEOBRLCAD_API GEOBRL_IMPORT
#  endif
#endif

/*
 * If GARGANTUA is defined, then geogram is compiled
 * with 64 bit indices.
 */
#ifdef GARGANTUA

#include <stdint.h>

/**
 * \brief Represents indices.
 * \details Used by the C API.
 */
typedef uint64_t geo_index_t;

/**
 * \brief Represents possibly negative indices.
 * \details Used by the C API.
 */
typedef int64_t geo_signed_index_t;

#else

/**
 * \brief Represents indices.
 * \details Used by the C API.
 */
typedef unsigned int geo_index_t;

/**
 * \brief Represents possibly negative indices.
 * \details Used by the C API.
 */
typedef int geo_signed_index_t;

#endif

#include "geogram/basic/geogram_options.h"

/**
 * \file geogram/basic/geogram_common.h
 * \brief Common include file, providing basic definitions. Should be
 *  included before anything else by all header files in Vorpaline.
 */

/**
 * \def GEOBRL_DEBUG
 * \brief This macro is set when compiling in debug mode
 *
 * \def GEOBRL_PARANOID
 * \brief This macro is set when compiling in debug mode
 *
 * \def GEOBRL_OS_LINUX
 * \brief This macro is set on Linux systems (Android included).
 *
 * \def GEOBRL_OS_UNIX
 * \brief This macro is set on Unix systems (Android included).
 *
 * \def GEOBRL_OS_WINDOWS
 * \brief This macro is set on Windows systems.
 *
 * \def GEOBRL_OS_APPLE
 * \brief This macro is set on Apple systems.
 *
 * \def GEOBRL_OS_ANDROID
 * \brief This macro is set on Android systems (in addition to GEOBRL_OS_LINUX
 * and GEOBRL_OS_UNIX).
 *
 * \def GEOBRL_COMPILER_GCC
 * \brief This macro is set if the source code is compiled with GNU's gcc.
 *
 * \def GEOBRL_COMPILER_INTEL
 * \brief This macro is set if the source code is compiled with Intel's icc.
 *
 * \def GEOBRL_COMPILER_MSVC
 * \brief This macro is set if the source code is compiled with Microsoft's
 * Visual C++.
 */

#if (defined(NDEBUG) || defined(GEOBRLCAD_PSM)) && !defined(GEOBRLCAD_PSM_DEBUG)
#undef GEOBRL_DEBUG
#undef GEOBRL_PARANOID
#else
#define GEOBRL_DEBUG
#define GEOBRL_PARANOID
#endif

// =============================== LINUX defines ===========================

#if defined(__ANDROID__)
#define GEOBRL_OS_ANDROID
#endif

#if defined(__linux__)

#define GEOBRL_OS_LINUX
#define GEOBRL_OS_UNIX

#if defined(__INTEL_COMPILER)
#  define GEOBRL_COMPILER_INTEL
#elif defined(__clang__)
#  define GEOBRL_COMPILER_CLANG
#elif defined(__GNUC__)
#  define GEOBRL_COMPILER_GCC
#else
#  error "Unsupported compiler"
#endif

// =============================== WINDOWS defines =========================

#elif defined(_WIN32) || defined(_WIN64)

#define GEOBRL_OS_WINDOWS

#if defined(_MSC_VER)
#  define GEOBRL_COMPILER_MSVC
#elif defined(__MINGW32__) || defined(__MINGW64__)
#  define GEOBRL_COMPILER_MINGW
#endif

// =============================== APPLE defines ===========================

#elif defined(__APPLE__)

#define GEOBRL_OS_APPLE
#define GEOBRL_OS_UNIX

#if defined(__clang__)
#  define GEOBRL_COMPILER_CLANG
#elif defined(__GNUC__)
#  define GEOBRL_COMPILER_GCC
#else
#  error "Unsupported compiler"
#endif

// =============================== Other POSIX ==============================
#elif defined(__unix__) || defined(__unix)

#define GEOBRL_OS_UNIX

#if defined(__clang__)
#  define GEOBRL_COMPILER_CLANG
#elif defined(__GNUC__)
#  define GEOBRL_COMPILER_GCC
#endif

// =============================== Unsupported =============================
#else
/* Unrecognized OS: best-effort compilation without OS-specific optimizations */
#if defined(_MSC_VER)
#  pragma message("geogram: unrecognized operating system, continuing with limited support")
#elif defined(__GNUC__) || defined(__clang__)
#  warning "geogram: unrecognized operating system, continuing with limited support"
#endif
#endif

#if defined(GEOBRL_COMPILER_GCC)   ||              \
    defined(GEOBRL_COMPILER_CLANG) ||              \
    defined(GEOBRL_COMPILER_MINGW)
#define GEOBRL_COMPILER_GCC_FAMILY
#endif

#define FOR(I,UPPERBND) for(index_t I = 0; I<index_t(UPPERBND); ++I)

// Silence warnings for alloca()
// We use it at different places to allocate objects on the stack
// (for instance, in multi-precision predicates).
#ifdef GEOBRL_COMPILER_CLANG
#pragma GCC diagnostic ignored "-Walloca"
#endif

#endif
