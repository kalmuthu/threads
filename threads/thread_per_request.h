#ifndef THREAD_PER_REQ
#define THREAD_PER_REQ

void process_threads_per_request(int max_concurrency, int accept_fd);

#endif
