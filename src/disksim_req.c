/*
 * disksim_req.c
 *
 *  Created on: Jul 14, 2014
 *      Author: zyz915
 */

#include <stdlib.h>
#include <string.h>

#include "disksim_global.h"
#include "disksim_malloc.h"
#include "disksim_interface.h"
#include "disksim_req.h"
#include "disksim_timer.h"

extern struct disksim_interface *interface;
extern int sh_idx, wt_idx, dr_idx;

void sh_init(stripe_ctlr_t *sctlr, int nr_stripes, int nr_disks, int nr_units, int u_size)
{
	int i;
	sctlr->ht = (hash_table_t*) malloc(sizeof(hash_table_t));
	ht_create(sctlr->ht, 10);
	sctlr->inactive.prev = sctlr->inactive.next = &sctlr->inactive;
	sctlr->nr_disks = nr_disks;
	sctlr->waitreqs.prev = sctlr->waitreqs.next = &sctlr->waitreqs;
	sctlr->nr_units = nr_units;
	sctlr->u_size = u_size;
	for (i = 0; i < nr_stripes; i++) {
		stripe_head_t *sh = (stripe_head_t*) disksim_malloc(sh_idx);
		sh->users = 0;
		sh->stripeno = -1;
		sh->page = (page_t*) malloc(sizeof(page_t) * nr_units * u_size);
		memset(sh->page, 0, sizeof(page_t) * nr_units * u_size);
		list_add_tail(&(sh->list), &(sctlr->inactive));
	}
	sctlr->fails = 0; // normal
	sctlr->dev_failed = (int*) malloc(sizeof(int) * sctlr->nr_disks);
	sctlr->log_failed = (int*) malloc(sizeof(int) * sctlr->nr_disks);
	memset(sctlr->dev_failed, 0, sizeof(int) * sctlr->nr_disks);
}

void sh_set_mapreq_callback(stripe_ctlr_t *sctlr, sh_maprequest_t mapreq)
{
	sctlr->mapreq_fn = mapreq;
}

void sh_set_degraded_callback(stripe_ctlr_t *sctlr, sh_degraded_t degraded)
{
	sctlr->degraded_fn = degraded;
}

void sh_set_complete_callback(stripe_ctlr_t *sctlr, sh_iocomplete_t comp)
{
	sctlr->comp_fn = comp;
}

void sh_set_disk_failure(double time, stripe_ctlr_t *sctlr, int devno) {
	if (!sctlr->dev_failed[devno]) {
		sctlr->dev_failed[devno] = 1;
		sctlr->fails += 1;
	}
	if (sctlr->fails)
		sctlr->rec_prog = 0;
}

void sh_set_disk_repaired(double time, stripe_ctlr_t *sctlr, int devno) {
	if (sctlr->dev_failed[devno]) {
		sctlr->dev_failed[devno] = 0;
		sctlr->fails -= 1;
	}
}

static void sh_return_stripe(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	int stripeno = subreq->stripeno;
	if (subreq->reqtype == REQ_TYPE_NORMAL) {
		if (sctlr->fails == 0 || stripeno < sctlr->rec_prog)
			sctlr->mapreq_fn(time, subreq, sh);
		else {
			int i, disks = sctlr->nr_disks;
			memset(sctlr->log_failed, 0, sizeof(int) * disks);
			for (i = 0; i < disks; i++)
				if (sctlr->dev_failed[i])
					sctlr->log_failed[(i + disks - stripeno % disks) % disks] = 1;
			sctlr->degraded_fn(time, subreq, sh, sctlr->log_failed);
		}
	}
}

void sh_get_active_stripe(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq)
{
	int stripeno = subreq->stripeno;
	if (ht_contains(sctlr->ht, stripeno)) {
		stripe_head_t *sh = (stripe_head_t*) ht_getvalue(sctlr->ht, stripeno);
		sh->users += 1;
		if (sh->users == 1) // remove from inactive queue
			list_del(&(sh->list));
		sh_return_stripe(time, sctlr, subreq, sh);
		return;
	}
	if (!list_empty(&(sctlr->inactive))) {
		stripe_head_t *sh = list_entry(sctlr->inactive.next, stripe_head_t, list);
		list_del(&(sh->list));
		if (sh->stripeno != -1) {
			ht_remove(sctlr->ht, sh->stripeno);
			memset(sh->page, 0, sizeof(page_t) * sctlr->nr_units * sctlr->u_size);
		}
		sh->stripeno = stripeno;
		ht_insert(sctlr->ht, stripeno, sh);
		sh->users += 1;
		sh_return_stripe(time, sctlr, subreq, sh);
		return;
	}
	// sleep
	wait_req_t *wait = (wait_req_t*) disksim_malloc(wt_idx);
	wait->subreq = subreq;
	list_add_tail(&(wait->list), &(sctlr->waitreqs));
}

void sh_release_stripe(double time, stripe_ctlr_t *sctlr, int stripeno)
{
	stripe_head_t *sh = (stripe_head_t*) ht_getvalue(sctlr->ht, stripeno);
	sh->users -= 1;
	if (sh->users == 0) {
		list_add_tail(&(sh->list), &(sctlr->inactive));
		if (!list_empty(&(sctlr->waitreqs))) {
			wait_req_t *wait = list_entry(sctlr->waitreqs.next, wait_req_t, list);
			list_del(&(wait->list));
			sh_get_active_stripe(time, sctlr, wait->subreq);
			disksim_free(wt_idx, wait);
		}
	}
}

int dr_reqs = 0, intr_events = 0;
static void sh_send_request(double time, stripe_ctlr_t *sctlr, sh_request_t *shreq)
{
	struct disksim_request *dr = (struct disksim_request*) disksim_malloc(dr_idx);
	int base = (shreq->stripeno * sctlr->nr_units + shreq->blkno) * sctlr->u_size;
	dr->start = time;
	dr->flags = shreq->flag;
	dr->devno = (shreq->devno + shreq->stripeno) % sctlr->nr_disks;
	dr->blkno = base + shreq->v_begin;
	dr->bytecount = (shreq->v_end - shreq->v_begin) * 512;
	dr->reqctx = (void*) shreq;
	if (sctlr->dev_failed[dr->devno]) {
		fprintf(stderr, "invalid request: device %d is failed!\n", dr->devno);
		exit(-1);
	}
	dr_reqs++;
	disksim_interface_request_arrive(interface, time, dr);
}

void sh_request_arrive(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sh_request_t *shreq)
{
	sub_ioreq_t *subreq = (sub_ioreq_t*) shreq->reqctx;
	int unit_id = shreq->devno * sctlr->nr_units + shreq->blkno;
	page_t *pg = sh->page + unit_id;
	if (shreq->flag & DISKSIM_READ) {
		if (pg->state == 0 || shreq->v_begin < pg->v_begin || shreq->v_end > pg->v_end) // empty
			sh_send_request(time, sctlr, shreq);
		else
			sctlr->comp_fn(time, subreq, sh);
	} else {
		pg->v_begin = min(pg->v_begin, shreq->v_begin);
		pg->v_end = max(pg->v_end, shreq->v_end);
		sh_send_request(time, sctlr, shreq);
	}
}

static int lasttime = 0, iops = 0;
void sh_request_complete(double time, struct disksim_request *dr)
{
	while ((lasttime + 1 ) * 1000. < time) {
		timer_stop(TIMER_GLOBAL);
		double secs = timer_millisecond(TIMER_GLOBAL) * 1e-6;
		printf("time %4d, IOPS %4d, now %.3f, freqs %5d, events/s %.2f\n", lasttime, iops,
				timer_millisecond(TIMER_GLOBAL) * 1e-6, dr_reqs, intr_events / secs);
		lasttime += 1;
		iops = 0;
		timer_start(TIMER_GLOBAL);
	}
	fflush(stdout);
	iops += 1;
	sh_request_t *shreq = (sh_request_t*) dr->reqctx;
	sub_ioreq_t *subreq = (sub_ioreq_t*) shreq->reqctx;
	stripe_ctlr_t *sctlr = (stripe_ctlr_t*) shreq->meta;
	stripe_head_t *sh = (stripe_head_t*) ht_getvalue(sctlr->ht, subreq->stripeno);
	if (shreq->flag & DISKSIM_READ) {
		int unit_id = shreq->devno * sctlr->nr_units + shreq->blkno;
		page_t *pg = sh->page + unit_id;
		pg->v_begin = min(pg->v_begin, shreq->v_begin);
		pg->v_end   = max(pg->v_end  , shreq->v_end  );
	}
	disksim_free(sh_idx, shreq);
	sctlr->comp_fn(time, subreq, sh);
}
