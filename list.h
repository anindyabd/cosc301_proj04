#ifndef __LIST_H__
#define __LIST_H__

#include <ucontext.h>


typedef struct node {
    ucontext_t ctx;
    struct node *next;
} Node;

void list_append(ucontext_t ctx, Node **head);
void list_clear(Node *list);
void list_remove(struct node **head);
ucontext_t* get_last(Node *head);


#endif /* __LIST_H__ */