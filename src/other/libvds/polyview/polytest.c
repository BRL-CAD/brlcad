#include <stdio.h>
#include "poly.h"
#include "geom.h"

int main (int argc, char *argv[0]) {
  object o;
  int i, j;

  if (argc != 2) {printf("Usage: polytest filename\n"); exit(1); }
  read_poly(argv[1],&o);

  printf("Vertices = %d, Faces = %d\n",o.nv,o.nf);

  for(i=0;i<o.nv;i++) {
    printf("Vertex = (%f,%f,%f)",o.v[i].coord[0],o.v[i].coord[1],o.v[i].coord[2]);
    if (o.v[i].hasNormal) {
      printf(", Normal = (%f,%f,%f)\n",o.v[i].normal[0],o.v[i].normal[1],
	     o.v[i].normal[2]);
    } else {
      printf("\n");
    }
  }

  for(i=0;i<o.nt;i++) {
    printf("Face = ");
    for(j=0;j<3;j++) {
      printf("%d ",o.t[i].verts[j]);
    }
    printf(" ( ");
    for(j=0;j<3;j++) {
      printf("%f ",o.t[i].normal[j]);
    }
    printf(")\n");
  }
}

