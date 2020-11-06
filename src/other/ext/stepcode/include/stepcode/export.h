#ifndef STEPCODE_EXPORT_H
#define STEPCODE_EXPORT_H

#if defined(_WIN32)
# define COMPILER_DLLEXPORT __declspec(dllexport)
# define COMPILER_DLLIMPORT __declspec(dllimport)
#else
# define COMPILER_DLLEXPORT __attribute__ ((visibility ("default")))
# define COMPILER_DLLIMPORT __attribute__ ((visibility ("default")))
#endif

/* Import/Export flags for base. */
#ifndef STEPCODE_BASE_EXPORT
# if defined(STEPCODE_BASE_DLL_EXPORTS) && defined(STEPCODE_BASE_DLL_IMPORTS)
#  error "Only STEPCODE_BASE_DLL_EXPORTS or STEPCODE_BASE_DLL_IMPORTS can be defined, not both."
# elif defined(STEPCODE_BASE_DLL_EXPORTS)
#  define STEPCODE_BASE_EXPORT COMPILER_DLLEXPORT
# elif defined(STEPCODE_BASE_DLL_IMPORTS)
#  define STEPCODE_BASE_EXPORT COMPILER_DLLIMPORT
# else
#  define STEPCODE_BASE_EXPORT
# endif
#endif

/* Import/Export flags for express. */
#ifndef STEPCODE_EXPRESS_EXPORT
#  if defined(STEPCODE_EXPRESS_DLL_EXPORTS) && defined(STEPCODE_EXPRESS_DLL_IMPORTS)
#    error "Only STEPCODE_EXPRESS_DLL_EXPORTS or STEPCODE_EXPRESS_DLL_IMPORTS can be defined, not both."
#  elif defined(STEPCODE_EXPRESS_DLL_EXPORTS)
#    define STEPCODE_EXPRESS_EXPORT COMPILER_DLLEXPORT
#  elif defined(STEPCODE_EXPRESS_DLL_IMPORTS)
#    define STEPCODE_EXPRESS_EXPORT COMPILER_DLLIMPORT
#  else
#    define STEPCODE_EXPRESS_EXPORT
#  endif
#endif

/* Import/Export flags for exppp. */
#ifndef STEPCODE_EXPPP_EXPORT
#  if defined(STEPCODE_EXPPP_DLL_EXPORTS) && defined(STEPCODE_EXPPP_DLL_IMPORTS)
#    error "Only STEPCODE_EXPPP_DLL_EXPORTS or STEPCODE_EXPPP_DLL_IMPORTS can be defined, not both."
#  elif defined(STEPCODE_EXPPP_DLL_EXPORTS)
#    define STEPCODE_EXPPP_EXPORT COMPILER_DLLEXPORT
#  elif defined(STEPCODE_EXPPP_DLL_IMPORTS)
#    define STEPCODE_EXPPP_EXPORT COMPILER_DLLIMPORT
#  else
#    define STEPCODE_EXPPP_EXPORT
#  endif
#endif

/* Import/Export flags for utils. */
#ifndef STEPCODE_UTILS_EXPORT
#  if defined(STEPCODE_UTILS_DLL_EXPORTS) && defined(STEPCODE_UTILS_DLL_IMPORTS)
#    error "Only STEPCODE_UTILS_DLL_EXPORTS or STEPCODE_UTILS_DLL_IMPORTS can be defined, not both."
#  elif defined(STEPCODE_UTILS_DLL_EXPORTS)
#    define STEPCODE_UTILS_EXPORT COMPILER_DLLEXPORT
#  elif defined(STEPCODE_UTILS_DLL_IMPORTS)
#    define STEPCODE_UTILS_EXPORT COMPILER_DLLIMPORT
#  else
#    define STEPCODE_UTILS_EXPORT
#  endif
#endif

/* Import/Export flags for dai. */
#ifndef STEPCODE_DAI_EXPORT
#  if defined(STEPCODE_DAI_DLL_EXPORTS) && defined(STEPCODE_DAI_DLL_IMPORTS)
#    error "Only STEPCODE_DAI_DLL_EXPORTS or STEPCODE_DAI_DLL_IMPORTS can be defined, not both."
#  elif defined(STEPCODE_DAI_DLL_EXPORTS)
#    define STEPCODE_DAI_EXPORT COMPILER_DLLEXPORT
#  elif defined(STEPCODE_DAI_DLL_IMPORTS)
#    define STEPCODE_DAI_EXPORT COMPILER_DLLIMPORT
#  else
#    define STEPCODE_DAI_EXPORT
#  endif
#endif

/* Import/Export flags for core. */
#ifndef STEPCODE_CORE_EXPORT
#  if defined(STEPCODE_CORE_DLL_EXPORTS) && defined(STEPCODE_CORE_DLL_IMPORTS)
#    error "Only STEPCODE_CORE_DLL_EXPORTS or STEPCODE_CORE_DLL_IMPORTS can be defined, not both."
#  elif defined(STEPCODE_CORE_DLL_EXPORTS)
#    define STEPCODE_CORE_EXPORT COMPILER_DLLEXPORT
#  elif defined(STEPCODE_CORE_DLL_IMPORTS)
#    define STEPCODE_CORE_EXPORT COMPILER_DLLIMPORT
#  else
#    define STEPCODE_CORE_EXPORT
#  endif
#endif

/* Import/Export flags for editor. */
#ifndef STEPCODE_EDITOR_EXPORT
#  if defined(STEPCODE_EDITOR_DLL_EXPORTS) && defined(STEPCODE_EDITOR_DLL_IMPORTS)
#    error "Only STEPCODE_EDITOR_DLL_EXPORTS or STEPCODE_EDITOR_DLL_IMPORTS can be defined, not both."
#  elif defined(STEPCODE_EDITOR_DLL_EXPORTS)
#    define STEPCODE_EDITOR_EXPORT COMPILER_DLLEXPORT
#  elif defined(STEPCODE_EDITOR_DLL_IMPORTS)
#    define STEPCODE_EDITOR_EXPORT COMPILER_DLLIMPORT
#  else
#    define STEPCODE_EDITOR_EXPORT
#  endif
#endif

/* Import/Export flags for lazyfile. */
#ifndef STEPCODE_LAZYFILE_EXPORT
#  if defined(STEPCODE_LAZYFILE_DLL_EXPORTS) && defined(STEPCODE_LAZYFILE_DLL_IMPORTS)
#    error "Only STEPCODE_LAZYFILE_DLL_EXPORTS or STEPCODE_LAZYFILE_DLL_IMPORTS can be defined, not both."
#  elif defined(STEPCODE_LAZYFILE_DLL_EXPORTS)
#    define STEPCODE_LAZYFILE_EXPORT COMPILER_DLLEXPORT
#  elif defined(STEPCODE_LAZYFILE_DLL_IMPORTS)
#    define STEPCODE_LAZYFILE_EXPORT COMPILER_DLLIMPORT
#  else
#    define STEPCODE_LAZYFILE_EXPORT
#  endif
#endif

#endif /* STEPCODE_EXPORT_H */
