/*
 * kthd_server.c

 *
 *  Created on: May 2, 2015
 *      Author: vagrant
 */
#include "kthd_server.h"
#include "search.h"

#include "lwt_cgrp.h"
#include "lwt_chan.h"
#include "lwt_kthd.h"
#include "objects.h"

#include "simple_http.h"
#include "content.h"
#include "server.h"

#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "assert.h"
#include "string.h"

#define MAX_CACHE_ENTRIES 10
#define POOL_SIZE 2
#define MAX_ACCEPTORS 2
#define LWT_CACHE 3
#define MAX_REQ_SZ 1024

/*
 * Once a request has been formulated (of type char *, with length
 * len), then we want to write it out to the client through the file
 * descriptor.  This function will do that, and when it is done, it
 * will free the request structure, and all memory associated with it.
 */
void
respond_and_free_req_kthd(struct http_req *r, char *response, int len)
{
	int amnt_written = 0;

	if (shttp_alloc_response_head(r, response, len)) {
		printf("Could not formulate HTTP response\n");
		shttp_free_req(r);
		return;
	}

	/*
	 * At this point, we have the response, and the http head to
	 * reply with.  Write them out to the client!
	 */
	while (amnt_written != r->resp_hd_len) {
		int ret = write(r->fd, r->resp_head + amnt_written,
				r->resp_hd_len - amnt_written);
		if (ret < 0) {
			printf("Could not write the response to the fd\n");
			goto done;
		}
		amnt_written += ret;
	}

	amnt_written = 0;
	while (amnt_written != r->resp_len) {
		int ret = write(r->fd, r->response + amnt_written,
				r->resp_len - amnt_written);
		if (ret < 0) {
			printf("Could not write the response to the fd\n");
			goto done;
		}
		amnt_written += ret;
	}
done:
	shttp_free_req(r);
	return;
}

/**
 * @brief Helper function for creating an http request
 * @param new_fd The file descriptor to open
 * @return The Http request received from the file descriptor
 */
struct http_req *
newfd_create_req_kthd(int new_fd)
{
	struct http_req *r;
	char *data;
	int amnt;

	data = calloc(MAX_REQ_SZ, sizeof(char));
	if (!data) return NULL;

	amnt = read(new_fd, data, MAX_REQ_SZ);
	if (amnt < 0) {
		perror("read off of new file descriptor");
		free(data);
		return NULL;
	}

	r = shttp_alloc_req(new_fd, data);
	if (!r) {
		printf("Could not allocate request\n");
		free(data);
		return NULL;
	}
	if (shttp_get_path(r)) {
		data[amnt] = '\0';
		printf("Incorrectly formatted HTTP request:\n\t%s\n", data);
		shttp_free_req(r);
		return NULL;
	}
	return r;
}

/**
 * @brief Processes the file system request; used for thread pool
 * @param cache_channel The channel to receive
 * @return NULL
 */
void * read_fs(lwt_chan_t cache_channel){
	//create channel; probably not the best method for this but instructions said to use thread pool
	lwt_chan_t fs_channel = lwt_chan(2);
	//send it
	lwt_snd_chan(cache_channel, fs_channel);
	//read data
	char * data = (char *)lwt_rcv(fs_channel);
	int * len = (int *)lwt_rcv(fs_channel);
	char * response = content_get(data, len);
	//send data back
	lwt_snd(cache_channel, response);
	//clean up
	lwt_chan_deref(fs_channel);
	lwt_chan_deref(cache_channel);
	return NULL;
}

/**
 * @brief Wrapper for the the file system workers; used for thread pool
 * @param main_channel The channel for sending the fs channel to
 */
void * spawn_fs_workers(lwt_chan_t main_channel){
	//create channel
	lwt_chan_t spawn_channel = lwt_chan(3);
	assert(spawn_channel);
	//send channel to main
	lwt_snd_chan(main_channel, spawn_channel);

	lwt_chan_t cache_channel;
	//spawn fs workers
	while(1){
		cache_channel = lwt_rcv(spawn_channel);
		read_fs(cache_channel);
	}

	lwt_chan_deref(main_channel);
	lwt_chan_deref(spawn_channel);

	return NULL;
}
/**
 * @brief LWT function for caching; checks if the path has been cached; if so, return it; else hit fs threads
 * @param kthd_channel The channel for the spawner
 * @return NULL
 */
void * read_cache(lwt_chan_t kthd_channel){
	ENTRY query;
	ENTRY * result;

	int accept_fd;

	struct http_req *r;
	int len;

	int num_hash_entries = 0;

	//set up channels
	lwt_chan_t my_channel = lwt_chan(3);
	assert(my_channel);
	lwt_snd_chan(kthd_channel, my_channel);

	lwt_chan_t main_channel = lwt_rcv_chan(my_channel);


	char * data;
	lwt_chan_t fs_channel;
	lwt_chan_t response_channel = lwt_chan(0);


	//create hash
	struct hsearch_data * htab = (struct hsearch_data *)calloc(1, sizeof(struct hsearch_data));
	hcreate_r(MAX_CACHE_ENTRIES, htab);


	while(1){
		//wait for request
		accept_fd = (int)lwt_rcv(my_channel);

		/*
		 * This code will be used to get the request and respond to
		 * it.  This should probably be in the worker
		 * threads/processes.
		 */
		r = newfd_create_req_kthd(accept_fd);
		if (!r || !r->path) {
			close(accept_fd);
			return NULL;
		}
		assert(r);
		assert(r->path);


		query.key = r->path;
		hsearch_r(query, FIND, &result, htab);
		//data is cached
		if(result && result->data){
			//prevent double frees
			data = (char *)calloc(MAX_REQ_SZ, sizeof(char));
			strncpy(data, result->data, MAX_REQ_SZ);
		}
		else
		{
			//we need to request it from one of the fs queues
			lwt_snd_chan(main_channel, response_channel);
			//wait for worker channel
			fs_channel = lwt_rcv_chan(response_channel);
			//send the path
			lwt_snd(fs_channel, r->path);
			lwt_snd(fs_channel, &len);
			//receive the response
			data = lwt_rcv(response_channel);
			//remove from channel
			lwt_chan_deref(fs_channel);
			//insert data into cache if there's capacity
			if(num_hash_entries < MAX_CACHE_ENTRIES){
				query.data = (char *)calloc(MAX_REQ_SZ, sizeof(char));
				strncpy(query.data, data, MAX_REQ_SZ);
				hsearch_r(query, ENTER, &result, htab);
				//there should be no errors insertion
				assert(result);
				num_hash_entries++;
			}
		}
		respond_and_free_req_kthd(r, data, len);
	}
	//cleanup
	lwt_chan_deref(main_channel);
	lwt_chan_deref(response_channel);
	lwt_chan_deref(my_channel);

	return NULL;
}

/**
 * @brief Function for running on cache to manage lwt thread pool
 * @param main_channel The channel from main used for passing data to other kthds
 * @return NULL
 */
void * read_cache_kthd(lwt_chan_t main_channel){
	//create cache channel
	lwt_chan_t cache_channels[MAX_ACCEPTORS];
	lwt_cgrp_t accept_group = lwt_cgrp();
	int i;
	for(i = 0; i < MAX_ACCEPTORS; ++i){
		cache_channels[i] = lwt_chan(3);
		lwt_cgrp_add(accept_group, cache_channels[i]);
		//send back to main channel
		lwt_snd_chan(main_channel, cache_channels[i]);
	}

	lwt_chan_t my_channel = lwt_chan(10);
	assert(my_channel);

	lwt_chan_t worker_channels[LWT_CACHE];
	for(i = 0; i < LWT_CACHE; ++i){
		lwt_create_chan(read_cache, my_channel, LWT_NOJOIN);
		worker_channels[i] = lwt_rcv_chan(my_channel);
		lwt_snd_chan(worker_channels[i], main_channel);
	}

	lwt_chan_t accept_channel;
	int fd;

	i = 0;
	while(1){
		//hey hey hey! here's the group wait!
		accept_channel = lwt_cgrp_wait(accept_group);
		fd = (int)lwt_rcv(accept_channel);
		lwt_snd(worker_channels[i], (void *)fd);
		i++;
		if(i >= LWT_CACHE){
			i = 0;
		}
	}
	//cleanup
	for(i = 0; i < MAX_ACCEPTORS; ++i){
		lwt_chan_deref(cache_channels[i]);
	}
	for(i = 0; i < LWT_CACHE; ++i){
		lwt_chan_deref(worker_channels[i]);
	}
	lwt_chan_deref(my_channel);
}

/**
 * @brief Accept worker kthd; accepts the new httd request
 * @param main_channel The channel to send data across
 * @return NULL
 */
void * accept_worker(lwt_chan_t main_channel){
	int server_fd;
	//create channel
	lwt_chan_t worker_channel = lwt_chan(3);
	//send to main channel
	lwt_snd_chan(main_channel, worker_channel);

	//receive cache channels
	lwt_chan_t cache_channels[MAX_ACCEPTORS];
	int i;
	for(i = 0; i < MAX_ACCEPTORS; ++i){
		cache_channels[i] = lwt_rcv_chan(worker_channel);
	}

	i = 0;
	//receive file descriptor; send to cache round robin style
	int fd = (int)lwt_rcv(worker_channel);
	while(1){
		server_fd = server_accept(fd);
		lwt_snd(cache_channels[i], (void *)server_fd);
		i++;
		if(i >= MAX_ACCEPTORS){
			i = 0;
		}
	}
}

/**
 * @brief Main function for the server; sets up channels and then passes data from cache to kthd modules
 * @param accept_fd The file descriptor for the http port being used
 */
void process_kthd_server(int accept_fd){
	//create channel
	lwt_chan_t main_channel = lwt_chan(20);
	lwt_chan_t fs_channels[POOL_SIZE];
	lwt_chan_t cache_channels[MAX_ACCEPTORS];
	lwt_chan_t accept_channels[MAX_ACCEPTORS];
	//create fs threadpool
	int i, j;
	for(i = 0; i < POOL_SIZE; ++i){
		assert(!lwt_kthd_create(spawn_fs_workers, main_channel, LWT_NOJOIN));
		fs_channels[i] = lwt_rcv_chan(main_channel);
	}

	//create the cache kthd
	assert(!lwt_kthd_create(read_cache_kthd, main_channel, LWT_NOJOIN));
	for(i = 0; i < MAX_ACCEPTORS; ++i){
		cache_channels[i] = lwt_rcv_chan(main_channel);
	}
	//create the acceptors
	i = 0;
	for(i = 0; i < MAX_ACCEPTORS; ++i){
		assert(!lwt_kthd_create(accept_worker, main_channel, LWT_NOJOIN));
		accept_channels[i] = lwt_rcv_chan(main_channel);
		//send cache channels
		for(j = 0; j < MAX_ACCEPTORS; ++j){
			lwt_snd_chan(accept_channels[i], cache_channels[j]);
		}
		//send file descriptor
		lwt_snd(accept_channels[i], (void *)accept_fd);
	}

	//rcv calls to spawn threads; round robin
	lwt_chan_t fs_read_channel;
	i = 0;
	while(1){
		fs_read_channel = lwt_rcv_chan(main_channel);
		lwt_snd_chan(fs_channels[i], fs_read_channel);
		i++;
		if(i >= POOL_SIZE){
			i = 0;
		}
	}

}
