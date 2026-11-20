#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT

#if defined(_MSC_VER)
#  define MINIZ_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__) || defined(__clang__)
#  define MINIZ_DEPRECATED __attribute__((__deprecated__))
#else
#  define MINIZ_DEPRECATED
#endif

#define MINIZ_DEPRECATED_EXPORT MINIZ_DEPRECATED
#define MINIZ_DEPRECATED_NO_EXPORT MINIZ_DEPRECATED

#endif
