#include <stddef.h>
#include <pthread.h>
#include <stdlib.h>
#include <server.h> //server functions
#include <util.h> //client functions
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "threadlist.h"





/**
 * @brief thread_process Function to execute on the threads; services the client's request
 * @param fd The file descriptor of the opened request
 * @return 0
 */
void * thread_process(void * fd){
    assert(fd);
    int * fd_int = (int *)fd;
    client_process(*fd_int);
    free(fd_int);
	return 0;
}

/**
 * @brief process_threads_per_request Processes the request using a thread per request
 * @param max_concurrency The maximum number of concurrent threads
 * @param accept_fd The accepted file descriptor
 */
void process_threads_per_request(int max_concurrency, int accept_fd){
	int num_threads_running = 0;

    //the list
    struct thread_list * thread_list = (struct thread_list *)malloc(sizeof(struct thread_list));
    thread_list->head = NULL;
    thread_list->tail = NULL;

	//loop continuously
    //while(1){
    int i = 0;
    while(i < 1000){
        //check to see if we can add any new threads
        if(num_threads_running >= max_concurrency){
            struct thread_node * head = thread_list->head;
            pthread_t * thread = head->thread;
            assert(thread);
			//wait until the last thread in the queue has finished
            pthread_join(*thread, NULL);
			//pop it from the list
            remove_from_thread_list(head, thread_list);
            free(head);
            free(thread);
			num_threads_running--;
		}
		else{
			//accept the new connection
            int * fd_ptr = (int *)malloc(sizeof(int));
            assert(fd_ptr);
            *fd_ptr = server_accept(accept_fd);
            assert(*fd_ptr > 0);
			//spawn new thread to handle response
            pthread_t * thread = (pthread_t *)malloc(sizeof(pthread_t));
            assert(thread);
            pthread_create(thread, NULL, &thread_process, (void *)fd_ptr);
            //push list
            struct thread_node * node = (struct thread_node *)malloc(sizeof(struct thread_node));
            node->thread = thread;
            insert_thread_list_tail(node, thread_list);
			num_threads_running++;
            ++i;
		}
	}

    //empty list
    while(thread_list->head){
        struct thread_node * node = thread_list->head;
        remove_from_thread_list(thread_list->head, thread_list);
        pthread_t * thread = node->thread;
        assert(thread);
        pthread_join(*thread, NULL);
        free(thread);
        free(node);
    }

    free(thread_list);
}

