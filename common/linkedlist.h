#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct node {
    void* data;
    struct node* next;
} node_t;

// Insert a new node at the front of the list
void push(node_t* head,void* data);
//remove the first node with the given data
void remove_by_value(node_t* head,void* data, int (*compare)(void*, void*));
//get the first node with the given data
void* get_data_by_value(node_t* head,void* data, int (*compare)(void*, void*));
//check if the list contains a node with the given data
int contains(node_t* head, void* data, int (*compare)(void*, void*)); 
#endif