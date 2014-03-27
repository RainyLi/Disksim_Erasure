/*
 * disksim_event_queue.c
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#include <stdlib.h>

#include "disksim_event_queue.h"
#include "disksim_global.h"

static enode *space = NULL;

static enode* get_event_node() {
	if (space == NULL) {
		int i;
		space = malloc(sizeof(enode) * 16);
		for (i = 0; i < 15; i++)
			space[i].next = space + i + 1;
		space[16].next = NULL;
	}
	enode *ret = space;
	space = space->next;
	return ret;
}

void free_event_node(enode *node) {
	node->next = space;
	space = node;
}

enode* create_event_node(double time, int type, void *ctx) {
	enode *ret = get_event_node();
	ret->time = time;
	ret->type = type;
	ret->ctx = ctx;
	ret->depth = 1;
	ret->left = NULL;
	ret->right = NULL;
	return ret;
}

void event_queue_initialize(equeue *queue) {
	queue->root = NULL;
}

static enode* merge(enode *a, enode *b) {
	enode *t;
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

void event_queue_add(equeue *queue, enode *node) {
	queue->root = merge(queue->root, node);
}

enode* event_queue_pop(equeue *queue) {
	if (queue->root == NULL)
		return NULL;
	enode *ret = queue->root;
	queue->root = merge(ret->right, ret->left);
	return ret;
}

iogroup* create_ioreq_group() {
	iogroup *ret = (iogroup*) getfromextraq();
	ret->numreqs = 0;
	ret->reqs = NULL;
	return ret;
}

void add_to_ioreq(ioreq *req, iogroup *group) {
	if (group->numreqs > 0) {
		if (req->curr == NULL)
			req->groups = group;
		else
			req->curr->next = group;
		req->curr = group;
		group->next = NULL;
	}
}
