/* symlink.c - create symbolic links corresponding to all the schema names */
/* used in an EXPRESS file */
/* Author: Don Libes, NIST, 20-Mar-1993 */

#include <stdlib.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "express/express.h"

void
create_links(Express model)
{
	DictionaryEntry de;
	Schema s;
	char linksrc[MAXPATHLEN];
	char linkname[MAXPATHLEN];
	char dir[MAXPATHLEN];
	char dir2[MAXPATHLEN];
	extern char *getcwd();

	getcwd(dir2,MAXPATHLEN);
	/* get rid of "/auto" added by automounter */
	/* At NIST, tribble doesn't automount things */
	if (0 == strncmp("/auto/",dir2,6)) {
		strcpy(dir,dir2+5);
	} else {
		strcpy(dir,dir2);
	}

	sprintf(linksrc,"%s/%s",dir,model->u.express->filename);

	DICTdo_init(model->symbol_table,&de);
	while (0 != (s = DICTdo(&de))) {

		char lower[MAXPATHLEN];
		char *dest, *src;
		/* convert to schema name lowercase */

		dest = lower;
		for (src = s->symbol.name;*src;src++) *dest++ = tolower(*src);
		*dest = '\0';

		sprintf(linkname,"%s.exp",lower);

		if (-1 == symlink(linksrc,linkname)) {
			fprintf(stderr,"symlink(%s,%s) failed: %s\n",
				linksrc,linkname,strerror(errno));
			exit(1);
		}
	}
	exit(0);
}

void
EXPRESSinit_init() {
	EXPRESSbackend = create_links;
}

