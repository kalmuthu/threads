#ifndef THREADLIST_H
#define THREADLIST_H

#include <pthread.h>

/**
 * @brief The node holding the thread info
 */
struct thread_node{
    /**
     * @brief The previous node
     */
    struct thread_node * prev;
    /**
     * @brief The next node
     */
    struct thread_node * next;
    /**
     * @brief The thread
     */
    pthread_t * thread;
};

/**
 * @brief The list of threads
 */
struct thread_list{
    /**
     * @brief The head of the list
     */
    struct thread_node * head;
    /**
     * @brief The tail of the list
     */
    struct thread_node * tail;
};

void insert_thread_list_head(struct thread_node *, struct thread_list *);
void insert_thread_list_tail(struct thread_node *, struct thread_list *);
void remove_from_thread_list(struct thread_node *, struct thread_list *);

#endif // THREADLIST_H

