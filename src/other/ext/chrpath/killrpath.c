/*
Taken from another list:

_Changing_ is a little tricky, but the attached program strips rpaths
from executables (I find it essential for debugging the binutils).
It's endian-dependent, if you want this for x86 you can just change
the occurrences of 'MSB' to 'LSB' and compile (I should really fix
that).

--
Geoffrey Keating <geoffk@ozemail.com.au>
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#if defined(HAVE_LINK_H)
#  include <link.h>
#endif /* HAVE_LINK_H */
#include <stdlib.h>
#include "protos.h"
#include <string.h>

/* Reads an ELF file, nukes all the RPATH entries. */

int
killrpath(const char *filename)
{
   int fd;
   Elf_Ehdr ehdr;
   int i;
   Elf_Phdr phdr;
   void *dyns;
   int dynpos;

   fd = elf_open(filename, O_RDWR, &ehdr);

   if (fd == -1)
   {
     perror ("elf_open");
     return 1;
   }

   if (0 != elf_find_dynamic_section(fd, &ehdr, &phdr))
   {
     perror("found no dynamic section");
     elf_close(fd);
     return 1;
   }

   dyns = malloc(PHDR(p_memsz));
   if (dyns == NULL)
     {
       perror ("allocating memory for dynamic section");
       elf_close(fd);
       return 1;
     }
   memset(dyns, 0, PHDR(p_memsz));
   if (lseek(fd, PHDR(p_offset), SEEK_SET) == -1
       || read(fd, dyns, PHDR(p_filesz)) != (ssize_t)PHDR(p_filesz))
     {
       perror ("reading dynamic section");
       free(dyns);
       elf_close(fd);
       return 1;
     }

   dynpos = 0;
   for (i = 0; DYNSS(i, d_tag) != DT_NULL; i++)
     {
       if (is_e32())
        ((Elf32_Dyn *)dyns)[dynpos] = ((Elf32_Dyn *)dyns)[i];
       else
        ((Elf64_Dyn *)dyns)[dynpos] = ((Elf64_Dyn *)dyns)[i];
       if ( ! elf_dynpath_tag(DYNSS(i, d_tag)) )
        dynpos++;
     }
   for (; dynpos < i; dynpos++)
     {
       if (is_e32()) {
        ((Elf32_Dyn *)dyns)[dynpos].d_tag = DT_NULL;
        ((Elf32_Dyn *)dyns)[dynpos].d_un.d_val = 0x0;
       } else {
        ((Elf64_Dyn *)dyns)[dynpos].d_tag = DT_NULL;
        ((Elf64_Dyn *)dyns)[dynpos].d_un.d_val = 0x0;
       }
     }

   if (lseek(fd, PHDR(p_offset), SEEK_SET) == -1
       || write(fd, dyns, PHDR(p_filesz)) != (int)PHDR(p_filesz))
     {
       perror ("writing dynamic section");
       free(dyns);
       elf_close(fd);
       return 1;
     }

   free(dyns);
   elf_close(fd);

   return 0;
}
