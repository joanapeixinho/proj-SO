#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct node {
    void* data;
    struct node* next;
} node_t;


void push(node_t* head,void* data);
void remove_by_value(node_t* head,void* data, int (*compare)(void*, void*));
struct node* get_data_by_value(node_t* head,void* data, int (*compare)(void*, void*));

#endif