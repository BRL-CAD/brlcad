#include <stdio.h>
#include <stdlib.h>


#define LIST_MAX	8388608

struct Node8 {
  int	a, b;
  float	x[16];
};


int main(int argc, char *args[]) {
  Node8	*list;
  int	i, ind[8], total;

  list = (Node8*)malloc(sizeof(Node8)*LIST_MAX);
  for (i= 0; i < LIST_MAX; i++) {
    list[i].a = 2*i;
    list[i].b = 3*i;
  }

//printf("rand: %d\n", rand());
  for (i= 0; i < 1024*1024*8; i++) {

  ind[0]= rand()%LIST_MAX;
  ind[1]= rand()%LIST_MAX;
  ind[2]= rand()%LIST_MAX;
  ind[3]= rand()%LIST_MAX;
  ind[4]= rand()%LIST_MAX;
  ind[5]= rand()%LIST_MAX;
  ind[6]= rand()%LIST_MAX;
  ind[7]= rand()%LIST_MAX;

  ind[0]= 0;
  ind[1]= 1;
  ind[2]= 2;
  ind[3]= 3;
  ind[4]= 4;
  ind[5]= 5;
  ind[6]= 6;
  ind[7]= 7;

    total=
list[ind[0]].a + list[ind[0]].b +
list[ind[1]].a + list[ind[1]].b +
list[ind[2]].a + list[ind[2]].b +
list[ind[3]].a + list[ind[3]].b +
list[ind[4]].a + list[ind[4]].b +
list[ind[5]].a + list[ind[5]].b +
list[ind[6]].a + list[ind[6]].b +
list[ind[7]].a + list[ind[7]].b;
  }

  return(1);
}
