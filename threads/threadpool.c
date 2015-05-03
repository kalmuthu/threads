
#include "threadpool.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/**
 * @brief Inserts the job at the head of the list
 * @param job_list The list of jobs
 * @param job The job to be inserted
 */
void insert_job_head(struct job_list * job_list, struct job * job){
    if(job_list->head){
        //job will be new head
        job->prev = NULL;
        job->next = job_list->head;
        job_list->head->prev = job;
        job_list->head = job;
    }
    else{
        //job will be head and tail
        job->prev = NULL;
        job->next = NULL;
        job_list->head = job;
        job_list->tail = job;
    }
}

/**
 * @brief Inserts the job to the tail of the list
 * @param job_list The list of jobs
 * @param job The job to be inserted
 */
void insert_job_tail(struct job_list * job_list, struct job * job){
    if(job_list->tail){
        //job will be new tail
        job->next = NULL;
        job->prev = job_list->tail;
        job_list->tail->next = job;
        job_list->tail = job;
    }
    else{
        //job will be head and tail
        job->prev = NULL;
        job->next = NULL;
        job_list->head = job;
        job_list->tail = job;
    }
}

/**
 * @brief Removes the given job from the list
 * @param job_list The list of jobs
 * @param job The job to be removed
 */
void remove_job(struct job_list * job_list, struct job * job){
    //detach
    if(job->next){
        job->next->prev = job->prev;
    }
    if(job->prev){
        job->prev->next = job->next;
    }
    if(job == job_list->head){
        job_list->head = job_list->head->next;
    }
    if(job == job_list->tail){
        job_list->tail = job_list->tail->prev;
    }
}
