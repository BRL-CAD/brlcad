#ifndef PROTOS_H
#define PROTOS_H

#include <elf.h>
#include "bswap.h"
#include "config.h"

#ifdef WORDS_BIGENDIAN
#define ELFDATA2 ELFDATA2MSB
#else
#define ELFDATA2 ELFDATA2LSB
#endif
#if SIZEOF_VOID_P != 8 && SIZEOF_VOID_P != 4
#error "Unknown word size (SIZEOF_VOID_P)!"
#endif

typedef union {
  unsigned char e_ident[EI_NIDENT];
  Elf32_Ehdr e32;
  Elf64_Ehdr e64;
} Elf_Ehdr;

typedef union {
  Elf32_Shdr e32;
  Elf64_Shdr e64;
} Elf_Shdr;

typedef union {
  Elf32_Phdr e32;
  Elf64_Phdr e64;
} Elf_Phdr;

int is_e32(void);
int swap_bytes(void);

#define DO_SWAPU16(x) ( !swap_bytes() ? x : (uint16_t)bswap_16(x) )
#define DO_SWAPU32(x) ( !swap_bytes() ? x : (uint32_t)bswap_32(x) )
#define DO_SWAPU64(x) ( !swap_bytes() ? x : (uint64_t)bswap_64(x) )
#define DO_SWAPS16(x) ( !swap_bytes() ? x : (int16_t)bswap_16(x) )
#define DO_SWAPS32(x) ( !swap_bytes() ? x : (int32_t)bswap_32(x) )
#define DO_SWAPS64(x) ( !swap_bytes() ? x : (int64_t)bswap_64(x) )

#define EHDRWS(x) (is_e32() ? DO_SWAPS32(ehdr.e32.x) : DO_SWAPS64(ehdr.e64.x))
#define EHDRHS(x) (is_e32() ? DO_SWAPS16(ehdr.e32.x) : DO_SWAPS16(ehdr.e64.x))
#define EHDRWU(x) (is_e32() ? DO_SWAPU32(ehdr.e32.x) : DO_SWAPU64(ehdr.e64.x))
#define EHDRHU(x) (is_e32() ? DO_SWAPU16(ehdr.e32.x) : DO_SWAPU16(ehdr.e64.x))
#define PHDR(x) (is_e32() ? DO_SWAPU32(phdr.e32.x) : DO_SWAPU64(phdr.e64.x))
#define SHDR_W(x) (is_e32() ? DO_SWAPU32(shdr.e32.x) : DO_SWAPU32(shdr.e64.x))
#define SHDR_O(x) (is_e32() ? DO_SWAPU32(shdr.e32.x) : DO_SWAPU64(shdr.e64.x))
#define DYNSU(i,x) (is_e32() ? DO_SWAPU32(((Elf32_Dyn *)dyns)[i].x) \
  : DO_SWAPU64(((Elf64_Dyn *)dyns)[i].x))
#define DYNSS(i,x) (is_e32() ? DO_SWAPS32(((Elf32_Dyn *)dyns)[i].x) \
  : DO_SWAPS64(((Elf64_Dyn *)dyns)[i].x))

int killrpath(const char *filename);
int chrpath(const char *filename, const char *newpath, int convert);

int elf_open(const char *filename, int flags, Elf_Ehdr *ehdr);
void elf_close(int fd);
int elf_find_dynamic_section(int fd, Elf_Ehdr *ehdr, Elf_Phdr *phdr);
const char *elf_tagname(int tag);
int elf_dynpath_tag(int tag);

#endif /* PROTOS_H */
