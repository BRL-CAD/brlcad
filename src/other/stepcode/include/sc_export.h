#ifndef SC_EXPORT_H
#define SC_EXPORT_H

/* Import/Export flags for base. */
#ifndef SC_BASE_EXPORT
# if defined(SC_BASE_DLL_EXPORTS) && defined(SC_BASE_DLL_IMPORTS)
#  error "Only SC_BASE_DLL_EXPORTS or SC_BASE_DLL_IMPORTS can be defined, not both."
# elif defined(SC_BASE_DLL_EXPORTS)
#  define SC_BASE_EXPORT __declspec(dllexport)
# elif defined(SC_BASE_DLL_IMPORTS)
#  define SC_BASE_EXPORT __declspec(dllimport)
# else
#  define SC_BASE_EXPORT
# endif
#endif

/* Import/Export flags for express. */
#ifndef SC_EXPRESS_EXPORT
#  if defined(SC_EXPRESS_DLL_EXPORTS) && defined(SC_EXPRESS_DLL_IMPORTS)
#    error "Only SC_EXPRESS_DLL_EXPORTS or SC_EXPRESS_DLL_IMPORTS can be defined, not both."
#  elif defined(SC_EXPRESS_DLL_EXPORTS)
#    define SC_EXPRESS_EXPORT __declspec(dllexport)
#  elif defined(SC_EXPRESS_DLL_IMPORTS)
#    define SC_EXPRESS_EXPORT __declspec(dllimport)
#  else
#    define SC_EXPRESS_EXPORT
#  endif
#endif

/* Import/Export flags for exppp. */
#ifndef SC_EXPPP_EXPORT
#  if defined(SC_EXPPP_DLL_EXPORTS) && defined(SC_EXPPP_DLL_IMPORTS)
#    error "Only SC_EXPPP_DLL_EXPORTS or SC_EXPPP_DLL_IMPORTS can be defined, not both."
#  elif defined(SC_EXPPP_DLL_EXPORTS)
#    define SC_EXPPP_EXPORT __declspec(dllexport)
#  elif defined(SC_EXPPP_DLL_IMPORTS)
#    define SC_EXPPP_EXPORT __declspec(dllimport)
#  else
#    define SC_EXPPP_EXPORT
#  endif
#endif

/* Import/Export flags for utils. */
#ifndef SC_UTILS_EXPORT
#  if defined(SC_UTILS_DLL_EXPORTS) && defined(SC_UTILS_DLL_IMPORTS)
#    error "Only SC_UTILS_DLL_EXPORTS or SC_UTILS_DLL_IMPORTS can be defined, not both."
#  elif defined(SC_UTILS_DLL_EXPORTS)
#    define SC_UTILS_EXPORT __declspec(dllexport)
#  elif defined(SC_UTILS_DLL_IMPORTS)
#    define SC_UTILS_EXPORT __declspec(dllimport)
#  else
#    define SC_UTILS_EXPORT
#  endif
#endif

/* Import/Export flags for dai. */
#ifndef SC_DAI_EXPORT
#  if defined(SC_DAI_DLL_EXPORTS) && defined(SC_DAI_DLL_IMPORTS)
#    error "Only SC_DAI_DLL_EXPORTS or SC_DAI_DLL_IMPORTS can be defined, not both."
#  elif defined(SC_DAI_DLL_EXPORTS)
#    define SC_DAI_EXPORT __declspec(dllexport)
#  elif defined(SC_DAI_DLL_IMPORTS)
#    define SC_DAI_EXPORT __declspec(dllimport)
#  else
#    define SC_DAI_EXPORT
#  endif
#endif

/* Import/Export flags for core. */
#ifndef SC_CORE_EXPORT
#  if defined(SC_CORE_DLL_EXPORTS) && defined(SC_CORE_DLL_IMPORTS)
#    error "Only SC_CORE_DLL_EXPORTS or SC_CORE_DLL_IMPORTS can be defined, not both."
#  elif defined(SC_CORE_DLL_EXPORTS)
#    define SC_CORE_EXPORT __declspec(dllexport)
#  elif defined(SC_CORE_DLL_IMPORTS)
#    define SC_CORE_EXPORT __declspec(dllimport)
#  else
#    define SC_CORE_EXPORT
#  endif
#endif

/* Import/Export flags for editor. */
#ifndef SC_EDITOR_EXPORT
#  if defined(SC_EDITOR_DLL_EXPORTS) && defined(SC_EDITOR_DLL_IMPORTS)
#    error "Only SC_EDITOR_DLL_EXPORTS or SC_EDITOR_DLL_IMPORTS can be defined, not both."
#  elif defined(SC_EDITOR_DLL_EXPORTS)
#    define SC_EDITOR_EXPORT __declspec(dllexport)
#  elif defined(SC_EDITOR_DLL_IMPORTS)
#    define SC_EDITOR_EXPORT __declspec(dllimport)
#  else
#    define SC_EDITOR_EXPORT
#  endif
#endif

/* Import/Export flags for lazyfile. */
#ifndef SC_LAZYFILE_EXPORT
#  if defined(SC_LAZYFILE_DLL_EXPORTS) && defined(SC_LAZYFILE_DLL_IMPORTS)
#    error "Only SC_LAZYFILE_DLL_EXPORTS or SC_LAZYFILE_DLL_IMPORTS can be defined, not both."
#  elif defined(SC_LAZYFILE_DLL_EXPORTS)
#    define SC_LAZYFILE_EXPORT __declspec(dllexport)
#  elif defined(SC_LAZYFILE_DLL_IMPORTS)
#    define SC_LAZYFILE_EXPORT __declspec(dllimport)
#  else
#    define SC_LAZYFILE_EXPORT
#  endif
#endif

#endif /* SC_EXPORT_H */
