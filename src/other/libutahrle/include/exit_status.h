/** @addtogroup utahrle */
/** @{ */
/** @file exit_status.h */

/* for libutahrle */

#ifndef EXIT_STATUS_DEFINED

#define EXIT_STATUS_DEFINED

#ifdef __STDC__

#   define EXIT_STATUS_SUCCESS  0
#   define EXIT_STATUS_FAILURE  1

#else

#   ifdef VMS
#       define EXIT_STATUS_SUCCESS      (1 | 0x10000000)
#       define EXIT_STATUS_FAILURE      (0 | 0x10000000)
#   else
#       define EXIT_STATUS_SUCCESS      0
#       define EXIT_STATUS_FAILURE      1
#   endif

#endif
#endif
/** @} */
