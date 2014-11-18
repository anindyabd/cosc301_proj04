/*
 * Anindya Guha
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <string.h>

#include "list.h"
#include "threadsalive.h"


/* ***************************** 
     stage 1 library functions
   ***************************** */

#define STACKSIZE 128000
static Node *ready_queue;
static Node *all; // this exists so that I can free all the stacks
static ucontext_t parent_ctx;
static int num_blocked_threads;
static mutex_list_node *cond_mutex_list; // list of released mutexes that must be reacquired


void ta_libinit(void) {
    ready_queue = NULL;
    all = NULL;    
    num_blocked_threads = 0;
    return;
}

void ta_create(void (*func)(void *), void *arg) {
    
    unsigned char *stack = (unsigned char *)malloc(STACKSIZE);
    ucontext_t actx;
    list_append(actx, 0, &ready_queue);
    list_append(actx, 0, &all);
    ucontext_t *ctx = get_last(ready_queue);
    ucontext_t *copy = get_last(all);
    getcontext(&(*ctx));
    getcontext(&(*copy));
    copy->uc_stack.ss_sp = stack;
    ctx->uc_stack.ss_sp   = stack;
    ctx->uc_stack.ss_size = STACKSIZE;
    ctx->uc_link = &parent_ctx;
    makecontext(&(*ctx), (void (*)(void))func, 1, arg);
    
}

void ta_yield(void) {
    if (ready_queue == NULL) {
        return;
    }
    ucontext_t actx;
    list_append(actx, 0, &ready_queue);
    ucontext_t* ctx = get_last(ready_queue);
    ucontext_t toswitch = ready_queue->ctx;
    if (ready_queue->must_reacquire) {  
        ta_lock(cond_mutex_list->mutex); 
        mutex_list_remove(&cond_mutex_list); 
    }
    list_remove(&ready_queue);
    swapcontext(&(*ctx), &toswitch);
}

int ta_waitall(void) {
    if (ready_queue == NULL) {
        return 0;
    }
    while (ready_queue != NULL) {
        ucontext_t ctx = ready_queue->ctx;
        /* if ready_queue->must_reacquire is 1, that means that this 
        particular thread was woken from a condition variable, and must
        reacquire the lock. Clearly, I am not creative with naming. */
        if (ready_queue->must_reacquire) { 
            /* this "parent thread" reacquires the lock, and not the thread itself,
             but that doesn't matter. The lock is held when the  
             thread that needs the lock runs, so this is correct, in the sense
             that the thread has the lock. For all practical purposes this should work correctly.
             This is done for simplicity's sake. */
            ta_lock(cond_mutex_list->mutex); 
            mutex_list_remove(&cond_mutex_list);
        }
        list_remove(&ready_queue);
        ctx.uc_link = &parent_ctx;
        swapcontext(&parent_ctx, &ctx);
    }   
    list_clear(all);
    return -num_blocked_threads;
}


/* ***************************** 
     stage 2 library functions
   ***************************** */

void ta_sem_init(tasem_t *sem, int value) {
    assert(value > -1);
    sem->value = value;
    sem->queue = NULL;
}       

void ta_sem_destroy(tasem_t *sem) {
    list_clear(sem->queue);
    sem->value = -1; // this value should never occur in a valid semaphore
}

void ta_sem_post(tasem_t *sem) {
    assert(sem->value > -1);
    if (sem->queue != NULL) {
        ucontext_t ctx = sem->queue->ctx;
        list_append(ctx, 0, &ready_queue);
        list_remove(&sem->queue); 
        num_blocked_threads--;
    }
    sem->value++;
}

void ta_sem_wait(tasem_t *sem) {
    assert(sem->value > -1);
    if (sem->value == 0) {
        ucontext_t actx;
        list_append(actx, 0, &sem->queue);
        ucontext_t* ctx = get_last(sem->queue);
        ucontext_t toswitch = ready_queue->ctx;
        list_remove(&ready_queue);
        num_blocked_threads++;
        swapcontext(&(*ctx), &toswitch);
    }
    sem->value--;
}

void ta_lock_init(talock_t *mutex) {
    tasem_t sem;
    mutex->sem = sem;
    ta_sem_init(&mutex->sem, 1);
}

void ta_lock_destroy(talock_t *mutex) {
    ta_sem_destroy(&mutex->sem);
}

void ta_lock(talock_t *mutex) {
    ta_sem_wait(&mutex->sem);
}

void ta_unlock(talock_t *mutex) {
    ta_sem_post(&mutex->sem);
}


/* ***************************** 
     stage 3 library functions
   ***************************** */

void ta_cond_init(tacond_t *cond) {
    cond->queue = NULL;
    cond_mutex_list = NULL;
}

void ta_cond_destroy(tacond_t *cond) {
    list_clear(cond->queue);
}

void ta_wait(talock_t *mutex, tacond_t *cond) {
    ucontext_t actx;
    list_append(actx, 0, &(cond->queue));
    ucontext_t* ctx = get_last(cond->queue);
    ta_unlock(mutex);
    mutex_list_append(mutex, &cond_mutex_list); // must remember this mutex
    ucontext_t toswitch = ready_queue->ctx;
    list_remove(&ready_queue);
    num_blocked_threads++;
    swapcontext(&(*ctx), &toswitch);  
}

void ta_signal(tacond_t *cond) {
    if (cond->queue == NULL) {
        return;
    }
    ucontext_t ctx = cond->queue->ctx;
    list_append(ctx, 1, &ready_queue); // must reacquire lock after waking up!
    list_remove(&cond->queue); 
    num_blocked_threads--;   
}

/* *******************************
    Queue that holds mutex locks that need 
    to be reacquired by threads after 
    being woken up from condition variable. 
    ******************************* */

void mutex_list_clear(mutex_list_node *list) {
    while (list != NULL) {
        mutex_list_node *tmp = list;
        list = list->next;
        free(tmp);
    }
}

void mutex_list_append(talock_t *mutex, mutex_list_node **head) {
    mutex_list_node *newnode = malloc(sizeof(Node));
    newnode->mutex = mutex;
    newnode->next = NULL;
    mutex_list_node *curr = *head;
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

void mutex_list_remove(mutex_list_node **head) {
    if (*head != NULL) {
        mutex_list_node *tmp = *head;
        *head = (*head)->next;
        free(tmp);
    }
}
