/* Copyright Gregory Pakosz (@gpakosz) 2019
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://github.com/gpakosz/whereami */

#include "common.h"

#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/malloc.h"

extern const char *_bu_progname_raw(void);

#define WAI_MALLOC(size) bu_malloc(size, "WAI_MALLOC")
#define WAI_FREE(p) bu_free(p, "WAI_FREE")
#define WAI_REALLOC(p, size) bu_realloc(p, size, "WAI_REALLOC")

// in case you want to #include "whereami.c" in a larger compilation unit
#if !defined(WHEREAMI_H)
#include "whereami.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(WAI_MALLOC) || !defined(WAI_FREE) || !defined(WAI_REALLOC)
#include <stdlib.h>
#endif

#if defined(HAVE_DLADDR) && !defined(HAVE_DECL_DLADDR) && !defined(dladdr)
typedef struct dl_info {
        const char      *dli_fname;     /* Pathname of shared object */
        void            *dli_fbase;     /* Base address of shared object */
        const char      *dli_sname;     /* Name of nearest symbol */
        void            *dli_saddr;     /* Address of nearest symbol */
} Dl_info;
extern int dladdr(const void *, Dl_info *);
#endif /* HAVE_DLADDR */

#if !defined(WAI_MALLOC)
#define WAI_MALLOC(size) malloc(size)
#endif

#if !defined(WAI_FREE)
#define WAI_FREE(p) free(p)
#endif

#if !defined(WAI_REALLOC)
#define WAI_REALLOC(p, size) realloc(p, size)
#endif

#ifndef WAI_NOINLINE
#if defined(_MSC_VER)
#define WAI_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define WAI_NOINLINE __attribute__((noinline))
#else
#error unsupported compiler
#endif
#endif

#if defined(_MSC_VER)
#define WAI_RETURN_ADDRESS() _ReturnAddress()
#elif defined(__GNUC__)
#define WAI_RETURN_ADDRESS() __builtin_extract_return_addr(__builtin_return_address(0))
#else
#error unsupported compiler
#endif

/* LIBBU fallback implementation */
static
int _bu_getExecutablePath(char* out, int capacity, int* dirname_length)
{
    const char *pname = _bu_progname_raw();
    struct bu_vls epath = BU_VLS_INIT_ZERO;

    if (pname[0] == '.') {
        char iwd[MAXPATHLEN];
        char fullpath[MAXPATHLEN];
        bu_getiwd(iwd, MAXPATHLEN);
	// Use a VLS for this, since in principle there's nothing stopping the
	// iwd and the bu_progname each individually from running right up to
	// MAXPATHLEN, and if they do a MAXPATHLEN buffer won't hold both of
	// them for realpath to try and digest down into something sane.
        bu_vls_sprintf(&epath, "%s%c%s", iwd, BU_DIR_SEPARATOR, pname);
        if (!bu_file_realpath(bu_vls_cstr(&epath), fullpath)) {
            /* Unable to resolve initial path concatentation */
	    bu_vls_free(&epath);
	    return -1;
        }
	bu_vls_sprintf(&epath, "%s", fullpath);
    } else {
	bu_vls_sprintf(&epath, "%s", pname);
    }

    int length = bu_vls_strlen(&epath);
    if (length <= capacity) {

	memcpy(out, bu_vls_cstr(&epath), length);

	if (dirname_length) {
	    int i;
	    for (i = length - 1; i >= 0; --i) {
		if (out[i] == '/') {
		    *dirname_length = i;
		    break;
		}
	    }
	}
    }

    bu_vls_free(&epath);

    return length;
}


#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#if defined(_MSC_VER)
#pragma warning(push, 3)
#endif
#include <windows.h>
#include <intrin.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

static int WAI_PREFIX(getModulePath_)(HMODULE module, char* out, int capacity, int* dirname_length)
{
  wchar_t buffer1[MAX_PATH];
  wchar_t buffer2[MAX_PATH];
  wchar_t* path = NULL;
  int length = -1;

  for (;;)
  {
    DWORD size;
    int length_, length__;

    size = GetModuleFileNameW(module, buffer1, sizeof(buffer1) / sizeof(buffer1[0]));

    if (size == 0)
      break;
    else if (size == (DWORD)(sizeof(buffer1) / sizeof(buffer1[0])))
    {
      DWORD size_ = size;
      do
      {
        wchar_t* path_;

        path_ = (wchar_t*)WAI_REALLOC(path, sizeof(wchar_t) * size_ * 2);
        if (!path_)
          break;
        size_ *= 2;
        path = path_;
        size = GetModuleFileNameW(module, path, size_);
      }
      while (size == size_);

      if (size == size_)
        break;
    }
    else
      path = buffer1;

    if (!_wfullpath(buffer2, path, MAX_PATH))
      break;
    length_ = (int)wcslen(buffer2);
    length__ = WideCharToMultiByte(CP_UTF8, 0, buffer2, length_ , out, capacity, NULL, NULL);

    if (length__ == 0)
      length__ = WideCharToMultiByte(CP_UTF8, 0, buffer2, length_, NULL, 0, NULL, NULL);
    if (length__ == 0)
      break;

    if (length__ <= capacity && dirname_length)
    {
      int i;

      for (i = length__ - 1; i >= 0; --i)
      {
        if (out[i] == '\\')
        {
          *dirname_length = i;
          break;
        }
      }
    }

    length = length__;

    break;
  }

  if (path != buffer1)
    WAI_FREE(path);

  return length;
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  return WAI_PREFIX(getModulePath_)(NULL, out, capacity, dirname_length);
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  HMODULE module;
  int length = -1;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4054)
#endif
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)WAI_RETURN_ADDRESS(), &module))
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
  {
    length = WAI_PREFIX(getModulePath_)(module, out, capacity, dirname_length);
  }

  return length;
}

#elif defined(__linux__) || defined(__CYGWIN__) || defined(__sun) || defined(WAI_USE_PROC_SELF_EXE)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#if !defined(WAI_PROC_SELF_EXE)
#if defined(__sun)
#define WAI_PROC_SELF_EXE "/proc/self/path/a.out"
#else
#define WAI_PROC_SELF_EXE "/proc/self/exe"
#endif
#endif

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[MAXPATHLEN];
  char* resolved = NULL;
  int length = -1;

  for (;;)
  {
    resolved = bu_file_realpath(WAI_PROC_SELF_EXE, buffer);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }

    break;
  }

  if (length <= 0) {
    return _bu_getExecutablePath(out, capacity, dirname_length);
  }

  return length;
}

#if !defined(WAI_PROC_SELF_MAPS_RETRY)
#define WAI_PROC_SELF_MAPS_RETRY 5
#endif

#if !defined(WAI_PROC_SELF_MAPS)
#if defined(__sun)
#define WAI_PROC_SELF_MAPS "/proc/self/map"
#else
#define WAI_PROC_SELF_MAPS "/proc/self/maps"
#endif
#endif

#if defined(__ANDROID__) || defined(ANDROID)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  int length = -1;
  FILE* maps = NULL;

  for (int r = 0; r < WAI_PROC_SELF_MAPS_RETRY; ++r)
  {
    maps = fopen(WAI_PROC_SELF_MAPS, "r");
    if (!maps)
      break;

    for (;;)
    {
      char buffer[MAXPATHLEN < 1024 ? 1024 : MAXPATHLEN];
      uint64_t low, high;
      char perms[5];
      uint64_t offset;
      uint32_t major, minor;
      char path[MAXPATHLEN];
      uint32_t inode;

      if (!bu_fgets(buffer, sizeof(buffer), maps))
        break;

      if (sscanf(buffer, "%" PRIx64 "-%" PRIx64 " %s %" PRIx64 " %x:%x %u %s\n", &low, &high, perms, &offset, &major, &minor, &inode, path) == 8)
      {
        void *paddr = WAI_RETURN_ADDRESS();
        uint64_t addr = (uint64_t)(uintptr_t)paddr;
        if (low <= addr && addr <= high)
        {
          char* resolved;

          resolved = bu_file_realpath(path, buffer);
          if (!resolved)
            break;

          length = (int)strlen(resolved);
#if defined(__ANDROID__) || defined(ANDROID)
          if (length > 4
              &&buffer[length - 1] == 'k'
              &&buffer[length - 2] == 'p'
              &&buffer[length - 3] == 'a'
              &&buffer[length - 4] == '.')
          {
            int fd = open(path, O_RDONLY);
            char* begin;
            char* p;

            begin = (char*)mmap(0, offset, PROT_READ, MAP_SHARED, fd, 0);
            p = begin + offset;

            while (p >= begin) // scan backwards
            {
              if (*((uint32_t*)p) == 0x04034b50UL) // local file header found
              {
                uint16_t length_ = *((uint16_t*)(p + 26));

                if (length + 2 + length_ < (int)sizeof(buffer))
                {
                  memcpy(&buffer[length], "!/", 2);
                  memcpy(&buffer[length + 2], p + 30, length_);
                  length += 2 + length_;
                }

                break;
              }

              p -= 4;
            }

            munmap(begin, offset);
            close(fd);
          }
#endif
          if (length <= capacity)
          {
            memcpy(out, resolved, length);

            if (dirname_length)
            {
              int i;

              for (i = length - 1; i >= 0; --i)
              {
                if (out[i] == '/')
                {
                  *dirname_length = i;
                  break;
                }
              }
            }
          }

          break;
        }
      }
    }

    fclose(maps);
    maps = NULL;

    if (length != -1)
      break;
  }

  if (maps)
    fclose(maps);

  return length;
}

#elif defined(__APPLE__)

#define _DARWIN_BETTER_REALPATH
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>


WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer1[MAXPATHLEN];
  char buffer2[MAXPATHLEN];
  char* path = buffer1;
  char* resolved = NULL;
  int length = -1;

  for (;;)
  {
    uint32_t size = (uint32_t)sizeof(buffer1);
    if (_NSGetExecutablePath(path, &size) == -1)
    {
      path = (char*)WAI_MALLOC(size);
      if (!_NSGetExecutablePath(path, &size))
        break;
    }

    resolved = bu_file_realpath(path, buffer2);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }

    break;
  }

  if (path != buffer1)
    WAI_FREE(path);

  if (length <= 0) {
    return _bu_getExecutablePath(out, capacity, dirname_length);
  }

  return length;
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[MAXPATHLEN];
  char* resolved = NULL;
  int length = -1;

  for(;;)
  {
    Dl_info info;

    if (dladdr(WAI_RETURN_ADDRESS(), &info))
    {
      resolved = bu_file_realpath(info.dli_fname, buffer);
      if (!resolved)
        break;

      length = (int)strlen(resolved);
      if (length <= capacity)
      {
        memcpy(out, resolved, length);

        if (dirname_length)
        {
          int i;

          for (i = length - 1; i >= 0; --i)
          {
            if (out[i] == '/')
            {
              *dirname_length = i;
              break;
            }
          }
        }
      }
    }

    break;
  }

  return length;
}

#elif defined(__QNXNTO__)

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#if !defined(WAI_PROC_SELF_EXE)
#define WAI_PROC_SELF_EXE "/proc/self/exefile"
#endif

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer1[MAXPATHLEN];
  char buffer2[MAXPATHLEN];
  char* resolved = NULL;
  FILE* self_exe = NULL;
  int length = -1;

  for (;;)
  {
    self_exe = fopen(WAI_PROC_SELF_EXE, "r");
    if (!self_exe)
      break;

    if (!bu_fgets(buffer1, sizeof(buffer1), self_exe))
      break;

    resolved = bu_file_realpath(buffer1, buffer2);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }

    break;
  }

  fclose(self_exe);

  if (length <= 0) {
    return _bu_getExecutablePath(out, capacity, dirname_length);
  }

  return length;
}

WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[MAXPATHLEN];
  char* resolved = NULL;
  int length = -1;

  for(;;)
  {
    Dl_info info;

    if (dladdr(WAI_RETURN_ADDRESS(), &info))
    {
      resolved = bu_file_realpath(info.dli_fname, buffer);
      if (!resolved)
        break;

      length = (int)strlen(resolved);
      if (length <= capacity)
      {
        memcpy(out, resolved, length);

        if (dirname_length)
        {
          int i;

          for (i = length - 1; i >= 0; --i)
          {
            if (out[i] == '/')
            {
              *dirname_length = i;
              break;
            }
          }
        }
      }
    }

    break;
  }

  return length;
}

#elif defined(__DragonFly__) || defined(__FreeBSD__) || \
      defined(__FreeBSD_kernel__) || defined(__NetBSD__)

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dlfcn.h>

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer1[MAXPATHLEN];
  char buffer2[MAXPATHLEN];
  char* path = buffer1;
  char* resolved = NULL;
  int length = -1;

  for (;;)
  {
#if defined(__NetBSD__)
    int mib[4] = { CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME };
#else
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
#endif
    size_t size = sizeof(buffer1);

    if (sysctl(mib, (u_int)(sizeof(mib) / sizeof(mib[0])), path, &size, NULL, 0) != 0)
        break;

    resolved = bu_file_realpath(path, buffer2);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }

    break;
  }

  if (path != buffer1)
    WAI_FREE(path);

  if (length <= 0) {
    return _bu_getExecutablePath(out, capacity, dirname_length);
  }

  return length;
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[MAXPATHLEN];
  char* resolved = NULL;
  int length = -1;

  for(;;)
  {
    Dl_info info;

    if (dladdr(WAI_RETURN_ADDRESS(), &info))
    {
      resolved = bu_file_realpath(info.dli_fname, buffer);
      if (!resolved)
        break;

      length = (int)strlen(resolved);
      if (length <= capacity)
      {
        memcpy(out, resolved, length);

        if (dirname_length)
        {
          int i;

          for (i = length - 1; i >= 0; --i)
          {
            if (out[i] == '/')
            {
              *dirname_length = i;
              break;
            }
          }
        }
      }
    }

    break;
  }

  return length;
}

#else

/* LIBBU fallback implementation */
WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
    return _bu_getExecutablePath(out, capacity, dirname_length);
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* UNUSED(out), int UNUSED(capacity), int* UNUSED(dirname_length))
{
    return -1;
}

#endif

#ifdef __cplusplus
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
