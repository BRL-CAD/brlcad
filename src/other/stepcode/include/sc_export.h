#ifndef SC_EXPORT_H
#define SC_EXPORT_H

/* Import/Export flags for base. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_BASE_DLL_EXPORTS)
#  define SC_BASE_EXPORT __declspec(dllexport)
# else
#  define SC_BASE_EXPORT __declspec(dllimport)
# endif
#else
#  define SC_BASE_EXPORT
#endif

/* Import/Export flags for express. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_EXPRESS_DLL_EXPORTS)
#  define SC_EXPRESS_EXPORT __declspec(dllexport)
# else
#  define SC_EXPRESS_EXPORT __declspec(dllimport)
# endif
#else
# define SC_EXPRESS_EXPORT
#endif

/* Import/Export flags for exppp. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_EXPPP_DLL_EXPORTS)
#  define SC_EXPPP_EXPORT __declspec(dllexport)
# else
#  define SC_EXPPP_EXPORT __declspec(dllimport)
# endif
#else
# define SC_EXPPP_EXPORT
#endif

/* Import/Export flags for utils. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_UTILS_DLL_EXPORTS)
#  define SC_UTILS_EXPORT __declspec(dllexport)
# else
#  define SC_UTILS_EXPORT __declspec(dllimport)
# endif
#else
# define SC_UTILS_EXPORT
#endif

/* Import/Export flags for dai. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_DAI_DLL_EXPORTS)
#  define SC_DAI_EXPORT __declspec(dllexport)
# else
#  define SC_DAI_EXPORT __declspec(dllimport)
# endif
#else
# define SC_DAI_EXPORT
#endif

/* Import/Export flags for core. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_CORE_DLL_EXPORTS)
#  define SC_CORE_EXPORT __declspec(dllexport)
# else
#  define SC_CORE_EXPORT __declspec(dllimport)
# endif
#else
# define SC_CORE_EXPORT
#endif

/* Import/Export flags for editor. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_EDITOR_DLL_EXPORTS)
#  define SC_EDITOR_EXPORT __declspec(dllexport)
# else
#  define SC_EDITOR_EXPORT __declspec(dllimport)
# endif
#else
# define SC_EDITOR_EXPORT
#endif

/* Import/Export flags for lazyfile. */
#if !defined(SC_STATIC) && defined(_WIN32)
# if defined(SC_LAZYFILE_DLL_EXPORTS)
#  define SC_LAZYFILE_EXPORT __declspec(dllexport)
# else
#  define SC_LAZYFILE_EXPORT __declspec(dllimport)
# endif
#else
#  define SC_LAZYFILE_EXPORT
#endif

#endif /* SC_EXPORT_H */
