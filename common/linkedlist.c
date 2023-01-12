
#include <stdlib.h>
#include "linkedlist.h"





void push(node_t* head, void* data) {
    struct node* new_node = (struct node*) malloc(sizeof(struct node));
    new_node->data = data;
    new_node->next = head;
    head = new_node;
}

void* get_data_by_value(node_t* head,void* data, int (*compare)(void*, void*)) {
    struct node* temp = head;
    while (temp != NULL) {
        if ((*compare)(temp->data, data) == 0) {
            return temp->data;  // Found
        }
        temp = temp->next;
    }
    return NULL;
}


void remove_by_value(node_t* head, void* data, int (*compare)(void*, void*)) {
    struct node* temp = head;
    struct node* prev = NULL;

    while (temp != NULL) {
        if ((*compare)(temp->data, data) == 0) {
            if (prev == NULL) {
                head = temp->next;
            } else {
                prev->next = temp->next;
            }
            free(temp->data);
            free(temp);
            return;
        }
        prev = temp;
        temp = temp->next;
    }
}


int contains(node_t* head, void* data, int (*compare)(void*, void*)) {
    struct node* temp = head;
    while (temp != NULL) {
        if ((*compare)(temp->data, data) == 0) {
            return 1;  // Found
        }
        temp = temp->next;
    }
    return 0;
}
