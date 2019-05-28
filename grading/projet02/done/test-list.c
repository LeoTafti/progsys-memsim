#include <stdio.h>
#include "list.h"


int main() {
  printf("Testing list\n");

  list_t l;
  init_list(&l);
  list_content_t v = 0;
  printf("empty list:");
  print_list(stdout, &l);

  push_front(&l, &v);
  v = 1;
  push_back(&l, &v);
  v = 2;
  push_back(&l, &v);

  printf("\nintialized: ");
  print_list(stdout, &l);
  printf("\nin reverse: ");
  print_reverse_list(stdout, &l);

  pop_back(&l);
  printf("\npopped back: ");
  print_list(stdout, &l);
  pop_front(&l);
  printf("\npopped front: ");
  print_list(stdout, &l);

  v = 4;
  push_front(&l, &v);
  v = 5;
  push_front(&l, &v);
  printf("\nnew list:");
  print_list(stdout, &l);
  move_back(&l, l.front);
  printf("\nmoved back head:");
  print_list(stdout, &l);
  fflush(stdout);

  clear_list(&l);

  printf("\ndone and clean\n");

}
