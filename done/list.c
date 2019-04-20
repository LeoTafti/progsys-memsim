/**
 * @file list.c
 * @brief Doubly linked list
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include <stdio.h> //for prints
#include <stdlib.h> //for malloc
#include <inttypes.h> //for print_node

#include "list.h"
#include "error.h"

int is_empty_list(const list_t* this) {
  M_REQUIRE_NON_NULL(this);
  return this->back == NULL && this->front == NULL;
}

void init_list(list_t* this) {
  if(this!=NULL) {
    this->back = NULL;
    this->front = NULL;
  }
}

/* make a singleton list from an empty list given a node */
void static singleton(list_t* this, node_t* n) {
  // if(is_empty_list(this))
  this->front = n;
  this->back = n;
  n->previous = NULL;
  n->next = NULL;
}

void clear_list(list_t* this) {
  if(this != NULL) {
    if(!is_empty_list(this)) {
    //remove all but last
    for_all_nodes(n, this) {
      if(n->previous!=NULL) free(n->previous);
    }
    free(this->back);
    }
  }
  // the list itself must be freed at higher level
}

node_t* push_back(list_t* this, const list_content_t* value) {
  if(this != NULL && value != NULL) {

  node_t* n = malloc(sizeof(node_t));
  if(n != NULL) {
    n->value = *value;
    if(is_empty_list(this)) singleton(this, n);
    else {
      n->previous = this->back;
      n->next = NULL;
      this->back->next = n;
      this->back = n;
    }
  }
  return n;
  }
  return 0; // TODO warning if we return an error code but this is not satisfactory
}

node_t* push_front(list_t* this, const list_content_t* value) {
  if(this != NULL && value != NULL) {

  node_t* n = malloc(sizeof(node_t));
  if(n != NULL) {
    n->value = *value;
    if(is_empty_list(this)) singleton(this, n);
    else {
      n->previous = NULL;
      n->next = this->front;
      this->front->previous = n;
      this->front = n;
    }
  }
  return n;
  }
  return 0;
}

void pop_back(list_t* this) {
  if(this != NULL) {
  if(!is_empty_list(this)) {
    node_t* rm = this->back;
    node_t* llast = this->back->previous;

    this->back = llast;
    if( llast != NULL) llast->next = NULL; // if not a singleton

    free(rm);
  }
  }
}

void pop_front(list_t* this) {
  if(this != NULL) {
  if(!is_empty_list(this)) {
    node_t* rm = this->front;
    node_t* ffirst = this->front->next;

    this->front = ffirst;
    if(ffirst != NULL) ffirst->previous = NULL; // if not a singleton

    free(rm);
  }
  }
}

void move_back(list_t* this, node_t* node) {
  // TODO check contains??
  if(this != NULL && node != NULL) {
    if(node->next != NULL) { // do nothing if it is already back
    node->previous != NULL ? (node->previous->next = node->next) :
                             (this->front = node->next);
    node->next->previous = node->previous;

    node = push_back(this, &(node->value));
    }
  }
}

int print_list(FILE* stream, const list_t* this) {
  M_REQUIRE_NON_NULL(this);
  M_REQUIRE_NON_NULL(stream);

  int count = 0;

  fputc('(', stream);
  for_all_nodes(n, this){
    count += print_node(stream, n->value);
    if(n->next != NULL) count += fprintf(stream, ", ");
  }
  fputc(')', stream);
  //will miss a '\n'
  return count+2; // 2 for parenthesis
}

int print_reverse_list(FILE* stream, const list_t* this){
  M_REQUIRE_NON_NULL(this);
  M_REQUIRE_NON_NULL(stream);

  int count = 0;

  fputc('(', stream);
  for_all_nodes_reverse(n, this){
    count += print_node(stream, n->value);
    if(n->previous != NULL) count += fprintf(stream, ", ");
  }
  fputc(')', stream);
  //this will miss a '\n'
  return count+2; // 2 for parenthesis
}
