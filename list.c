#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include "list.h"

/* ***************************** 
     linked list library functions
   ***************************** */
void list_clear(Node *list) {
    while (list != NULL) {
        struct node *tmp = list;
        free(list->ctx.uc_stack.ss_sp);
        list = list->next;
        free(tmp);
    }
}

void list_append(ucontext_t ctx, Node **head) {
    Node *newnode = malloc(sizeof(Node));
    newnode->ctx = ctx;
    newnode->next = NULL;
    struct node *curr = *head;
    if (curr == NULL) {
        *head = newnode;
        return;
    }
    while (curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = newnode;
    newnode->next = NULL;
}

void list_remove(Node **head) {
    if (*head != NULL) {
        struct node *tmp = *head;
        *head = (*head)->next;
        free(tmp);
    }
}

ucontext_t* get_last(Node *head) {
    Node *curr = head;
    while (curr->next != NULL) {
        curr = curr->next;
    } 
    return &(curr->ctx);
}
