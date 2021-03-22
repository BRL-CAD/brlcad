
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <elf.h>
#if defined(HAVE_SYS_LINK_H)
#  include <sys/link.h> /* Find DT_RPATH on Solaris 2.6 */
#endif /*  HAVE_SYS_LINK_H */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "protos.h"

#define EHDR_PWS(x) (is_e32() ? DO_SWAPS32(ehdr->e32.x) : DO_SWAPS64(ehdr->e64.x))
#define EHDR_PHS(x) (is_e32() ? DO_SWAPS16(ehdr->e32.x) : DO_SWAPS16(ehdr->e64.x))
#define PHDR_PWS(x) (is_e32() ? DO_SWAPS32(phdr->e32.x) : DO_SWAPS64(phdr->e64.x))
#define EHDR_PWU(x) (is_e32() ? DO_SWAPU32(ehdr->e32.x) : DO_SWAPU64(ehdr->e64.x))
#define EHDR_PHU(x) (is_e32() ? DO_SWAPU16(ehdr->e32.x) : DO_SWAPU16(ehdr->e64.x))
#define PHDR_PWU(x) (is_e32() ? DO_SWAPU32(phdr->e32.x) : DO_SWAPU32(phdr->e64.x))
#define PHDR_POU(x) (is_e32() ? DO_SWAPU32(phdr->e32.x) : DO_SWAPU64(phdr->e64.x))

static int is_e32_flag;
static int swap_bytes_flag;

int
is_e32(void)
{
  return is_e32_flag;
}

int
swap_bytes(void)
{
  return swap_bytes_flag;
}

int
elf_open(const char *filename, int flags, Elf_Ehdr *ehdr)
{
   int fd;
   size_t sz_ehdr;
   size_t sz_phdr;

   fd = open(filename, flags);
   if (fd == -1)
   {
     perror ("open");
     return -1;
   }

   if (read(fd, ehdr, EI_NIDENT) != EI_NIDENT)
   {
     perror ("reading header (e_ident)");
     close(fd);
     return -1;
   }

   if (0 != memcmp(ehdr->e_ident, ELFMAG, SELFMAG) ||
       (ehdr->e_ident[EI_CLASS] != ELFCLASS32 &&
        ehdr->e_ident[EI_CLASS] != ELFCLASS64) ||
       (ehdr->e_ident[EI_DATA] != ELFDATA2LSB &&
        ehdr->e_ident[EI_DATA] != ELFDATA2MSB) ||
       ehdr->e_ident[EI_VERSION] != EV_CURRENT)
   {
     fprintf(stderr, "`%s' probably isn't an ELF file.\n", filename);
     close(fd);
     errno = ENOEXEC; /* Hm, is this the best errno code to use? */
     return -1;
   }

   is_e32_flag = ehdr->e_ident[EI_CLASS] == ELFCLASS32;
   swap_bytes_flag = ehdr->e_ident[EI_DATA] != ELFDATA2;

   sz_ehdr = is_e32() ? sizeof(Elf32_Ehdr) : sizeof(Elf64_Ehdr);
   if (read(fd, ((char *)ehdr) + EI_NIDENT, sz_ehdr - EI_NIDENT)
       != (ssize_t)(sz_ehdr - EI_NIDENT))
   {
     perror ("reading header");
     close(fd);
     return -1;
   }

   sz_phdr = is_e32() ? sizeof(Elf32_Phdr) : sizeof(Elf64_Phdr);
   if ((size_t)EHDR_PHS(e_phentsize) != sz_phdr)
   {
     fprintf(stderr, "section size was read as %zd, not %zd!\n",
            (size_t)EHDR_PHS(e_phentsize), sz_phdr);
     close(fd);
     return -1;
   }
   return fd;
}

int
elf_find_dynamic_section(int fd, Elf_Ehdr *ehdr, Elf_Phdr *phdr)
{
  int i;
  if (lseek(fd, EHDR_PWU(e_phoff), SEEK_SET) == -1)
  {
    perror ("positioning for sections");
    return 1;
  }

  for (i = 0; i < EHDR_PHS(e_phnum); i++)
  {
    const size_t sz_phdr = is_e32() ? sizeof(Elf32_Phdr) : sizeof(Elf64_Phdr);
    if (read(fd, phdr, sz_phdr) != (ssize_t)sz_phdr)
    {
      perror ("reading section header");
      return 1;
    }
    if (PHDR_PWU(p_type) == PT_DYNAMIC)
      break;
  }
  if (i == EHDR_PHS(e_phnum))
    {
      fprintf (stderr, "No dynamic section found.\n");
      return 2;
    }

  if (0 == PHDR_POU(p_filesz))
    {
      fprintf (stderr, "Length of dynamic section is zero.\n");
      return 3;
    }

  return 0;
}

void
elf_close(int fd)
{
  close(fd);
}

const char *
elf_tagname(int tag)
{
  switch (tag) {
  case DT_RPATH:
    return "RPATH";
    break;
#if defined(DT_RUNPATH)
  case DT_RUNPATH:
    return "RUNPATH";
    break;
#endif /* DT_RUNPATH */
  }
  return "UNKNOWN";
}

int
elf_dynpath_tag(int tag)
{
  return ( tag == DT_RPATH
#if defined(DT_RUNPATH)
           || tag == DT_RUNPATH
#endif /* DT_RUNPATH */
           );
}
