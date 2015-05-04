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

#define MAX_CACHE_ENTRIES 10
#define POOL_SIZE 2
#define MAX_ACCEPTORS 2

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

#define MAX_REQ_SZ 1024
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


void * read_fs(lwt_chan_t cache_channel){
	//create channel
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

void * read_cache(lwt_chan_t main_channel){
	ENTRY query;
	ENTRY * result;

	lwt_chan_t accept_channel;
	int accept_fd;

	struct http_req *r;
	int len;

	int num_hash_entries = 0;


	char * data;
	lwt_chan_t fs_channel;
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
	lwt_chan_t response_channel = lwt_chan(0);

	//create hash
	hcreate(MAX_CACHE_ENTRIES);


	while(1){
		accept_channel = (lwt_chan_t)lwt_cgrp_wait(accept_group);
		//wait for request
		accept_fd = (int)lwt_rcv(accept_channel);

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
		result = hsearch(query, FIND);
		//data is cached
		if(result && result->data){
			data = result->data;
			printf("Found data: %s\n", data);
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
				strcpy(query.data, data);
				result = hsearch(query, ENTER);
				//there should be no errors insertion
				assert(result);
				num_hash_entries++;
			}
		}
		respond_and_free_req_kthd(r, data, len);
	}
	for(i = 0; i < MAX_ACCEPTORS; i++){
		lwt_cgrp_rem(accept_group, cache_channels[i]);
		lwt_chan_deref(cache_channels[i]);
	}
	lwt_chan_deref(response_channel);

	return NULL;
}

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
	//receive file descriptor
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
	assert(!lwt_kthd_create(read_cache, main_channel, LWT_NOJOIN));
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
			lwt_snd_chan(accept_channels[i], cache_channels[i]);
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
