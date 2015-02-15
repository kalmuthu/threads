#ifndef LINKED_LIST_H
#define LINKED_LIST_H

/**
 * @brief Basic Node structure
 * Holds pointer to next node and value
 */
typedef struct node node_t;
struct node{
	/**
	 * Pointer to the next node in the list
	 */
	node_t * next;
	/**
	 * The value held by the node
	 */
	void * value;
};

/**
 * @brief Linked list structure
 * Holds pointers to head and tail
 */
typedef struct list list_t;
struct list{
	/**
	 * The head of the list
	 */
	node_t * head;
	/**
	 * The tail of the list
	 */
	node_t * tail;
	/**
	 * The count of elements in the list
	 */
	int count;
};


node_t * create_node(void * value);

void push_node(node_t ** head, void * value);

void * pop_node(node_t ** head);

node_t * remove_node(node_t ** head, void * value);

void * peek_list(list_t * list);

void * pop_list(list_t * list);

void push_list(list_t * list, void * value);

void push_head_list(list_t * list, void * value);

void remove_list(list_t * list, void * value);

void init_list(list_t * list);

void empty_list_free(list_t * list);

#endif
