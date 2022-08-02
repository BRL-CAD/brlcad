/*                         V R M L - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file vrml/vrml-g.c
 *
 * Convert Virtual Reality Markup Language (Vrml) format files to BRL-CAD .g binary format
 *
 *
 */

#include "common.h"

#include "file_util.h"
#include "string_util.h"
#include "node_type.h"
#include "node.h"
#include "parser.h"
#include "transform_node.h"

#include "bu/getopt.h"
#include "gcv/api.h"
#include "vmath.h"
#include "wdb.h"

#define LEN 20


char *nextWord(char *inputstring, char *nextwd);
bool findKeyWord(char *inputstring, int kw);
char *getNextWord(char *instring, char *nextword);
char *getNextWord( char *nextword);
void formatString(char *instring);
void get4vec(float *p);
void get3vec(float *p);

using namespace std;

static struct bg_vert_tree *tree;
static struct wmember all_head;
static int *bot_faces=NULL;	 /* array of ints (indices into tree->the_array array) three per face */
static int bot_fcurr=0;		/* current bot face */
static double *allvert = NULL;
static int vertsize;
static int objnumb = -1;
static struct rt_wdb *fd_out = NULL;
PARSER parse;
static int id_no=1000;		/* Ident numbers */
static int mat_code=1;		/* default material code */
unsigned char color[3]={ 128, 128, 128 };

#define BOT_FBLOCK 128

int
Check_degenerate(int face[3])
{
    if (face[0] == face[1]) {
	return 0;
    }
    if (face[0] == face[2]) {
	return 0;
    }
    if (face[1] == face[2]) {
	return 0;
    }
    return 1;
}

static void
Convert_input(NODE *node)
{
    int face_count=0;
    struct wmember head;
    int vert_no=0;
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;
    char sname[5];
    char rname[5];

    bot_fcurr = 0;

    objnumb++;
    sprintf(sname, "%d", objnumb);
    sprintf(rname, "r%d", objnumb);

    bu_vls_strcpy(&solid_name, "s");
    bu_vls_strcat(&solid_name, sname);
    bu_vls_strcpy(&region_name, rname);

    if (node->ispoly) {
	int tmp_face[3] = {0, 0, 0};
	for (vert_no = 0; vert_no < vertsize; vert_no++) {
	    double x, y, z;
	    x = allvert[vert_no*3];
	    y = allvert[vert_no*3+1];
	    z = allvert[vert_no*3+2];

	    tmp_face[vert_no%3] = bg_vert_tree_add( tree,x, y, z, 0.0);

	    if (((vert_no+1)%3 == 0) && (vert_no != 0) ) {
		if (Check_degenerate(tmp_face)) {
		    VMOVE(&bot_faces[3*bot_fcurr], tmp_face);
		    bot_fcurr++;
		    face_count++;
		}
	    }
	}
	if (face_count == 0) {
	    objnumb--;
	    return;
	}
	mk_bot(fd_out, bu_vls_addr(&solid_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, tree->curr_vert, bot_fcurr,
	tree->the_array, bot_faces, NULL, NULL);
	bg_vert_tree_clean(tree);
    }else if (node->nnodetype == NODE_CONE) {
	mk_tgc(fd_out,bu_vls_addr(&solid_name), &allvert[0], &allvert[3], &allvert[6], &allvert[9], &allvert[12], &allvert[15]);
    }else if (node->nnodetype == NODE_BOX) {
	mk_arb8(fd_out,bu_vls_addr(&solid_name), &allvert[0]);
    }else if (node->nnodetype == NODE_CYLINDER) {
	mk_tgc(fd_out,bu_vls_addr(&solid_name), &allvert[0], &allvert[3], &allvert[6], &allvert[9], &allvert[6], &allvert[9]);
    }else if (node->nnodetype == NODE_SPHERE) {
	mk_ell(fd_out,bu_vls_addr(&solid_name), &allvert[0], &allvert[3], &allvert[6], &allvert[9]);
    } else {
	objnumb--;
	return;
    }

    BU_LIST_INIT(&head.l);
    (void)mk_addmember(bu_vls_addr(&solid_name), &head.l, NULL, WMOP_UNION);

    mk_lrcomb(fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL, (char *)NULL, color, id_no, 0, mat_code, 100, 0);

    (void)mk_addmember(bu_vls_addr(&region_name), &all_head.l, NULL, WMOP_UNION);
    id_no++;


    bu_vls_free(&region_name);
    bu_vls_free(&solid_name);

    return;
}

static void
parseNodeData(NODE* node)
{
    size_t size = node->vertics.size();
    int i;

    if (size) {
	vertsize = size/3;
	bot_faces = (int *)bu_realloc((void *)bot_faces, vertsize * (bot_fcurr+1) * sizeof(int), "bot_faces increase");
	for (i = 0; i < 3; i++)
	   color[i] = (int)((node->diffusecolor[i] * 255.0));

	allvert = (double*)bu_malloc(size * sizeof(double), "allvert");
	copy(node->vertics.begin(), node->vertics.end(), allvert);
	Convert_input(node);
	bu_free((float *)allvert, "allvert");
    }
}

static void
Parse_input(vector<NODE*> &childlist)
{
    size_t size = childlist.size();
    size_t i;

    for (i = 0; i < size; i++) {
	if (childlist[i]->nnodetype == NODE_CONE) {
	    parseNodeData(childlist[i]);
	}else if (childlist[i]->nnodetype == NODE_BOX) {
	    parseNodeData(childlist[i]);
	}else if (childlist[i]->nnodetype == NODE_CYLINDER) {
	    parseNodeData(childlist[i]);
	}else if (childlist[i]->nnodetype == NODE_SPHERE) {
	    parseNodeData(childlist[i]);
	}else if (childlist[i]->nnodetype == NODE_INDEXEDFACESET) {
	    parseNodeData(childlist[i]);
	}else if (childlist[i]->nnodetype == NODE_INDEXEDLINESET) {
	    parseNodeData(childlist[i]);
	}
    }
    return;
}


HIDDEN int
vrml_read(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *UNUSED(options_data), const char *source_path)
{
    vector<NODE *> childlist;
    vector<NODE *> parent;
    NODE snode;
    TRANSFORM tnode;
    int type;
    char *fptr;

    FileUtil infile(source_path);

    type = infile.getFileType();

    if (type == 1) {
	bu_log("Does not have support for vrml version 1\n");
	return 0;
    }else if (type != 2) {
	bu_log("Can not open or identify the file type\n");
	return 0;
    }

    //fptr get file data
    fptr = infile.storeFileInput();
    //Format string to ease parsing
    formatString(fptr);

	//Parse file date
    parse.parseScene(fptr);
    //Get node stored in scenegraph
    parse.getChildNodeList(&parse.rootnode, childlist);
    //Split nodes
    snode.doMakePoly(childlist);
    //Do transformation on node vertices
    tnode.transformSceneVert(childlist);
    //Parse shape color value to child
    parse.doColor(childlist);

    infile.freeFileInput();

    FILE *fd_in;
    fd_out = context->dbip->dbi_wdbp;

    if ((fd_in=fopen(source_path, "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", source_path);
	perror("libgcv");
	return 0;
    }

    BU_LIST_INIT(&all_head.l);
    /* create a tree structure to hold the input vertices */
    tree = bg_vert_tree_create();

    Parse_input(childlist);
    fclose(fd_in);

    if (objnumb < 0) {
	bu_log("ERROR: unable to get objects\n");
	return 0;
    }

    /* make a top level group */
    mk_lcomb(fd_out, "all", &all_head, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    if (gcv_options->verbosity_level)
	bu_log("%d objects created\n", objnumb + 1);

    return 1;
}



HIDDEN int
vrml_can_read(const char *source_path)
{
    if (!source_path) return 0;
    FileUtil infile(source_path);
    if (infile.getFileType() == 2) return 1;
    return 0;
}

extern "C" {
    struct gcv_filter gcv_conv_vrml_read =
    {"VRML Reader", GCV_FILTER_READ, BU_MIME_MODEL_VRML, vrml_can_read, NULL, NULL, vrml_read};
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
