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

#ifndef CPUINFO_H
#define CPUINFO_H


typedef struct
{
  uint32_t endianness;
  uint32_t intellevel;
  uint32_t amdlevel;

  uint32_t arch;
  char vendorstring[12+1];
  char identifier[48+1];

  uint32_t vendor;
  uint32_t cpu_class;
  uint32_t family;
  uint32_t model;
  uint32_t cacheline;
  uint32_t clflushsize;

  int32_t cachelineL1code, cachesizeL1code, cacheassociativityL1code, cachesharedL1code;
  int32_t cachelineL1data, cachesizeL1data, cacheassociativityL1data, cachesharedL1data;
  int32_t cacheunifiedL1;
  int32_t cachelineL2, cachesizeL2, cacheassociativityL2, cachesharedL2;
  int32_t cachelineL3, cachesizeL3, cacheassociativityL3, cachesharedL3;

  int64_t sysmemory;
  uint32_t socketphysicalcores;
  uint32_t socketlogicalcores;
  uint32_t socketcount;
  uint32_t totalcorecount;
  uint32_t wordsize;
  uint32_t gpregs;
  uint32_t fpregs;


  /* CPU capabilities */
  unsigned capcmov:1;
  unsigned capclflush:1;
  unsigned captsc:1;
  unsigned capmmx:1;
  unsigned capmmxext:1;
  unsigned cap3dnow:1;
  unsigned cap3dnowext:1;
  unsigned capsse:1;
  unsigned capsse2:1;
  unsigned capsse3:1;
  unsigned capssse3:1;
  unsigned capsse4p1:1;
  unsigned capsse4p2:1;
  unsigned capsse4a:1;
  unsigned capavx:1;
  unsigned capavx2:1;
  unsigned capxop:1;
  unsigned capfma3:1;
  unsigned capfma4:1;
  unsigned capmisalignsse:1;
  unsigned capaes:1;
  unsigned cappclmul;
  unsigned caprdrnd:1;
  unsigned capcmpxchg16b:1;
  unsigned cappopcnt:1;
  unsigned caplzcnt:1;
  unsigned capmovbe:1;
  unsigned caprdtscp:1;
  unsigned capconstanttsc:1;
  unsigned capf16c:1;
  unsigned capbmi:1;
  unsigned capbmi2:1;
  unsigned captbm:1;
  unsigned caphyperthreading:1;
  unsigned capmwait:1;
  unsigned caplongmode:1;
  unsigned capthermalsensor:1;
  unsigned capclockmodulation:1;
  /* CPU capabilities */

} cpuInfo;


void cpuGetInfo( cpuInfo *cpu );

void cpuGetInfoEnv( cpuInfo *cpu );


/****/


enum
{
  CPUINFO_LITTLE_ENDIAN,
  CPUINFO_BIG_ENDIAN,
  CPUINFO_MIXED_ENDIAN
};


enum
{
  CPUINFO_ARCH_AMD64,
  CPUINFO_ARCH_IA32,
  CPUINFO_ARCH_UNKNOWN
};


enum
{
  CPUINFO_VENDOR_AMD,
  CPUINFO_VENDOR_INTEL,
  CPUINFO_VENDOR_UNKNOWN
};


enum
{
  CPUINFO_CLASS_STEAMROLLER,
  CPUINFO_CLASS_JAGUAR,
  CPUINFO_CLASS_PILEDRIVER,
  CPUINFO_CLASS_BULLDOZER,
  CPUINFO_CLASS_BOBCAT,
  CPUINFO_CLASS_BARCELONA,
  CPUINFO_CLASS_ATHLON64SSE3,
  CPUINFO_CLASS_ATHLON64,
  CPUINFO_CLASS_ATHLON,
  CPUINFO_CLASS_K6_2,
  CPUINFO_CLASS_K6,

  CPUINFO_CLASS_COREI7AVX2,
  CPUINFO_CLASS_COREI7AVX,
  CPUINFO_CLASS_COREI7,
  CPUINFO_CLASS_CORE2,
  CPUINFO_CLASS_NOCONA,
  CPUINFO_CLASS_PRESCOTT,
  CPUINFO_CLASS_PENTIUM4,
  CPUINFO_CLASS_PENTIUM3,
  CPUINFO_CLASS_PENTIUM2,

  CPUINFO_CLASS_I686,
  CPUINFO_CLASS_I586,

  CPUINFO_CLASS_AMD64GENERIC,
  CPUINFO_CLASS_IA32GENERIC,
  CPUINFO_CLASS_UNKNOWN,

  CPUINFO_CLASS_COUNT
};



#endif /* CPUINFO_H */


