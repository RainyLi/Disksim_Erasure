/*
 * disksim_req.h
 *
 *  Created on: Jul 14, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_REQ_H_
#define DISKSIM_REQ_H_

#define REQ_TYPE_NORMAL		0
#define REQ_TYPE_RECOVERY	1
#define REQ_TYPE_MIGRATION	2


#include "disksim_container.h"
#include "disksim_interface.h"
#include "disksim_list.h"

enum req_state {
	STATE_BEGIN,
	STATE_READ,
	STATE_WRITE,
	STATE_DONE
};

typedef struct ioreq {
	double time;   // request start time
	int reqtype;   // request type (normal/recovery/migration)
	int reqno;     // request number
	int blkno;     // request start location (in sectors)
	int bcount;    // request length (in sectors)
	int flag;      // request flag, defined in disksim_global.h
	int out_reqs;  // decrease to 0 means finished
} ioreq_t;

typedef struct sub_ioreq {
	double time;   // request start time
	int reqtype;   // request type (normal/recovery/migration)
	int reqno;     // equal to main ioreq
	int stripeno;  // stripe number
	int blkno;     // start location in stripe (in sectors)
	int bcount;    // request length (in sectors)
	int flag;      // request flag
	int out_reqs;  // decrease to 0 means finished
	enum req_state state;
	void *reqctx, *meta;
} sub_ioreq_t;

typedef struct sh_request {
	double time;
	int flag;
	int stripeno; // stripe number
	int devno;    // logical device number
	int blkno;   // unit number
	int v_begin, v_end; // range
	void *reqctx, *meta;
} sh_request_t;

typedef struct page {
	int state; // 0=empty, 1=filled
	int v_begin, v_end; // filled range
} page_t;

typedef struct stripe_head {
	int users; // active or inactive
	int stripeno; // stripe number (default -1)
	page_t *page;
	struct list_head list; // link to active or inactive queue
	struct stripe_ctlr *sctlr; // link to controller
} stripe_head_t;

typedef struct wait_request {
	sub_ioreq_t *subreq;
	struct list_head list; // wait list
} wait_req_t;

typedef void(*sh_maprequest_t)(double time, struct sub_ioreq *subreq, struct stripe_head *sh);
typedef void(*sh_iocomplete_t)(double time, struct sub_ioreq *subreq, struct stripe_head *sh);

typedef struct stripe_ctlr {
	int nr_disks; // number of devices
	int nr_units; // number of units
	int u_size;   // unit size (in sectors)

	struct list_head inactive; // inactive queue

	struct list_head waitreqs; // no enough stripe_head

	hash_table_t *ht;

	sh_maprequest_t mapreq_fn; // call this function when becomes active
	sh_iocomplete_t comp_fn; // call this function when finishes requests
} stripe_ctlr_t;

void sh_init(stripe_ctlr_t *sctlr, int nr_stripes, int nr_disks, int nr_units, int u_size);
void sh_set_mapreq_callback(stripe_ctlr_t *sctlr, sh_maprequest_t mapreq);
void sh_set_complete_callback(stripe_ctlr_t *sctlr, sh_iocomplete_t comp);

void sh_get_active_stripe(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq);
void sh_release_stripe(double time, stripe_ctlr_t *sctlr, int stripeno);
void sh_request_arrive(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sh_request_t *shreq);
void sh_complete_callback(double time, struct disksim_request *dr, void *ctx);

#endif /* DISKSIM_REQ_H_ */
