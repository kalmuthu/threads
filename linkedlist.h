#ifndef LINKED_LIST_H
#define LINKED_LIST_H

/**
 * @brief node_t Basic Node structure
 * Holds pointer to next node and value
 */
typedef struct node node_t;
struct node{
	node_t * next;
	void * value;
};

/**
 * @brief list_t Linked list structure
 * Holds pointers to head and tail
 */
typedef struct list list_t;
struct list{
	node_t * head;
	node_t * tail;
	int count;
};


node_t * create_node(void * value);

void push_node(node_t ** head, void * value);

void * pop_node(node_t ** head);

node_t * remove_node(node_t ** head, void * value);

void * peek_list(list_t * list);

void * pop_list(list_t * list);

void push_list(list_t * list, void * value);

void remove_list(list_t * list, void * value);

void init_list(list_t * list);

void empty_list_free(list_t * list);

#endif
