/*                    T E S T _ D E N S I T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <sys/stat.h>
#include "analyze.h"

int
main(int argc, char **argv)
{
  struct density_entry *densities = NULL;
  static int num_densities = 1028;

  struct stat sb;
  FILE *fp = (FILE *)NULL;
  char *buf = NULL;
  int ret = 0;
  int i;

  if (argc < 2) {
      bu_log("Error - please supply density file\n");
      bu_exit(EXIT_FAILURE, NULL);
  }

  fp = fopen(argv[1], "rb");
  if (fp == (FILE *)NULL) {
      bu_log("Error - file %s not opened\n", argv[1]);
      bu_exit(EXIT_FAILURE, NULL);
  }

  if (stat(argv[1], &sb)) {
      bu_log("Error - file %s not stat successfully\n", argv[1]);
      bu_exit(EXIT_FAILURE, NULL);
  }

  buf = (char *)bu_malloc(sb.st_size+1, "density buffer");
  ret = fread(buf, sb.st_size, 1, fp);
  if (ret != 1) {
    bu_log("Error reading file %s\n", argv[1]);
    bu_exit(EXIT_FAILURE, NULL);
  }

  densities = (struct density_entry *)bu_calloc(num_densities, sizeof(struct density_entry), "density entries");

  ret = parse_densities_buffer(buf, (unsigned long)sb.st_size, densities, NULL, &num_densities);

  for (i = 0; i < num_densities; i++) {
      if (densities[i].name)
	  bu_log("densities[%i]: %s, %f\n", i, densities[i].name, densities[i].grams_per_cu_mm);
  }

  return 0;
}
