
#import "linkedlist.h"
#import <stdlib.h>
#import <assert.h>
#import <stdio.h>

/**
 * @brief create_node Allocates and intiializes a node with the provided value
 * @param value The value to use
 * @return A new node
 */
node_t * create_node(void * value){
	//ensure value is not null
	assert(value);
	node_t * new_node = (node_t *)malloc(sizeof(node_t));
	new_node->value = value;
	return new_node;
}

/**
 * @brief push_node Pushes the value to the front of the lsit
 * @param head The pointer to the front of the list
 * @param value The value of the node to use
 */
void push_node(node_t ** head, void * value){
	//ensure head is not null
	assert(head);
	assert(*head);
	node_t * curr_head = *head;
	node_t * new_node = create_node(value);
	new_node->next = curr_head;
	//fix head
	*head = new_node;
}

/**
 * @brief pop_node Pops the node from the head
 * @param head The head of the list
 * @return The value at that head
 */
void * pop_node(node_t ** head){
	//ensure head is not null
	assert(head);
	assert(*head);
	node_t * curr_head = *head;
	node_t * next = curr_head->next;
	//update head
	*head = next;
	void* value = curr_head->value;
	//free memory
	free(curr_head);
	return value;
}

/**
 * @brief remove_node Removes a node from the list
 * @param head The head of the list
 * @param value The value to search for
 * @return The detached node if it exists in the list
 */
node_t * remove_node(node_t ** head, void * value){
	//look for the value in the list
	assert(head);
	assert(*head);

	node_t * curr_node = *head;
	node_t * prev = NULL;
	while(curr_node){
		if(curr_node->value == value){
			//remove curr node
			if(prev){
				prev->next = curr_node->next;
			}
			else{
				*head = curr_node->next;
			}
			return curr_node;
		}
        prev = curr_node;
		curr_node = curr_node->next;
	}
	return NULL;
}

/**
 * @brief init_list Initializes the list
 * @param list The list to init
 */
void init_list(list_t * list){
    assert(list);
	list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

/**
 * @brief peek_list Peeks at the head of the list and returns its value
 * @param list The list
 * @return The value of the head
 */
void * peek_list(list_t * list){
	assert(list);
	assert(list->head);
	return list->head->value;
}

/**
 * @brief pop_list Pops the head of the list and returns its value
 * @param list The list
 * @return The value of the head
 */
void * pop_list(list_t * list){
    assert(list);
    assert(list->head);
    if(list->head){
        list->count--;
        return pop_node(&(list->head));
    }
    return NULL;
}

/**
 * @brief push_list Pushes the node to the end of the list
 * @param list The list to use
 * @param value The value to store in the list
 */
void push_list(list_t * list, void * value){
	node_t * node = (node_t *)malloc(sizeof(node_t));
    assert(node);
    node->next = NULL;
    node->value = value;
	//check for default case
	if(list->tail){
		list->tail->next = node;
		list->tail = node;
	}
	else{
		list->head = node;
		list->tail = node;
	}
	list->count++;
}

void push_head_list(list_t * list, void * value){
	node_t * node = (node_t *)malloc(sizeof(node_t));
	assert(node);
	node->next = NULL;
	node->value = value;
	if(list->head){
		node->next = list->head;
		list->head = node->next;
	}
	else{
		list->head = node;
		list->tail = node;
	}
	list->count++;
}

/**
 * @brief remove_list Removes the value from the list
 * @param list The list to use
 * @param value The value to look for
 */
void remove_list(list_t * list, void * value){
	node_t * node = remove_node(&(list->head), value);
	if(node){
		list->count--;
		free(node);
	}
}

void empty_list_free(list_t * list){
	while(list->head){
		void * value = pop_list(list);
		free(value);
	}
}
