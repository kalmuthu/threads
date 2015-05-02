#include <stdlib.h>
#include "threadlist.h"
#include "threadpool.h"
#include <server.h> //server functions
#include <util.h> //client functions
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stddef.h>

/**
 * @brief The function that each worker job performs
 * @param args The arguments for the pool
 * @see pool_args
 * @return 0 if no errors
 */
void * start_job(void * args){
    struct pool_args * pool_args = (struct pool_args *)args;
    struct job * job;
    //continuously loop
    while(1){
        //printf("Acquiring worker thread mutex\n");
        //acquire lock
        pthread_mutex_lock(pool_args->jobs->mutex);
        //printf("CHECKING WORKER QUEUE\n");
        while(!pool_args->jobs->head){
            //break loop if count reached
            if(pool_args->jobs->completed_jobs >= 1000){
                //printf("HIT MAX LIMIT; exiting...\n");
                pthread_cond_signal(pool_args->jobs->cond);
                pthread_mutex_unlock(pool_args->jobs->mutex);
                return 0;
            }
            //printf("WAITING IF THERE'S A JOB READY\n");
            //wait until there's a job ready
            pthread_cond_wait(pool_args->jobs->cond, pool_args->jobs->mutex);
        }

        //printf("READING NEXT JOB\n");
        //read the next job
        job = pool_args->jobs->head;
        remove_job(pool_args->jobs, job);
        pool_args->jobs->current_jobs--;

        //printf("PERFORMING TASK\n");
        //perform action
        job->function(job->args);
        //printf("TASK COMPLETE\n");
        free(job);

        //update counts
        pool_args->jobs->completed_jobs++;

        //signal
        pthread_cond_signal(pool_args->jobs->cond);

        //release lock
        pthread_mutex_unlock(pool_args->jobs->mutex);
    }
    return 0;
}

/**
 * @brief thread_process Function to execute on the threads; services the client's request
 * @param fd The file descriptor of the opened request
 * @return 0
 */
void * pool_thread_process(void * fd){
    assert(fd);
    int * fd_int = (int *)fd;
    client_process(*fd_int);
    free(fd_int);
    return 0;
}

/**
 * @brief process_request_thread_pool Creates and runs a thread pool
 * @param max_size The max size of the thread pool
 * @param accept_fd The accepted file descriptor of the listener
 */
void process_request_thread_pool(int max_size, int accept_fd){
    //initialize pool args
    struct pool_args * pool_args  = (struct pool_args *)malloc(sizeof(struct pool_args));
    pool_args->pool_max_size = max_size;
    pool_args->jobs = (struct job_list *)malloc(sizeof(struct job_list));

    //initalize job list
    pool_args->jobs->cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pool_args->jobs->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(pool_args->jobs->mutex, NULL);
    pthread_cond_init(pool_args->jobs->cond, NULL);
    pool_args->jobs->head = NULL;
    pool_args->jobs->tail = NULL;
    pool_args->jobs->current_jobs = 0;
    pool_args->jobs->completed_jobs = 0;


    //initalize the threads
    struct thread_list * thread_list = (struct thread_list *)malloc(sizeof(struct thread_list));
    thread_list->head = NULL;
    thread_list->tail = NULL;
    int i;
    for(i = 0; i < max_size; ++i){
        struct thread_node * node = (struct thread_node *)malloc(sizeof(struct thread_node));
        node->thread = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(node->thread, NULL, start_job, pool_args);
        insert_thread_list_tail(node, thread_list);
    }

    //read the pools
    i = 0;
    while(i < 1000){
        //printf("STARTING POOL\n");
        //acquire pool lock
        pthread_mutex_lock(pool_args->jobs->mutex);
        //printf("CHECKING IF WE CAN ENQUEUE\n");
        //check to see if we can enqueue
        while(pool_args->jobs->current_jobs >= pool_args->pool_max_size){
            //printf("WAITING UNTIL WORKER QUEUE IS FREE\n");
            //wait
            pthread_cond_wait(pool_args->jobs->cond, pool_args->jobs->mutex);
        }
        //printf("INSERTING JOB: %d\n", i);
        //insert jobs into the queue
        struct job * job = (struct job *)malloc(sizeof(struct job));
        int * fd_ptr = (int *)malloc(sizeof(int));
        assert(fd_ptr);
        *fd_ptr = server_accept(accept_fd);
        assert(*fd_ptr > 0);
        job->args = fd_ptr;
        job->function = pool_thread_process;
        insert_job_tail(pool_args->jobs, job);
        //increment jobs
        pool_args->jobs->current_jobs++;

        //signal
        pthread_cond_signal(pool_args->jobs->cond);

        //release lock
        pthread_mutex_unlock(pool_args->jobs->mutex);
        i++;
    }

    //check if we can cleanup
    //acquire lock
    pthread_mutex_lock(pool_args->jobs->mutex);
    //check to see if we can leave
    while(pool_args->jobs->completed_jobs < 1000){
        //wait
        pthread_cond_wait(pool_args->jobs->cond, pool_args->jobs->mutex);
    }
    pthread_cond_broadcast(pool_args->jobs->cond);
    pthread_mutex_unlock(pool_args->jobs->mutex);

    //cleanup
    struct thread_node * head = thread_list->head;
    pthread_t * thread;
    while(head){
        thread = head->thread;
        pthread_join(*thread, NULL);
        free(thread);
        remove_from_thread_list(head, thread_list);
        free(head);
        head = thread_list->head;
    }
    free(thread_list);

    pthread_cond_destroy(pool_args->jobs->cond);
    free(pool_args->jobs->cond);

    pthread_mutex_destroy(pool_args->jobs->mutex);
    free(pool_args->jobs->mutex);

    free(pool_args->jobs);
    free(pool_args);
}
