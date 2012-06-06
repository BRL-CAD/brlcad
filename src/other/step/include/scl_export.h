#ifndef SCL_EXPORT_H
#define SCL_EXPORT_H

/* Import/Export flags for base. */
#ifndef SCL_BASE_EXPORT
# if defined(SCL_BASE_DLL_EXPORTS) && defined(SCL_BASE_DLL_IMPORTS)
#  error "Only SCL_BASE_DLL_EXPORTS or SCL_BASE_DLL_IMPORTS can be defined, not both."
# elif defined(SCL_BASE_DLL_EXPORTS)
#  define SCL_BASE_EXPORT __declspec(dllexport)
# elif defined(SCL_BASE_DLL_IMPORTS)
#  define SCL_BASE_EXPORT __declspec(dllimport)
# else
#  define SCL_BASE_EXPORT
# endif
#endif

/* Import/Export flags for express. */
#ifndef SCL_EXPRESS_EXPORT
#  if defined(SCL_EXPRESS_DLL_EXPORTS) && defined(SCL_EXPRESS_DLL_IMPORTS)
#    error "Only SCL_EXPRESS_DLL_EXPORTS or SCL_EXPRESS_DLL_IMPORTS can be defined, not both."
#  elif defined(SCL_EXPRESS_DLL_EXPORTS)
#    define SCL_EXPRESS_EXPORT __declspec(dllexport)
#  elif defined(SCL_EXPRESS_DLL_IMPORTS)
#    define SCL_EXPRESS_EXPORT __declspec(dllimport)
#  else
#    define SCL_EXPRESS_EXPORT
#  endif
#endif

/* Import/Export flags for exppp. */
#ifndef SCL_EXPPP_EXPORT
#  if defined(SCL_EXPPP_DLL_EXPORTS) && defined(SCL_EXPPP_DLL_IMPORTS)
#    error "Only SCL_EXPPP_DLL_EXPORTS or SCL_EXPPP_DLL_IMPORTS can be defined, not both."
#  elif defined(SCL_EXPPP_DLL_EXPORTS)
#    define SCL_EXPPP_EXPORT __declspec(dllexport)
#  elif defined(SCL_EXPPP_DLL_IMPORTS)
#    define SCL_EXPPP_EXPORT __declspec(dllimport)
#  else
#    define SCL_EXPPP_EXPORT
#  endif
#endif

/* Import/Export flags for utils. */
#ifndef SCL_UTILS_EXPORT
#  if defined(SCL_UTILS_DLL_EXPORTS) && defined(SCL_UTILS_DLL_IMPORTS)
#    error "Only SCL_UTILS_DLL_EXPORTS or SCL_UTILS_DLL_IMPORTS can be defined, not both."
#  elif defined(SCL_UTILS_DLL_EXPORTS)
#    define SCL_UTILS_EXPORT __declspec(dllexport)
#  elif defined(SCL_UTILS_DLL_IMPORTS)
#    define SCL_UTILS_EXPORT __declspec(dllimport)
#  else
#    define SCL_UTILS_EXPORT
#  endif
#endif

/* Import/Export flags for dai. */
#ifndef SCL_DAI_EXPORT
#  if defined(SCL_DAI_DLL_EXPORTS) && defined(SCL_DAI_DLL_IMPORTS)
#    error "Only SCL_DAI_DLL_EXPORTS or SCL_DAI_DLL_IMPORTS can be defined, not both."
#  elif defined(SCL_DAI_DLL_EXPORTS)
#    define SCL_DAI_EXPORT __declspec(dllexport)
#  elif defined(SCL_DAI_DLL_IMPORTS)
#    define SCL_DAI_EXPORT __declspec(dllimport)
#  else
#    define SCL_DAI_EXPORT
#  endif
#endif

/* Import/Export flags for core. */
#ifndef SCL_CORE_EXPORT
#  if defined(SCL_CORE_DLL_EXPORTS) && defined(SCL_CORE_DLL_IMPORTS)
#    error "Only SCL_CORE_DLL_EXPORTS or SCL_CORE_DLL_IMPORTS can be defined, not both."
#  elif defined(SCL_CORE_DLL_EXPORTS)
#    define SCL_CORE_EXPORT __declspec(dllexport)
#  elif defined(SCL_CORE_DLL_IMPORTS)
#    define SCL_CORE_EXPORT __declspec(dllimport)
#  else
#    define SCL_CORE_EXPORT
#  endif
#endif

/* Import/Export flags for editor. */
#ifndef SCL_EDITOR_EXPORT
#  if defined(SCL_EDITOR_DLL_EXPORTS) && defined(SCL_EDITOR_DLL_IMPORTS)
#    error "Only SCL_EDITOR_DLL_EXPORTS or SCL_EDITOR_DLL_IMPORTS can be defined, not both."
#  elif defined(SCL_EDITOR_DLL_EXPORTS)
#    define SCL_EDITOR_EXPORT __declspec(dllexport)
#  elif defined(SCL_EDITOR_DLL_IMPORTS)
#    define SCL_EDITOR_EXPORT __declspec(dllimport)
#  else
#    define SCL_EDITOR_EXPORT
#  endif
#endif

#endif /* SCL_EXPORT_H */
