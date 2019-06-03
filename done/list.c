/**
 * @file list.c
 * @brief Doubly linked list
 *
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
  if( this != NULL) {
    this->back = NULL;
    this->front = NULL;
  }
}

static node_t* init_node(const list_content_t* value) {
  if(value == NULL) return NULL;

  node_t* n = malloc(sizeof(node_t));
  if(n != NULL) {
    n->value = *value;
    n->previous = NULL;
    n->next = NULL;
  }
  return n;
}

static void insert_between( node_t* this, node_t* prev, node_t* next){
  if(this != NULL) {

    // Link to prev
    if(prev != NULL) {
      this->previous = prev;
      prev->next = this;
    }
    // Link to next
    if(next != NULL) {
        this->next = next;
        next->previous = this;
    }
  }
}

static void cut(node_t* n) {
  if(n != NULL) {
    if( n->previous != NULL ) n->previous->next = n->next;
    if( n->next != NULL ) n->next->previous = n->previous;
    n->previous = NULL;
    n->next = NULL;
  }
}

static void remove_node(node_t* n) {
  cut(n);
  free(n);
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
  if(this == NULL || value == NULL) return NULL;

  node_t* n = init_node(value);

  if(n != NULL) {
    if(is_empty_list(this)) singleton(this, n);
    else {
      insert_between(n, this->back, NULL);
      this->back = n;
    }
  }

  return n;
}

node_t* push_front(list_t* this, const list_content_t* value) {
  if(this == NULL || value == NULL) return NULL;

  node_t* n = init_node(value);

  if(n != NULL) {
    if(is_empty_list(this)) singleton(this, n);
    else {
      insert_between(n, NULL, this->front);
      this->front = n;
    }
  }

  return n;
}

void pop_back(list_t* this) {
  if(this != NULL && this->back != NULL) {
    node_t* rm = this->back;

    this->back = rm->previous;
    cut(rm);
  }
}

void pop_front(list_t* this) {
  if(this != NULL && this->front != NULL) {
    node_t* rm = this->front;

    this->front = rm->next;
    remove_node(rm);
  }
}

void move_back(list_t* this, node_t* node) {
  if(this != NULL && node != NULL) {
    if(this->back != node) { // nothing to be done if it is already at the back
      if(this->front == node) this->front = node->next;

      cut(node);
      insert_between(node, this->back, NULL);
      this->back = node;
    }
  }
}

int print_list(FILE* stream, const list_t* this) {
  M_REQUIRE_NON_NULL(this);
  M_REQUIRE_NON_NULL(stream);

  int count = 0;

  fputc('(', stream);
  count += 1;
  for_all_nodes(n, this){
    count += print_node(stream, n->value);
    if(n->next != NULL){
      count += fprintf(stream, ", ");
    }
  }
  fputc(')', stream);
  count += 1;

  return count;
}

int print_reverse_list(FILE* stream, const list_t* this){
  M_REQUIRE_NON_NULL(this);
  M_REQUIRE_NON_NULL(stream);

  int count = 0;

  fputc('(', stream);
  count += 1;
  for_all_nodes_reverse(n, this){
    count += print_node(stream, n->value);
    if(n->previous != NULL){
      count += fprintf(stream, ", ");
    }
  }
  fputc(')', stream);
  count += 1;

  return count;
}
