/* Configuration
   -------------
   Define as appropriate for your system.
   Sensible defaults provided.
*/


#ifndef TIME64_CONFIG_H
#    define TIME64_CONFIG_H

/* Debugging
   TIME_64_DEBUG
   Define if you want debugging messages
*/
/* #define TIME_64_DEBUG */


/* INT_64_T
   A 64 bit integer type to use to store time and others.
   Must be defined.

   MODIFIED: Use BRL-CAD's int64_t
*/
#define INT_64_T                int64_t


/* USE_TM64
   Should we use a 64 bit safe replacement for tm?  This will
   let you go past year 2 billion but the struct will be incompatible
   with tm.  Conversion functions will be provided.
*/
/* #define USE_TM64 */


/* Availability of system functions.

   HAS_GMTIME_R
   Define if your system has gmtime_r()

   HAS_LOCALTIME_R
   Define if your system has localtime_r()

   HAS_TIMEGM
   Define if your system has timegm(), a GNU extension.

   MODIFIED: Reduce necessary number of CMake checks by assuming that
   nothing nonstandard exists.
*/
/* #define HAS_GMTIME_R */
/* #define HAS_LOCALTIME_R */
/* #define HAS_TIMEGM */


/* Details of non-standard tm struct elements.

   HAS_TM_TM_GMTOFF
   True if your tm struct has a "tm_gmtoff" element.
   A BSD extension.

   HAS_TM_TM_ZONE
   True if your tm struct has a "tm_zone" element.
   A BSD extension.
*/
/* #define HAS_TM_TM_GMTOFF */
/* #define HAS_TM_TM_ZONE */


/* USE_SYSTEM_LOCALTIME
   USE_SYSTEM_GMTIME
   USE_SYSTEM_MKTIME
   USE_SYSTEM_TIMEGM
   Should we use the system functions if the time is inside their range?
   Your system localtime() is probably more accurate, but our gmtime() is
   fast and safe.

   MODIFIED: Never use any system functions so that we don't have to
   worry about the size of the host time_t.  DO NOT RE-ENABLE ANY OF
   THESE: the limits of the system functions have not been hooked into
   BRL-CAD's build system and may be incorrect on some platforms.
*/
/* #define USE_SYSTEM_LOCALTIME */
/* #define USE_SYSTEM_GMTIME */
/* #define USE_SYSTEM_MKTIME */
/* #define USE_SYSTEM_TIMEGM */

#endif /* TIME64_CONFIG_H */
