/*
 * Anindya Guha
 */

#ifndef __THREADSALIVE_H__
#define __THREADSALIVE_H__

#include <ucontext.h>
#include "list.h"

/* ***************************
        type definitions
   *************************** */

typedef struct {
    int value;
    Node *queue;
} tasem_t;

typedef struct {
    tasem_t sem;
} talock_t;

typedef struct {
    Node *queue;
} tacond_t;


typedef struct mutex_node {
    talock_t *mutex;
    struct mutex_node *next;
} mutex_list_node;


/* ***************************
       stage 1 functions
   *************************** */

void ta_libinit(void);
void ta_create(void (*)(void *), void *);
void ta_yield(void);
int ta_waitall(void);

/* ***************************
       stage 2 functions
   *************************** */

void ta_sem_init(tasem_t *, int);
void ta_sem_destroy(tasem_t *);
void ta_sem_post(tasem_t *);
void ta_sem_wait(tasem_t *);
void ta_lock_init(talock_t *);
void ta_lock_destroy(talock_t *);
void ta_lock(talock_t *);
void ta_unlock(talock_t *);

/* ***************************
       stage 3 functions
   *************************** */

void ta_cond_init(tacond_t *);
void ta_cond_destroy(tacond_t *);
void ta_wait(talock_t *, tacond_t *);
void ta_signal(tacond_t *);

/* **************************
    Mutex Linked List functions 

  ************************** */

void mutex_list_clear(mutex_list_node *list); 
void mutex_list_append(talock_t *mutex, mutex_list_node **head);
void mutex_list_remove(mutex_list_node **head);

#endif /* __THREADSALIVE_H__ */
