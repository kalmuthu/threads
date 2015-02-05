
void __lwt_schedule(void);
void __lwt_dispatch(lwt_t next, lwt_t current);
void __lwt_trampoline(void);
void *__lwt_stack_get(void);
void __lwt_stack_return(void *stk);

//global counter for the id
int __next_id = 1;

//pointer to the current thread
lwt_t __current_thread;

/**
 * Counter for the id
 */
int __get_new_id(){
	return __next_id++; //return and then increment
}

lwt_t lwt_current(){
	return __current_thread;
}

/**
 * @brief Gets the thread id
 * @return The id of the thread
 */
int lwt_id(lwt_t thread){
	return thread->lwt_id;
}


