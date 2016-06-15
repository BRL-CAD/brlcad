#  if !defined(__cplusplus) && defined(HAVE_WORKING_LRINT_MACRO)
#    define lrint(_x) ((long int)(((_x)<0)?(_x)-0.5:(_x)+0.5))
#    define HAVE_LRINT 1
#  endif
