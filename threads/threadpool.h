#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#import <pthread.h>

/**
 * @brief The job information
 */
struct job{
    /**
     * @brief The previous job in the list
     */
    struct job * prev;
    /**
     * @brief The next job in the list
     */
    struct job * next;
    /**
     * @brief The function pointer for the job task
     */
    void * (*function)(void * arg);
    /**
     * @brief The arguments for the function pointer
     */
    void * args;
};

/**
 * @brief The list of jobs
 */
struct job_list{
    /**
     * @brief The mutex lock for the jobs
     */
    pthread_mutex_t * mutex;
    /**
     * @brief The condition variable for the jobs
     */
    pthread_cond_t * cond;
    /**
     * @brief The head of the list
     */
    struct job * head;
    /**
     * @brief The tail of the list
     */
    struct job * tail;
    /**
     * @brief The number of jobs currently being processed
     */
    int current_jobs;
    /**
     * @brief The number of completed jobs
     */
    int completed_jobs;
};

/**
 * @brief The arguments to the pool
 */
struct pool_args{
    /**
     * @brief The size of the pool
     */
    int pool_max_size;
    /**
     * @brief The list of jobs
     */
    struct job_list * jobs;
};

void insert_job_head(struct job_list *, struct job *);
void insert_job_tail(struct job_list *, struct job *);
void remove_job(struct job_list *, struct job *);

#endif
