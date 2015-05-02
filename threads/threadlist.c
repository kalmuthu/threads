#import "threadlist.h"
#include <stddef.h>

/**
 * @brief Inserts the node to the head of the list
 * @param thread_node The node to be inserted
 * @param thread_list The list of threads
 */
void insert_thread_list_head(struct thread_node * thread_node, struct thread_list * thread_list){
    if(thread_list->head){
        //thread_node will be new head
        thread_node->next = thread_list->head;
        thread_node->prev = NULL;
        thread_list->head->prev = thread_node;
        thread_list->head = thread_node;
    }
    else{
        //thread will be head
        thread_node->prev = NULL;
        thread_node->next = NULL;
        thread_list->head = thread_node;
        thread_list->tail = thread_node;
    }
}

/**
 * @brief Inserts the given node to the end of the list
 * @param thread_node The node to be inserted
 * @param thread_list The list of threads
 */
void insert_thread_list_tail(struct thread_node * thread_node, struct thread_list * thread_list){
    if(thread_list->tail){
        //thread_node will be new tail
        thread_node->next = NULL;
        thread_node->prev = thread_list->tail;
        thread_list->tail->next = thread_node;
        thread_list->tail = thread_node;
    }
    else{
        //thread will be head and tail
        thread_node->next = NULL;
        thread_node->prev = NULL;
        thread_list->head = thread_node;
        thread_list->tail = thread_node;
    }
}

/**
 * @brief Removes the node from the list
 * @param thread_node The node to be remvoed
 * @param thread_list The list of threads
 */
void remove_from_thread_list(struct thread_node * thread_node, struct thread_list * thread_list){
    //detach
    if(thread_node->next){
        thread_node->next->prev = thread_node->prev;
    }
    if(thread_node->prev){
        thread_node->prev->next = thread_node->next;
    }
    //update head
    if(thread_node == thread_list->head){
        thread_list->head = thread_list->head->next;
    }
    //update tail
    if(thread_node == thread_list->tail){
        thread_list->tail = thread_list->tail->prev;
    }
}
