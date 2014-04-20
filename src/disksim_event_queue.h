/*
 * disksim_event_queue.h
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_EVENT_QUEUE_H_
#define DISKSIM_EVENT_QUEUE_H_

#include "disksim_global.h"

typedef struct event_node_t {
	double time;
	int type;
	int *ctx;

	struct event_node_t *left, *right;
	struct event_node_t *next;
	int depth;
} enode;

typedef struct event_queue_t {
	enode *root;
} equeue;

enode* create_event_node(double time, int type, void *ctx);
void   free_event_node(enode *);

void   event_queue_initialize(equeue *);
void   event_queue_add(equeue *, enode *);
enode* event_queue_pop(equeue *);

#endif /* DISKSIM_EVENT_QUEUE_H_ */
