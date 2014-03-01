/*
 * disksim_set.h
 *
 *  Created on: Feb 27, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_SET_H_
#define DISKSIM_SET_H_

#include <stdlib.h>
#include <stdio.h>

typedef struct tree_node_t {
	int first, second; // key
	int size;
	struct tree_node_t *left;
	struct tree_node_t *right;
	struct tree_node_t *next; // for garbage collecting
} node;

node* create_set();
void free_set(node *root);
node* make_list(node *root, node *tail);
void insert(node **root, int key1, int key2);
int contains(node *root, int key1, int key2);

#endif /* DISKSIM_SET_H_ */
