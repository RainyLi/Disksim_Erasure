/*
 * disksim_arm.c
 *
 *  Created on: Jul 19, 2014
 *      Author: zyz915
 */

#include <stdlib.h>

#include "disksim_arm.h"

extern int sb_idx;

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
				map[ID(elem->row, elem->col)] = 1;
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
		return -1;
	}
	return score;
}

static void arm_do_recovery(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sub_ioreq_t *subreq, int disk, int pattern)
{
	subreq->state += 1;
	subreq->out_reqs += 1;
	arm_t *arm = (arm_t*) subreq->meta;
	metadata_t *meta = arm->meta;
	int uid, r, nr_total = meta->w * meta->n, *map = arm->map;
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
			sh_request_t *shreq = (sh_request_t*) shreq;
			shreq->time = time;
			shreq->flag = flag;
			shreq->stripeno = subreq->stripeno;
			shreq->devno = uid % meta->n;
			shreq->blkno = uid / meta->n;
			shreq->v_begin = 0;
			shreq->v_end = meta->usize;
			shreq->reqctx = (void*) subreq;
			shreq->meta = (void*) arm;
		}
	arm_complete(time, sctlr, sh, subreq);
}

void arm_init(arm_t *arm, int method, int nr_stripes, metadata_t *meta, arm_complete_t *comp)
{
	arm->method = method;
	arm->stripes = nr_stripes;
	arm->meta = meta;
	arm->comp_fn = comp;
	arm->map = (int*) malloc(sizeof(int) * meta->w * meta->n);
	arm->distr = (int*) malloc(sizeof(int) * meta->n);
}

void arm_run(double time, void *ctx)
{
	arm_t *arm = (arm_t*) ctx;
	int i;
	for (i = 0; i < arm->stripes; i++) {
		sub_ioreq_t *subreq = (sub_ioreq_t*) disksim_malloc(sb_idx);
		sh_get_active_stripe(time, arm->meta->sctlr, subreq);
	}
}

void arm_recovery(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sub_ioreq_t *subreq, int fails, int *fd)
{
	assert(fails == 1);
	arm_t *arm = (arm_t*) subreq->meta;
	metadata_t *meta = arm->meta;
	int i, disk = 0, mask = (1 << meta->w) - 1;
	int score = arm_calculate(arm, disk, 0), pattern = 0;
	while (!fd[disk]) ++disk;
	for (i = 0; i < 64; i++) {
		int pat = rand() & mask;
		int cur = arm_calculate(arm, disk, pat);
		if (cur > score) { // higher is better
			score = cur;
			pattern = pat;
		}
	}
	arm_do_recovery(time, sctlr, sh, subreq, pattern);
}

void arm_complete(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sub_ioreq_t *subreq)
{
	arm_t *arm = (arm_t*) subreq->meta;
	if (!(--subreq->out_reqs)) {
		if (subreq->state == STATE_WRITE) {
			sh_release_stripe(time, arm->meta->sctlr, subreq->stripeno);
			ioreq_t *req = (ioreq_t*) subreq->reqctx;
			if ((--req->out_reqs) == 0)
				arm->comp_fn(time, (ioreq_t*) subreq->reqctx);
			disksim_free(sb_idx, subreq);
		} else
			sh_redo_callback(time, arm->meta->sctlr, subreq, sh);
	}
}
