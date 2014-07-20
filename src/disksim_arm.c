/*
 * disksim_arm.c
 *
 *  Created on: Jul 19, 2014
 *      Author: zyz915
 */

#include <stdlib.h>
#include <string.h>

#include "disksim_arm.h"
#include "disksim_global.h"

extern int sb_idx, sh_idx;

static int arm_calculate(arm_t *arm, int disk, int pattern)
{
	metadata_t *meta = arm->meta;
	int *map = arm->map, *distr = arm->distr;
	int nr_total = meta->w * meta->n;
	int r, c, uid;
	memset(map, 0, sizeof(int) * nr_total);
	memset(distr, 0, sizeof(int) * meta->n);
	for (r = 0; r < meta->w; r++) {
		int ch = 1 & (pattern >> r);
		uid = ID(r, disk);
		while (meta->map_p2[uid][ch] == -1) --ch;
		element_t *elem = meta->chains[meta->map_p2[uid][ch]];
		for (; elem; elem = elem->next)
			if (elem->col != disk)
				map[ID(elem->row, elem->col)] = meta->usize;
	}
	for (uid = 0; uid < nr_total; uid++)
		distr[uid % meta->n] += map[uid];
	int *cnt = meta->sctlr->count, score = 0;
	switch (arm->method) {
	case ARM_DO_MAX:
		for (c = 0; c < meta->n; c++)
			score = max(score, distr[c] + cnt[c]);
		break;
	case ARM_DO_SUM:
		for (c = 0; c < meta->n; c++)
			score += distr[c];
		break;
	default:
		fprintf(stderr, "invalid method: %d\n", arm->method);
		exit(-1);
	}
	return score;
}

static void arm_make_request(double time, arm_t *arm)
{
	if (arm->progress < arm->max_stripes) {
		sub_ioreq_t *subreq = (sub_ioreq_t*) disksim_malloc(sb_idx);
		subreq->time = time;
		subreq->reqtype = REQ_TYPE_RECOVERY;
		subreq->state = STATE_BEGIN;
		subreq->stripeno = arm->progress++;
		subreq->meta = (void*) arm;
		subreq->reqctx = NULL;
		if (arm->lastreq <= time) {
			sh_get_active_stripe(time, arm->meta->sctlr, subreq);
			arm->lastreq = time;
		} else
			arm->internal_fn(arm->lastreq, (void*)subreq);
		arm->lastreq += arm->delay;
	}
}

static void arm_complete(double time, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	arm_t *arm = (arm_t*) subreq->meta;
	if (!(--subreq->out_reqs)) {
		if (subreq->state == STATE_WRITE) {
			sh_release_stripe(time, arm->meta->sctlr, subreq->stripeno);
			disksim_free(sb_idx, subreq);
			arm->completed += 1;
			if (arm->completed == arm->max_stripes) {
				arm->complete_fn(time);
			} else
				arm_make_request(time, arm);
		} else
			sh_redo_callback(time, arm->meta->sctlr, subreq, sh);
	}
}

static void arm_do_recovery(double time, stripe_head_t *sh, sub_ioreq_t *subreq, int disk, int pattern)
{
	subreq->state += 1;
	subreq->out_reqs = 1;
	arm_t *arm = (arm_t*) subreq->meta;
	metadata_t *meta = arm->meta;
	stripe_ctlr_t *sctlr = meta->sctlr;
	int uid, r, nr_total = meta->w * meta->n, *map = arm->map;
	memset(map, 0, sizeof(int) * nr_total);
	if (subreq->state == STATE_READ) {
		for (r = 0; r < meta->w; r++) {
			int ch = 1 & (pattern >> r);
			uid = ID(r, disk);
			while (meta->map_p2[uid][ch] == -1) --ch;
			element_t *elem = meta->chains[meta->map_p2[uid][ch]];
			for (; elem; elem = elem->next)
				if (elem->col != disk)
					map[ID(elem->row, elem->col)] = 1;
		}
	} else {
		for (r = 0; r < meta->w; r++)
			map[ID(r, disk)] = 1;
	}
	int flag = (subreq->state == STATE_READ ? DISKSIM_READ : DISKSIM_WRITE);
	for (uid = 0; uid < nr_total; uid++)
		if (map[uid]) {
			sh_request_t *shreq = (sh_request_t*) disksim_malloc(sh_idx);
			shreq->time = time;
			shreq->flag = flag;
			shreq->stripeno = subreq->stripeno;
			shreq->devno = uid % meta->n;
			shreq->blkno = uid / meta->n;
			shreq->v_begin = 0;
			shreq->v_end = meta->usize;
			shreq->reqctx = (void*) subreq;
			shreq->meta = (void*) sctlr;
			subreq->out_reqs += 1;
			sh_request_arrive(time, sctlr, sh, shreq);
		}
	arm_complete(time, subreq, sh);
}

static void arm_recovery(double time, sub_ioreq_t *subreq, stripe_head_t *sh, int fails, int *fd)
{
	arm_t *arm = (arm_t*) subreq->meta;
	int pattern = 0;
	int i, f_disk = 0, mask = (1 << arm->meta->w) - 1;
	while (!fd[f_disk]) ++f_disk;
	if (arm->method != ARM_NORMAL) {
		int score = arm_calculate(arm, f_disk, 0);
		for (i = 0; i < 64; i++) {
			int pat = rand() & mask;
			int cur = arm_calculate(arm, f_disk, pat);
			if (cur < score) { // higher is better
				score = cur;
				pattern = pat;
			}
		}
	}
	arm_do_recovery(time, sh, subreq, f_disk, pattern);
}

void arm_init(arm_t *arm, int method, int threads, int max_sectors, double delay,
		metadata_t *meta, arm_internal_t internal, arm_complete_t complete)
{
	arm->method = method;
	arm->threads = threads;
	arm->delay = delay;
	arm->meta = meta;
	arm->map = (int*) malloc(sizeof(int) * meta->w * meta->n);
	arm->distr = (int*) malloc(sizeof(int) * meta->n);
	memset(arm->map, 0, sizeof(int) * meta->w * meta->n);
	memset(arm->distr, 0, sizeof(int) * meta->n);
	arm->max_stripes = max_sectors / (meta->w * meta->usize);
	sh_set_rec_callbacks(meta->sctlr, arm_recovery, arm_complete);
	arm->internal_fn = internal;
	arm->complete_fn = complete;
}

void arm_run(double time, arm_t *arm)
{
	arm->progress = 0;
	arm->completed = 0;
	arm->lastreq = 0;
	int i;
	for (i = 0; i < arm->threads; i++)
		arm_make_request(time, arm);
}

void arm_internal_event(double time, arm_t *arm, void* ctx)
{
	sh_get_active_stripe(time, arm->meta->sctlr, (sub_ioreq_t*)ctx);
}
