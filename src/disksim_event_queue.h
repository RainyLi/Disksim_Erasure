/*
 * disksim_event_queue.h
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_EVENT_QUEUE_H_
#define DISKSIM_EVENT_QUEUE_H_

#define EVENT_STOP_SIM			0
#define EVENT_TRACE_FETCH		1
#define EVENT_TRACE_RECON		2
#define EVENT_TRACE_MAPREQ		3
#define EVENT_TRACE_ITEM		4
#define EVENT_IO_COMPLETE		5
#define EVENT_IO_INTERNAL		6
#define EVENT_STAT_ADD			7
#define EVENT_STAT_PEAK			8

#define TYPE_NORMAL				0
#define TYPE_RECON				1

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

typedef struct io_request_group_t {
	struct disksim_request *reqs;
	int numreqs;
	int cnt;
	struct io_request_group_t *next;
} iogroup;

typedef struct io_request_t {
	int reqno;
	int reqtype;
	double time;
	int blkno;
	int bcount;
	int flag;
	int stat;
	iogroup *groups;
	iogroup *curr;
} ioreq;

enode* create_event_node(double time, int type, void *ctx);
void   free_event_node(enode *);

void   event_queue_initialize(equeue *);
void   event_queue_add(equeue *, enode *);
enode* event_queue_pop(equeue *);

iogroup* create_ioreq_group();
void   add_to_ioreq(ioreq *req, iogroup *group);

#endif /* DISKSIM_EVENT_QUEUE_H_ */
