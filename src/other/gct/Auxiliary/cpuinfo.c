/* *****************************************************************************
 *
 * Copyright (c) 2014 Alexis Naveros. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * *****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>


#include "cpuinfo.h"


#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64)
 #define ENV_ARCH_AMD64
#elif defined(__i386__) || defined(__i386)  || defined(i386)
 #define ENV_ARCH_IA32
#endif

#if defined(__linux__) || defined(__gnu_linux__) || defined(__linux) || defined(__linux)
 #include <sys/sysinfo.h>
 #define ENV_OS_LINUX
#endif

#if defined(__unix__) || defined(__unix) || defined(unix)
 #include <unistd.h>
 #define ENV_OS_UNIX
#endif

#if defined(_WIN32) || defined(WIN32)
 #include <windows.h>
 #include <winbase.h>
 #define ENV_OS_WINDOWS
#endif

#if defined(_MSC_VER)
 #include <intrin.h>
#endif



#define CAP001_ECX_SSE3 (1<<0)
#define CAP001_ECX_PCLMUL (1<<1)
#define CAP001_ECX_MWAIT (1<<3)
#define CAP001_ECX_SSSE3 (1<<9)
#define CAP001_ECX_FMA3 (1<<12)
#define CAP001_ECX_CMPXCHG16B (1<<13)
#define CAP001_ECX_SSE4_1 (1<<19)
#define CAP001_ECX_SSE4_2 (1<<20)
#define CAP001_ECX_POPCNT (1<<22)
#define CAP001_ECX_MOVBE (1<<23)
#define CAP001_ECX_AES (1<<25)
#define CAP001_ECX_AVX (1<<28)
#define CAP001_ECX_F16C (1<<29)
#define CAP001_ECX_RDRND (1<<30)

#define CAP001_EDX_TSC (1<<4)
#define CAP001_EDX_CMOV (1<<15)
#define CAP001_EDX_CLFLUSH (1<<19)
#define CAP001_EDX_MMX (1<<23)
#define CAP001_EDX_SSE (1<<25)
#define CAP001_EDX_SSE2 (1<<26)
#define CAP001_EDX_HYPERTHREADING (1<<28)

#define CAP006_EAX_THERMALSENSOR (1<<0)
#define CAP006_EAX_CLOCKMODULATION (1<<5)

#define CAP007_EBX_BMI (1<<3)
#define CAP007_EBX_AVX2 (1<<5)
#define CAP007_EBX_BMI2 (1<<8)

#define CAP801_ECX_NOHT (1<<1)
#define CAP801_ECX_LZCNT (1<<5)
#define CAP801_ECX_SSE4A (1<<6)
#define CAP801_ECX_MISALIGNSSE (1<<7)
#define CAP801_ECX_XOP (1<<11)
#define CAP801_ECX_FMA4 (1<<16)
#define CAP801_ECX_TBM (1<<21)

#define CAP801_EDX_MMXEXT (1<<22)
#define CAP801_EDX_RDTSCP (1<<27)
#define CAP801_EDX_LONGMODE (1<<29)
#define CAP801_EDX_3DNOWEXT (1<<30)
#define CAP801_EDX_3DNOW (1<<31)

#define CAP807_EDX_CONSTANTTSC (1<<8)


static void cpuInitInfo( cpuInfo *cpu )
{
  memset( cpu, 0, sizeof(cpuInfo) );
  sprintf( cpu->identifier, "Unknown" );
  cpu->cachesizeL1code = -1;
  cpu->cachesizeL1data = -1;
  cpu->cachesizeL2 = -1;
  cpu->cachesizeL3 = -1;
#if defined(ENV_ARCH_AMD64)
  cpu->arch = CPUINFO_ARCH_AMD64;
  cpu->wordsize = 64;
  cpu->gpregs = 16;
  cpu->fpregs = 16;
#elif defined(ENV_ARCH_IA32)
  cpu->arch = CPUINFO_ARCH_IA32;
  cpu->wordsize = 32;
  cpu->gpregs = 8;
  cpu->fpregs = 8;
#else
  cpu->arch = CPUINFO_ARCH_UNKNOWN;
  cpu->wordsize = sizeof(void *) * CHAR_BIT;
#endif
  cpu->class = CPUINFO_CLASS_UNKNOWN;
  return;
}


static void cpuGetEndian( cpuInfo *cpu )
{
  static const uint32_t ref = 0x44332211;
  if( ( ((char *)&ref)[0] == 0x11 ) && ( ((char *)&ref)[1] == 0x22 ) && ( ((char *)&ref)[2] == 0x33 ) && ( ((char *)&ref)[3] == 0x44 ) )
    cpu->endianness = CPUINFO_LITTLE_ENDIAN;
  else if( ( ((char *)&ref)[0] == 0x44 ) && ( ((char *)&ref)[1] == 0x33 ) && ( ((char *)&ref)[2] == 0x22 ) && ( ((char *)&ref)[3] == 0x11 ) )
    cpu->endianness = CPUINFO_LITTLE_ENDIAN;
  else
    cpu->endianness = CPUINFO_MIXED_ENDIAN;
  return;
}


#if defined(ENV_ARCH_IA32) || defined(ENV_ARCH_AMD64)


#ifdef ENV_ARCH_IA32

 #define EFLAG_CPUID 0x00200000

static int cpuEflagCheck( uint32_t mask )
{
 #if defined(__GNUC__)
  uint32_t t0, t1;
  asm("pushfl\n\t"
      "pushfl\n\t"
      "popl %0\n\t"
      "movl %0, %1\n\t"
      "xorl %2, %1\n\t"
      "pushl %1\n\t"
      "popfl\n\t"
      "pushfl\n\t"
      "popl %1\n\t"
      "popfl\n\t"
      : "=&r" (t0), "=&r" (t1)
      : "ri" (mask));
  return ( ( t0 ^ t1 ) & mask );
 #else
  return 1;
 #endif
}

#endif


static void cpuCpuid( uint32_t level, uint32_t index, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx )
{
#if defined(__GNUC__)
 #ifdef ENV_ARCH_AMD64
  asm("cpuid"
    : "=a" (*eax),
      "=b" (*ebx),
      "=c" (*ecx),
      "=d" (*edx)
    : "0" (level), "c"(index) );
 #endif
 #ifdef ENV_ARCH_IA32
  asm("movl %%ebx, %%esi\n\t"
      "cpuid\n\t"
      "movl %3, %%edi\n\t"
      "movl %%ebx, (%%edi)\n\t"
      "movl %%esi, %%ebx\n\t"
    : "=a" (*eax), "=c" (*ecx), "=d" (*edx)
    : "m" (ebx), "a" (level), "c"(index) : "edi", "esi", "memory" );
 #endif
#elif defined(_MSC_VER)
  int regs[4];
  __cpuidex( regs, level, index );
  *eax = regs[0];
  *ebx = regs[1];
  *ecx = regs[2];
  *edx = regs[3];
#else
 #warning WARNING: Unknown compiler, we do not know how to execute CPUID instructions
  *eax = 0;
  *ebx = 0;
  *ecx = 0;
  *edx = 0;
#endif
  return;
}


static int cpuGetGeneral( cpuInfo *cpu )
{
  uint32_t eax, ebx, ecx, edx, vendorstring[3];

#ifdef ENV_ARCH_IA32
  if( !( cpuEflagCheck( EFLAG_CPUID ) ) )
    return 0;
#endif

  cpuCpuid( 0x00000000, 0, &cpu->intellevel, &vendorstring[0], &vendorstring[2], &vendorstring[1] );
  memcpy( cpu->vendorstring, vendorstring, 3*sizeof(uint32_t) );
  if( !( strcmp( cpu->vendorstring, "GenuineIntel" ) ) )
    cpu->vendor = CPUINFO_VENDOR_INTEL;
  else if( !( strcmp( cpu->vendorstring, "AuthenticAMD" ) ) )
    cpu->vendor = CPUINFO_VENDOR_AMD;
  else
    cpu->vendor = CPUINFO_VENDOR_UNKNOWN;

  /* Intel flags */
  if( ( cpu->intellevel >= 0x00000001 ) && ( cpu->intellevel <= 0x0000ffff ) )
  {
    cpuCpuid( 0x00000001, 0, &eax, &ebx, &ecx, &edx );
    cpu->family = ( eax >> 8 ) & 0xf;
    if( cpu->family == 0xf )
      cpu->family += ( eax >> 20 ) & 0xff;
    cpu->model = ( eax >> 4 ) & 0xf;
    if( cpu->family >= 6 )
      cpu->model += ( ( eax >> 16 ) & 0xf ) << 4;
    cpu->socketlogicalcores = ( ebx >> 16 ) & 0xff;
    if( ecx & CAP001_ECX_SSE3 )
      cpu->capsse3 = 1;
    if( ecx & CAP001_ECX_PCLMUL )
      cpu->cappclmul = 1;
    if( ecx & CAP001_ECX_MWAIT )
      cpu->capmwait = 1;
    if( ecx & CAP001_ECX_SSSE3 )
      cpu->capssse3 = 1;
    if( ecx & CAP001_ECX_FMA3 )
      cpu->capfma3 = 1;
    if( ecx & CAP001_ECX_CMPXCHG16B )
      cpu->capcmpxchg16b = 1;
    if( ecx & CAP001_ECX_SSE4_1 )
      cpu->capsse4p1 = 1;
    if( ecx & CAP001_ECX_SSE4_2 )
      cpu->capsse4p2 = 1;
    if( ecx & CAP001_ECX_POPCNT )
      cpu->cappopcnt = 1;
    if( ecx & CAP001_ECX_MOVBE )
      cpu->capmovbe = 1;
    if( ecx & CAP001_ECX_AES )
      cpu->capaes = 1;
    if( ecx & CAP001_ECX_AVX )
      cpu->capavx = 1;
    if( ecx & CAP001_ECX_F16C )
      cpu->capf16c = 1;
    if( ecx & CAP001_ECX_RDRND )
      cpu->caprdrnd = 1;
    if( edx & CAP001_EDX_TSC )
      cpu->captsc = 1;
    if( edx & CAP001_EDX_CMOV )
      cpu->capcmov = 1;
    if( edx & CAP001_EDX_CLFLUSH )
    {
      cpu->capclflush = 1;
      cpu->clflushsize = ( ( ebx >> 8 ) & 0xff ) * 8;
      cpu->cacheline = cpu->clflushsize;
    }
    if( edx & CAP001_EDX_MMX )
      cpu->capmmx = 1;
    if( edx & CAP001_EDX_SSE )
      cpu->capsse = 1;
    if( edx & CAP001_EDX_SSE2 )
      cpu->capsse2 = 1;
    if( edx & CAP001_EDX_HYPERTHREADING )
      cpu->caphyperthreading = 1;
  }

  /* Thermal and clock management */
  if( ( cpu->intellevel >= 0x00000006 ) && ( cpu->intellevel <= 0x0000ffff ) )
  {
    cpuCpuid( 0x00000006, 0, &eax, &ebx, &ecx, &edx );
    if( eax & CAP006_EAX_THERMALSENSOR )
      cpu->capthermalsensor = 1;
    if( eax & CAP006_EAX_CLOCKMODULATION )
      cpu->capclockmodulation = 1;
  }

  /* Flags */
  if( ( cpu->intellevel >= 0x00000007 ) && ( cpu->intellevel <= 0x0000ffff ) )
  {
    cpuCpuid( 0x00000006, 0, &eax, &ebx, &ecx, &edx );
    if( eax & CAP007_EBX_BMI )
      cpu->capbmi = 1;
    if( eax & CAP007_EBX_AVX2 )
      cpu->capavx2 = 1;
    if( eax & CAP007_EBX_BMI2 )
      cpu->capbmi2 = 1;
  }

  /* AMD flags */
  cpuCpuid( 0x80000000, 0, &cpu->amdlevel, &ebx, &ecx, &edx );
  if( ( cpu->amdlevel >= 0x80000001 ) && ( cpu->amdlevel <= 0x8000ffff ) )
  {
    cpuCpuid( 0x80000001, 0, &eax, &ebx, &ecx, &edx );
    if( ecx & CAP801_ECX_NOHT )
      cpu->caphyperthreading = 0;
    if( ecx & CAP801_ECX_LZCNT )
      cpu->caplzcnt = 1;
    if( ecx & CAP801_ECX_SSE4A )
      cpu->capsse4a = 1;
    if( ecx & CAP801_ECX_MISALIGNSSE )
      cpu->capmisalignsse = 1;
    if( ecx & CAP801_ECX_XOP )
      cpu->capxop = 1;
    if( ecx & CAP801_ECX_FMA4 )
      cpu->capfma4 = 1;
    if( ecx & CAP801_ECX_TBM )
      cpu->captbm = 1;
    if( edx & CAP801_EDX_MMXEXT )
      cpu->capmmxext = 1;
    if( edx & CAP801_EDX_RDTSCP )
      cpu->caprdtscp = 1;
    if( edx & CAP801_EDX_LONGMODE )
      cpu->caplongmode = 1;
    if( edx & CAP801_EDX_3DNOW )
      cpu->cap3dnow = 1;
    if( edx & CAP801_EDX_3DNOWEXT )
      cpu->cap3dnowext = 1;
  }

  if( ( cpu->amdlevel >= 0x80000007 ) && ( cpu->amdlevel <= 0x8000ffff ) )
  {
    cpuCpuid( 0x80000001, 0, &eax, &ebx, &ecx, &edx );
    if( edx & CAP807_EDX_CONSTANTTSC )
      cpu->capconstanttsc = 1;
  }

  return 1;
}


////


static void cpuGetAmdModelName( cpuInfo *cpu )
{
  uint32_t *id;
  char *c;

  if( cpu->amdlevel >= 0x80000004 )
  {
    id = (uint32_t *)cpu->identifier;
    cpuCpuid( 0x80000002, 0, &id[0], &id[1], &id[2], &id[3] );
    cpuCpuid( 0x80000003, 0, &id[4], &id[5], &id[6], &id[7] );
    cpuCpuid( 0x80000004, 0, &id[8], &id[9], &id[10], &id[11] );
    cpu->identifier[48] = 0;
    for( c = cpu->identifier ; *c == ' ' ; c++ );
    if( c != cpu->identifier )
      memmove( cpu->identifier, c, strlen(c)+1 );
  }

  return;
}


static void cpuGetCacheOld( cpuInfo *cpu )
{
  uint32_t eax, ebx, ecx, edx;
  static const int cacheways[0x10] =
  {
    [0x0] = 0,
    [0x1] = 1,
    [0x2] = 2,
    [0x3] = 3, /* ??? */
    [0x4] = 4,
    [0x5] = 6, /* ??? */
    [0x6] = 8,
    [0x7] = 12, /* ??? */
    [0x8] = 16,
    [0x9] = 24, /* ??? */
    [0xa] = 32,
    [0xb] = 48,
    [0xc] = 64,
    [0xd] = 96,
    [0xe] = 128,
    [0xf] = 256
  };

  if( ( cpu->amdlevel >= 0x80000005 ) && ( ( cpu->cachesizeL1code == -1 ) || ( cpu->cachesizeL1data == -1 ) ) )
  {
    cpuCpuid( 0x80000005, 0, &eax, &ebx, &ecx, &edx );
    if( ( ecx | edx ) )
    {
      cpu->cachesizeL1code = edx >> 24;
      cpu->cachelineL1code = edx & 0xff;
      cpu->cacheassociativityL1code = ( edx >> 8 ) & 0xff;
      cpu->cachesizeL1data = ecx >> 24;
      cpu->cachelineL1data = ecx & 0xff;
      cpu->cacheassociativityL1data = ( ecx >> 8 ) & 0xff;
    }
  }
  if( ( cpu->amdlevel >= 0x80000006 ) && ( ( cpu->cachesizeL2 == -1 ) || ( cpu->cachesizeL3 == -1 ) ) )
  {
    cpuCpuid( 0x80000006, 0, &eax, &ebx, &ecx, &edx );
    if( ( ecx | edx ) )
    {
      cpu->cachesizeL2 = ecx >> 16;
      cpu->cachelineL2 = ecx & 0xff;
      cpu->cacheassociativityL2 = cacheways[ ( ecx >> 12 ) & 0xf ];
      cpu->cachesizeL3 = ( edx >> 18 ) * 512;
      cpu->cachelineL3 = edx & 0xff;
      cpu->cacheassociativityL3 = cacheways[ ( edx >> 12 ) & 0xf ];
    }
  }

  return;
}


static void cpuGetCores( cpuInfo *cpu )
{
  uint32_t eax, ebx, ecx, edx;

  if( cpu->vendor == CPUINFO_VENDOR_INTEL )
  {
    if( cpu->intellevel >= 0x00000004 )
    {
      cpuCpuid( 0x00000004, 0, &eax, &ebx, &ecx, &edx );
      if( eax & 0x1f )
        cpu->socketphysicalcores = ( ( eax >> 26 ) & 0x3f ) + 1;
      if( cpu->socketphysicalcores == cpu->socketlogicalcores )
        cpu->caphyperthreading = 0;
    }
  }

  if( cpu->vendor == CPUINFO_VENDOR_AMD )
  {
    if( cpu->amdlevel >= 0x80000008 )
    {
      cpuCpuid( 0x80000008, 0, &eax, &ebx, &ecx, &edx );
      cpu->socketphysicalcores = ( ecx & 0xff ) + 1;
    }
  }

  return;
}


static void cpuGetCacheNew( cpuInfo *cpu )
{
  uint32_t cpuidindex;
  uint32_t eax, ebx, ecx, edx, queryindex;
  uint32_t cachetype, cachelevel, cacheline, cacheassociativity, cachepartitions, cachesets, cachesize, cacheshared;

  cpuidindex = 0;
  if( cpu->vendor == CPUINFO_VENDOR_INTEL )
  {
    if( cpu->intellevel >= 4 )
      cpuidindex = 0x00000004;
  }
  else if( cpu->vendor == CPUINFO_VENDOR_AMD )
  {
    if( cpu->amdlevel >= 0x8000001d )
      cpuidindex = 0x8000001d;
  }
  else
  {
    if( cpu->intellevel >= 4 )
      cpuidindex = 0x00000004;
  }

  if( cpuidindex )
  {
    for( queryindex = 0 ; queryindex < 0x10 ; queryindex++ )
    {
      cpuCpuid( cpuidindex, queryindex, &eax, &ebx, &ecx, &edx );
      cachetype = eax & 0x1f;
      if( !( cachetype ) )
        break;
      cachelevel = ( eax >> 5 ) & 0x7;
      cacheline = ( ebx & 0x0fff ) + 1;
      cachepartitions = ( ( ebx >> 12 ) & 0x03ff ) + 1;
      cacheassociativity = ( ( ebx >> 22 ) & 0x03ff ) + 1;
      cachesets = ecx + 1;
      cachesize = ( cacheassociativity * cachepartitions * cacheline * cachesets ) / 1024;
      cacheshared = ( ( eax >> 14 ) & 0xfff ) + 1;
      switch( cachelevel )
      {
        case 1:
          if( cachetype & 0x1 )
          {
            cpu->cachesizeL1data = cachesize;
            cpu->cachelineL1data = cacheline;
            cpu->cacheassociativityL1data = cacheassociativity;
            cpu->cachesharedL1data = cacheshared;
          }
          if( cachetype & 0x2 )
          {
            cpu->cachesizeL1code = cachesize;
            cpu->cachelineL1code = cacheline;
            cpu->cacheassociativityL1code = cacheassociativity;
            cpu->cachesharedL1code = cacheshared;
          }
          if( cachetype == 0x3 )
            cpu->cacheunifiedL1 = 1;
          break;
        case 2:
          cpu->cachesizeL2 = cachesize;
          cpu->cachelineL2 = cacheline;
          cpu->cacheassociativityL2 = cacheassociativity;
          cpu->cachesharedL2 = cacheshared;
          break;
        case 3:
          cpu->cachesizeL3 = cachesize;
          cpu->cachelineL3 = cacheline;
          cpu->cacheassociativityL3 = cacheassociativity;
          cpu->cachesharedL3 = cacheshared;
          break;
      }
    }
  }

  return;
}


#endif


static void cpuGetCpuCount( cpuInfo *cpu )
{
#if defined(ENV_OS_LINUX)
  cpu->totalcorecount = get_nprocs();
#elif defined(ENV_OS_WINDOWS)
  SYSTEM_INFO si;
  GetSystemInfo( &si );
  cpu->totalcorecount = si.dwNumberOfProcessors;
#elif defined(ENV_OS_UNIX)
  cpu->totalcorecount = sysconf( _SC_NPROCESSORS_CONF );
#endif
  if( cpu->socketlogicalcores )
    cpu->socketcount = cpu->totalcorecount / cpu->socketlogicalcores;
  return;
}


static void cpuGetSystemMemory( cpuInfo *cpu )
{
#ifdef ENV_OS_UNIX
  int64_t pages;
  pages = sysconf( _SC_PHYS_PAGES );
  if( pages < 0 )
    return;
  cpu->sysmemory = pages * (uint64_t)sysconf( _SC_PAGESIZE );
  if( cpu->sysmemory < 0 )
    cpu->sysmemory = 0;
#endif
  return;
}


static void cpuGetClass( cpuInfo *cpu )
{
  if( cpu->class != CPUINFO_CLASS_UNKNOWN )
    return;
  if( cpu->vendor == CPUINFO_VENDOR_AMD )
  {
    if( cpu->family == 0xf+0x7 )
      cpu->class = CPUINFO_CLASS_JAGUAR;
    else if( cpu->family == 0xf+0x5 )
      cpu->class = CPUINFO_CLASS_BOBCAT;
    else if( cpu->capbmi )
    {
      /*
       * TODO: I have NO IDEA how to detect a steamroller CPU, I checked every AMD resources I could find.
       * Okay, maybe I should wait for the chip to come out first.
       */
      if( 0 )
        cpu->class = CPUINFO_CLASS_STEAMROLLER;
      else
        cpu->class = CPUINFO_CLASS_PILEDRIVER;
    }
    else if( cpu->capfma4 )
      cpu->class = CPUINFO_CLASS_BULLDOZER;
    else if( cpu->capsse4a )
      cpu->class = CPUINFO_CLASS_BARCELONA;
    else if( cpu->capsse3 )
      cpu->class = CPUINFO_CLASS_ATHLON64SSE3;
    else if( cpu->capsse2 )
      cpu->class = CPUINFO_CLASS_ATHLON64;
    else if( cpu->capsse )
      cpu->class = CPUINFO_CLASS_ATHLON;
    else if( cpu->cap3dnow )
      cpu->class = CPUINFO_CLASS_K6_2;
    else if( cpu->capmmx )
      cpu->class = CPUINFO_CLASS_K6;
    else if( cpu->family >= 0x5 )
      cpu->class = CPUINFO_CLASS_I586;
  }
  else if( cpu->vendor == CPUINFO_VENDOR_INTEL )
  {
    if( cpu->caplongmode )
    {
      if( cpu->capavx2 )
        cpu->class = CPUINFO_CLASS_COREI7AVX2;
      else if( cpu->capavx )
        cpu->class = CPUINFO_CLASS_COREI7AVX;
      else if( cpu->capsse4p2 )
        cpu->class = CPUINFO_CLASS_COREI7;
      else if( cpu->capssse3 )
        cpu->class = CPUINFO_CLASS_CORE2;
      else
        cpu->class = CPUINFO_CLASS_NOCONA;
    }
    else if( cpu->family >= 0x6 )
    {
      if( cpu->capsse3 )
        cpu->class = CPUINFO_CLASS_PRESCOTT;
      else if( cpu->capsse2 )
        cpu->class = CPUINFO_CLASS_PENTIUM4;
      else if( cpu->capsse )
        cpu->class = CPUINFO_CLASS_PENTIUM3;
      else
        cpu->class = CPUINFO_CLASS_PENTIUM2;
    }
    else if( cpu->family >= 0x5 )
      cpu->class = CPUINFO_CLASS_I586;
  }
  else
  {
    if( cpu->arch == CPUINFO_ARCH_AMD64 )
      cpu->class = CPUINFO_CLASS_AMD64GENERIC;
    else if( cpu->arch == CPUINFO_ARCH_IA32 )
      cpu->class = CPUINFO_CLASS_IA32GENERIC;
    else
      cpu->class = CPUINFO_CLASS_UNKNOWN;
  }
  return;
}


typedef struct
{
  int32_t l1i;
  int32_t l1d;
  int32_t l2;
  int32_t l3;
} cpuClassCache;

/*
A table of per-class cache sizes, if they are not already known.
We pretty much have to vaguely guess the cache sizes down here...
*/
static cpuClassCache cpuClassCacheTable[CPUINFO_CLASS_COUNT] =
{
 [CPUINFO_CLASS_STEAMROLLER] = { 64, 64, 2048, 8192 },
 [CPUINFO_CLASS_JAGUAR] = { 32, 32, 2048, 0 },
 [CPUINFO_CLASS_PILEDRIVER] = { 64, 64, 2048, 8192 },
 [CPUINFO_CLASS_BULLDOZER] = { 64, 64, 2048, 8192 },
 [CPUINFO_CLASS_BOBCAT] = { 32, 32, 1024, 0 },
 [CPUINFO_CLASS_BARCELONA] = { 64, 64, 2048, 0 },
 [CPUINFO_CLASS_ATHLON64SSE3] = { 64, 64, 1024, 0 },
 [CPUINFO_CLASS_ATHLON64] = { 64, 64, 1024, 0 },
 [CPUINFO_CLASS_ATHLON] = { 32, 32, 512, 0 },
 [CPUINFO_CLASS_K6_2] = { 8, 8, 256, 0 },
 [CPUINFO_CLASS_K6] = { 8, 8, 256, 0 },

 [CPUINFO_CLASS_COREI7AVX2] = { 32, 32, 256, 8192 },
 [CPUINFO_CLASS_COREI7AVX] = { 32, 32, 256, 8192 },
 [CPUINFO_CLASS_COREI7] = { 32, 32, 256, 8192 },
 [CPUINFO_CLASS_CORE2] = { 32, 32, 4096, 0 },
 [CPUINFO_CLASS_NOCONA] = { 64, 8, 2048, 0 },
 [CPUINFO_CLASS_PRESCOTT] = { 64, 8, 1024, 0 },
 [CPUINFO_CLASS_PENTIUM4] = { 64, 8, 512, 0 },
 [CPUINFO_CLASS_PENTIUM3] = { 16, 16, 256, 0 },
 [CPUINFO_CLASS_PENTIUM2] = { 16, 16, 256, 0 },

 [CPUINFO_CLASS_I686] = { 8, 8, 256, 0 },
 [CPUINFO_CLASS_I586] = { 8, 8, 128, 0 },

 [CPUINFO_CLASS_AMD64GENERIC] = { 64, 64, 512, 0 },
 [CPUINFO_CLASS_IA32GENERIC] = { 8, 8, 128, 0 },
 [CPUINFO_CLASS_UNKNOWN] = { 0, 0, 0, 0 }
};

static void cpuGuessCaches( cpuInfo *cpu )
{
  int32_t cacheline;
  cpuClassCache *cache;

  cache = &cpuClassCacheTable[ cpu->class ];
  cacheline = cpu->cacheline;
  if( cpu->cachesizeL1code < 0 )
  {
    cpu->cachesizeL1code = cache->l1i;
    if( cpu->cachesizeL1code )
      cpu->cachelineL1code = cacheline;
  }
  if( cpu->cachesizeL1data < 0 )
  {
    cpu->cachesizeL1data = cache->l1d;
    if( cpu->cachesizeL1data )
      cpu->cachelineL1data = cacheline;
  }
  if( cpu->cachesizeL2 < 0 )
  {
    cpu->cachesizeL2 = cache->l2;
    if( cpu->cachesizeL2 )
      cpu->cachelineL2 = cacheline;
  }
  if( cpu->cachesizeL3 < 0 )
  {
    cpu->cachesizeL3 = cache->l3;
    if( cpu->cachesizeL3 )
      cpu->cachelineL3 = cacheline;
  }

  return;
}


/* A last pass to apply erratas */
static void cpuApplyLastFixes( cpuInfo *cpu )
{
/*
  if( ( cpu->class == CPUINFO_CLASS_PENTIUM4 ) && ( cpu->cacheline == 64 ) )
    cpu->cacheload = 128;
*/
  if( ( ( cpu->family == 0xf ) && ( cpu->model >= 0x03 ) ) || ( ( cpu->family == 0x6 ) && ( cpu->model >= 0x0e ) ) )
    cpu->capconstanttsc = 1;
  return;
}


////


static void cpuGetEnv( cpuInfo *cpu )
{
  /* Default cache line */
  cpu->cacheline = 64;
  cpu->clflushsize = 64;
#if defined(__tune_bdver3__)
  sprintf( cpu->identifier, "AMD Steamroller Class CPU" );
  cpu->class = CPUINFO_CLASS_STEAMROLLER;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capsse4a = 1;
  cpu->capsse4p1 = 1;
  cpu->capsse4p2 = 1;
  cpu->capfma4 = 1;
  cpu->capxop = 1;
  cpu->caplzcnt = 1;
  cpu->cappopcnt = 1;
  cpu->capaes = 1;
  cpu->capf16c = 1;
  cpu->capcmpxchg16b = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_bdver2__)
  sprintf( cpu->identifier, "AMD Piledriver Class CPU" );
  cpu->class = CPUINFO_CLASS_PILEDRIVER;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capsse4a = 1;
  cpu->capsse4p1 = 1;
  cpu->capsse4p2 = 1;
  cpu->capfma4 = 1;
  cpu->capxop = 1;
  cpu->caplzcnt = 1;
  cpu->cappopcnt = 1;
  cpu->capaes = 1;
  cpu->capf16c = 1;
  cpu->capcmpxchg16b = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_bdver1__)
  sprintf( cpu->identifier, "AMD Bulldozer Class CPU" );
  cpu->class = CPUINFO_CLASS_BULLDOZER;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capsse4a = 1;
  cpu->capsse4p1 = 1;
  cpu->capsse4p2 = 1;
  cpu->capfma4 = 1;
  cpu->capxop = 1;
  cpu->caplzcnt = 1;
  cpu->cappopcnt = 1;
  cpu->capaes = 1;
  cpu->capf16c = 1;
  cpu->capcmpxchg16b = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_btver2__)
  sprintf( cpu->identifier, "AMD Jaguar Class CPU" );
  cpu->class = CPUINFO_CLASS_JAGUAR;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capssse3 = 1;
  cpu->capsse4a = 1;
  cpu->capsse4p1 = 1;
  cpu->capsse4p2 = 1;
  cpu->capfma4 = 1;
  cpu->caplzcnt = 1;
  cpu->cappopcnt = 1;
  cpu->capaes = 1;
  cpu->capf16c = 1;
  cpu->capcmpxchg16b = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_btver1__)
  sprintf( cpu->identifier, "AMD Bobcat Class CPU" );
  cpu->class = CPUINFO_CLASS_BOBCAT;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capssse3 = 1;
  cpu->capsse4a = 1;
  cpu->capfma4 = 1;
  cpu->caplzcnt = 1;
  cpu->cappopcnt = 1;
  cpu->capaes = 1;
  cpu->capcmpxchg16b = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_amdfam10__)
  sprintf( cpu->identifier, "AMD Barcelona Class CPU" );
  cpu->class = CPUINFO_CLASS_BARCELONA;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capssse3 = 1;
  cpu->capsse4a = 1;
  cpu->caplzcnt = 1;
  cpu->cappopcnt = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_k8__)
  sprintf( cpu->identifier, "AMD Athlon64 Class CPU" );
  cpu->class = CPUINFO_CLASS_ATHLON64;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->caplongmode = 1;
 #if __SSE3__
  cpu->class = CPUINFO_CLASS_ATHLON64SSE3;
  cpu->capsse3 = 1;
 #endif
#elif defined(__tune_athlon__)
  sprintf( cpu->identifier, "AMD Athlon Class CPU" );
  cpu->class = CPUINFO_CLASS_ATHLON;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capmmxext = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->cap3dnow = 1;
  cpu->cap3dnowext = 1;
#elif defined(__tune_k6__)
  sprintf( cpu->identifier, "AMD K6 Class CPU" );
  cpu->class = CPUINFO_CLASS_K6;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->cap3dnow = 1;
  cpu->cacheline = 32;
  cpu->clflushsize = 32;
#elif defined(__tune_corei7__)
  sprintf( cpu->identifier, "Core i7 Class CPU" );
  cpu->class = CPUINFO_CLASS_COREI7;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capssse3 = 1;
  cpu->capsse4a = 1;
  cpu->capsse4p1 = 1;
  cpu->capsse4p2 = 1;
  cpu->caplongmode = 1;
 #if __AVX__
  cpu->class = CPUINFO_CLASS_COREI7AVX;
  cpu->capavx = 1;
 #endif
 #if __AVX2__
  cpu->class = CPUINFO_CLASS_COREI7AVX2;
  cpu->capavx2 = 1;
  cpu->capbmi = 1;
  cpu->capbmi2 = 1;
 #endif
#elif defined(__tune_core2__)
  sprintf( cpu->identifier, "Core2 Class CPU" );
  cpu->class = CPUINFO_CLASS_CORE2;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->capssse3 = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_nocona__)
  sprintf( cpu->identifier, "Nocona Class CPU" );
  cpu->class = CPUINFO_CLASS_NOCONA;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->capsse3 = 1;
  cpu->caplongmode = 1;
#elif defined(__tune_pentium4__)
  sprintf( cpu->identifier, "Intel Pentium 4 Class CPU" );
  cpu->class = CPUINFO_CLASS_PENTIUM4;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->capsse2 = 1;
  cpu->caphyperthreading = 1;
#elif defined(__tune_pentium3__)
  sprintf( cpu->identifier, "Intel Pentium III Class CPU" );
  cpu->class = CPUINFO_CLASS_PENTIUM3;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->capsse = 1;
  cpu->cacheline = 32;
  cpu->clflushsize = 32;
#elif defined(__tune_i686__) || defined(__tune_pentium2__)
  sprintf( cpu->identifier, "Intel Pentium II Class CPU" );
  cpu->class = CPUINFO_CLASS_I686;
  cpu->capcmov = 1;
  cpu->capmmx = 1;
  cpu->cacheline = 32;
  cpu->clflushsize = 32;
#elif defined(__tune_i586__)
  sprintf( cpu->identifier, "Intel Pentium Class CPU" );
  cpu->class = CPUINFO_CLASS_I586;
  cpu->capcmov = 1;
  cpu->cacheline = 32;
  cpu->clflushsize = 32;
#else
  if( cpu->arch == CPUINFO_ARCH_AMD64 )
  {
    cpu->class = CPUINFO_CLASS_AMD64GENERIC;
    cpu->capcmov = 1;
    cpu->capmmx = 1;
    cpu->capsse = 1;
    cpu->capsse2 = 1;
    cpu->caplongmode = 1;
  }
  else if( cpu->arch == CPUINFO_ARCH_IA32 )
    cpu->class = CPUINFO_CLASS_IA32GENERIC;
  else
    cpu->class = CPUINFO_CLASS_UNKNOWN;
#endif

#if __MMX__
  cpu->capmmx = 1;
#endif
#if __3dNOW__
  cpu->cap3dnow = 1;
#endif
#if __3dNOW_A__
  cpu->cap3dnowext = 1;
#endif
#if __SSE__
  cpu->capsse = 1;
#endif
#if __SSE2__
  cpu->capsse2 = 1;
#endif
#if __SSE3__
  cpu->capsse3 = 1;
#endif
#if __SSSE3__
  cpu->capssse3 = 1;
#endif
#if __SSE4_1__
  cpu->capsse4p1 = 1;
#endif
#if __SSE4_2__
  cpu->capsse4p2 = 1;
#endif
#if __AVX__
  cpu->capavx = 1;
#endif
#if __AVX2__
  cpu->capavx2 = 1;
#endif
#if __XOP__
  cpu->capxop = 1;
#endif
#if __FMA__
  cpu->capfma3 = 1;
#endif
#if __FMA4__
  cpu->capfma4 = 1;
#endif
#if __POPCNT__
  cpu->cappopcnt = 1;
#endif
#if __LZCNT__
  cpu->caplzcnt = 1;
#endif
#if __AES__
  cpu->capaes = 1;
#endif
#if __F16C__
  cpu->capf16c = 1;
#endif
#if __PCLMUL__
  cpu->cappclmul = 1;
#endif
#if __RDRAND__
  cpu->caprdrnd = 1;
#endif
#if __BMI__
  cpu->capbmi = 1;
#endif
#if __BMI2__
  cpu->capbmi2 = 1;
#endif
#if __TBM__
  cpu->captbm = 1;
#endif

  return;
}


////


void cpuGetInfo( cpuInfo *cpu )
{
  cpuInitInfo( cpu );
  cpuGetEndian( cpu );
#if defined(ENV_ARCH_IA32) || defined(ENV_ARCH_AMD64)
  cpuGetGeneral( cpu );
  cpuGetAmdModelName( cpu );
  cpuGetCores( cpu );
  cpuGetCacheNew( cpu );
  cpuGetCacheOld( cpu );
  cpuGetCpuCount( cpu );
  cpuGetSystemMemory( cpu );
#endif
  cpuGetClass( cpu );
  cpuGuessCaches( cpu );
  cpuApplyLastFixes( cpu );
  return;
}



void cpuGetInfoEnv( cpuInfo *cpu )
{
  cpuInitInfo( cpu );
  cpuGetEndian( cpu );
  cpuGetEnv( cpu );
  cpuGetClass( cpu );
  cpuGuessCaches( cpu );
  cpuApplyLastFixes( cpu );
  return;
}



