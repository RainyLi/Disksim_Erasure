/*
 * disksim_event_queue.c
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#include <stdlib.h>

#include "disksim_event_queue.h"
#include "disksim_global.h"
#include "disksim_malloc.h"

extern int en_idx;

event_node_t* create_event(double time, int type, void *ctx) {
	event_node_t *ret = (event_node_t*) disksim_malloc(en_idx);
	ret->time = time;
	ret->type = type;
	ret->ctx = ctx;
	ret->depth = 1;
	ret->left = NULL;
	ret->right = NULL;
	return ret;
}

void free_event(event_node_t *node)
{
	disksim_free(en_idx, node);
}

void event_queue_initialize(event_queue_t *queue) {
	queue->root = NULL;
}

static event_node_t* merge(event_node_t *a, event_node_t *b) {
	event_node_t *t;
	if (a == NULL) return b;
	if (a->time < b->time) {
		a->right = merge(a->right, b);
		if (!a->left || a->left->depth < a->right->depth) {
			t = a->left;
			a->left = a->right;
			a->right = t;
		}
		a->depth = a->left->depth + 1;
		return a;
	} else {
		b->right = merge(b->right, a);
		if (!b->left || b->left->depth < b->right->depth) {
			t = b->left;
			b->left = b->right;
			b->right = t;
		}
		b->depth = b->left->depth + 1;
		return b;
	}
}

void event_queue_add(event_queue_t *queue, event_node_t *node) {
	queue->root = merge(queue->root, node);
}

event_node_t* event_queue_pop(event_queue_t *queue) {
	if (queue->root == NULL)
		return NULL;
	event_node_t *ret = queue->root;
	queue->root = merge(ret->right, ret->left);
	return ret;
}
