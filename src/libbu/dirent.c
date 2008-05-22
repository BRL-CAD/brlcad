/*                           D I R E N T . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2008 United States Government as represented by
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
/** @addtogroup bu_log */
/** @{ */
/** @file dirent.c
 *
 * Functionality for accessing all files in a directory.
 *
 */

/*#include "common.h"*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
/*#ifdef HAVE_DIRENT_H*/
#include <dirent.h>
/*#endif*/

/* Count number of files in directory whose type matches substr */
int bu_countpath(char *path, char *substr)
{
    int filecount = 0;
    DIR *dir = opendir(path);
    struct dirent *dp;
    while((dp = readdir(dir)) != NULL) {
	if (strcmp(substr,"") == 0) {
	   filecount++;
	} else {
	   if (strcmp(dp->d_name+(strlen(dp->d_name)-strlen(substr)),substr) == 0){
              filecount++;       
	   }
        }
    }
    closedir(dir);
    return filecount;
}

/* Return array with filenames with suffix matching substr*/
void bu_listpath(char *path, char *substr, char **filearray)
{
   int filecount = -1;
   DIR *dir = opendir(path); 
   struct dirent *dp;
   while((dp = readdir(dir)) != NULL) {
        if (strcmp(substr,"") == 0) {
           filecount++;
	   filearray[filecount]=dp->d_name;
        } else {
           if (strcmp(dp->d_name+(strlen(dp->d_name)-strlen(substr)),substr) == 0){
              filecount++;
              filearray[filecount]=dp->d_name;
           }
        }
   }
}

/* Testing code - will go away */ 
main(int argc, char **argv)
{
int files,i;
char testpath[56]="/Users/cyapp/brlcad-install/share/brlcad/7.12.3/nirt/";
char suffix[5]=".nrt";
files = bu_countpath(testpath,suffix);
char **filearray;
filearray  = (char **)malloc(files*sizeof(char *));

bu_listpath(testpath,suffix,filearray);
for (i = 0; i < files; i++){
	printf("Found file %s\n", filearray[i]);
}
free(filearray);
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
