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
static void singleton(list_t* this, node_t* n) {
  this->front = n;
  this->back = n;
  n->previous = NULL;
  n->next = NULL;
}

void clear_list(list_t* this) {
  if(this != NULL && !is_empty_list(this)) {
    //free all but last
    for_all_nodes(n, this) {
      if(n->previous!=NULL) free(n->previous);
    }
    //free last
    free(this->back);
  }
  init_list(this);
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
  return NULL;
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
  return NULL;
}

void pop_back(list_t* this) {
  if(this != NULL && !is_empty_list(this)) {
    node_t* rm = this->back;
    node_t* llast = rm->previous;

    this->back = llast;
    free(rm);

    if(llast != NULL){  // if not a singleton
      llast->next = NULL;
    }
  }
}

void pop_front(list_t* this) {
  if(this != NULL && !is_empty_list(this)) {
    node_t* rm = this->front;
    node_t* ffirst = rm->next;

    this->front = ffirst;
    free(rm);

    if(ffirst != NULL){ // if not a singleton
      ffirst->previous = NULL;
    }
  }
}

void move_back(list_t* this, node_t* node) {
  // would be more rigourous to check contains but
  // too costly and can't signal error with this signature
  if(this != NULL && node != NULL) {
    if(node->next != NULL) { // nothing to be done if it is already at the back

      // L <-> n <-> R ===> L <-> R <-> n
      // if L!=NIL make L -> R else make 'front' -> R
      node->previous != NULL ? (node->previous->next = node->next) : (this->front = node->next);
      // make L <- R
      node->next->previous = node->previous;
      // make R <-> n
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
    if(n->next != NULL){
      count += fprintf(stream, ", ");
    }
  }
  fputc(')', stream);
  return count+2; // account for the parenthesis
}

int print_reverse_list(FILE* stream, const list_t* this){
  M_REQUIRE_NON_NULL(this);
  M_REQUIRE_NON_NULL(stream);

  int count = 0;

  fputc('(', stream);
  for_all_nodes_reverse(n, this){
    count += print_node(stream, n->value);
    if(n->previous != NULL){
      count += fprintf(stream, ", ");
    }
  }
  fputc(')', stream);
  return count+2; // account for the parenthesis
}
